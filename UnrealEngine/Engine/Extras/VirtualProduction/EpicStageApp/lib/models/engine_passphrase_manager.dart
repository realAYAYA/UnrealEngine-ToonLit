// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:flutter_secure_storage/flutter_secure_storage.dart';

import '../utilities/net_utilities.dart';

/// Handles storing and retrieving passphrases to use for each engine instance.
class EnginePassphraseManager {
  final FlutterSecureStorage _secureStorage = FlutterSecureStorage();

  /// Get the passphrase associated with the given [connectionData], or null if one isn't saved.
  Future<String?> getPassphrase(ConnectionData connectionData) {
    return _secureStorage.read(key: _makeKeyForConnection(connectionData));
  }

  /// Set the [passphrase] associated with the given [connectionData], or delete it if no passphrase is given.
  void setPassphrase(ConnectionData connectionData, String? passphrase) {
    final String key = _makeKeyForConnection(connectionData);

    if (passphrase == null) {
      _secureStorage.delete(key: key);
    } else {
      _secureStorage.write(key: key, value: passphrase);
    }
  }

  /// Given [connectionData], create a unique key to refer to its passphrase in the secure storage database.
  String _makeKeyForConnection(ConnectionData connectionData) {
    return 'enginePassphrase.${connectionData.websocketAddress.address}';
  }
}
