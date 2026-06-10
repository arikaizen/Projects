"""PKCE (RFC 7636) utilities."""
import base64
import hashlib
import re
import secrets


def generate_code_verifier(length: int = 64) -> str:
    """Generate a cryptographically random code_verifier (43–128 chars)."""
    return secrets.token_urlsafe(length)[:128]


def compute_code_challenge(verifier: str, method: str = "S256") -> str:
    if method != "S256":
        raise ValueError(f"Unsupported code_challenge_method: {method}")
    digest = hashlib.sha256(verifier.encode()).digest()
    return base64.urlsafe_b64encode(digest).rstrip(b"=").decode()


def verify_pkce(verifier: str, challenge: str, method: str) -> bool:
    if method != "S256":
        return False
    expected = compute_code_challenge(verifier, method)
    return secrets.compare_digest(expected, challenge)


_VERIFIER_RE = re.compile(r"^[A-Za-z0-9\-._~]{43,128}$")


def validate_verifier_format(verifier: str) -> bool:
    return bool(_VERIFIER_RE.match(verifier))
