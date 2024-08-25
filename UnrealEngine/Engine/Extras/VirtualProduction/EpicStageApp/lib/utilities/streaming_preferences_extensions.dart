// Copyright Epic Games, Inc. All Rights Reserved.

import 'dart:convert';

import 'package:logging/logging.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:streaming_shared_preferences/streaming_shared_preferences.dart';

import '../models/settings/recent_actor_settings.dart';
import '../models/unreal_object_filters.dart';

final _log = Logger('PrefExtensions');

/// Extensions to serialize/deserialize new types of preference values.
extension PreferenceAdapterExtensions on StreamingSharedPreferences {
  /// Starts with the current list value for the given [key], then emits a new value every time there are changes to the
  /// value associated with [key].
  ///
  /// If the value is null, starts with the value provided in [defaultValue]. When the value transitions from non-null
  /// to null (ie. when the value is removed), emits [defaultValue].
  Preference<List<RecentlyPlacedActorData>> getRecentlyPlacedActorList(
    String key, {
    required List<RecentlyPlacedActorData> defaultValue,
  }) {
    return getCustomValue(key, defaultValue: defaultValue, adapter: RecentlyPlacedActorListAdapter.instance);
  }

  /// Starts with the current Set value for the given [key], then emits a new value every time there are changes to the
  /// value associated with [key].
  ///
  /// If the value is null, starts with the value provided in [defaultValue]. When the value transitions from non-null
  /// to null (ie. when the value is removed), emits [defaultValue].
  Preference<Set<UnrealObjectFilter>> getUnrealObjectFilterSet(
    String key, {
    required Set<UnrealObjectFilter> defaultValue,
  }) {
    return getCustomValue(key, defaultValue: defaultValue, adapter: UnrealObjectFilterListAdapter.instance);
  }
}

/// A [PreferenceAdapter] implementation for storing and retrieving a [List] of [RecentlyPlacedActorData] values.
class RecentlyPlacedActorListAdapter extends PreferenceAdapter<List<RecentlyPlacedActorData>> {
  static const instance = RecentlyPlacedActorListAdapter._();
  const RecentlyPlacedActorListAdapter._();

  @override
  List<RecentlyPlacedActorData>? getValue(SharedPreferences preferences, String key) {
    try {
      return preferences
          .getStringList(key)
          ?.map((jsonData) => RecentlyPlacedActorData.fromJson(json.decode(jsonData)))
          .toList();
    } catch (e) {
      _log.warning('Failed to deserialize RecentlyPlacedActorData list: $e');
      return null;
    }
  }

  @override
  Future<bool> setValue(SharedPreferences preferences, String key, List<RecentlyPlacedActorData> value) {
    final encodedData = value.map((data) => json.encode(data.toJson())).toList(growable: false);
    return preferences.setStringList(key, encodedData);
  }
}

/// A [PreferenceAdapter] implementation for storing and retrieving a [Set] of [UnrealObjectFilter] values.
class UnrealObjectFilterListAdapter extends PreferenceAdapter<Set<UnrealObjectFilter>> {
  static const instance = UnrealObjectFilterListAdapter._();
  const UnrealObjectFilterListAdapter._();

  @override
  Set<UnrealObjectFilter>? getValue(SharedPreferences preferences, String key) {
    try {
      return preferences
          .getStringList(key)
          ?.map((filterName) => UnrealObjectFilterRegistry.getFilterByName(filterName))
          // Remove any filters that no longer exist
          .where((element) => element != null)
          .map((element) => element!)
          .toSet();
    } catch (e) {
      _log.warning('Failed to deserialize RecentlyPlacedActorData list: $e');
      return null;
    }
  }

  @override
  Future<bool> setValue(SharedPreferences preferences, String key, Set<UnrealObjectFilter> value) {
    final nameList = value.map((filter) => filter.internalName).toList(growable: false);
    return preferences.setStringList(key, nameList);
  }
}
