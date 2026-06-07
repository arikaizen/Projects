import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'providers/agent_provider.dart';
import 'screens/dashboard_screen.dart';
import 'services/agent_api_service.dart';
import 'theme/app_theme.dart';

void main() {
  runApp(const AgentStudioApp());
}

class AgentStudioApp extends StatelessWidget {
  const AgentStudioApp({super.key});

  @override
  Widget build(BuildContext context) {
    return ChangeNotifierProvider(
      create: (_) => AgentProvider(AgentApiService()),
      child: MaterialApp(
        title: 'Agent Studio',
        debugShowCheckedModeBanner: false,
        theme: AppTheme.dark,
        home: const DashboardScreen(),
      ),
    );
  }
}
