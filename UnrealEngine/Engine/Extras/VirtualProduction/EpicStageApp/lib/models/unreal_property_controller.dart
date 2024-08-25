// Copyright Epic Games, Inc. All Rights Reserved.

import 'dart:collection';

import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:logging/logging.dart';
import 'package:provider/provider.dart';
import 'package:flutter_gen/gen_l10n/app_localizations.dart';

import '../widgets/elements/delta_widget_base.dart';
import '../widgets/elements/unreal_widget_base.dart';
import 'property_modify_operations.dart';
import 'unreal_property_manager.dart';
import 'unreal_types.dart';
import 'navigator_keys.dart';

final _log = Logger('UnrealPropertyController');

/// A Controller that allows modifying a list of unreal properties sharing the same name and type of all currently
/// selected actors.
class UnrealPropertyController<PropertyType> with ChangeNotifier {
  /// Construct a property manager and register it with the connection manager.
  UnrealPropertyController(BuildContext context, {bool bShouldInitTransaction = true})
      : _propertyManager = Provider.of<UnrealPropertyManager>(context, listen: false),
        _bShouldInitTransaction = bShouldInitTransaction {}

  /// The property manager this widget uses to update its properties.
  final UnrealPropertyManager _propertyManager;

  /// Whether or not to initiate transactions internally or to allow transactions be initiated externally by calling
  /// [beginTransaction] and [endTransaction] respectively to start & end a transaction.
  final bool _bShouldInitTransaction;

  /// List of properties currently controlled by the widget.
  /// We can have multiple properties due to multi-selection.
  /// properties are of the same name and type.
  List<WidgetControlledUnrealProperty<PropertyType>?> _properties = [];

  /// Unmodifiable getter getter for [_properties].
  UnmodifiableListView<WidgetControlledUnrealProperty<PropertyType>?> get properties =>
      UnmodifiableListView(_properties);

  /// A list of properties for enabling widget properties current controlled by the widget.
  final List<WidgetControlledUnrealProperty<bool>?> _enableProperties = [];

  /// The last list of properties that were passed to the widget. This is stored so we can compare the list on rebuild
  /// and register/unregister properties when the list changes.
  final List<UnrealProperty> _lastUnrealProperties = [];

  /// The last list of enable properties that were passed to the widget. This is stored so we can compare the list on
  /// rebuild and register/unregister properties when the list changes.
  final List<UnrealProperty> _lastEnableProperties = [];

  /// Property's metadata as received from the engine.
  UnrealPropertyMetadata? _propertyMetadata;

  /// This is incremented whenever the set of tracked properties changes so we can ignore any
  /// callbacks from previous versions. For example, if the user rapidly changes the list multiple times, we may get
  /// changed value or newly tracked value callbacks for properties that we no longer care about.
  int _latestTrackedPropertyVersion = 0;

  /// Whether the properties' values are ready to be display to the user.
  bool _bIsReady = false;

  /// Whether the widget is actively in a transaction or not.
  bool _bIsInTransaction = false;

  /// The label of the property to show to users.
  String get propertyLabel => (_propertyMetadata?.displayName ?? '');

  /// The property's minimum value as received from the engine.
  PropertyType? get engineMin => _propertyMetadata?.minValue;

  /// The property's maximum value as received from the engine.
  PropertyType? get engineMax => _propertyMetadata?.maxValue;

  /// The overridden minimum value the controlled properties can reach. If null, [engineMin] will be used.
  PropertyType? get overrideMin => null;

  /// The overridden maximum value the controlled properties can reach. If null, [engineMax] will be used.
  PropertyType? get overrideMax => null;

  /// The property's possible enum values as received from the engine.
  List<String> get propertyEnumValues => _propertyMetadata?.enumValues ?? [];

  /// The type of modify operation this widget uses to apply deltas.
  PropertyModifyOperation get modifyOperation => const AddOperation();

  /// Extra meta-data properties to pass to conversion functions operating on this widget's value.
  Map<String, dynamic>? get conversionMetadata => null;

  /// Call this to indicate that the user has started interacting with the widget.
  /// This will be automatically called for you as soon as any properties change, but you can safely call it earlier.
  /// A [description] will be automatically generated if null is passed.
  /// Returns true if a new transaction was started.
  bool beginTransaction([String? description]) {
    if (_bIsInTransaction) {
      return false;
    }

    if (_propertyManager.beginTransaction(
        description ?? AppLocalizations.of(rootNavigatorKey.currentContext!)!.transactionEditProperty(propertyLabel))) {
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
  void handleOnResetByUser(List<UnrealProperty>? enableProperties) {
    if (!_propertyManager.beginTransaction(
        AppLocalizations.of(rootNavigatorKey.currentContext!)!.transactionResetProperty(propertyLabel))) {
      return;
    }

    for (final WidgetControlledUnrealProperty<PropertyType>? property in _properties) {
      if (property != null) {
        _propertyManager.modifyTrackedPropertyValue(property.trackedId, const ResetOperation());
      }
    }

    if (enableProperties != null) {
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
  /// If [_bShouldInitTransaction] is false, we ought to call [beginTransaction] before calling
  void modifyProperties(
    PropertyModifyOperation operation, {
    List<dynamic>? values,
    bool bIgnoreLimits = false,
    PropertyMinMaxBehaviour minMaxBehaviour = PropertyMinMaxBehaviour.clamp,
    void Function()? onChangedByUser,
  }) {
    assert(values == null || values.length == _properties.length);

    for (int index = 0; index < _properties.length; index++) {
      modifyProperty(
        operation,
        index,
        value: values?[index],
        bIgnoreLimits: bIgnoreLimits,
        minMaxBehaviour: minMaxBehaviour,
      );
    }

    if (onChangedByUser != null) {
      onChangedByUser();
    }
  }

  /// Apply an [operation] to a single property specified by the property [index] this controls by the amounts specified
  /// in [value].
  /// If [value] is null, this is treated as null and applied to matching property [index].
  /// If [bIgnoreLimits] is true, the min and max for the widget will be ignored.
  /// If [_bShouldInitTransaction] is false, we ought to call [beginTransaction] before calling
  void modifyProperty(
    PropertyModifyOperation operation,
    int index, {
    dynamic value,
    bool bIgnoreLimits = false,
    PropertyMinMaxBehaviour minMaxBehaviour = PropertyMinMaxBehaviour.clamp,
  }) {
    if (_bShouldInitTransaction == true) {
      if (!_bIsInTransaction) {
        if (!beginTransaction()) {
          // If we can't start a transaction, ignore user input or we'll get out of sync with the engine
          return;
        }
      }
    }

    try {
      //Enable the property in the engine
      if (index < _enableProperties.length) {
        final WidgetControlledUnrealProperty<bool>? enableProperty = _enableProperties[index];

        if (enableProperty != null) {
          if (!_propertyManager.getTrackedPropertyValue(enableProperty.trackedId)) {
            _propertyManager.modifyTrackedPropertyValue(enableProperty.trackedId, const SetOperation(),
                deltaValue: true);
          }
        }
      }

      final WidgetControlledUnrealProperty<PropertyType>? property = _properties[index];

      _propertyManager.modifyTrackedPropertyValue(
        property!.trackedId,
        operation,
        deltaValue: value,
        minMaxBehaviour: bIgnoreLimits ? PropertyMinMaxBehaviour.ignore : minMaxBehaviour,
        overrideMin: overrideMin,
        overrideMax: overrideMax,
      );
    } on Exception catch (e) {
      _log.severe('failed to modify tracked property', e);
    }
  }

  /// Check whether the widget's list of properties has changed, and if so, update our list of tracked properties.
  void trackAllPropertiesIfChanged(List<UnrealProperty> unrealProperties, List<UnrealProperty>? enableProperties) {
    bool bHavePropertiesChanged = unrealProperties.length != _lastUnrealProperties.length ||
        (enableProperties?.length ?? 0) != _lastEnableProperties.length;

    if (!bHavePropertiesChanged) {
      bHavePropertiesChanged = !listEquals(unrealProperties, _lastUnrealProperties);
    }

    if (!bHavePropertiesChanged && enableProperties != null) {
      bHavePropertiesChanged = !listEquals(enableProperties, _lastEnableProperties);
    }

    if (!bHavePropertiesChanged) {
      return;
    }

    trackAllProperties(unrealProperties, enableProperties);
  }

  /// Resets the internal list of properties and starts tracking them again.
  void trackAllProperties(List<UnrealProperty> unrealProperties, [List<UnrealProperty>? enableProperties]) {
    _bIsReady = false;
    ++_latestTrackedPropertyVersion;

    // The tracked property version when we started this function, so we can ignore callbacks later in the function
    // if they come back too late to be relevant.
    final int trackedPropertyVersion = _latestTrackedPropertyVersion;

    // Futures that will complete when each property is exposed.
    final List<Future<void>> exposeFutures = [];

    trackRelevantProperties<PropertyType>(
      trackedProperties: _properties,
      oldProperties: _lastUnrealProperties,
      newProperties: unrealProperties,
      exposeFutures: exposeFutures,
      callback: _handleOnManagedValueChanged,
    );

    trackRelevantProperties<bool>(
      trackedProperties: _enableProperties,
      oldProperties: _lastEnableProperties,
      newProperties: enableProperties ?? [],
      exposeFutures: exposeFutures,
      callback: _handleOnEnableValueChanged,
    );

    // Wait for all properties to be subscribed, then mark the widget as ready to be used
    Future.wait(exposeFutures).then((_) {
      if (_latestTrackedPropertyVersion != trackedPropertyVersion) {
        return;
      }

      _onPropertiesReady();
      notifyListeners();
    }).onError((error, stackTrace) {
      _log.severe('Error while waiting for Unreal widget properties', error, stackTrace);
    });
  }

  /// Given a list of [trackedProperties], a corresponding list of [oldProperties] that are being tracked, and a list
  /// of [newProperties] that are about to be tracked, stop tracking any properties that are no longer relevant and
  /// start any properties that are newly relevant. [exposeFutures] will be filled with futures for each newly-tracked
  /// property that will complete when the property is exposed, and [callback] will be called in the future whenever a
  /// property's value changes.
  void trackRelevantProperties<T>({
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
    _bIsReady = true;
    notifyListeners();
  }

  /// Called when the property manager reports a [newValue] for the property with the given [index] in the widget.
  /// If [trackedPropertyVersion] does not match [_latestTrackedPropertyVersion], the change will be ignored.
  void _handleOnManagedValueChanged(int trackedPropertyVersion, int index, dynamic newValue) {
    if (trackedPropertyVersion != _latestTrackedPropertyVersion) {
      return;
    }

    _properties[index]?.value = newValue;
    notifyListeners();
  }

  /// Called when an enable value for one of the properties changes.
  void _handleOnEnableValueChanged(int trackedPropertyVersion, int index, dynamic newValue) {}

  @override
  void dispose() {
    for (final WidgetControlledUnrealProperty<PropertyType>? property in properties) {
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
}
