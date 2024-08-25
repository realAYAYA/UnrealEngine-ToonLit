// Copyright Epic Games, Inc. All Rights Reserved.

import 'dart:async';
import 'dart:collection';

import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:logging/logging.dart';
import 'package:provider/provider.dart';

import '../utilities/unreal_utilities.dart';
import 'actor_data/generic_actor_data.dart';
import 'actor_data/light_card_actor_data.dart';
import 'actor_data/per_class_actor_data.dart';
import 'engine_connection.dart';
import 'settings/selected_actor_settings.dart';
import 'unreal_class_data.dart';
import 'unreal_property_manager.dart';
import 'unreal_types.dart';

final _log = Logger('UnrealActorManager');

/// Callback for when the list of actors of Unreal type [className] changes.
typedef ActorUpdateCallback = Function(ActorUpdateDetails details);

/// Details object for callbacks that use [ActorUpdateCallback].
class ActorUpdateDetails {
  const ActorUpdateDetails({
    required this.className,
    required this.addedActors,
    required this.renamedActors,
    required this.deletedActors,
    this.bIsDueToDisconnect = false,
  });

  /// The name of class for which we received an update.
  final String className;

  /// Actors of this class that were added.
  final List<UnrealObject> addedActors;

  /// Actors of this class that were renamed.
  final List<UnrealObject> renamedActors;

  /// Actors of this class that were deleted.
  final List<UnrealObject> deletedActors;

  /// If true, this update is because the app has been disconnected from the engine.
  final bool bIsDueToDisconnect;
}

/// Function that takes per-class actor data parameters and creates a data class.
typedef _ActorDataCreatorFunction<T extends UnrealPerClassActorData> = T Function(UnrealPerClassActorDataParams);

/// Stores an [_ActorDataCreatorFunction] and allows us to query the actor data type.
class _ActorDataCreator<T extends UnrealPerClassActorData> {
  const _ActorDataCreator(this._fn)
      : assert(T != UnrealPerClassActorData, '_ActorDataCreator must use a concrete actor data type');

  /// The function this will use to create a new data object.
  final _ActorDataCreatorFunction<T> _fn;

  /// The type of actor data object this will create.
  Type get dataType => T;

  /// Create a new actor data object of the appropriate type.
  T create(UnrealPerClassActorDataParams params) => _fn(params);
}

/// Maintains information about and provides event hooks for actors in the engine that the app cares about.
class UnrealActorManager with WidgetsBindingObserver {
  /// Construct an actor manager and register it with the connection manager.
  UnrealActorManager(BuildContext context)
      : _connectionManager = Provider.of<EngineConnectionManager>(context, listen: false),
        _propertyManager = Provider.of<UnrealPropertyManager>(context, listen: false),
        _selectedActorSettings = Provider.of<SelectedActorSettings>(context, listen: false) {
    _registerAllActorDataCreators();

    _connectionManager
      ..registerConnectionStateListener(_onConnectionStateChanged)
      ..registerMessageListener('ActorsChanged', _onActorsChanged);
    _onConnectionStateChanged(_connectionManager.connectionState);

    _selectedActorSettings.displayClusterRootPath.listen(_onRootActorChanged);

    WidgetsBinding.instance.addObserver(this);
  }

  /// The connection manager we'll use to communicate with the engine.
  final EngineConnectionManager _connectionManager;

  /// The property manager we'll use to retrieve additional actor data.
  final UnrealPropertyManager _propertyManager;

  /// Settings we need access to for actor-related operations.
  final SelectedActorSettings _selectedActorSettings;

  /// Map from watched class names to the callback functions to be called when they change.
  final Map<String, Set<ActorUpdateCallback>> _classWatchCallbacks = {};

  /// Callback functions for when any actor is updated.
  final Set<ActorUpdateCallback> _anyClassWatchCallbacks = {};

  /// Map from an actor's path in the editor to the data we've stored about it locally.
  final Map<String, UnrealObject> _actorsByPath = {};

  /// Map from watched class name to the list of actors that have been seen with that class name.
  final Map<String, Set<UnrealObject>> _actorsByClass = {};

  /// Map from watched class name to completers to complete when we receive the initial list of actor of that class
  /// from the engine.
  final Map<String, Completer<Set<UnrealObject>>> _firstUpdateCompleters = {};

  /// Map from class name to a function that creates an [UnrealPerClassActorData] for a given class name.
  final Map<UnrealClass, List<_ActorDataCreator>> _actorDataCreatorsByClass = {};

  /// Set of light cards that belong to the current DCRA.
  ValueNotifier<UnmodifiableSetView<UnrealObject>> _currentRootActorLightCards = ValueNotifier(UnmodifiableSetView({}));

  /// Set of light cards that belong to the current DCRA.
  ValueListenable<UnmodifiableSetView<UnrealObject>> get currentRootActorLightCards => _currentRootActorLightCards;

  /// Start watching for any engine-side changes to actors of the Unreal type [className], and call [callback] when
  /// there's any change in the list of relevant actors.
  void watchClassName(String className, ActorUpdateCallback callback) {
    if (_connectionManager.bIsInDemoMode) {
      return;
    }

    if (_classWatchCallbacks.containsKey(className)) {
      // We're already watching this class, so just add the callback
      _classWatchCallbacks[className]!.add(callback);
      return;
    }

    _classWatchCallbacks[className] = {callback};

    if (!_firstUpdateCompleters.containsKey(className)) {
      _firstUpdateCompleters[className] = Completer<Set<UnrealObject>>();
    }

    _connectionManager.sendRawMessage(_createActorRegisterMessage(className, true));
  }

  /// Stop watching for engine-side changes to actors of the Unreal type [className] with the given [callback].
  void stopWatchingClassName(String className, ActorUpdateCallback callback) {
    final Set<ActorUpdateCallback>? callbacks = _classWatchCallbacks[className];
    if (callbacks == null) {
      return;
    }

    callbacks.remove(callback);

    if (callbacks.isNotEmpty) {
      return;
    }

    // No more watchers of this class
    _classWatchCallbacks.remove(className);
    _connectionManager.sendRawMessage(_createActorRegisterMessage(className, false));

    // Forget about actors of this class
    final Set<UnrealObject> actorsToForget = getActorsOfClass(className);
    _actorsByClass.remove(className);

    if (actorsToForget.isEmpty) {
      return;
    }

    // Filter out any actors we're about to forget that are also part of another watched class
    for (final Set<UnrealObject> relevantActors in _actorsByClass.values) {
      actorsToForget.removeWhere((UnrealObject actor) => relevantActors.contains(actor));
    }

    for (final UnrealObject actor in actorsToForget) {
      actor.onUnwatched();
      _actorsByPath.remove(actor.path);
    }
  }

  /// Watch for updates to all actor classes which are already watched by previous or future calls to [watchClassName].
  /// Note that this will not subscribe to any new classes in the engine.
  void watchExistingSubscriptions(ActorUpdateCallback callback) {
    _anyClassWatchCallbacks.add(callback);
  }

  /// Stop watching for updates to all actor classes for a callback that was previously passed to
  /// [watchExistingSubscriptions].
  void stopWatchingExistingSubscriptions(ActorUpdateCallback callback) {
    _anyClassWatchCallbacks.remove(callback);
  }

  /// Check whether a [className] is currently being watched by at least one watcher.
  bool isClassWatched(String className) {
    final callbacks = _classWatchCallbacks[className];
    return callbacks != null && callbacks.isNotEmpty;
  }

  /// Get the watched [UnrealObject] corresponding to a path name.
  UnrealObject? getActorAtPath(String path) {
    return _actorsByPath[path];
  }

  /// Get the cached set of watched [UnrealObject]s corresponding to a [className]. Note that if the class is not
  /// currently watched, this will always return an empty set.
  Set<UnrealObject> getActorsOfClass(String className) {
    return _actorsByClass[className]?.toSet() ?? {};
  }

  /// Get the cached set of [UnrealObject]s corresponding to a [className], but wait until the initial list of actors
  /// has been received from the engine if it hasn't already.
  /// If [className] isn't watched, this will automatically watch the name for long enough to retrieve the list, then
  /// stop watching it.
  Future<Set<UnrealObject>> getInitialActorsOfClass(String className) {
    // If nobody is already watching this class, do so temporarily
    late final Function(ActorUpdateDetails) tempWatcherStub;
    final bool bNeedsTempWatch = !isClassWatched(className);

    if (bNeedsTempWatch) {
      tempWatcherStub = (_) {};
      watchClassName(className, tempWatcherStub);
    }

    late final Future<Set<UnrealObject>> result;

    final Completer<Set<UnrealObject>>? completer = _firstUpdateCompleters[className];

    if (completer == null) {
      // If there's no completer, we already have the initial list of actors
      result = Future.value(getActorsOfClass(className));
    } else {
      // Otherwise, wait until the initial list is received
      result = completer.future;
    }

    if (bNeedsTempWatch) {
      result.then((_) => stopWatchingClassName(className, tempWatcherStub));
    }

    return result;
  }

  @override
  void didChangeAppLifecycleState(AppLifecycleState state) {
    super.didChangeAppLifecycleState(state);

    switch (state) {
      case AppLifecycleState.resumed:
        _subscribeToEngineActors(true);
        break;

      case AppLifecycleState.paused:
      case AppLifecycleState.detached:
        _subscribeToEngineActors(false);
        break;

      default:
        break;
    }
  }

  /// Register [UnrealPerClassActorData]s for relevant class types.
  void _registerAllActorDataCreators() {
    _registerActorDataCreatorForClass(UnrealClassRegistry.lightCard, (params) => LightCardActorData(params));
  }

  /// Register a creator function for a class.
  /// We use a generic function for this instead of adding directly to the list because otherwise, Dart will infer T to
  /// be the base UnrealPerClassActorData class instead of the specific type. We need the specific type later so we can
  /// check if the actor already has the corresponding data attached.
  void _registerActorDataCreatorForClass<T extends UnrealPerClassActorData>(
    UnrealClass unrealClass,
    _ActorDataCreatorFunction<T> creatorFn,
  ) {
    List<_ActorDataCreator>? creatorList = _actorDataCreatorsByClass[unrealClass];
    if (creatorList == null) {
      creatorList = [];
      _actorDataCreatorsByClass[unrealClass] = creatorList;
    }

    creatorList.add(_ActorDataCreator<T>(creatorFn));
  }

  /// Subscribe to the WebSocket route that gives us updates about actors
  void _subscribeToEngineActors(bool bShouldRegister) {
    final List<dynamic> messages = [];

    // Re-register for any actor classes that we're still watching
    for (String className in _classWatchCallbacks.keys) {
      messages.add(_createActorRegisterMessage(className, true));
    }

    _connectionManager.sendBatchedMessage(messages);
  }

  /// Called when we lose connection to the engine.
  void _onEngineDisconnected() {
    final Set<UnrealObject> deletedActors = {};

    // Send destroy event for all actors since we will no longer know if they exist
    for (UnrealObject actor in _actorsByPath.values) {
      deletedActors.add(actor);
      actor.onDestroyed();
    }

    // Send destroy event for each class
    for (MapEntry<String, Set<UnrealObject>> classPair in _actorsByClass.entries) {
      final className = classPair.key;

      final Set<ActorUpdateCallback>? callbacks = _classWatchCallbacks[classPair.key];
      if (callbacks == null) {
        continue;
      }

      final updateDetails = ActorUpdateDetails(
        className: className,
        addedActors: [],
        renamedActors: [],
        deletedActors: classPair.value.toList(growable: false),
        bIsDueToDisconnect: true,
      );

      for (ActorUpdateCallback callback in callbacks) {
        callback(updateDetails);
      }

      for (ActorUpdateCallback callback in _anyClassWatchCallbacks) {
        callback(updateDetails);
      }
    }

    // Dispose actors now that nobody should be accessing them
    for (UnrealObject actor in deletedActors) {
      actor.dispose();
    }

    _actorsByClass.clear();
    _actorsByPath.clear();
    _currentRootActorLightCards.value = UnmodifiableSetView({});
  }

  /// Called when the connection manager's connection state changes.
  void _onConnectionStateChanged(EngineConnectionState connectionState) {
    switch (connectionState) {
      case EngineConnectionState.connected:
        _subscribeToEngineActors(true);
        break;

      case EngineConnectionState.disconnected:
        _onEngineDisconnected();
        break;

      default:
        break;
    }
  }

  /// Called when the actors in the engine have changed.
  void _onActorsChanged(dynamic message) {
    var changes = message['Changes'];
    if (changes == null) {
      return;
    }

    changes.forEach((String className, dynamic classChanges) {
      if (_classWatchCallbacks.keys.contains(className)) {
        _updateActorsFromEngine(className, classChanges);
      }

      final Completer<Set<UnrealObject>>? completer = _firstUpdateCompleters[className];
      if (completer != null) {
        completer.complete(getActorsOfClass(className));
        _firstUpdateCompleters.remove(className);
      }
    });

    _updateRootActorLightCards();
  }

  /// Create a message to register/unregister a [className] for updates from the engine.
  /// If [bShouldRegister] is false, unregister for the class name.
  dynamic _createActorRegisterMessage(String className, bool bShouldRegister) {
    return createUnrealWebSocketMessage(
      bShouldRegister ? 'actors.register' : 'actors.unregister',
      {'ClassName': className},
    );
  }

  /// Update our actor data about actors of class [className] based on change events from the engine.
  void _updateActorsFromEngine(String className, dynamic classChanges) {
    // Remove any actors that no longer exist. Do this first in case the actor was also re-added this frame
    final List? deletedActorPaths = classChanges['DeletedActors'];
    final List<UnrealObject> deletedActors = [];

    if (deletedActorPaths != null) {
      for (var actorData in deletedActorPaths) {
        final String? path = actorData['Path'];
        if (path == null) {
          continue;
        }

        final UnrealObject? actor = _actorsByPath[path];
        if (actor == null) {
          continue;
        }

        deletedActors.add(actor);
        actor.onDestroyed();
        _actorsByPath.remove(path);

        for (final Set<UnrealObject> classActors in _actorsByClass.values) {
          classActors.remove(actor);
        }
      }
    }

    // Add any new actors
    final List? addedActorPaths = classChanges['AddedActors'];
    final List<UnrealObject> addedActors = [];

    if (addedActorPaths != null) {
      for (var actorData in addedActorPaths) {
        final String? path = actorData['Path'];
        final String? name = actorData['Name'];

        if (path == null || name == null) {
          continue;
        }

        // Create a new entry if this is a new actor
        UnrealObject? actor = _actorsByPath[path];
        if (actor == null) {
          actor = UnrealObject(name: name, path: path);
          _actorsByPath[path] = actor;
        } else {
          // Update the actor's name if a new one has been created at the same path
          actor.name = name;
        }

        // Add this actor to the class list. An actor may have multiple watched classes, so we want to do this
        // regardless of whether the actor is actually new.
        Set<UnrealObject>? classActors = _actorsByClass[className];
        if (classActors == null) {
          classActors = {};
          _actorsByClass[className] = classActors;
        }

        if (!actor.classNames.contains(className)) {
          actor.addClassName(className);

          // Add additional data depending on the actor's class
          final actorDataParams = UnrealPerClassActorDataParams(
            actor: actor,
            propertyManager: _propertyManager,
            connectionManager: _connectionManager,
          );

          // All actors should have generic data
          if (actor.getPerClassData<GenericActorData>() == null) {
            actor.addPerClassData(GenericActorData(actorDataParams));
          }

          // Add any specific class data that wasn't already present
          UnrealClass? unrealClass = actor.unrealClass;

          while (unrealClass != null) {
            final List<_ActorDataCreator>? actorDataCreators = _actorDataCreatorsByClass[unrealClass];
            if (actorDataCreators != null) {
              for (final _ActorDataCreator creator in actorDataCreators) {
                if (actor.getPerClassDataOfType(creator.dataType) != null) {
                  continue;
                }

                actor.addPerClassData(creator.create(actorDataParams));
              }
            }

            unrealClass = unrealClass.parent;
          }
        }

        classActors.add(actor);
        addedActors.add(actor);
      }
    }

    // Rename existing actors
    final List renamedActorPaths = classChanges['RenamedActors'] ?? [];
    final List<UnrealObject> renamedActors = [];

    for (var actorData in renamedActorPaths) {
      final String? path = actorData['Path'];
      final String? name = actorData['Name'];

      if (path == null || name == null) {
        continue;
      }

      final UnrealObject? actor = _actorsByPath[path];
      if (actor == null) {
        continue;
      }

      actor.name = name;
      renamedActors.add(actor);
    }

    final updateDetails = ActorUpdateDetails(
      className: className,
      addedActors: addedActors,
      renamedActors: renamedActors,
      deletedActors: deletedActors,
    );

    // Call callbacks for watchers of this class
    final Set<ActorUpdateCallback>? callbacks = _classWatchCallbacks[className];
    if (callbacks != null) {
      for (final ActorUpdateCallback callback in callbacks) {
        callback(updateDetails);
      }
    }

    // Call callbacks for watchers of any class
    for (ActorUpdateCallback callback in _anyClassWatchCallbacks) {
      callback(updateDetails);
    }

    // Dispose of actors that should no longer be referenced
    for (UnrealObject actor in deletedActors) {
      actor.dispose();
    }
  }

  /// Send a request to update the list of light cards for the current DCRA.
  void _updateRootActorLightCards() async {
    if (_connectionManager.connectionState != EngineConnectionState.connected) {
      return;
    }

    final String rootPath = _selectedActorSettings.displayClusterRootPath.getValue();
    if (rootPath.isEmpty) {
      return;
    }

    final UnrealHttpResponse response = await _connectionManager.sendHttpRequest(UnrealHttpRequest(
      url: '/remote/object/call',
      verb: 'PUT',
      body: {
        'objectPath': '/Script/DisplayCluster.Default__DisplayClusterBlueprintLib',
        'functionName': 'FindLightCardsForRootActor',
        'parameters': {
          'rootActor': rootPath,
        },
      },
    ));

    _currentRootActorLightCards.value = UnmodifiableSetView({});

    if (response.code != 200) {
      _log.warning('Tried to get light cards for root actor ("$rootPath"), but got a ${response.code} response');
      return;
    }

    final List? lightCardPaths = response.body['OutLightCards'];
    if (lightCardPaths == null) {
      _log.warning('Tried to get light cards for root actor ("$rootPath"), but body contained no OutLightCards');
      return;
    }

    final Set<UnrealObject> lightCards = {};
    for (final String path in lightCardPaths) {
      final UnrealObject? lightCard = getActorAtPath(path);
      if (lightCard == null) {
        continue;
      }

      lightCards.add(lightCard);
    }

    _currentRootActorLightCards.value = UnmodifiableSetView(lightCards);
  }

  /// Called when the selected DCRA changes.
  void _onRootActorChanged(_) {
    // Clear the set first so we immediately hide irrelevant actors
    _currentRootActorLightCards.value = UnmodifiableSetView({});
    _updateRootActorLightCards();
  }
}
