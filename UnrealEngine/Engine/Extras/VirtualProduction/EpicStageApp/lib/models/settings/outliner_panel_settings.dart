// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:epic_common/preferences.dart';
import 'package:streaming_shared_preferences/streaming_shared_preferences.dart';

import '../../utilities/streaming_preferences_extensions.dart';
import '../unreal_object_filters.dart';

/// Ways the user can sort the list of actors in the outliner panel.
enum OutlinerActorSortMode {
  name,
  recent,
}

/// Holds user settings used to render and control the stage map.
class OutlinerPanelSettings {
  OutlinerPanelSettings(PreferencesBundle preferences)
      : bShouldSortActorsDescending = preferences.persistent.getBool(
          'outliner.bShouldSortActorsDescending',
          defaultValue: true,
        ),
        actorSortMode = preferences.persistent.getEnum<OutlinerActorSortMode>(
          'outliner.actorSortMode',
          defaultValue: OutlinerActorSortMode.name,
          enumValues: OutlinerActorSortMode.values,
        ),
        selectedFilters = preferences.persistent.getUnrealObjectFilterSet(
          'outliner.filters',
          defaultValue: {},
        );

  /// Whether to sort actors in descending order.
  final Preference<bool> bShouldSortActorsDescending;

  /// The metric by which to sort the actors.
  final Preference<OutlinerActorSortMode> actorSortMode;

  /// Which actor filters are selected.
  final Preference<Set<UnrealObjectFilter>> selectedFilters;
}
