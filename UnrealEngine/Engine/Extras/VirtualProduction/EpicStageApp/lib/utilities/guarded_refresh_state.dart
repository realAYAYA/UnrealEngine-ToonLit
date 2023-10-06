// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:flutter/material.dart';

/// Adds a function to refresh the widget only if it's still mounted.
mixin GuardedRefreshState<T extends StatefulWidget> on State<T> {
  /// Refresh the widget if it's still mounted.
  void guardedRefresh() {
    if (mounted) {
      setState(() {});
    }
  }

  /// Convenience version of the function for listening to a subscription stream, which passes in a value that we don't
  /// care about for this function's purpose.
  void refreshOnData(_) {
    guardedRefresh();
  }
}
