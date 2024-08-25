// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:epic_common/preferences.dart';
import 'package:streaming_shared_preferences/streaming_shared_preferences.dart';

import '../unreal_types.dart';

/// Holds user settings used in the Color Grading tab.
class ColorGradingTabSettings {
  ColorGradingTabSettings(PreferencesBundle preferences)
      : target = preferences.transient.get(
          'colorGradingTab.target',
          defaultValue: UnrealProperty.empty,
        ),
        panelHeight = preferences.persistent.getDouble(
          'colorGradingTab.panelHeight',
          defaultValue: -1,
        );

  /// The property selected as the color grading target.
  final TransientPreference<UnrealProperty> target;

  /// How tall the color grading target panel should be.
  final Preference<double> panelHeight;
}
