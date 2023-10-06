// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:flutter/cupertino.dart';
import 'package:flutter/foundation.dart';
import 'package:flutter_gen/gen_l10n/app_localizations.dart';
import 'package:logging/logging.dart';
import 'package:provider/provider.dart';

import '../../models/property_modify_operations.dart';
import '../../models/unreal_property_manager.dart';
import '../../models/unreal_types.dart';
import 'delta_widget_base.dart';

final _log = Logger('UnrealWidget');

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
  /// The property manager this widget uses to update its properties.
  late final UnrealPropertyManager _propertyManager;

  /// A list of properties currently controlled by the widget.
  final List<WidgetControlledUnrealProperty<PropertyType>?> _properties = [];

  /// A list of properties for enabling widget properties current controlled by the widget.
  final List<WidgetControlledUnrealProperty<bool>?> _enableProperties = [];

  /// The last list of properties that were passed to the widget. This is stored so we can compare the list on rebuild
  /// and register/unregister properties when the list changes.
  final List<UnrealProperty> _lastUnrealProperties = [];

  /// The last list of enable properties that were passed to the widget. This is stored so we can compare the list on
  /// rebuild and register/unregister properties when the list changes.
  final List<UnrealProperty> _lastEnableProperties = [];

  /// The property's metadata as received from the engine.
  UnrealPropertyMetadata? _propertyMetadata;

  /// This is incremented whenever the set of tracked properties changes so we can ignore any callbacks from previous
  /// versions. For example, if the user rapidly changes the list multiple times, we may get changed value or newly
  /// tracked value callbacks for properties that we no longer care about.
  int _latestTrackedPropertyVersion = 0;

  /// Whether the properties' values are ready to display to the user.
  bool _bIsReady = false;

  /// Whether the widget is actively in a transaction.
  bool _bIsInTransaction = false;

  /// Whether the properties' values are ready to display to the user.
  bool get bIsReady => _bIsReady;

  /// The property's metadata as received from the engine.
  UnrealPropertyMetadata? get propertyMetadata => _propertyMetadata;

  /// The label of the property to show to users.
  String get propertyLabel => widget.overrideName ?? (propertyMetadata?.displayName ?? '');

  /// The property's minimum value as received from the engine.
  PropertyType? get engineMin => propertyMetadata?.minValue;

  /// The property's maximum value as received from the engine.
  PropertyType? get engineMax => propertyMetadata?.maxValue;

  /// The overridden minimum value the controlled properties can reach. If null, [engineMin] will be used.
  PropertyType? get overrideMin => null;

  /// The overridden maximum value the controlled properties can reach. If null, [engineMax] will be used.
  PropertyType? get overrideMax => null;

  /// The property's possible enum values as received from the engine.
  List<String> get propertyEnumValues => propertyMetadata?.enumValues ?? [];

  /// The number of properties this is controlling.
  int get propertyCount => _properties.length;

  /// The type of modify operation this widget uses to apply deltas.
  PropertyModifyOperation get modifyOperation => const AddOperation();

  /// Extra meta-data properties to pass to conversion functions operating on this widget's value.
  Map<String, dynamic>? get conversionMetadata => null;

  @override
  void initState() {
    super.initState();

    _propertyManager = Provider.of<UnrealPropertyManager>(context, listen: false);
    _trackAllProperties();
  }

  @override
  void dispose() {
    for (final WidgetControlledUnrealProperty<PropertyType>? property in _properties) {
      if (property != null) {
        _propertyManager.stopTrackingProperty(property.trackedId, property.changedCallback);
      }
    }

    for (final WidgetControlledUnrealProperty<bool>? property in _enableProperties) {
      if (property != null) {
        _propertyManager.stopTrackingProperty(property.trackedId, property.changedCallback);
      }
    }

    super.dispose();
  }

  @override
  void didUpdateWidget(WidgetType oldWidget) {
    super.didUpdateWidget(oldWidget);

    _trackAllPropertiesIfChanged();
  }

  /// Call this to indicate that the user has started interacting with the widget.
  /// This will be automatically called for you as soon as any properties change, but you can safely call it earlier.
  /// A [description] will be automatically generated if null is passed.
  /// Returns true if a new transaction was started.
  bool beginTransaction([String? description]) {
    if (_bIsInTransaction) {
      return false;
    }

    if (_propertyManager
        .beginTransaction(description ?? AppLocalizations.of(context)!.transactionEditProperty(propertyLabel))) {
      _bIsInTransaction = true;
      return true;
    }

    return false;
  }

  /// Call this to indicate that the user has finished interacting with the widget.
  void endTransaction() {
    if (!_bIsInTransaction) {
      return;
    }

    _bIsInTransaction = false;
    _propertyManager.endTransaction();
  }

  /// Generate a list of [DeltaWidgetValueData] containing the data needed to display each control in the widget.
  /// Values may be null if the value hasn't been received from the engine yet.
  List<DeltaWidgetValueData<PropertyType>?> makeValueDataList() =>
      _bIsReady ? _properties.map((property) => property?.toValueData()).toList() : [];

  /// Called when the user changes controlled values.
  void handleOnChangedByUser(List<PropertyType> deltaValues) {
    modifyProperties(modifyOperation, values: deltaValues);
  }

  /// Called when the user presses the reset button.
  void handleOnResetByUser() {
    if (!_propertyManager.beginTransaction(AppLocalizations.of(context)!.transactionResetProperty(propertyLabel))) {
      return;
    }

    for (final WidgetControlledUnrealProperty<PropertyType>? property in _properties) {
      if (property != null) {
        _propertyManager.modifyTrackedPropertyValue(property.trackedId, const ResetOperation());
      }
    }

    if (widget.enableProperties != null) {
      for (final WidgetControlledUnrealProperty<bool>? property in _enableProperties) {
        if (property != null) {
          _propertyManager.modifyTrackedPropertyValue(property.trackedId, const ResetOperation());
        }
      }
    }

    _propertyManager.endTransaction();
  }

  /// Return a SingleSharedValue indicating whether there is a single value shared by all controlled properties.
  /// If there are no controlled values, the SingleSharedValue will instead contain [defaultValue].
  SingleSharedValue<PropertyType> getSingleSharedValue([PropertyType? defaultValue]) {
    // Get the value of all non-null properties
    final List<PropertyType?> controlledValues = _properties.map((property) => property?.value).toList();
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

  /// Apply an [operation] to each property this controls by the amounts specified in [values].
  /// If [values] is null, this is treated as if it was a list of null values matching the number of properties.
  /// If [bIgnoreLimits] is true, the min and max for the widget will be ignored.
  void modifyProperties(PropertyModifyOperation operation, {List<dynamic>? values, bool bIgnoreLimits = false}) {
    assert(values == null || values.length == _properties.length);

    if (!_bIsInTransaction) {
      if (!beginTransaction()) {
        // If we can't start a transaction, ignore user input or we'll get out of sync with the engine
        return;
      }
    }

    // Enable the property in the engine
    for (final WidgetControlledUnrealProperty<bool>? property in _enableProperties) {
      if (property != null) {
        if (!_propertyManager.getTrackedPropertyValue(property.trackedId)) {
          _propertyManager.modifyTrackedPropertyValue(property.trackedId, const SetOperation(), deltaValue: true);
        }
      }
    }

    for (int propertyIndex = 0; propertyIndex < _properties.length; ++propertyIndex) {
      final WidgetControlledUnrealProperty<PropertyType>? property = _properties[propertyIndex];

      if (property != null) {
        _propertyManager.modifyTrackedPropertyValue(
          property.trackedId,
          operation,
          deltaValue: values?[propertyIndex],
          minMaxBehaviour: bIgnoreLimits ? PropertyMinMaxBehaviour.ignore : widget.minMaxBehaviour,
          overrideMin: overrideMin,
          overrideMax: overrideMax,
        );
      }
    }

    if (widget.onChangedByUser != null) {
      widget.onChangedByUser!();
    }
  }

  /// Check whether the widget's list of properties has changed, and if so, update our list of tracked properties.
  void _trackAllPropertiesIfChanged() {
    bool bHavePropertiesChanged = widget.unrealProperties.length != _lastUnrealProperties.length ||
        (widget.enableProperties?.length ?? 0) != _lastEnableProperties.length;

    if (!bHavePropertiesChanged) {
      bHavePropertiesChanged = !listEquals(widget.unrealProperties, _lastUnrealProperties);
    }

    if (!bHavePropertiesChanged && widget.enableProperties != null) {
      bHavePropertiesChanged = !listEquals(widget.enableProperties!, _lastEnableProperties);
    }

    if (!bHavePropertiesChanged) {
      return;
    }

    _trackAllProperties();
  }

  /// Resets the internal list of properties and starts tracking them again.
  void _trackAllProperties() {
    _bIsReady = false;
    ++_latestTrackedPropertyVersion;

    // The tracked property version when we started this function, so we can ignore callbacks later in the function
    // if they come back too late to be relevant.
    final int trackedPropertyVersion = _latestTrackedPropertyVersion;

    // Futures that will complete when each property is exposed.
    final List<Future<void>> exposeFutures = [];

    _trackRelevantProperties<PropertyType>(
      trackedProperties: _properties,
      oldProperties: _lastUnrealProperties,
      newProperties: widget.unrealProperties,
      exposeFutures: exposeFutures,
      callback: _handleOnManagedValueChanged,
    );

    _trackRelevantProperties<bool>(
      trackedProperties: _enableProperties,
      oldProperties: _lastEnableProperties,
      newProperties: widget.enableProperties ?? [],
      exposeFutures: exposeFutures,
      callback: _handleOnEnableValueChanged,
    );

    // Wait for all properties to be subscribed, then mark the widget as ready to be used
    Future.wait(exposeFutures).then((_) {
      if (_latestTrackedPropertyVersion != trackedPropertyVersion || !mounted) {
        return;
      }

      _onPropertiesReady();
    }).onError((error, stackTrace) {
      _log.severe('Error while waiting for Unreal widget properties', error, stackTrace);
    });
  }

  /// Given a list of [trackedProperties], a corresponding list of [oldProperties] that are being tracked, and a list
  /// of [newProperties] that are about to be tracked, stop tracking any properties that are no longer relevant and
  /// start any properties that are newly relevant. [exposeFutures] will be filled with futures for each newly-tracked
  /// property that will complete when the property is exposed, and [callback] will be called in the future whenever a
  /// property's value changes.
  void _trackRelevantProperties<T>({
    required List<WidgetControlledUnrealProperty<T>?> trackedProperties,
    required List<UnrealProperty> oldProperties,
    required List<UnrealProperty> newProperties,
    required List<Future<void>> exposeFutures,
    required Function(int trackedPropertyVersion, int index, dynamic newValue) callback,
  }) {
    assert(oldProperties.length == trackedProperties.length);

    // Stop tracking all properties. We either aren't tracking them anymore, or we may need to update their
    // propertyIndex if the number tracked has changed size, in which case we need to recreate the callback anyway.
    for (int propertyIndex = 0; propertyIndex < oldProperties.length; ++propertyIndex) {
      final WidgetControlledUnrealProperty<T>? property = trackedProperties[propertyIndex];

      if (property != null) {
        _propertyManager.stopTrackingProperty(property.trackedId, property.changedCallback);
      }
    }

    oldProperties.clear();
    oldProperties.addAll(newProperties);

    trackedProperties.length = newProperties.length;

    // Track each property we want to control with the remote property manager.
    for (int propertyIndex = 0; propertyIndex < newProperties.length; ++propertyIndex) {
      final UnrealProperty property = newProperties[propertyIndex];

      // Create a callback function for this property
      changedCallback(newValue) => callback(_latestTrackedPropertyVersion, propertyIndex, newValue);

      // Start tracking the property in the property manager
      final TrackedPropertyId trackedId =
          _propertyManager.trackProperty(property, changedCallback, conversionMetadata: conversionMetadata);

      final newProperty = WidgetControlledUnrealProperty<T>(
        trackedId: trackedId,
        propertyIndex: propertyIndex,
        changedCallback: changedCallback,
        value: _propertyManager.getTrackedPropertyValue(trackedId),
        ownerName: property.objectName,
      );
      trackedProperties[propertyIndex] = newProperty;

      // Add to the list of properties we're waiting for
      final Future<void> exposeFuture = _propertyManager.waitForProperty(trackedId);
      exposeFutures.add(exposeFuture);

      exposeFuture.then((_) {
        newProperty.value = _propertyManager.getTrackedPropertyValue(trackedId);
      }).onError((error, stackTrace) {
        _log.severe(
            'Error while exposing property ${property.propertyName} on ${property.objectName}', error, stackTrace);
      });
    }
  }

  /// Called when all properties have been subscribed and the widget is ready to be interacted with.
  void _onPropertiesReady() {
    if (_properties.isNotEmpty) {
      // Properties could differ in their metadata, but for now we'll assume they share the same values
      final WidgetControlledUnrealProperty<PropertyType>? firstProperty = _properties[0];

      if (firstProperty == null) {
        assert(false, 'If an Unreal widget is ready, all properties should be non-null');
        return;
      }

      _propertyMetadata = _propertyManager.getTrackedPropertyMetaData(firstProperty.trackedId);
    }

    setState(() {
      _bIsReady = true;
    });
  }

  /// Called when the property manager reports a [newValue] for the property with the given [index] in the widget.
  /// If [trackedPropertyVersion] does not match [_latestTrackedPropertyVersion], the change will be ignored.
  void _handleOnManagedValueChanged(int trackedPropertyVersion, int index, dynamic newValue) {
    if (trackedPropertyVersion != _latestTrackedPropertyVersion) {
      return;
    }

    setState(() {
      _properties[index]?.value = newValue;
    });
  }

  /// Called when an enable value for one of the properties changes.
  void _handleOnEnableValueChanged(int trackedPropertyVersion, int index, dynamic newValue) {}
}
