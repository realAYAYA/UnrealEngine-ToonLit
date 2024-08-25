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

/// Uses the context's [UnrealPropertyManager] to track a property, then builds a widget exposing its value and a
/// function to modify it.
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
    if (_trackedPropertyId != null) {
      _stopTrackingProperty();
    }

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

/// Uses the context's [UnrealPropertyManager] to track multiple properties of the same type, then builds a widget
/// exposing the shared value and a function to modify it.
class UnrealMultiPropertyBuilder<T> extends StatefulWidget {
  const UnrealMultiPropertyBuilder({
    Key? key,
    required this.properties,
    required this.fallbackValue,
    required this.builder,
  }) : super(key: key);

  /// The properties to track.
  final List<UnrealProperty> properties;

  /// If the properties have different values, this will be passed down as the shared value instead.
  final T fallbackValue;

  /// Function that takes the build [context], the current [sharedValue] shared by all properties (or null if not
  /// available yet, or the [fallbackValue] if the values aren't all the same), and a [modify] function which can be
  /// called to modify the property.
  final Widget Function(BuildContext context, T? sharedValue, PropertyModifyFunction<T> modify) builder;

  @override
  State<UnrealMultiPropertyBuilder> createState() => _UnrealMultiPropertyBuilderState<T>();
}

class _UnrealMultiPropertyBuilderState<T> extends State<UnrealMultiPropertyBuilder<T>> with GuardedRefreshState {
  late final UnrealPropertyManager _propertyManager;
  Iterable<TrackedPropertyId> _trackedPropertyIds = [];
  bool _bIsWaitingForValues = true;

  @override
  void initState() {
    super.initState();

    _propertyManager = Provider.of<UnrealPropertyManager>(context, listen: false);
    _trackProperties();
  }

  @override
  void dispose() {
    _stopTrackingProperties();

    super.dispose();
  }

  @override
  void didUpdateWidget(covariant UnrealMultiPropertyBuilder<T> oldWidget) {
    super.didUpdateWidget(oldWidget);

    if (oldWidget.properties.length != widget.properties.length) {
      _trackProperties();
      return;
    }

    for (int propertyIndex = 0; propertyIndex < oldWidget.properties.length; ++propertyIndex) {
      if (oldWidget.properties[propertyIndex] != widget.properties[propertyIndex]) {
        _trackProperties();
        return;
      }
    }
  }

  @override
  Widget build(BuildContext context) {
    return widget.builder(
      context,
      _getSharedValue(),
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
      for (final TrackedPropertyId propertyId in _trackedPropertyIds) {
        _propertyManager.modifyTrackedPropertyValue(
          propertyId,
          operation,
          deltaValue: deltaValue,
          minMaxBehaviour: minMaxBehaviour,
          overrideMin: overrideMin,
          overrideMax: overrideMax,
        );
      }

      _propertyManager.endTransaction();
    }
  }

  /// Start tracking the widget's property (and stop tracking the old one if one exists).
  void _trackProperties() async {
    if (_trackedPropertyIds.isNotEmpty) {
      _stopTrackingProperties();
    }

    _trackedPropertyIds =
        widget.properties.map((property) => _propertyManager.trackProperty(property, _onPropertyChanged));

    // Refresh once all values are received
    _bIsWaitingForValues = true;
    await Future.wait(_trackedPropertyIds.map((propertyId) => _propertyManager.waitForProperty(propertyId)));

    _bIsWaitingForValues = false;
    guardedRefresh();
  }

  /// Stop tracking the currently tracked property.
  void _stopTrackingProperties() {
    if (_trackedPropertyIds.isEmpty) {
      return;
    }

    for (final TrackedPropertyId propertyId in _trackedPropertyIds) {
      _propertyManager.stopTrackingProperty(propertyId, _onPropertyChanged);
    }
  }

  /// Get the value to pass down to the builder function.
  T? _getSharedValue() {
    if (_bIsWaitingForValues) {
      return null;
    }

    final List<T?> values = _trackedPropertyIds
        .map((propertyId) => _propertyManager.getTrackedPropertyValue(propertyId) as T?)
        .toList(growable: false);

    final firstValue = values.first;

    for (int propertyIndex = 1; propertyIndex < values.length; ++propertyIndex) {
      if (values[propertyIndex] != firstValue) {
        return widget.fallbackValue;
      }
    }

    return firstValue;
  }

  /// Called when the tracked property changes.
  void _onPropertyChanged(dynamic newValue) {
    guardedRefresh();
  }
}
