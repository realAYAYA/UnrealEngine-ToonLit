// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:epic_common/preferences.dart';
import 'package:streaming_shared_preferences/streaming_shared_preferences.dart';

/// What type of color editor to display.
enum ColorWheelEditMode {
  wheel,
  sliders,
}

/// Which color components to display.
enum ColorWheelColorMode {
  rgb,
  hsv,
}

/// Color ranges that the user can control separately.
enum ColorGradingRange {
  global,
  shadows,
  midtones,
  highlights,
}

/// Color grading color properties that the user can control in each range.
enum ColorGradingSubproperty {
  saturation,
  contrast,
  gamma,
  gain,
  offset,
}

/// Settings that the user can change on color wheels.
class ColorWheelSettings {
  ColorWheelSettings(PreferencesBundle preferences)
      : editMode = preferences.persistent.getEnum(
          'colorWheel.editMode',
          defaultValue: ColorWheelEditMode.wheel,
          enumValues: ColorWheelEditMode.values,
        ),
        colorMode = preferences.persistent.getEnum(
          'colorWheel.colorMode',
          defaultValue: ColorWheelColorMode.rgb,
          enumValues: ColorWheelColorMode.values,
        );

  /// What type of color editor to display.
  final Preference<ColorWheelEditMode> editMode;

  /// Which color components to display.
  final Preference<ColorWheelColorMode> colorMode;
}
