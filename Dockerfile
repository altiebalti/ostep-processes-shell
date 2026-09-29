# Minimal Debian with just enough to build and test the wish shell.
FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
        gcc \
        libc6-dev \
        make \
        diffutils \
        valgrind \
        gdb \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /work/processes-shell
CMD ["/bin/bash"]
