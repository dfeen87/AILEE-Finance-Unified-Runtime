FROM ubuntu:22.04

# Install build tools
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    curl

# Copy project
WORKDIR /app
COPY . .

# Build REST API server
RUN make rest_api_server

# Expose port
EXPOSE 8080

# Run server with dashboard mode
CMD ["./rest_api_server", "--dashboard"]
