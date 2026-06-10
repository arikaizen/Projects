import 'dart:async';
import 'dart:convert';
import 'dart:html' as html;
import 'dart:math';

import 'package:crypto/crypto.dart';
import 'package:http/http.dart' as http;

import 'google_auth_types.dart';

GoogleAuthService createGoogleAuthService() => GoogleAuthService();

/// Web Google sign-in via the OAuth 2.0 authorization-code + PKCE flow using a
/// popup window and postMessage. The redirect page (web/google_oauth.html)
/// posts the authorization code back to the opener, which exchanges it for
/// tokens. No client secret is used in the browser (public client).
class GoogleAuthService {
  static const _authEndpoint  = 'https://accounts.google.com/o/oauth2/v2/auth';
  static const _tokenEndpoint = 'https://oauth2.googleapis.com/token';

  bool get isSupported => true;

  Future<GoogleTokens> signIn({
    required String clientId,
    String clientSecret = '', // unused in the browser
    List<String> scopes = kGeminiScopes,
  }) async {
    final verifier  = _randomUrlSafe(64);
    final challenge = _s256(verifier);
    final state     = _randomUrlSafe(24);

    final redirectUri = '${html.window.location.origin}/google_oauth.html';

    final authUrl = Uri.parse(_authEndpoint).replace(queryParameters: {
      'client_id': clientId,
      'redirect_uri': redirectUri,
      'response_type': 'code',
      'scope': scopes.join(' '),
      'code_challenge': challenge,
      'code_challenge_method': 'S256',
      'state': state,
      'access_type': 'online',
      'prompt': 'consent',
    });

    final popup = html.window.open(
      authUrl.toString(),
      'google_oauth',
      'width=500,height=650',
    );

    final completer = Completer<String>();
    late StreamSubscription sub;
    sub = html.window.onMessage.listen((event) {
      final data = event.data;
      if (data is! Map && data is! String) return;
      final Map parsed = data is String
          ? (jsonDecode(data) as Map)
          : (data as Map);
      if (parsed['type'] != 'google_oauth') return;
      if (parsed['state'] != state) {
        completer.completeError(Exception('Google sign-in state mismatch'));
      } else if (parsed['error'] != null) {
        completer.completeError(Exception('Google sign-in error: ${parsed['error']}'));
      } else if (parsed['code'] != null) {
        completer.complete(parsed['code'] as String);
      }
      sub.cancel();
      try { popup?.close(); } catch (_) {}
    });

    final code = await completer.future.timeout(
      const Duration(minutes: 5),
      onTimeout: () {
        sub.cancel();
        try { popup?.close(); } catch (_) {}
        throw TimeoutException('Google sign-in timed out');
      },
    );

    return _exchangeCode(
      clientId: clientId,
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
    // Browsers use short-lived access tokens (access_type=online); when one
    // expires the user signs in again. We still expose refresh for parity.
    final res = await http.post(
      Uri.parse(_tokenEndpoint),
      body: {
        'client_id': clientId,
        'refresh_token': refreshToken,
        'grant_type': 'refresh_token',
      },
    );
    if (res.statusCode != 200) {
      throw Exception('Google token refresh failed: ${res.body}');
    }
    final j = jsonDecode(res.body) as Map<String, dynamic>;
    j['refresh_token'] ??= refreshToken;
    return GoogleTokens.fromJson(j);
  }

  Future<GoogleTokens> _exchangeCode({
    required String clientId,
    required String code,
    required String verifier,
    required String redirectUri,
  }) async {
    final res = await http.post(
      Uri.parse(_tokenEndpoint),
      body: {
        'client_id': clientId,
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

  static String _randomUrlSafe(int bytes) {
    final rng = Random.secure();
    final data = List<int>.generate(bytes, (_) => rng.nextInt(256));
    return base64Url.encode(data).replaceAll('=', '');
  }

  static String _s256(String verifier) {
    final digest = sha256.convert(ascii.encode(verifier));
    return base64Url.encode(digest.bytes).replaceAll('=', '');
  }
}
