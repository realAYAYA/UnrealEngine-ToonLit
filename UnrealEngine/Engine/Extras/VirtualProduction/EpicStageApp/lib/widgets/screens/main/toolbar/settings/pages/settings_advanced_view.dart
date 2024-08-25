// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:epic_common/widgets.dart';
import 'package:flutter/material.dart';
import 'package:flutter_gen/gen_l10n/app_localizations.dart';
import 'package:provider/provider.dart';
import 'package:streaming_shared_preferences/streaming_shared_preferences.dart';

import '../../../../../../models/settings/connection_settings.dart';
import 'settings_compression_picker.dart';

/// Page showing the advanced settings.
class SettingsAdvancedView extends StatelessWidget {
  const SettingsAdvancedView({Key? key}) : super(key: key);

  static const String route = '/advanced';

  Widget build(BuildContext context) {
    return SettingsPageScaffold(
      title: AppLocalizations.of(context)!.settingsDialogAdvancedLabel,
      body: SizedBox(
        height: 200,
        child: MediaQuery.removePadding(
          removeTop: true,
          removeBottom: true,
          context: context,
          child: Column(children: [
            PreferenceBuilder(
              preference: Provider.of<ConnectionSettings>(context).webSocketCompressionMode,
              builder: (context, final WebSocketCompressionMode compressionMode) => SettingsMenuItem(
                title: AppLocalizations.of(context)!.settingsDialogCompressionLabel,
                iconPath: 'packages/epic_common/assets/icons/compression.svg',
                trailing: Text(getNameForWebSocketCompressionMode(context, compressionMode)),
                onTap: () => Navigator.of(context).pushNamed(SettingsCompressionPicker.route),
              ),
            ),
          ]),
        ),
      ),
    );
  }
}
