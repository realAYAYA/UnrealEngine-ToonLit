// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:flutter/material.dart';

import '../../models/property_modify_operations.dart';
import '../../models/unreal_property_manager.dart';
import '../../models/unreal_property_controller.dart';
import '../../models/unreal_types.dart';
import 'delta_widget_base.dart';

/// Represents a single value shared by all controlled properties of a widget (if such a value exists).
class SingleSharedValue<T> {
  const SingleSharedValue({required this.value, required this.bHasMultipleValues});

  /// The value shared by all controlled properties. This may be null either if all property values are null or if
  /// there's no single shared value.
  final T? value;

  /// True if there is no single shared value among all properties.
  final bool bHasMultipleValues;

  bool operator ==(Object other) {
    if (identical(this, other)) {
      return true;
    }

    if (other.runtimeType != runtimeType) {
      return false;
    }

    return other is SingleSharedValue<T> && other.value == value && other.bHasMultipleValues == bHasMultipleValues;
  }
}

/// Data for an Unreal property controlled by a widget. Each widget should have its own instance of this class per
/// value that the widget is controlling.
class WidgetControlledUnrealProperty<T> {
  WidgetControlledUnrealProperty(
      {required this.trackedId,
      required this.propertyIndex,
      required this.changedCallback,
      required this.value,
      this.ownerName});

  /// The ID used to refer to this property with the property manager.
  final TrackedPropertyId trackedId;

  /// The index of this property in the list of properties the widget is displaying.
  final int propertyIndex;

  /// The callback function when the property manager changes the value of the property.
  final PropertyChangeCallback changedCallback;

  /// The name of the object this property belongs to.
  final String? ownerName;

  /// The current value of the property, or null if it hasn't been set yet.
  T? value;

  /// Generate the data needed to display a dot for this property.
  /// This should only be called once the [value] is ready (i.e. not null).
  DeltaWidgetValueData<T> toValueData() {
    if (value == null) {
      throw Exception('toValueData should not be called until the property\'s value is non-null');
    }

    return DeltaWidgetValueData(value: value!);
  }
}

/// A widget that controls one or more Unreal properties.
abstract class UnrealWidget extends StatefulWidget {
  const UnrealWidget({
    Key? key,
    required this.unrealProperties,
    this.overrideName,
    this.enableProperties,
    this.onChangedByUser,
    this.minMaxBehaviour = PropertyMinMaxBehaviour.clamp,
  }) : super(key: key);

  /// An optional name that will override the property name received from the engine.
  final String? overrideName;

  /// The properties this widget controls in the engine.
  final List<UnrealProperty> unrealProperties;

  /// How to treat values that exceed the engine min/max.
  final PropertyMinMaxBehaviour minMaxBehaviour;

  /// A callback function called when the user changes the value of the widget.
  final void Function()? onChangedByUser;

  /// A list of properties controlling whether the value is enabled in the engine. If provided, a checkbox will be
  /// shown controlling these properties.
  final List<UnrealProperty>? enableProperties;
}

/// Mixin that adds state management for widgets that control Unreal properties.
/// To use this mixin:
/// - Call [makeValueDataList] to create a list of [DeltaWidgetValueData] containing the current values.
/// - Call [handleOnChangedByUser] with a list of delta values when the user interacts with the widget.
/// - Call [handleOnResetByUser] when the user presses the widget's reset button.
/// - Override [modifyOperation] if needed.
mixin UnrealWidgetStateMixin<WidgetType extends UnrealWidget, PropertyType> on State<WidgetType> {
  late final UnrealPropertyController<PropertyType> _singlePropertyController;

  /// The label of the property to show to users.
  String get propertyLabel => widget.overrideName ?? (_singlePropertyController.propertyLabel);

  /// The property's minimum value as received from the engine.
  PropertyType? get engineMin => _singlePropertyController.engineMin;

  /// The property's maximum value as received from the engine.
  PropertyType? get engineMax => _singlePropertyController.engineMax;

  /// The overridden minimum value the controlled properties can reach. If null, [engineMin] will be used.
  PropertyType? get overrideMin => _singlePropertyController.overrideMin;

  /// The overridden maximum value the controlled properties can reach. If null, [engineMax] will be used.
  PropertyType? get overrideMax => _singlePropertyController.overrideMax;

  /// The type of modify operation this widget uses to apply deltas.
  PropertyModifyOperation get modifyOperation => _singlePropertyController.modifyOperation;

  /// The number of properties this is controlling.
  int get propertyCount => _singlePropertyController.properties.length;

  /// The property's possible enum values as received from the engine.
  List<String> get propertyEnumValues => _singlePropertyController.propertyEnumValues;

  @override
  void initState() {
    _singlePropertyController = UnrealPropertyController(context);
    _singlePropertyController.trackAllProperties(widget.unrealProperties, widget.enableProperties);
    _singlePropertyController.addListener(_handleOnPropertiesChanged);

    super.initState();
  }

  @override
  void dispose() {
    _singlePropertyController.removeListener(_handleOnPropertiesChanged);
    _singlePropertyController.dispose();

    super.dispose();
  }

  @override
  void didUpdateWidget(WidgetType oldWidget) {
    super.didUpdateWidget(oldWidget);
    _singlePropertyController.trackAllPropertiesIfChanged(widget.unrealProperties, widget.enableProperties);
  }

  /// Call this to indicate that the user has started interacting with the widget.
  /// This will be automatically called for you as soon as any properties change, but you can safely call it earlier.
  /// A [description] will be automatically generated if null is passed.
  /// Returns true if a new transaction was started.
  bool beginTransaction([String? description]) {
    return _singlePropertyController.beginTransaction(description);
  }

  /// Call this to indicate that the user has finished interacting with the widget.
  void endTransaction() {
    _singlePropertyController.endTransaction();
  }

  /// Generate a list of [DeltaWidgetValueData] containing the data needed to display each control in the widget.
  /// Values may be null if the value hasn't been received from the engine yet.
  List<DeltaWidgetValueData<PropertyType>?> makeValueDataList() => _singlePropertyController.makeValueDataList();

  /// Return a SingleSharedValue indicating whether there is a single value shared by all controlled properties.
  /// If there are no controlled values, the SingleSharedValue will instead contain [defaultValue].
  SingleSharedValue<PropertyType> getSingleSharedValue([PropertyType? defaultValue]) {
    // Get the value of all non-null properties
    final List<PropertyType?> controlledValues =
        _singlePropertyController.properties.map((property) => property?.value).toList();
    controlledValues.removeWhere((element) => element == null);

    bool bHasMultipleValues = false;
    PropertyType? sharedValue;

    if (controlledValues.length > 1) {
      sharedValue = controlledValues[0];

      // We're controlling multiple properties. Check that they all have the same value
      for (int valueIndex = 1; valueIndex < controlledValues.length; ++valueIndex) {
        if (controlledValues[valueIndex] != sharedValue) {
          bHasMultipleValues = true;
          sharedValue = null;
          break;
        }
      }
    } else if (controlledValues.isNotEmpty) {
      // There's exactly one value, so use it directly
      sharedValue = controlledValues[0];
    } else {
      // There are no properties, so return the default
      sharedValue = defaultValue;
    }

    return SingleSharedValue(value: sharedValue, bHasMultipleValues: bHasMultipleValues);
  }

  /// handle state change for [properties].
  void _handleOnPropertiesChanged() {
    setState(() {});
  }

  /// Called when the user changes controlled values.
  void handleOnChangedByUser(List<PropertyType> deltaValues) {
    setState(() {
      _singlePropertyController.modifyProperties(
        modifyOperation,
        values: deltaValues,
        onChangedByUser: widget.onChangedByUser,
      );
    });
  }

  /// Called when the user presses the reset button.
  void handleOnResetByUser() {
    setState(() => _singlePropertyController.handleOnResetByUser(widget.enableProperties));
  }

  /// Apply an [operation] to each property this controls by the amounts specified in [values].
  /// If [values] is null, this is treated as if it was a list of null values matching the number of properties.
  /// If [bIgnoreLimits] is true, the min and max for the widget will be ignored.
  void modifyProperties(PropertyModifyOperation operation, {List<dynamic>? values, bool bIgnoreLimits = false}) {
    _singlePropertyController.modifyProperties(operation, values: values, bIgnoreLimits: bIgnoreLimits);
  }
}
