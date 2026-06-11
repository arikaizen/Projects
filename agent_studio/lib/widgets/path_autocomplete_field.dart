import 'package:flutter/material.dart';
import '../theme/app_theme.dart';
import '../services/fs_browser_stub.dart'
    if (dart.library.io) '../services/fs_browser_io.dart';

/// A text field that autocompletes local filesystem paths as the user types.
/// Folders are listed first; selecting a folder keeps the trailing slash so the
/// user can keep drilling down. Pre-fills the user's home directory.
class PathAutocompleteField extends StatefulWidget {
  final TextEditingController controller;
  final String hintText;
  final bool dirsOnly;
  final ValueChanged<String>? onSubmitted;

  const PathAutocompleteField({
    super.key,
    required this.controller,
    this.hintText = 'Type a path…',
    this.dirsOnly = false,
    this.onSubmitted,
  });

  @override
  State<PathAutocompleteField> createState() => _PathAutocompleteFieldState();
}

class _PathAutocompleteFieldState extends State<PathAutocompleteField> {
  @override
  void initState() {
    super.initState();
    // Pre-configure the user's home directory if nothing is set yet.
    if (widget.controller.text.trim().isEmpty && FsBrowser.home.isNotEmpty) {
      widget.controller.text = '${FsBrowser.home}/';
    }
  }

  @override
  Widget build(BuildContext context) {
    return RawAutocomplete<FsEntry>(
      textEditingController: widget.controller,
      focusNode: FocusNode(),
      optionsBuilder: (value) async {
        if (value.text.isEmpty) return const Iterable<FsEntry>.empty();
        return FsBrowser.suggest(value.text, dirsOnly: widget.dirsOnly);
      },
      displayStringForOption: (e) => e.path,
      onSelected: (e) {
        widget.controller.text = e.path;
        widget.controller.selection =
            TextSelection.collapsed(offset: e.path.length);
      },
      fieldViewBuilder: (context, textCtrl, focusNode, onSubmit) {
        return TextField(
          controller: textCtrl,
          focusNode: focusNode,
          style: const TextStyle(color: AppColors.textPrimary, fontSize: 13),
          decoration: InputDecoration(
            hintText: widget.hintText,
            prefixIcon: const Icon(Icons.folder_open_outlined,
                size: 16, color: AppColors.textMuted),
          ),
          onChanged: (_) => setState(() {}),
          onSubmitted: (v) {
            onSubmit();
            widget.onSubmitted?.call(v);
          },
        );
      },
      optionsViewBuilder: (context, onSelected, options) {
        return Align(
          alignment: Alignment.topLeft,
          child: Material(
            color: AppColors.surface,
            elevation: 6,
            borderRadius: BorderRadius.circular(8),
            child: ConstrainedBox(
              constraints: const BoxConstraints(maxHeight: 280, maxWidth: 480),
              child: ListView(
                padding: EdgeInsets.zero,
                shrinkWrap: true,
                children: options.map((e) {
                  return InkWell(
                    onTap: () => onSelected(e),
                    child: Padding(
                      padding: const EdgeInsets.symmetric(
                          horizontal: 12, vertical: 8),
                      child: Row(
                        children: [
                          Icon(
                            e.isDir ? Icons.folder : Icons.insert_drive_file_outlined,
                            size: 15,
                            color: e.isDir
                                ? AppColors.primary
                                : AppColors.textMuted,
                          ),
                          const SizedBox(width: 10),
                          Expanded(
                            child: Text(e.name,
                                style: const TextStyle(
                                    color: AppColors.textPrimary, fontSize: 12),
                                overflow: TextOverflow.ellipsis),
                          ),
                        ],
                      ),
                    ),
                  );
                }).toList(),
              ),
            ),
          ),
        );
      },
    );
  }
}
