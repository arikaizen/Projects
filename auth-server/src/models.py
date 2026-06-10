"""Pydantic models for the OAuth 2.1 authorization server."""
import secrets
from datetime import datetime, timezone
from typing import Optional
from pydantic import BaseModel, Field


class ClientRegistration(BaseModel):
    client_name: str
    redirect_uris: list[str]
    grant_types: list[str] = ["authorization_code", "refresh_token"]
    response_types: list[str] = ["code"]
    scope: str = "tools:read tools:call"
    token_endpoint_auth_method: str = "none"  # public client (PKCE)


class ClientRecord(BaseModel):
    client_id: str
    client_name: str
    redirect_uris: list[str]
    grant_types: list[str]
    response_types: list[str]
    scope: str
    created_at: datetime = Field(default_factory=lambda: datetime.now(timezone.utc))


class AuthCode(BaseModel):
    code: str
    client_id: str
    redirect_uri: str
    scope: str
    code_challenge: str
    code_challenge_method: str
    expires_at: datetime
    used: bool = False


class TokenRecord(BaseModel):
    jti: str                    # JWT ID — unique per token
    client_id: str
    scope: str
    issued_at: datetime
    expires_at: datetime
    token_type: str = "Bearer"
    revoked: bool = False


class RefreshTokenRecord(BaseModel):
    token: str
    client_id: str
    scope: str
    issued_at: datetime
    expires_at: datetime
    revoked: bool = False


class InMemoryStore:
    """Thread-unsafe in-memory store; swap for Redis/DB in production."""

    def __init__(self) -> None:
        self.clients: dict[str, ClientRecord] = {}
        self.auth_codes: dict[str, AuthCode] = {}
        self.tokens: dict[str, TokenRecord] = {}       # keyed by jti
        self.refresh_tokens: dict[str, RefreshTokenRecord] = {}

    # ── clients ──────────────────────────────────────────────────────────────

    def add_client(self, reg: ClientRegistration) -> ClientRecord:
        cid = "client_" + secrets.token_urlsafe(16)
        record = ClientRecord(
            client_id=cid,
            client_name=reg.client_name,
            redirect_uris=reg.redirect_uris,
            grant_types=reg.grant_types,
            response_types=reg.response_types,
            scope=reg.scope,
        )
        self.clients[cid] = record
        return record

    def get_client(self, client_id: str) -> Optional[ClientRecord]:
        return self.clients.get(client_id)

    # ── auth codes ────────────────────────────────────────────────────────────

    def save_auth_code(self, code: AuthCode) -> None:
        self.auth_codes[code.code] = code

    def consume_auth_code(self, code: str) -> Optional[AuthCode]:
        rec = self.auth_codes.get(code)
        if rec and not rec.used:
            rec.used = True
            return rec
        return None

    # ── access tokens ─────────────────────────────────────────────────────────

    def save_token(self, record: TokenRecord) -> None:
        self.tokens[record.jti] = record

    def get_token(self, jti: str) -> Optional[TokenRecord]:
        return self.tokens.get(jti)

    def revoke_token(self, jti: str) -> None:
        if jti in self.tokens:
            self.tokens[jti].revoked = True

    # ── refresh tokens ────────────────────────────────────────────────────────

    def save_refresh_token(self, record: RefreshTokenRecord) -> None:
        self.refresh_tokens[record.token] = record

    def consume_refresh_token(self, token: str) -> Optional[RefreshTokenRecord]:
        rec = self.refresh_tokens.get(token)
        if rec and not rec.revoked:
            rec.revoked = True
            return rec
        return None
