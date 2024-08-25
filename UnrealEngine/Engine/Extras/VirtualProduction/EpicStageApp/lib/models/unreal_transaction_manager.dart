// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:flutter/material.dart';
import 'package:logging/logging.dart';
import 'package:provider/provider.dart';

import '../utilities/unreal_utilities.dart';
import 'engine_connection.dart';

final _log = Logger('UnrealTransactionManager');

/// Manages the state of transactions in the Unreal Engine.
class UnrealTransactionManager {
  /// Construct a property manager and register it with the connection manager.
  UnrealTransactionManager(BuildContext context)
      : _connectionManager = Provider.of<EngineConnectionManager>(context, listen: false) {
    _connectionManager.registerMessageListener('TransactionEnded', _onEngineTransactionEnded);
  }

  /// Transaction ID returned when a transaction could not be created.
  static const int invalidId = -1;

  /// The connection manager we'll use to communicate with the engine.
  final EngineConnectionManager _connectionManager;

  /// The ID of the next transaction to be created.
  int _nextTransactionId = 0;

  /// Data for the current active transaction, or null if there isn't one.
  _EngineTransaction? _activeTransaction;

  /// Check whether there's a transaction in progress.
  bool get bHasActiveTransaction => _activeTransaction != null;

  /// The ID of the active transaction, or [invalidId] if there isn't one.
  int get activeTransactionId => _activeTransaction?.id ?? invalidId;

  /// Start a transaction.
  /// Returns true if the transaction was successfully created.
  bool beginTransaction(String description, {void Function()? prematureEndCallback}) {
    final message = createBeginTransactionMessage(description, prematureEndCallback: prematureEndCallback);
    if (message == null) {
      return false;
    }

    _connectionManager.sendRawMessage(message);
    return true;
  }

  /// End the current transaction.
  /// Returns false if there was no active transaction to end.
  bool endTransaction() {
    final message = createEndTransactionMessage();
    if (message == null) {
      return false;
    }

    _connectionManager.sendRawMessage(message);
    return true;
  }

  /// Create a message to start a transaction.
  /// The caller is expected to send this message using [EngineConnectionManager], e.g. as part of a batch.
  /// Returns null if there was already an active transaction.
  dynamic createBeginTransactionMessage(String description, {void Function()? prematureEndCallback}) {
    if (_activeTransaction != null) {
      _log.warning('Tried to start transaction, but one was already in progress');
      return null;
    }

    _activeTransaction = _EngineTransaction(
      id: _nextTransactionId,
      prematureEndCallback: prematureEndCallback,
    );

    final message = createUnrealWebSocketMessage('transaction.begin', {
      'Description': description,
      'TransactionId': _nextTransactionId,
    });

    ++_nextTransactionId;

    return message;
  }

  /// Create a message to end a transaction.
  /// The caller is expected to send this message using [EngineConnectionManager], e.g. as part of a batch.
  /// Returns null if there was no active transaction to end.
  dynamic createEndTransactionMessage() {
    if (_activeTransaction == null) {
      _log.warning('Tried to end transaction, but no transaction was in progress');
      return null;
    }

    final message = createUnrealWebSocketMessage('transaction.end', {
      'TransactionId': _activeTransaction!.id,
    });

    _activeTransaction = null;
    return message;
  }

  /// Helper function to automatically create and end a transaction before/after calling a function, taking into account
  /// whether the engine version allows a manual transaction for property changes.
  /// If a manual transaction is allowed, this will create a transaction with the given [description], it will call
  /// [sendRequest] passing true for [bIsManualTransaction], then end the transaction.
  /// Otherwise, it will just call [sendRequest], passing false for [bIsManualTransaction].
  dynamic wrapWithTransactionIfManualAllowedForProperties(
    String description,
    Function(bool bIsManualTransaction) sendRequest,
  ) {
    final bool bIsManualTransaction = _connectionManager.apiVersion?.bCanHttpSetPropertyInManualTransaction == true;
    if (bIsManualTransaction) {
      if (!beginTransaction(description)) {
        _log.warning('Failed to begin transaction for "$description"');
        return;
      }
    }

    final result = sendRequest(bIsManualTransaction);

    if (bIsManualTransaction) {
      endTransaction();
    }

    return result;
  }

  /// Called when the engine reports that a transaction has ended.
  void _onEngineTransactionEnded(dynamic message) {
    final int? id = message['TransactionId'];
    if (id == null) {
      _log.warning('Received TransactionEnded event with no transaction ID');
      return;
    }

    if (_activeTransaction?.id != id) {
      return;
    }

    _activeTransaction?.prematureEndCallback?.call();
    _activeTransaction = null;
  }
}

/// Data about an active transaction that we created in the engine.
class _EngineTransaction {
  _EngineTransaction({required this.id, this.prematureEndCallback});

  /// The ID used to the transaction in the engine.
  final int id;

  /// Callback function if this transaction ends prematurely.
  final void Function()? prematureEndCallback;
}
