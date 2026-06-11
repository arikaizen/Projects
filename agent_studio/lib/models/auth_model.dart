import 'dart:convert';
import 'package:crypto/crypto.dart';

enum UserRole { admin, user }
enum AuthMethod { password, google }

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
