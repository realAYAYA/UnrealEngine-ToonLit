// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:flutter/material.dart';

import '../engine_connection.dart';
import '../unreal_property_manager.dart';
import '../unreal_types.dart';

/// Parameters for creating an [UnrealPerClassActorData].
class UnrealPerClassActorDataParams {
  const UnrealPerClassActorDataParams({
    required this.actor,
    required this.connectionManager,
    required this.propertyManager,
  });

  /// The actor to which this data corresponds.
  final UnrealObject actor;

  /// The manager responsible for the engine connection.
  final EngineConnectionManager connectionManager;

  /// The property manager used to retrieve the data.
  final UnrealPropertyManager propertyManager;
}

/// Base class for extra data to retrieve about an actor from Unreal Engine based on the actor's class.
abstract class UnrealPerClassActorData extends ChangeNotifier {
  UnrealPerClassActorData(UnrealPerClassActorDataParams params)
      : _actor = params.actor,
        _propertyManager = params.propertyManager,
        _connectionManager = params.connectionManager {
    initialize().then((_) {
      _bIsDataReady = true;
      notifyListeners();
    });
  }

  /// The actor to which this data corresponds.
  final UnrealObject _actor;

  /// The property manager used to retrieve the data.
  final UnrealPropertyManager _propertyManager;

  /// The manager responsible for the engine connection.
  final EngineConnectionManager _connectionManager;

  /// Whether this fully retrieved its initial data from the engine.
  bool _bIsDataReady = false;

  /// Whether this fully retrieved its initial data from the engine.
  bool get bIsDataReady => _bIsDataReady;

  /// Async function to retrieve any data from the engine.
  @protected
  Future<void> initialize();

  /// The property manager used to retrieve the data.
  @protected
  UnrealPropertyManager get propertyManager => _propertyManager;

  /// The connection manager used to retrieve the data.
  @protected
  EngineConnectionManager get connectionManager => _connectionManager;

  /// The actor to which this data corresponds.
  @protected
  UnrealObject get actor => _actor;

  /// Convenience function which accepts a property argument from callbacks for UnrealPropertyManager.trackProperty.
  void onPropertyChanged(_) {
    notifyListeners();
  }
}
