import '../models/auth_model.dart';

class AuthService {
  // Default accounts — in production these come from engine config.
  // Passwords: admin123, user123
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

  // Runtime-added users (survive the session)
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

  // Override admin emails here — everyone else gets 'user' role.
  static const _adminEmails = <String>[];

  UserRole roleForGoogleEmail(String email) =>
      _adminEmails.contains(email) ? UserRole.admin : UserRole.user;
}
