# AILLE Framework - REST API Documentation

## Overview

The AILLE Framework REST API provides HTTP endpoints for making validated algorithmic decisions over the network. This enables integration with:
- Web applications
- Microservices architectures
- Remote trading systems
- Multi-language environments (Python, JavaScript, Java, etc.)

## Quick Start

### 1. Setup

Download the required dependency:
```bash
./setup_rest_api.sh
```

Or manually:
```bash
mkdir -p external
curl -L -o external/httplib.h "https://raw.githubusercontent.com/yhirose/cpp-httplib/v0.14.3/httplib.h"
```

### 2. Build

```bash
make rest_api_server
```

### 3. Run

```bash
./rest_api_server [port] [host]
```

Default configuration:
- **Port:** 8080
- **Host:** 0.0.0.0 (accessible from all network interfaces)

Examples:
```bash
# Run on default port 8080, accessible globally
./rest_api_server

# Run on custom port
./rest_api_server 9000

# Run on specific interface
./rest_api_server 8080 127.0.0.1  # Local only
./rest_api_server 8080 0.0.0.0    # Global access
```

## API Endpoints

### GET /

**Description:** Welcome page with API documentation

**Response:** HTML page with usage examples

**Example:**
```bash
curl http://localhost:8080/
```

---

### GET /health

**Description:** Health check endpoint

**Response:**
```json
{
  "status": "healthy",
  "service": "AILLE Framework REST API",
  "timestamp": 1234567890123456789
}
```

**Example:**
```bash
curl http://localhost:8080/health
```

---

### GET /api/info

**Description:** Get API information and available endpoints

**Response:**
```json
{
  "name": "AILLE Framework REST API",
  "version": "1.0.0",
  "description": "AI-Load Integrity and Layered Evaluation Framework",
  "endpoints": {
    "GET /health": "Health check",
    "GET /api/info": "API information",
    "POST /api/decision": "Make a decision based on model signals"
  }
}
```

**Example:**
```bash
curl http://localhost:8080/api/info
```

---

### POST /api/decision

**Description:** Make a validated AILLE decision based on model signals

**Request Body:** JSON array of model signals

```json
[
  {
    "value": 0.05,
    "confidence": 0.85,
    "model_id": 0
  },
  {
    "value": 0.03,
    "confidence": 0.72,
    "model_id": 1
  }
]
```

**Request Fields:**
- `value` (float, required): Model prediction value
- `confidence` (float, required): Confidence score (0.0-1.0)
- `model_id` (int, optional): Model identifier (default: 0)

**Response:**
```json
{
  "status": "valid",
  "final_value": 0.0415,
  "confidence": 0.785,
  "models_agreed": 2,
  "fallback_used": false,
  "timestamp_ns": 1234567890123456789,
  "reasoning": "Consensus: 2 models",
  "contributing_models": [0, 1]
}
```

**Response Fields:**
- `status`: Decision status
  - `"valid"`: Decision passed all validation layers
  - `"rejected_low_confidence"`: Rejected due to low confidence
  - `"rejected_no_consensus"`: Models did not agree
  - `"fallback_activated"`: Using fallback mechanism
  - `"error_no_models"`: No models provided
- `final_value`: Validated decision value
- `confidence`: Overall confidence score
- `models_agreed`: Number of models that agreed
- `fallback_used`: Whether fallback was triggered
- `timestamp_ns`: Nanosecond timestamp
- `reasoning`: Human-readable explanation
- `contributing_models`: IDs of models that contributed

**Example:**
```bash
curl -X POST http://localhost:8080/api/decision \
  -H "Content-Type: application/json" \
  -d '[
    {"value": 0.05, "confidence": 0.85, "model_id": 0},
    {"value": 0.03, "confidence": 0.72, "model_id": 1},
    {"value": 0.02, "confidence": 0.68, "model_id": 2}
  ]'
```

**Error Response (400 Bad Request):**
```json
{
  "error": "Invalid request format. Expected array of signals with value, confidence, and optional model_id"
}
```

**Error Response (500 Internal Server Error):**
```json
{
  "error": "Internal error: [error message]"
}
```

## Usage Examples

### Python

```python
import requests
import json

# API endpoint
url = "http://localhost:8080/api/decision"

# Model signals
signals = [
    {"value": 0.05, "confidence": 0.85, "model_id": 0},
    {"value": 0.03, "confidence": 0.72, "model_id": 1},
    {"value": 0.02, "confidence": 0.68, "model_id": 2}
]

# Make request
response = requests.post(url, json=signals)
decision = response.json()

# Process decision
if decision["status"] == "valid":
    print(f"Execute trade: {decision['final_value']}")
    print(f"Confidence: {decision['confidence']}")
else:
    print(f"Decision rejected: {decision['reasoning']}")
```

### JavaScript (Node.js)

```javascript
const axios = require('axios');

// API endpoint
const url = 'http://localhost:8080/api/decision';

// Model signals
const signals = [
    { value: 0.05, confidence: 0.85, model_id: 0 },
    { value: 0.03, confidence: 0.72, model_id: 1 },
    { value: 0.02, confidence: 0.68, model_id: 2 }
];

// Make request
axios.post(url, signals)
    .then(response => {
        const decision = response.data;
        if (decision.status === 'valid') {
            console.log(`Execute trade: ${decision.final_value}`);
            console.log(`Confidence: ${decision.confidence}`);
        } else {
            console.log(`Decision rejected: ${decision.reasoning}`);
        }
    })
    .catch(error => {
        console.error('Error:', error.message);
    });
```

### cURL

```bash
# Health check
curl http://localhost:8080/health

# Get API info
curl http://localhost:8080/api/info

# Make a decision
curl -X POST http://localhost:8080/api/decision \
  -H "Content-Type: application/json" \
  -d '[{"value": 0.05, "confidence": 0.85, "model_id": 0}]'

# Pretty print JSON response
curl -X POST http://localhost:8080/api/decision \
  -H "Content-Type: application/json" \
  -d '[{"value": 0.05, "confidence": 0.85, "model_id": 0}]' \
  | python -m json.tool
```

## Network Access Configuration

### Global Network Access (Default)

The server binds to `0.0.0.0` by default, making it accessible from:
- Localhost: `http://localhost:8080`
- LAN: `http://192.168.x.x:8080`
- WAN: `http://your-public-ip:8080` (requires firewall/port forwarding)

### Local-Only Access

For security, you can restrict access to localhost:

```bash
./rest_api_server 8080 127.0.0.1
```

Now only accessible via `http://localhost:8080`

### Firewall Configuration

**Ubuntu/Debian:**
```bash
# Allow port 8080
sudo ufw allow 8080/tcp

# Allow from specific IP
sudo ufw allow from 192.168.1.100 to any port 8080
```

**CentOS/RHEL:**
```bash
# Allow port 8080
sudo firewall-cmd --permanent --add-port=8080/tcp
sudo firewall-cmd --reload
```

## Security Considerations

⚠️ **Important Security Notes:**

1. **No Built-in Authentication**: The API does not include authentication. For production:
   - Use a reverse proxy (nginx, Apache) with authentication
   - Implement API keys or OAuth
   - Use HTTPS/TLS encryption

2. **Input Validation**: All inputs are validated, but consider additional sanitization for your use case

3. **Rate Limiting**: Consider implementing rate limiting in production

4. **Network Security**: 
   - Use firewall rules to restrict access
   - Consider VPN for remote access
   - Use HTTPS in production

## Configuration

The AILLE engine configuration can be customized in `examples/rest_api_server.cpp`:

```cpp
AILLE::AILLEConfig config;
config.min_confidence_threshold = 0.40f;  // Minimum confidence (0.0-1.0)
config.min_models_required = 2;           // Minimum agreeing models
config.fallback_window_size = 100;        // Historical window for fallback
```

Rebuild after changes:
```bash
make rest_api_server
```

## Performance

- **Latency**: <100 microseconds for decision-making
- **Throughput**: Depends on hardware, typically >10,000 requests/second
- **Concurrency**: cpp-httplib handles concurrent requests

## Troubleshooting

### Port Already in Use

```
Error: Address already in use
```

**Solution:** Use a different port or stop the conflicting service
```bash
./rest_api_server 9000  # Use port 9000 instead
```

### Cannot Connect from Remote Machine

1. Check firewall settings
2. Verify server is bound to `0.0.0.0`, not `127.0.0.1`
3. Check network routing

### Compilation Errors

**Missing httplib.h:**
```bash
./setup_rest_api.sh
```

**C++17 support:**
```bash
# Update compiler
g++ --version  # Should be 7.0 or higher
```

## Dependencies

- **cpp-httplib**: Single-header HTTP library (MIT License)
  - Version: v0.14.3
  - Repository: https://github.com/yhirose/cpp-httplib

## License

MIT License - Same as AILLE Framework

## Support

- 📧 **Email**: dfeen87@gmail.com
- 💼 **LinkedIn**: www.linkedin.com/in/don-michael-feeney-jr-908a96351

## See Also

- [AILLE Framework README](../README.md)
- [AILLE Quick Start](../QUICKSTART.md)
- [Example Usage](../examples/)
