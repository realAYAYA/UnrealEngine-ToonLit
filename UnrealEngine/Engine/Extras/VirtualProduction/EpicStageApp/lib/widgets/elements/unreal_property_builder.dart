// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../../models/property_modify_operations.dart';
import '../../models/unreal_property_manager.dart';
import '../../models/unreal_types.dart';
import '../../utilities/guarded_refresh_state.dart';

/// Signature for a function to modify a remote property in Unreal Engine.
typedef PropertyModifyFunction<T> = void Function(
  PropertyModifyOperation operation, {
  required String description,
  T? deltaValue,
  PropertyMinMaxBehaviour minMaxBehaviour,
  T? overrideMin,
  T? overrideMax,
});

/// Uses the context's [UnrealPropertyManager] to track a property, then builds a widget exposing its tracked ID.
class UnrealPropertyBuilder<T> extends StatefulWidget {
  const UnrealPropertyBuilder({
    Key? key,
    required this.property,
    required this.builder,
  }) : super(key: key);

  /// The property to track.
  final UnrealProperty property;

  /// Function that takes the build [context], the current [value] of the property (or null if not available yet), and a
  /// [modify] function which can be called to modify the property.
  final Widget Function(BuildContext context, T? value, PropertyModifyFunction<T> modify) builder;

  @override
  State<UnrealPropertyBuilder> createState() => _UnrealPropertyBuilderState<T>();
}

class _UnrealPropertyBuilderState<T> extends State<UnrealPropertyBuilder<T>> with GuardedRefreshState {
  late final UnrealPropertyManager _propertyManager;
  TrackedPropertyId? _trackedPropertyId;

  @override
  void initState() {
    super.initState();

    _propertyManager = Provider.of<UnrealPropertyManager>(context, listen: false);
    _trackProperty();
  }

  @override
  void dispose() {
    _stopTrackingProperty();

    super.dispose();
  }

  @override
  void didUpdateWidget(covariant UnrealPropertyBuilder<T> oldWidget) {
    super.didUpdateWidget(oldWidget);

    if (oldWidget.property != widget.property) {
      _trackProperty();
    }
  }

  @override
  Widget build(BuildContext context) {
    return widget.builder(
      context,
      _propertyManager.getTrackedPropertyValue(_trackedPropertyId!),
      _modifyProperty,
    );
  }

  /// Modify the tracked property.
  void _modifyProperty(
    PropertyModifyOperation operation, {
    required String description,
    T? deltaValue,
    PropertyMinMaxBehaviour minMaxBehaviour = PropertyMinMaxBehaviour.clamp,
    T? overrideMin,
    T? overrideMax,
  }) {
    if (_propertyManager.beginTransaction(description)) {
      _propertyManager.modifyTrackedPropertyValue(
        _trackedPropertyId!,
        operation,
        deltaValue: deltaValue,
        minMaxBehaviour: minMaxBehaviour,
        overrideMin: overrideMin,
        overrideMax: overrideMax,
      );

      _propertyManager.endTransaction();
    }
  }

  /// Start tracking the widget's property (and stop tracking the old one if one exists).
  void _trackProperty() {
    _trackedPropertyId = _propertyManager.trackProperty(widget.property, _onPropertyChanged);
    _propertyManager.waitForProperty(_trackedPropertyId!).then(refreshOnData);
  }

  /// Stop tracking the currently tracked property.
  void _stopTrackingProperty() {
    if (_trackedPropertyId != null) {
      _propertyManager.stopTrackingProperty(_trackedPropertyId!, _onPropertyChanged);
    }
  }

  /// Called when the tracked property changes.
  void _onPropertyChanged(dynamic newValue) {
    guardedRefresh();
  }
}
