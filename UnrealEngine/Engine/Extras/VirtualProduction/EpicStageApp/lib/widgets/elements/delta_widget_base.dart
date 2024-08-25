// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:epic_common/widgets.dart';
import 'package:flutter/material.dart';
import 'package:flutter_gen/gen_l10n/app_localizations.dart';
import 'package:provider/provider.dart';

import '../../models/settings/delta_widget_settings.dart';

class DeltaWidgetConstants {
  /// Size of icon buttons used for delta widgets.
  static const double deltaButtonIconSize = 32.0;

  /// Default horizontal padding for delta widget buttons.
  static const double defaultButtonXPadding = 6.0;

  /// Size of an icon button including the default padding on both sides.
  static const double deltaButtonSize = deltaButtonIconSize + defaultButtonXPadding * 2;

  /// Padding between the edge of a widget's container and its visible components.
  static const double widgetOuterXPadding = 10.0;

  /// Horizontal offset between a widget's label and the widget control itself.
  static const double widgetInnerXPadding = 2.0;
}

/// A button to reset the value controlled by a widget.
class ResetValueButton extends StatelessWidget {
  const ResetValueButton(
      {this.onPressed, this.bEnabled = true, this.xPadding = DeltaWidgetConstants.defaultButtonXPadding, Key? key})
      : super(key: key);

  /// Callback for when the button is pressed.
  final Function()? onPressed;

  /// Whether the button is enabled. If false, events won't be called and the app_icon will be greyed out.
  final bool bEnabled;

  /// Padding along the x-axis on either side.
  final double? xPadding;

  @override
  Widget build(BuildContext context) {
    return EpicIconButton(
      onPressed: onPressed,
      iconPath: 'packages/epic_common/assets/icons/reset.svg',
      tooltipMessage: AppLocalizations.of(context)!.propertyResetButtonTooltip,
      iconSize: 24,
      buttonSize: const Size(32, 32),
    );
  }
}

/// Mixin that adds state for any widgets that use delta-based controls.
mixin DeltaWidgetStateMixin<T extends StatefulWidget> on State<T> {
  /// If true, ignore the sensitivity settings and just use the base delta multiplier.
  bool get bIgnoreSensitivity => false;

  /// Base scroll rate of a value (generally multiplied by the gesture's delta movement divided the widget's size).
  /// Override this to change the base scroll rate for a widget.
  double get baseDeltaMultiplier => 0.8;

  /// The effective multiplier, combining the base rate and any other scalars.
  double get deltaMultiplier =>
      baseDeltaMultiplier *
      (bIgnoreSensitivity ? 1.0 : Provider.of<DeltaWidgetSettings>(context, listen: false).sensitivity.getValue());
}

/// Data needed to display a control representing a single object's property on a delta-based widget.
class DeltaWidgetValueData<T> {
  const DeltaWidgetValueData({required this.value});

  /// The value the dot represents.
  final T value;
}
