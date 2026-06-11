import 'google_auth_types.dart';

// Default export used when neither dart:io nor dart:html is available.
// The conditional import in google_auth_service.dart selects the desktop or
// web implementation instead; this stub only exists to satisfy the analyzer.
GoogleAuthService createGoogleAuthService() => GoogleAuthService();

class GoogleAuthService {
  bool get isSupported => false;

  Future<GoogleTokens> signIn({
    required String clientId,
    String clientSecret = '',
    List<String> scopes = kGeminiScopes,
  }) async {
    throw UnsupportedError(
        'Google sign-in is not available on this platform. '
        'Paste an API key or OAuth bearer token instead.');
  }

  Future<GoogleTokens> refresh({
    required String clientId,
    required String refreshToken,
    String clientSecret = '',
  }) async {
    throw UnsupportedError('Google sign-in is not available on this platform.');
  }
}
