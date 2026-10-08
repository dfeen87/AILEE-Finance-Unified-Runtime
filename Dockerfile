# syntax=docker/dockerfile:1

# The versioned GCC image fixes the C/C++ toolchain while retaining Debian Bookworm.
FROM gcc:12.3.0-bookworm AS builder

ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update \
    && apt-get install --yes --no-install-recommends \
        libssl-dev \
        curl \
        git \
        python3-dev \
        python3-pip \
        python3-pytest \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

# Keep dependency and build policy in the repository's Makefile.
RUN make deps && make build
RUN python3 -c "import hashlib; from pathlib import Path; hashes = b''.join(hashlib.sha256(Path(name).read_bytes()).digest() for name in ('demo', 'test_suite')); Path('BUILD_HASH').write_text(hashlib.sha256(hashes).hexdigest() + '\n')"


FROM debian:12.7-slim AS runtime

ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update \
    && apt-get install --yes --no-install-recommends \
        ca-certificates \
        libpython3.11 \
        libssl3 \
        make \
    && rm -rf /var/lib/apt/lists/* \
    && groupadd --gid 10001 ailee \
    && useradd --uid 10001 --gid ailee --create-home --shell /usr/sbin/nologin ailee

WORKDIR /opt/ailee

# Runtime contains only built/test artifacts plus the Makefile test interface and docs.
COPY --from=builder --chown=ailee:ailee /src/demo /src/test_suite /src/BUILD_HASH ./
COPY --from=builder --chown=ailee:ailee /src/Makefile /src/README.md /src/LICENSE ./
COPY docker-entrypoint.sh /usr/local/bin/docker-entrypoint.sh

LABEL org.opencontainers.image.title="AILEE Finance Unified Runtime" \
      org.opencontainers.image.licenses="MIT" \
      org.opencontainers.image.documentation="/opt/ailee/README.md"

ENV REPRO_DIR=/repro
RUN mkdir -p /repro && chown ailee:ailee /repro \
    && chmod 0555 /usr/local/bin/docker-entrypoint.sh

USER ailee
EXPOSE 8080
ENTRYPOINT ["/usr/local/bin/docker-entrypoint.sh"]
CMD ["make", "test"]
