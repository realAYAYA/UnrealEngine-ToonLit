// Copyright Epic Games, Inc. All Rights Reserved.

import 'dart:async';

import 'package:flutter/material.dart';
import 'package:logging/logging.dart';
import 'package:provider/provider.dart';

import '../utilities/unreal_utilities.dart';
import './engine_connection.dart';
import './property_modify_operations.dart';
import './unreal_types.dart';
import 'unreal_transaction_manager.dart';

final _log = Logger('UnrealPropertyManager');

/// Callback for when a tracked property changes.
typedef PropertyChangeCallback = Function(dynamic newValue);

/// Manages all in-engine properties that are being controlled by the app, including communication with the engine,
/// caching of values, and notifying of changes.
class UnrealPropertyManager {
  /// Construct a property manager and register it with the connection manager.
  UnrealPropertyManager(BuildContext context)
      : _connectionManager = Provider.of<EngineConnectionManager>(context, listen: false),
        _transactionManager = Provider.of<UnrealTransactionManager>(context, listen: false) {
    _connectionManager.registerConnectionStateListener(_onConnectionStateChanged);
    _connectionManager.registerMessageListener('PresetFieldsChanged', _onPresetFieldsChanged);
    _onConnectionStateChanged(_connectionManager.connectionState);
  }

  /// Rate at which we send property changes back to the engine.
  static const Duration _tickRate = Duration(milliseconds: 100);

  /// The connection manager we'll use to communicate with the engine.
  final EngineConnectionManager _connectionManager;

  /// The transaction manager used to maintain editor undo/redo state on our property changes.
  final UnrealTransactionManager _transactionManager;

  /// A map from [TrackedPropertyId] to the information we're tracking about the property.
  final Map<TrackedPropertyId, _TrackedProperty> _trackedProperties = {};

  /// Lookup table from a property's preset label to our internal ID for it.
  final Map<String, TrackedPropertyId> _propertyIdsByLabel = {};

  /// Editor transactions that had activity this tick.
  /// If the last entry in this list is active, it's the active transaction in the engine.
  final List<_EngineTransaction> _currentTickTransactions = [];

  /// Changes to be sent to the engine pending the creation of our transient preset.
  final List<dynamic> _messagesPendingPreset = [];

  /// IDs of tracked properties for which we need to send an expose request.
  final Set<TrackedPropertyId> _propertiesPendingExpose = {};

  /// IDs of tracked properties for which we need to send an unexpose request.
  final Set<TrackedPropertyId> _propertiesPendingUnexpose = {};

  /// Timer used to regularly send changes back to the engine while connected.
  Timer? _tickTimer;

  /// The name of the transient preset we're using to manage properties, or null if it doesn't exist yet.
  String? _transientPresetName;

  /// The future which will return when the transient preset has been created, or null if we haven't started creating
  /// a transient preset yet.
  Future<String>? _transientPresetNameFuture;

  /// The ID of the last newly-tracked property.
  TrackedPropertyId _lastTrackedId = const TrackedPropertyId.invalid();

  /// The sequence number to send to the engine with the next batch of changes.
  int _sequenceNumber = 0;

  /// The ID of the active transaction that this created, or [UnrealTransactionManager.invalidId] if there isn't an
  /// active transaction.
  int _activeTransactionId = UnrealTransactionManager.invalidId;

  /// Start tracking updates about [property] between the engine and the app, and call [changeCallback] when
  /// the engine sends a message about a change.
  /// Returns a future which will provide the tracking ID of the property if it is successfully registered.
  /// If the property is already tracked with the given callback, this will just return the existing property ID.
  /// If provided, [conversionMetadata] will be stored and provided to functions converting to/from JSON data. If the
  /// property is already tracked, its metadata will be replaced by the new value.
  TrackedPropertyId trackProperty(UnrealProperty property, PropertyChangeCallback changeCallback,
      {Map<String, dynamic>? conversionMetadata}) {
    // Check if this property is already tracked
    for (MapEntry<TrackedPropertyId, _TrackedProperty> trackedEntry in _trackedProperties.entries) {
      final _TrackedProperty trackedProperty = trackedEntry.value;

      // We're already tracking this property
      if (trackedProperty.property == property) {
        if (conversionMetadata != null) {
          // Update conversion metadata
          trackedProperty.conversionMetadata = conversionMetadata;
        }

        trackedProperty.changeCallbacks.add(changeCallback);

        if (trackedProperty.bIsPendingUnexpose) {
          if (_propertiesPendingUnexpose.remove(trackedEntry.key)) {
            // The property was waiting to be unexposed, but we hadn't sent the request yet, so forget about the
            // unexpose entirely.
            trackedProperty.bIsPendingUnexpose = false;
          } else {
            // We already sent an unexpose request (or will do so when the exposeCompleter completes).
            // We can't guarantee the order in which the engine will receive our requests, so we need to wait for
            // unexpose and then send another expose request.
            // This completer will complete as soon as the new expose request is received.
            trackedProperty.exposeCompleter ??= Completer<void>();
          }

          return trackedEntry.key;
        }

        // Property is already good to go, so just return its existing ID
        return trackedEntry.key;
      }
    }

    // We aren't tracking the property yet, so we need to register with the engine first.
    final TrackedPropertyId newId = _getNextTrackedPropertyId();
    _trackedProperties[newId] = _TrackedProperty(
      property: property,
      initialCallbacks: [changeCallback],
      conversionMetadata: conversionMetadata ?? {},
    );

    _exposeProperty(newId);

    return newId;
  }

  /// Returns true if the property with the given [propertyId] is exposed, otherwise false.
  bool isPropertyExposed(TrackedPropertyId propertyId) {
    final _TrackedProperty? property = _trackedProperties[propertyId];
    if (property == null) {
      return false;
    }

    return property.exposeCompleter == null;
  }

  /// Returns a future that will complete when the property with the given [propertyId] is exposed (or re-exposed).
  Future<void> waitForProperty(TrackedPropertyId propertyId) {
    final _TrackedProperty? property = _trackedProperties[propertyId];
    if (property == null) {
      return Future.error('Property with id $propertyId does not exist');
    }

    // If the property is waiting for expose, return that future. Otherwise, it's already exposed.
    return property.exposeCompleter?.future ?? Future.value();
  }

  /// Stop tracking the property with ID [propertyId]. [changeCallback] must be the same callback that was initially
  /// passed in to [trackProperty].
  void stopTrackingProperty(TrackedPropertyId propertyId, PropertyChangeCallback changeCallback) async {
    final _TrackedProperty? property = _trackedProperties[propertyId];
    if (property == null) {
      return;
    }

    if (!property.changeCallbacks.remove(changeCallback)) {
      _log.warning('Tried to stop tracking ${property.label}, but there was no callback to remove');
    }

    // If nobody is watching this property anymore, unsubscribe from updates to it
    if (property.changeCallbacks.isEmpty) {
      if (property.bIsPendingUnexpose) {
        // Property is already pending unexpose, so we don't need to send another request
        return;
      }

      // As soon as this property is exposed, or immediately if it's already exposed, flag it to be unexposed.
      property.bIsPendingUnexpose = true;

      queueForUnexpose() {
        _propertiesPendingUnexpose.add(propertyId);
      }

      if (property.exposeCompleter == null) {
        // Immediately queue the property to be unexposed if it's already exposed
        queueForUnexpose();
      } else if (_propertiesPendingExpose.remove(propertyId)) {
        // Property was queued for expose, but we hadn't actually sent the message, so just drop it
        property.exposeCompleter?.complete();
        property.exposeCompleter = null;
        _trackedProperties.remove(propertyId);
      } else {
        // Wait until it's exposed, then unexpose it
        waitForProperty(propertyId).then((_) {
          if (property.changeCallbacks.isNotEmpty) {
            // In the time we were waiting for the property to be exposed, somebody started listening for it, so we
            // don't actually want to unexpose it anymore.
            property.bIsPendingUnexpose = false;
            return;
          }

          queueForUnexpose();
        });
      }
    }
  }

  /// Get the value of a tracked property, or null if the property isn't tracked/ready.
  dynamic getTrackedPropertyValue(TrackedPropertyId propertyId) {
    return _getReadyTrackedProperty(propertyId)?.value;
  }

  /// Get the metadata for a tracked property, or null if the property isn't tracked/ready.
  dynamic getTrackedPropertyMetaData(TrackedPropertyId propertyId) {
    return _getReadyTrackedProperty(propertyId)?.metadata;
  }

  /// Start a tracked property transaction.
  /// Returns false if there was already an active transaction.
  /// Note that this creates a transaction within the [UnrealPropertyManager] only -- the actual transaction will not be
  /// created until updates are ready to send on the next tick. If you want to immediately create a transaction, use
  /// [UnrealTransactionManager.beginTransaction] instead.
  bool beginTransaction(String description) {
    final _EngineTransaction? activeTransaction = _getActiveTransaction();

    if (activeTransaction != null) {
      _log.warning('Tried to start transaction, but one was already in progress');
      return false;
    }

    _currentTickTransactions.add(_EngineTransaction(description: description));

    return true;
  }

  /// Mark the current tracked property transaction as complete.
  /// Returns false if there was no active transaction to complete.
  /// Note that this should only be used to end a transaction started using [UnrealPropertyManager.beginTransaction].
  bool endTransaction() {
    final _EngineTransaction? activeTransaction = _getActiveTransaction();

    if (activeTransaction == null) {
      return false;
    }

    activeTransaction.bIsActive = false;

    return true;
  }

  /// Modify the value of a tracked property.
  /// The property will be retrieved based on the [propertyId], and its current value will by modified by the amount in
  /// [deltaValue] using the provided [operation].
  /// [deltaValue] may be optional depending on the operation.
  /// [beginTransaction] should be called before this if [isEditorTransactionActive] returns false.
  /// If [minMaxBehaviour] behaviour is provided, it will change how the value is handled when it passes the engine's
  /// min/max values for the property.
  /// If [overrideMin] and/or [overrideMax] are provided, they will be used instead of the min/max values provided by
  /// the engine when applying the [minMaxBehaviour].
  /// Returns the new value of the property, or null if the property isn't tracked.
  dynamic modifyTrackedPropertyValue(
    TrackedPropertyId propertyId,
    PropertyModifyOperation operation, {
    dynamic deltaValue,
    PropertyMinMaxBehaviour minMaxBehaviour = PropertyMinMaxBehaviour.clamp,
    dynamic overrideMin = null,
    dynamic overrideMax = null,
  }) {
    final _EngineTransaction? activeTransaction = _getActiveTransaction();

    if (activeTransaction == null) {
      throw Exception('Called modifyTrackedPropertyValue with no active transaction. '
          'Call beginEditorTransaction first.');
    }

    final _TrackedProperty? property = _getReadyTrackedProperty(propertyId);
    if (property == null) {
      return null;
    }

    final newChange = _TrackedPropertyChange(operation: operation, deltaValue: deltaValue);

    // Keep the value in the range provided by engine, if necessary
    switch (minMaxBehaviour) {
      case PropertyMinMaxBehaviour.loop:
        newChange.loop(
          referenceValue: property.value,
          min: overrideMin ?? property.metadata.minValue,
          max: overrideMax ?? property.metadata.maxValue,
        );
        break;

      case PropertyMinMaxBehaviour.clamp:
        // Clamp the change as appropriate, and if the result is that we won't actually change anything, early out.
        final bool bIsMeaningfulChange = newChange.clamp(
          referenceValue: property.value,
          min: overrideMin ?? property.metadata.minValue,
          max: overrideMax ?? property.metadata.maxValue,
        );

        if (!bIsMeaningfulChange) {
          return property.value;
        }
        break;

      case PropertyMinMaxBehaviour.ignore:
        break;
    }

    // Update the list of changes to the property this frame, combining with existing changes if possible
    final List<_TrackedPropertyChange> changesThisFrame = activeTransaction.changes.putIfAbsent(propertyId, () => []);

    if (changesThisFrame.isNotEmpty && changesThisFrame.last.canCombine(newChange)) {
      // We can combine this change with the previous one, so we don't need to add a new one
      changesThisFrame.last.combine(newChange);
    } else {
      changesThisFrame.add(newChange);
    }

    // Update the predicted value
    property.value = newChange.apply(property.value);
    return property.value;
  }

  /// Get the transaction that is currently active in the engine, if any.
  _EngineTransaction? _getActiveTransaction() {
    if (_currentTickTransactions.isNotEmpty) {
      final _EngineTransaction latestTransaction = _currentTickTransactions.last;
      if (latestTransaction.bIsActive) {
        return latestTransaction;
      }
    }

    return null;
  }

  /// Marks an Unreal Engine property with the given [propertyId] to be exposed in the next batch of requests.
  void _exposeProperty(TrackedPropertyId propertyId) {
    final _TrackedProperty? property = _trackedProperties[propertyId];

    if (property == null) {
      assert(false, '_exposeProperty should only be called with existing tracked properties');
      return;
    }

    _propertiesPendingExpose.add(propertyId);

    // If the property didn't already have a completer, create one so we know we're waiting for expose
    property.exposeCompleter ??= Completer();
  }

  /// Create a request to the engine to expose a property with the given [propertyId] on the transient preset.
  UnrealHttpRequestWithCallback _createExposePropertyRequest(TrackedPropertyId propertyId) {
    final _TrackedProperty? property = _trackedProperties[propertyId];

    assert(property != null);

    return UnrealHttpRequestWithCallback(
      UnrealHttpRequest(
        url: '/remote/preset/$_transientPresetName/expose/property',
        verb: 'PUT',
        body: {
          'ObjectPath': property!.property.objectPath,
          'PropertyName': property.property.propertyName,
          'EnableEditCondition': false,
        },
      ),
      (response) => _onExposePropertyResponse(propertyId, response)
          .onError((error, stackTrace) => _onExposePropertyError(error, stackTrace, propertyId, property)),
    );
  }

  /// Create a request to the engine to unexpose a tracked property with the given [propertyId] on the transient
  /// preset.
  UnrealHttpRequestWithCallback _createUnexposePropertyRequest(TrackedPropertyId propertyId) {
    final _TrackedProperty? property = _trackedProperties[propertyId];

    assert(property != null);

    return UnrealHttpRequestWithCallback(
      UnrealHttpRequest(
        url: '/remote/preset/$_transientPresetName/unexpose/property/${property!.label}',
        verb: 'PUT',
      ),
      (response) => _onUnexposePropertyResponse(propertyId, response),
    );
  }

  /// Called when we receive a response from the engine after exposing a property.
  /// Returns a future which will provide true if the property was exposed, false if we already stopped tracking it,
  /// and an error otherwise.
  Future<void> _onExposePropertyResponse(TrackedPropertyId propertyId, UnrealHttpResponse response) {
    final _TrackedProperty? property = _trackedProperties[propertyId];
    if (property == null) {
      // Property no longer exists. We may have already unregistered it.
      return Future.value();
    }

    if (response.code != HttpResponseCode.ok) {
      return Future.error('Response code ${response.code}');
    }

    final dynamic description = response.body['ExposedPropertyDescription'];
    if (description == null) {
      return Future.error('Response did not contain a property description');
    }

    // Get the name used to look up the property
    final String? label = description['DisplayName'];
    if (label == null) {
      return Future.error('Response did not contain a property label');
    }

    // Get the name used to look up the property (unless we already have an override for it)
    final dynamic underlyingProperty = description['UnderlyingProperty'];
    String? typeName = property.property.typeNameOverride;
    List<String>? enumValues;

    if (typeName == null) {
      // No override type name provided, so get the type from the data we received
      final dynamic enumValuesData = underlyingProperty['Metadata']?['EnumValues'];
      if (enumValuesData != null) {
        // This is an enum type, so we can replace the specific enum type name provided by the engine with our internal
        // generic enum type.
        typeName = unrealEnumTypeName;
        enumValues = enumValuesData.toString().split(', ');
      } else {
        // Just use the type provided by the engine
        typeName = underlyingProperty?['Type'];
      }

      if (typeName == null) {
        return Future.error('Response did not contain a property type');
      }
    }

    if (!canConvertUnrealType(typeName)) {
      return Future.error('Unsupported Unreal type $typeName');
    }

    // Get the initial value of the property
    final dynamic rawValue = response.body['PropertyValues']?[0]?['PropertyValue'];
    if (rawValue == null) {
      return Future.error('Response did not contain a value');
    }

    // Get metadata if it was included
    final dynamic responseMetaData = description['Metadata'];
    final metaData = UnrealPropertyMetadata(
      displayName: underlyingProperty?['DisplayName'] ?? '',
      minValue: convertUnrealTypeJsonToDart(typeName, responseMetaData?['Min']),
      maxValue: convertUnrealTypeJsonToDart(typeName, responseMetaData?['Max']),
      enumValues: enumValues,
    );

    property.label = label;
    property.value = convertUnrealTypeJsonToDart(
      typeName,
      rawValue,
      conversionMetadata: property.conversionMetadata,
    );
    property.typeName = typeName;
    property.metadata = metaData;

    _propertyIdsByLabel[label] = propertyId;

    // Complete the exposure future, then null it to indicate that this is already exposed.
    property.exposeCompleter?.complete();
    property.exposeCompleter = null;

    return Future.value();
  }

  /// Called when an error happens while attempting to expose a property.
  void _onExposePropertyError<T>(
      T error, StackTrace stackTrace, TrackedPropertyId propertyId, _TrackedProperty property) {
    _log.severe(
        'Failed to expose property "${property.property.propertyName}" on ${property.property.objectPath}', error);

    _trackedProperties.remove(propertyId);
  }

  /// Called when the engine confirms that a property has been unexposed.
  void _onUnexposePropertyResponse(TrackedPropertyId propertyId, UnrealHttpResponse response) {
    final _TrackedProperty? property = _trackedProperties[propertyId];
    if (property == null) {
      return;
    }

    property.bIsPendingUnexpose = false;

    if (property.exposeCompleter != null) {
      if (response.code == HttpResponseCode.ok) {
        // We started tracking this property again after requesting unexpose, so we need to send a new expose request
        // instead of removing it
        _exposeProperty(propertyId);
      } else {
        // Treat the property as re-exposed since we failed to remove it anyway
        property.exposeCompleter!.complete();
        property.exposeCompleter = null;
      }
      return;
    }

    // If we aren't intending to re-expose this property right away and got an error, the property was likely unexposed
    // without our knowledge, so we can just forget about it

    _propertyIdsByLabel.remove(property.label);
    _trackedProperties.remove(propertyId);
  }

  /// Called each tick, when we're ready to send updates back to the engine.
  void _onTick(Timer timer) {
    if (_connectionManager.bIsInDemoMode) {
      return;
    }

    final List<dynamic> messages = _finalizeTickChanges();

    if (_transientPresetName == null) {
      // We don't have a transient preset to send the message to, so try to create it and queue the messages to be sent
      // once it's created.
      _getOrCreateTransientPreset();
      _messagesPendingPreset.addAll(messages);
      return;
    }

    if (_messagesPendingPreset.isNotEmpty) {
      // Update the waiting messages with the preset name
      for (final dynamic message in _messagesPendingPreset) {
        if (message['Parameters']?['PresetName'] != null) {
          message['Parameters']['PresetName'] = _transientPresetName;
        }
      }

      messages.insertAll(0, _messagesPendingPreset);
    }

    // Send the property update request
    if (messages.isNotEmpty) {
      _connectionManager.sendBatchedMessage(messages);
    }

    // Request expose/unexpose for any properties whose exposure states were changed
    final List<UnrealHttpRequestWithCallback> httpRequests = _generatePropertyExposureChangeRequests();
    if (httpRequests.isNotEmpty) {
      _connectionManager.sendBatchedHttpRequestWithCallbacks(httpRequests);
    }
  }

  /// Get the name of the transient preset we use to manage property subscriptions.
  /// If the transient preset doesn't exist yet, this will send a creation request.
  Future<String> _getOrCreateTransientPreset() {
    if (_transientPresetNameFuture == null) {
      // We don't already have a transient preset name (or one pending), so request a new one
      final Future<UnrealHttpResponse> futureRequest =
          _connectionManager.sendHttpRequest(const UnrealHttpRequest(url: '/remote/preset/transient', verb: 'PUT'));

      _transientPresetNameFuture = futureRequest.then(
        (response) {
          if (response.code != HttpResponseCode.ok) {
            return Future.error('Failed to create transient preset');
          }

          String? name = response.body?['Preset']?['Name'];
          if (name == null) {
            return Future.error('No preset name received from engine');
          }

          _onTransientPresetCreated(name);

          return Future.value(name);
        },
      );
    }

    return _transientPresetNameFuture!;
  }

  /// Called when we receive the name of the transient preset requested from the engine.
  void _onTransientPresetCreated(String name) {
    _transientPresetName = name;

    final List<dynamic> messages = [];

    // Subscribe to property events for this preset, and mark it to be destroyed when we disconnect
    messages.add(createUnrealWebSocketMessage('preset.register', {'PresetName': name}));
    messages.add(createUnrealWebSocketMessage('preset.transient.autodestroy', {'PresetName': name}));

    if (_messagesPendingPreset.isNotEmpty) {
      // Update the preset name in all waiting messages
      for (dynamic message in _messagesPendingPreset) {
        if (message['Parameters']?['PresetName'] != null) {
          message['Parameters']['PresetName'] = _transientPresetName;
        }
      }

      // Send the messages that were waiting
      messages.addAll(_messagesPendingPreset);
      _messagesPendingPreset.clear();
    }

    _connectionManager.sendBatchedMessage(messages);
  }

  /// Returns the ID that should be used for the next tracked property.
  TrackedPropertyId _getNextTrackedPropertyId() {
    // The last tracked ID is no longer tracked, so we can reuse it
    if (_lastTrackedId.isValid && !_trackedProperties.containsKey(_lastTrackedId)) {
      return _lastTrackedId;
    }

    // Increment until we find an unused ID
    final TrackedPropertyId startId = _lastTrackedId;
    do {
      _lastTrackedId = TrackedPropertyId(_lastTrackedId.id + 1);
    } while (_trackedProperties.containsKey(_lastTrackedId) && _lastTrackedId != startId);

    // If we've somehow exhausted all possible IDs, give up
    if (_lastTrackedId == startId) {
      throw Exception('All possible tracked property IDs exhausted');
    }

    return _lastTrackedId;
  }

  /// Get a tracked property if it's been exposed in the engine, or null if it isn't.
  /// You should generally call this instead of directly indexing into [_trackedProperties] so that you don't access
  /// values of a property that isn't ready.
  _TrackedProperty? _getReadyTrackedProperty(TrackedPropertyId propertyId) {
    final _TrackedProperty? property = _trackedProperties[propertyId];

    if (property?.value == null) {
      return null;
    }

    return property;
  }

  /// Convert all the value changes this tick into a list of messages to send to the server, and remember the change in
  /// each corresponding property for future rollback/replay.
  List<dynamic> _finalizeTickChanges() {
    if (_transactionManager.activeTransactionId != _activeTransactionId) {
      // If somebody else holds the active transaction, we can't make changes
      assert(_activeTransactionId == UnrealTransactionManager.invalidId);
      return [];
    }

    final List<dynamic> messages = [];

    // Send changes from this tick
    for (final _EngineTransaction transaction in _currentTickTransactions) {
      if (transaction.changes.isNotEmpty) {
        if (!transaction.bWasCreatedInEngine) {
          // Create the transaction in the engine before we send its changes
          final beginMessage = _transactionManager.createBeginTransactionMessage(
            transaction.description,
            // If the transaction ends prematurely, flag it to be re-created
            prematureEndCallback: () {
              _activeTransactionId = UnrealTransactionManager.invalidId;
              transaction.bWasCreatedInEngine = false;
            },
          );
          assert(beginMessage != null);
          messages.add(beginMessage);

          _activeTransactionId = _transactionManager.activeTransactionId;
          transaction.bWasCreatedInEngine = true;
        }

        // Add transaction changes
        for (final MapEntry<TrackedPropertyId, _TrackedProperty> propertyEntry in _trackedProperties.entries) {
          final List<_TrackedPropertyChange>? propertyChanges = transaction.changes[propertyEntry.key];

          if (propertyChanges == null) {
            continue;
          }

          final _TrackedProperty property = propertyEntry.value;
          property.recentTickChanges.add(_SequencedPropertyChangeRecord(
            sequenceNumber: _sequenceNumber,
            changes: List.from(propertyChanges),
          ));

          // Create a request for each change
          for (final _TrackedPropertyChange change in propertyChanges) {
            // Note that the preset name is "TEMP" if we don't yet have a transient preset. We'll update this for any
            // queued messages before sending them (in [_onTransientPresetCreated])
            messages.add(_createPropertyChangeMessage(
              propertyEntry.value,
              change,
              _activeTransactionId,
              _transientPresetName ?? 'TEMP',
            ));
          }
        }
      }

      if (!transaction.bIsActive && transaction.bWasCreatedInEngine) {
        // Transaction is no longer active, so end it in the engine
        final endMessage = _transactionManager.createEndTransactionMessage();
        assert(endMessage != null);
        messages.add(endMessage);

        _activeTransactionId = UnrealTransactionManager.invalidId;
      }
    }

    if (messages.isNotEmpty) {
      ++_sequenceNumber;
    }

    // Clear out the list of inactive transactions
    _currentTickTransactions.removeWhere((_EngineTransaction transaction) => !transaction.bIsActive);

    // Only one active transaction should exist at a time
    assert(_currentTickTransactions.length <= 1);

    // Clear out changes we just sent for the active transaction
    if (_currentTickTransactions.isNotEmpty) {
      _currentTickTransactions[0].changes.clear();
    }

    return messages;
  }

  /// Generate a list of HTTP requests to expose/unexpose pending properties on the transient preset.
  List<UnrealHttpRequestWithCallback> _generatePropertyExposureChangeRequests() {
    final List<UnrealHttpRequestWithCallback> requests = [];

    // Add messages for any properties that need to be exposed/unexposed
    for (final TrackedPropertyId propertyId in _propertiesPendingExpose) {
      final _TrackedProperty? property = _trackedProperties[propertyId];
      assert(property != null);
      assert(property!.exposeCompleter != null);

      requests.add(_createExposePropertyRequest(propertyId));
    }

    for (final TrackedPropertyId propertyId in _propertiesPendingUnexpose) {
      final _TrackedProperty? property = _trackedProperties[propertyId];
      assert(property != null);
      assert(property!.bIsPendingUnexpose);

      requests.add(_createUnexposePropertyRequest(propertyId));
    }

    _propertiesPendingExpose.clear();
    _propertiesPendingUnexpose.clear();

    return requests;
  }

  /// Create a WebSocket message to apply [change] to [property], as part of the transaction with ID [transactionId],
  /// via the preset named [presetName].
  dynamic _createPropertyChangeMessage(
      _TrackedProperty property, _TrackedPropertyChange change, int transactionId, String presetName) {
    final dynamic parameters = change.operation.makeMessageParameters(PropertyModifyEvent(
      property: property.property,
      currentValue: property.value,
      deltaValue: change.deltaValue,
      presetName: presetName,
      presetPropertyLabel: property.label,
      typeName: property.typeName,
      sequenceNumber: _sequenceNumber,
      transactionId: transactionId,
    ));

    return createUnrealWebSocketMessage(change.operation.messageName, parameters);
  }

  /// Called when we get a message from the engine that our transient preset's fields have changed.
  void _onPresetFieldsChanged(dynamic message) {
    final List<dynamic>? changedFields = message['ChangedFields'];
    if (changedFields == null) {
      _log.warning('Received a changed fields message with no ChangedFields property. It will be ignored.');
      return;
    }

    final String? engineSequenceNumberString = message['SequenceNumber'];
    if (engineSequenceNumberString == null) {
      _log.warning('Received a changed fields message with no sequence number. It will be ignored.');
      return;
    }

    final int? engineSequenceNumber = int.tryParse(engineSequenceNumberString);
    if (engineSequenceNumber == null) {
      _log.warning('Received a changed fields message but failed to parse the sequence number. It will be ignored.');
      return;
    }

    for (dynamic changeData in changedFields) {
      _onSinglePresetFieldChanged(changeData, engineSequenceNumber);
    }
  }

  /// Called for each field that changed in a PresetFieldsChanged message from the engine.
  void _onSinglePresetFieldChanged(dynamic changeData, int engineSequenceNumber) {
    final String? propertyLabel = changeData['PropertyLabel'];
    if (propertyLabel == null) {
      _log.warning('Received a changed fields message with no property label');
      return;
    }

    final TrackedPropertyId? propertyId = _propertyIdsByLabel[propertyLabel];
    if (propertyId == null) {
      _log.warning('Received a changed fields message, but property $propertyLabel is not tracked');
      return;
    }

    final _TrackedProperty? property = _trackedProperties[propertyId];
    assert(property != null, 'A property in _propertyIdsByLabel should also be in _trackedProperties');

    // Parse the corrected value from the JSON data
    final dynamic engineValue = convertUnrealTypeJsonToDart(
      property!.typeName,
      changeData['PropertyValue'],
      previousValue: property.value,
      conversionMetadata: property.conversionMetadata,
    );
    if (engineValue == null) {
      _log.warning('Failed to read PropertyValue for change to $propertyLabel');
      return;
    }

    // Start at the time given by the server, then reapply our own input since then
    final List<_SequencedPropertyChangeRecord> recentTickChanges = property.recentTickChanges;

    // First, skip any changes we recorded that were already handled by the server
    int lastSkippedChangeIndex = -1;
    for (int i = 0; i < recentTickChanges.length; ++i) {
      if (recentTickChanges[i].sequenceNumber <= engineSequenceNumber) {
        lastSkippedChangeIndex = i;
      } else {
        break;
      }
    }

    recentTickChanges.removeRange(0, lastSkippedChangeIndex + 1);

    // Apply any remaining changes sequentially
    dynamic newValue = engineValue;

    for (_SequencedPropertyChangeRecord changeRecord in recentTickChanges) {
      for (_TrackedPropertyChange change in changeRecord.changes) {
        newValue = change.apply(newValue);
      }
    }

    // Also apply current tick's changes if applicable
    final _EngineTransaction? activeTransaction = _getActiveTransaction();
    if (activeTransaction != null) {
      final List<_TrackedPropertyChange>? currentChanges = activeTransaction.changes[propertyId];
      if (currentChanges != null) {
        for (_TrackedPropertyChange change in currentChanges) {
          newValue = change.apply(newValue);
        }
      }
    }

    property.value = newValue;
  }

  /// Called when the connection manager's connection state changes.
  void _onConnectionStateChanged(EngineConnectionState connectionState) {
    switch (connectionState) {
      case EngineConnectionState.connected:
        _onConnect();
        break;

      case EngineConnectionState.disconnected:
        _onDisconnect();
        break;
    }
  }

  /// Called when the connection manager has connected to the engine.
  void _onConnect() {
    _transientPresetNameFuture = _getOrCreateTransientPreset();

    _tickTimer = Timer.periodic(_tickRate, _onTick);
  }

  /// Called when the connection manager has disconnected from the engine.
  void _onDisconnect() {
    _tickTimer?.cancel();

    _tickTimer = null;
    _trackedProperties.clear();
    _propertyIdsByLabel.clear();
    _currentTickTransactions.clear();
    _messagesPendingPreset.clear();
    _transientPresetName = null;
    _transientPresetNameFuture = null;
    _lastTrackedId = const TrackedPropertyId.invalid();
  }
}

/// An ID used to refer to a property being tracked by EnginePropertyManager.
class TrackedPropertyId {
  static const _invalidId = -1;

  /// Create a tracked property ID with the given numerical value.
  const TrackedPropertyId(this.id);

  /// Create an invalid property ID.
  const TrackedPropertyId.invalid() : id = _invalidId;

  /// The internal int representation of the ID.
  final int id;

  @override
  bool operator ==(other) {
    return other is TrackedPropertyId && other.id == id;
  }

  @override
  int get hashCode => id;

  /// Returns whether this ID refers to a valid property.
  bool get isValid => id != _invalidId;

  @override
  String toString() => isValid ? '$id' : 'INVALID ($id)';
}

/// Metadata about a property received from the engine.
/// Any data in this class is exposed publicly via [UnrealPropertyManager.getTrackedPropertyMetaData].
class UnrealPropertyMetadata {
  const UnrealPropertyMetadata({this.displayName = '', this.minValue, this.maxValue, this.enumValues});

  /// The in-engine name of the property, if provided.
  final String displayName;

  /// The property's minimum value, if any.
  final dynamic minValue;

  /// The property's maximum value, if any.
  final dynamic maxValue;

  /// The list of possible enum values, if provided.
  final List<String>? enumValues;
}

/// Information about a transaction we created (or will create) in the engine.
class _EngineTransaction {
  _EngineTransaction({required this.description});

  /// The description of the transaction displayed in the editor.
  final String description;

  /// Whether the transaction is currently active in the app.
  bool bIsActive = true;

  /// Whether the transaction has been created in the engine.
  bool bWasCreatedInEngine = false;

  /// Changes that happened to each property as part of this transaction.
  /// The changes are listed chronologically and may be combined if they used the same type of operation.
  final Map<TrackedPropertyId, List<_TrackedPropertyChange>> changes = {};
}

/// Data about a property being tracked by EnginePropertyManager.
class _TrackedProperty {
  _TrackedProperty(
      {required this.property, required this.conversionMetadata, List<PropertyChangeCallback>? initialCallbacks}) {
    if (initialCallbacks != null) {
      changeCallbacks.addAll(initialCallbacks);
    }
  }

  /// Data used to find the property in the engine.
  final UnrealProperty property;

  /// Functions to call when this property's value changes.
  final Set<PropertyChangeCallback> changeCallbacks = {};

  /// Changes that have happened in each tick since the last engine update was received for this property.
  /// The last entry in this list is the most recently completed tick.
  final List<_SequencedPropertyChangeRecord> recentTickChanges = [];

  /// The label of the property as exposed on the transient preset.
  late String label;

  /// The type name of the property provided by the engine.
  late String typeName;

  /// Metadata about the property provided by the engine..
  late UnrealPropertyMetadata metadata;

  /// If true, we've sent a message requesting that the property be unexposed in the engine.
  bool bIsPendingUnexpose = false;

  /// Completer for when this property is finished being exposed. If this is non-null, the property is pending expose
  /// (or re-expose if bIsPendingUnexpose is also true).
  Completer<void>? exposeCompleter;

  /// Arbitrary data that will be passed to conversion functions.
  Map<String, dynamic> conversionMetadata;

  /// Underlying predicted value of the property. Use the [value] instead so callbacks are triggered.
  dynamic _value;

  /// The current predicted value of the property.
  dynamic get value => _value;

  set value(dynamic newValue) {
    bool bWasReady = _value != null;
    _value = newValue;

    // If we weren't ready before, then this is the first time the value was set. Callers are expecting to receive
    // confirmation that the property was exposed (including its value) rather than a callback containing the value,
    // so don't send callbacks yet.
    if (bWasReady) {
      for (PropertyChangeCallback callback in changeCallbacks) {
        callback(newValue);
      }
    }
  }
}

/// A single change to a tracked property.
class _TrackedPropertyChange {
  _TrackedPropertyChange({required this.operation, this.deltaValue});

  /// The operation used to apply the change.
  final PropertyModifyOperation operation;

  /// The amount by which the property's value was changed.
  dynamic deltaValue;

  /// Returns whether this can be combined with another operation.
  bool canCombine(_TrackedPropertyChange other) {
    return other.operation == operation;
  }

  /// Combine the delta values of two operations if possible. The combined value will be stored in this change.
  void combine(_TrackedPropertyChange other) {
    if (!canCombine(other)) {
      throw Exception('Unable to combine tracked property changes');
    }

    deltaValue = other.apply(deltaValue);
  }

  /// Returns the result of applying the operation to the given [value].
  dynamic apply(dynamic value) => operation.apply(value, deltaValue);

  /// Given a [referenceValue] that the change will be applied to, modify the change so that it doesn't exceed the
  /// provided [min] and [max] values by clamping it. If [min] or [max] are null, that constraint won't be applied.
  /// Returns true if the change is still necessary, and false if after clamping, there would effectively be no change.
  bool clamp({required dynamic referenceValue, dynamic min, dynamic max}) {
    final dynamic clampedDeltaValue = operation.clamp(referenceValue, deltaValue, min, max);

    if (clampedDeltaValue != null) {
      deltaValue = clampedDeltaValue;
      return true;
    }

    return operation.bIsAlwaysSignificant;
  }

  /// Given a [referenceValue] that the change will be applied to, modify the change so that it doesn't exceed the
  /// provided [min] and [max] by looping around them.
  /// Returns true if loop was successful.
  bool loop({required dynamic referenceValue, dynamic min, dynamic max}) {
    final dynamic loopedDeltaValue = operation.loop(referenceValue, deltaValue, min, max);

    if (loopedDeltaValue != null) {
      deltaValue = loopedDeltaValue;
    }

    return loopedDeltaValue != null;
  }
}

/// All changes that occurred to a property on a given tick.
class _SequencedPropertyChangeRecord {
  const _SequencedPropertyChangeRecord({required this.sequenceNumber, required this.changes});

  /// The sequence number when the tick was completed.
  final int sequenceNumber;

  /// The changes that occurred to the property on the tick.
  final List<_TrackedPropertyChange> changes;
}
