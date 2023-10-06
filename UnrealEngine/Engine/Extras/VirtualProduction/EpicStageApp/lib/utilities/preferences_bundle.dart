// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:flutter/cupertino.dart';
import 'package:provider/provider.dart';
import 'package:streaming_shared_preferences/streaming_shared_preferences.dart';

import 'transient_preference.dart';

/// A convenience class holding both types of centralized preference classes so they can be easily accessed throughout
/// the app.
class PreferencesBundle {
  const PreferencesBundle(this.persistent, this.transient);

  /// Preferences that will be serialized to disk and persisted across runs of the app.
  final StreamingSharedPreferences persistent;

  /// Preferences that will be held in memory and cleared between runs of the app./**/
  final TransientSharedPreferences transient;

  /// Get the [PreferencesBundle] provided in the given [context].
  static PreferencesBundle of(BuildContext context) {
    return Provider.of(context, listen: false);
  }
}
