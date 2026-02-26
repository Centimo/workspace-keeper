FROM ubuntu:24.04
COPY runtime-deps.txt /tmp/runtime-deps.txt
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake pkg-config \
    qt6-base-dev \
    libgl-dev \
    libsystemd-dev \
    libxcb1-dev \
    $(cat /tmp/runtime-deps.txt) \
    && rm -rf /var/lib/apt/lists/*
