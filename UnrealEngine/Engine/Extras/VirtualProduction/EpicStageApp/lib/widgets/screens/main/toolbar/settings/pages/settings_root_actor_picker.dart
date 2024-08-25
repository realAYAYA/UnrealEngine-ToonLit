// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:epic_common/widgets.dart';
import 'package:flutter/material.dart';
import 'package:flutter_gen/gen_l10n/app_localizations.dart';
import 'package:provider/provider.dart';

import '../../../../../../models/settings/selected_actor_settings.dart';
import '../../../../../../models/unreal_actor_manager.dart';
import '../../../../../../models/unreal_types.dart';
import '../../../../../../utilities/constants.dart';
import '../../../../../../utilities/guarded_refresh_state.dart';

/// Page showing the list of nDisplay root actors to pick from.
class SettingsDialogRootActorPicker extends StatefulWidget {
  const SettingsDialogRootActorPicker({Key? key}) : super(key: key);

  static const String route = '/select_root_actor';

  @override
  State<SettingsDialogRootActorPicker> createState() => _SettingsDialogRootActorPickerState();
}

class _SettingsDialogRootActorPickerState extends State<SettingsDialogRootActorPicker> with GuardedRefreshState {
  late final UnrealActorManager _actorManager;

  @override
  void initState() {
    super.initState();

    _actorManager = Provider.of<UnrealActorManager>(context, listen: false);
    _actorManager.watchClassName(nDisplayRootActorClassName, refreshOnData);
  }

  @override
  void dispose() {
    _actorManager.stopWatchingClassName(nDisplayRootActorClassName, refreshOnData);
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final selectedActorSettings = Provider.of<SelectedActorSettings>(context, listen: false);

    return SettingsPageScaffold(
      title: AppLocalizations.of(context)!.settingsDialogRootActorSettingLabel,
      body: StreamBuilder(
        stream: selectedActorSettings.displayClusterRootPath,
        builder: (_, final AsyncSnapshot<String> rootActorPath) => Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            for (final UnrealObject actor in _actorManager.getActorsOfClass(nDisplayRootActorClassName))
              SettingsMenuItem(
                title: actor.name,
                iconPath: 'packages/epic_common/assets/icons/ndisplay.svg',
                trailingIconPath:
                    rootActorPath.data == actor.path ? 'packages/epic_common/assets/icons/check.svg' : null,
                onTap: () => selectedActorSettings.displayClusterRootPath.setValue(actor.path),
              ),
          ],
        ),
      ),
    );
  }
}
