// Copyright Epic Games, Inc. All Rights Reserved.

import 'dart:async';
import 'dart:convert';
import 'dart:io';
import 'dart:typed_data';

import 'package:crypto/crypto.dart';
import 'package:epic_common/widgets.dart';
import 'package:flutter/material.dart';
import 'package:flutter_gen/gen_l10n/app_localizations.dart';
import 'package:logging/logging.dart';
import 'package:provider/provider.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:web_socket_channel/io.dart';

import '../utilities/net_utilities.dart';
import '../utilities/unreal_utilities.dart';
import '../widgets/screens/connect/connect.dart';
import '../widgets/screens/connect/views/passphrase_modal.dart';
import '../widgets/screens/main/stage_app_main_screen.dart';
import '../widgets/screens/reconnect_screen.dart';
import './navigator_keys.dart';
import 'api_version.dart';
import 'engine_passphrase_manager.dart';
import 'settings/connection_settings.dart';

final _log = Logger('EngineConnection');

/// The state of a connection to Unreal Engine.
enum EngineConnectionState {
  connected,
  disconnected,
}

/// Possible results of attempting to connect to the engine.
enum EngineConnectionResult {
  /// The connection was successful.
  success,

  /// The connection was cancelled.
  cancelled,

  /// The connection couldn't be opened because another one was already open.
  alreadyOpen,

  /// The connection failed because the passphrase was incorrect.
  passphraseRejected,

  /// The connection failed because we were unable to configure compression.
  compressionFailed,

  /// The connection failed for unknown reasons.
  genericFailure,
}

/// HTTP response codes
class HttpResponseCode {
  static const int ok = 200;
  static const int unauthorized = 401;
  static const int clientClosedRequest = 499;
}

/// Callback type for a WebSocket message. Passes dynamic JSON data parsed from the message.
typedef WebSocketMessageCallback = void Function(dynamic message);

/// Callback for changes to the state of the engine connection.
typedef ConnectionStateCallback = void Function(EngineConnectionState connectionState);

/// An HTTP request in the format required by Unreal Engine.
class UnrealHttpRequest {
  const UnrealHttpRequest({required this.url, required this.verb, this.body});

  final String url;
  final String verb;
  final dynamic body;
}

/// An HTTP response received from Unreal Engine.
class UnrealHttpResponse {
  const UnrealHttpResponse({required this.requestId, required this.code, required this.body});

  final int requestId;
  final int code;
  final dynamic body;
}

/// An HTTP request to the engine and an associated callback function which will be automatically called when a response
/// is received.
class UnrealHttpRequestWithCallback {
  const UnrealHttpRequestWithCallback(this.request, this.callback);

  final UnrealHttpRequest request;
  final Function(dynamic response) callback;
}

/// Holds the state of the app's connection to the engine and notifies when the connection changes.
class EngineConnectionManager with WidgetsBindingObserver {
  EngineConnectionManager(this.context)
      : _connectionSettings = Provider.of<ConnectionSettings>(context, listen: false) {
    _loadLastConnectionIfNone();

    WidgetsBinding.instance.addObserver(this);
  }

  /// The context this was built in.
  final BuildContext context;

  /// Name of the JSON field identifying the request ID of an HTTP request over WebSocket.
  static const _httpRequestIdFieldName = 'RequestId';

  /// The API version of the currently connected engine.
  EpicStageAppAPIVersion? _apiVersion;

  /// The data for the last active connection.
  ConnectionData? _lastConnectionData;

  /// The websocket channel we currently have open to the engine.
  IOWebSocketChannel? _webSocketChannel;

  /// The app's connection-related settings.
  final ConnectionSettings _connectionSettings;

  /// Map from message type to callback functions for when the message type is received.
  final Map<String, List<WebSocketMessageCallback>> _messageCallbacks = {};

  /// Callbacks for whenever the connection state changes.
  final List<ConnectionStateCallback> _connectionStateCallbacks = [];

  /// Map from waiting request ID to the completer to complete when the corresponding response is received.
  final Map<int, Completer<UnrealHttpResponse>> _httpResponseCompleters = {};

  /// List of HTTP request IDs that must be processed before connection is complete.
  final Set<int> _handshakeHttpMessageIds = {};

  /// Map from message type to callback functions for when the message type is received during the initial connection
  /// handshake. These messages will not be sent outside theis class.
  final Map<String, WebSocketMessageCallback> _handshakeMessageCallbacks = {};

  /// List of messages that have been received, but are waiting to dispatch after connection setup is finished.
  final List<dynamic> _initialPendingMessages = [];

  /// The last request ID we used.
  int _lastRequestId = 0;

  /// Future that will complete pending a response to a message sent to check the engine's responsiveness, or null if
  /// there's no pending message.
  Future<void>? _respondingCheck;

  /// The current state of the connection to the engine.
  EngineConnectionState _internalConnectionState = EngineConnectionState.disconnected;

  /// The current subscription to stream events from the connection.
  StreamSubscription? _connectionSubscription;

  /// Future that will return when the current connection attempt completes.
  FutureOr<EngineConnectionResult>? _pendingConnectionAttempt;

  /// HTTP client of the current WebSocket connection or connection attempt.
  HttpClient? _webSocketHttpClient;

  /// Whether the current connection attempt was cancelled.
  bool _bIsPendingConnectionCancelled = false;

  /// Whether this should attempt to reconnect when the app becomes active again.
  bool _bShouldReconnectOnWake = false;

  /// Whether the current connection is in demo mode, meaning no actual Unreal Engine instance is connected.
  bool _bIsInDemoMode = false;

  /// MD5 hash of the passphrase to be passed along with all messages for the current connection, or null if no
  /// passphrase was given.
  String? _passphraseHash;

  /// The compression mode used to compress and decompress WebSocket messages.
  WebSocketCompressionMode _compressionMode = WebSocketCompressionMode.none;

  /// Get the current state of the connection to the engine.
  EngineConnectionState get connectionState => _internalConnectionState;

  /// Set the current state of the connection to the engine.
  set _connectionState(EngineConnectionState state) {
    _internalConnectionState = state;

    for (ConnectionStateCallback callback in _connectionStateCallbacks) {
      callback(state);
    }
  }

  /// Get the version of the engine we're currently connected to, or null if we're not connected.
  EpicStageAppAPIVersion? get apiVersion => _apiVersion;

  /// Whether the current connection is in demo mode, meaning no actual Unreal Engine instance is connected.
  bool get bIsInDemoMode => _bIsInDemoMode;

  /// Dispose of any stored data.
  void dispose() {
    disconnect();
    WidgetsBinding.instance.removeObserver(this);
  }

  @override
  void didChangeAppLifecycleState(AppLifecycleState state) {
    switch (state) {
      case AppLifecycleState.detached:
      case AppLifecycleState.paused:
      case AppLifecycleState.inactive:
      case AppLifecycleState.hidden:
        _onAppPaused();
        break;

      case AppLifecycleState.resumed:
        _onAppResumed();
        break;
    }
  }

  /// Get the last engine connection we connected to, either in this session or a previous one.
  Future<ConnectionData?> getLastConnectionData() {
    if (_lastConnectionData != null) {
      return Future.value(_lastConnectionData);
    }

    return _loadLastConnectionIfNone();
  }

  /// Try to connect with the given connection data. Returns a future which will provide the result of the attempt.
  Future<EngineConnectionResult> connect(ConnectionData connectionData) async {
    if (_pendingConnectionAttempt != null) {
      if (!_bIsPendingConnectionCancelled) {
        _log.warning('Tried to connect while an uncancelled connection attempt was in progress. New attempt ignored');
        return EngineConnectionResult.alreadyOpen;
      }

      // Wait for cancelled attempt to finish so we don't try to open the same socket twice
      await _pendingConnectionAttempt;
    }

    _bIsPendingConnectionCancelled = false;
    Future<EngineConnectionResult> result;

    try {
      _pendingConnectionAttempt = _internalConnect(connectionData);
      result = Future.value(await _pendingConnectionAttempt);
    } catch (error, stack) {
      _log.warning('Connection failed: $error\n$stack');
      result = Future.value(EngineConnectionResult.genericFailure);
    }

    _pendingConnectionAttempt = null;

    return result;
  }

  /// Try to reconnect to the last connection.
  Future<EngineConnectionResult> reconnect() async {
    if (_lastConnectionData != null) {
      return connect(_lastConnectionData!);
    } else {
      return Future.error('Could not reconnect as there was no previous connection');
    }
  }

  /// Disconnect from the engine.
  void disconnect() {
    if (_pendingConnectionAttempt != null) {
      _bIsPendingConnectionCancelled = true;
    }

    _cancelHttpRequestsWithError(HttpResponseCode.clientClosedRequest);

    if (_webSocketChannel != null) {
      _log.info('Disconnecting from engine');

      _webSocketChannel!.sink.close();
      _connectionSubscription?.cancel();
    }

    _onWebSocketStreamClosed(
      bWasExpected: true,
      // If this was in the middle of attempting to connect, we were already on the connect screen and it can handle
      // the navigator by itself
      bShouldReturnToConnectScreen: _pendingConnectionAttempt == null,
    );

    _bIsInDemoMode = false;
  }

  /// Send a message via the current WebSocket connection in standard Unreal WebSocket format.
  void sendMessage(String messageName, Object parameters) {
    if (connectionState != EngineConnectionState.connected && _pendingConnectionAttempt == null) {
      return;
    }

    sendRawMessage(createUnrealWebSocketMessage(messageName, parameters));
  }

  /// Send a batch of messages as a single message via the current WebSocket connection.
  void sendBatchedMessage(List<dynamic> messages) {
    if (connectionState != EngineConnectionState.connected && _pendingConnectionAttempt == null) {
      return;
    }

    if (messages.length == 1) {
      // No point in including the batch overhead.
      sendRawMessage(messages[0]);
      return;
    }

    sendMessage('batch', {'Requests': messages});
  }

  /// Send a raw message via the current WebSocket connection, encoded as JSON.
  /// You may want to create the message using [createUnrealWebSocketMessage].
  void sendRawMessage(dynamic message) {
    if (_bIsInDemoMode) {
      return;
    }

    if (connectionState != EngineConnectionState.connected && _pendingConnectionAttempt == null) {
      return;
    }

    if (_passphraseHash != null) {
      message['Passphrase'] = _passphraseHash;
    }

    dynamic outData = jsonEncode(message);

    switch (_compressionMode) {
      case WebSocketCompressionMode.zlib:
        final List<int> uncompressedData = utf8.encode(outData);
        final List<int> compressedData = zlib.encode(uncompressedData);

        // Unreal requires the total WebSocket message length when sending in binary mode, and also the total
        // uncompressed size when sending compressed data.
        const int headerSize = 8;
        final int messageSize = compressedData.length + headerSize;
        final outBytes = ByteData(messageSize);
        outBytes.setUint32(0, messageSize - 4, Endian.little); // Don't include the size of this size
        outBytes.setInt32(4, uncompressedData.length, Endian.little);

        final Uint8List outBuffer = outBytes.buffer.asUint8List();
        outBuffer.setRange(headerSize, messageSize, compressedData);

        outData = outBuffer;
        break;

      default:
        break;
    }

    _webSocketChannel?.sink.add(outData);
  }

  /// Send an HTTP request via the current WebSocket connection.
  /// The response's body will contain decoded JSON data.
  Future<UnrealHttpResponse> sendHttpRequest(UnrealHttpRequest request) {
    return _internalSendHttpRequest(request);
  }

  /// Send an HTTP request via the current WebSocket connection and receive the response via a callback on the request.
  void sendHttpRequestWithCallback(UnrealHttpRequestWithCallback request) {
    sendHttpRequest(request.request).then(request.callback);
  }

  /// Send a batched HTTP request via the current WebSocket connection.
  /// The response's body will contain a list of nullable [UnrealHttpResponse], where a response is null if there was a
  /// problem parsing it.
  Future<UnrealHttpResponse> sendBatchedHttpRequest(Iterable<UnrealHttpRequest> requests) {
    final List<dynamic> requestParameters = [];

    // Generate sub-requests
    for (UnrealHttpRequest subRequest in requests) {
      final parameters = _generateHttpRequestParameters(subRequest);
      requestParameters.add(parameters);
    }

    // Set up the batch request and listen for its completion
    final batchRequest = UnrealHttpRequest(url: '/remote/batch', verb: 'PUT', body: {'Requests': requestParameters});
    final batchParameters = _generateHttpRequestParameters(batchRequest);
    final message = {
      'MessageName': 'http',
      'Parameters': batchParameters,
    };

    final Completer<UnrealHttpResponse> completer = _makeHttpResponseCompleterFromParameters(batchParameters);
    sendRawMessage(message);

    return completer.future.then((batchResponse) {
      if (batchResponse.code != HttpResponseCode.ok) {
        return batchResponse;
      }

      final List? responses = batchResponse.body['Responses'];
      if (responses == null) {
        return Future.error('Got a valid batch response, but it contained no responses');
      }

      return UnrealHttpResponse(
          requestId: batchResponse.requestId,
          code: batchResponse.code,
          body: responses.map((responseJson) => _decodeHttpResponseJsonData(responseJson)).toList());
    });
  }

  /// Send a batched HTTP request via the current WebSocket connection and receive the response via callbacks on each
  /// request.
  void sendBatchedHttpRequestWithCallbacks(List<UnrealHttpRequestWithCallback> requests) {
    final Iterable<UnrealHttpRequest> baseRequests =
        requests.map((UnrealHttpRequestWithCallback requestWithCallback) => requestWithCallback.request);

    sendBatchedHttpRequest(baseRequests).then((UnrealHttpResponse batchResponse) {
      for (int responseIndex = 0; responseIndex < batchResponse.body.length; ++responseIndex) {
        final UnrealHttpResponse? response = batchResponse.body[responseIndex];
        requests[responseIndex].callback(response);
      }
    });
  }

  /// Check that the engine is responding to messages (i.e. not tied up in a queue or otherwise failing to acknowledge
  /// them).
  /// Returns a future that will complete when the engine responds to a message. Multiple calls to this function will
  /// be combined into the most recent pending message.
  Future<void> testEngineResponse() async {
    if (_respondingCheck == null) {
      // This message has a small response, so sending it isn't expensive
      _respondingCheck = sendHttpRequest(UnrealHttpRequest(url: '/remote', verb: 'OPTIONS'));
    }

    await _respondingCheck;

    _respondingCheck = null;
  }

  /// Register for callbacks when a type of WebSocket message is received.
  void registerMessageListener(String messageType, WebSocketMessageCallback callback) {
    if (_messageCallbacks.containsKey(messageType)) {
      _messageCallbacks[messageType]!.add(callback);
    } else {
      _messageCallbacks[messageType] = [callback];
    }
  }

  /// Unregister for callbacks when a type of WebSocket message is received.
  void unregisterMessageListener(String messageType, WebSocketMessageCallback callback) {
    if (_messageCallbacks.containsKey(messageType)) {
      _messageCallbacks[messageType]!.remove(callback);
    }
  }

  /// Register for callbacks when the connection state changes.
  void registerConnectionStateListener(ConnectionStateCallback callback) {
    _connectionStateCallbacks.add(callback);
  }

  /// Unregister for callbacks when the connection state changes.
  void unregisterConnectionStateListener(ConnectionStateCallback callback) {
    _connectionStateCallbacks.remove(callback);
  }

  /// Make and remember a new HTTP client to use for WebSocket connections.
  HttpClient _makeHttpClient() {
    assert(_webSocketHttpClient == null);

    _webSocketHttpClient = HttpClient();
    _webSocketHttpClient!.connectionTimeout = Duration(seconds: 3);

    return _webSocketHttpClient!;
  }

  /// Internal function to connect to the engine.
  /// You should almost always call [connect] instead of this so we know we have a pending connection attempt.
  Future<EngineConnectionResult> _internalConnect(ConnectionData connectionData) async {
    _bIsInDemoMode = connectionData.bIsDemo;

    if (_bIsInDemoMode) {
      // Run the app in demo mode, where the connection will be spoofed
      _connectionState = EngineConnectionState.connected;
      _apiVersion = EpicStageAppAPIVersion(0, 0, 0);
      return Future.value(EngineConnectionResult.success);
    }

    // Get the passphrase ready
    final passphraseManager = Provider.of<EnginePassphraseManager>(context, listen: false);
    final String? passphrase = await passphraseManager.getPassphrase(connectionData);

    if (passphrase == null) {
      _passphraseHash = null;
    } else {
      _passphraseHash = md5.convert(utf8.encode(passphrase)).toString();
    }

    // Close the existing connection
    if (connectionState == EngineConnectionState.connected) {
      _onWebSocketStreamClosed(bWasExpected: true, bShouldReturnToConnectScreen: false);
    }

    _log.info('Connecting to engine at ${connectionData.websocketAddress.address}:${connectionData.websocketPort}');

    _lastConnectionData = connectionData;
    _webSocketChannel = null;
    _initialPendingMessages.clear();
    _handshakeHttpMessageIds.clear();

    final String address = 'ws://${connectionData.websocketAddress.address}:${connectionData.websocketPort.toString()}';
    WebSocket? webSocket;

    // Try to connect
    const int maxAttempts = 3;
    bool bHasConnected = false;
    String lastError = '';

    for (int attempt = 0; attempt < maxAttempts && !_bIsPendingConnectionCancelled; ++attempt) {
      _log.info('Connection attempt ${attempt + 1}...');
      try {
        webSocket = await WebSocket.connect(
          address,
          customClient: _makeHttpClient(),
        );

        bHasConnected = true;
      } catch (error) {
        _webSocketHttpClient?.close(force: true);
        _webSocketHttpClient = null;

        lastError = error.toString();
        _log.info('Failed: $lastError');
      }

      if (bHasConnected) {
        break;
      }
    }

    if (_bIsPendingConnectionCancelled) {
      if (bHasConnected) {
        disconnect();
      }
      return EngineConnectionResult.cancelled;
    }

    if (!bHasConnected) {
      return Future.error('Failed to connect to engine at $address: ' + lastError);
    }

    _log.info('Connected to $address');

    // Set up the websocket channel
    _webSocketChannel = IOWebSocketChannel(webSocket!);
    _connectionSubscription = _webSocketChannel!.stream.listen(
      _onWebSocketMessageReceived,
      onError: _onWebSocketError,
      onDone: _onWebSocketStreamClosed,
    );

    _saveLastConnection(connectionData);

    // Check passphrase
    final bool bIsPassphraseValid = await _checkPassphrase().timeout(
      Duration(seconds: 3),
      onTimeout: () => false,
    );
    if (!bIsPassphraseValid) {
      // The HTTP handler will have already disconnected us, so we can just return the result
      return EngineConnectionResult.passphraseRejected;
    }

    // Retrieve API version
    _log.info('Retrieving API version');
    try {
      _apiVersion = await _retrieveAPIVersion().timeout(Duration(seconds: 3));
    } catch (e) {
      _log.warning('Timed out waiting for API version');
      disconnect();
      return EngineConnectionResult.genericFailure;
    }
    _log.info('API version: $_apiVersion');

    // Bailed out early
    if (_bIsPendingConnectionCancelled) {
      _log.info('User cancelled connection');
      disconnect();
      return EngineConnectionResult.cancelled;
    }

    // Set up compression if available and desired
    if (_apiVersion!.bIsWebSocketCompressionAvailable &&
        _connectionSettings.webSocketCompressionMode.getValue() != WebSocketCompressionMode.none) {
      final bool isCompressionReady = await _enableWebSocketCompression().timeout(
        Duration(seconds: 3),
        onTimeout: () => false,
      );
      if (!isCompressionReady) {
        _log.warning('Timed out on compression response');
        disconnect();
        return EngineConnectionResult.compressionFailed;
      }
    } else {
      _log.info('No compression requested');
    }

    // Now that we're fully connected, handle any messages that were waiting
    for (final dynamic message in _initialPendingMessages) {
      _handleWebSocketMessage(message);
    }

    // This will notify external classes, so don't set it until everything is ready to go
    _connectionState = EngineConnectionState.connected;

    return EngineConnectionResult.success;
  }

  /// Enable WebSocket compression during the connection's handshake phase.
  Future<bool> _enableWebSocketCompression() async {
    const compressionMessage = 'CompressionChanged';

    final WebSocketCompressionMode targetCompressionMode = _connectionSettings.webSocketCompressionMode.getValue();
    final String targetCompressionModeString = targetCompressionMode.name.toUpperCase();

    _log.info('Requesting compression mode $targetCompressionModeString');

    final compressionReadyCompleter = Completer<bool>();
    final handleCompressionChanged =
        (message) => compressionReadyCompleter.complete(message['Mode'] == targetCompressionModeString);

    // Send message
    _handshakeMessageCallbacks[compressionMessage] = handleCompressionChanged;
    sendMessage('compression.change', {'Mode': targetCompressionModeString});

    // Wait for response
    final bool bIsCorrectMode = await compressionReadyCompleter.future;
    _handshakeMessageCallbacks.remove(compressionMessage);

    if (bIsCorrectMode) {
      _compressionMode = targetCompressionMode;
    }

    return bIsCorrectMode;
  }

  /// Called when a WebSocket error occurs.
  void _onWebSocketError(dynamic error) {
    _log.severe('WebSocket error', error);
  }

  /// Called when the WebSocket connection is closed.
  void _onWebSocketStreamClosed({bool bWasExpected = false, bool bShouldReturnToConnectScreen = true}) async {
    _webSocketChannel = null;
    _connectionSubscription = null;
    _connectionState = EngineConnectionState.disconnected;
    _compressionMode = WebSocketCompressionMode.none;
    _apiVersion = null;
    _respondingCheck = null;
    _passphraseHash = null;

    // Force the HTTP client itself shut in case we were in the middle of connecting and want to immediately stop.
    // The WebSocket itself isn't returned until the connection completes, so we have to kill the connection from here.
    _webSocketHttpClient?.close(force: true);
    _webSocketHttpClient = null;

    if (bShouldReturnToConnectScreen) {
      _goToNamedRoute(ConnectScreen.route);
    }

    if (!bWasExpected) {
      final BuildContext context = rootNavigatorKey.currentContext!;
      InfoModalDialog.show(context, AppLocalizations.of(context)!.engineConnectionLostMessage);
    }
  }

  /// Called when a WebSocket message is received.
  void _onWebSocketMessageReceived(dynamic data) {
    switch (_compressionMode) {
      case WebSocketCompressionMode.zlib:
        try {
          data = zlib.decode(Uint8List.fromList(data));
        } catch (_) {
          // Data may not have been compressed if it was cheaper to send raw, so fall through to directly decoding JSON
        }
        break;

      default:
        break;
    }

    final String stringData = String.fromCharCodes(data);

    final dynamic jsonMessage;
    try {
      jsonMessage = jsonDecode(stringData);
    } catch (error) {
      _log.warning('Failed to parse WebSocket message:\n$error\nMessage text:$stringData');
      return;
    }

    // Passphrase failures are reported with no request ID and with a Verb of 401. In this case, all future messages
    // will fail, so we can close out all completers with the same response and disconnect.
    if (jsonMessage['Verb'] == '${HttpResponseCode.unauthorized}') {
      _log.info('Passphrase rejected; disconnecting');

      _cancelHttpRequestsWithError(HttpResponseCode.unauthorized);

      // If we were mid connection attempt, the connection screen UI will handle showing the prompt if needed
      final bool bShowPassphraseDialog = _pendingConnectionAttempt == null;

      disconnect();

      if (bShowPassphraseDialog) {
        GenericModalDialogRoute.showDialog(
          context: rootNavigatorKey.currentContext!,
          builder: (context) => PassphraseModalDialog(
            initialErrorMessage: AppLocalizations.of(context)!.connectScreenPassphraseDisconnectErrorMessage,
          ),
        );
      }

      return;
    }

    // If we're in the middle of connecting, we don't want to dispatch messages to outside systems.
    // Unless the message is required for us to complete the connection handshake, queue it for later.
    if (_pendingConnectionAttempt != null && !_handshakeHttpMessageIds.contains(jsonMessage[_httpRequestIdFieldName])) {
      _initialPendingMessages.add(jsonMessage);

      // If this is a WebSocket message that needs to be handled during the handshake, call the internal callback even
      // if we won't call external ones
      final WebSocketMessageCallback? handshakeCallback = _handshakeMessageCallbacks[jsonMessage['Type']];
      handshakeCallback?.call(jsonMessage);

      return;
    }

    _handleWebSocketMessage(jsonMessage);
  }

  /// Handle a message received via WebSocket.
  void _handleWebSocketMessage(dynamic jsonMessage) {
    // Handle generic WebSocket messages
    String? messageType = jsonMessage['Type'];
    if (messageType != null) {
      // Call any listeners of this message.
      if (_messageCallbacks.containsKey(messageType)) {
        for (WebSocketMessageCallback callback in _messageCallbacks[messageType]!) {
          callback(jsonMessage);
        }
      }

      return;
    }

    // Handle HTTP responses
    final UnrealHttpResponse? decodedResponse = _decodeHttpResponseJsonData(jsonMessage);
    if (decodedResponse != null && _httpResponseCompleters.containsKey(decodedResponse.requestId)) {
      _httpResponseCompleters[decodedResponse.requestId]!.complete(decodedResponse);
      _httpResponseCompleters.remove(decodedResponse.requestId);
    }
  }

  /// Returns true if the current passphrase (if any) is accepted by the engine, and false otherwise.
  /// Note that if the passphrase is rejected, we will automatically disconnect before this returns false.
  Future<bool> _checkPassphrase() async {
    final UnrealHttpResponse response = await _internalSendHttpRequest(
      UnrealHttpRequest(
        url: '/remote/passphrase',
        verb: 'GET',
      ),
      bIsHandshakeMessage: true,
    );

    return response.code != HttpResponseCode.unauthorized;
  }

  /// Retrieve the API version from the engine, if possible.
  Future<EpicStageAppAPIVersion> _retrieveAPIVersion() async {
    const String stageAppLibraryPath = '/Script/EpicStageApp.Default__StageAppFunctionLibrary';
    const EpicStageAppAPIVersion defaultVersion = EpicStageAppAPIVersion(1, 0, 0);

    // Ask to describe the library first. If this fails, it won't dump an error in the editor log (unlike a failed
    // remote call), and we'll know the engine is an old version without an ESA API version.
    final UnrealHttpResponse describeResponse = await _internalSendHttpRequest(
      UnrealHttpRequest(
        url: '/remote/object/describe',
        verb: 'PUT',
        body: {
          'objectPath': stageAppLibraryPath,
        },
      ),
      bIsHandshakeMessage: true,
    );

    if (describeResponse.code != 200) {
      // The library doesn't exist, so the engine can't report a version. Assume oldest API.
      _log.info('Engine has no StageAppFunctionLibrary. Assuming default version.');
      return defaultVersion;
    }

    final UnrealHttpResponse callResponse = await _internalSendHttpRequest(
      UnrealHttpRequest(
        url: '/remote/object/call',
        verb: 'PUT',
        body: {
          'objectPath': stageAppLibraryPath,
          'functionName': 'GetAPIVersion',
        },
      ),
      bIsHandshakeMessage: true,
    );

    final String? returnValue = callResponse.body['ReturnValue'];
    if (returnValue == null) {
      _log.info('Failed to retrieve engine version. Assuming default version.');
      return defaultVersion;
    }

    return EpicStageAppAPIVersion.fromString(returnValue);
  }

  /// Send an HTTP request via the current WebSocket connection.
  /// The response's body will contain decoded JSON data.
  /// If [bIsHandshakeMessage] is true, the request ID will be added to [_handshakeHttpMessageIds].
  Future<UnrealHttpResponse> _internalSendHttpRequest(UnrealHttpRequest request, {bool bIsHandshakeMessage = false}) {
    final parameters = _generateHttpRequestParameters(request);
    final completer = _makeHttpResponseCompleterFromParameters(parameters);

    if (bIsHandshakeMessage) {
      _handshakeHttpMessageIds.add(_lastRequestId);
    }

    sendMessage('http', parameters);
    return completer.future;
  }

  /// Decode JSON contained in an HTTP response body, or return null if the response was invalid.
  UnrealHttpResponse? _decodeHttpResponseJsonData(dynamic jsonData) {
    final int? requestId = jsonData[_httpRequestIdFieldName];
    if (requestId == null) {
      return null;
    }

    final int? responseCode = jsonData['ResponseCode'];
    if (responseCode == null) {
      return null;
    }

    // It's possible to leave this out, so we don't null check it
    final dynamic responseBody = jsonData['ResponseBody'];

    return UnrealHttpResponse(
      requestId: requestId,
      code: responseCode,
      body: responseBody,
    );
  }

  /// Generate an object containing request parameters for the given HTTP request.
  dynamic _generateHttpRequestParameters(UnrealHttpRequest request) {
    ++_lastRequestId;
    return {
      _httpRequestIdFieldName: _lastRequestId,
      'Url': request.url,
      'Verb': request.verb,
      if (request.body != null) 'Body': request.body,
    };
  }

  /// Generate a completer that will complete its future when a response is received for the request with the given
  /// parameters.
  Completer<UnrealHttpResponse> _makeHttpResponseCompleterFromParameters(dynamic parameters) {
    final int? requestId = parameters[_httpRequestIdFieldName];

    if (requestId == null) {
      throw Exception('Expected a request ID to listen for');
    }

    final completer = Completer<UnrealHttpResponse>();
    _httpResponseCompleters[requestId] = completer;

    return completer;
  }

  /// Get the last connection stored in persisted  preferences and restore it into [_lastConnectionData] if we don't
  /// already have a connection saved.
  Future<ConnectionData?> _loadLastConnectionIfNone() async {
    final sharedPrefs = await SharedPreferences.getInstance();

    if (_lastConnectionData != null) {
      // We already connected somewhere newer
      return Future.value(_lastConnectionData);
    }

    // Get the last connection's data
    final String? name = sharedPrefs.getString('lastConnection.name');
    final String? address = sharedPrefs.getString('lastConnection.websocketAddress');
    final int? port = sharedPrefs.getInt('lastConnection.websocketPort');

    if (name != null && address != null && port != null) {
      ConnectionData? loadedConnectionData;
      try {
        loadedConnectionData = ConnectionData(
          name: name,
          websocketAddress: InternetAddress(address),
          websocketPort: port,
        );
      } catch (error) {
        _log.warning('Failed to restore connection data from preferences', error);
      }

      if (loadedConnectionData != null) {
        _lastConnectionData = loadedConnectionData;
      }
    }

    return Future.value(_lastConnectionData);
  }

  /// Save the last connection to persisted preferences.
  void _saveLastConnection(ConnectionData lastConnection) async {
    final sharedPrefs = await SharedPreferences.getInstance();

    sharedPrefs.setString('lastConnection.name', lastConnection.name);
    sharedPrefs.setString('lastConnection.websocketAddress', lastConnection.websocketAddress.address);
    sharedPrefs.setInt('lastConnection.websocketPort', lastConnection.websocketPort);
  }

  /// Complete all pending HTTP-over-WebSocket requests with the same error response.
  void _cancelHttpRequestsWithError(int errorCode) {
    for (final MapEntry<int, Completer<UnrealHttpResponse>> httpCompleterEntry in _httpResponseCompleters.entries) {
      final response = UnrealHttpResponse(
        requestId: httpCompleterEntry.key,
        code: errorCode,
        body: {},
      );
      httpCompleterEntry.value.complete(response);
    }
    _httpResponseCompleters.clear();
  }

  /// Called when the app is paused.
  void _onAppPaused() {
    if (connectionState == EngineConnectionState.disconnected && _pendingConnectionAttempt == null) {
      return;
    }

    _bShouldReconnectOnWake = true;
  }

  /// Called when the app resumes after being paused.
  void _onAppResumed() async {
    if (!_bShouldReconnectOnWake) {
      return;
    }

    _bShouldReconnectOnWake = false;
    if (connectionState == EngineConnectionState.connected || _pendingConnectionAttempt != null) {
      // We're still connected/connecting
      return;
    }

    // Show a temporary screen while we try to reconnect
    _goToNamedRoute(ReconnectScreen.route);

    try {
      await reconnect();
    } catch (e) {
      // Failed to connect, which will be handled below
    }

    if (connectionState == EngineConnectionState.connected) {
      _goToNamedRoute(StageAppMainScreen.route);
    } else {
      disconnect();
      _goToNamedRoute(ConnectScreen.route);
    }
  }

  /// Pop all screens and push a named route.
  void _goToNamedRoute(String name) {
    rootNavigatorKey.currentState?.pushNamedAndRemoveUntil(
      name,
      (route) => false,
    );
  }
}
