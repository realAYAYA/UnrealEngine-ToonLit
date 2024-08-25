// Copyright Epic Games, Inc. All Rights Reserved.

import 'dart:async';
import 'dart:io';

import 'package:archive/archive.dart';
import 'package:epic_common/unreal_beacon.dart';
import 'package:logging/logging.dart';

import '../models/engine_connection.dart';

/// The configuration to use for the beacon that detects compatible Unreal Engine instances.
final unrealEngineBeaconConfig = UnrealEngineBeaconConfig(
  beaconAddress: InternetAddress('230.0.0.2'),
  beaconPort: 6667,
  protocolIdentifier: 'ES@p',
  userDataReader: (InputStream inputStream, InternetAddress address, _, __) {
    final int port = inputStream.readUint32();
    String? name = readStringFromStream(inputStream);

    // If the engine didn't provide a friendly name, just use the address and WebSocket port.
    if (name == null || name.isEmpty || (name.length == 1 && name[0] == '\x00')) {
      name = '${address.address}:${port.toString()}';
    }

    return UnrealBeaconResponseUserData(
      additionalData: UnrealStageBeaconData(websocketPort: port),
      name: name,
    );
  },
);

/// Type of response received from beacon messages for this app.
typedef UnrealStageBeaconResponse = UnrealBeaconResponse<UnrealStageBeaconData>;

/// Additional Unreal Stage-specific data received from beacon messages.
class UnrealStageBeaconData {
  const UnrealStageBeaconData({required this.websocketPort});

  /// Port to connect to for WebSocket communication.
  final int websocketPort;
}

/// Data about a connection to Unreal Engine.
class ConnectionData {
  const ConnectionData({
    required this.name,
    required this.websocketAddress,
    required this.websocketPort,
  }) : this.bIsDemo = false;

  const ConnectionData._({
    required this.name,
    required this.websocketAddress,
    required this.websocketPort,
    required this.bIsDemo,
  });

  /// Create connection data for demo mode.
  static ConnectionData forDemoMode() {
    return ConnectionData._(
      name: 'Demo',
      websocketAddress: InternetAddress('127.0.0.1'),
      websocketPort: 0,
      bIsDemo: true,
    );
  }

  /// Create connection data from a [response] to an UnrealEngineBeacon message.
  static ConnectionData fromBeaconResponse(UnrealStageBeaconResponse response) {
    return ConnectionData(
      name: response.name,
      websocketAddress: response.address,
      websocketPort: response.additionalData.websocketPort,
    );
  }

  /// User-friendly name of the connection.
  final String name;

  /// Address to connect to for WebSocket communication.
  final InternetAddress websocketAddress;

  /// Port to connect to for WebSocket communication.
  final int websocketPort;

  /// Whether this connection is a demo, meaning no actual Unreal Engine instance exists.
  final bool bIsDemo;
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
