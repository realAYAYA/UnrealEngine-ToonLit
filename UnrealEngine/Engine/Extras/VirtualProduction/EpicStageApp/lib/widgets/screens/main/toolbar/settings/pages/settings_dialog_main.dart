// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:epic_common/theme.dart';
import 'package:epic_common/utilities/version.dart';
import 'package:epic_common/widgets.dart';
import 'package:flutter/material.dart';
import 'package:flutter_gen/gen_l10n/app_localizations.dart';
import 'package:provider/provider.dart';
import 'package:url_launcher/url_launcher.dart';

import '../../../../../../models/engine_connection.dart';
import '../../../../../../models/navigator_keys.dart';
import '../../../../../../models/settings/selected_actor_settings.dart';
import '../../../../../../models/unreal_actor_manager.dart';
import '../../../../eula/eula_screen.dart';
import 'settings_advanced_view.dart';
import 'settings_log_list.dart';
import 'settings_root_actor_picker.dart';

/// Main screen of the [SettingsDialog].
class SettingsDialogMain extends StatelessWidget {
  static const String route = '/';

  const SettingsDialogMain({Key? key}) : super(key: key);

  final String _documentationUri =
      'https://dev.epicgames.com/community/learning/courses/OKO/unreal-engine-unreal-stage-ios-app/';

  @override
  Widget build(BuildContext context) {
    final actorManager = Provider.of<UnrealActorManager>(context, listen: false);
    final connection = Provider.of<EngineConnectionManager>(context, listen: false);

    final bool bIsConnected = connection.connectionState == EngineConnectionState.connected;
    final localizations = AppLocalizations.of(context)!;

    return SettingsPageScaffold(
      title: AppLocalizations.of(context)!.settingsDialogTitle,
      body: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          if (bIsConnected)
            StreamBuilder(
              stream: Provider.of<SelectedActorSettings>(context, listen: false).displayClusterRootPath,
              builder: (_, final AsyncSnapshot<String> rootPath) => SettingsMenuItem(
                title: localizations.settingsDialogRootActorSettingLabel,
                iconPath: 'packages/epic_common/assets/icons/ndisplay.svg',
                trailing: Text(actorManager.getActorAtPath(rootPath.data ?? '')?.name ?? ''),
                onTap: () => Navigator.of(context).pushNamed(SettingsDialogRootActorPicker.route),
              ),
            ),
          if (bIsConnected) const SettingsMenuDivider(),
          SettingsMenuItem(
            title: localizations.settingsDialogApplicationLogLabel,
            iconPath: 'packages/epic_common/assets/icons/log.svg',
            onTap: () => Navigator.of(context).pushNamed(SettingsLogList.route),
          ),
          SettingsMenuItem(
            title: localizations.settingsDialogAdvancedLabel,
            iconPath: 'packages/epic_common/assets/icons/advanced.svg',
            onTap: () => Navigator.of(context).pushNamed(SettingsAdvancedView.route),
          ),
          const SettingsMenuDivider(),
          SettingsMenuItem(
            title: localizations.settingsDialogApplicationHelpLabel,
            iconPath: 'packages/epic_common/assets/icons/help.svg',
            trailingIconPath: null,
            onTap: () => launchUrl(Uri.parse(_documentationUri)),
          ),
          SettingsMenuItem(
            title: localizations.settingsDialogApplicationAboutLabel,
            iconPath: 'packages/epic_common/assets/icons/info.svg',
            onTap: () => rootNavigatorKey.currentState?.pushNamed(EulaScreen.route, arguments: {'onPressed': () {}}),
          ),
          const SettingsMenuDivider(),
          Container(
            padding: EdgeInsets.symmetric(horizontal: 8),
            height: 54,
            child: Row(
              children: [
                if (bIsConnected) const _DisconnectButton(),
                const Spacer(),
                FutureBuilder<String>(
                  future: getFriendlyPackageVersion(),
                  builder: (context, snapshot) => Text(
                    snapshot.data ?? '',
                    style: Theme.of(context).textTheme.labelMedium,
                  ),
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}

/// A button that disconnects from the engine.
class _DisconnectButton extends StatelessWidget {
  const _DisconnectButton({Key? key}) : super(key: key);

  @override
  Widget build(BuildContext context) {
    return Semantics(
      button: true,
      child: MouseRegion(
        cursor: MaterialStateMouseCursor.clickable,
        child: GestureDetector(
          onTap: () => _disconnect(context),
          behavior: HitTestBehavior.opaque,
          child: Container(
            height: 36,
            padding: EdgeInsets.symmetric(horizontal: 24),
            decoration: BoxDecoration(
              borderRadius: BorderRadius.circular(20),
              border: Border.all(
                width: 2,
                color: UnrealColors.warning,
              ),
            ),
            child: Center(
              child: Row(children: [
                AssetIcon(
                  path: 'packages/epic_common/assets/icons/exit.svg',
                  size: 24,
                  color: UnrealColors.warning,
                ),
                const SizedBox(width: 8),
                Text(
                  AppLocalizations.of(context)!.settingsDialogDisconnectButtonLabel,
                  style: Theme.of(context).textTheme.headlineSmall!.copyWith(
                        color: UnrealColors.warning,
                      ),
                ),
              ]),
            ),
          ),
        ),
      ),
    );
  }

  /// Disconnect from the engine
  void _disconnect(BuildContext context) {
    Provider.of<EngineConnectionManager>(context, listen: false).disconnect();
  }
}
