// Copyright Epic Games, Inc. All Rights Reserved.

import 'dart:async';
import 'dart:convert';
import 'dart:math' as math;
import 'dart:typed_data';

import 'package:flutter/material.dart';
import 'package:logging/logging.dart';
import 'package:provider/provider.dart';
import 'package:vector_math/vector_math_64.dart' as vec;

import '../utilities/constants.dart';
import '../utilities/net_utilities.dart';
import '../utilities/unreal_utilities.dart';
import 'engine_connection.dart';
import 'settings/selected_actor_settings.dart';
import 'settings/stage_map_settings.dart';
import 'unreal_actor_manager.dart';
import 'unreal_types.dart';

final _log = Logger('PreviewRender');

/// Maintains information about and provides event hooks for actors in the engine that the app cares about.
class PreviewRenderManager with WidgetsBindingObserver {
  /// Quality of the JPEG to request (in range 50-100).
  static const int _jpegQuality = 50;

  /// How long to wait after receiving a preview render before requesting a new one, as a factor of the response time
  /// for the last render.
  static const double _previewRequestDelayFactor = 0.5;

  /// The maximum number of seconds to wait before requesting a new preview render, regardless of how long the previous
  /// request took.
  static const double _maxPreviewRequestDelay = 1.0;

  /// The maximum time to wait for the engine to respond with a render before sending a new request, in case the
  /// previous request was lost.
  static const Duration _previewRequestTimeout = Duration(seconds: 3);

  /// The maximum consecutive number of timeouts before we give up and create a new preview renderer.
  static const int _maxPreviewTimeoutsBeforeGiveUp = 3;

  /// Rate at which we send light card/camera config updates back to the engine.
  static const Duration _engineUpdateRate = Duration(milliseconds: 100);

  /// The current consumers for preview renders, if any.
  Set<PreviewRenderConsumer> _consumers = {};

  /// The last root actor path that we first sent to the engine, stored so we can check if we need to update it once the
  /// engine replies with a renderer ID.
  String? _initialRootPath;

  /// The ID of our preview renderer.
  int? _previewRendererId;

  /// The last time we sent a request to render a new preview of the scene.
  DateTime _lastRenderRequestTime = DateTime.now();

  /// Timer used to regularly send changes back to the engine while connected.
  Timer? _engineUpdateTimer;

  /// Whether we've sent a request to create the preview renderer.
  bool _bHasCreatedRenderer = false;

  /// Whether an actor drag operation is in progress.
  bool _bIsDraggingActors = false;

  /// The size of preview to request from the engine. The engine may provide a lower resolution with the same aspect
  /// ratio.
  Size _desiredResolution = const Size(1920, 1080);

  /// Helper currently attempting to initialize a preview renderer.
  MessageTimeoutRetryHelper? _initializeRendererHelper;

  /// Helper currently attempting to request a preview render.
  MessageTimeoutRetryHelper? _requestRenderHelper;

  /// The last sequence number we sent the engine for a drag update.
  int _dragSequenceNumber = 0;

  /// Stream subscriptions to user settings.
  final List<StreamSubscription> _settingSubscriptions = [];

  /// The connection manager we'll use to communicate with the engine.
  final EngineConnectionManager _connectionManager;

  /// Actor manager used to watch for root actor changes.
  final UnrealActorManager _actorManager;

  /// Settings for which actors are selected.
  final SelectedActorSettings _selectedActorSettings;

  /// The stage map settings, which include camera controls.
  final StageMapSettings _stageMapSettings;

  /// Modified renderer settings to send to the engine on the next tick. This doesn't have to include all settings; just
  /// the ones that we want to change.
  final Map<String, dynamic> _tickRendererSettingsChanges = {};

  /// The ID of the preview renderer, or null if it hasn't been created.
  int? get rendererId => _previewRendererId;

  /// The focal point stored in the stage map settings and adjusted for the current renderer's aspect ratio.
  Offset get rendererFocalPoint {
    final Offset focalPoint = _stageMapSettings.focalPoint.getValue();

    return Offset(
      0.5 + (focalPoint.dx - 0.5) / _desiredResolution.aspectRatio,
      focalPoint.dy,
    );
  }

  /// Whether an actor drag operation is in progress.
  bool get bIsDraggingActors => _bIsDraggingActors;

  /// Construct the manager and register it with the connection manager.
  PreviewRenderManager(BuildContext context)
      : _connectionManager = Provider.of<EngineConnectionManager>(context, listen: false),
        _stageMapSettings = Provider.of<StageMapSettings>(context, listen: false),
        _selectedActorSettings = Provider.of<SelectedActorSettings>(context, listen: false),
        _actorManager = Provider.of<UnrealActorManager>(context, listen: false) {
    _startListeningToEngineConnection();

    _onConnectionStateChanged(_connectionManager.connectionState);

    _actorManager.watchClassName(nDisplayRootActorClassName, _onRootActorUpdate);

    _settingSubscriptions.addAll([
      _selectedActorSettings.displayClusterRootPath.listen(_onRootActorPathChanged),
      _stageMapSettings.projectionMode.listen(_onProjectionModeChanged),
      _stageMapSettings.cameraAngle.listen(_onCameraAngleChanged),
    ]);

    WidgetsBinding.instance.addObserver(this);
  }

  /// Register a consumer with the manager, indicating that it should receive render updates.
  void addConsumer(PreviewRenderConsumer newConsumer) {
    _consumers.add(newConsumer);

    // Request a preview render if we weren't already waiting for one
    if (_requestRenderHelper?.bIsActive != true) {
      _log.info('Activated preview renderer and started new preview render request');
      _requestPreviewRender();
    }
  }

  /// Unregister a consumer with the manager.
  void removeConsumer(PreviewRenderConsumer consumer) {
    _consumers.remove(consumer);
  }

  /// Change the desired preview resolution.
  /// Returns whether the new resolution was valid. If not, the resolution won't be changed.
  bool setDesiredResolution(Size resolution) {
    if (resolution.width <= 0 || resolution.height <= 0) {
      return false;
    }

    if (_desiredResolution != resolution) {
      _desiredResolution = resolution;

      _tickRendererSettingsChanges['Resolution'] = {
        'X': _desiredResolution.width,
        'Y': _desiredResolution.height,
      };
    }

    return true;
  }

  /// Send a message to the engine beginning a drag operation on the given [actors] (paths) and using the [primaryActor]
  /// as the fulcrum point.
  /// A drag must not already be in progress, which you can check with [bIsDraggingActors].
  void beginActorDrag({required List<String> actors, String? primaryActor}) {
    if (rendererId == null) {
      _log.warning('Tried to start a drag operation, but there was no renderer ID');
      return;
    }

    if (bIsDraggingActors) {
      _log.warning('Tried to start a drag operation, but one was already in progress');
      return;
    }

    _bIsDraggingActors = true;

    ++_dragSequenceNumber;
    _connectionManager.sendMessage('ndisplay.preview.actor.drag.begin', {
      'RendererId': rendererId!,
      'Actors': actors,
      'PrimaryActor': primaryActor ?? actors.last,
      'SequenceNumber': _dragSequenceNumber,
    });
  }

  /// Send a message to the engine updating the normalized [position] of the current actor drag operation within the
  /// preview. A drag must already be in progress, which you can check with [bIsDraggingActors].
  void updateActorDrag(Offset position) {
    if (rendererId == null) {
      _log.warning('Tried to update a drag operation, but there was no renderer ID');
      return;
    }

    if (!bIsDraggingActors) {
      _log.warning('Tried to update a drag operation, but there was not one in progress');
      return;
    }

    // Send the latest actor drag position
    ++_dragSequenceNumber;
    _connectionManager.sendMessage('ndisplay.preview.actor.drag.move', {
      'RendererId': rendererId!,
      'DragPosition': offsetToJson(position),
      'SequenceNumber': _dragSequenceNumber,
    });
  }

  /// Send a message to the engine ending the current actor drag operation at the given normalized [position] within the
  /// preview.
  /// A drag must already be in progress, which you can check with [bIsDraggingActors].
  void endActorDrag(Offset position) {
    if (rendererId == null) {
      _log.warning('Tried to end a drag operation, but there was no renderer ID');
      return;
    }

    if (!bIsDraggingActors) {
      _log.warning('Tried to end a drag operation, but there was not one in progress');
      return;
    }

    _bIsDraggingActors = false;

    // Send the latest actor drag position
    ++_dragSequenceNumber;
    _connectionManager.sendMessage('ndisplay.preview.actor.drag.end', {
      'RendererId': rendererId!,
      'DragPosition': offsetToJson(position),
      'SequenceNumber': _dragSequenceNumber,
    });
  }

  @override
  void didChangeAppLifecycleState(AppLifecycleState state) {
    super.didChangeAppLifecycleState(state);

    switch (state) {
      case AppLifecycleState.resumed:
        if (_connectionManager.connectionState == EngineConnectionState.connected && !_bHasCreatedRenderer) {
          _initializePreviewRenderer();
        }
        break;

      case AppLifecycleState.paused:
      case AppLifecycleState.detached:
        _destroyPreviewRenderer();
        break;

      default:
        break;
    }
  }

  /// Call this before the manager will be destroyed.
  void dispose() {
    _onDisconnected();

    _stopListeningToEngineConnection();

    for (final StreamSubscription subscription in _settingSubscriptions) {
      subscription.cancel();
    }
  }

  /// Start listening for messages from the engine connection manager.
  void _startListeningToEngineConnection() {
    _connectionManager
      ..registerMessageListener('PreviewRendererCreated', _onPreviewRendererCreated)
      ..registerMessageListener('PreviewRenderCompleted', _onPreviewRenderCompleted)
      ..registerConnectionStateListener(_onConnectionStateChanged);
  }

  /// Stop listening for messages from the engine connection manager.
  void _stopListeningToEngineConnection() {
    _connectionManager
      ..unregisterMessageListener('PreviewRendererCreated', _onPreviewRendererCreated)
      ..unregisterMessageListener('PreviewRenderCompleted', _onPreviewRenderCompleted)
      ..unregisterConnectionStateListener(_onConnectionStateChanged);
  }

  /// Request that the engine destroy the preview renderer.
  void _destroyPreviewRenderer() {
    if (_previewRendererId != null && _connectionManager.connectionState == EngineConnectionState.connected) {
      _connectionManager.sendMessage('ndisplay.preview.renderer.destroy', {'RendererId': _previewRendererId!});
    }

    _requestRenderHelper?.giveUp();

    _previewRendererId = null;
    _bHasCreatedRenderer = false;
    _bIsDraggingActors = false;
  }

  /// Called when the connection manager's connection state changes.
  void _onConnectionStateChanged(EngineConnectionState connectionState) {
    switch (connectionState) {
      case EngineConnectionState.connected:
        _onConnected();
        break;

      default:
        _onDisconnected();
        break;
    }
  }

  /// Called when we connect to the engine.
  void _onConnected() {
    _engineUpdateTimer ??= Timer.periodic(_engineUpdateRate, _onEngineUpdateTick);

    if (!_bHasCreatedRenderer) {
      _initializePreviewRenderer();
    }
  }

  /// Called when we disconnect from the engine.
  void _onDisconnected() {
    _destroyPreviewRenderer();

    _engineUpdateTimer?.cancel();
    _engineUpdateTimer = null;
  }

  /// Send a request to initialize the preview renderer in the engine.
  void _initializePreviewRenderer() {
    if (_connectionManager.bIsInDemoMode) {
      _onPreviewRendererCreated({'RendererId': 0});
      return;
    }

    final rootPath = _selectedActorSettings.displayClusterRootPath.getValue();
    if (rootPath.isEmpty) {
      return;
    }

    _log.info('Requesting preview renderer');

    _initializeRendererHelper = MessageTimeoutRetryHelper(
      logger: _log,
      logDescription: 'initialize preview renderer',
      connection: _connectionManager,
      timeoutDuration: _previewRequestTimeout,
      sendMessage: () {
        final rootPath = _selectedActorSettings.displayClusterRootPath.getValue();
        if (rootPath.isEmpty) {
          _log.info('No root path while attempting to request preview renderer; giving up for now');
          _initializeRendererHelper?.giveUp();
          _destroyPreviewRenderer();
          return;
        }

        _connectionManager.sendMessage('ndisplay.preview.renderer.create', {
          'RootActorPath': rootPath,
          'Settings': {
            'JpegQuality': _jpegQuality,
            'Resolution': {
              'X': _desiredResolution.width,
              'Y': _desiredResolution.height,
            },
            'Rotation': _getCameraRotationSettings(),
            'IncludeActorPositions': true,
            'ProjectionType': _getProjectionModeTypeName(_stageMapSettings.projectionMode.getValue()),
            'FOV': _getFOV(),
          },
        });
      },
    );
    _initializeRendererHelper!.start();

    _initialRootPath = rootPath;
    _bHasCreatedRenderer = true;
  }

  /// Called when the engine has successfully created a preview renderer.
  void _onPreviewRendererCreated(dynamic message) {
    final int? rendererId = message['RendererId'];
    if (rendererId == null) {
      return;
    }

    _initializeRendererHelper?.succeed();

    _log.info('Preview renderer #$rendererId ready');

    _previewRendererId = rendererId;
    _dragSequenceNumber = 0;

    final String rootPath = _selectedActorSettings.displayClusterRootPath.getValue();
    if (_initialRootPath != rootPath) {
      // Root path has changed since we requested the renderer. Update it.
      _onRootActorPathChanged(rootPath);
    }

    // Request a render to kick off our render loop.
    _requestPreviewRender();
  }

  /// Request a preview render from the engine (if there's anybody to consume it).
  void _requestPreviewRender() {
    if (_previewRendererId == null || _consumers.isEmpty || _requestRenderHelper?.bIsActive == true) {
      return;
    }

    if (_connectionManager.bIsInDemoMode) {
      _onPreviewRenderCompleted({
        'RendererId': _previewRendererId,
        // A 1x1 black square encoded as a base64 image so that we have something to display
        'ImageBase64': 'iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNk+A8AAQUBAScY42YAAAAASUVORK5CYII=',
      });
      return;
    }

    _requestRenderHelper = MessageTimeoutRetryHelper(
      logger: _log,
      logDescription: 'new preview render',
      connection: _connectionManager,
      timeoutDuration: _previewRequestTimeout,
      sendMessage: () {
        _lastRenderRequestTime = DateTime.now();
        _connectionManager.sendMessage('ndisplay.preview.render', {'RendererId': _previewRendererId!});
      },
      onTimeout: (int timeoutCount) {
        if (timeoutCount > _maxPreviewTimeoutsBeforeGiveUp) {
          _log.info('Too many failed preview render attempts. Creating a new preview renderer.');
          _requestRenderHelper?.giveUp();
          _destroyPreviewRenderer();
          _initializePreviewRenderer();
        }
      },
    );

    _requestRenderHelper?.start();
  }

  /// Called when the engine completes a preview render of the scene.
  void _onPreviewRenderCompleted(dynamic message) {
    if (_previewRendererId == null) {
      // We're ignoring new renders at the moment, probably because we were disconnected
      return;
    }

    final int? receivedRendererId = message['RendererId'];
    if (receivedRendererId != _previewRendererId) {
      _log.warning('Received preview for wrong renderer (ours: $_previewRendererId; received: $receivedRendererId)');
      return;
    }

    _requestRenderHelper?.succeed();
    _requestRenderHelper = null;

    final DateTime receivedTime = DateTime.now();

    if (_consumers.isNotEmpty) {
      final String? base64Data = message['ImageBase64'];
      final Uint8List imageData = base64Data != null ? base64Decode(base64Data) : Uint8List(0);

      final num sequenceNumber = message['SequenceNumber'] ?? -1;
      final bool bIsRealTimeDisabled = message['IsRealTimeDisabled'] == true;

      final dynamic resolutionData = message['Resolution'];
      Size resolution;
      if (resolutionData != null) {
        resolution = Size(
          jsonNumberToDouble(resolutionData['X'])!,
          jsonNumberToDouble(resolutionData['Y'])!,
        );
      } else {
        // No resolution was sent, so assume it's the same as our upcoming request. Note that this can cause pins to
        // jump when the containing widget is resized.
        resolution = _desiredResolution;
      }

      // Gather actor position data
      final List<PreviewActorData> actorPositions = [];
      final dynamic actorPositionsData = message['ActorPositions'];

      if (actorPositionsData is List) {
        for (dynamic positionPair in actorPositionsData) {
          final String? actorPath = positionPair['Path'];
          if (actorPath == null) {
            continue;
          }

          final dynamic actorPositionData = positionPair['Position'];
          if (actorPositionData == null) {
            continue;
          }

          final double? xPosition = jsonNumberToDouble(actorPositionData['X']);
          final double? yPosition = jsonNumberToDouble(actorPositionData['Y']);
          if (xPosition == null || yPosition == null) {
            continue;
          }

          actorPositions.add(PreviewActorData(
            path: actorPath,
            position: Offset(xPosition, yPosition),
          ));
        }
      }

      for (final PreviewRenderConsumer consumer in _consumers) {
        consumer.onPreviewRenderCompleted(
          bIsForLatestRequest: sequenceNumber.toInt() >= _dragSequenceNumber,
          resolution: resolution,
          imageData: imageData,
          actorPositions: actorPositions,
          bIsRealTimeDisabled: bIsRealTimeDisabled,
        );
      }
    }

    // Wait for a bit before we request another preview so we don't eat up too much bandwidth
    final Duration timeSinceRequest = receivedTime.difference(_lastRenderRequestTime);
    final double delayMilliseconds = math.min(timeSinceRequest.inMilliseconds * _previewRequestDelayFactor,
        _maxPreviewRequestDelay * Duration.millisecondsPerSecond);

    Future.delayed(Duration(milliseconds: delayMilliseconds.toInt()), _requestPreviewRender);
  }

  /// Called each tick, when we're ready to send updates back to the engine.
  void _onEngineUpdateTick(Timer timer) {
    if (_previewRendererId == null || _tickRendererSettingsChanges.isEmpty) {
      return;
    }

    // Send the latest renderer config changes
    _connectionManager.sendMessage('ndisplay.preview.renderer.configure', {
      'RendererId': _previewRendererId,
      'Settings': _tickRendererSettingsChanges,
    });

    _tickRendererSettingsChanges.clear();
  }

  /// Get the name to send to the engine to request a projection mode
  String _getProjectionModeTypeName(ProjectionMode mode) {
    switch (mode) {
      case ProjectionMode.azimuthal:
        return 'Azimuthal';

      case ProjectionMode.orthographic:
        return 'Orthographic';

      case ProjectionMode.perspective:
        return 'Perspective';

      case ProjectionMode.uv:
        return 'UV';
    }
  }

  /// Update the preview renderer camera's angle.
  void _onCameraAngleChanged(vec.Vector2 cameraAngle) {
    _tickRendererSettingsChanges['Rotation'] = _getCameraRotationSettings();
  }

  /// Update the projection mode and affected settings.
  void _onProjectionModeChanged(ProjectionMode projectionMode) {
    _tickRendererSettingsChanges['ProjectionType'] = _getProjectionModeTypeName(projectionMode);
    _tickRendererSettingsChanges['FOV'] = _getFOV();
    _onCameraAngleChanged(_stageMapSettings.cameraAngle.getValue());
  }

  /// Called when the root actors in the engine have changed.
  void _onRootActorUpdate(ActorUpdateDetails details) {
    if (_previewRendererId == null || _connectionManager.connectionState != EngineConnectionState.connected) {
      return;
    }

    final String rootPath = _selectedActorSettings.displayClusterRootPath.getValue();
    if (details.addedActors.isEmpty || rootPath.isEmpty) {
      return;
    }

    if (!_bHasCreatedRenderer) {
      // May have been waiting for valid settings
      _initializePreviewRenderer();
    }

    final UnrealObject? currentRoot = _actorManager.getActorAtPath(rootPath);
    if (currentRoot == null) {
      return;
    }

    if (!details.addedActors.contains(currentRoot)) {
      return;
    }

    // If the root actor has been re-added to the scene, our render will be cancelled, so we need to request a new one.
    _requestPreviewRender();
  }

  /// Called when the path to the root nDisplay actor changes.
  void _onRootActorPathChanged(String rootActorPath) {
    if (_previewRendererId != null) {
      _connectionManager.sendMessage('ndisplay.preview.renderer.setroot', {
        'RendererId': _previewRendererId!,
        'RootActorPath': rootActorPath,
      });

      // Request a new render with the new root, since changing roots may mean we never receive the last render
      _requestPreviewRender();
    }
  }

  /// Get the field of view based on the current projection mode.
  double _getFOV() {
    switch (_stageMapSettings.projectionMode.getValue()) {
      case ProjectionMode.uv:
        return 45.0;

      case ProjectionMode.azimuthal:
        return 130.0;

      case ProjectionMode.orthographic:
      case ProjectionMode.perspective:
        return 90.0;
    }
  }

  /// Get the JSON data representing the camera's rotation settings based on the projection mode and user settings.
  dynamic _getCameraRotationSettings() {
    final bool bIsUvMode = _stageMapSettings.projectionMode.getValue() == ProjectionMode.uv;
    final vec.Vector2 cameraAngle = _stageMapSettings.cameraAngle.getValue();

    return {
      'Yaw': bIsUvMode ? 0 : cameraAngle.x,
      'Pitch': bIsUvMode ? 0 : cameraAngle.y,
      'Roll': 0,
    };
  }
}

/// Data about an actor's position as seen in a preview render.
class PreviewActorData {
  const PreviewActorData({required this.path, required this.position});

  /// The path of the actor object.
  final String path;

  /// The actor's position within the preview render, normalized to the resolution of the preview.
  final Offset position;
}

/// Mixin for classes that want to receive renders from [PreviewRenderManager].
abstract class PreviewRenderConsumer {
  /// Called when a preview render from the engine completes.
  /// [bIsForLatestRequest] is true if this contains the latest available preview and actor position data, or false if
  /// the engine is expected to send more recent data after this.
  /// [resolution] is the resolution of the preview image.
  /// [imageData] is the data for the preview image.
  /// [actorPositions] is a list of position data for all actors visible in the preview.
  /// [bIsRealTimeDisabled] is whether real-time rendering was disabled in the engine when the preview was rendered.
  void onPreviewRenderCompleted({
    required bool bIsForLatestRequest,
    required Size resolution,
    required Uint8List imageData,
    required List<PreviewActorData> actorPositions,
    required bool bIsRealTimeDisabled,
  });
}
