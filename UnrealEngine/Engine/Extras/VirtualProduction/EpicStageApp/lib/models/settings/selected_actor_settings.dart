// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:epic_common/preferences.dart';
import 'package:streaming_shared_preferences/streaming_shared_preferences.dart';

/// Holds user settings that impact the state of the selected actors.
class SelectedActorSettings {
  SelectedActorSettings(PreferencesBundle preferences)
      : displayClusterRootPath = preferences.persistent.getString('common.displayClusterRootPath', defaultValue: ''),
        selectedActors = preferences.transient.get<Set<String>>(
          'common.selectedActors',
          defaultValue: {},
        ),
        lastSelectedTimeForActors = preferences.transient.get<Map<String, DateTime>>(
          'common.lastSelectedTimeForActors',
          defaultValue: {},
        ),
        bIsInMultiSelectMode = preferences.persistent.getBool(
          'common.bIsInMultiSelectMode',
          defaultValue: false,
        ) {
    bIsInMultiSelectMode.listen(_onIsInMultiSelectModeChanged);
    displayClusterRootPath.listen(_onDisplayClusterRootPathChanged);
  }

  /// One-off serialization adapter for a [Map<String, DateTime>].
  static PreferenceAdapter<Map<String, DateTime>> stringDateTimeMapAdapter = JsonAdapter(
    serializer: (Map<String, DateTime> map) => map.map(
      (String key, DateTime value) => MapEntry(key, value.millisecondsSinceEpoch.toString()),
    ),
    deserializer: (jsonData) {
      if (!(jsonData is Map<String, String>)) {
        return {};
      }

      return jsonData.map(
        (key, value) => MapEntry(key, DateTime.fromMillisecondsSinceEpoch(int.parse(value), isUtc: true)),
      );
    },
  );

  /// The path of the selected nDisplay cluster.
  final Preference<String> displayClusterRootPath;

  /// Paths of actors that the user has selected to multi-edit.
  final TransientPreference<Set<String>> selectedActors;

  /// The last time the actor with the given path was selected.
  final TransientPreference<Map<String, DateTime>> lastSelectedTimeForActors;

  /// If true, the user can select multiple light cards at once. Otherwise, only the most recently selected light card
  /// will be selected.
  final Preference<bool> bIsInMultiSelectMode;

  /// Convenience function to check if an actor with the given [path] is selected.
  bool isActorSelected(String path) {
    return selectedActors.getValue().contains(path);
  }

  /// Select or deselect an actor for multi-edit.
  void selectActor(String path, {bool bShouldSelect = true}) {
    final Set<String> selectedActorSet = selectedActors.getValue().toSet();
    bool bChanged = false;

    if (bShouldSelect) {
      if (!bIsInMultiSelectMode.getValue()) {
        selectedActorSet.clear();
        bChanged = true;
      }

      if (selectedActorSet.add(path)) {
        bChanged = true;
      }
    } else if (selectedActorSet.remove(path)) {
      // The actor was deselected, so remember that it was selected until just now
      final newLastSelectedTimeForActors = lastSelectedTimeForActors.getValue();
      newLastSelectedTimeForActors[path] = DateTime.now();
      lastSelectedTimeForActors.setValue(newLastSelectedTimeForActors);

      bChanged = true;
    }

    if (bChanged) {
      selectedActors.setValue(selectedActorSet);
    }
  }

  /// Deselect all actors that are currently selected.
  void deselectAllActors() {
    selectedActors.setValue({});
  }

  /// Get the last time an actor was still selected.
  /// If the actor was selected but no longer is, return the time at which it became deselected.
  /// If the actor is currently selected, return the current time.
  /// If the actor was never selected, return null.
  DateTime? getActorLastSelectedTime(String path) {
    if (isActorSelected(path)) {
      return DateTime.now();
    }

    return lastSelectedTimeForActors.getValue()[path];
  }

  /// Called when the value of [bIsInMultiSelectMode] changes.
  void _onIsInMultiSelectModeChanged(bool bNewIsInMultiSelectMode) {
    if (bNewIsInMultiSelectMode) {
      return;
    }

    final selectedActorSet = selectedActors.getValue();
    if (selectedActorSet.length > 0) {
      // Reduce to one actor. Set is ordered, so this should give us the last selected actor.
      selectedActors.setValue({selectedActorSet.last});
    }
  }

  /// Called when the value of [displayClusterRootPath] changes.
  void _onDisplayClusterRootPathChanged(_) {
    deselectAllActors();
  }
}
