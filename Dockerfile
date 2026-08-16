FROM ubuntu:24.04

# Update and install all build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    git \
    wget \
    flex \
    bison \
    libncurses-dev \
    libarchive-tools \
    python3 \
    python3-pip \
    gcc-aarch64-linux-gnu \
    make \
    unzip \
    iputils-ping

# Set working directory
WORKDIR /workspace

# Default command
CMD ["/bin/bash"]
