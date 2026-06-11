import 'models.dart';

class AgentStore {
  final _agents = <String, AgentConfig>{};
  final _groups = <String, GroupConfig>{};
  final _runs = <String, AgentRun>{};

  // Agents
  AgentConfig put(AgentConfig a) { _agents[a.id] = a; return a; }
  AgentConfig? get(String id) => _agents[id];
  void remove(String id) => _agents.remove(id);
  List<AgentConfig> listAgents() => _agents.values.toList();

  // Groups
  GroupConfig putGroup(GroupConfig g) { _groups[g.id] = g; return g; }
  GroupConfig? getGroup(String id) => _groups[id];
  void removeGroup(String id) => _groups.remove(id);
  List<GroupConfig> listGroups() => _groups.values.toList();
  Map<String, AgentConfig> get agentsMap => Map.unmodifiable(_agents);

  // Runs
  AgentRun putRun(AgentRun r) { _runs[r.id] = r; return r; }
  AgentRun? getRun(String id) => _runs[id];
  List<AgentRun> listRuns() => _runs.values.toList();
}
