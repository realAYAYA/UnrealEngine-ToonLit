// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:flutter/material.dart';
import 'package:logging/logging.dart';

import '../../utilities/unreal_colors.dart';
import 'layout/measurable.dart';

final _log = Logger('SwipeRevealer');

/// Function that builds an action widget to reveal on one side of a [SwipeRevealer] when swiped.
/// Takes a function which, when called, will unlock the widget's swipe state and return it to its resting state.
/// If [bDestroy] is true, [SwipeRevealer] will transition out, then call a function to indicate that its item has been
/// deleted.
typedef SwipeRevealerActionBuilder = Widget Function(
  BuildContext context,
  void Function({bool bDeleteItem}) onFinished,
);

/// A wrapper which allows a widget to be swiped left or right to reveal additional actions.
class SwipeRevealer extends StatefulWidget {
  const SwipeRevealer({
    Key? key,
    required this.child,
    this.leftSwipeActionBuilder,
    this.rightSwipeActionBuilder,
    this.backgroundPadding = EdgeInsets.zero,
    this.onDeleted,
  }) : super(key: key);

  /// The primary widget when this is idle.
  final Widget child;

  /// Function that builds the widget to reveal when this is swiped left (shown on the right of the widget).
  final SwipeRevealerActionBuilder? leftSwipeActionBuilder;

  /// Function that builds the widget to reveal when this is swiped right (shown on the left of the widget).
  final SwipeRevealerActionBuilder? rightSwipeActionBuilder;

  /// Padding to add around the background when any swipe effects are drawn (such as the red background on deletion).
  final EdgeInsets backgroundPadding;

  /// Function called if this is deleted by one of its swipe actions.
  final Function()? onDeleted;

  @override
  State<SwipeRevealer> createState() => _SwipeRevealerState();
}

class _SwipeRevealerState extends State<SwipeRevealer> with TickerProviderStateMixin {
  late final AnimationController _backgroundFadeController;
  late final AnimationController _slideController;
  late final AnimationController _deleteController;

  late Animation<double> _backgroundFadeAnimation;
  late Animation<double> _slideAnimation;
  late Animation<double> _deleteHeightAnimation;

  Size _mainContentSize = Size.zero;
  Size _leftActionSize = Size.zero;
  Size _rightActionSize = Size.zero;

  /// How far the user has dragged the slider
  double _slideExtent = 0;

  /// How much to adjust the slider position to account for action sizes.
  /// The slider is centered within its stack, so when the actions are of different sizes, it will be off-center.
  /// This value is calculated to compensate for that effect.
  double _slideOffset = 0;

  /// If true, the slider is currently locked open on one of the swipe actions.
  bool _isLockedOpen = false;

  /// Whether this is being deleted due to a swipe action.
  bool _bIsBeingDeleted = false;

  double get _minSlideExtent => -_leftActionSize.width;
  double get _maxSlideExtent => _rightActionSize.width;

  @override
  void initState() {
    super.initState();

    _slideController = AnimationController(
      vsync: this,
      duration: const Duration(milliseconds: 150),
      lowerBound: 0,
      upperBound: 1,
    );

    _backgroundFadeController = AnimationController(
      vsync: this,
      duration: const Duration(milliseconds: 150),
      lowerBound: 0,
      upperBound: 1,
    );
    _backgroundFadeAnimation = _backgroundFadeController.drive(Tween<double>(
      begin: 0,
      end: 1,
    ));

    _deleteController = AnimationController(
      duration: const Duration(milliseconds: 400),
      vsync: this,
    );

    _updateSlideAnimation();
  }

  @override
  void dispose() {
    _slideController.dispose();
    _backgroundFadeController.dispose();
    _deleteController.dispose();

    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return LayoutBuilder(builder: (context, final BoxConstraints constraints) {
      Widget mainContents = Measurable(
        onMeasured: _onMainContentMeasured,
        child: widget.child,
      );

      if (_mainContentSize == Size.zero) {
        return mainContents;
      }

      // Ignore pointer events on the child when locked open. This will instead be handled by returning to resting
      // state instead.
      if (_bIsBeingDeleted || _isLockedOpen) {
        mainContents = IgnorePointer(
          child: mainContents,
        );
      }

      // Touch inputs for swipe + unlock tap
      if (!_bIsBeingDeleted) {
        mainContents = GestureDetector(
          behavior: HitTestBehavior.opaque,
          onTap: _isLockedOpen ? _returnToResting : null,
          onHorizontalDragStart: _onDragStart,
          onHorizontalDragUpdate: _onDragUpdate,
          onHorizontalDragEnd: _onDragEnd,
          child: mainContents,
        );
      }

      final Widget? leftAction = _buildSwipeActionWidget(
        bIsLeftSide: true,
        builder: widget.leftSwipeActionBuilder,
        measuredCallback: _onLeftActionMeasured,
        currentSize: _leftActionSize,
      );

      final Widget? rightAction = _buildSwipeActionWidget(
        bIsLeftSide: false,
        builder: widget.rightSwipeActionBuilder,
        measuredCallback: _onRightActionMeasured,
        currentSize: _rightActionSize,
      );

      final mainStack = Stack(
        key: const Key('mainStack'),
        clipBehavior: Clip.none,
        children: [
          // Background highlight shown when being dragged
          Positioned(
            left: widget.backgroundPadding.left,
            right: widget.backgroundPadding.right,
            top: widget.backgroundPadding.top,
            bottom: widget.backgroundPadding.bottom,
            child: FadeTransition(
              opacity: _backgroundFadeAnimation,
              child: Container(
                color: UnrealColors.gray13,
              ),
            ),
          ),

          // Red background shown when an item is being deleted
          if (_bIsBeingDeleted)
            AnimatedBuilder(
              animation: _slideAnimation,
              child: Container(
                color: UnrealColors.highlightRed,
              ),
              builder: (context, child) => Positioned(
                left: widget.backgroundPadding.left + _mainContentSize.width + _slideAnimation.value,
                right: widget.backgroundPadding.right,
                top: widget.backgroundPadding.top,
                bottom: widget.backgroundPadding.bottom,
                child: child!,
              ),
            ),

          // Row containing revealed swipe actions and main content
          ClipRect(
            clipBehavior: Clip.hardEdge,
            child: SizedBox(
              height: _mainContentSize.height,
              child: OverflowBox(
                maxWidth: double.infinity,
                maxHeight: double.infinity,
                child: AnimatedBuilder(
                  animation: _slideAnimation,

                  child: Row(children: [
                    if (rightAction != null) rightAction,
                    ConstrainedBox(
                      constraints: BoxConstraints(maxWidth: constraints.maxWidth),
                      child: mainContents,
                    ),
                    if (leftAction != null) leftAction,
                  ]),

                  // Position the row based on animation
                  builder: (context, child) => Transform(
                    transform: Matrix4.translationValues(
                      _slideAnimation.value + _slideOffset,
                      0,
                      0,
                    ),
                    transformHitTests: true,
                    child: child!,
                  ),
                ),
              ),
            ),
          ),
        ],
      );

      // Ignore input while deleting
      if (_bIsBeingDeleted) {
        return IgnorePointer(
          child: AnimatedBuilder(
            animation: _deleteHeightAnimation,
            child: mainStack,
            builder: (context, child) => SizedBox(
              height: _deleteHeightAnimation.value,
              child: child!,
            ),
          ),
        );
      }

      return TapRegion(
        behavior: HitTestBehavior.translucent,
        onTapOutside: (_) => _returnToResting(),
        child: mainStack,
      );
    });
  }

  /// Make a swipe action widget wrapped with [Measurable] and [Offstage] widgets as necessary.
  /// Returns null if no builder was provided.
  Widget? _buildSwipeActionWidget({
    required bool bIsLeftSide,
    required SwipeRevealerActionBuilder? builder,
    required Function(Size) measuredCallback,
    required Size currentSize,
  }) {
    if (builder == null) {
      return null;
    }

    Widget swipeActionWidget = Measurable(
      onMeasured: measuredCallback,
      child: builder(
        context,
        ({bool bDeleteItem = false}) => _onSwipeActionFinished(
          bIsLeftSide: bIsLeftSide,
          bDeleteItem: bDeleteItem,
        ),
      ),
    );

    // If size hasn't been determined, draw offstage to avoid flickering
    if (currentSize.width == 0) {
      swipeActionWidget = Offstage(child: swipeActionWidget);
    }

    // Disable input if this is about to be deleted
    if (_bIsBeingDeleted) {
      swipeActionWidget = IgnorePointer(child: swipeActionWidget);
    }

    return swipeActionWidget;
  }

  /// Called when a swipe action is finished.
  void _onSwipeActionFinished({required bool bIsLeftSide, bool bDeleteItem = false}) {
    if (bDeleteItem) {
      _delete();
    } else {
      _returnToResting();
    }
  }

  /// Play the delete animation.
  void _delete() {
    setState(() {
      _bIsBeingDeleted = true;

      _slideAnimation = Tween<double>(
        begin: _slideAnimation.value,
        end: -_mainContentSize.width - _leftActionSize.width,
      ).animate(CurvedAnimation(
        parent: _deleteController,
        curve: const Interval(0.0, 0.7, curve: Curves.easeInOut),
      ));

      _deleteHeightAnimation = Tween<double>(
        begin: _mainContentSize.height,
        end: 0,
      ).animate(CurvedAnimation(
        parent: _deleteController,
        curve: const Interval(0.3, 1.0, curve: Curves.easeInOut),
      ));

      _deleteController.forward();

      _deleteController.addStatusListener((status) {
        if (status == AnimationStatus.completed) {
          if (widget.onDeleted == null) {
            _log.warning('${this.widget} was deleted, but has no onDeleted function');
          } else {
            widget.onDeleted!();
          }
        }
      });
    });
  }

  /// Called when this should unlock its swiped state and return to its resting state.
  void _returnToResting() {
    setState(() {
      _isLockedOpen = false;
    });

    _slideController.forward();
    _backgroundFadeController.reverse();
  }

  /// Called when we learn the size of the main child widget's contents.
  void _onMainContentMeasured(Size size) {
    if (_bIsBeingDeleted || !mounted) {
      // Ignore during delete animation since we're going to transition it out regardless
      return;
    }

    _mainContentSize = size;
    _updateSlideAnimation();
  }

  /// Called when we learn the size of the left action widget.
  void _onLeftActionMeasured(Size size) {
    if (!mounted) {
      return;
    }

    setState(() {
      _leftActionSize = size;
      _updateSlideOffset();
    });
  }

  /// Called when we learn the size of the right action widget.
  void _onRightActionMeasured(Size size) {
    if (!mounted) {
      return;
    }

    setState(() {
      _rightActionSize = size;
      _updateSlideOffset();
    });
  }

  /// Called when either swipe action changes size and we need to adjust the slide offset accordingly.
  void _updateSlideOffset() {
    _slideOffset = (_leftActionSize.width - _rightActionSize.width) / 2;
  }

  /// Update the slide animation so its start point matches the current slide extent.
  void _updateSlideAnimation() {
    // Determine offset relative to size of main content
    final double xOffset;
    if (_mainContentSize.width > 0) {
      xOffset = _slideExtent;
    } else {
      xOffset = 0;
    }

    setState(() {
      _slideAnimation = _slideController.drive(Tween<double>(
        begin: xOffset,
        end: 0,
      ));
    });
  }

  /// Called when the user starts a horizontal drag gesture on this.
  void _onDragStart(DragStartDetails details) {
    _slideExtent = _slideAnimation.value;
    _slideController.reset();
    _updateSlideAnimation();

    _backgroundFadeController.forward();
  }

  /// Called when the user continues a horizontal drag gesture on this.
  void _onDragUpdate(DragUpdateDetails details) {
    _slideExtent += details.primaryDelta!;
    _slideExtent = _slideExtent.clamp(_minSlideExtent, _maxSlideExtent);
    _updateSlideAnimation();
  }

  /// Called when the user finishes a horizontal drag gesture on this.
  void _onDragEnd(DragEndDetails details) {
    // Lock open if at min/max extents
    if (_slideExtent == _minSlideExtent || _slideExtent == _maxSlideExtent) {
      setState(() {
        _isLockedOpen = true;
      });
    } else {
      _returnToResting();
    }
  }
}
