// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:epic_common/preferences.dart';
import 'package:epic_common/utilities/json.dart';
import 'package:streaming_shared_preferences/streaming_shared_preferences.dart';

import '../../utilities/streaming_preferences_extensions.dart';

enum RecentlyPlacedActorType {
  lightCard,
  chromakeyCard,
  flag,
  colorCorrectWindow,
}

/// Data about an actor the user recently placed.
class RecentlyPlacedActorData {
  const RecentlyPlacedActorData({this.name, required this.type, this.templatePath});

  RecentlyPlacedActorData.fromJson(Map<String, dynamic> json)
      : name = json['Name'],
        type = jsonToEnumValue(json['Type'], RecentlyPlacedActorType.values) ?? RecentlyPlacedActorType.lightCard,
        templatePath = json['TemplatePath'];

  /// The name to show for the actor. If null, use the default name for the [type].
  final String? name;

  /// The type of actor to create.
  final RecentlyPlacedActorType type;

  /// The path to the template used to create the actor.
  final String? templatePath;

  @override
  bool operator ==(Object other) {
    if (identical(this, other)) {
      return true;
    }

    if (other.runtimeType != runtimeType) {
      return false;
    }

    return other is RecentlyPlacedActorData && other.type == type && other.templatePath == templatePath;
  }

  Map<String, dynamic> toJson() {
    return {
      'Name': name,
      'Type': enumToJsonValue(type),
      'TemplatePath': templatePath,
    };
  }
}

/// Holds settings about actors that the user recently placed.
class RecentActorSettings {
  RecentActorSettings(PreferencesBundle preferences)
      : recentlyPlacedActors = preferences.persistent.getRecentlyPlacedActorList(
          'common.recentlyPlacedActors',
          defaultValue: [],
        );

  /// The last time the actor with the given path was selected.
  final Preference<List<RecentlyPlacedActorData>> recentlyPlacedActors;

  /// Add a recently placed actor (or move it to the top of the list if it's already in the list).
  void addRecentlyPlacedActor(RecentlyPlacedActorData actorData) {
    const int historyLength = 5;

    final List<RecentlyPlacedActorData> actorList = recentlyPlacedActors.getValue();

    actorList.remove(actorData);
    actorList.insert(0, actorData);

    while (actorList.length > historyLength) {
      actorList.removeLast();
    }

    recentlyPlacedActors.setValue(actorList);
  }
}
