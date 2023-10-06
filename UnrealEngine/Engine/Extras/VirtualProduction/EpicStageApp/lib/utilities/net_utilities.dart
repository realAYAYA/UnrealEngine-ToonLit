// Copyright Epic Games, Inc. All Rights Reserved.

import 'dart:async';
import 'dart:io';
import 'dart:typed_data';

import 'package:archive/archive_io.dart';
import 'package:logging/logging.dart';
import 'package:uuid/uuid.dart';

import '../models/engine_connection.dart';

const int _protocolVersion = 0; // The current protocol version for beacon messages
const String _protocolIdentifier = 'ES@p';

/// Data about a detected engine connection
class ConnectionData {
  ConnectionData({required this.uuid, required this.name, required this.websocketAddress, required this.websocketPort})
      : lastSeenTime = DateTime.now();

  final UuidValue uuid;
  String name;
  InternetAddress websocketAddress;
  int websocketPort;
  DateTime lastSeenTime;
}

/// Convert a Uint8List to a UUID.
UuidValue uuidFromUint8List(Uint8List data) {
  String guidString = Uuid.unparse(data);
  return UuidValue(guidString, false);
}

/// Make the beacon message that will be multicast to find compatible engine instances.
Uint8List makeBeaconMessage() {
  return Uint8List.fromList(_protocolIdentifier.codeUnits + [_protocolVersion]);
}

/// Retrieve the connection data from the datagram received from the response to a beacon message.
/// If the connection data couldn't be retrieved, this will return null.
ConnectionData? getConnectionFromBeaconResponse(Datagram datagram) {
  final InputStream stream = InputStream(datagram.data);
  stream.byteOrder = LITTLE_ENDIAN;
  ConnectionData? connection;

  try {
    stream.readByte(); // Protocol version; currently ignored
    final UuidValue engineUuid = uuidFromUint8List(stream.readBytes(16).toUint8List());
    final int port = stream.readUint32();

    final int nameLength = stream.readUint32();
    String name = stream.readString(size: nameLength, utf8: true);

    if (name[name.length - 1] == '\x00') {
      // Remove the null terminator
      name = name.substring(0, name.length - 1);
    }

    // If the engine didn't provide a friendly name, just use the address and port.
    if (name.isEmpty || (name.length == 1 && name[0] == '\x00')) {
      name = '${datagram.address.address}:${port.toString()}';
    }

    connection = ConnectionData(uuid: engineUuid, name: name, websocketAddress: datagram.address, websocketPort: port);
  } catch (error) {
    // TODO: Set up error logging so we can do something here
  }

  return connection;
}

/// A helper class that automatically retries sending a message to the engine if it times out.
class MessageTimeoutRetryHelper {
  MessageTimeoutRetryHelper({
    required this.connection,
    required this.sendMessage,
    this.onTimeout,
    required this.timeoutDuration,
    this.logger,
    this.logDescription,
    this.bLogOnlyAfterFailure = true,
  }) : assert(!(logDescription == null && logger != null));

  /// The connection to test before attempting to retry.
  final EngineConnectionManager connection;

  /// A function that sends the message to the engine.
  final void Function() sendMessage;

  /// A function that will be called each time this times out, passing the consecutive [timeoutCount].
  final void Function(int timeoutCount)? onTimeout;

  /// How long to wait before timing out.
  final Duration timeoutDuration;

  /// The log to which debug logs should be sent. If null, this won't log anything.
  final Logger? logger;

  /// A debug description of what this is trying to send. If [logger] is set, this must be too.
  final String? logDescription;

  /// If true, only send a log on success if this failed at least once before succeeding.
  final bool bLogOnlyAfterFailure;

  /// Timer use to determine if we've timed out.
  Timer? _timer;

  /// Whether this is currently attempting to send a message.
  bool _bIsActive = false;

  /// Whether our last attempt was successful.
  bool _bWasSuccessful = false;

  /// How many consecutive times this timed out since being started. This is not cleared until [start] is called.
  int _timeoutCount = 0;

  /// Whether this is currently attempting to send a message.
  bool get bIsActive => _bIsActive;

  /// How many consecutive times this timed out since being started. This is not cleared until [start] is called.
  int get timeoutCount => _timeoutCount;

  /// Start attempting to send the message.
  void start() {
    if (bIsActive) {
      return;
    }

    _bIsActive = true;
    _bWasSuccessful = false;
    _timeoutCount = 0;
    _internalSendMessage();
  }

  /// Give up trying to send the message.
  void giveUp() {
    if (!_cancel()) {
      return;
    }

    logger?.info('Abandoned "$logDescription" after $timeoutCount failed attempt(s)');
  }

  /// Stop trying to send the message after succeeding.
  void succeed() {
    if (!_cancel()) {
      return;
    }

    _bWasSuccessful = true;
  }

  /// Cancel the timer and return to inactive state.
  bool _cancel() {
    if (!bIsActive) {
      return false;
    }

    _bIsActive = false;
    _timer?.cancel();

    return true;
  }

  /// Send the message and start the timer.
  void _internalSendMessage() {
    sendMessage();

    _timer?.cancel();
    _timer = Timer(timeoutDuration, _internalOnTimeout);
  }

  /// Called when this times out waiting for a response.
  void _internalOnTimeout() async {
    ++_timeoutCount;

    logger?.info('Timed out waiting for "$logDescription" (attempt #$timeoutCount)');

    onTimeout?.call(_timeoutCount);

    if (!_bIsActive) {
      _logCancellation();
      return;
    }

    logger?.info('Confirming engine is receiving messages for "$logDescription"');

    // Check that our messages are getting through before we attempt to send another request. This confirms that the
    // engine actually failed to receive/respond that we're not just waiting for it (or our request) to make it through
    // network traffic
    await connection.testEngineResponse();

    if (!_bIsActive) {
      logger?.info('Engine OK for "$logDescription"');
      _logCancellation();
      return;
    }

    logger?.info('Engine OK, retrying for "$logDescription"');
    _internalSendMessage();
  }

  /// If needed, log that this has succeeded/failed.
  void _logCancellation() {
    if (_bWasSuccessful) {
      if (!bLogOnlyAfterFailure || timeoutCount > 0) {
        logger?.info('Succeeded at "$logDescription" after $timeoutCount failed attempt(s)');
      }
    } else {
      logger?.info('Abandoned "$logDescription" after $timeoutCount failed attempt(s)');
    }
  }
}
