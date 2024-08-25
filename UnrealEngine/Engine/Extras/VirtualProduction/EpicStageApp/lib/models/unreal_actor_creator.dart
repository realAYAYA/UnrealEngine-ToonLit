// Copyright Epic Games, Inc. All Rights Reserved.

import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter_gen/gen_l10n/app_localizations.dart';
import 'package:logging/logging.dart';
import 'package:provider/provider.dart';

import '../utilities/constants.dart';
import '../utilities/unreal_utilities.dart';
import 'color.dart';
import 'engine_connection.dart';
import 'navigator_keys.dart';
import 'preview_render_manager.dart';
import 'settings/recent_actor_settings.dart';
import 'settings/selected_actor_settings.dart';
import 'unreal_actor_manager.dart';
import 'unreal_types.dart';

final _log = Logger('UnrealActorCreator');

/// How long to wait before forgetting about an actor creation request (meaning even if the actor is created, we won't
/// select it in the app).
const Duration _creationRequestTimeout = Duration(seconds: 1);

/// Settings that apply when creating any type of actor.
class ActorCreationSettings {
  const ActorCreationSettings({
    this.name,
    this.template,
    this.mapPosition,
    this.callback,
    this.extraParams,
  });

  /// The name to give the actor.
  final String? name;

  /// The template from which to create the actor.
  final UnrealTemplateData? template;

  /// The normalized position within the preview map at which to place the actor.
  final Offset? mapPosition;

  /// A function to call once the engine reports that the actor has been created.
  final void Function()? callback;

  /// Any extra JSON parameters to pass with the creation request.
  final Map<String, dynamic>? extraParams;

  /// Actor settings left at their default values.
  static const ActorCreationSettings defaults = ActorCreationSettings();

  /// Make a copy of the settings, replacing certain parameters.
  ActorCreationSettings copyWith({
    String? name,
    UnrealTemplateData? template,
    Offset? mapPosition,
    void Function()? callback,
    Map<String, dynamic>? extraParams,
  }) {
    return ActorCreationSettings(
      name: name ?? this.name,
      template: template ?? this.template,
      mapPosition: mapPosition ?? this.mapPosition,
      callback: callback ?? this.callback,
      extraParams: extraParams ?? this.extraParams,
    );
  }
}

/// A utility that manages creating actors and handling creation responses from the engine.
/// This is separate from UnrealActorManager because it also depends on user settings, previews, etc.
class UnrealActorCreator {
  UnrealActorCreator(BuildContext context)
      : _connectionManager = Provider.of<EngineConnectionManager>(context, listen: false),
        _previewRenderManager = Provider.of<PreviewRenderManager>(context, listen: false),
        _actorManager = Provider.of<UnrealActorManager>(context, listen: false),
        _recentActorSettings = Provider.of<RecentActorSettings>(context, listen: false),
        _selectedActorSettings = Provider.of<SelectedActorSettings>(context, listen: false),
        _localizations = AppLocalizations.of(rootNavigatorKey.currentContext!)! {
    _connectionManager..registerMessageListener('RequestedActorsCreated', _onRequestedActorsCreated);
  }

  final EngineConnectionManager _connectionManager;
  final UnrealActorManager _actorManager;
  final PreviewRenderManager _previewRenderManager;
  final RecentActorSettings _recentActorSettings;
  final SelectedActorSettings _selectedActorSettings;
  final AppLocalizations _localizations;

  /// The ID to send with actor creation requests to the engine.
  int _lastRequestId = 0;

  /// Map from IDs of actor creation requests pending a response from the engine to data about the request.
  Map<int, _PendingCreationRequest> _pendingRequests = {};

  /// Call this to clean up any subscriptions.
  void dispose() {
    _connectionManager.unregisterMessageListener('RequestedActorsCreated', _onRequestedActorsCreated);
  }

  /// Send a message to the engine requesting that a new lightcard be created.
  void createLightcard([ActorCreationSettings settings = ActorCreationSettings.defaults]) async {
    final UnrealTemplateData? passedTemplate = settings.template;

    // Set the default template if there is one. This isn't strictly necessary since the engine should use it anyway,
    // but we want to explicitly set it so we know that the actor already has its default position provided by the
    // template.
    UnrealTemplateData? defaultTemplate;
    if (settings.template == null) {
      // Check if there's a default template
      final UnrealHttpResponse templateResponse = await _connectionManager.sendHttpRequest(UnrealHttpRequest(
        url: '/remote/object/property',
        verb: 'PUT',
        body: {
          'objectPath': '/Script/DisplayClusterLightCardEditor.Default__DisplayClusterLightCardEditorProjectSettings',
          'propertyName': 'DefaultLightCardTemplate',
          'access': 'READ_ACCESS',
        },
      ));

      if (templateResponse.code == HttpResponseCode.ok) {
        final String? defaultTemplatePath = templateResponse.body?['DefaultLightCardTemplate'];
        if (defaultTemplatePath != null && defaultTemplatePath.isNotEmpty) {
          defaultTemplate = UnrealTemplateData(name: '', path: defaultTemplatePath);
        }
      }
    }

    settings = settings.copyWith(
      name: settings.name ?? _localizations.newActorNameLightCard,
      template: passedTemplate ?? defaultTemplate,
    );

    createActor(className: lightCardClassName, settings: settings);

    _recentActorSettings.addRecentlyPlacedActor(RecentlyPlacedActorData(
      name: passedTemplate?.name,
      type: RecentlyPlacedActorType.lightCard,
      templatePath: passedTemplate?.path,
    ));
  }

  /// Send a message to the engine requesting that a new lightcard be created.
  void createChromakeyCard([ActorCreationSettings settings = ActorCreationSettings.defaults]) {
    if (settings.name == null) {
      settings = settings.copyWith(name: _localizations.newActorNameChromakeyCard);
    }

    createActor(className: chromakeyCardClassName, settings: settings);

    _recentActorSettings.addRecentlyPlacedActor(RecentlyPlacedActorData(
      name: settings.template?.name,
      type: RecentlyPlacedActorType.chromakeyCard,
      templatePath: settings.template?.path,
    ));
  }

  /// Send a message to the engine requesting that a new flag be created.
  void createFlag([ActorCreationSettings settings = ActorCreationSettings.defaults]) async {
    // First, try to get the default flag template
    final UnrealHttpResponse templateResponse = await _connectionManager.sendHttpRequest(UnrealHttpRequest(
      url: '/remote/object/property',
      verb: 'PUT',
      body: {
        'objectPath': '/Script/DisplayClusterLightCardEditor.Default__DisplayClusterLightCardEditorProjectSettings',
        'propertyName': 'DefaultFlagTemplate',
        'access': 'READ_ACCESS',
      },
    ));

    // If the template isn't available, explicitly send None so we don't get the default LC template instead.
    String templatePath = 'None';

    if (templateResponse.code == HttpResponseCode.ok) {
      final String? defaultTemplate = templateResponse.body?['DefaultFlagTemplate'];
      if (defaultTemplate != null && defaultTemplate.isNotEmpty) {
        templatePath = defaultTemplate;
      }
    }

    final Map<String, dynamic> extraParams = {
      'bIsLightCardFlag': true,
      'OverrideColor': true,
      'Color': FloatColor(0, 0, 0).toJson(),
    };

    if (settings.extraParams != null) {
      extraParams.addAll(settings.extraParams!);
    }

    createActor(
      className: lightCardClassName,
      settings: settings.copyWith(
        name: settings.name ?? _localizations.newActorNameFlag,
        template: UnrealTemplateData(name: '', path: templatePath),
        extraParams: extraParams,
      ),
    );

    _recentActorSettings.addRecentlyPlacedActor(RecentlyPlacedActorData(
      type: RecentlyPlacedActorType.flag,
    ));
  }

  /// Send a message to the engine requesting that a new CCW be created.
  void createColorCorrectionWindow([ActorCreationSettings settings = ActorCreationSettings.defaults]) {
    if (settings.name == null) {
      settings = settings.copyWith(name: settings.name ?? _localizations.newActorNameColorCorrectWindow);
    }

    createActor(
      className: colorCorrectWindowClassNames.first,
      settings: settings.copyWith(
        mapPosition: settings.mapPosition ?? Offset(0.5, 0.5),
      ),
    );

    _recentActorSettings.addRecentlyPlacedActor(RecentlyPlacedActorData(
      type: RecentlyPlacedActorType.colorCorrectWindow,
    ));
  }

  /// Send a message to the engine requesting that a new actor be created.
  void createActor({
    required String className,
    ActorCreationSettings settings = ActorCreationSettings.defaults,
  }) {
    Offset? position = settings.mapPosition;

    if (position == null && (settings.template == null || settings.template?.path == 'None')) {
      // Place at camera center
      position = _previewRenderManager.rendererFocalPoint;
    }

    final messageBody = {
      'RendererId': _previewRenderManager.rendererId,
      if (settings.name != null) 'ActorName': settings.name,
      if (settings.template != null) 'TemplatePath': settings.template?.path,
      'ActorClass': className,
      'RequestId': _startActorCreationRequest(callback: settings.callback, position: position),
      'OverridePosition': position != null,
      if (position != null) 'Position': offsetToJson(position),
    };

    if (settings.extraParams != null) {
      messageBody.addAll(settings.extraParams!);
    }

    _connectionManager.sendMessage('ndisplay.preview.actor.create', messageBody);
  }

  /// Send a message to the engine requesting that the currently selected actors be duplicated.
  void duplicateSelectedActors({void Function()? callback}) {
    final List<String> actorPathsToDuplicate = _selectedActorSettings.selectedActors
        .getValue()
        .where((String actorPath) => _actorManager.getActorAtPath(actorPath)?.bIsValid == true)
        .toList(growable: false);

    _connectionManager.sendMessage('stageapp.actors.duplicate', {
      'Actors': actorPathsToDuplicate,
      'RequestId': _startActorCreationRequest(callback: callback),
    });
  }

  /// Create an ID for an actor creation request and start a timer for it.
  /// When the engine confirms that the actor has been created, [callback] will be called.
  /// If [position] is provided and the API version requires it, we will attempt to move the actor to that position
  /// after its creation.
  int _startActorCreationRequest({void Function()? callback, Offset? position}) {
    final int requestId = _lastRequestId;
    ++_lastRequestId;

    _pendingRequests[requestId] = _PendingCreationRequest(callback: callback, position: position);
    return requestId;
  }

  /// Called when the engine sends a message that actors we requested were created.
  void _onRequestedActorsCreated(dynamic message) {
    final dynamic requestId = message['RequestId'];
    final _PendingCreationRequest? pendingRequest = _pendingRequests.remove(requestId);
    if (pendingRequest == null) {
      return;
    }

    final List<dynamic>? actorPaths = message['ActorPaths'];
    if (pendingRequest.bSelectOnComplete) {
      if (actorPaths != null) {
        // We got a list of actors, so drop the current selection and select those ones
        _selectedActorSettings.deselectAllActors();

        for (dynamic actorPath in actorPaths) {
          if (actorPath is! String) {
            continue;
          }
          _selectedActorSettings.selectActor(actorPath);
        }
      }
    }

    // On older versions of the engine, a bug causes the initial actor position to be incorrect in some cases, so send
    // a quick drag request to fix it if possible.
    if (_connectionManager.apiVersion?.bIsNewActorOverridePositionAccurate != true &&
        pendingRequest.position != null &&
        actorPaths != null &&
        actorPaths.length == 1 &&
        actorPaths.first is String) {
      if (_previewRenderManager.bIsDraggingActors) {
        _log.info('Could not fix position for new actor ${actorPaths[0]} because a drag was in progress');
      } else {
        _previewRenderManager.beginActorDrag(actors: [actorPaths.first] /*, primaryActor: actorPaths.first*/);
        _previewRenderManager.endActorDrag(pendingRequest.position!);
      }
    }

    pendingRequest.callback?.call();
    pendingRequest.dispose();
  }
}

/// Data about a request to create actors pending a response from the engine.
class _PendingCreationRequest {
  _PendingCreationRequest({this.callback, this.position}) {
    _autoSelectTimeoutTimer = Timer(_creationRequestTimeout, _onAutoSelectTimedOut);
  }

  /// Callback function for when the request completes.
  final void Function()? callback;

  /// The actor's desired position.
  final Offset? position;

  /// Timer that fires when the request took too long to automatically select the new actors.
  late final Timer _autoSelectTimeoutTimer;

  /// Whether to automatically select the new actors when the request completes.
  bool bSelectOnComplete = true;

  /// Clean up any remaining data.
  void dispose() {
    _autoSelectTimeoutTimer.cancel();
  }

  /// Called when a creation request has waited too long for a response to select the actors automatically.
  void _onAutoSelectTimedOut() {
    bSelectOnComplete = false;
  }
}
