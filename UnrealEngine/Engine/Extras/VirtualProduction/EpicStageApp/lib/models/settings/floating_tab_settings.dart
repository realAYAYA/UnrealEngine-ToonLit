// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:epic_common/preferences.dart';
import 'package:streaming_shared_preferences/streaming_shared_preferences.dart';

/// Which side of the screen a tab is on.
enum TabCurrentSide {
  left,
  right,
}

/// Generic holder for user settings used to configure floating tabs.
/// required and instance of [preference] bundle and a [prefix] string indicating which tab an instance of
/// [FloatingTabSettings] controls.
class FloatingTabSettings {
  FloatingTabSettings(PreferencesBundle preferences, String prefix)
      : tabSide = preferences.persistent.getEnum<TabCurrentSide>(
          '$prefix.tabSide',
          defaultValue: TabCurrentSide.right,
          enumValues: TabCurrentSide.values,
        ),
        yAxis = preferences.persistent.getDouble(
          '$prefix.yAxis',
          defaultValue: 1.0,
        ),
        bIsTabDocked = preferences.persistent.getBool(
          '$prefix.bIsTabDocked',
          defaultValue: true,
        );

  /// Which side of the screen the tab is on.
  Preference<TabCurrentSide> tabSide;

  /// The vertical position at which the tab should sit (as a fraction of its container height).
  Preference<double> yAxis;

  /// Whether the tab is docked off-screen.
  Preference<bool> bIsTabDocked;
}
