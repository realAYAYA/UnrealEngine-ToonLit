// Copyright Epic Games, Inc. All Rights Reserved.

import 'dart:math';

import 'package:flutter/material.dart';
import 'package:logging/logging.dart';
import 'package:provider/provider.dart';

import '../../models/navigator_keys.dart';
import 'layout/measurable.dart';
import 'list_menu.dart';

final _log = Logger('DropdownListMenu');

/// Function type used to create a [DropDownListMenu].
typedef DropDownListMenuBuilder = Widget Function(BuildContext context);

/// Holds data about how a dropdown list should be placed on-screen.
class DropDownListPlacement {
  const DropDownListPlacement({
    required this.pivotRect,
    required this.bStretch,
  });

  /// The rectangle next to which the dropdown should be placed.
  final Rect pivotRect;

  /// Whether the menu should be stretched to fit the pivot rectangle along the opposite axis to [pivotAxis].
  final bool bStretch;
}

/// A modal drop-down list menu that can be placed at an arbitrary position on the screen.
class DropDownListMenu extends StatefulWidget {
  const DropDownListMenu({
    Key? widgetKey,
    required this.children,
    this.backgroundColor,
    this.originTabBuilder,
    this.bCenterOriginTabOnPivot = false,
    this.minHeight = 0,
  }) : super(key: widgetKey);

  /// Items in the list.
  final List<Widget> children;

  /// Optional background color for the list.
  final Color? backgroundColor;

  /// Optional builder for an extra widget to display on top of/underneath the menu.
  /// This will be positioned to line up with the menu's origin point.
  final Widget Function(BuildContext context, bool bIsOnTop)? originTabBuilder;

  /// If true, position the menu with the origin tab built by [originTabBuilder] centered on the center of the pivot.
  final bool bCenterOriginTabOnPivot;

  /// The list's minimum height. If list would extend off the screen, it will be constrained to this height at minimum
  /// and add a scroll bar.
  final double minHeight;

  /// Show a drop-down menu originating from the given [pivotRect] and created using the [builder]. It will be
  /// automatically placed at the side of the rectangle with the most space.
  static void showAtRect({
    required Rect pivotRect,
    required DropDownListMenuBuilder builder,
    bool bStretch = false,
    void Function()? onPopped,
  }) {
    final route = _DropDownListMenuRoute(
      placement: DropDownListPlacement(
        bStretch: bStretch,
        pivotRect: pivotRect,
      ),
      builder: builder,
    );

    if (onPopped != null) {
      route.popped.then((_) => onPopped());
    }

    Navigator.of(rootNavigatorKey.currentContext!, rootNavigator: true).push<void>(route);
  }

  /// Show a drop-down menu at the given [pivotPosition] and created using the [builder].
  /// If provided, the menu will be placed at a distance of [pivotRadius] from the given position.
  static void showAtPosition({
    required Offset pivotPosition,
    required DropDownListMenuBuilder builder,
    bool bStretch = false,
    Size? pivotSize,
    void Function()? onPopped,
  }) {
    pivotSize = pivotSize ?? const Size(32, 64);

    showAtRect(
      pivotRect: Rect.fromCenter(
        center: pivotPosition,
        width: pivotSize.width,
        height: pivotSize.height,
      ),
      bStretch: bStretch,
      builder: builder,
      onPopped: onPopped,
    );
  }

  /// Show a drop-down menu next to the widget with the given [widgetKey] and created using the [builder].
  /// If provided, the widget's bounds will be inflated by [rectPadding] for the purpose of choosing the menu's
  /// position.
  static void showAtWidget({
    required GlobalKey widgetKey,
    required DropDownListMenuBuilder builder,
    bool bStretch = false,
    double rectPadding = 0,
    void Function()? onPopped,
  }) {
    final RenderObject? widgetRenderObject = widgetKey.currentContext?.findRenderObject();
    final RenderBox? widgetBox = (widgetRenderObject != null) ? (widgetRenderObject as RenderBox) : null;
    if (widgetBox == null) {
      _log.severe('No widget provided for drop-down menu location');
      return;
    }

    final RenderObject rootRenderObject = rootNavigatorKey.currentContext!.findRenderObject()!;
    Rect widgetRect = widgetBox.localToGlobal(Offset.zero, ancestor: rootRenderObject) & widgetBox.size;
    widgetRect = widgetRect.inflate(rectPadding);

    showAtRect(
      pivotRect: widgetRect,
      bStretch: bStretch,
      builder: builder,
      onPopped: onPopped,
    );
  }

  @override
  State<StatefulWidget> createState() => _DropDownListMenuState();
}

class _DropDownListMenuState extends State<DropDownListMenu> {
  Size? _menuSize;
  Size? _menuSizeWithScrollbar;
  Size? _originTabSize;

  @override
  Widget build(BuildContext context) {
    return Consumer<DropDownListPlacement>(builder: (context, dropdownData, _) {
      if (_menuSize == null) {
        // Build once so we can determine the menu's size. Use Align here so the menu doesn't expand to fill the entire
        // screen. This assumes that the menu won't change size on the next build (e.g. if its edgeDirection changes).
        return Offstage(
          child: Align(
            child: Stack(
              children: [
                if (widget.originTabBuilder != null)
                  Measurable(
                    child: widget.originTabBuilder!(context, true),
                    onMeasured: _onOriginTabMeasured,
                  ),
                Measurable(
                  child: ListMenu(children: widget.children),
                  onMeasured: _onMenuMeasured,
                ),
                Measurable(
                  child: ListMenu(children: widget.children, bIsScrollable: true),
                  onMeasured: _onMenuWithScrollBarMeasured,
                ),
              ],
            ),
          ),
        );
      }

      if (_menuSize == null || _menuSizeWithScrollbar == null || _originTabSize == null) {
        // Still waiting for initial build to be measured
        return SizedBox();
      }

      final Size screenSize = MediaQuery.of(context).size;
      final EdgeInsets screenPadding = MediaQuery.of(context).viewPadding;

      late final Rect pivotRect;
      late final Offset initialPosition;

      if (widget.bCenterOriginTabOnPivot) {
        pivotRect = dropdownData.pivotRect.center & Size.zero;
        initialPosition = pivotRect.center +
            Offset(
              -_originTabSize!.width / 2,
              _originTabSize!.height / 2,
            );
      } else {
        pivotRect = dropdownData.pivotRect;
        initialPosition = pivotRect.bottomLeft;
      }

      // Adjust Y position if we would extend off the bottom of the screen
      // Max Y coordinate of menu's bottom
      final double bottomLimit = screenSize.height - screenPadding.bottom;

      // Actual Y coordinate of menu's bottom
      final double menuBottom = initialPosition.dy + widget.minHeight;

      // The height of the menu, which we will apply constraints to as necessary to fit on-screen
      double menuHeight = _menuSize!.height;

      bool bIsOriginTop = true;
      double yPosition = initialPosition.dy;

      if (menuBottom > bottomLimit) {
        // Display the menu above the pivot instead of below
        bIsOriginTop = false;

        if (widget.bCenterOriginTabOnPivot) {
          yPosition -= _menuSize!.height + _originTabSize!.height;
        } else {
          yPosition -= _menuSize!.height + pivotRect.height;
        }

        if (yPosition < 0) {
          // Shrink menu to fit on screen
          menuHeight += yPosition;
          yPosition = 0;
        }
      } else {
        // Clamp height in bounds of screen
        menuHeight = menuHeight.clamp(
          widget.minHeight,
          bottomLimit - yPosition,
        );
      }

      // Adjust Y position to accommodate origin tab
      if (_originTabSize != null && bIsOriginTop) {
        yPosition -= _originTabSize!.height;
      }

      // Minimum menu width so rounded border lines up with edge
      double minMenuWidth = 0;
      if (_originTabSize != null && !dropdownData.bStretch) {
        minMenuWidth = _originTabSize!.width + ListMenu.borderRadius.x;
      }

      final bool bWillScroll = menuHeight < _menuSize!.height;
      final double menuWidth = max(minMenuWidth, bWillScroll ? _menuSizeWithScrollbar!.width : _menuSize!.width);

      // Adjust X position if we would extend off the right of the screen
      bool bIsOriginLeft = true;
      double xPosition = initialPosition.dx;

      if (!dropdownData.bStretch) {
        if (xPosition < screenPadding.left) {
          xPosition = screenPadding.left;
        } else {
          // Push menu to the left if the dropdown would cross the right edge of the safe area
          final double rightLimit = screenSize.width - screenPadding.left;
          final double menuRight = xPosition + menuWidth;

          if (menuRight > rightLimit) {
            // Display menu to left of pivot instead of right
            if (widget.bCenterOriginTabOnPivot) {
              xPosition -= menuWidth - _originTabSize!.width;
            } else {
              xPosition -= menuWidth - pivotRect.width;
            }
            bIsOriginLeft = false;

            // Move the menu left to fit on-screen
            final double newRight = xPosition + menuWidth;
            if (newRight > rightLimit) {
              xPosition -= newRight - rightLimit;
            }
          }
        }
      }

      late final ListMenuEdgeDirection edgeDirection;
      if (bIsOriginTop) {
        if (dropdownData.bStretch) {
          edgeDirection = ListMenuEdgeDirection.top;
        } else if (bIsOriginLeft) {
          edgeDirection = ListMenuEdgeDirection.topLeft;
        } else {
          edgeDirection = ListMenuEdgeDirection.topRight;
        }
      } else {
        if (dropdownData.bStretch) {
          edgeDirection = ListMenuEdgeDirection.bottom;
        } else if (bIsOriginLeft) {
          edgeDirection = ListMenuEdgeDirection.bottomLeft;
        } else {
          edgeDirection = ListMenuEdgeDirection.bottomRight;
        }
      }

      // Create the origin tab if necessary
      late final Widget? originTab;
      if (widget.originTabBuilder != null) {
        // Ignore the pointer so tapping the tab also closes the dropdown
        originTab = IgnorePointer(
          child: widget.originTabBuilder!(context, bIsOriginTop),
        );
      } else {
        originTab = null;
      }

      // Create the contents of the dropdown menu.
      Widget listWidget = ListMenu(
        minWidth: dropdownData.bStretch ? pivotRect.width : 0,
        maxWidth: dropdownData.bStretch ? pivotRect.width : double.infinity,
        children: widget.children,
        backgroundColor: widget.backgroundColor ?? Theme.of(context).colorScheme.surfaceTint,
        edgeDirection: edgeDirection,
        bIsScrollable: bWillScroll,
      );

      // Wrap with scrolling view
      if (bWillScroll) {
        listWidget = SizedBox(
          height: menuHeight,
          child: listWidget,
        );
      }

      return CustomSingleChildLayout(
        delegate: _DropDownListMenuLayout(position: Offset(xPosition, yPosition)),
        child: Align(
          alignment: Alignment.topLeft,
          child: Column(
            crossAxisAlignment: bIsOriginLeft ? CrossAxisAlignment.start : CrossAxisAlignment.end,
            children: [
              if (bIsOriginTop && originTab != null) originTab,
              ConstrainedBox(
                constraints: BoxConstraints(minWidth: minMenuWidth),
                child: listWidget,
              ),
              if (!bIsOriginTop && originTab != null) originTab,
            ],
          ),
        ),
      );
    });
  }

  void _onMenuMeasured(Size size) {
    if (_menuSize != size) {
      setState(() {
        _menuSize = size;
      });
    }
  }

  void _onMenuWithScrollBarMeasured(Size size) {
    if (_menuSizeWithScrollbar != size) {
      setState(() {
        _menuSizeWithScrollbar = size;
      });
    }
  }

  void _onOriginTabMeasured(Size size) {
    if (_originTabSize != size) {
      setState(() {
        _originTabSize = size;
      });
    }
  }
}

/// A route that displays the drop-down menu popup and positions it.
class _DropDownListMenuRoute<T> extends PopupRoute<T> {
  _DropDownListMenuRoute({
    required this.placement,
    required this.builder,
  });

  final DropDownListPlacement placement;
  final DropDownListMenuBuilder builder;

  @override
  Color? get barrierColor => Colors.black38;

  @override
  bool get barrierDismissible => true;

  @override
  String? get barrierLabel => 'Dismiss';

  @override
  Duration get transitionDuration => Duration(milliseconds: 200);

  @override
  Duration get reverseTransitionDuration => Duration(milliseconds: 100);

  @override
  Widget buildPage(BuildContext context, Animation<double> animation, Animation<double> secondaryAnimation) {
    return Builder(
      builder: (BuildContext context) {
        return Provider(
          create: (context) => placement,
          child: builder(context),
        );
      },
    );
  }
}

/// Layout that places the dropdown within the screen.
class _DropDownListMenuLayout<T> extends SingleChildLayoutDelegate {
  _DropDownListMenuLayout({required this.position});

  final Offset position;

  @override
  Offset getPositionForChild(Size size, Size childSize) {
    return position;
  }

  @override
  bool shouldRelayout(covariant SingleChildLayoutDelegate oldDelegate) {
    return oldDelegate != this;
  }
}
