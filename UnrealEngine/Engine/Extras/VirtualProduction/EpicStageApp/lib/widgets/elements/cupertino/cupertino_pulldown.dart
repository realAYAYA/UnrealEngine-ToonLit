// Copyright Epic Games, Inc. All Rights Reserved.

import 'dart:ui';

import 'package:flutter/cupertino.dart';

/// Width of an iOS-style modal.
const double _modalWidth = 250;

/// Color of the barrier behind the modal.
const Color _modalBarrierColor = Color(0x00000000);

/// Corner radius of the pull-down menu's shape so it can cast a matching shadow.
const double _modalCornerRadius = 14.0;

const Duration _transitionInDuration = Duration(milliseconds: 335);
const Duration _transitionOutDuration = Duration(milliseconds: 275);

/// Data describing an action to display in an iOS-style pull-down menu.
class CupertinoPullDownActionData {
  const CupertinoPullDownActionData({
    required this.title,
    this.trailingIcon,
    this.bIsDestructive = false,
    this.bCanBeToggled = false,
    this.bIsToggled = false,
    this.onPressed,
    Key? key,
  }) : assert(bCanBeToggled || !bIsToggled);

  /// The title of the action.
  final String title;

  /// Optional icon to display after the title.
  final IconData? trailingIcon;

  /// Whether to style this as a destructive action.
  final bool bIsDestructive;

  /// If true, leave space next to the title for a checkmark.
  final bool bCanBeToggled;

  /// If true and bCanBeToggled is set, show a checkmark next to the title.
  final bool bIsToggled;

  /// Called when the action is pressed.
  final VoidCallback? onPressed;
}

/// An iOS-style pull-down button that exposes a menu when pressed.
/// See https://developer.apple.com/design/human-interface-guidelines/ios/controls/buttons/#pull-down-buttons
class CupertinoPullDownButton extends StatefulWidget {
  const CupertinoPullDownButton({required this.child, required this.actionGroups, Key? key}) : super(key: key);

  /// The contents of the button widget.
  final Widget child;

  /// Widgets to display in the action list. The top-level list is a list of groups which will have thick dividers
  /// between them, while the inner lists are lists of action widgets to display in each group.
  final List<List<CupertinoPullDownActionData>> actionGroups;

  @override
  State<CupertinoPullDownButton> createState() => _CupertinoPullDownButtonState();
}

class _CupertinoPullDownButtonState extends State<CupertinoPullDownButton> {
  @override
  Widget build(BuildContext context) {
    return CupertinoButton(
      child: widget.child,
      onPressed: _showPullDownMenu,
      padding: const EdgeInsets.symmetric(vertical: 16.0),
    );
  }

  /// Open the pull-down menu in a new navigator context.
  void _showPullDownMenu() {
    // Determine the position of the button in global coordinates
    final NavigatorState navigator = Navigator.of(context);
    final RenderBox buttonBox = context.findRenderObject()! as RenderBox;
    final Rect buttonRect =
        buttonBox.localToGlobal(Offset.zero, ancestor: navigator.context.findRenderObject()) & buttonBox.size;

    final route = _PullDownMenuRoute(
      buttonRect: buttonRect,
      actionGroups: widget.actionGroups,
    );

    Navigator.of(context, rootNavigator: true).push<void>(route);
  }
}

/// An action shown in the pull-down list of a [CupertinoPullDownButton].
class _CupertinoPullDownAction extends StatefulWidget {
  const _CupertinoPullDownAction({
    required this.actionData,
    this.bShowTogglePadding = false,
    Key? key,
  }) : super(key: key);

  /// The data describing this action.
  final CupertinoPullDownActionData actionData;

  /// If true, padding will be added on the left side to make room for a toggle checkmark.
  final bool bShowTogglePadding;

  @override
  State<_CupertinoPullDownAction> createState() => _CupertinoPullDownActionState();
}

class _CupertinoPullDownActionState extends State<_CupertinoPullDownAction> {
  // Color constants based on screenshots from iOS
  static const Color _backgroundColor = Color(0xC0212122);
  static const Color _backgroundColorPressed = Color(0xC03F3F40);
  static const double _buttonHeight = 44.0;
  static const double _buttonVerticalPadding = 8.0;

  static const TextStyle _titleTextStyle = TextStyle(
    fontFamily: '.SF UI Text',
    inherit: false,
    fontSize: 16.0,
    textBaseline: TextBaseline.alphabetic,
  );

  /// Whether the tap/mouse is current pressing on this.
  bool _bIsPressed = false;

  /// Whether this was tapped, meaning it's been selected and the pull-down will close.
  bool _bWasTapped = false;

  bool get _bShouldHighlight => _bIsPressed || _bWasTapped;

  @override
  Widget build(BuildContext context) {
    final Color textColor = widget.actionData.bIsDestructive
        ? CupertinoDynamicColor.resolve(CupertinoColors.destructiveRed, context)
        : CupertinoTheme.of(context).primaryContrastingColor;

    return GestureDetector(
      behavior: HitTestBehavior.opaque,
      onTapDown: _onTapDown,
      onTapUp: _onTapUp,
      onTapCancel: _onTapCancel,
      onTap: _onTap,
      child: ConstrainedBox(
        constraints: const BoxConstraints(minHeight: _buttonHeight),
        child: Semantics(
          button: true,
          child: Container(
            decoration: BoxDecoration(
              color: CupertinoDynamicColor.resolve(
                  _bShouldHighlight ? _backgroundColorPressed : _backgroundColor, context),
            ),
            padding: EdgeInsets.fromLTRB(
              widget.bShowTogglePadding ? 10.0 : 20.0,
              _buttonVerticalPadding,
              widget.actionData.trailingIcon != null ? 16.0 : 20.0,
              _buttonVerticalPadding,
            ),
            child: IconTheme(
              data: IconThemeData(color: textColor),
              child: DefaultTextStyle(
                style: _titleTextStyle.copyWith(color: textColor),
                child: Row(
                  children: [
                    if (widget.bShowTogglePadding)
                      SizedBox(
                        width: 22.0,
                        child: widget.actionData.bIsToggled
                            ? const Align(
                                alignment: Alignment.centerLeft,
                                child: Icon(
                                  CupertinoIcons.check_mark,
                                  size: 16.0,
                                ),
                              )
                            : null,
                      ),
                    Expanded(child: Text(widget.actionData.title)),
                    if (widget.actionData.trailingIcon != null)
                      Icon(
                        widget.actionData.trailingIcon!,
                        size: 20.0,
                      ),
                  ],
                ),
              ),
            ),
          ),
        ),
      ),
    );
  }

  void _onTapDown(TapDownDetails details) {
    setState(() {
      _bIsPressed = true;
    });
  }

  void _onTapUp(TapUpDetails details) {
    setState(() {
      _bIsPressed = false;
    });
  }

  void _onTapCancel() {
    setState(() {
      _bIsPressed = false;
    });
  }

  void _onTap() {
    setState(() {
      _bWasTapped = true;
    });

    widget.actionData.onPressed?.call();

    // Close the pull-down menu
    Navigator.of(context).pop();
  }
}

/// A route that displays the pull-down menu popup, positions it, and animates it into place.
class _PullDownMenuRoute<T> extends PopupRoute<T> {
  _PullDownMenuRoute({required this.buttonRect, required this.actionGroups});

  final Rect buttonRect;
  final List<List<CupertinoPullDownActionData>> actionGroups;

  @override
  Color? get barrierColor => _modalBarrierColor;

  @override
  bool get barrierDismissible => true;

  @override
  String? get barrierLabel => 'Dismiss';

  @override
  Widget buildPage(BuildContext context, Animation<double> animation, Animation<double> secondaryAnimation) {
    return Builder(
      builder: (BuildContext context) {
        return CustomSingleChildLayout(
          delegate: _PullDownMenuRouteLayout<T>(
            buttonRect: buttonRect,
          ),
          child: _PullDownMenu(
            actionGroups: actionGroups,
            buttonRect: buttonRect,
            animation: animation,
          ),
        );
      },
    );
  }

  @override
  Duration get transitionDuration => _transitionInDuration;

  @override
  Duration get reverseTransitionDuration => _transitionOutDuration;
}

class _PullDownMenuRouteLayout<T> extends SingleChildLayoutDelegate {
  _PullDownMenuRouteLayout({required this.buttonRect});

  final Rect buttonRect;

  @override
  BoxConstraints getConstraintsForChild(BoxConstraints constraints) {
    return const BoxConstraints(minWidth: _modalWidth, maxWidth: _modalWidth);
  }

  @override
  Offset getPositionForChild(Size size, Size childSize) {
    return buttonRect.bottomRight + const Offset(-_modalWidth, 6);
  }

  @override
  bool shouldRelayout(covariant SingleChildLayoutDelegate oldDelegate) {
    return oldDelegate != this;
  }
}

/// A pull-down menu displayed when a pull-down button is pressed.
class _PullDownMenu extends StatelessWidget {
  const _PullDownMenu({required this.actionGroups, required this.buttonRect, required this.animation, Key? key})
      : super(key: key);

  final List<List<CupertinoPullDownActionData>> actionGroups;
  final Rect buttonRect;
  final Animation<double> animation;

  static const _borderRadius = BorderRadius.all(Radius.circular(_modalCornerRadius));

  static final CurveTween _scaleInTween = CurveTween(curve: const Cubic(0.28, 0.67, 0.4, 1.15));
  static final CurveTween _scaleOutTween = CurveTween(curve: Curves.easeInQuad);
  static final CurveTween _fadeInTween = CurveTween(curve: Curves.easeOutCubic);
  static final CurveTween _fadeOutTween = CurveTween(curve: const Interval(0.2, 1.0, curve: Curves.easeInQuad));
  static final CurveTween _blurInTween = CurveTween(curve: const Interval(0.0, 0.6, curve: Cubic(0.5, 0.0, 1.0, 0.5)));
  static final CurveTween _blurOutTween = CurveTween(curve: Curves.easeInQuad);

  @override
  Widget build(BuildContext context) {
    return AnimatedBuilder(
      animation: animation,
      child: _buildActionColumn(),
      builder: (BuildContext context, Widget? child) {
        final bool bIsReversing = animation.status == AnimationStatus.reverse;
        final double scale = (bIsReversing ? _scaleOutTween : _scaleInTween).evaluate(animation);
        final double opacity = (bIsReversing ? _fadeOutTween : _fadeInTween).evaluate(animation);
        final double blurAmount = (bIsReversing ? _blurOutTween : _blurInTween).evaluate(animation) * 10.0;

        return Transform.scale(
          alignment: Alignment.topRight,
          origin: Offset(-buttonRect.width / 2, 0),
          scale: scale,
          child: Container(
            decoration: BoxDecoration(
              borderRadius: _borderRadius,
              boxShadow: [
                BoxShadow(color: Color.fromRGBO(0, 0, 0, opacity * 0.2), blurRadius: 20.0, spreadRadius: 25.0)
              ],
            ),
            child: ClipRRect(
              borderRadius: _borderRadius,
              // BackdropFilter doesn't work when inside an Opacity widget, so we need to animate its blur separately.
              // As a result, we re-implement CupertinoPopupSurface here so we can control the blur directly.
              child: BackdropFilter(
                filter: ImageFilter.blur(sigmaX: blurAmount, sigmaY: blurAmount),
                child: Opacity(
                  opacity: opacity,
                  child: child,
                ),
              ),
            ),
          ),
        );
      },
    );
  }

  /// Build a column of action widgets and separators based on the nested groups passed in.
  Widget _buildActionColumn() {
    List<Widget> separatedActions = [];

    const thinSeparator = _ThinPullDownSeparator();
    const thickSeparator = _ThickPullDownSeparator();

    // First, check if any of the actions can be toggled so we know whether need the extra padding.
    bool bCanAnyBeToggled = false;
    for (List<CupertinoPullDownActionData> actionGroup in actionGroups) {
      for (CupertinoPullDownActionData actionData in actionGroup) {
        if (actionData.bCanBeToggled) {
          bCanAnyBeToggled = true;
          break;
        }
      }

      if (bCanAnyBeToggled) {
        break;
      }
    }

    // Build a single list of widgets to display, including separators, based on the nested groups
    for (int groupIndex = 0; groupIndex < actionGroups.length; ++groupIndex) {
      List<CupertinoPullDownActionData> actionGroup = actionGroups[groupIndex];

      if (groupIndex != 0) {
        separatedActions.add(thickSeparator);
      }

      for (int actionIndex = 0; actionIndex < actionGroup.length; ++actionIndex) {
        if (actionIndex != 0) {
          separatedActions.add(thinSeparator);
        }

        CupertinoPullDownActionData actionData = actionGroup[actionIndex];
        separatedActions.add(_CupertinoPullDownAction(actionData: actionData, bShowTogglePadding: bCanAnyBeToggled));
      }
    }

    return Column(mainAxisSize: MainAxisSize.min, children: separatedActions);
  }
}

/// A thin separator between pull-down actions in the same group.
class _ThinPullDownSeparator extends StatelessWidget {
  const _ThinPullDownSeparator({Key? key}) : super(key: key);

  @override
  Widget build(BuildContext context) {
    return Container(
      height: 1.0 / MediaQuery.of(context).devicePixelRatio,
      color: CupertinoDynamicColor.resolve(CupertinoColors.opaqueSeparator.withOpacity(0.3), context),
    );
  }
}

/// A thick separator between pull-down action groups.
class _ThickPullDownSeparator extends StatelessWidget {
  const _ThickPullDownSeparator({Key? key}) : super(key: key);

  static const _groupDividerColor = Color(0xC0181818);

  @override
  Widget build(BuildContext context) {
    return Container(
      height: 8.0,
      color: CupertinoDynamicColor.resolve(_groupDividerColor, context),
    );
  }
}
