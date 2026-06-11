import 'package:flutter/material.dart';
import 'package:flutter_animate/flutter_animate.dart';
import 'package:provider/provider.dart';
import '../providers/auth_provider.dart';
import '../theme/app_theme.dart';

class LoginScreen extends StatefulWidget {
  const LoginScreen({super.key});
  @override
  State<LoginScreen> createState() => _LoginScreenState();
}

class _LoginScreenState extends State<LoginScreen> {
  final _user    = TextEditingController();
  final _pass    = TextEditingController();
  bool _obscure  = true;
  bool _shake    = false;
  bool _showPass = false; // false = show Google prompt, true = show password form

  @override
  void dispose() {
    _user.dispose();
    _pass.dispose();
    super.dispose();
  }

  Future<void> _submitPassword() async {
    final ok = await context.read<AuthProvider>().login(
      _user.text.trim(), _pass.text);
    if (!ok && mounted) {
      setState(() => _shake = true);
      Future.delayed(const Duration(milliseconds: 500), () {
        if (mounted) setState(() => _shake = false);
      });
    }
  }

  Future<void> _googleSignIn() async {
    await context.read<AuthProvider>().loginWithGoogle();
  }

  @override
  Widget build(BuildContext context) {
    final auth = context.watch<AuthProvider>();
    return Scaffold(
      backgroundColor: AppColors.background,
      body: Center(
        child: AnimatedSlide(
          offset: _shake ? const Offset(0.02, 0) : Offset.zero,
          duration: const Duration(milliseconds: 80),
          child: Container(
            width: 400,
            padding: const EdgeInsets.all(36),
            decoration: BoxDecoration(
              color: AppColors.surface,
              borderRadius: BorderRadius.circular(20),
              border: Border.all(color: AppColors.border),
              boxShadow: [BoxShadow(
                color: Colors.black.withOpacity(0.4),
                blurRadius: 40, offset: const Offset(0, 16))],
            ),
            child: Column(
              mainAxisSize: MainAxisSize.min,
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                // Header
                Row(children: [
                  Container(
                    padding: const EdgeInsets.all(10),
                    decoration: BoxDecoration(
                      color: AppColors.primary.withOpacity(0.15),
                      borderRadius: BorderRadius.circular(12),
                    ),
                    child: const Icon(Icons.smart_toy, color: AppColors.primary, size: 24),
                  ),
                  const SizedBox(width: 14),
                  const Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
                    Text('Agent Studio',
                      style: TextStyle(color: AppColors.textPrimary,
                          fontSize: 18, fontWeight: FontWeight.w700)),
                    Text('Sign in to continue',
                      style: TextStyle(color: AppColors.textMuted, fontSize: 12)),
                  ]),
                ]),
                const SizedBox(height: 32),

                // Google Sign-In button
                _GoogleButton(
                  loading: auth.loading,
                  onPressed: _googleSignIn,
                ),
                const SizedBox(height: 16),

                // Divider
                Row(children: [
                  const Expanded(child: Divider(color: AppColors.border)),
                  Padding(
                    padding: const EdgeInsets.symmetric(horizontal: 12),
                    child: Text('or',
                      style: TextStyle(color: AppColors.textMuted.withOpacity(0.7),
                          fontSize: 11)),
                  ),
                  const Expanded(child: Divider(color: AppColors.border)),
                ]),
                const SizedBox(height: 16),

                // Toggle: show password form
                if (!_showPass) ...[
                  SizedBox(
                    width: double.infinity,
                    child: OutlinedButton(
                      onPressed: () => setState(() => _showPass = true),
                      style: OutlinedButton.styleFrom(
                        foregroundColor: AppColors.textSecondary,
                        side: const BorderSide(color: AppColors.border),
                        padding: const EdgeInsets.symmetric(vertical: 13),
                      ),
                      child: const Text('Sign in with username / password'),
                    ),
                  ),
                ] else ...[
                  // Username
                  const Text('Username',
                    style: TextStyle(color: AppColors.textSecondary,
                        fontSize: 12, fontWeight: FontWeight.w600)),
                  const SizedBox(height: 6),
                  TextField(
                    controller: _user,
                    autofocus: true,
                    style: const TextStyle(color: AppColors.textPrimary),
                    decoration: const InputDecoration(
                      hintText: 'Enter username',
                      prefixIcon: Icon(Icons.person_outline, size: 18,
                          color: AppColors.textMuted),
                    ),
                    onSubmitted: (_) => _submitPassword(),
                  ),
                  const SizedBox(height: 14),
                  // Password
                  const Text('Password',
                    style: TextStyle(color: AppColors.textSecondary,
                        fontSize: 12, fontWeight: FontWeight.w600)),
                  const SizedBox(height: 6),
                  TextField(
                    controller: _pass,
                    obscureText: _obscure,
                    style: const TextStyle(color: AppColors.textPrimary),
                    decoration: InputDecoration(
                      hintText: 'Enter password',
                      prefixIcon: const Icon(Icons.lock_outline, size: 18,
                          color: AppColors.textMuted),
                      suffixIcon: IconButton(
                        icon: Icon(_obscure
                            ? Icons.visibility_outlined
                            : Icons.visibility_off_outlined,
                            size: 18, color: AppColors.textMuted),
                        onPressed: () => setState(() => _obscure = !_obscure),
                      ),
                    ),
                    onSubmitted: (_) => _submitPassword(),
                  ),
                  const SizedBox(height: 20),
                  SizedBox(
                    width: double.infinity,
                    child: ElevatedButton(
                      onPressed: auth.loading ? null : _submitPassword,
                      style: ElevatedButton.styleFrom(
                        padding: const EdgeInsets.symmetric(vertical: 14),
                        backgroundColor: AppColors.primary,
                        foregroundColor: Colors.white,
                      ),
                      child: auth.loading
                          ? const SizedBox(width: 18, height: 18,
                              child: CircularProgressIndicator(
                                  strokeWidth: 2, color: Colors.white))
                          : const Text('Sign In',
                              style: TextStyle(fontWeight: FontWeight.w600)),
                    ),
                  ),
                ],

                // Error
                if (auth.error != null) ...[
                  const SizedBox(height: 12),
                  Container(
                    padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
                    decoration: BoxDecoration(
                      color: AppColors.error.withOpacity(0.08),
                      borderRadius: BorderRadius.circular(8),
                      border: Border.all(color: AppColors.error.withOpacity(0.3)),
                    ),
                    child: Row(children: [
                      const Icon(Icons.error_outline, size: 14, color: AppColors.error),
                      const SizedBox(width: 8),
                      Expanded(child: Text(auth.error!,
                        style: const TextStyle(color: AppColors.error, fontSize: 12))),
                    ]),
                  ),
                ],

                const SizedBox(height: 20),
                const Divider(color: AppColors.border),
                const SizedBox(height: 10),
                const Text('Default: admin / admin123   ·   user / user123',
                  style: TextStyle(color: AppColors.textMuted, fontSize: 10),
                  textAlign: TextAlign.center),
              ],
            ),
          ).animate().fadeIn(duration: 400.ms).slideY(begin: 0.04, end: 0),
        ),
      ),
    );
  }
}

class _GoogleButton extends StatelessWidget {
  final bool loading;
  final VoidCallback onPressed;
  const _GoogleButton({required this.loading, required this.onPressed});

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      width: double.infinity,
      child: ElevatedButton(
        onPressed: loading ? null : onPressed,
        style: ElevatedButton.styleFrom(
          backgroundColor: Colors.white,
          foregroundColor: const Color(0xFF1F1F1F),
          padding: const EdgeInsets.symmetric(vertical: 13),
          shape: RoundedRectangleBorder(
            borderRadius: BorderRadius.circular(10),
            side: const BorderSide(color: Color(0xFFDADCE0)),
          ),
          elevation: 1,
        ),
        child: loading
            ? const SizedBox(width: 20, height: 20,
                child: CircularProgressIndicator(
                    strokeWidth: 2, color: Color(0xFF4285F4)))
            : Row(mainAxisAlignment: MainAxisAlignment.center, children: [
                _GoogleLogo(),
                const SizedBox(width: 12),
                const Text('Continue with Google',
                  style: TextStyle(fontWeight: FontWeight.w500,
                      fontSize: 14, color: Color(0xFF1F1F1F))),
              ]),
      ),
    );
  }
}

class _GoogleLogo extends StatelessWidget {
  @override
  Widget build(BuildContext context) {
    return SizedBox(
      width: 20,
      height: 20,
      child: CustomPaint(painter: _GooglePainter()),
    );
  }
}

class _GooglePainter extends CustomPainter {
  @override
  void paint(Canvas canvas, Size size) {
    final cx = size.width / 2;
    final cy = size.height / 2;
    final r  = size.width / 2;

    // Simplified 4-colour G
    final segments = [
      [0xFF4285F4, -0.3, 1.25],  // blue
      [0xFF34A853,  1.25, 2.3],  // green
      [0xFFFBBC05,  2.3, 3.5],   // yellow
      [0xFFEA4335,  3.5, 5.95],  // red
    ];

    for (final s in segments) {
      final paint = Paint()
        ..color = Color(s[0] as int)
        ..style = PaintingStyle.stroke
        ..strokeWidth = r * 0.45;
      canvas.drawArc(
        Rect.fromCircle(center: Offset(cx, cy), radius: r * 0.75),
        s[1] as double,
        (s[2] as double) - (s[1] as double),
        false,
        paint,
      );
    }
  }

  @override
  bool shouldRepaint(_) => false;
}
