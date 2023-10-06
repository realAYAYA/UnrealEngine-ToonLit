// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:flutter/material.dart';

import '../../utilities/unreal_colors.dart';
import 'asset_icon.dart';
import 'epic_scroll_view.dart';
import 'modal.dart';

/// Default padding for a list menu item.
const EdgeInsetsGeometry defaultListMenuItemPadding = EdgeInsets.fromLTRB(18, 2, 4, 2);

/// Which edge of the list menu should be a flattened edge to indicate its origin.
enum ListMenuEdgeDirection {
  none,
  topLeft,
  topRight,
  bottomLeft,
  bottomRight,
  top,
  bottom,
}

/// A menu containing a vertical list of items.
class ListMenu extends StatelessWidget {
  const ListMenu({
    Key? widgetKey,
    required this.children,
    this.edgeDirection = ListMenuEdgeDirection.none,
    this.minWidth = 0,
    this.maxWidth = double.infinity,
    this.minHeight = 0,
    this.backgroundColor,
    this.bIsScrollable = false,
  }) : super(key: widgetKey);

  /// Widgets to show in the list.
  final List<Widget> children;

  /// Which edge of the menu to flatten. Regardless of value, the menu will be the same size.
  final ListMenuEdgeDirection edgeDirection;

  /// The minimum width of the menu.
  final double minWidth;

  /// The maximum width of the menu.
  final double maxWidth;

  /// The minimum height of the menu.
  final double minHeight;

  /// Optional background color for the list.
  final Color? backgroundColor;

  /// If true, add a scrollbar and make the inner contents scrollable.
  final bool bIsScrollable;

  /// Radius of the list menu's borders.
  static const Radius borderRadius = Radius.circular(8);

  @override
  Widget build(BuildContext context) {
    final Color color = backgroundColor ?? Theme.of(context).colorScheme.surfaceVariant;

    late final BorderRadius? boxBorderRadius = BorderRadius.only(
      topLeft: (edgeDirection != ListMenuEdgeDirection.topLeft && edgeDirection != ListMenuEdgeDirection.top)
          ? borderRadius
          : Radius.zero,
      topRight: (edgeDirection != ListMenuEdgeDirection.topRight && edgeDirection != ListMenuEdgeDirection.top)
          ? borderRadius
          : Radius.zero,
      bottomLeft: (edgeDirection != ListMenuEdgeDirection.bottomLeft && edgeDirection != ListMenuEdgeDirection.bottom)
          ? borderRadius
          : Radius.zero,
      bottomRight: (edgeDirection != ListMenuEdgeDirection.bottomRight && edgeDirection != ListMenuEdgeDirection.bottom)
          ? borderRadius
          : Radius.zero,
    );

    Widget contents = IntrinsicWidth(
      child: Column(
        mainAxisSize: MainAxisSize.min,
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: children,
      ),
    );

    if (bIsScrollable) {
      final ThemeData theme = Theme.of(context);
      contents = Theme(
        data: theme.copyWith(
          scrollbarTheme: theme.scrollbarTheme.copyWith(
            // Lighten scrollbar to contrast better with lighter list menu background
            thumbColor: MaterialStatePropertyAll(UnrealColors.gray31),
          ),
        ),
        child: EpicScrollView(child: contents),
      );
    }

    return Container(
      constraints: BoxConstraints(
        minWidth: minWidth,
        maxWidth: maxWidth,
        minHeight: minHeight,
      ),
      decoration: BoxDecoration(
        color: color,
        borderRadius: boxBorderRadius,
        boxShadow: modalBoxShadow,
      ),
      clipBehavior: Clip.antiAlias,
      child: contents,
    );
  }
}

/// An item shown in a [ListMenu] with a standard icon + text layout.
class ListMenuSimpleItem extends StatelessWidget {
  const ListMenuSimpleItem({
    Key? key,
    required this.title,
    this.bIsSelected = false,
    this.bIsEnabled = true,
    this.bIsChecked = false,
    this.bShowCheckbox = false,
    this.bShowArrow = false,
    this.iconPath,
    this.onTap,
  }) : super(key: key);

  /// The text to show on the item.
  final String title;

  /// Whether the tile is selected.
  final bool bIsSelected;

  /// Whether the tile is enabled. If not, it will be greyed out and ignore input.
  final bool bIsEnabled;

  /// Whether to show a checkbox leading the content.
  final bool bShowCheckbox;

  /// Whether the checkbox should be checked, if shown.
  final bool bIsChecked;

  /// Whether to show a chevron trailing the content.
  final bool bShowArrow;

  /// An optional path to an image file to show as an icon before the item's name.
  final String? iconPath;

  /// An optional function to call when this list item is tapped.
  final void Function()? onTap;

  @override
  Widget build(BuildContext context) {
    const double arrowSize = 24;

    return MouseRegion(
      cursor: SystemMouseCursors.click,
      child: Semantics(
        selected: bIsSelected,
        button: true,
        child: GestureDetector(
          behavior: HitTestBehavior.opaque,
          onTap: onTap,
          child: SizedBox(
            height: 56,
            child: Stack(
              alignment: Alignment.centerLeft,
              children: [
                Padding(
                  padding: EdgeInsets.only(right: bShowArrow ? arrowSize : 0),
                  child: Row(
                    mainAxisSize: MainAxisSize.min,
                    crossAxisAlignment: CrossAxisAlignment.center,
                    children: [
                      if (bShowCheckbox)
                        Padding(
                          padding: EdgeInsets.only(left: 8, right: 4),
                          child: AssetIcon(
                            path: bIsChecked
                                ? 'assets/images/icons/checkbox_checked.svg'
                                : 'assets/images/icons/checkbox_unchecked.svg',
                            size: 18,
                          ),
                        ),
                      if (iconPath != null)
                        Container(
                          padding: EdgeInsets.all(16),
                          child: AssetIcon(
                            path: iconPath!,
                            size: 24,
                          ),
                        ),
                      Flexible(
                        child: Padding(
                          padding: EdgeInsets.only(
                            left: iconPath == null ? 12 : 4,
                            right: bShowArrow ? 0 : 12,
                          ),
                          child: Text(
                            title,
                            style: Theme.of(context).textTheme.headlineSmall!.copyWith(color: UnrealColors.white),
                            overflow: TextOverflow.ellipsis,
                          ),
                        ),
                      ),
                    ],
                  ),
                ),
                if (bShowArrow)
                  Align(
                    alignment: Alignment.centerRight,
                    child: AssetIcon(
                      path: 'assets/images/icons/chevron_right.svg',
                      size: arrowSize,
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

/// A title shown in a [ListMenu] to categorize items.
class ListMenuHeader extends StatelessWidget {
  const ListMenuHeader(this.text, {Key? key}) : super(key: key);

  final String text;

  @override
  Widget build(BuildContext context) {
    final TextStyle textStyle = Theme.of(context).textTheme.labelMedium!.copyWith(
          fontSize: 14,
          letterSpacing: 0.25,
        );

    return Container(
      height: 38,
      padding: EdgeInsets.symmetric(horizontal: 16, vertical: 4),
      child: Column(
        mainAxisAlignment: MainAxisAlignment.spaceBetween,
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text(text, style: textStyle),
          Container(
            height: 2,
            color: UnrealColors.white.withOpacity(0.1),
          )
        ],
      ),
    );
  }
}
