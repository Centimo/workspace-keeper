FROM ubuntu:24.04
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake pkg-config \
    qtbase5-dev qtdeclarative5-dev \
    libgl-dev \
    && rm -rf /var/lib/apt/lists/*
