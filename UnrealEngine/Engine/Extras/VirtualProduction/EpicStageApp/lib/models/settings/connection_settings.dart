// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:epic_common/preferences.dart';
import 'package:flutter/material.dart';
import 'package:flutter_gen/gen_l10n/app_localizations.dart';
import 'package:streaming_shared_preferences/streaming_shared_preferences.dart';

/// Compression modes that can be used for WebSocket traffic.
enum WebSocketCompressionMode {
  none,
  zlib,
}

/// Given a [context] with localization data, get the name corresponding to a WebSocket [compressionMode].
String getNameForWebSocketCompressionMode(BuildContext context, WebSocketCompressionMode compressionMode) {
  final localizations = AppLocalizations.of(context)!;

  switch (compressionMode) {
    case WebSocketCompressionMode.none:
      return localizations.compressionModeNone;

    case WebSocketCompressionMode.zlib:
      return localizations.compressionModeZlib;
  }
}

/// Holds user settings used to set up the connection to the engine.
class ConnectionSettings {
  ConnectionSettings(PreferencesBundle preferences)
      : webSocketCompressionMode = preferences.persistent.getEnum<WebSocketCompressionMode>(
          'connection.webSocketCompressionMode',
          defaultValue: WebSocketCompressionMode.zlib,
          enumValues: WebSocketCompressionMode.values,
        );

  /// Which compression mode to use for WebSocket traffic.
  final Preference<WebSocketCompressionMode> webSocketCompressionMode;
}
