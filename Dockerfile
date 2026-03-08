FROM ubuntu:24.04
COPY daemon/runtime-deps.txt /tmp/runtime-deps.txt
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake pkg-config \
    qtbase5-dev qtdeclarative5-dev \
    libgl-dev \
    libsystemd-dev \
    libxcb1-dev \
    $(cat /tmp/runtime-deps.txt) \
    && rm -rf /var/lib/apt/lists/*
