import 'dart:convert';
import 'package:flutter/material.dart';
import 'package:http/http.dart' as http;
import '../theme/app_theme.dart';

// ── Model ─────────────────────────────────────────────────────────────────────

class McpToolEntry {
  final String name;
  final String description;
  final String category;
  final Map<String, dynamic> inputSchema;

  const McpToolEntry({
    required this.name,
    required this.description,
    required this.category,
    required this.inputSchema,
  });

  factory McpToolEntry.fromJson(Map<String, dynamic> j) => McpToolEntry(
    name:        j['name']        as String? ?? '',
    description: j['description'] as String? ?? '',
    category:    j['category']    as String? ?? 'misc',
    inputSchema: j['inputSchema'] as Map<String, dynamic>? ?? {},
  );

  List<String> get requiredParams {
    final req = inputSchema['required'];
    if (req is List) return req.cast<String>();
    return [];
  }

  Map<String, dynamic> get properties {
    final props = inputSchema['properties'];
    if (props is Map) return props.cast<String, dynamic>();
    return {};
  }
}

// ── Screen ────────────────────────────────────────────────────────────────────

class ApiCatalogScreen extends StatefulWidget {
  final List<Map<String, String>> mcpServers;
  const ApiCatalogScreen({super.key, required this.mcpServers});

  @override
  State<ApiCatalogScreen> createState() => _ApiCatalogScreenState();
}

class _ApiCatalogScreenState extends State<ApiCatalogScreen> {
  List<McpToolEntry> _tools       = [];
  bool               _loading     = false;
  String?            _error;
  String             _search      = '';
  String?            _filterCat;
  McpToolEntry?      _selected;

  final _searchCtrl = TextEditingController();

  @override
  void initState() {
    super.initState();
    _loadTools();
  }

  @override
  void dispose() {
    _searchCtrl.dispose();
    super.dispose();
  }

  Future<void> _loadTools() async {
    if (widget.mcpServers.isEmpty) {
      setState(() {
        _error = 'No MCP server connected. Add one in Settings → MCP Servers.';
      });
      return;
    }

    setState(() { _loading = true; _error = null; });

    final server = widget.mcpServers.first;
    final url    = server['url'] ?? '';
    final token  = server['bearer_token'] ?? '';

    try {
      final body = jsonEncode({
        'jsonrpc': '2.0',
        'id':      1,
        'method':  'tools/list',
        'params':  <String, dynamic>{},
      });

      final res = await http.post(
        Uri.parse('$url/mcp/v1'),
        headers: {
          'Content-Type':  'application/json',
          if (token.isNotEmpty) 'Authorization': 'Bearer $token',
        },
        body: body,
      ).timeout(const Duration(seconds: 10));

      final doc  = jsonDecode(res.body) as Map<String, dynamic>;
      final list = (doc['result']?['tools'] as List?) ?? [];
      setState(() {
        _tools   = list.map((e) => McpToolEntry.fromJson(e as Map<String, dynamic>)).toList();
        _loading = false;
      });
    } catch (e) {
      setState(() {
        _error   = 'Failed to load tools: $e';
        _loading = false;
      });
    }
  }

  List<McpToolEntry> get _filtered {
    final q = _search.toLowerCase();
    return _tools.where((t) {
      if (_filterCat != null && t.category != _filterCat) return false;
      if (q.isNotEmpty && !t.name.contains(q) && !t.description.toLowerCase().contains(q)) return false;
      return true;
    }).toList();
  }

  List<String> get _categories {
    final cats = _tools.map((t) => t.category).toSet().toList()..sort();
    return cats;
  }

  @override
  Widget build(BuildContext context) {
    return Row(
      children: [
        // Main list
        Expanded(
          child: Column(
            children: [
              _toolbar(),
              Expanded(child: _body()),
            ],
          ),
        ),
        // Detail panel
        if (_selected != null)
          Container(
            width: 340,
            decoration: const BoxDecoration(
              border: Border(left: BorderSide(color: AppColors.border)),
              color: AppColors.surface,
            ),
            child: _DetailPanel(
              tool:    _selected!,
              onClose: () => setState(() => _selected = null),
            ),
          ),
      ],
    );
  }

  Widget _toolbar() {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 20, vertical: 12),
      decoration: const BoxDecoration(
        border: Border(bottom: BorderSide(color: AppColors.border)),
      ),
      child: Row(
        children: [
          // Search
          Expanded(
            child: SizedBox(
              height: 36,
              child: TextField(
                controller: _searchCtrl,
                onChanged: (v) => setState(() => _search = v),
                style: const TextStyle(color: AppColors.textPrimary, fontSize: 13),
                decoration: InputDecoration(
                  hintText: 'Search ${_tools.length} APIs…',
                  hintStyle: const TextStyle(color: AppColors.textMuted, fontSize: 13),
                  prefixIcon: const Icon(Icons.search, size: 16, color: AppColors.textMuted),
                  suffixIcon: _search.isNotEmpty
                    ? IconButton(
                        icon: const Icon(Icons.close, size: 14, color: AppColors.textMuted),
                        onPressed: () {
                          _searchCtrl.clear();
                          setState(() => _search = '');
                        },
                        padding: EdgeInsets.zero,
                      )
                    : null,
                  contentPadding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
                  isDense: true,
                  filled: true,
                  fillColor: AppColors.surfaceAlt,
                  border: OutlineInputBorder(
                    borderRadius: BorderRadius.circular(8),
                    borderSide: const BorderSide(color: AppColors.border),
                  ),
                  enabledBorder: OutlineInputBorder(
                    borderRadius: BorderRadius.circular(8),
                    borderSide: const BorderSide(color: AppColors.border),
                  ),
                  focusedBorder: OutlineInputBorder(
                    borderRadius: BorderRadius.circular(8),
                    borderSide: const BorderSide(color: AppColors.primary, width: 1.5),
                  ),
                ),
              ),
            ),
          ),
          const SizedBox(width: 12),
          // Category filter chips
          if (_categories.isNotEmpty)
            SizedBox(
              height: 36,
              child: ListView(
                scrollDirection: Axis.horizontal,
                shrinkWrap: true,
                children: [
                  _catChip(null, 'All'),
                  ..._categories.map((c) => _catChip(c, c)),
                ],
              ),
            ),
          const SizedBox(width: 12),
          // Refresh
          IconButton(
            icon: _loading
              ? const SizedBox(width: 14, height: 14,
                  child: CircularProgressIndicator(strokeWidth: 1.5, color: AppColors.primary))
              : const Icon(Icons.refresh, size: 18, color: AppColors.textMuted),
            tooltip: 'Refresh',
            onPressed: _loading ? null : _loadTools,
            padding: EdgeInsets.zero,
          ),
        ],
      ),
    );
  }

  Widget _catChip(String? value, String label) {
    final active = _filterCat == value;
    return GestureDetector(
      onTap: () => setState(() => _filterCat = value),
      child: AnimatedContainer(
        duration: const Duration(milliseconds: 120),
        margin: const EdgeInsets.only(right: 6),
        padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 7),
        decoration: BoxDecoration(
          color: active ? AppColors.primary.withOpacity(0.15) : AppColors.surfaceAlt,
          borderRadius: BorderRadius.circular(20),
          border: Border.all(
            color: active ? AppColors.primary.withOpacity(0.6) : AppColors.border,
          ),
        ),
        child: Text(
          label,
          style: TextStyle(
            color: active ? AppColors.primary : AppColors.textSecondary,
            fontSize: 11,
            fontWeight: active ? FontWeight.w600 : FontWeight.w400,
          ),
        ),
      ),
    );
  }

  Widget _body() {
    if (_loading) {
      return const Center(
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            CircularProgressIndicator(color: AppColors.primary, strokeWidth: 2),
            SizedBox(height: 16),
            Text('Loading tools…', style: TextStyle(color: AppColors.textMuted)),
          ],
        ),
      );
    }

    if (_error != null) {
      return Center(
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            const Icon(Icons.cloud_off_outlined, size: 48, color: AppColors.textMuted),
            const SizedBox(height: 16),
            Text(_error!,
              textAlign: TextAlign.center,
              style: const TextStyle(color: AppColors.textSecondary, fontSize: 13)),
            const SizedBox(height: 20),
            ElevatedButton.icon(
              icon: const Icon(Icons.refresh, size: 16),
              label: const Text('Retry'),
              onPressed: _loadTools,
            ),
          ],
        ),
      );
    }

    if (_tools.isEmpty) {
      return const Center(
        child: Text('No tools found on this MCP server.',
          style: TextStyle(color: AppColors.textMuted)),
      );
    }

    final filtered = _filtered;
    if (filtered.isEmpty) {
      return const Center(
        child: Text('No tools match your search.',
          style: TextStyle(color: AppColors.textMuted)),
      );
    }

    return _GroupedList(
      tools:    filtered,
      selected: _selected,
      onTap:    (t) => setState(() => _selected = _selected?.name == t.name ? null : t),
    );
  }
}

// ── Grouped list ──────────────────────────────────────────────────────────────

class _GroupedList extends StatelessWidget {
  final List<McpToolEntry> tools;
  final McpToolEntry?      selected;
  final ValueChanged<McpToolEntry> onTap;

  const _GroupedList({required this.tools, required this.selected, required this.onTap});

  @override
  Widget build(BuildContext context) {
    // Group by category
    final Map<String, List<McpToolEntry>> grouped = {};
    for (final t in tools) {
      grouped.putIfAbsent(t.category, () => []).add(t);
    }
    final cats = grouped.keys.toList()..sort();

    return ListView.builder(
      padding: const EdgeInsets.all(20),
      itemCount: cats.length,
      itemBuilder: (_, ci) {
        final cat  = cats[ci];
        final list = grouped[cat]!;
        return Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            if (ci > 0) const SizedBox(height: 20),
            // Category header
            Row(
              children: [
                Container(
                  padding: const EdgeInsets.all(6),
                  decoration: BoxDecoration(
                    color: _categoryColor(cat).withOpacity(0.12),
                    borderRadius: BorderRadius.circular(8),
                  ),
                  child: Icon(_categoryIcon(cat), size: 14, color: _categoryColor(cat)),
                ),
                const SizedBox(width: 10),
                Text(
                  cat.replaceAll('_', ' ').toUpperCase(),
                  style: TextStyle(
                    color: _categoryColor(cat),
                    fontSize: 11,
                    fontWeight: FontWeight.w700,
                    letterSpacing: 1.0,
                  ),
                ),
                const SizedBox(width: 8),
                Text(
                  '${list.length} tool${list.length == 1 ? '' : 's'}',
                  style: const TextStyle(color: AppColors.textMuted, fontSize: 11),
                ),
              ],
            ),
            const SizedBox(height: 10),
            // Tool rows
            ...list.map((t) => _ToolRow(
              tool:     t,
              catColor: _categoryColor(cat),
              selected: selected?.name == t.name,
              onTap:    () => onTap(t),
            )),
          ],
        );
      },
    );
  }

  static Color _categoryColor(String cat) {
    const m = {
      'web_search':  AppColors.primary,
      'maps':        Color(0xFF4ECDC4),
      'weather':     Color(0xFF74B9FF),
      'news':        Color(0xFFFDCB6E),
      'finance':     Color(0xFF00B894),
      'github':      Color(0xFFB2BEC3),
      'translation': Color(0xFFA29BFE),
      'youtube':     Color(0xFFFF7675),
      'knowledge':   Color(0xFF6C5CE7),
      'nlp':         Color(0xFFE17055),
      'vision':      Color(0xFF0984E3),
      'images':      Color(0xFFE84393),
      'utilities':   AppColors.textSecondary,
      'astronomy':   Color(0xFF6C5CE7),
      'social':      Color(0xFFFF7675),
      'country':     Color(0xFF00CEC9),
      'math':        Color(0xFF00B894),
      'music':       Color(0xFFA29BFE),
      'food':        Color(0xFFFDCB6E),
      'network':     Color(0xFF74B9FF),
      'calendar':    Color(0xFF55EFC4),
      'trivia':      Color(0xFFFD79A8),
    };
    return m[cat] ?? AppColors.textMuted;
  }

  static IconData _categoryIcon(String cat) {
    const m = {
      'web_search':  Icons.search,
      'maps':        Icons.map_outlined,
      'weather':     Icons.wb_sunny_outlined,
      'news':        Icons.newspaper_outlined,
      'finance':     Icons.show_chart,
      'github':      Icons.code,
      'translation': Icons.translate,
      'youtube':     Icons.play_circle_outline,
      'knowledge':   Icons.menu_book_outlined,
      'nlp':         Icons.psychology_outlined,
      'vision':      Icons.visibility_outlined,
      'images':      Icons.image_outlined,
      'utilities':   Icons.build_outlined,
      'astronomy':   Icons.nightlight_outlined,
      'social':      Icons.forum_outlined,
      'country':     Icons.public,
      'math':        Icons.calculate_outlined,
      'music':       Icons.music_note_outlined,
      'food':        Icons.restaurant_outlined,
      'network':     Icons.wifi_outlined,
      'calendar':    Icons.calendar_today_outlined,
      'trivia':      Icons.quiz_outlined,
    };
    return m[cat] ?? Icons.extension_outlined;
  }
}

class _ToolRow extends StatefulWidget {
  final McpToolEntry tool;
  final Color        catColor;
  final bool         selected;
  final VoidCallback onTap;

  const _ToolRow({
    required this.tool,
    required this.catColor,
    required this.selected,
    required this.onTap,
  });

  @override
  State<_ToolRow> createState() => _ToolRowState();
}

class _ToolRowState extends State<_ToolRow> {
  bool _hovered = false;

  @override
  Widget build(BuildContext context) {
    final t = widget.tool;
    return MouseRegion(
      onEnter:  (_) => setState(() => _hovered = true),
      onExit:   (_) => setState(() => _hovered = false),
      child: GestureDetector(
        onTap: widget.onTap,
        child: AnimatedContainer(
          duration: const Duration(milliseconds: 120),
          margin: const EdgeInsets.only(bottom: 6),
          padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 11),
          decoration: BoxDecoration(
            color: widget.selected
              ? widget.catColor.withOpacity(0.10)
              : _hovered
                ? AppColors.cardHover
                : AppColors.card,
            borderRadius: BorderRadius.circular(10),
            border: Border.all(
              color: widget.selected
                ? widget.catColor.withOpacity(0.4)
                : AppColors.border,
            ),
          ),
          child: Row(
            children: [
              // Left color dot
              Container(
                width: 4,
                height: 36,
                decoration: BoxDecoration(
                  color: widget.selected
                    ? widget.catColor
                    : widget.catColor.withOpacity(0.3),
                  borderRadius: BorderRadius.circular(4),
                ),
              ),
              const SizedBox(width: 12),
              Expanded(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(
                      t.name,
                      style: const TextStyle(
                        color: AppColors.textPrimary,
                        fontSize: 13,
                        fontWeight: FontWeight.w600,
                        fontFamily: 'monospace',
                      ),
                    ),
                    const SizedBox(height: 2),
                    Text(
                      t.description,
                      style: const TextStyle(
                        color: AppColors.textSecondary,
                        fontSize: 11,
                      ),
                      maxLines: 1,
                      overflow: TextOverflow.ellipsis,
                    ),
                  ],
                ),
              ),
              // Required param count badge
              if (t.requiredParams.isNotEmpty)
                Container(
                  margin: const EdgeInsets.only(left: 8),
                  padding: const EdgeInsets.symmetric(horizontal: 7, vertical: 3),
                  decoration: BoxDecoration(
                    color: AppColors.surfaceAlt,
                    borderRadius: BorderRadius.circular(6),
                  ),
                  child: Text(
                    '${t.requiredParams.length} req',
                    style: const TextStyle(
                      color: AppColors.textMuted,
                      fontSize: 10,
                      fontWeight: FontWeight.w600,
                    ),
                  ),
                ),
              const SizedBox(width: 8),
              Icon(
                widget.selected ? Icons.chevron_right : Icons.chevron_right,
                size: 16,
                color: widget.selected ? widget.catColor : AppColors.textMuted,
              ),
            ],
          ),
        ),
      ),
    );
  }
}

// ── Detail panel ──────────────────────────────────────────────────────────────

class _DetailPanel extends StatelessWidget {
  final McpToolEntry tool;
  final VoidCallback onClose;

  const _DetailPanel({required this.tool, required this.onClose});

  @override
  Widget build(BuildContext context) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        // Header
        Container(
          padding: const EdgeInsets.all(16),
          decoration: const BoxDecoration(
            border: Border(bottom: BorderSide(color: AppColors.border)),
          ),
          child: Row(
            children: [
              Expanded(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(
                      tool.name,
                      style: const TextStyle(
                        color: AppColors.textPrimary,
                        fontWeight: FontWeight.w700,
                        fontSize: 14,
                        fontFamily: 'monospace',
                      ),
                    ),
                    const SizedBox(height: 4),
                    Container(
                      padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 3),
                      decoration: BoxDecoration(
                        color: AppColors.primary.withOpacity(0.1),
                        borderRadius: BorderRadius.circular(6),
                      ),
                      child: Text(
                        tool.category,
                        style: const TextStyle(
                          color: AppColors.primary,
                          fontSize: 10,
                          fontWeight: FontWeight.w600,
                        ),
                      ),
                    ),
                  ],
                ),
              ),
              IconButton(
                icon: const Icon(Icons.close, size: 16, color: AppColors.textMuted),
                onPressed: onClose,
                padding: EdgeInsets.zero,
              ),
            ],
          ),
        ),

        Expanded(
          child: SingleChildScrollView(
            padding: const EdgeInsets.all(16),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                // Description
                Text(
                  tool.description,
                  style: const TextStyle(
                    color: AppColors.textSecondary,
                    fontSize: 13,
                    height: 1.5,
                  ),
                ),
                const SizedBox(height: 20),

                // Parameters
                if (tool.properties.isNotEmpty) ...[
                  _sectionHeader('Parameters'),
                  const SizedBox(height: 8),
                  ...tool.properties.entries.map((e) => _ParamRow(
                    name:     e.key,
                    schema:   e.value as Map<String, dynamic>,
                    required: tool.requiredParams.contains(e.key),
                  )),
                  const SizedBox(height: 20),
                ],

                // JSON Schema block
                _sectionHeader('Input Schema'),
                const SizedBox(height: 8),
                Container(
                  width: double.infinity,
                  padding: const EdgeInsets.all(12),
                  decoration: BoxDecoration(
                    color: const Color(0xFF0D1117),
                    borderRadius: BorderRadius.circular(8),
                    border: Border.all(color: AppColors.border),
                  ),
                  child: SelectableText(
                    _prettyJson(tool.inputSchema),
                    style: const TextStyle(
                      color: Color(0xFF8FBCBB),
                      fontSize: 11,
                      fontFamily: 'monospace',
                      height: 1.6,
                    ),
                  ),
                ),
              ],
            ),
          ),
        ),
      ],
    );
  }

  Widget _sectionHeader(String label) {
    return Text(
      label.toUpperCase(),
      style: const TextStyle(
        color: AppColors.textMuted,
        fontSize: 10,
        fontWeight: FontWeight.w700,
        letterSpacing: 1.0,
      ),
    );
  }

  String _prettyJson(Map<String, dynamic> m) {
    const encoder = JsonEncoder.withIndent('  ');
    return encoder.convert(m);
  }
}

class _ParamRow extends StatelessWidget {
  final String  name;
  final Map<String, dynamic> schema;
  final bool    required;

  const _ParamRow({required this.name, required this.schema, required this.required});

  @override
  Widget build(BuildContext context) {
    final type = schema['type'] as String? ?? 'any';
    final desc = schema['description'] as String? ?? '';
    final enums = schema['enum'] as List?;

    return Container(
      margin: const EdgeInsets.only(bottom: 8),
      padding: const EdgeInsets.all(10),
      decoration: BoxDecoration(
        color: AppColors.surfaceAlt,
        borderRadius: BorderRadius.circular(8),
        border: Border.all(color: AppColors.border),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              Text(
                name,
                style: const TextStyle(
                  color: AppColors.textPrimary,
                  fontSize: 12,
                  fontWeight: FontWeight.w600,
                  fontFamily: 'monospace',
                ),
              ),
              const SizedBox(width: 8),
              Container(
                padding: const EdgeInsets.symmetric(horizontal: 6, vertical: 2),
                decoration: BoxDecoration(
                  color: AppColors.primary.withOpacity(0.1),
                  borderRadius: BorderRadius.circular(4),
                ),
                child: Text(
                  type,
                  style: const TextStyle(
                    color: AppColors.primary,
                    fontSize: 10,
                    fontFamily: 'monospace',
                  ),
                ),
              ),
              const SizedBox(width: 6),
              if (required)
                Container(
                  padding: const EdgeInsets.symmetric(horizontal: 6, vertical: 2),
                  decoration: BoxDecoration(
                    color: AppColors.error.withOpacity(0.1),
                    borderRadius: BorderRadius.circular(4),
                  ),
                  child: const Text(
                    'required',
                    style: TextStyle(color: AppColors.error, fontSize: 10),
                  ),
                )
              else
                Container(
                  padding: const EdgeInsets.symmetric(horizontal: 6, vertical: 2),
                  decoration: BoxDecoration(
                    color: AppColors.textMuted.withOpacity(0.1),
                    borderRadius: BorderRadius.circular(4),
                  ),
                  child: const Text(
                    'optional',
                    style: TextStyle(color: AppColors.textMuted, fontSize: 10),
                  ),
                ),
            ],
          ),
          if (desc.isNotEmpty) ...[
            const SizedBox(height: 4),
            Text(desc, style: const TextStyle(color: AppColors.textSecondary, fontSize: 11)),
          ],
          if (enums != null) ...[
            const SizedBox(height: 6),
            Wrap(
              spacing: 4,
              children: enums.map((v) => Container(
                padding: const EdgeInsets.symmetric(horizontal: 6, vertical: 2),
                decoration: BoxDecoration(
                  color: AppColors.secondary.withOpacity(0.1),
                  borderRadius: BorderRadius.circular(4),
                  border: Border.all(color: AppColors.secondary.withOpacity(0.3)),
                ),
                child: Text('$v',
                  style: const TextStyle(color: AppColors.secondary, fontSize: 10, fontFamily: 'monospace')),
              )).toList(),
            ),
          ],
        ],
      ),
    );
  }
}
