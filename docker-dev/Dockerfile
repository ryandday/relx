# Development/build image for relx: GCC 16 with C++26 reflection support.
# Build:  docker build -t relx-gcc16-dev docker-dev
# Usage:  docker run --rm -v $(pwd):/repo -w /repo relx-gcc16-dev \
#           bash -c 'cmake -B build-gcc16 -G Ninja -DRELX_DEV_MODE=ON && cmake --build build-gcc16 -j'
# To run the full test suite against the docker-compose postgres (host port 5434):
#   docker run --rm --add-host=host.docker.internal:host-gateway -v $(pwd):/repo -w /repo relx-gcc16-dev \
#     bash -c 'socat TCP-LISTEN:5434,fork,reuseaddr TCP:host.docker.internal:5434 & sleep 1; ./build-gcc16/test/relx_tests'
FROM gcc:16
# NOT libboost-all-dev: it pulls in openmpi/gfortran which conflicts with the
# gcc image's /usr/local toolchain (distro gfortran postinst fails)
RUN apt-get update && apt-get install -y \
    cmake \
    ninja-build \
    libboost-dev \
    libboost-system-dev \
    libboost-thread-dev \
    libpq-dev \
    postgresql-client \
    socat \
    git \
    && rm -rf /var/lib/apt/lists/*
