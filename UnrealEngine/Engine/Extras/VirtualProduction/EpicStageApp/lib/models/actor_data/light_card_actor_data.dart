// Copyright Epic Games, Inc. All Rights Reserved.

import '../engine_connection.dart';
import '../unreal_property_manager.dart';
import '../unreal_types.dart';
import 'per_class_actor_data.dart';

/// Retrieves [LightCardActorData] for an actor from the engine.
class LightCardActorData extends UnrealPerClassActorData {
  LightCardActorData(super.params);

  late final TrackedPropertyId _isUVPropertyId;
  late final TrackedPropertyId _isFlagPropertyId;

  /// If this is set, the UV property can't be tracked and we should use this value instead.
  bool? _bIsUVFallback;

  /// Whether this is light card is positioned in UV space.
  bool get bIsUV => _bIsUVFallback ?? propertyManager.getTrackedPropertyValue(_isUVPropertyId) ?? false;

  /// Whether this light card is a flag.
  bool get bIsFlag => propertyManager.getTrackedPropertyValue(_isFlagPropertyId) ?? false;

  @override
  Future<void> initialize() async {
    // Old API version can't subscribe to these properties
    if (!connectionManager.apiVersion!.bSupportsLightcardSubtypes) {
      _isUVPropertyId = TrackedPropertyId.invalid();
      _isFlagPropertyId = TrackedPropertyId.invalid();

      // This property can't be subscribed to on this API version, but we can at least get its value directly and assume
      // that it won't change
      final UnrealHttpResponse response = await connectionManager.sendHttpRequest(UnrealHttpRequest(
        url: '/remote/object/property',
        verb: 'PUT',
        body: {
          'objectPath': actor.path,
          'propertyName': 'bIsUVLightCard',
          'access': 'READ_ACCESS',
        },
      ));

      _bIsUVFallback = response.body?['bIsUVLightCard'];

      return Future.value();
    }

    _isUVPropertyId = propertyManager.trackProperty(
        UnrealProperty(objectPath: actor.path, propertyName: 'bIsUVLightCard'), onPropertyChanged);

    _isFlagPropertyId = propertyManager.trackProperty(
        UnrealProperty(objectPath: actor.path, propertyName: 'bIsLightCardFlag'), onPropertyChanged);

    await Future.wait([
      propertyManager.waitForProperty(_isUVPropertyId),
      propertyManager.waitForProperty(_isFlagPropertyId),
    ]);
  }

  @override
  void dispose() {
    propertyManager.stopTrackingProperty(_isUVPropertyId, onPropertyChanged);
    propertyManager.stopTrackingProperty(_isFlagPropertyId, onPropertyChanged);

    super.dispose();
  }
}
