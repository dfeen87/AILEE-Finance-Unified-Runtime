#!/bin/bash
set -e

echo "=== AILLE Framework WebSocket Setup ==="
mkdir -p external

# Download websocketpp
echo "Downloading websocketpp..."
if [ ! -d "external/websocketpp" ]; then
    git clone --depth 1 --branch 0.8.2 https://github.com/zaphoyd/websocketpp.git external/websocketpp
else
    echo "websocketpp already exists."
fi

# Download standalone ASIO (websocketpp can use it instead of boost)
echo "Downloading standalone ASIO..."
if [ ! -d "external/asio" ]; then
    git clone --depth 1 --branch asio-1-28-0 https://github.com/chriskohlhoff/asio.git external/asio
else
    echo "asio already exists."
fi

echo "=== Setup Complete ==="
