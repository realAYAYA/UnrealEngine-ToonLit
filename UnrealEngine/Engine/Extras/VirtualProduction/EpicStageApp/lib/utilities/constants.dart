// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:flutter/material.dart';

/// The Unreal class for nDisplay root actors.
const String nDisplayRootActorClassName = '/Script/DisplayCluster.DisplayClusterRootActor';

/// The Unreal class name used for light cards.
const String lightCardClassName = '/Script/DisplayCluster.DisplayClusterLightCardActor';

/// The Unreal class name used for chroma key cards.
const String chromakeyCardClassName = '/Script/DisplayCluster.DisplayClusterChromakeyCardActor';

/// The Unreal class names used for color correct windows.
/// The preferred class name must be FIRST in this list.
const List<String> colorCorrectWindowClassNames = [
  '/Script/ColorCorrectRegions.ColorCorrectionWindow',
  '/Script/ColorCorrectRegions.ColorCorrectWindow',
];

/// The Unreal class name used for color correct regions.
const String colorCorrectRegionClassName = '/Script/ColorCorrectRegions.ColorCorrectRegion';

/// The Unreal class name used for post-process volumes.
const String postProcessVolumeClassName = '/Script/Engine.PostProcessVolume';

/// List of names for scene actor classes that should be controllable from the app.
List<String> get controllableClassNames {
  if (_controllableClassNames == null) {
    _controllableClassNames = [lightCardClassName, chromakeyCardClassName, colorCorrectRegionClassName];
    _controllableClassNames!.addAll(colorCorrectWindowClassNames);
  }

  return _controllableClassNames!;
}

List<String>? _controllableClassNames = null;

/// Default decoration style for text inputs.
final InputDecoration collapsedInputDecoration = const InputDecoration(
  isCollapsed: true,
  contentPadding: EdgeInsets.symmetric(horizontal: 12, vertical: 16),
  border: OutlineInputBorder(
    borderRadius: BorderRadius.all(Radius.circular(4)),
    borderSide: BorderSide.none,
  ),
);
