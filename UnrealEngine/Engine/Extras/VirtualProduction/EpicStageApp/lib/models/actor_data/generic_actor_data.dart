// Copyright Epic Games, Inc. All Rights Reserved.

import '../property_modify_operations.dart';
import '../unreal_property_manager.dart';
import '../unreal_types.dart';
import 'per_class_actor_data.dart';

/// Retrieves generically useful data for actor in Unreal Engine.
class GenericActorData extends UnrealPerClassActorData {
  GenericActorData(super.params);

  late final TrackedPropertyId _isHiddenInGamePropertyId;

  /// Whether this actor is hidden in-game.
  bool get bIsHiddenInGame => propertyManager.getTrackedPropertyValue(_isHiddenInGamePropertyId) ?? false;

  set bIsHiddenInGame(bool bNewValue) {
    if (!bIsDataReady) {
      return;
    }

    if (propertyManager.beginTransaction('Set Hidden In Game')) {
      propertyManager.modifyTrackedPropertyValue(
        _isHiddenInGamePropertyId,
        const SetOperation(),
        deltaValue: bNewValue,
      );
      propertyManager.endTransaction();
    }
  }

  @override
  Future<void> initialize() async {
    _isHiddenInGamePropertyId = propertyManager.trackProperty(
      UnrealProperty(objectPath: actor.path, propertyName: 'bHidden'),
      onPropertyChanged,
    );

    await propertyManager.waitForProperty(_isHiddenInGamePropertyId);
  }

  @override
  void dispose() {
    propertyManager.stopTrackingProperty(_isHiddenInGamePropertyId, onPropertyChanged);

    super.dispose();
  }
}
