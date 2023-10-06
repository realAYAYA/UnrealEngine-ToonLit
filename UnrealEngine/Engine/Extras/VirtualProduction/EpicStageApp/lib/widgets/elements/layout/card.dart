// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:flutter/material.dart';

import '../../../utilities/unreal_colors.dart';
import '../asset_icon.dart';
import '../epic_icon_button.dart';

/// A large title bar for a [Card] including an icon and subtitle.
class CardLargeHeader extends StatelessWidget {
  const CardLargeHeader({
    Key? key,
    this.iconPath,
    this.title,
    this.subtitle,
    this.trailing,
  }) : super(key: key);

  final String? iconPath;
  final String? title;
  final String? subtitle;

  /// Optional widget to show on the opposite side to the header.
  final Widget? trailing;

  @override
  Widget build(BuildContext context) {
    return Container(
      height: 44,
      padding: const EdgeInsets.only(left: 16),
      alignment: Alignment.centerLeft,
      color: Theme.of(context).colorScheme.surfaceTint,
      child: Row(
        children: [
          if (iconPath != null)
            Padding(
              padding: EdgeInsets.only(right: 16),
              child: AssetIcon(path: iconPath!, size: 24),
            ),
          Expanded(
            child: RichText(
              overflow: TextOverflow.ellipsis,
              text: TextSpan(children: [
                if (title != null) TextSpan(text: title, style: Theme.of(context).textTheme.displayLarge),
                if (subtitle != null)
                  TextSpan(
                    text: (title != null) ? ' – $subtitle' : subtitle,
                    style: Theme.of(context).textTheme.displayMedium,
                  ),
              ]),
            ),
          ),
          if (trailing != null) trailing!,
        ],
      ),
    );
  }
}

/// A small title bar for a [Card].
class CardSmallHeader extends StatelessWidget {
  const CardSmallHeader({
    Key? key,
    required this.title,
  }) : super(key: key);

  final String title;

  @override
  Widget build(BuildContext context) {
    return Container(
      height: 44,
      padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 10),
      alignment: Alignment.centerLeft,
      color: Theme.of(context).colorScheme.surfaceTint,
      child: Text(
        title.toUpperCase(),
        style: Theme.of(context).textTheme.titleLarge,
      ),
    );
  }
}

/// Possible states for displaying a [CardListTile] which may have foldout children.
enum CardListTileExpansionState {
  /// This can't be expanded or collapsed.
  none,

  /// The list of children is currently collapsed.
  collapsed,

  /// The list of children is currently expanded.
  expanded,
}

/// A list tile shown in a [Card].
class CardListTile extends StatelessWidget {
  const CardListTile({
    Key? key,
    required this.title,
    required this.bIsSelected,
    this.stateIconPath,
    this.iconPath,
    this.trailing,
    this.onTap,
    this.onTapIcon,
    this.bDeEmphasize = false,
    this.expansionState = CardListTileExpansionState.none,
    this.indentation = 0,
  }) : super(key: key);

  /// The label to display on the item.
  final String title;

  /// Whether this is currently selected.
  final bool bIsSelected;

  /// If provided, show a small icon with the image at this path before the rest of the contents to indicate state.
  final String? stateIconPath;

  /// If provided, show an icon with the image at this path.
  final String? iconPath;

  /// Optional widget to show after the title.
  final Widget? trailing;

  /// Function called when the user taps this item.
  final void Function()? onTap;

  /// Function called when the user taps the icon, if present.
  final void Function()? onTapIcon;

  /// If true, fade the text and icon of this panel.
  final bool bDeEmphasize;

  /// Current state indicating whether this is expandable and if so, whether it's currently expanded.
  final CardListTileExpansionState expansionState;

  /// How many layers deep this item is indented.
  final int indentation;

  @override
  Widget build(BuildContext context) {
    final ThemeData theme = Theme.of(context);
    final ColorScheme colorScheme = theme.colorScheme;

    final double opacity = bDeEmphasize ? 0.4 : 1.0;
    final Color textColor = (bIsSelected ? colorScheme.onPrimary : colorScheme.onSurface).withOpacity(opacity);
    final TextStyle textStyle = theme.textTheme.headlineSmall!.copyWith(
      color: textColor,
      fontStyle: bDeEmphasize ? FontStyle.italic : FontStyle.normal,
    );

    Widget? stateIcon = null;
    if (stateIconPath != null) {
      stateIcon = AssetIcon(
        path: stateIconPath!,
        size: 16,
      );
    }

    Widget? icon = null;
    if (iconPath != null) {
      icon = AssetIcon(
        path: iconPath!,
        size: 24,
        color: Color.fromRGBO(255, 255, 255, opacity),
      );
    }

    final bool bIsExpandable = (expansionState == CardListTileExpansionState.collapsed) ||
        (expansionState == CardListTileExpansionState.expanded);

    Color color = Colors.transparent;
    if (bIsSelected) {
      color = Theme.of(context).colorScheme.primary;
    } else if (bIsExpandable) {
      color = UnrealColors.gray13;
    }

    final Widget mainBody = Padding(
      padding: EdgeInsets.only(top: 2),
      child: Container(
        color: color,
        height: 40,
        child: ListTile(
          leading: Row(
            mainAxisSize: MainAxisSize.min,
            children: [
              if (indentation > 0) SizedBox(width: 20.0 * indentation),
              if (stateIcon != null)
                Padding(
                  padding: EdgeInsets.only(right: 10),
                  child: stateIcon,
                ),
              SizedBox.square(dimension: 24, child: icon),
            ],
          ),
          trailing: trailing,
          title: Text(
            title,
            style: textStyle,
            softWrap: false,
            overflow: TextOverflow.ellipsis,
          ),
          horizontalTitleGap: 8,
          minLeadingWidth: 0,
          contentPadding: EdgeInsets.only(
            left: 14,
            right: 8,
          ),
          visualDensity: const VisualDensity(vertical: -4),
          onTap: onTap,
        ),
      ),
    );

    if (expansionState == CardListTileExpansionState.none) {
      return mainBody;
    }

    // Place a gesture detector to intercept taps on the icon's area
    if (icon != null && onTapIcon != null) {
      return Stack(
        clipBehavior: Clip.none,
        children: [
          mainBody,
          Positioned(
            top: 0,
            bottom: 0,
            width: 48,
            child: GestureDetector(
              onTap: onTapIcon,
            ),
          ),
        ],
      );
    } else {
      return mainBody;
    }
  }
}

/// A button revealed in a [SwipeRevealer] in conjunction with a [CardListTile].
class CardListTileSwipeAction extends StatelessWidget {
  const CardListTileSwipeAction({
    Key? key,
    required this.iconPath,
    required this.color,
    required this.onPressed,
    this.iconSize = 24,
  }) : super(key: key);

  /// The path of the image file to show inside the button.
  final String iconPath;

  /// Color of the button background.
  final Color color;

  /// Function to call when this is pressed.
  final Function() onPressed;

  /// The size of the icon (both width and height).
  final double iconSize;

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.only(top: 2),
      child: EpicIconButton(
        iconPath: iconPath,
        bIsToggledOn: true,
        iconSize: iconSize,
        activeBackgroundColor: color,
        buttonSize: const Size(64, 40),
        onPressed: onPressed,
      ),
    );
  }
}

/// A sub-header in a [Card].
class CardSubHeader extends StatelessWidget {
  const CardSubHeader({
    Key? key,
    required this.child,
    this.padding,
    this.height = 48,
  }) : super(key: key);

  final Widget child;
  final EdgeInsetsGeometry? padding;
  final double height;

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: padding,
      height: height,
      color: Theme.of(context).colorScheme.surfaceTint,
      child: child,
    );
  }
}

/// An icon button shown in a card sub-header.
class CardSubHeaderButton extends StatelessWidget {
  const CardSubHeaderButton({
    Key? key,
    required this.iconPath,
    required this.tooltipMessage,
    this.onPressed,
    this.bIsToggledOn = false,
    this.bIsVisualOnly = false,
  }) : super(key: key);

  /// The path of the image file to show inside the button.
  final String iconPath;

  /// An optional tooltip message to show when the button is hovered or long pressed.
  final String tooltipMessage;

  /// The function to call when the button is pressed. If not provided and [bIsVisualOnly] is false, the button is
  /// considered inactive and will change color accordingly.
  final Function()? onPressed;

  /// If true, highlight the button using the [activeBackgroundColor].
  final bool bIsToggledOn;

  /// If true, leave out button semantics and only include the button's visuals.
  /// This will disable the tooltip message.
  final bool bIsVisualOnly;

  @override
  Widget build(BuildContext context) {
    return EpicIconButton(
      iconPath: iconPath,
      tooltipMessage: tooltipMessage,
      onPressed: onPressed,
      buttonSize: const Size(48, 48),
      bIsToggledOn: bIsToggledOn,
      bIsVisualOnly: bIsVisualOnly,
    );
  }
}
