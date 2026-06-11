import 'dart:async';
import 'models.dart';
import 'agent_loop.dart';

class FormationRunner {
  final AgentLoop _loop;
  FormationRunner() : _loop = AgentLoop();

  Future<String> run({
    required GroupConfig group,
    required Map<String, AgentConfig> agents,
    required String userPrompt,
    required StepSink onStep,
  }) async {
    final agentList = group.agentIds
        .map((id) => agents[id])
        .where((a) => a != null)
        .cast<AgentConfig>()
        .toList();

    if (agentList.isEmpty) return 'No agents in group.';

    switch (group.formation) {
      case FormationType.parallel:
        return _parallel(agentList, userPrompt, onStep);
      case FormationType.sequential:
      case FormationType.pipeline:
        return _sequential(agentList, userPrompt, onStep);
      case FormationType.broadcast:
        return _broadcast(agentList, userPrompt, onStep);
      case FormationType.consensus:
        return _consensus(agentList, userPrompt, onStep, agents);
      case FormationType.hierarchy:
        return _hierarchy(group, agentList, userPrompt, onStep, agents);
      case FormationType.star:
        return _star(group, agentList, userPrompt, onStep, agents);
      case FormationType.mesh:
        return _mesh(agentList, userPrompt, onStep);
      case FormationType.graph:
        return _graph(group, agents, userPrompt, onStep);
    }
  }

  // All agents run simultaneously, results are joined
  Future<String> _parallel(List<AgentConfig> agents, String prompt, StepSink onStep) async {
    onStep(RunStep(type: StepType.formation, content: 'parallel: running ${agents.length} agents simultaneously'));
    final futures = agents.map((a) => _loop.run(agent: a, userPrompt: prompt, onStep: onStep));
    final results = await Future.wait(futures);
    return results.asMap().entries
        .map((e) => '**${agents[e.key].name}**: ${e.value}')
        .join('\n\n');
  }

  // Output of agent N becomes input to agent N+1
  Future<String> _sequential(List<AgentConfig> agents, String prompt, StepSink onStep) async {
    onStep(RunStep(type: StepType.formation, content: 'sequential: chaining ${agents.length} agents'));
    String current = prompt;
    for (final agent in agents) {
      onStep(RunStep(type: StepType.agentCall, agentId: agent.id, agentName: agent.name, content: current));
      current = await _loop.run(agent: agent, userPrompt: current, onStep: onStep);
      onStep(RunStep(type: StepType.agentResult, agentId: agent.id, agentName: agent.name, content: current));
    }
    return current;
  }

  // Same prompt to all, collect all responses
  Future<String> _broadcast(List<AgentConfig> agents, String prompt, StepSink onStep) async {
    onStep(RunStep(type: StepType.formation, content: 'broadcast: sending to ${agents.length} agents'));
    final results = <String>[];
    for (final a in agents) {
      onStep(RunStep(type: StepType.agentCall, agentId: a.id, agentName: a.name));
      final r = await _loop.run(agent: a, userPrompt: prompt, onStep: onStep);
      results.add('**${a.name}**: $r');
    }
    return results.join('\n\n---\n\n');
  }

  // Broadcast then synthesize with first agent (coordinator)
  Future<String> _consensus(List<AgentConfig> agents, String prompt, StepSink onStep,
      Map<String, AgentConfig> allAgents) async {
    onStep(RunStep(type: StepType.formation, content: 'consensus: broadcasting then synthesizing'));
    final responses = await Future.wait(
      agents.skip(1).map((a) => _loop.run(agent: a, userPrompt: prompt, onStep: onStep))
    );
    final combined = responses.asMap().entries
        .map((e) => 'Agent ${agents[e.key + 1].name}: ${e.value}')
        .join('\n\n');
    final coordinator = agents.first;
    final synthesisPrompt = 'Synthesize these responses into one answer:\n\nOriginal question: $prompt\n\n$combined';
    onStep(RunStep(type: StepType.agentCall, agentId: coordinator.id, agentName: coordinator.name, content: 'Synthesizing...'));
    return _loop.run(agent: coordinator, userPrompt: synthesisPrompt, onStep: onStep);
  }

  // Coordinator (first agent or coordinatorId) delegates to sub-agents
  Future<String> _hierarchy(GroupConfig group, List<AgentConfig> agents,
      String prompt, StepSink onStep, Map<String, AgentConfig> allAgents) async {
    final coordinator = group.coordinatorId != null
        ? (allAgents[group.coordinatorId] ?? agents.first)
        : agents.first;
    final subAgents = agents.where((a) => a.id != coordinator.id).toList();
    onStep(RunStep(type: StepType.formation,
        content: 'hierarchy: coordinator=${coordinator.name}, sub-agents=${subAgents.map((a) => a.name).join(', ')}'));
    // Coordinator gets prompt + list of sub-agent capabilities
    final subList = subAgents.map((a) => '- ${a.name}: ${a.systemPrompt.split('.').first}').join('\n');
    final coordPrompt = '$prompt\n\nAvailable sub-agents:\n$subList\n\nRespond directly or prefix with [DELEGATE:AgentName] to delegate.';
    var reply = await _loop.run(agent: coordinator, userPrompt: coordPrompt, onStep: onStep);
    // Check if coordinator wants to delegate
    final delegateMatch = RegExp(r'\[DELEGATE:(.+?)\]').firstMatch(reply);
    if (delegateMatch != null) {
      final targetName = delegateMatch.group(1)!.trim();
      final target = subAgents.firstWhere((a) => a.name == targetName, orElse: () => subAgents.first);
      onStep(RunStep(type: StepType.agentCall, agentId: target.id, agentName: target.name));
      reply = await _loop.run(agent: target, userPrompt: prompt, onStep: onStep);
    }
    return reply;
  }

  // One central agent, all others are spokes it can query
  Future<String> _star(GroupConfig group, List<AgentConfig> agents,
      String prompt, StepSink onStep, Map<String, AgentConfig> allAgents) async {
    final center = group.coordinatorId != null
        ? (allAgents[group.coordinatorId] ?? agents.first)
        : agents.first;
    final spokes = agents.where((a) => a.id != center.id).toList();
    // Gather spoke responses in parallel
    final spokeResults = await Future.wait(
        spokes.map((a) => _loop.run(agent: a, userPrompt: prompt, onStep: onStep)));
    final context = spokeResults.asMap().entries
        .map((e) => '${spokes[e.key].name}: ${e.value}').join('\n\n');
    final centerPrompt = 'Consolidate these responses for: $prompt\n\n$context';
    return _loop.run(agent: center, userPrompt: centerPrompt, onStep: onStep);
  }

  // Each agent sees all prior agent outputs (round-robin)
  Future<String> _mesh(List<AgentConfig> agents, String prompt, StepSink onStep) async {
    onStep(RunStep(type: StepType.formation, content: 'mesh: each agent sees all prior outputs'));
    final results = <String, String>{};
    for (final a in agents) {
      final context = results.entries.map((e) => '${e.key}: ${e.value}').join('\n\n');
      final input = context.isEmpty ? prompt : '$prompt\n\nPrior agent responses:\n$context';
      results[a.name] = await _loop.run(agent: a, userPrompt: input, onStep: onStep);
    }
    return results.entries.map((e) => '**${e.key}**: ${e.value}').join('\n\n');
  }

  // Topological execution following explicit edge map
  Future<String> _graph(GroupConfig group, Map<String, AgentConfig> allAgents,
      String prompt, StepSink onStep) async {
    onStep(RunStep(type: StepType.formation, content: 'graph: following edge topology'));
    // Find entry nodes (no incoming edges)
    final allTargets = group.edges.values.expand((v) => v).toSet();
    final entryIds = group.agentIds.where((id) => !allTargets.contains(id)).toList();
    final results = <String, String>{};

    Future<String> runNode(String id) async {
      if (results.containsKey(id)) return results[id]!;
      final agent = allAgents[id];
      if (agent == null) return '';
      // Gather inputs from agents that point to this one
      final incoming = group.edges.entries
          .where((e) => e.value.contains(id))
          .map((e) => e.key)
          .toList();
      final inputParts = <String>[];
      for (final inId in incoming) {
        final r = await runNode(inId);
        if (r.isNotEmpty) inputParts.add('${allAgents[inId]?.name ?? inId}: $r');
      }
      final nodePrompt = inputParts.isEmpty
          ? prompt
          : '$prompt\n\nInputs:\n${inputParts.join('\n\n')}';
      final r = await _loop.run(agent: agent, userPrompt: nodePrompt, onStep: onStep);
      results[id] = r;
      return r;
    }

    // Find leaf nodes (no outgoing edges to other nodes in the group)
    final leafIds = group.agentIds
        .where((id) => (group.edges[id] ?? []).isEmpty)
        .toList();
    if (leafIds.isEmpty) leafIds.addAll(entryIds);

    await Future.wait(leafIds.map(runNode));
    return leafIds.map((id) => '**${allAgents[id]?.name ?? id}**: ${results[id] ?? ''}').join('\n\n');
  }
}
