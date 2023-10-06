// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:flutter/material.dart';

/// An extension of ChangeNotifier that allows [notifyListeners] to be called externally.
class ExternalNotifier extends ChangeNotifier {
  void notifyListeners() {
    // This function is normally protected, but we want to make it public for this class.
    super.notifyListeners();
  }
}
