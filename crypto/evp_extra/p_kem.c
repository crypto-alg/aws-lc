// Copyright Amazon.com Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0 OR ISC

#include <openssl/evp.h>

#include <openssl/err.h>
#include <openssl/mem.h>

#include "../fipsmodule/evp/internal.h"
#include "../fipsmodule/delocate.h"
#include "../kem/internal.h"
#include "../internal.h"
#include "internal.h"

typedef struct {
  const KEM *kem;
} KEM_PKEY_CTX;

static int pkey_kem_init(EVP_PKEY_CTX *ctx) {
  KEM_PKEY_CTX *dctx;
  dctx = OPENSSL_malloc(sizeof(KEM_PKEY_CTX));
  if (dctx == NULL) {
    return 0;
  }
  OPENSSL_memset(dctx, 0, sizeof(KEM_PKEY_CTX));

  ctx->data = dctx;

  return 1;
}

static void pkey_kem_cleanup(EVP_PKEY_CTX *ctx) {
  OPENSSL_free(ctx->data);
}

static int pkey_kem_keygen(EVP_PKEY_CTX *ctx, EVP_PKEY *pkey) {
  KEM_PKEY_CTX *dctx = ctx->data;
  const KEM *kem = dctx->kem;
  if (kem == NULL) {
    if (ctx->pkey == NULL) {
      OPENSSL_PUT_ERROR(EVP, EVP_R_NO_PARAMETERS_SET);
      return 0;
    }
    kem = KEM_KEY_get0_kem(ctx->pkey->pkey.kem_key);
  }

  KEM_KEY *key = KEM_KEY_new();
  if (key == NULL ||
      !KEM_KEY_init(key, kem) ||
      !kem->method->keygen(key->public_key, key->secret_key) ||
      !EVP_PKEY_assign(pkey, EVP_PKEY_KEM, key)) {
    KEM_KEY_free(key);
    return 0;
  }

  key->has_secret_key = 1;

  return 1;
}

static int pkey_kem_encapsulate(EVP_PKEY_CTX *ctx,
                                uint8_t *ciphertext,
                                size_t  *ciphertext_len,
                                uint8_t *shared_secret,
                                size_t  *shared_secret_len) {
  KEM_PKEY_CTX *dctx = ctx->data;
  const KEM *kem = dctx->kem;
  if (kem == NULL) {
    if (ctx->pkey == NULL) {
      OPENSSL_PUT_ERROR(EVP, EVP_R_NO_PARAMETERS_SET);
      return 0;
    }
    kem = KEM_KEY_get0_kem(ctx->pkey->pkey.kem_key);
  }

  // Caller is getting parameter values.
  if (ciphertext == NULL) {
    *ciphertext_len = kem->ciphertext_len;
    *shared_secret_len = kem->shared_secret_len;
    return 1;
  }

  // The output buffers need to be large enough.
  if (*ciphertext_len < kem->ciphertext_len ||
      *shared_secret_len < kem->shared_secret_len) {
      OPENSSL_PUT_ERROR(EVP, EVP_R_BUFFER_TOO_SMALL);
      return 0;
  }

  // Check that the context is properly configured.
  if (ctx->pkey == NULL ||
      ctx->pkey->pkey.kem_key == NULL ||
      ctx->pkey->type != EVP_PKEY_KEM) {
      OPENSSL_PUT_ERROR(EVP, EVP_R_OPERATON_NOT_INITIALIZED);
      return 0;
  }

  KEM_KEY *key = ctx->pkey->pkey.kem_key;
  if (!kem->method->encaps(ciphertext, shared_secret, key->public_key)) {
    return 0;
  }

  // The size of the ciphertext and the shared secret
  // that has been writen to the output buffers.
  *ciphertext_len = kem->ciphertext_len;
  *shared_secret_len = kem->shared_secret_len;

  return 1;
}

static int pkey_kem_decapsulate(EVP_PKEY_CTX *ctx,
                                uint8_t *shared_secret,
                                size_t  *shared_secret_len,
                                const uint8_t *ciphertext,
                                size_t ciphertext_len) {
  KEM_PKEY_CTX *dctx = ctx->data;
  const KEM *kem = dctx->kem;
  if (kem == NULL) {
    if (ctx->pkey == NULL) {
      OPENSSL_PUT_ERROR(EVP, EVP_R_NO_PARAMETERS_SET);
      return 0;
    }
    kem = KEM_KEY_get0_kem(ctx->pkey->pkey.kem_key);
  }

  // Caller is getting parameter values.
  if (shared_secret == NULL) {
    *shared_secret_len = kem->shared_secret_len;
    return 1;
  }

  // The input and output buffers need to be large enough.
  if (ciphertext_len < kem->ciphertext_len ||
      *shared_secret_len < kem->shared_secret_len) {
      OPENSSL_PUT_ERROR(EVP, EVP_R_BUFFER_TOO_SMALL);
      return 0;
  }

  // Check that the context is properly configured.
  if (ctx->pkey == NULL ||
      ctx->pkey->pkey.kem_key == NULL ||
      ctx->pkey->type != EVP_PKEY_KEM) {
      OPENSSL_PUT_ERROR(EVP, EVP_R_OPERATON_NOT_INITIALIZED);
      return 0;
  }

  KEM_KEY *key = ctx->pkey->pkey.kem_key;
  if (!key->has_secret_key) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_NO_KEY_SET);
    return 0;
  }

  if (!kem->method->decaps(shared_secret, ciphertext, key->secret_key)) {
    return 0;
  }

  // The size of the shared secret that has been writen to the output buffer.
  *shared_secret_len = kem->shared_secret_len;

  return 1;
}

const EVP_PKEY_METHOD kem_pkey_meth = {
    EVP_PKEY_KEM,
    pkey_kem_init,
    NULL,
    pkey_kem_cleanup,
    pkey_kem_keygen,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    pkey_kem_encapsulate,
    pkey_kem_decapsulate,
};

// Additional KEM specific EVP functions.

int EVP_PKEY_CTX_kem_set_params(EVP_PKEY_CTX *ctx, int nid) {
  if (ctx == NULL || ctx->data == NULL) {
    OPENSSL_PUT_ERROR(EVP, ERR_R_PASSED_NULL_PARAMETER);
    return 0;
  }

  // It's not allowed to change context parameters if
  // a PKEY is already associated with the contex.
  if (ctx->pkey != NULL) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_INVALID_OPERATION);
    return 0;
  }

  const KEM *kem = KEM_find_kem_by_nid(nid);
  if (kem == NULL) {
    return 0;
  }

  KEM_PKEY_CTX *dctx = ctx->data;
  dctx->kem = kem;

  return 1;
}


// This function sets KEM parameters defined by |nid| in |pkey|.
static int EVP_PKEY_kem_set_params(EVP_PKEY *pkey, int nid) {
  const KEM *kem = KEM_find_kem_by_nid(nid);
  if (kem == NULL) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_UNSUPPORTED_ALGORITHM);
    return 0;
  }

  if (!EVP_PKEY_set_type(pkey, EVP_PKEY_KEM)) {
    // EVP_PKEY_set_type sets the appropriate error.
    return 0;
  }

  KEM_KEY *key = KEM_KEY_new();
  if (key == NULL) {
    // KEM_KEY_new sets the appropriate error.
    return 0;
  }

  key->kem = kem;
  pkey->pkey.kem_key = key;

  return 1;
}

// Returns a fresh EVP_PKEY object of type EVP_PKEY_KEM,
// and sets KEM parameters defined by |nid|.
static EVP_PKEY *EVP_PKEY_kem_new(int nid) {
  EVP_PKEY *ret = EVP_PKEY_new();
  if (ret == NULL || !EVP_PKEY_kem_set_params(ret, nid)) {
    EVP_PKEY_free(ret);
    return NULL;
  }

  return ret;
}

EVP_PKEY *EVP_PKEY_kem_new_raw_public_key(int nid, const uint8_t *in, size_t len) {
  if (in == NULL) {
    OPENSSL_PUT_ERROR(EVP, ERR_R_PASSED_NULL_PARAMETER);
    return NULL;
  }

  EVP_PKEY *ret = EVP_PKEY_kem_new(nid);
  if (ret == NULL || ret->pkey.kem_key == NULL) {
    // EVP_PKEY_kem_new sets the appropriate error.
    goto err;
  }

  const KEM *kem = KEM_KEY_get0_kem(ret->pkey.kem_key);
  if (kem->public_key_len != len) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_INVALID_BUFFER_SIZE);
    goto err;
  }

  if (!KEM_KEY_set_raw_public_key(ret->pkey.kem_key, in)) {
    // KEM_KEY_set_raw_public_key sets the appropriate error.
    goto err;
  }

  return ret;

err:
  EVP_PKEY_free(ret);
  return NULL;
}

EVP_PKEY *EVP_PKEY_kem_new_raw_secret_key(int nid, const uint8_t *in, size_t len) {
  if (in == NULL) {
    OPENSSL_PUT_ERROR(EVP, ERR_R_PASSED_NULL_PARAMETER);
    return NULL;
  }

  EVP_PKEY *ret = EVP_PKEY_kem_new(nid);
  if (ret == NULL || ret->pkey.kem_key == NULL) {
    // EVP_PKEY_kem_new sets the appropriate error.
    goto err;
  }

  const KEM *kem = KEM_KEY_get0_kem(ret->pkey.kem_key);
  if (kem->secret_key_len != len) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_INVALID_BUFFER_SIZE);
    goto err;
  }

  if (!KEM_KEY_set_raw_secret_key(ret->pkey.kem_key, in)) {
    // KEM_KEY_set_raw_secret_key sets the appropriate error.
    goto err;
  }

  return ret;

err:
  EVP_PKEY_free(ret);
  return NULL;
}

EVP_PKEY *EVP_PKEY_kem_new_raw_key(int nid,
                                   const uint8_t *in_public, size_t len_public,
                                   const uint8_t *in_secret, size_t len_secret) {
  if (in_public == NULL || in_secret == NULL) {
    OPENSSL_PUT_ERROR(EVP, ERR_R_PASSED_NULL_PARAMETER);
    return NULL;
  }

  EVP_PKEY *ret = EVP_PKEY_kem_new(nid);
  if (ret == NULL || ret->pkey.kem_key == NULL) {
    // EVP_PKEY_kem_new sets the appropriate error.
    goto err;
  }

  const KEM *kem = KEM_KEY_get0_kem(ret->pkey.kem_key);
  if (kem->public_key_len != len_public || kem->secret_key_len != len_secret) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_INVALID_BUFFER_SIZE);
    goto err;
  }
  
  if (!KEM_KEY_set_raw_key(ret->pkey.kem_key, in_public, in_secret)) {
    // KEM_KEY_set_raw_key sets the appropriate error.
    goto err;
  }

  return ret;

err:
  EVP_PKEY_free(ret);
  return NULL;
}
