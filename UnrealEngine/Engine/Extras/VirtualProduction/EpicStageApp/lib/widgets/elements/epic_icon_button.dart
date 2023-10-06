// Copyright Epic Games, Inc. All Rights Reserved.

import 'dart:ui';

import 'package:flutter/material.dart';

import '../../utilities/unreal_colors.dart';
import 'asset_icon.dart';

/// A standard-sized button with arbitrary contents.
class EpicGenericButton extends StatelessWidget {
  const EpicGenericButton({
    Key? key,
    required this.child,
    this.tooltipMessage,
    this.onPressed,
    this.size,
    this.backgroundSize,
    this.backgroundShape,
    this.bIsToggledOn = false,
    this.color,
    this.activeBackgroundColor,
    this.bIsVisualOnly = false,
    this.onLongPressed,
  }) : super(key: key);

  final Widget child;

  /// An optional tooltip message to show when the button is hovered or long pressed.
  final String? tooltipMessage;

  /// The function to call when the button is pressed. If not provided and [bIsVisualOnly] if false, the button is
  /// considered inactive and will change color accordingly.
  final Function()? onPressed;

  /// The function to call when the button is long pressed
  final Function()? onLongPressed;

  /// The size of the button.
  final Size? size;

  /// The size of the button's background when visible.
  final Size? backgroundSize;

  /// The shape of the button's background when visible.
  final BoxShape? backgroundShape;

  /// If true, highlight the button using the [activeBackgroundColor].
  final bool bIsToggledOn;

  /// The color of the button's contents.
  final Color? color;

  /// The background color of the button when it's toggled on.
  final Color? activeBackgroundColor;

  /// If true, leave out interactive behaviour and only include the button's visuals.
  /// This will disable the tooltip message.
  final bool bIsVisualOnly;

  @override
  Widget build(BuildContext context) {
    Color? backgroundColor = null;

    if (bIsToggledOn) {
      if (onPressed == null && !bIsVisualOnly && onLongPressed == null) {
        // Disabled color
        backgroundColor = UnrealColors.gray42;
      } else {
        // Toggled on color
        backgroundColor = activeBackgroundColor ?? Theme.of(context).colorScheme.primary;
      }
    }

    Widget contents = Container(
      decoration: BoxDecoration(
        color: backgroundColor,
        shape: backgroundShape ?? BoxShape.rectangle,
      ),
      width: backgroundSize?.width ?? size?.width ?? 48,
      height: backgroundSize?.height ?? size?.height ?? 48,
      child: DefaultTextStyle(
        style: Theme.of(context).textTheme.bodyMedium!.copyWith(color: color ?? UnrealColors.white),
        child: Center(
          child: child,
        ),
      ),
    );

    // Add button semantics
    if (!bIsVisualOnly) {
      contents = Semantics(
        button: true,
        child: MouseRegion(
          cursor: MaterialStateMouseCursor.clickable,
          child: GestureDetector(
            child: contents,
            onTap: onPressed,
            behavior: HitTestBehavior.opaque,
            onLongPress: onLongPressed,
          ),
        ),
      );
    }

    if (tooltipMessage == null) {
      return contents;
    }

    return Tooltip(
      message: tooltipMessage,
      child: contents,
    );
  }
}

/// A standard-sized button with an icon.
class EpicIconButton extends StatelessWidget {
  const EpicIconButton({
    Key? key,
    required this.iconPath,
    this.tooltipMessage,
    this.onPressed,
    this.bIsToggledOn = false,
    this.iconSize = 24,
    this.buttonSize,
    this.backgroundSize,
    this.backgroundShape,
    this.color,
    this.activeColor,
    this.activeBackgroundColor,
    this.bIsVisualOnly = false,
    this.onLongPressed,
  }) : super(key: key);

  /// The path of the image file to show inside the button.
  final String iconPath;

  /// An optional tooltip message to show when the button is hovered or long pressed.
  final String? tooltipMessage;

  /// The function to call when the button is pressed. If not provided and [bIsVisualOnly] if false, the button is
  /// considered inactive and will change color accordingly.
  final Function()? onPressed;

  /// The function to call when the button is Long pressed. If not provided and
  /// [bIsVisualOnly] if false, the button is
  /// considered inactive and will change color accordingly.
  final Function()? onLongPressed;

  /// If true, highlight the button using the [activeBackgroundColor].
  final bool bIsToggledOn;

  /// The size of the icon (both width and height).
  final double iconSize;

  /// The size of the button's pressable area.
  final Size? buttonSize;

  /// The size of the button's background when visible.
  final Size? backgroundSize;

  /// The shape of the button's background when visible.
  final BoxShape? backgroundShape;

  /// The color of the button's contents.
  final Color? color;

  /// The color of the button's when the button is toggled on.
  final Color? activeColor;

  /// The background color of the button when it's toggled on.
  final Color? activeBackgroundColor;

  /// If true, leave out button semantics and only include the button's visuals.
  /// This will disable the tooltip message.
  final bool bIsVisualOnly;

  @override
  Widget build(BuildContext context) {
    Color? iconColor = color;

    if (bIsToggledOn) {
      iconColor = activeColor;
    } else if (onPressed == null && onLongPressed == null && !bIsVisualOnly) {
      iconColor = UnrealColors.gray42;
    }

    return EpicGenericButton(
      child: AssetIcon(
        path: iconPath,
        color: iconColor,
        size: iconSize,
      ),
      tooltipMessage: tooltipMessage,
      onPressed: onPressed,
      onLongPressed: onLongPressed,
      bIsToggledOn: bIsToggledOn,
      color: color,
      size: buttonSize,
      backgroundSize: backgroundSize,
      backgroundShape: backgroundShape,
      activeBackgroundColor: activeBackgroundColor,
      bIsVisualOnly: bIsVisualOnly,
    );
  }
}

/// A standard-sized rounded button containing text.
class EpicLozengeButton extends StatelessWidget {
  const EpicLozengeButton({
    Key? key,
    required this.label,
    this.width,
    this.color,
    this.onPressed,
  }) : super(key: key);

  /// The button's label.
  final String label;

  /// If set, force the button to be exactly this width.
  final double? width;

  /// The color of the button's contents.
  /// Defaults to the theme's primary color. If this is transparent, the text will be colored darker.
  final Color? color;

  /// The function to call when the button is pressed. If not provided and [bIsVisualOnly] if false, the button is
  /// considered inactive and will change color accordingly.
  final Function()? onPressed;

  @override
  Widget build(BuildContext context) {
    final ThemeData theme = Theme.of(context);

    return Semantics(
      button: true,
      child: MouseRegion(
        cursor: MaterialStateMouseCursor.clickable,
        child: GestureDetector(
          onTap: onPressed,
          behavior: HitTestBehavior.opaque,
          child: Container(
            width: width,
            height: 36,
            padding: EdgeInsets.symmetric(vertical: 10, horizontal: 28),
            decoration: BoxDecoration(
              color: color ?? theme.colorScheme.primary,
              borderRadius: BorderRadius.circular(20),
            ),
            child: Center(
              child: Text(
                label,
                style: theme.textTheme.headlineSmall!.copyWith(
                  color: color?.alpha == 0 ? theme.colorScheme.onSurface : theme.colorScheme.onPrimary,
                ),
              ),
            ),
          ),
        ),
      ),
    );
  }
}

/// A standard-sized short, wide button with text and an icon.
class EpicWideButton extends StatelessWidget {
  const EpicWideButton({
    Key? key,
    required this.text,
    required this.iconPath,
    this.onPressed,
    this.color,
    this.iconColor,
    this.textColor,
    this.border,
  }) : super(key: key);

  /// The text to show within the button.
  final String text;

  /// The path of the asset to use for the icon within the button.
  final String iconPath;

  /// Callback function for when the user presses the button.
  final Function()? onPressed;

  /// An optional color for the button's background. If not provided, uses the surfaceTint of the theme's color scheme.
  final Color? color;

  /// An optional color for the button's icon.
  final Color? iconColor;

  /// An optional color for the button's text. If not provided, uses white.
  final Color? textColor;

  /// An optional border to show around the button. If not provided, no border is shown.
  final Border? border;

  @override
  Widget build(BuildContext context) {
    return Semantics(
      button: true,
      child: MouseRegion(
        cursor: MaterialStateMouseCursor.clickable,
        child: GestureDetector(
          onTap: onPressed,
          behavior: HitTestBehavior.opaque,
          child: Container(
            height: 32,
            decoration: BoxDecoration(
              color: color ?? UnrealColors.gray22,
              border: border,
              borderRadius: BorderRadius.circular(4),
            ),
            padding: EdgeInsets.only(left: 6, right: 8),
            child: Row(
              mainAxisSize: MainAxisSize.min,
              mainAxisAlignment: MainAxisAlignment.center,
              crossAxisAlignment: CrossAxisAlignment.center,
              children: [
                AssetIcon(
                  path: iconPath,
                  size: 24,
                  color: iconColor,
                ),
                const SizedBox(width: 6),
                Text(
                  text,
                  style: Theme.of(context).textTheme.bodyMedium!.copyWith(
                    color: textColor ?? UnrealColors.white,
                    fontVariations: [FontVariation('wght', 600)],
                  ),
                ),
              ],
            ),
          ),
        ),
      ),
    );
  }
}
