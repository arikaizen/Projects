import 'package:flutter/material.dart';
import '../models/auth_model.dart';
import '../services/auth_service.dart';
import '../services/google_auth_service.dart';

class AuthProvider extends ChangeNotifier {
  final _service = AuthService();
  AuthSession? _session;
  bool    _loading = false;
  String? _error;

  AuthSession? get session    => _session;
  bool         get loading    => _loading;
  String?      get error      => _error;
  bool         get isLoggedIn => _session != null;
  bool         get isAdmin    => _session?.isAdmin ?? false;

  // ── Username / password ────────────────────────────────────────────────────

  Future<bool> login(String username, String password) async {
    _setLoading(true);
    await Future.delayed(const Duration(milliseconds: 200));
    final session = await _service.login(username, password);
    _setLoading(false);
    if (session != null) {
      _session = session;
      _error   = null;
    } else {
      _error = 'Invalid username or password';
    }
    notifyListeners();
    return session != null;
  }

  // ── Google Sign-In ─────────────────────────────────────────────────────────

  Future<bool> loginWithGoogle() async {
    _setLoading(true);
    final account = await GoogleAuthService.signIn();
    _setLoading(false);

    if (account == null) {
      _error = 'Google sign-in cancelled';
      notifyListeners();
      return false;
    }

    // Treat known admin emails or first-time Google users as 'user' role.
    // You can expand this list or store roles server-side.
    final role = _service.roleForGoogleEmail(account.email);
    final user = AuthUser.fromGoogle(
      email:       account.email,
      displayName: account.displayName ?? account.email,
      photoUrl:    account.photoUrl,
      role:        role,
    );
    _session = AuthSession(user: user);
    _error   = null;
    notifyListeners();
    return true;
  }

  void logout() {
    if (_session?.isGoogleUser == true) GoogleAuthService.signOut();
    _session = null;
    _error   = null;
    notifyListeners();
  }

  List<AuthUser> get users => _service.allUsers;

  void addUser(String username, String password, UserRole role) {
    _service.addUser(username, password, role);
    notifyListeners();
  }

  void _setLoading(bool v) {
    _loading = v;
    notifyListeners();
  }
}
