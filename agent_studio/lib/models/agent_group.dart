import 'package:flutter/material.dart';
import 'agent_model.dart';

enum FormationType {
  hierarchy,   // parent-child tree — orchestrator delegates to workers
  parallel,    // all agents run the same task simultaneously
  sequential,  // agents run one after another, output passed forward
  pipeline,    // each agent transforms the output of the previous
  broadcast,   // one coordinator sends task to all, collects responses
  consensus,   // all agents vote, majority wins
  mesh,        // any agent can talk to any other agent
  star,        // one hub agent routes to specialist spokes
  graph,       // arbitrary directed graph — each agent connects to any subset of others
}

// Keep GroupMode as alias for backward compat
typedef GroupMode = FormationType;

class AgentGroup {
  final String id;
  String name;
  String description;
  FormationType formation;
  List<String> agentIds;
  String? coordinatorId;
  // graph formation: adjacency map — agentId → list of agentIds it sends output to
  Map<String, List<String>> edges;
  final List<ChatMessage> sharedHistory;

  AgentGroup({
    required this.id,
    required this.name,
    this.description = '',
    this.formation   = FormationType.parallel,
    List<String>? agentIds,
    this.coordinatorId,
    Map<String, List<String>>? edges,
    List<ChatMessage>? sharedHistory,
  })  : agentIds      = agentIds ?? [],
        edges         = edges ?? {},
        sharedHistory = sharedHistory ?? [];

  static String formationLabel(FormationType f) {
    switch (f) {
      case FormationType.hierarchy:  return 'Hierarchy';
      case FormationType.parallel:   return 'Parallel';
      case FormationType.sequential: return 'Sequential';
      case FormationType.pipeline:   return 'Pipeline';
      case FormationType.broadcast:  return 'Broadcast';
      case FormationType.consensus:  return 'Consensus';
      case FormationType.mesh:       return 'Mesh';
      case FormationType.star:       return 'Star';
      case FormationType.graph:      return 'Graph';
    }
  }

  static String formationDescription(FormationType f) {
    switch (f) {
      case FormationType.hierarchy:  return 'Tree of agents — orchestrator delegates subtasks to workers';
      case FormationType.parallel:   return 'All agents tackle the same task simultaneously';
      case FormationType.sequential: return 'Agents run one after another, each seeing previous output';
      case FormationType.pipeline:   return 'Each agent transforms the output of the previous';
      case FormationType.broadcast:  return 'Coordinator sends to all agents and collects responses';
      case FormationType.consensus:  return 'All agents propose, majority vote determines result';
      case FormationType.mesh:       return 'Every agent can communicate directly with every other';
      case FormationType.star:       return 'Hub agent routes tasks to specialist spokes';
      case FormationType.graph:      return 'Arbitrary directed graph — each agent connects to any subset of others in any order';
    }
  }

  static IconData formationIcon(FormationType f) {
    switch (f) {
      case FormationType.hierarchy:  return Icons.account_tree;
      case FormationType.parallel:   return Icons.call_split;
      case FormationType.sequential: return Icons.linear_scale;
      case FormationType.pipeline:   return Icons.arrow_forward;
      case FormationType.broadcast:  return Icons.cell_tower;
      case FormationType.consensus:  return Icons.how_to_vote;
      case FormationType.mesh:       return Icons.hub;
      case FormationType.star:       return Icons.star_outline;
      case FormationType.graph:      return Icons.device_hub;
    }
  }
}
