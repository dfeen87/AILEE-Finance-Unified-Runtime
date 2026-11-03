"""Platform link requirements of the pinned TLS-enabled HTTP dependency."""
import sys


def tls_platform_flags():
    return ["-framework", "Security", "-framework", "CoreFoundation"] if sys.platform == "darwin" else []
