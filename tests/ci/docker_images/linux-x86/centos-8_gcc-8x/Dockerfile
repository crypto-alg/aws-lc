# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: Apache-2.0 OR ISC

FROM centos:8

SHELL ["/bin/bash", "-c"]

RUN set -ex && \
    sed -i 's/mirrorlist/#mirrorlist/g' /etc/yum.repos.d/CentOS-* && \
    sed -i 's|#baseurl=http://mirror.centos.org|baseurl=http://vault.centos.org|g' /etc/yum.repos.d/CentOS-* && \
    yum -y update && \
    yum --enablerepo=extras install epel-release -y && \
    yum -y install \
    gcc \
    libgcc.i686 \
    glibc-devel.x86_64 \
    glibc-devel.i686 \
    gcc-c++ \
    libstdc++-devel.x86_64 \
    libstdc++-devel.i686 \
    cmake3 \
    ninja-build \
    perl \
    golang \
    wget \
    unzip && \
    # Based on https://docs.aws.amazon.com/cli/latest/userguide/install-cliv2-linux.html
    curl "https://awscli.amazonaws.com/awscli-exe-linux-x86_64.zip" -o "awscliv2.zip" && \
    unzip awscliv2.zip && \
    ./aws/install --bin-dir /usr/bin && \
    rm -rf awscliv2.zip aws/ && \
    yum clean packages && \
    yum clean metadata && \
    yum clean all && \
    rm -rf /tmp/* && \
    rm -rf /var/cache/yum

ENV CC=gcc
ENV CXX=g++
ENV GOROOT=/usr/local/go
ENV PATH="$GOROOT/bin:$PATH"