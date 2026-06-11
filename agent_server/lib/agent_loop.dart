import 'dart:async';
import 'dart:convert';
import 'models.dart';
import 'llm_router.dart';
import 'tools.dart';

typedef StepSink = void Function(RunStep step);

class AgentLoop {
  final LlmRouter _router;
  AgentLoop() : _router = LlmRouter();

  /// Run the agent loop. Calls [onStep] for each step.
  /// Returns the final text reply.
  Future<String> run({
    required AgentConfig agent,
    required String userPrompt,
    required StepSink onStep,
    List<Map<String, dynamic>> history = const [], // prior messages [{role, content}]
  }) async {
    final messages = <ChatMessage>[
      if (agent.systemPrompt.isNotEmpty)
        ChatMessage(role: 'system', content: agent.systemPrompt),
      // Inject prior history
      for (final h in history)
        ChatMessage(role: h['role'] as String, content: h['content'] as String),
      ChatMessage(role: 'user', content: userPrompt),
    ];

    // Build tool definitions for this agent
    final tools = agent.tools
        .where((t) => kToolDefinitions.containsKey(t))
        .map((t) => kToolDefinitions[t]!)
        .toList();

    for (var step = 0; step < agent.maxSteps; step++) {
      onStep(RunStep(type: StepType.llmCall,
          content: 'Calling ${agent.llm.model} (step ${step + 1})'));

      late LlmResponse response;
      try {
        response = await _router.chat(
          config: agent.llm,
          messages: messages,
          tools: tools,
        );
      } catch (e) {
        onStep(RunStep(type: StepType.llmCall, content: 'LLM error: $e'));
        throw Exception('LLM call failed: $e');
      }

      if (!response.isToolCall) {
        // Final text answer
        final reply = response.text ?? '';
        onStep(RunStep(type: StepType.llmCall, content: reply));
        return reply;
      }

      // Tool call
      onStep(RunStep(
        type: StepType.toolCall,
        toolName: response.toolName,
        toolInput: response.toolInput,
      ));

      // Add assistant tool-use to messages
      messages.add(ChatMessage(
        role: 'assistant',
        content: jsonEncode({'tool': response.toolName, 'input': response.toolInput}),
      ));

      // Execute tool
      final toolResult = await executeTool(
          response.toolName!, response.toolInput ?? {});
      onStep(RunStep(type: StepType.toolResult,
          toolName: response.toolName, content: toolResult));

      // Feed result back
      messages.add(ChatMessage(
        role: 'tool',
        content: toolResult,
        toolCallId: response.toolCallId,
        toolName: response.toolName,
      ));
    }

    return 'Reached maximum steps (${agent.maxSteps}).';
  }
}
