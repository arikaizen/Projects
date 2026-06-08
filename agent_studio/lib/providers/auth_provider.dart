import 'package:flutter/material.dart';
import '../models/auth_model.dart';
import '../services/auth_service.dart';

class AuthProvider extends ChangeNotifier {
  final _service = AuthService();
  AuthSession? _session;
  bool _loading = false;
  String? _error;

  AuthSession? get session   => _session;
  bool         get loading   => _loading;
  String?      get error     => _error;
  bool         get isLoggedIn => _session != null;
  bool         get isAdmin    => _session?.isAdmin ?? false;

  Future<bool> login(String username, String password) async {
    _loading = true;
    _error   = null;
    notifyListeners();

    await Future.delayed(const Duration(milliseconds: 300));
    final session = await _service.login(username, password);

    _loading = false;
    if (session != null) {
      _session = session;
      _error   = null;
    } else {
      _error = 'Invalid username or password';
    }
    notifyListeners();
    return session != null;
  }

  void logout() {
    _session = null;
    _error   = null;
    notifyListeners();
  }

  List<AuthUser> get users => _service.allUsers;

  void addUser(String username, String password, UserRole role) {
    _service.addUser(username, password, role);
    notifyListeners();
  }
}
