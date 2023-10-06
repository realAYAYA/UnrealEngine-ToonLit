// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:flutter/material.dart';
import 'package:flutter_gen/gen_l10n/app_localizations.dart';
import 'package:provider/provider.dart';

import '../../models/settings/delta_widget_settings.dart';
import '../../utilities/constants.dart';
import '../../utilities/transient_preference.dart';
import 'epic_icon_button.dart';

/// Button to toggle reset mode on and off.
class ResetModeButton extends StatelessWidget {
  const ResetModeButton({Key? key}) : super(key: key);

  @override
  Widget build(BuildContext context) {
    final deltaSettings = Provider.of<DeltaWidgetSettings>(context);

    return Padding(
      padding: EdgeInsets.only(right: cardMargin),
      child: TransientPreferenceBuilder(
        preference: Provider.of<DeltaWidgetSettings>(context, listen: false).bIsInResetMode,
        builder: (context, final bool bIsInResetMode) => EpicIconButton(
          iconPath: 'assets/images/icons/reset_small.svg',
          iconSize: 17,
          backgroundSize: const Size(33, 33),
          tooltipMessage: AppLocalizations.of(context)!.resetModeToggleButtonTooltip,
          backgroundShape: BoxShape.circle,
          onPressed: () => deltaSettings.bIsInResetMode.setValue(!bIsInResetMode),
          bIsToggledOn: bIsInResetMode,
        ),
      ),
    );
  }
}
