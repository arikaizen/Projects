import 'dart:io';

/// Native (desktop) filesystem browser used to power path autocomplete.
class FsBrowser {
  static String get home =>
      Platform.environment['HOME'] ??
      Platform.environment['USERPROFILE'] ??
      Directory.current.path;

  /// Suggest filesystem entries matching the partial [input] path.
  /// Lists the contents of the directory portion of [input] and filters by the
  /// trailing partial name. When [dirsOnly] is true, only directories are
  /// returned (useful for picking a working folder).
  static Future<List<FsEntry>> suggest(String input,
      {bool dirsOnly = false}) async {
    try {
      var text = input.trim();
      if (text.isEmpty) text = home;
      // Expand a leading ~ to the home directory.
      if (text == '~' || text.startsWith('~/')) {
        text = home + text.substring(1);
      }

      // Determine the directory to list and the partial name to match.
      String dirPath;
      String partial;
      if (text.endsWith('/')) {
        dirPath = text;
        partial = '';
      } else {
        final idx = text.lastIndexOf('/');
        dirPath = idx <= 0 ? '/' : text.substring(0, idx);
        partial = text.substring(idx + 1);
      }

      final dir = Directory(dirPath);
      if (!await dir.exists()) return const [];

      final entries = <FsEntry>[];
      await for (final e in dir.list(followLinks: false)) {
        final isDir = e is Directory;
        if (dirsOnly && !isDir) continue;
        final name = e.path.split('/').last;
        if (name.startsWith('.')) continue; // skip hidden files
        if (partial.isNotEmpty &&
            !name.toLowerCase().startsWith(partial.toLowerCase())) {
          continue;
        }
        entries.add(FsEntry(isDir ? '${e.path}/' : e.path, isDir));
      }
      entries.sort((a, b) {
        if (a.isDir != b.isDir) return a.isDir ? -1 : 1; // folders first
        return a.path.toLowerCase().compareTo(b.path.toLowerCase());
      });
      return entries.take(50).toList();
    } catch (_) {
      return const [];
    }
  }
}

class FsEntry {
  final String path;
  final bool isDir;
  const FsEntry(this.path, this.isDir);
  String get name {
    final cleaned = path.endsWith('/') ? path.substring(0, path.length - 1) : path;
    final idx = cleaned.lastIndexOf('/');
    return idx >= 0 ? cleaned.substring(idx + 1) : cleaned;
  }
}
