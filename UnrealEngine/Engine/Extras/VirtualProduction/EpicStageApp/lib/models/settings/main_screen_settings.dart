// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:epic_common/preferences.dart';
import 'package:streaming_shared_preferences/streaming_shared_preferences.dart';

/// Holds settings that affect the entire layout of the main screen.
class MainScreenSettings {
  MainScreenSettings(PreferencesBundle preferences)
      : bIsOutlinerPanelOpen = preferences.persistent.getBool(
          'common.bIsOutlinerPanelOpen',
          defaultValue: false,
        ),
        selectedTab = preferences.persistent.getInt(
          'common.selectedTab',
          defaultValue: 0,
        );

  /// Whether the outliner panel is open.
  final Preference<bool> bIsOutlinerPanelOpen;

  /// The selected top-level tab index.
  final Preference<int> selectedTab;
}
