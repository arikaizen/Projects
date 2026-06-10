"""JWT signing and verification using RS256."""
import os
import secrets
from datetime import datetime, timedelta, timezone
from typing import Any

from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric import rsa
from jose import JWTError, jwt


_ACCESS_TOKEN_TTL_SECONDS = int(os.getenv("ACCESS_TOKEN_TTL", "3600"))
_REFRESH_TOKEN_TTL_SECONDS = int(os.getenv("REFRESH_TOKEN_TTL", "86400"))
_ISSUER = os.getenv("AUTH_SERVER_ISSUER", "http://localhost:8080")
_AUDIENCE = os.getenv("MCP_SERVER_AUDIENCE", "http://localhost:8081")


def _generate_rsa_keypair():
    private = rsa.generate_private_key(public_exponent=65537, key_size=2048)
    return private, private.public_key()


_PRIVATE_KEY, _PUBLIC_KEY = _generate_rsa_keypair()

_PRIVATE_PEM = _PRIVATE_KEY.private_bytes(
    serialization.Encoding.PEM,
    serialization.PrivateFormat.TraditionalOpenSSL,
    serialization.NoEncryption(),
)
_PUBLIC_PEM = _PUBLIC_KEY.public_bytes(
    serialization.Encoding.PEM,
    serialization.PublicFormat.SubjectPublicKeyInfo,
)

# JWKS representation
from cryptography.hazmat.primitives.asymmetric.rsa import RSAPublicKey
from cryptography.hazmat.primitives.serialization import Encoding, PublicFormat
import base64
import struct

def _int_to_base64url(n: int) -> str:
    length = (n.bit_length() + 7) // 8
    return base64.urlsafe_b64encode(n.to_bytes(length, "big")).rstrip(b"=").decode()

def get_jwks() -> dict:
    pub_numbers = _PUBLIC_KEY.public_numbers()
    return {
        "keys": [{
            "kty": "RSA",
            "use": "sig",
            "alg": "RS256",
            "kid": "1",
            "n": _int_to_base64url(pub_numbers.n),
            "e": _int_to_base64url(pub_numbers.e),
        }]
    }


def create_access_token(client_id: str, scope: str) -> tuple[str, str, datetime]:
    """Returns (jwt_string, jti, expires_at)."""
    now = datetime.now(timezone.utc)
    expires_at = now + timedelta(seconds=_ACCESS_TOKEN_TTL_SECONDS)
    jti = secrets.token_urlsafe(16)
    claims = {
        "iss": _ISSUER,
        "aud": _AUDIENCE,
        "sub": client_id,
        "client_id": client_id,
        "scope": scope,
        "jti": jti,
        "iat": int(now.timestamp()),
        "exp": int(expires_at.timestamp()),
    }
    token = jwt.encode(claims, _PRIVATE_PEM.decode(), algorithm="RS256",
                       headers={"kid": "1"})
    return token, jti, expires_at


def decode_access_token(token: str) -> dict[str, Any]:
    """Decode and verify; raises JWTError on failure."""
    return jwt.decode(
        token,
        _PUBLIC_PEM.decode(),
        algorithms=["RS256"],
        audience=_AUDIENCE,
        issuer=_ISSUER,
        options={"verify_exp": True},
    )


ACCESS_TOKEN_TTL = _ACCESS_TOKEN_TTL_SECONDS
REFRESH_TOKEN_TTL = _REFRESH_TOKEN_TTL_SECONDS
ISSUER = _ISSUER
AUDIENCE = _AUDIENCE
