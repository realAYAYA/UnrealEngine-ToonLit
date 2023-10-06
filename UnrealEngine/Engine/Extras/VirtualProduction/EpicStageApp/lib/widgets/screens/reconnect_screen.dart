// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:flutter/material.dart';

/// Screen shown when reconnecting to the engine.
class ReconnectScreen extends StatelessWidget {
  const ReconnectScreen({Key? key}) : super(key: key);

  static const String title = 'Connect to Unreal Engine';
  static const String route = '/reconnect';

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: Theme.of(context).colorScheme.background,
      body: Center(
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            Text(
              style: Theme.of(context).textTheme.titleLarge,
              'Reconnecting to Unreal Engine...',
            ),
            const SizedBox(height: 20),
            const SizedBox.square(
              dimension: 120,
              child: CircularProgressIndicator(
                strokeWidth: 8,
              ),
            ),
          ],
        ),
      ),
    );
  }
}
