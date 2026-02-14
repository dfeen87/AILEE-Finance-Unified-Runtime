#!/bin/bash
# AILLE Framework - Setup Script
# Downloads external dependencies required for REST API

set -e

echo "=== AILLE Framework Setup ==="
echo ""
echo "Downloading external dependencies..."
echo ""

# Create external directory if it doesn't exist
mkdir -p external

# Download cpp-httplib
echo "Downloading cpp-httplib..."
curl -L -o external/httplib.h "https://raw.githubusercontent.com/yhirose/cpp-httplib/v0.14.3/httplib.h"

# Verify download
if [ -f external/httplib.h ]; then
    SIZE=$(stat -f%z external/httplib.h 2>/dev/null || stat -c%s external/httplib.h 2>/dev/null)
    echo "✓ cpp-httplib downloaded successfully (${SIZE} bytes)"
else
    echo "✗ Failed to download cpp-httplib"
    exit 1
fi

echo ""
echo "=== Setup Complete ==="
echo ""
echo "You can now build the REST API server:"
echo "  make rest_api_server"
echo ""
