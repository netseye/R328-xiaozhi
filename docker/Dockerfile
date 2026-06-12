FROM debian:bullseye-slim

RUN apt-get update && apt-get install -y \
    build-essential cmake git autoconf automake libtool \
    pkg-config wget python3 \
    && rm -rf /var/lib/apt/lists/*

COPY r328-sdk /opt/r328-toolchain
ENV PATH="/opt/r328-toolchain/bin:${PATH}"

WORKDIR /build
