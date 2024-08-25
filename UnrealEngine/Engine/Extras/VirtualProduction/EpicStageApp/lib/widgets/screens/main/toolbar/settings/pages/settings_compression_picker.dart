// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:epic_common/widgets.dart';
import 'package:flutter/material.dart';
import 'package:flutter_gen/gen_l10n/app_localizations.dart';
import 'package:provider/provider.dart';
import 'package:streaming_shared_preferences/streaming_shared_preferences.dart';

import '../../../../../../models/engine_connection.dart';
import '../../../../../../models/settings/connection_settings.dart';

/// Page showing the possible compression modes.
class SettingsCompressionPicker extends StatelessWidget {
  const SettingsCompressionPicker({Key? key}) : super(key: key);

  static const String route = '/advanced/compression';

  Widget build(BuildContext context) {
    final ConnectionSettings connectionSettings = Provider.of<ConnectionSettings>(context);

    return SettingsPageScaffold(
      title: AppLocalizations.of(context)!.settingsDialogCompressionLabel,
      body: SizedBox(
        height: 200,
        child: MediaQuery.removePadding(
          removeTop: true,
          removeBottom: true,
          context: context,
          child: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              Column(
                children: [
                  for (final WebSocketCompressionMode mode in WebSocketCompressionMode.values)
                    PreferenceBuilder(
                      preference: connectionSettings.webSocketCompressionMode,
                      builder: (context, final WebSocketCompressionMode selectedMode) => SettingsMenuItem(
                        title: getNameForWebSocketCompressionMode(context, mode),
                        iconPath: mode == WebSocketCompressionMode.none
                            ? null
                            : 'packages/epic_common/assets/icons/compression.svg',
                        trailingIconPath: selectedMode == mode ? 'packages/epic_common/assets/icons/check.svg' : null,
                        onTap: () => connectionSettings.webSocketCompressionMode.setValue(mode),
                      ),
                    ),
                ],
              ),
              Spacer(),
              if (Provider.of<EngineConnectionManager>(context).connectionState == EngineConnectionState.connected)
                Padding(
                  padding: EdgeInsets.only(bottom: 4),
                  child: Text(
                    AppLocalizations.of(context)!.reconnectSettingMessage,
                    style: Theme.of(context).textTheme.labelMedium,
                  ),
                ),
            ],
          ),
        ),
      ),
    );
  }
}
