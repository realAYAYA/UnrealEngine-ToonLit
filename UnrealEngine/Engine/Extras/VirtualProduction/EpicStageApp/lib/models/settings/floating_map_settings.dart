// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:streaming_shared_preferences/streaming_shared_preferences.dart';

import '../../utilities/preferences_bundle.dart';
import '../../utilities/streaming_preferences_extensions.dart';

/// Which side of the screen the map preview is on.
enum PreviewMapSide {
  left,
  right,
}

/// Holds user settings used by the floating preview map.
class FloatingMapSettings {
  FloatingMapSettings(PreferencesBundle preferences)
      : mapSide = preferences.persistent.getEnum<PreviewMapSide>(
          'floatingMapSettings.mapSide',
          defaultValue: PreviewMapSide.right,
          enumValues: PreviewMapSide.values,
        ),
        mapY = preferences.persistent.getDouble(
          'floatingMapSettings.mapY',
          defaultValue: 1.0,
        ),
        bIsMapDocked = preferences.persistent.getBool(
          'floatingMapSettings.bIsMapDocked',
          defaultValue: false,
        );

  /// Which side of the screen the preview map is on.
  Preference<PreviewMapSide> mapSide;

  /// The vertical position at which the map preview should sit (as a fraction of its container height).
  Preference<double> mapY;

  /// Whether the preview map is docked off-screen.
  Preference<bool> bIsMapDocked;
}
