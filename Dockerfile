# Build stage
FROM ubuntu:22.04 AS builder

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    pkg-config \
    git \
    libsrt-dev \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /app

# Copy source files
COPY . .

# Build the project
RUN mkdir build && cd build \
    && cmake .. \
    && make -j$(nproc)

# Runtime stage
FROM ubuntu:22.04

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
    libsrt-dev \
    && rm -rf /var/lib/apt/lists/*

# Copy built executables and configs
COPY --from=builder /app/build/visca_srt_server /usr/local/bin/
COPY --from=builder /app/build/visca_srt_client /usr/local/bin/
COPY --from=builder /app/build/srt_example /usr/local/bin/
COPY config/ /etc/visca_srt/

# Set default command (can be overridden)
CMD ["visca_srt_server"]
