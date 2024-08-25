// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:epic_common/widgets.dart';
import 'package:flutter/material.dart';
import 'package:flutter_gen/gen_l10n/app_localizations.dart';

import '../../../elements/control_lock.dart';
import '../../../elements/place_actor_menu.dart';
import '../../../elements/sensitivity_slider.dart';
import '../sidebar/outliner_panel.dart';
import 'main_screen_tab_bar.dart';
import 'settings/settings_dialog.dart';

const double _buttonSpacing = 8;

/// The toolbar shown at the top of the [StageAppMainScreen].
class MainScreenToolbar extends StatelessWidget implements PreferredSizeWidget {
  const MainScreenToolbar({
    Key? key,
    required this.bEnableOutlinerToggle,
    required this.tabController,
    this.placeActorButtonKey,
  }) : super(key: key);

  /// Whether the outliner can be toggled in the current tab.
  final bool bEnableOutlinerToggle;

  /// Controller for which tab is selected.
  final TabController tabController;

  /// A key that can be used to refer to the place actors button within this toolbar.
  final Key? placeActorButtonKey;

  @override
  Size get preferredSize => const Size.fromHeight(48);

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.only(right: 8),
      decoration: BoxDecoration(
        color: Theme.of(context).colorScheme.surface,
      ),
      height: preferredSize.height,
      child: Row(
        children: [
          PlaceActorButton(key: placeActorButtonKey),
          Container(
            color: Theme.of(context).colorScheme.background,
            width: 4,
          ),
          const SizedBox(width: 4),
          MainScreenTabBar(tabController: tabController),
          const Spacer(),
          Wrap(
            spacing: _buttonSpacing,
            children: [
              OutlinerToggleButton(bEnabled: bEnableOutlinerToggle),
              const SensitivitySliderButton(),
              const ControlLock(),
              const SettingsButton(),
            ],
          ),
        ],
      ),
    );
  }
}

/// Button that opens the settings menu.
class SettingsButton extends StatelessWidget {
  const SettingsButton({Key? key}) : super(key: key);

  @override
  Widget build(BuildContext context) {
    return EpicIconButton(
      tooltipMessage: AppLocalizations.of(context)!.menuSettings,
      iconPath: 'packages/epic_common/assets/icons/settings.svg',
      onPressed: SettingsDialog.show,
    );
  }
}
