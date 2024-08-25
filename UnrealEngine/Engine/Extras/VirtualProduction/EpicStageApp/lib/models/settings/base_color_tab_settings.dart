// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:epic_common/preferences.dart';
import 'package:streaming_shared_preferences/streaming_shared_preferences.dart';

import '../../utilities/streaming_preferences_extensions.dart';
import 'color_wheel_settings.dart';

/// Types of limits the user can edit for a color range.
enum RangeLimitType {
  min,
  max,
}

/// User settings for [BaseColorTab].
class BaseColorTabSettings {
  BaseColorTabSettings(PreferencesBundle preferences)
      : colorGradingRange = preferences.persistent.getEnum(
          'colorTab.colorGradingRange',
          defaultValue: ColorGradingRange.global,
          enumValues: ColorGradingRange.values,
        ),
        colorGradingSubproperty = preferences.persistent.getEnum(
          'colorTab.colorGradingSubproperty',
          defaultValue: ColorGradingSubproperty.gain,
          enumValues: ColorGradingSubproperty.values,
        ),
        colorGradingHighlightsRangeLimitType = preferences.persistent.getEnum(
          'colorTab.colorGradingHighlightsRangeLimitType',
          defaultValue: RangeLimitType.min,
          enumValues: RangeLimitType.values,
        );

  /// The last color grading range selected for color grading control.
  final Preference<ColorGradingRange> colorGradingRange;

  /// The last color grading subproperty selected for color grading control.
  final Preference<ColorGradingSubproperty> colorGradingSubproperty;

  /// Which type of highlights range slider to show when color grading.
  final Preference<RangeLimitType> colorGradingHighlightsRangeLimitType;
}
