import 'dart:async';
import 'dart:convert';
import 'dart:io';
import 'dart:math';

import 'package:crypto/crypto.dart';
import 'package:http/http.dart' as http;

import 'google_auth_types.dart';

GoogleAuthService createGoogleAuthService() => GoogleAuthService();

/// Desktop Google sign-in via the OAuth 2.0 "Installed App" loopback flow
/// (RFC 8252) with PKCE. No client secret is strictly required for the PKCE
/// loopback flow, but Google "Desktop app" clients issue one, so we pass it
/// through when supplied.
///
/// Flow:
///   1. Spin up a localhost HTTP server on an ephemeral port.
///   2. Open the system browser at Google's consent screen with our
///      redirect_uri = http://localhost:<port>.
///   3. Google redirects back with ?code=...; we capture it, shut the server.
///   4. Exchange the code (+ PKCE verifier) for tokens at Google's token URL.
class GoogleAuthService {
  static const _authEndpoint  = 'https://accounts.google.com/o/oauth2/v2/auth';
  static const _tokenEndpoint = 'https://oauth2.googleapis.com/token';

  bool get isSupported =>
      Platform.isLinux || Platform.isMacOS || Platform.isWindows;

  Future<GoogleTokens> signIn({
    required String clientId,
    String clientSecret = '',
    List<String> scopes = kGeminiScopes,
  }) async {
    final verifier  = _randomUrlSafe(64);
    final challenge = _s256(verifier);
    final state     = _randomUrlSafe(24);

    final server = await HttpServer.bind(InternetAddress.loopbackIPv4, 0);
    final redirectUri = 'http://localhost:${server.port}';

    final authUrl = Uri.parse(_authEndpoint).replace(queryParameters: {
      'client_id': clientId,
      'redirect_uri': redirectUri,
      'response_type': 'code',
      'scope': scopes.join(' '),
      'code_challenge': challenge,
      'code_challenge_method': 'S256',
      'state': state,
      'access_type': 'offline',
      'prompt': 'consent',
    });

    await _openBrowser(authUrl.toString());

    final code = await _awaitRedirect(server, state)
        .timeout(const Duration(minutes: 5), onTimeout: () {
      server.close(force: true);
      throw TimeoutException('Google sign-in timed out (no redirect received)');
    });

    return _exchangeCode(
      clientId: clientId,
      clientSecret: clientSecret,
      code: code,
      verifier: verifier,
      redirectUri: redirectUri,
    );
  }

  Future<GoogleTokens> refresh({
    required String clientId,
    required String refreshToken,
    String clientSecret = '',
  }) async {
    final res = await http.post(
      Uri.parse(_tokenEndpoint),
      body: {
        'client_id': clientId,
        if (clientSecret.isNotEmpty) 'client_secret': clientSecret,
        'refresh_token': refreshToken,
        'grant_type': 'refresh_token',
      },
    );
    if (res.statusCode != 200) {
      throw Exception('Google token refresh failed: ${res.body}');
    }
    final j = jsonDecode(res.body) as Map<String, dynamic>;
    // Google omits refresh_token on refresh; keep the original.
    j['refresh_token'] ??= refreshToken;
    return GoogleTokens.fromJson(j);
  }

  // ── internals ──────────────────────────────────────────────────────────────

  Future<String> _awaitRedirect(HttpServer server, String expectedState) async {
    await for (final req in server) {
      final params = req.uri.queryParameters;
      final code   = params['code'];
      final state  = params['state'];
      final error  = params['error'];

      req.response.headers.contentType = ContentType.html;
      if (error != null) {
        req.response.write(_html('Sign-in failed', 'Error: $error. You can close this tab.'));
        await req.response.close();
        await server.close(force: true);
        throw Exception('Google sign-in error: $error');
      }
      if (code == null) {
        // Ignore favicon and other stray requests.
        req.response.statusCode = HttpStatus.noContent;
        await req.response.close();
        continue;
      }
      if (state != expectedState) {
        req.response.write(_html('Sign-in failed', 'State mismatch — possible CSRF.'));
        await req.response.close();
        await server.close(force: true);
        throw Exception('Google sign-in state mismatch');
      }

      req.response.write(_html(
        'Signed in',
        'Authentication complete. You can close this tab and return to Agent Studio.',
      ));
      await req.response.close();
      await server.close(force: true);
      return code;
    }
    throw Exception('Google sign-in: redirect server closed unexpectedly');
  }

  Future<GoogleTokens> _exchangeCode({
    required String clientId,
    required String clientSecret,
    required String code,
    required String verifier,
    required String redirectUri,
  }) async {
    final res = await http.post(
      Uri.parse(_tokenEndpoint),
      body: {
        'client_id': clientId,
        if (clientSecret.isNotEmpty) 'client_secret': clientSecret,
        'code': code,
        'code_verifier': verifier,
        'grant_type': 'authorization_code',
        'redirect_uri': redirectUri,
      },
    );
    if (res.statusCode != 200) {
      throw Exception('Google token exchange failed: ${res.body}');
    }
    return GoogleTokens.fromJson(jsonDecode(res.body) as Map<String, dynamic>);
  }

  Future<void> _openBrowser(String url) async {
    try {
      if (Platform.isLinux) {
        await Process.run('xdg-open', [url]);
      } else if (Platform.isMacOS) {
        await Process.run('open', [url]);
      } else if (Platform.isWindows) {
        await Process.run('cmd', ['/c', 'start', '', url]);
      }
    } catch (_) {
      // If the browser can't be launched the user can copy the URL from logs.
      // ignore: avoid_print
      print('Open this URL to continue Google sign-in:\n$url');
    }
  }

  static String _randomUrlSafe(int bytes) {
    final rng = Random.secure();
    final data = List<int>.generate(bytes, (_) => rng.nextInt(256));
    return base64Url.encode(data).replaceAll('=', '');
  }

  static String _s256(String verifier) {
    final digest = sha256.convert(ascii.encode(verifier));
    return base64Url.encode(digest.bytes).replaceAll('=', '');
  }

  static String _html(String title, String body) =>
      '<!doctype html><html><head><meta charset="utf-8"><title>$title</title>'
      '<style>body{font-family:system-ui,sans-serif;background:#0d1117;color:#e6edf3;'
      'display:flex;align-items:center;justify-content:center;height:100vh;margin:0}'
      '.card{text-align:center;padding:40px;border:1px solid #30363d;border-radius:12px;'
      'background:#161b22}h1{font-size:18px;margin:0 0 8px}p{color:#8b949e;font-size:14px}'
      '</style></head><body><div class="card"><h1>$title</h1><p>$body</p></div></body></html>';
}
