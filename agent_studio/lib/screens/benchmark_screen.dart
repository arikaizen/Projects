import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:provider/provider.dart';
import 'package:flutter_animate/flutter_animate.dart';
import '../models/benchmark.dart';
import '../models/model_provider.dart';
import '../providers/agent_provider.dart';
import '../theme/app_theme.dart';

/// Tab that lists benchmark runs — one prompt compared across many models.
class BenchmarkTab extends StatelessWidget {
  final AgentProvider prov;
  const BenchmarkTab({super.key, required this.prov});

  @override
  Widget build(BuildContext context) {
    final benches = prov.benchmarks;
    if (benches.isEmpty) {
      return _empty(context);
    }
    return ListView.builder(
      padding: const EdgeInsets.all(20),
      itemCount: benches.length,
      itemBuilder: (_, i) => _BenchmarkCard(bench: benches[i], prov: prov)
          .animate()
          .fadeIn(duration: 250.ms, delay: Duration(milliseconds: i * 40)),
    );
  }

  Widget _empty(BuildContext context) {
    return Center(
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          const Icon(Icons.speed_outlined, size: 64, color: AppColors.textMuted),
          const SizedBox(height: 16),
          const Text('No benchmarks yet',
              style: TextStyle(
                  color: AppColors.textPrimary,
                  fontSize: 20,
                  fontWeight: FontWeight.w600)),
          const SizedBox(height: 8),
          const Text('Send one prompt to several models and compare how they perform',
              style: TextStyle(color: AppColors.textMuted)),
          const SizedBox(height: 24),
          ElevatedButton.icon(
            icon: const Icon(Icons.add),
            label: const Text('Run Benchmark'),
            onPressed: () => BenchmarkDialog.show(context),
          ),
        ],
      ),
    );
  }
}

// ── Benchmark card ──────────────────────────────────────────────────────────

class _BenchmarkCard extends StatelessWidget {
  final Benchmark bench;
  final AgentProvider prov;
  const _BenchmarkCard({required this.bench, required this.prov});

  @override
  Widget build(BuildContext context) {
    final ranked = bench.leaderboard;
    final others = bench.runs.where((r) => !ranked.contains(r)).toList();
    final winnerId = bench.fastest?.id;

    return Container(
      margin: const EdgeInsets.only(bottom: 14),
      decoration: BoxDecoration(
        color: AppColors.card,
        borderRadius: BorderRadius.circular(12),
        border: Border.all(color: AppColors.border),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          // Header
          Padding(
            padding: const EdgeInsets.all(16),
            child: Row(
              children: [
                Container(
                  padding: const EdgeInsets.all(8),
                  decoration: BoxDecoration(
                    color: AppColors.accent.withOpacity(0.15),
                    borderRadius: BorderRadius.circular(10),
                  ),
                  child: Icon(
                    bench.isRunning ? Icons.bolt : Icons.emoji_events_outlined,
                    color: AppColors.accent,
                    size: 18,
                  ),
                ),
                const SizedBox(width: 12),
                Expanded(
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(bench.name,
                          style: const TextStyle(
                              color: AppColors.textPrimary,
                              fontWeight: FontWeight.w600,
                              fontSize: 15)),
                      Text(bench.prompt,
                          maxLines: 1,
                          overflow: TextOverflow.ellipsis,
                          style: const TextStyle(
                              color: AppColors.textSecondary, fontSize: 12)),
                    ],
                  ),
                ),
                if (bench.isRunning)
                  const Padding(
                    padding: EdgeInsets.only(right: 8),
                    child: SizedBox(
                      width: 14,
                      height: 14,
                      child: CircularProgressIndicator(
                          strokeWidth: 1.5,
                          valueColor:
                              AlwaysStoppedAnimation(AppColors.accent)),
                    ),
                  )
                else
                  _chip('${bench.completed.length}/${bench.runs.length} done',
                      AppColors.textMuted),
                IconButton(
                  icon: const Icon(Icons.delete_outline, size: 16),
                  color: AppColors.error,
                  tooltip: 'Delete',
                  onPressed: () => prov.deleteBenchmark(bench.id),
                ),
              ],
            ),
          ),
          const Divider(height: 1, color: AppColors.border),
          // Runs (ranked first, then unranked/errored)
          ...[
            for (var i = 0; i < ranked.length; i++)
              _RunTile(run: ranked[i], rank: i + 1, isWinner: ranked[i].id == winnerId),
            for (final r in others) _RunTile(run: r, rank: null, isWinner: false),
          ],
        ],
      ),
    );
  }

  Widget _chip(String label, Color color) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 3),
      decoration: BoxDecoration(
        color: color.withOpacity(0.12),
        borderRadius: BorderRadius.circular(6),
      ),
      child: Text(label,
          style: TextStyle(
              color: color, fontSize: 10, fontWeight: FontWeight.w600)),
    );
  }
}

// ── A single model's run row (expandable to show output) ────────────────────

class _RunTile extends StatefulWidget {
  final BenchmarkRun run;
  final int? rank;
  final bool isWinner;
  const _RunTile({required this.run, required this.rank, required this.isWinner});

  @override
  State<_RunTile> createState() => _RunTileState();
}

class _RunTileState extends State<_RunTile> {
  bool _expanded = false;

  @override
  Widget build(BuildContext context) {
    final run = widget.run;
    final pColor = ModelProvider.typeColor(run.providerType);

    return Container(
      decoration: BoxDecoration(
        color: widget.isWinner ? AppColors.accent.withOpacity(0.06) : null,
        border: const Border(bottom: BorderSide(color: AppColors.border)),
      ),
      child: Column(
        children: [
          InkWell(
            onTap: run.output == null && run.error == null
                ? null
                : () => setState(() => _expanded = !_expanded),
            child: Padding(
              padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 12),
              child: Row(
                children: [
                  _rankBadge(),
                  const SizedBox(width: 12),
                  Container(
                    width: 6,
                    height: 6,
                    decoration:
                        BoxDecoration(color: pColor, shape: BoxShape.circle),
                  ),
                  const SizedBox(width: 8),
                  Expanded(
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Row(
                          children: [
                            Flexible(
                              child: Text(run.modelLabel,
                                  overflow: TextOverflow.ellipsis,
                                  style: TextStyle(
                                      color: AppColors.textPrimary,
                                      fontSize: 13,
                                      fontWeight: widget.isWinner
                                          ? FontWeight.w700
                                          : FontWeight.w500)),
                            ),
                            if (widget.isWinner) ...[
                              const SizedBox(width: 6),
                              const Icon(Icons.emoji_events,
                                  size: 13, color: AppColors.accent),
                            ],
                          ],
                        ),
                        Text(run.providerName,
                            style: TextStyle(color: pColor, fontSize: 10)),
                      ],
                    ),
                  ),
                  ..._metrics(run),
                  const SizedBox(width: 4),
                  if (run.output != null || run.error != null)
                    Icon(_expanded ? Icons.expand_less : Icons.expand_more,
                        size: 18, color: AppColors.textMuted),
                ],
              ),
            ),
          ),
          if (_expanded) _detail(run),
        ],
      ),
    );
  }

  Widget _rankBadge() {
    if (widget.run.status == BenchmarkRunStatus.running ||
        widget.run.status == BenchmarkRunStatus.pending) {
      return const SizedBox(
        width: 20,
        height: 20,
        child: Padding(
          padding: EdgeInsets.all(3),
          child: CircularProgressIndicator(
              strokeWidth: 1.5,
              valueColor: AlwaysStoppedAnimation(AppColors.statusRunning)),
        ),
      );
    }
    if (widget.run.status == BenchmarkRunStatus.error) {
      return const SizedBox(
          width: 20,
          child: Icon(Icons.error_outline, size: 16, color: AppColors.error));
    }
    final rank = widget.rank;
    return SizedBox(
      width: 20,
      child: Text(rank == null ? '–' : '#$rank',
          textAlign: TextAlign.center,
          style: TextStyle(
              color: widget.isWinner ? AppColors.accent : AppColors.textMuted,
              fontSize: 12,
              fontWeight: FontWeight.w700)),
    );
  }

  List<Widget> _metrics(BenchmarkRun run) {
    if (run.status != BenchmarkRunStatus.done) return const [];
    final tps = run.tokensPerSecond;
    return [
      _metric('${run.latency!.inMilliseconds} ms', 'latency'),
      _metric(run.totalTokens != null ? '${run.totalTokens}' : '—', 'tokens'),
      _metric(tps != null ? '${tps.toStringAsFixed(1)}' : '—', 'tok/s'),
    ];
  }

  Widget _metric(String value, String label) {
    return Container(
      width: 72,
      margin: const EdgeInsets.only(left: 4),
      child: Column(
        children: [
          Text(value,
              style: const TextStyle(
                  color: AppColors.textPrimary,
                  fontSize: 13,
                  fontWeight: FontWeight.w600)),
          Text(label,
              style: const TextStyle(color: AppColors.textMuted, fontSize: 9)),
        ],
      ),
    );
  }

  Widget _detail(BenchmarkRun run) {
    final isErr = run.error != null;
    final text = run.error ?? run.output ?? '';
    return Container(
      width: double.infinity,
      margin: const EdgeInsets.fromLTRB(16, 0, 16, 14),
      padding: const EdgeInsets.all(12),
      decoration: BoxDecoration(
        color: AppColors.surfaceAlt,
        borderRadius: BorderRadius.circular(8),
        border: Border.all(
            color: isErr ? AppColors.error.withOpacity(0.4) : AppColors.border),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              Text(isErr ? 'ERROR' : 'OUTPUT',
                  style: TextStyle(
                      color: isErr ? AppColors.error : AppColors.textMuted,
                      fontSize: 10,
                      fontWeight: FontWeight.w700,
                      letterSpacing: 0.8)),
              const Spacer(),
              if (!isErr)
                InkWell(
                  onTap: () {
                    Clipboard.setData(ClipboardData(text: text));
                    ScaffoldMessenger.of(context).showSnackBar(
                      const SnackBar(content: Text('Output copied')),
                    );
                  },
                  child: const Icon(Icons.copy,
                      size: 13, color: AppColors.textMuted),
                ),
            ],
          ),
          const SizedBox(height: 6),
          SelectableText(text,
              style: TextStyle(
                  color: isErr ? AppColors.error : AppColors.textSecondary,
                  fontSize: 12,
                  height: 1.4)),
        ],
      ),
    );
  }
}

// ── New benchmark dialog ────────────────────────────────────────────────────

class BenchmarkDialog extends StatefulWidget {
  const BenchmarkDialog({super.key});

  static void show(BuildContext context) => showDialog(
        context: context,
        builder: (_) => const BenchmarkDialog(),
      );

  @override
  State<BenchmarkDialog> createState() => _BenchmarkDialogState();
}

class _BenchmarkDialogState extends State<BenchmarkDialog> {
  final _name = TextEditingController();
  final _prompt = TextEditingController();
  final Set<String> _selected = {}; // model ids
  double _temperature = 0.7;

  @override
  void dispose() {
    _name.dispose();
    _prompt.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final prov = context.watch<AgentProvider>();
    final models = prov.availableModels;

    return Dialog(
      backgroundColor: AppColors.surface,
      shape: RoundedRectangleBorder(
        borderRadius: BorderRadius.circular(14),
        side: const BorderSide(color: AppColors.border),
      ),
      child: ConstrainedBox(
        constraints: const BoxConstraints(maxWidth: 560, maxHeight: 640),
        child: Padding(
          padding: const EdgeInsets.all(22),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            mainAxisSize: MainAxisSize.min,
            children: [
              Row(
                children: [
                  Container(
                    padding: const EdgeInsets.all(8),
                    decoration: BoxDecoration(
                      color: AppColors.accent.withOpacity(0.15),
                      borderRadius: BorderRadius.circular(10),
                    ),
                    child: const Icon(Icons.speed,
                        color: AppColors.accent, size: 20),
                  ),
                  const SizedBox(width: 12),
                  const Text('New Benchmark',
                      style: TextStyle(
                          color: AppColors.textPrimary,
                          fontSize: 18,
                          fontWeight: FontWeight.w700)),
                  const Spacer(),
                  IconButton(
                    icon: const Icon(Icons.close, size: 18),
                    color: AppColors.textMuted,
                    onPressed: () => Navigator.pop(context),
                  ),
                ],
              ),
              const SizedBox(height: 16),
              TextField(
                controller: _name,
                style: const TextStyle(color: AppColors.textPrimary),
                decoration: const InputDecoration(
                  labelText: 'Name (optional)',
                  hintText: 'e.g. Summarization quality',
                ),
              ),
              const SizedBox(height: 12),
              TextField(
                controller: _prompt,
                autofocus: true,
                maxLines: 4,
                style: const TextStyle(color: AppColors.textPrimary),
                decoration: const InputDecoration(
                  labelText: 'Prompt',
                  hintText: 'The task every model will be given…',
                  alignLabelWithHint: true,
                ),
              ),
              const SizedBox(height: 16),
              Row(
                children: [
                  const Text('Temperature',
                      style: TextStyle(
                          color: AppColors.textSecondary, fontSize: 13)),
                  Expanded(
                    child: Slider(
                      value: _temperature,
                      min: 0,
                      max: 1,
                      divisions: 20,
                      label: _temperature.toStringAsFixed(2),
                      activeColor: AppColors.accent,
                      onChanged: (v) => setState(() => _temperature = v),
                    ),
                  ),
                  Text(_temperature.toStringAsFixed(2),
                      style: const TextStyle(
                          color: AppColors.textPrimary, fontSize: 13)),
                ],
              ),
              const SizedBox(height: 8),
              Row(
                children: [
                  const Text('Models to compare',
                      style: TextStyle(
                          color: AppColors.textSecondary,
                          fontSize: 13,
                          fontWeight: FontWeight.w600)),
                  const Spacer(),
                  Text('${_selected.length} selected',
                      style: const TextStyle(
                          color: AppColors.textMuted, fontSize: 11)),
                ],
              ),
              const SizedBox(height: 8),
              Flexible(child: _modelList(models)),
              const SizedBox(height: 16),
              Row(
                mainAxisAlignment: MainAxisAlignment.end,
                children: [
                  TextButton(
                    onPressed: () => Navigator.pop(context),
                    child: const Text('Cancel'),
                  ),
                  const SizedBox(width: 8),
                  ElevatedButton.icon(
                    icon: const Icon(Icons.play_arrow, size: 18),
                    label: const Text('Run'),
                    style: ElevatedButton.styleFrom(
                        backgroundColor: AppColors.accent,
                        foregroundColor: Colors.black),
                    onPressed: _canRun(models) ? () => _run(prov, models) : null,
                  ),
                ],
              ),
            ],
          ),
        ),
      ),
    );
  }

  Widget _modelList(List<ModelInfo> models) {
    if (models.isEmpty) {
      return Container(
        width: double.infinity,
        padding: const EdgeInsets.all(16),
        decoration: BoxDecoration(
          color: AppColors.surfaceAlt,
          borderRadius: BorderRadius.circular(10),
          border: Border.all(color: AppColors.border),
        ),
        child: const Text(
          'No connected models. Open Settings → Model Providers and connect at '
          'least one provider (Anthropic, OpenAI, Gemini, Ollama, …) to benchmark.',
          style: TextStyle(color: AppColors.textMuted, fontSize: 12),
        ),
      );
    }
    return Container(
      decoration: BoxDecoration(
        color: AppColors.surfaceAlt,
        borderRadius: BorderRadius.circular(10),
        border: Border.all(color: AppColors.border),
      ),
      child: ListView.builder(
        shrinkWrap: true,
        itemCount: models.length,
        itemBuilder: (_, i) {
          final m = models[i];
          final on = _selected.contains(m.id);
          final pColor = ModelProvider.typeColor(m.providerType);
          return CheckboxListTile(
            dense: true,
            value: on,
            activeColor: AppColors.accent,
            controlAffinity: ListTileControlAffinity.leading,
            onChanged: (v) => setState(() {
              if (v == true) {
                _selected.add(m.id);
              } else {
                _selected.remove(m.id);
              }
            }),
            title: Text(m.displayName,
                style: const TextStyle(
                    color: AppColors.textPrimary, fontSize: 13)),
            subtitle: Text(m.providerName,
                style: TextStyle(color: pColor, fontSize: 11)),
          );
        },
      ),
    );
  }

  bool _canRun(List<ModelInfo> models) =>
      _selected.isNotEmpty && _prompt.text.trim().isNotEmpty;

  void _run(AgentProvider prov, List<ModelInfo> models) {
    final chosen = models.where((m) => _selected.contains(m.id)).toList();
    prov.runBenchmark(
      prompt: _prompt.text.trim(),
      models: chosen,
      temperature: _temperature,
      name: _name.text,
    );
    Navigator.pop(context);
  }
}
