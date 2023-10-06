// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:flutter/material.dart';

import '../../../../../models/navigator_keys.dart';
import '../../../../../utilities/unreal_colors.dart';
import '../../../../elements/asset_icon.dart';
import '../../../../elements/epic_icon_button.dart';
import 'settings_dialog.dart';

/// Outer scaffold to use for all settings pages.
class SettingsPageScaffold extends StatelessWidget {
  const SettingsPageScaffold({
    Key? key,
    required this.title,
    required this.body,
    this.titleBarTrailing,
  }) : super(key: key);

  /// The title of the page.
  final String title;

  /// The main contents of the page.
  final Widget body;

  /// Widget to add trailing the title bar.
  final Widget? titleBarTrailing;

  @override
  Widget build(BuildContext context) {
    final bool bCanPop = Navigator.of(context).canPop();

    return Column(
      mainAxisSize: MainAxisSize.min,
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        Container(
          height: 40,
          color: Theme.of(context).colorScheme.surfaceTint,
          padding: const EdgeInsets.symmetric(horizontal: 4),
          child: Stack(
            alignment: Alignment.center,
            clipBehavior: Clip.none,
            children: [
              Positioned(
                left: 0,
                child: bCanPop
                    ? EpicIconButton(
                        iconPath: 'assets/images/icons/chevron_left.svg',
                        onPressed: () => Navigator.of(context).maybePop(),
                      )
                    : EpicIconButton(
                        iconPath: 'assets/images/icons/close.svg',
                        onPressed: () => Navigator.of(rootNavigatorKey.currentContext!).maybePop(),
                      ),
              ),
              SizedBox(
                width: 400,
                child: Text(
                  title,
                  style: Theme.of(context).textTheme.headlineSmall!.copyWith(color: UnrealColors.white),
                  overflow: TextOverflow.ellipsis,
                  textAlign: TextAlign.center,
                ),
              ),
              if (titleBarTrailing != null)
                Positioned(
                  right: 0,
                  child: titleBarTrailing!,
                ),
            ],
          ),
        ),
        const SettingsMenuDivider(),
        body,
      ],
    );
  }
}

/// An item in a [SettingsDialog] menu.
class SettingsMenuItem extends StatelessWidget {
  const SettingsMenuItem({
    Key? key,
    required this.title,
    required this.iconPath,
    this.onTap,
    this.trailingIconPath = 'assets/images/icons/chevron_right.svg',
    this.trailing,
  }) : super(key: key);

  /// The title to display for this item.
  final String title;

  /// The path of the icon asset to show next to the title.
  final String iconPath;

  /// Function to call when the user taps on this item.
  final void Function()? onTap;

  /// Path of the icon to show trailing all content.
  final String? trailingIconPath;

  /// An extra widget to show before the right edge/trailing icon.
  final Widget? trailing;

  @override
  Widget build(BuildContext context) {
    final TextStyle textStyle = Theme.of(context).textTheme.labelMedium!;

    return SizedBox(
      height: 40,
      child: ListTile(
        leading: SizedBox.square(
          dimension: 24,
          child: AssetIcon(path: iconPath),
        ),
        title: Text(
          title,
          softWrap: false,
          overflow: TextOverflow.ellipsis,
          style: textStyle,
        ),
        trailing: Row(
          mainAxisSize: MainAxisSize.min,
          children: [
            if (trailing != null)
              Padding(
                padding: EdgeInsets.only(right: 16),
                child: DefaultTextStyle(
                  style: textStyle,
                  child: trailing!,
                ),
              ),
            if (trailingIconPath != null)
              AssetIcon(
                path: trailingIconPath!,
                size: 24,
              ),
          ],
        ),
        horizontalTitleGap: 16,
        minLeadingWidth: 0,
        contentPadding: EdgeInsets.only(
          left: 15,
          right: 15,
        ),
        visualDensity: const VisualDensity(vertical: -4),
        onTap: onTap,
      ),
    );
  }
}

/// A divider shown within a [SettingsDialog] menu.
class SettingsMenuDivider extends StatelessWidget {
  const SettingsMenuDivider({Key? key}) : super(key: key);

  @override
  Widget build(BuildContext context) {
    return Container(
      height: 2,
      color: Theme.of(context).colorScheme.surfaceVariant,
    );
  }
}
