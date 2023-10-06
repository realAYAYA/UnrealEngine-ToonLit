// Copyright Epic Games, Inc. All Rights Reserved.

import 'dart:async';
import 'dart:collection';
import 'dart:io';
import 'dart:math';
import 'dart:typed_data';

import 'package:flutter/cupertino.dart';
import 'package:flutter/foundation.dart';
import 'package:logging/logging.dart';

import '../utilities/net_utilities.dart';

final _log = Logger('UnrealEngineBeacon');

/// The address to multicast beacon messages to.
final InternetAddress _multicastAddress = InternetAddress('230.0.0.2');

/// The port to multicast beacon messages to.
const int _multicastPort = 6667;

/// The base rate at which to send beacon messages.
const Duration _beaconInterval = Duration(seconds: 1);

/// How long to wait before considering a connection stale and pruning it from the list.
const Duration _connectionStaleTime = Duration(seconds: 3);

/// The amount of time before reporting to the user that the beacon has failed to connect.
const Duration _beaconFailureTimeout = Duration(seconds: 5);

/// The amount to multiply [_beaconInterval] by for each consecutive failure to send a message.
const double _beaconFailureBackoff = 2.0;

/// Sends UDP multicasts on all available interfaces to search for Unreal Engine instances and maintains a list of
/// recently seen instances.
class UnrealEngineBeacon {
  UnrealEngineBeacon({
    required BuildContext context,
    this.onEngineInstancesChanged,
    this.onBeaconFailure,
  }) {
    _updateBeaconSockets().then((value) => _startUpdateTimer());
  }

  /// Timer used to fire the periodic update.
  Timer? _updateTimer;

  /// If true, we already reported that all beacons are failing.
  bool _bHasReportedFailure = false;

  /// Map from addresses for each network interface to our beacon socket for that interface.
  final Map<InternetAddress, _BeaconSocket> _sockets = {};

  /// List of possible engine connections.
  final List<ConnectionData> _connections = [];

  /// Callback for when the list of available engine connections changes.
  final Function()? onEngineInstancesChanged;

  /// Callback for when all beacon sockets are failing, so engines can't be detected.
  final Function()? onBeaconFailure;

  /// The current list of active engine connections.
  UnmodifiableListView<ConnectionData> get connections => UnmodifiableListView(_connections);

  /// Clean up all sockets and stop listening for engine instances.
  void dispose() {
    _stopUpdateTimer();

    for (final _BeaconSocket socket in _sockets.values) {
      socket.dispose();
    }
    _sockets.clear();
  }

  /// Pause sending beacon messages if paused.
  void pause() {
    _stopUpdateTimer();

    for (final _BeaconSocket socket in _sockets.values) {
      socket.bIsPaused = true;
    }
  }

  /// Resume sending beacon messages if paused.
  void resume() {
    _startUpdateTimer();

    for (final _BeaconSocket socket in _sockets.values) {
      socket.bIsPaused = false;
    }
  }

  /// Start/restart the update timer.
  void _startUpdateTimer() {
    _stopUpdateTimer();

    _updateTimer = Timer.periodic(_beaconInterval, _onUpdatePeriod);
  }

  /// Stop the update timer.
  void _stopUpdateTimer() {
    if (_updateTimer != null) {
      _updateTimer!.cancel();
      _updateTimer = null;
    }
  }

  /// Called on each update period.
  void _onUpdatePeriod([Timer? timer]) {
    _checkForBeaconSocketFailure();
    _pruneConnections();
    _updateBeaconSockets();
  }

  /// Check if all beacon sockets are failing, and if so, report it.
  void _checkForBeaconSocketFailure() {
    bool bAllFailing = true;
    for (final _BeaconSocket socket in _sockets.values) {
      if (!socket.bIsFailing) {
        bAllFailing = false;
      }
    }

    if (bAllFailing) {
      if (!_bHasReportedFailure) {
        _bHasReportedFailure = true;
        onBeaconFailure?.call();
      }
    } else {
      _bHasReportedFailure = false;
    }
  }

  /// Bind/unbind beacon sockets in order to match the current list of network interfaces.
  Future<void> _updateBeaconSockets() async {
    final Map<InternetAddress, String> adapterNames = {};
    final Set<InternetAddress> addresses = {};

    if (NetworkInterface.listSupported) {
      // Get the list of all available network interfaces
      final List<NetworkInterface> interfaces = await NetworkInterface.list();

      for (final NetworkInterface interface in interfaces) {
        addresses.addAll(interface.addresses);

        for (final InternetAddress address in interface.addresses) {
          adapterNames[address] = interface.name;
        }
      }
    } else {
      // We can't get a list of interfaces, so fall back to whatever the OS gives us
      addresses.add(InternetAddress.anyIPv4);
      addresses.add(InternetAddress.anyIPv6);
    }

    // Close beacons we no longer need
    final List<InternetAddress> staleAddresses = [];
    for (final InternetAddress existingAddress in _sockets.keys) {
      if (!addresses.contains(existingAddress)) {
        staleAddresses.add(existingAddress);
      }
    }

    for (final InternetAddress staleAddress in staleAddresses) {
      _sockets[staleAddress]?.dispose();
      _sockets.remove(staleAddress);
    }

    // Create beacons for new interfaces
    for (final InternetAddress newAddress in addresses) {
      if (_sockets.containsKey(newAddress)) {
        continue;
      }

      final beaconSocket = _BeaconSocket(
        adapterName: adapterNames[newAddress] ?? 'unknown',
        address: newAddress,
        onNewConnectionData: _updateConnection,
      );

      _sockets[newAddress] = beaconSocket;
    }
  }

  /// Update the data for a new or existing connection.
  void _updateConnection(ConnectionData newConnectionData) {
    bool bAlreadyExisted = false;
    bool bAnyChanges = false;

    // Check if we've already seen this connection
    for (int i = 0; i < _connections.length; ++i) {
      final ConnectionData oldConnectionData = _connections[i];
      if (oldConnectionData.uuid == newConnectionData.uuid) {
        // Check if there were any changes other than last seen time
        if (oldConnectionData.name != newConnectionData.name ||
            oldConnectionData.websocketPort != newConnectionData.websocketPort ||
            oldConnectionData.websocketAddress != newConnectionData.websocketAddress) {
          bAnyChanges = true;
        }

        // Refresh the connection data (including last seen time)
        _connections[i] = newConnectionData;

        bAlreadyExisted = true;
        break;
      }
    }

    if (!bAlreadyExisted) {
      bAnyChanges = true;
      _connections.add(newConnectionData);
    }

    if (bAnyChanges) {
      _connections.sort((a, b) => a.name.compareTo(b.name));
      onEngineInstancesChanged?.call();
    }
  }

  /// Remove any stale connections from the list
  void _pruneConnections() {
    final DateTime now = DateTime.now();
    final List<int> toRemove = [];

    for (int i = 0; i < _connections.length; ++i) {
      if (now.difference(_connections[i].lastSeenTime) > _connectionStaleTime) {
        toRemove.add(i);
      }
    }

    for (final int i in toRemove.reversed) {
      _connections.removeAt(i);
    }

    if (toRemove.length > 0) {
      onEngineInstancesChanged?.call();
    }
  }
}

/// A socket open on a specific interface to send and receive beacon messages.
class _BeaconSocket {
  _BeaconSocket({
    required this.adapterName,
    required this.address,
    required this.onNewConnectionData,
  }) {
    _bindSocket().then((final bool bIsValid) {
      if (!bIsValid) {
        return;
      }

      /// Send the first message immediately so we don't have to wait for an update
      _sendBeaconMessage();
    });
  }

  /// The name of the adapter this is using.
  final String adapterName;

  /// The address of the interface to send/listen on.
  final InternetAddress address;

  /// Callback function when new engine connection data has been received.
  final Function(ConnectionData) onNewConnectionData;

  /// Timer for sending out beacon messages.
  Timer? _messageTimer;

  /// Timer for when we consider this beacon to have failed.
  Timer? _failTimer;

  /// Whether the beacon has recently successfully sent a multicast message.
  bool _bHasRecentlySucceeded = false;

  /// Whether this has been disposed.
  bool _bWasDisposed = false;

  /// Whether this is temporarily not sending beacon messages.
  bool _bIsPaused = false;

  /// Whether this encountered an instant failure code and should not attempt to reconnect.
  bool _bWasInstantFailed = false;

  /// The socket used to send and receive beacon messages.
  RawDatagramSocket? _socket;

  /// The subscription by which this listens to [_socket].
  StreamSubscription? _subscription;

  /// The number of times this has consecutively failed to send a message via its socket.
  int _consecutiveFailures = 0;

  /// Error codes which, if encountered, should cause the beacon to immediately be treated as failed.
  static Set<int> get _instantFailErrorCodes {
    if (Platform.isIOS) {
      return {
        // Operation not permitted
        1,
      };
    }

    if (Platform.isAndroid) {
      return {
        // Network is unreachable
        101,
      };
    }

    return {};
  }

  /// Whether this is currently failing to send messages to its interface.
  bool get bIsFailing => !_bHasRecentlySucceeded;

  /// Whether this is currently prevented from sending beacon messages.
  bool get bIsPaused => _bIsPaused;

  void set bIsPaused(bool bNewValue) {
    _bIsPaused = bNewValue;
    if (_bIsPaused) {
      _stopUpdateTimer();
    } else {
      _startUpdateTimer();
    }
  }

  /// Stop sending and receiving for this beacon and close its socket.
  void dispose() {
    if (_bWasDisposed) {
      return;
    }

    _bWasDisposed = true;
    _giveUp();
  }

  /// Update the number of consecutive failures and adjust the timer accordingly.
  void _updateConsecutiveFailures(int newValue) {
    if (_consecutiveFailures != newValue) {
      _consecutiveFailures = newValue;
      _startUpdateTimer();
    }
  }

  /// Start sending periodic beacon messages.
  void _startUpdateTimer() {
    if (_messageTimer != null) {
      return;
    }

    // Send periodic beacons to discover any new Unreal Engine instances
    final Duration period = _beaconInterval * pow(_beaconFailureBackoff, _consecutiveFailures);
    _messageTimer = Timer.periodic(period, (_) => _sendBeaconMessage());
  }

  /// Stop sending periodic beacon messages.
  void _stopUpdateTimer() {
    _messageTimer?.cancel();
    _messageTimer = null;
  }

  /// Send a beacon message on this socket.
  void _sendBeaconMessage() {
    if (_socket != null) {
      final Uint8List message = makeBeaconMessage();
      if (_socket!.send(message, _multicastAddress, _multicastPort) > 0) {
        // We successfully sent a beacon message, so cancel the failure timer and indicate that we should consider it a
        // new failure if we fail again later.
        _bHasRecentlySucceeded = true;
        _failTimer?.cancel();
        _updateConsecutiveFailures(0);
      }
    }
  }

  /// Bind the socket used to send and receive beacon messages.
  Future<bool> _bindSocket() async {
    _stopUpdateTimer();

    _subscription?.cancel();
    _socket?.close();

    late final RawDatagramSocket newSocket;

    try {
      newSocket = await RawDatagramSocket.bind(address, _multicastPort, ttl: 4);
      _log.info('Beacon socket open on "$adapterName" (${address.address})');
    } catch (error) {
      _log.warning('Failed to bind socket for "$adapterName" (${address.address}):\n$error');
      return Future.value(false);
    }

    _socket = newSocket;
    _subscription = newSocket.listen(
      _receiveMessage,
      onError: _onSocketError,
      onDone: _onSocketClosed,
      cancelOnError: false,
    );

    if (!_bIsPaused) {
      _startUpdateTimer();
    }

    return Future.value(true);
  }

  /// Called when an error occurs on the socket that sends beacon messages.
  void _onSocketError(Object error, StackTrace stackTrace) {
    _log.info('Failed to send beacon message on "$adapterName" (${address.address}):\n$error');

    if (error is SocketException && _instantFailErrorCodes.contains(error.osError?.errorCode)) {
      _log.info('Error code ${error.osError?.errorCode} is an instant fail; closing beacon');
      _bWasInstantFailed = true;
      _giveUp();
      return;
    }

    _updateConsecutiveFailures(_consecutiveFailures + 1);

    if (_bHasRecentlySucceeded && _failTimer == null) {
      _failTimer = Timer(_beaconFailureTimeout, _handleBeaconFailed);
    }
  }

  /// Called when the beacon has failed for too long.
  void _handleBeaconFailed() {
    _bHasRecentlySucceeded = false;
    _failTimer = null;
  }

  /// Receive a message on the beacon socket (presumably a reply to a beacon message).
  void _receiveMessage(RawSocketEvent event) {
    final Datagram? datagram = _socket!.receive();
    if (datagram != null) {
      ConnectionData? newConnection = getConnectionFromBeaconResponse(datagram);

      if (newConnection != null) {
        onNewConnectionData(newConnection);
      }
    }
  }

  /// Called when the socket closes.
  void _onSocketClosed() {
    if (_bWasDisposed || _bWasInstantFailed) {
      return;
    }

    _stopUpdateTimer();
    _bindSocket().then((_) {
      if (!_bIsPaused) {
        _startUpdateTimer();
      }
    });
  }

  /// Close the socket and cancel the subscription if necessary, then report failure.
  void _giveUp() {
    _subscription?.cancel();
    _socket?.close();
    _socket = null;
    _bHasRecentlySucceeded = false;
  }
}
