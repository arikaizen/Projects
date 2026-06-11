import 'package:google_sign_in/google_sign_in.dart';

class GoogleAuthService {
  static final _instance = GoogleSignIn(
    scopes: [
      'email',
      'profile',
      // Gemini / Google AI access
      'https://www.googleapis.com/auth/generative-language',
    ],
  );

  static GoogleSignInAccount? get currentUser => _instance.currentUser;

  static Future<GoogleSignInAccount?> signIn() async {
    try {
      return await _instance.signIn();
    } catch (_) {
      return null;
    }
  }

  static Future<void> signOut() async {
    await _instance.signOut();
  }

  /// Returns the OAuth access token (used as Gemini bearer token).
  static Future<String?> getAccessToken() async {
    final account = _instance.currentUser ?? await signIn();
    if (account == null) return null;
    try {
      final auth = await account.authentication;
      return auth.accessToken;
    } catch (_) {
      return null;
    }
  }

  static String? get email => _instance.currentUser?.email;
  static String? get displayName => _instance.currentUser?.displayName;
  static String? get photoUrl => _instance.currentUser?.photoUrl;
}
