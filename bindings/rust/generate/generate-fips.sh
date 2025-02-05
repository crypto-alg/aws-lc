#!/usr/bin/env bash
# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: Apache-2.0 OR ISC

set -e

IGNORE_DIRTY=0
IGNORE_BRANCH=0
IGNORE_UPSTREAM=0
IGNORE_MACOS=0
SKIP_TEST=0
GENERATE_FIPS=1

AWS_LC_FIPS_SYS_VERSION="0.2.0"

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
AWS_LC_DIR=$( cd -- "${SCRIPT_DIR}/../../../" &> /dev/null && pwd)
AWS_LC_FIPS_BRANCH="fips-2022-11-02"
CRATE_TEMPLATE_DIR="${AWS_LC_DIR}"/bindings/rust/aws-lc-fips-sys-template
TMP_DIR="${AWS_LC_DIR}"/bindings/rust/tmp
AWS_LC_FIPS_DIR="${TMP_DIR}"/aws-lc
SYMBOLS_FILE="${TMP_DIR}"/symbols.txt
CRATE_DIR="${TMP_DIR}"/aws-lc-fips-sys
COMPLETION_MARKER="${CRATE_DIR}"/.generation_complete
CRATE_AWS_LC_DIR="${CRATE_DIR}"/deps/aws-lc
PREFIX_HEADERS_FILE="${CRATE_AWS_LC_DIR}"/include/boringssl_prefix_symbols.h

source "${SCRIPT_DIR}"/_generation_tools.sh

# Clone the FIPS branch in local. 
# TODO: This can be optimized to be ran and checked on the FIPS branch when this
# commit is in the latest FIPS branch.
function clone_fips_branch {
  pushd "${TMP_DIR}"
  rm -rf aws-lc
  git clone -b ${AWS_LC_FIPS_BRANCH} --depth 1 --single-branch https://github.com/awslabs/aws-lc.git
  popd
}

function prepare_crate_dir {
  echo Preparing crate directory: "${CRATE_DIR}"
  # Removes completion marker and any other file remaining from a previous crate generation
  rm -rf "${CRATE_DIR}"

  mkdir -p "${CRATE_DIR}"
  mkdir -p "${CRATE_AWS_LC_DIR}"/

  cp -r "${CRATE_TEMPLATE_DIR}"/* "${CRATE_DIR}"/
  perl -pi -e "s/__AWS_LC_FIPS_SYS_VERSION__/${AWS_LC_FIPS_SYS_VERSION}/g" "${CRATE_DIR}"/Cargo.toml

  cp -r "${AWS_LC_FIPS_DIR}"/crypto  \
        "${AWS_LC_FIPS_DIR}"/ssl  \
        "${AWS_LC_FIPS_DIR}"/include \
        "${AWS_LC_FIPS_DIR}"/tool \
        "${AWS_LC_FIPS_DIR}"/CMakeLists.txt \
        "${AWS_LC_FIPS_DIR}"/LICENSE \
        "${AWS_LC_FIPS_DIR}"/sources.cmake \
        "${AWS_LC_FIPS_DIR}"/go.mod \
        "${CRATE_AWS_LC_DIR}"/

  cp "${AWS_LC_FIPS_DIR}"/LICENSE  "${CRATE_AWS_LC_DIR}"/
  cp "${AWS_LC_FIPS_DIR}"/LICENSE  "${CRATE_DIR}"/

  mkdir -p "${CRATE_AWS_LC_DIR}"/util
  cp -r "${AWS_LC_FIPS_DIR}"/util/fipstools \
        "${AWS_LC_FIPS_DIR}"/util/godeps.go \
        "${AWS_LC_FIPS_DIR}"/util/ar \
        "${CRATE_AWS_LC_DIR}"/util

  mkdir -p "${CRATE_AWS_LC_DIR}"/third_party/
  cp -r  "${AWS_LC_FIPS_DIR}"/third_party/googletest \
          "${AWS_LC_FIPS_DIR}"/third_party/s2n-bignum \
          "${AWS_LC_FIPS_DIR}"/third_party/fiat \
          "${AWS_LC_FIPS_DIR}"/third_party/jitterentropy \
          "${CRATE_AWS_LC_DIR}"/third_party/

  mkdir -p  "${CRATE_AWS_LC_DIR}"/tests/compiler_features_tests
  cp "${AWS_LC_FIPS_DIR}"/tests/compiler_features_tests/*.c "${CRATE_AWS_LC_DIR}"/tests/compiler_features_tests
}

generation_options "$@"
shift $((OPTIND - 1))

if [[ ! -d ${AWS_LC_DIR} ]]; then
  echo "$(basename "${0}")" Sanity Check Failed
  exit 1
fi
pushd "${AWS_LC_DIR}"
check_workspace
mkdir -p "${TMP_DIR}"

# Crate preparation.
clone_fips_branch
prepare_crate_dir
create_prefix_headers
source "${SCRIPT_DIR}"/_generate_all_bindings_flavors.sh

# Crate testing.
if [[ ${SKIP_TEST} -eq 1 ]]; then
  echo Aborting. Crate generated but not tested.
  exit 1
fi
source "${SCRIPT_DIR}"/_test_supported_builds.sh

touch "${COMPLETION_MARKER}"
