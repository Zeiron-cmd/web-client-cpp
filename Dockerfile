FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt update && apt install -y \
    build-essential \
    cmake \
    git \
    libuuid1 uuid-dev \
    libhiredis-dev \
    libcurl4-openssl-dev \
    ca-certificates \
 && rm -rf /var/lib/apt/lists/*

# ===== redis-plus-plus =====
WORKDIR /opt
RUN git clone https://github.com/sewenew/redis-plus-plus.git
WORKDIR /opt/redis-plus-plus
RUN mkdir build && cd build \
 && cmake .. \
 && make -j$(nproc) \
 && make install \
 && ldconfig

# ===== Crow =====
WORKDIR /opt
RUN git clone https://github.com/CrowCpp/Crow.git
WORKDIR /opt/Crow
RUN cmake -B build \
 && cmake --build build -j$(nproc) \
 && cmake --install build

# ===== app =====
WORKDIR /app
COPY . .
RUN cmake -B build \
 && cmake --build build -j$(nproc)

CMD ["./build/web-client"]
