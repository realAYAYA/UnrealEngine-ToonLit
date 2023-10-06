// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:streaming_shared_preferences/streaming_shared_preferences.dart';

import '../../utilities/preferences_bundle.dart';
import '../../utilities/streaming_preferences_extensions.dart';

/// Which type of actor properties to display in the details panel.
enum DetailsPropertyDisplayType {
  appearance,
  orientation,
}

/// Holds user settings used in the Details tab.
class DetailsTabSettings {
  DetailsTabSettings(PreferencesBundle preferences)
      : detailsPropertyDisplayType = preferences.persistent.getEnum(
          'colorGradingTab.detailsPropertyDisplayType',
          defaultValue: DetailsPropertyDisplayType.appearance,
          enumValues: DetailsPropertyDisplayType.values,
        );

  /// Which type of actor properties to display in the details panel.
  final Preference<DetailsPropertyDisplayType> detailsPropertyDisplayType;
}
