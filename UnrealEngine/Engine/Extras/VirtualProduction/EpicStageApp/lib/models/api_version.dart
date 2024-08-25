// Copyright Epic Games, Inc. All Rights Reserved.

/// A semantic version indicating which engine API is available to the app.
class EpicStageAppAPIVersion implements Comparable<EpicStageAppAPIVersion> {
  const EpicStageAppAPIVersion(this.major, this.minor, this.patch);

  factory EpicStageAppAPIVersion.fromString(String string) {
    final List<String> components = string.split('.');
    if (components.length != 3) {
      throw ArgumentError('Expected 3 components in version number but got ${components.length}');
    }

    return EpicStageAppAPIVersion(
      int.parse(components[0]),
      int.parse(components[1]),
      int.parse(components[2]),
    );
  }

  final int major;
  final int minor;
  final int patch;

  @override
  int compareTo(EpicStageAppAPIVersion other) {
    int comparison = major.compareTo(other.major);
    if (comparison != 0) {
      return comparison;
    }

    comparison = minor.compareTo(other.minor);
    if (comparison != 0) {
      return comparison;
    }

    return patch.compareTo(other.patch);
  }

  @override
  bool operator ==(Object other) {
    if (!(other is EpicStageAppAPIVersion)) {
      return false;
    }

    return compareTo(other) == 0;
  }

  @override
  int get hashCode => Object.hash(major, minor, patch);

  bool operator >(EpicStageAppAPIVersion other) => compareTo(other) > 0;
  bool operator >=(EpicStageAppAPIVersion other) => compareTo(other) >= 0;
  bool operator <(EpicStageAppAPIVersion other) => compareTo(other) <= 0;
  bool operator <=(EpicStageAppAPIVersion other) => compareTo(other) <= 0;

  @override
  String toString() {
    return '$major.$minor.$patch';
  }

  /// Whether the engine API has a dedicated GetRemoteControlWebInterfacePort function.
  bool get bHasWebInterfacePortFunction => this >= const EpicStageAppAPIVersion(1, 1, 0);

  /// Whether the engine API has full support for the bIsLightCardFlag and bIsUVLightCard properties;
  bool get bSupportsLightcardSubtypes => this >= const EpicStageAppAPIVersion(1, 3, 0);

  /// Whether the engine API supports append/insert/remove for array properties.
  bool get bSupportsArrayPropertyOperations => this >= const EpicStageAppAPIVersion(1, 4, 0);

  /// Whether the engine API supports resetting struct properties contained within lists.
  bool get bCanResetListItems => this >= const EpicStageAppAPIVersion(1, 5, 0);

  /// Whether the engine API correctly handles HTTP property set calls as part of a manual transaction.
  bool get bCanHttpSetPropertyInManualTransaction => this >= const EpicStageAppAPIVersion(1, 6, 0);

  /// Whether the engine API allows for query parameters in URLs passed to the HTTP WebSocket route.
  bool get bCanUseQueryParamsInWebSocketHttpUrl => this >= const EpicStageAppAPIVersion(1, 7, 0);

  /// Whether the initial actor position passed to the ndisplay.preview.actor.create route is applied correctly.
  bool get bIsNewActorOverridePositionAccurate => this >= const EpicStageAppAPIVersion(1, 8, 0);

  /// Whether compression of WebSocket traffic can be enabled in the engine.
  bool get bIsWebSocketCompressionAvailable => this >= const EpicStageAppAPIVersion(1, 9, 0);
}
