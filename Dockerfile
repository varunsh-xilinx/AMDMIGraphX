FROM ubuntu:xenial-20180417

ARG PREFIX=/usr/local

# Support multiarch
RUN dpkg --add-architecture i386

# Add rocm repository
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y curl apt-utils wget
RUN curl https://raw.githubusercontent.com/RadeonOpenCompute/ROCm-docker/master/add-rocm.sh | bash

# Install dependencies
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --allow-unauthenticated \
    apt-utils \
    build-essential \
    clang-5.0 \
    clang-format-5.0 \
    clang-tidy-5.0 \
    cmake \
    curl \
    doxygen \
    gdb \
    git \
    hsa-rocr-dev \
    hsakmt-roct-dev \
    lcov \
    libelf-dev \
    libncurses5-dev \
    libpthread-stubs0-dev \
    libnuma-dev \
    python \
    python-dev \
    python-pip \
    rocminfo \
    rocm-opencl \
    rocm-opencl-dev \
    software-properties-common \
    wget && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

# Install cget
RUN pip install cget

# Install rclone
RUN pip install https://github.com/pfultz2/rclone/archive/master.tar.gz

# Install hcc
RUN rclone -b sanitizer1 https://github.com/RadeonOpenCompute/hcc.git /hcc
RUN cget -p $PREFIX install hcc,/hcc

# Use hcc
RUN cget -p $PREFIX init --cxx $PREFIX/bin/hcc

# Workaround hip's broken cmake
RUN ln -s $PREFIX /opt/rocm/hip
RUN ln -s $PREFIX /opt/rocm/hcc

# Install dependencies
ADD dev-requirements.txt /dev-requirements.txt
ADD requirements.txt /requirements.txt
RUN cget -p $PREFIX install -f /dev-requirements.txt

ENV LD_LIBRARY_PATH=$PREFIX/lib

# Install doc requirements
ADD doc/requirements.txt /doc-requirements.txt
RUN pip install -r /doc-requirements.txt
