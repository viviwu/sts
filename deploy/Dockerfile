# Minimal image for the local paper drill (M3-4 slot).
# Real deployment images (hardened base, non-root, signed) land with M3 infra.
FROM ubuntu:24.04

RUN apt-get update -q && apt-get install -y -q --no-install-recommends \
      g++ cmake make git ca-certificates \
    && rm -rf /var/lib/apt/lists/*
# PostgreSQL-gated image (ADR-038): add libpq-dev above and pass
# -DQTS_ENABLE_PG=ON below. The default stays OFF (zero system deps).

WORKDIR /qts
COPY . /qts
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DQTS_BUILD_TESTS=OFF \
 && cmake --build build -j"$(nproc)"

# Ledger/sequence files go here; mount a persistent volume in compose.
RUN mkdir -p /var/lib/qts
