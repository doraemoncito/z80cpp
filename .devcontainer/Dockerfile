FROM ubuntu:22.04

# Install build dependencies
RUN apt-get update && export DEBIAN_FRONTEND=noninteractive \
    && apt-get -y install --no-install-recommends \
    cmake \
    make \
    git \
    curl \
    ca-certificates \
    xz-utils \
    sudo \
    g++ \
    gcc \
    qemu-user-static \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Download and install ARM GNU toolchain for Raspberry Pi 4
# Detect architecture and download appropriate toolchain
RUN ARCH=$(uname -m) && \
    if [ "$ARCH" = "aarch64" ] || [ "$ARCH" = "arm64" ]; then \
        TOOLCHAIN_URL="https://armkeil.blob.core.windows.net/developer/files/downloads/gnu/15.2.rel1/binrel/arm-gnu-toolchain-15.2.rel1-aarch64-aarch64-none-linux-gnu.tar.xz"; \
    else \
        TOOLCHAIN_URL="https://armkeil.blob.core.windows.net/developer/files/downloads/gnu/15.2.rel1/binrel/arm-gnu-toolchain-15.2.rel1-x86_64-aarch64-none-linux-gnu.tar.xz"; \
    fi && \
    curl -L "$TOOLCHAIN_URL" -o /tmp/arm-toolchain.tar.xz && \
    mkdir -p /opt/arm-toolchain && \
    tar -xf /tmp/arm-toolchain.tar.xz -C /opt/arm-toolchain --strip-components=1 && \
    rm /tmp/arm-toolchain.tar.xz

# Add toolchain to PATH
ENV PATH="/opt/arm-toolchain/bin:${PATH}"

# Create a non-root user
ARG USERNAME=vscode
ARG USER_UID=1000
ARG USER_GID=$USER_UID

RUN groupadd --gid $USER_GID $USERNAME \
    && useradd --uid $USER_UID --gid $USER_GID -m $USERNAME \
    && echo $USERNAME ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/$USERNAME \
    && chmod 0440 /etc/sudoers.d/$USERNAME

# Create CMake toolchain file for ARM64 cross-compilation (Raspberry Pi 4)
RUN echo 'set(CMAKE_SYSTEM_NAME Linux)\n\
set(CMAKE_SYSTEM_PROCESSOR aarch64)\n\
\n\
set(CMAKE_C_COMPILER aarch64-none-linux-gnu-gcc)\n\
set(CMAKE_CXX_COMPILER aarch64-none-linux-gnu-g++)\n\
\n\
# Optimize for Raspberry Pi 4 (Cortex-A72)\n\
set(CMAKE_C_FLAGS_INIT "-mcpu=cortex-a72 -mtune=cortex-a72")\n\
set(CMAKE_CXX_FLAGS_INIT "-mcpu=cortex-a72 -mtune=cortex-a72")\n\
\n\
# Static link all libraries for tests (to run with QEMU)\n\
# For production builds on the target, remove -static flag\n\
set(CMAKE_EXE_LINKER_FLAGS_INIT "-static")\n\
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-static-libstdc++ -static-libgcc")\n\
\n\
set(CMAKE_FIND_ROOT_PATH /opt/arm-toolchain/aarch64-none-linux-gnu)\n\
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)\n\
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)\n\
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)\n\
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)\n\
\n\
# Use QEMU for running cross-compiled tests\n\
set(CMAKE_CROSSCOMPILING_EMULATOR /usr/bin/qemu-aarch64-static)\n' > /opt/toolchain-aarch64.cmake

USER $USERNAME
