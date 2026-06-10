import 'dart:convert';
import 'dart:math';
import 'package:crypto/crypto.dart';
import 'package:http/http.dart' as http;

import '../models/auth_model.dart';

// ── Legacy password auth ───────────────────────────────────────────────────────

class AuthService {
  static final _users = [
    AuthUser(
      username: 'admin',
      passwordHash: AuthUser.hashPassword('admin123'),
      role: UserRole.admin,
    ),
    AuthUser(
      username: 'user',
      passwordHash: AuthUser.hashPassword('user123'),
      role: UserRole.user,
    ),
  ];

  final List<AuthUser> _extra = [];

  Future<AuthSession?> login(String username, String password) async {
    final all = [..._users, ..._extra];
    try {
      final u = all.firstWhere(
        (u) => u.username == username && u.checkPassword(password),
      );
      return AuthSession(user: u);
    } catch (_) {
      return null;
    }
  }

  void addUser(String username, String password, UserRole role) {
    _extra.add(AuthUser(
      username: username,
      passwordHash: AuthUser.hashPassword(password),
      role: role,
    ));
  }

  List<AuthUser> get allUsers => [..._users, ..._extra];
}

// ── OAuth 2.1 + PKCE client (steps ①–②) ──────────────────────────────────────

class OAuthService {
  final String authServerBase;
  final String mcpServerAudience;

  OAuthClientRegistration? _registration;
  OAuthToken? _token;
  PkceState? _pendingPkce;

  OAuthService({
    required this.authServerBase,
    required this.mcpServerAudience,
  });

  // ── step ①: dynamic client registration ───────────────────────────────────

  Future<OAuthClientRegistration> register({
    required String clientName,
    required List<String> redirectUris,
    String scope = 'tools:read tools:call',
  }) async {
    final resp = await http.post(
      Uri.parse('$authServerBase/register'),
      headers: {'Content-Type': 'application/json'},
      body: jsonEncode({
        'client_name': clientName,
        'redirect_uris': redirectUris,
        'scope': scope,
        'grant_types': ['authorization_code', 'refresh_token'],
        'response_types': ['code'],
        'token_endpoint_auth_method': 'none',
      }),
    );
    if (resp.statusCode != 201) {
      throw Exception('Client registration failed: ${resp.body}');
    }
    _registration = OAuthClientRegistration.fromJson(
      jsonDecode(resp.body) as Map<String, dynamic>,
    );
    return _registration!;
  }

  // ── step ①: build authorization URL with PKCE ─────────────────────────────

  String buildAuthorizationUrl({
    String scope = 'tools:read tools:call',
    String? redirectUri,
  }) {
    if (_registration == null) {
      throw StateError('Call register() before buildAuthorizationUrl()');
    }
    final verifier = _generateCodeVerifier();
    final challenge = _computeCodeChallenge(verifier);
    final stateVal = _generateState();
    final redirect = redirectUri ?? _registration!.redirectUris.first;

    _pendingPkce = PkceState(
      codeVerifier: verifier,
      codeChallenge: challenge,
      state: stateVal,
      redirectUri: redirect,
    );

    final params = {
      'response_type': 'code',
      'client_id': _registration!.clientId,
      'redirect_uri': redirect,
      'scope': scope,
      'state': stateVal,
      'code_challenge': challenge,
      'code_challenge_method': 'S256',
    };
    final uri = Uri.parse('$authServerBase/authorize')
        .replace(queryParameters: params);
    return uri.toString();
  }

  // ── step ②: exchange authorization code for tokens ────────────────────────

  Future<OAuthToken> exchangeCode(String code, {String? state}) async {
    if (_registration == null || _pendingPkce == null) {
      throw StateError('No pending PKCE flow — call buildAuthorizationUrl() first');
    }
    if (state != null && state != _pendingPkce!.state) {
      throw Exception('OAuth state mismatch — possible CSRF');
    }

    final resp = await http.post(
      Uri.parse('$authServerBase/token'),
      headers: {'Content-Type': 'application/x-www-form-urlencoded'},
      body: {
        'grant_type': 'authorization_code',
        'client_id': _registration!.clientId,
        'redirect_uri': _pendingPkce!.redirectUri,
        'code': code,
        'code_verifier': _pendingPkce!.codeVerifier,
      },
    );
    if (resp.statusCode != 200) {
      throw Exception('Token exchange failed: ${resp.body}');
    }
    _token = OAuthToken.fromJson(jsonDecode(resp.body) as Map<String, dynamic>);
    _pendingPkce = null;
    return _token!;
  }

  // ── step ②: refresh expired access token ──────────────────────────────────

  Future<OAuthToken> refreshToken() async {
    if (_registration == null) throw StateError('Not registered');
    final rt = _token?.refreshToken;
    if (rt == null) throw StateError('No refresh token available');

    final resp = await http.post(
      Uri.parse('$authServerBase/token'),
      headers: {'Content-Type': 'application/x-www-form-urlencoded'},
      body: {
        'grant_type': 'refresh_token',
        'client_id': _registration!.clientId,
        'refresh_token': rt,
      },
    );
    if (resp.statusCode != 200) {
      throw Exception('Token refresh failed: ${resp.body}');
    }
    _token = OAuthToken.fromJson(jsonDecode(resp.body) as Map<String, dynamic>);
    return _token!;
  }

  // ── helpers ────────────────────────────────────────────────────────────────

  /// Returns a valid access token, refreshing automatically if expired.
  Future<String> getValidAccessToken() async {
    if (_token == null) throw StateError('Not authenticated');
    if (_token!.isExpired) await refreshToken();
    return _token!.accessToken;
  }

  OAuthToken? get currentToken => _token;
  bool get isAuthenticated => _token != null && !_token!.isExpired;

  // ── PKCE helpers (RFC 7636) ────────────────────────────────────────────────

  static String _generateCodeVerifier({int length = 64}) {
    const chars =
        'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~';
    final rng = Random.secure();
    return List.generate(length, (_) => chars[rng.nextInt(chars.length)]).join();
  }

  static String _computeCodeChallenge(String verifier) {
    final bytes = utf8.encode(verifier);
    final digest = sha256.convert(bytes);
    return base64Url.encode(digest.bytes).replaceAll('=', '');
  }

  static String _generateState({int length = 16}) {
    const chars = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789';
    final rng = Random.secure();
    return List.generate(length, (_) => chars[rng.nextInt(chars.length)]).join();
  }
}
