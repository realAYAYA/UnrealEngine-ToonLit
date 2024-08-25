//Copyright Epic Games, Inc. All Rights Reserved.

import 'dart:async';

import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../../models/actor_data/light_card_actor_data.dart';
import '../../models/settings/delta_widget_settings.dart';
import '../../models/unreal_property_controller.dart';
import '../../models/settings/selected_actor_settings.dart';

import '../../models/property_modify_operations.dart';
import '../../models/unreal_actor_manager.dart';
import '../../models/unreal_types.dart';
import '../../utilities/constants.dart';

enum _TrackpadOperation { negative, positive }

/// Trackpad widget to moving and positioning LCs, works like an actual trackpad.
class LightCardTrackPad extends StatefulWidget {
  const LightCardTrackPad({super.key});

  @override
  State<LightCardTrackPad> createState() => _LightCardTrackPadState();
}

class _LightCardTrackPadState extends State<LightCardTrackPad> {
  late final UnrealActorManager _actorManager;
  late final SelectedActorSettings _selectedActorSettings;

  late final DeltaWidgetSettings _deltaSettings;

  /// Property Controller for latitude.
  late final UnrealPropertyController<double> _latitudeController;

  /// Property controller for longitude.
  late final UnrealPropertyController<double> _longitudeController;

  /// List containing whether or not the trackpad is reversed for each selected actor.
  List<bool> _listBIsTrackpadReversed = [];

  /// Gets a list of selected actor paths.
  List<String> _getSelectedActorPaths() {
    final List<String> positionedActorClasses = [lightCardClassName];
    positionedActorClasses.addAll(colorCorrectWindowClassNames);

    final List<String> nonUVActorPaths = [];

    for (final String actorPath in _selectedActorSettings.selectedActors.getValue()) {
      final UnrealObject? actor = _actorManager.getActorAtPath(actorPath);
      if (!(actor?.isAny(positionedActorClasses) ?? false)) {
        // Actor is not a class with position properties, so leave it out entirely
        continue;
      }

      final LightCardActorData? lightCardActorData = actor!.getPerClassData<LightCardActorData>();
      if (lightCardActorData?.bIsUV == false) {
        nonUVActorPaths.add(actorPath);
      }
    }

    return nonUVActorPaths;
  }

  /// Getter for list of selected actors path.
  List<String> get _paths => _getSelectedActorPaths();

  /// stream subscriptions for watching changes to selected actors.
  StreamSubscription? _selectedActorsSubscription;

  @override
  void initState() {
    _actorManager = Provider.of<UnrealActorManager>(context, listen: false);
    _selectedActorSettings = Provider.of<SelectedActorSettings>(context, listen: false);
    _deltaSettings = Provider.of<DeltaWidgetSettings>(context, listen: false);

    _latitudeController = UnrealPropertyController(context, bShouldInitTransaction: false);
    _longitudeController = UnrealPropertyController(context, bShouldInitTransaction: false);

    _handleUpdateTrackedProperties();

    _selectedActorsSubscription = _selectedActorSettings.selectedActors.listen((event) {
      _handleUpdateTrackedProperties();
    });

    super.initState();
  }

  @override
  void dispose() {
    super.dispose();
    _latitudeController.dispose();
    _longitudeController.dispose();
    _selectedActorsSubscription?.cancel();
  }

  @override
  Widget build(BuildContext context) {
    return GestureDetector(
      onPanUpdate: _onPanUpdate,
      onPanEnd: _onPanEnd,
      onPanCancel: _onPanCancel,
      onPanStart: _onPanStart,
      child: Container(
        decoration: BoxDecoration(
          borderRadius: BorderRadius.circular(6),
          color: Colors.black,
        ),
      ),
    );
  }

  /// Handle vertical drag/pan across the trackpad widget providing Drag [details] containing positions on the x
  /// and y axis.
  void _onVerticalDrag(DragUpdateDetails details) {
    List<double> summations = [];

    double yAxis = details.delta.dy * _deltaSettings.sensitivity.getValue();

    /// get actors latitude (property value) at [index].
    double _getPropertyValue(int index) {
      if (_latitudeController.makeValueDataList().isNotEmpty) {
        return _latitudeController.makeValueDataList()[index]?.value ?? 0;
      }
      return 0;
    }

    /// increment the actors longitude at [index] with [value].
    void _incrementLongitude(double value, int index) {
      _modifyProperty(value, index, _longitudeController, PropertyMinMaxBehaviour.loop);
    }

    /// increment the actors longitude at [index] by 180° only when the actors latitude value at [index] is at 90° or
    /// -90°.
    void _incrementLongitudeByOneEighty(int index) {
      if (_getPropertyValue(index) == 90 || _getPropertyValue(index) == -90) {
        _incrementLongitude(180, index);
      }
    }

    /// handle bounce back for when we hit the north pole or its opposite side, specifying a negative/positive
    /// [operation] indicated whether the actors latitude is at 90° or -90° respectively, [summation]
    /// which denotes the to be position on the Y-axis derived from adding the current y-position and the incoming
    /// position from [details.delta.dy], and [index] which is the property index to be modified.
    void _handleBounceBack(_TrackpadOperation operation, double summation, int index) {
      // remainder from after we drive the latitude to 90° or -90°.
      double overflow = operation == _TrackpadOperation.positive ? summation - 90 : -90 - summation;

      // Drive the latitude to the max of 90° or -90°
      _modifyProperty(yAxis - overflow, index, _latitudeController);

      _incrementLongitudeByOneEighty(index);

      // Drive the latitude in the reverse direction with the [overflow].
      double tempValue = operation == _TrackpadOperation.positive ? -overflow * 2 : overflow * 2;

      _modifyProperty(tempValue, index, _latitudeController);

      // specify that we want our trackpad to now be reversed.
      _listBIsTrackpadReversed[index] = true;
    }

    /// handle case for where the trackpad is marked as reversed.
    void _handleReverseTrackpad(double summation, int index) {
      bool bIsTrackpadReversed = _listBIsTrackpadReversed[index];

      if (bIsTrackpadReversed) {
        double currentPosition = _getPropertyValue(index);

        if (summation < 90) {
          _modifyProperty(-yAxis, index, _latitudeController);

          if (currentPosition == 90) {
            _incrementLongitudeByOneEighty(index);
            _listBIsTrackpadReversed[index] = false;
          }
        }

        if (currentPosition == -90) {
          _incrementLongitudeByOneEighty(index);
          _modifyProperty(yAxis, index, _latitudeController);

          _listBIsTrackpadReversed[index] = false;
        }

        return;
      }

      _modifyProperty(yAxis, index, _latitudeController);
    }

    /// get (summations) to be positions of all actors on the y-axis by adding the value of [details.delta.dy] to an
    /// actors current position.
    for (int index = 0; index < _paths.length; index++) {
      double latitude = 0;

      /// check to make sure all selected actors have a valid position on the y-axis and that the length of paths
      /// matches the length of data values for the actors.
      if (_latitudeController.makeValueDataList().isNotEmpty &&
          _paths.length == _latitudeController.makeValueDataList().length) {
        latitude = _latitudeController.makeValueDataList()[index]?.value ?? 0;
      }

      /// sum of incoming dy [yAxis] and the current Y position [latitude] of LC'(s).
      double summation = latitude + yAxis;

      summations.add(summation);
    }

    /// handle when we hit either the negative or positive south pole.
    for (int index = 0; index < summations.length; index++) {
      double sum = summations[index];
      if (sum > 90) {
        _handleBounceBack(_TrackpadOperation.positive, sum, index);
      } else if (sum < -90) {
        _handleBounceBack(_TrackpadOperation.negative, sum, index);
      } else {
        _handleReverseTrackpad(sum, index);
      }
    }
  }

  /// Handle horizontal drag/pan across the trackpad widget providing Drag [details] containing positions on the x
  /// and y axis.
  void _onHorizontalDrag(DragUpdateDetails details) {
    double dx = details.delta.dx * _deltaSettings.sensitivity.getValue();
    for (var index = 0; index < _paths.length; index++) {
      _modifyProperty(dx, index, _longitudeController, PropertyMinMaxBehaviour.loop);
    }
  }

  /// Track all properties and update a property with [name] at the specified [index], modifying the property with
  /// [value], using the related UnrealPropertyController [controller], optionally providing a
  /// PropertyMinMaxBehaviour [behaviour] or [PropertyMinMaxBehaviour.clamp] will be used as default.
  void _modifyProperty(double value, int index, UnrealPropertyController controller,
      [PropertyMinMaxBehaviour behaviour = PropertyMinMaxBehaviour.clamp]) {
    controller.modifyProperty(controller.modifyOperation, index, value: value, minMaxBehaviour: behaviour);
  }

  /// update tracked properties for both latitude and longitude controllers.
  void _handleUpdateTrackedProperties() {
    _latitudeController
        .trackAllProperties(_getProperties('Latitude', _paths, modifierFn: _modifyPositionalPropertyNameBasedOnClass));
    _longitudeController
        .trackAllProperties(_getProperties('Longitude', _paths, modifierFn: _modifyPositionalPropertyNameBasedOnClass));
  }

  /// Handles all sort of drag/pan gestures on the trackpad, whether vertically or horizontally, providing Drag
  /// update [details] which contains x [details.delta.dx] & y [details.delta.dy] positions.
  void _onPanUpdate(DragUpdateDetails details) {
    _longitudeController.beginTransaction();
    _onVerticalDrag(details);
    _onHorizontalDrag(details);
  }

  /// Called when pan/drag gestures end's, providing Drag end [details].
  void _onPanEnd(DragEndDetails details) {
    _longitudeController.endTransaction();
    _listBIsTrackpadReversed = _paths.map((e) => false).toList();
  }

  /// Called when pan/drag gesture is canceled.
  void _onPanCancel() {
    _longitudeController.endTransaction();
    _listBIsTrackpadReversed = _paths.map((e) => false).toList();
  }

  /// Called when pan/drag gesture starts.
  void _onPanStart(DragStartDetails details) {
    _listBIsTrackpadReversed = _paths.map((e) => false).toList();
  }

  /// Given an [actorPath] and a [propertyName], return a modified positional property name accounting for the actor's
  /// type.
  String _modifyPositionalPropertyNameBasedOnClass(String actorPath, String propertyName) {
    final UnrealObject? actor = _actorManager.getActorAtPath(actorPath);

    if (actor != null && actor.isAny(colorCorrectWindowClassNames)) {
      // CCWs store their positional properties in a sub-structure.
      return 'PositionalParams.' + propertyName;
    }

    return propertyName;
  }
}

/// A simple type definition used by the [_geProperties] function.
typedef ModifierFn = String Function(String path, String name);

/// Get a list of properties with the given [name] for all of the actors with path in [paths].
/// If [modifierFn] is provided, it will be called for each [actorPath] and the [propertyName] of the property,
/// and its return value will be used in place of [name].
List<UnrealProperty> _getProperties(String name, List<String> paths, {ModifierFn? modifierFn, String? overrideName}) {
  return paths
      .map(
        (actorPath) => UnrealProperty(
          objectPath: actorPath,
          propertyName: (modifierFn != null) ? modifierFn(actorPath, name) : name,
          typeNameOverride: overrideName,
        ),
      )
      .toList();
}
