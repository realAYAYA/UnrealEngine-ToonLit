// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:flutter/cupertino.dart';
import 'package:flutter/material.dart';

/// A button with no fill and a single colored app_icon visible.
class CupertinoIconButton extends StatelessWidget {
  const CupertinoIconButton(
      {this.bEnabled = true,
      this.iconSize,
      this.description = '',
      this.color,
      this.onPressed,
      this.icon = CupertinoIcons.question,
      this.xPadding,
      this.pressedOpacity = 0.4,
      Key? key})
      : super(key: key);

  /// Size of the icon.
  final double? iconSize;

  /// Color of the app_icon.
  final Color? color;

  /// Whether the button is enabled. If false, events won't be called and the app_icon will be greyed out.
  final bool bEnabled;

  /// Callback for when the button is pressed.
  final void Function()? onPressed;

  /// The app_icon to display.
  final IconData icon;

  /// Description of the button shown as a tooltip when hovered/long-pressed.
  final String description;

  /// The opacity that the button will fade to when pressed.
  final double? pressedOpacity;

  /// Padding along the x-axis on either side.
  final double? xPadding;

  @override
  Widget build(BuildContext context) {
    Color colorToShow;
    if (bEnabled) {
      if (color != null) {
        colorToShow = color!;
      } else {
        colorToShow = CupertinoTheme.of(context).primaryColor;
      }
    } else {
      colorToShow = CupertinoDynamicColor.resolve(CupertinoColors.inactiveGray, context);
    }

    final double size = iconSize ?? const CupertinoIconThemeData().resolve(context).size ?? 24.0;

    return SizedBox(
        width: size + ((xPadding ?? 6) * 2),
        height: size,
        child: Tooltip(
            message: description,
            waitDuration: const Duration(seconds: 1),
            child: CupertinoButton(
              padding: EdgeInsets.zero,
              onPressed: bEnabled ? onPressed : null,
              pressedOpacity: pressedOpacity,
              child: Icon(icon, size: size, color: colorToShow),
            )));
  }
}
