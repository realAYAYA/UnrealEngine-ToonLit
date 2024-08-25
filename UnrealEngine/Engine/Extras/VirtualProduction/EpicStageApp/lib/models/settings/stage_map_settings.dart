// Copyright Epic Games, Inc. All Rights Reserved.

import 'dart:ui';

import 'package:epic_common/preferences.dart';
import 'package:streaming_shared_preferences/streaming_shared_preferences.dart';
import 'package:vector_math/vector_math_64.dart' as vec;

enum ProjectionMode {
  azimuthal,
  orthographic,
  perspective,
  uv,
}

/// Holds user settings used to render and control the stage map.
class StageMapSettings {
  StageMapSettings(PreferencesBundle preferences)
      : bIsInTransformMode = preferences.persistent.getBool(
          'stageMap.bIsInMultiSelectMode',
          defaultValue: false,
        ),
        projectionMode = preferences.persistent.getEnum<ProjectionMode>(
          'stageMap.projectionMode',
          defaultValue: ProjectionMode.azimuthal,
          enumValues: ProjectionMode.values,
        ),
        cameraAngle = preferences.transient.get(
          'stageMap.cameraAngle',
          defaultValue: vec.Vector2(0, 90),
        ),
        focalPoint = preferences.transient.get(
          'stageMap.focalPoint',
          defaultValue: Offset(0.5, 0.5),
        ),
        zoomLevel = preferences.transient.get(
          'stageMap.zoomLevel',
          defaultValue: 1.0,
        );

  /// If true, the map is in transform mode and the user can pinch/pan to adjust lightcard scale/rotation/location.
  /// If false, the map is in map mode, and the user can pinch/pan to move the map or long-press pins to drag them
  /// around.
  final Preference<bool> bIsInTransformMode;

  /// The camera's current projection mode for stage actor locations.
  final Preference<ProjectionMode> projectionMode;

  /// The camera's current rotation setting in degrees, where X is yaw and Y is pitch.
  final TransientPreference<vec.Vector2> cameraAngle;

  /// The location on the map in center of the stage map view, assuming a 1:1 aspect ratio.
  final TransientPreference<Offset> focalPoint;

  /// The zoom level of the stage map view.
  final TransientPreference<double> zoomLevel;
}
