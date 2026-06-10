/// Shared types for the Google OAuth flow (desktop + web stubs).
class GoogleTokens {
  final String accessToken;
  final String? refreshToken;
  final String? idToken;
  final int expiresIn; // seconds
  final DateTime issuedAt;

  GoogleTokens({
    required this.accessToken,
    this.refreshToken,
    this.idToken,
    required this.expiresIn,
    DateTime? issuedAt,
  }) : issuedAt = issuedAt ?? DateTime.now();

  bool get isExpired =>
      DateTime.now().isAfter(issuedAt.add(Duration(seconds: expiresIn - 60)));

  factory GoogleTokens.fromJson(Map<String, dynamic> j) => GoogleTokens(
        accessToken: j['access_token'] as String,
        refreshToken: j['refresh_token'] as String?,
        idToken: j['id_token'] as String?,
        expiresIn: (j['expires_in'] as num?)?.toInt() ?? 3600,
      );
}

/// Scopes that allow calling the Gemini API with the resulting access token.
const kGeminiScopes = [
  'https://www.googleapis.com/auth/generative-language',
  'https://www.googleapis.com/auth/cloud-platform',
];
