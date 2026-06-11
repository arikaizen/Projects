import 'dart:convert';
import 'package:crypto/crypto.dart';

enum UserRole { admin, user }
enum AuthMethod { password, google }

// ── Local user (legacy password-based sessions) ──────────────────────────────

class AuthUser {
  final String username;
  final String passwordHash;
  final UserRole role;
  final AuthMethod method;
  final String? googleEmail;
  final String? googlePhotoUrl;

  const AuthUser({
    required this.username,
    required this.passwordHash,
    required this.role,
    this.method = AuthMethod.password,
    this.googleEmail,
    this.googlePhotoUrl,
  });

  static String hashPassword(String password) =>
      sha256.convert(utf8.encode(password)).toString();

  bool checkPassword(String password) =>
      method == AuthMethod.google || hashPassword(password) == passwordHash;

  factory AuthUser.fromGoogle({
    required String email,
    required String displayName,
    String? photoUrl,
    UserRole role = UserRole.user,
  }) => AuthUser(
    username: displayName,
    passwordHash: '',
    role: role,
    method: AuthMethod.google,
    googleEmail: email,
    googlePhotoUrl: photoUrl,
  );
}

class AuthSession {
  final AuthUser user;
  final DateTime createdAt;

  AuthSession({required this.user}) : createdAt = DateTime.now();

  bool get isAdmin      => user.role == UserRole.admin;
  bool get isGoogleUser => user.method == AuthMethod.google;
  String get username   => user.username;
  String? get photoUrl  => user.googlePhotoUrl;
  String? get email     => user.googleEmail;
}

// ── OAuth 2.1 tokens (step ②) ─────────────────────────────────────────────────

class OAuthToken {
  final String accessToken;
  final String tokenType;
  final int expiresIn;
  final String? refreshToken;
  final String scope;
  final DateTime issuedAt;

  OAuthToken({
    required this.accessToken,
    required this.tokenType,
    required this.expiresIn,
    this.refreshToken,
    required this.scope,
    DateTime? issuedAt,
  }) : issuedAt = issuedAt ?? DateTime.now();

  bool get isExpired =>
      DateTime.now().isAfter(issuedAt.add(Duration(seconds: expiresIn - 30)));

  factory OAuthToken.fromJson(Map<String, dynamic> json) => OAuthToken(
        accessToken: json['access_token'] as String,
        tokenType: json['token_type'] as String? ?? 'Bearer',
        expiresIn: json['expires_in'] as int? ?? 3600,
        refreshToken: json['refresh_token'] as String?,
        scope: json['scope'] as String? ?? '',
      );

  Map<String, dynamic> toJson() => {
        'access_token': accessToken,
        'token_type': tokenType,
        'expires_in': expiresIn,
        if (refreshToken != null) 'refresh_token': refreshToken,
        'scope': scope,
        'issued_at': issuedAt.toIso8601String(),
      };
}

// ── OAuth 2.1 client registration (step ①) ───────────────────────────────────

class OAuthClientRegistration {
  final String clientId;
  final String clientName;
  final List<String> redirectUris;
  final String scope;

  const OAuthClientRegistration({
    required this.clientId,
    required this.clientName,
    required this.redirectUris,
    required this.scope,
  });

  factory OAuthClientRegistration.fromJson(Map<String, dynamic> json) =>
      OAuthClientRegistration(
        clientId: json['client_id'] as String,
        clientName: json['client_name'] as String,
        redirectUris: (json['redirect_uris'] as List).cast<String>(),
        scope: json['scope'] as String? ?? '',
      );
}

// ── PKCE state (held in memory during the authorization flow) ─────────────────

class PkceState {
  final String codeVerifier;
  final String codeChallenge;
  final String state;
  final String redirectUri;

  const PkceState({
    required this.codeVerifier,
    required this.codeChallenge,
    required this.state,
    required this.redirectUri,
  });
}
