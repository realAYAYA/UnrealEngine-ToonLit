// Copyright Epic Games, Inc. All Rights Reserved.

import 'dart:convert';

import 'package:logging/logging.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:streaming_shared_preferences/streaming_shared_preferences.dart';

import '../models/settings/recent_actor_settings.dart';
import '../models/unreal_object_filters.dart';
import 'json_utilities.dart';

final _log = Logger('PrefExtensions');

/// Extensions to serialize/deserialize new types of preference values.
extension PreferenceAdapterExtensions on StreamingSharedPreferences {
  /// Starts with the current String set value for the given [key], then emits a new value every time there are changes
  /// to the value associated with [key].
  ///
  /// If the value is null, starts with the value provided in [defaultValue]. When the value transitions from non-null
  /// to null (ie. when the value is removed), emits [defaultValue].
  Preference<Set<String>> getStringSet(
    String key, {
    required Set<String> defaultValue,
  }) {
    return getCustomValue(key, defaultValue: defaultValue, adapter: StringSetAdapter.instance);
  }

  /// Starts with the current Enum value for the given [key], then emits a new value every time there are changes to the
  /// value associated with [key].
  ///
  /// If the value is null, starts with the value provided in [defaultValue]. When the value transitions from non-null
  /// to null (ie. when the value is removed), emits [defaultValue].
  Preference<T> getEnum<T extends Enum>(
    String key, {
    required T defaultValue,
    required List<T> enumValues,
  }) {
    return getCustomValue(key, defaultValue: defaultValue, adapter: EnumAdapter(enumValues));
  }

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

/// A [PreferenceAdapter] implementation for storing and retrieving a [Set] of [String] objects.
class StringSetAdapter extends PreferenceAdapter<Set<String>> {
  static const instance = StringSetAdapter._();
  const StringSetAdapter._();

  @override
  Set<String>? getValue(preferences, key) => preferences.getStringList(key)?.toSet();

  @override
  Future<bool> setValue(preferences, key, values) => preferences.setStringList(key, values.toList(growable: false));
}

/// A [PreferenceAdapter] implementation for storing and retrieving an [Enum] value as a string.
class EnumAdapter<T extends Enum> extends PreferenceAdapter<T> {
  const EnumAdapter(this.enumValues);

  final List<T> enumValues;

  @override
  T? getValue(preferences, key) {
    try {
      final String? name = preferences.getString(key);

      if (name == null) {
        return null;
      }

      return jsonToEnumValue(name, enumValues);
    } catch (_) {
      return null;
    }
  }

  @override
  Future<bool> setValue(preferences, key, value) => preferences.setString(key, enumToJsonValue(value));
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
