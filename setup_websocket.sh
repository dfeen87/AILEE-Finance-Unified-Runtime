#!/bin/bash
set -e

echo "=== AILLE Framework WebSocket Setup ==="
mkdir -p external

WEBSOCKETPP_VERSION="0.8.2"
ASIO_VERSION="asio-1-28-0"

cleanup() {
    rm -rf external/.websocketpp.tmp external/.asio.tmp
}
trap cleanup EXIT

# Download websocketpp
echo "Downloading websocketpp..."
if [ ! -f "external/websocketpp/websocketpp/config/asio_no_tls.hpp" ]; then
    rm -rf external/websocketpp external/.websocketpp.tmp
    git clone --depth 1 --branch "$WEBSOCKETPP_VERSION" \
        https://github.com/zaphoyd/websocketpp.git external/.websocketpp.tmp
    mv external/.websocketpp.tmp external/websocketpp
else
    echo "websocketpp ${WEBSOCKETPP_VERSION} already exists."
fi

# Download standalone ASIO (websocketpp can use it instead of boost)
echo "Downloading standalone ASIO..."
if [ ! -f "external/asio/asio/include/asio.hpp" ]; then
    rm -rf external/asio external/.asio.tmp
    git clone --depth 1 --branch "$ASIO_VERSION" \
        https://github.com/chriskohlhoff/asio.git external/.asio.tmp
    mv external/.asio.tmp external/asio
else
    echo "ASIO ${ASIO_VERSION} already exists."
fi

echo "=== Setup Complete ==="
