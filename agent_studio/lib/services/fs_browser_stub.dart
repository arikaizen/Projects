/// Web stub — no local filesystem access in the browser.
class FsBrowser {
  static String get home => '';

  /// Returns filesystem entries whose path starts with [input].
  /// On web there is no local FS, so this is always empty.
  static Future<List<FsEntry>> suggest(String input, {bool dirsOnly = false}) async =>
      const [];
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
