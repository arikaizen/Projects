import 'dart:convert';
import 'package:crypto/crypto.dart';

enum UserRole { admin, user }

class AuthUser {
  final String username;
  final String passwordHash; // SHA-256 hex
  final UserRole role;

  const AuthUser({
    required this.username,
    required this.passwordHash,
    required this.role,
  });

  static String hashPassword(String password) =>
      sha256.convert(utf8.encode(password)).toString();

  bool checkPassword(String password) =>
      hashPassword(password) == passwordHash;
}

class AuthSession {
  final AuthUser user;
  final DateTime createdAt;

  AuthSession({required this.user}) : createdAt = DateTime.now();

  bool get isAdmin => user.role == UserRole.admin;
  String get username => user.username;
}
