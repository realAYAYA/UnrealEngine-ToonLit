// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:flutter/material.dart';
import 'package:flutter/gestures.dart';

import '../../gestures/rotate_and_scale.dart';

class TransformGestureDetector extends StatelessWidget {
  const TransformGestureDetector({
    Key? key,
    this.child,
    this.behavior,
    this.onScaleStart,
    this.onScaleUpdate,
    this.onScaleEnd,
    this.onPanStart,
    this.onPanUpdate,
    this.onPanEnd,
    this.onLongPressStart,
  }) : super(key: key);

  final Widget? child;

  /// How this gesture detector should behave during hit testing.
  final HitTestBehavior? behavior;

  /// The pointers in contact with the screen have established a focal point and
  /// initial scale of 1.0.
  final GestureScaleStartCallback? onScaleStart;

  /// The pointers in contact with the screen have indicated a new focal point
  /// and/or scale.
  final GestureScaleUpdateCallback? onScaleUpdate;

  /// The pointers are no longer in contact with the screen.
  final GestureScaleEndCallback? onScaleEnd;

  /// A pointer has contacted the screen with a primary button and has begun to
  /// move.
  final GestureDragStartCallback? onPanStart;

  /// A pointer that is in contact with the screen with a primary button and
  /// moving has moved again.
  final GestureDragUpdateCallback? onPanUpdate;

  /// A pointer that was previously in contact with the screen with a primary
  /// button and moving is no longer in contact with the screen and was moving
  /// at a specific velocity when it stopped contacting the screen.
  final GestureDragEndCallback? onPanEnd;

  /// A pointer has performed a long press in a single position on the screen.
  final GestureLongPressStartCallback? onLongPressStart;

  @override
  Widget build(BuildContext context) {
    return RawGestureDetector(
      behavior: behavior,
      child: child,
      gestures: {
        if (onScaleStart != null || onScaleUpdate != null || onScaleEnd != null)
          RotateAndScaleGestureRecognizer: GestureRecognizerFactoryWithHandlers<RotateAndScaleGestureRecognizer>(
            () => RotateAndScaleGestureRecognizer(),
            (RotateAndScaleGestureRecognizer instance) {
              instance
                ..onStart = onScaleStart
                ..onUpdate = onScaleUpdate
                ..onEnd = onScaleEnd;
            },
          ),
        if (onLongPressStart != null)
          LongPressGestureRecognizer: GestureRecognizerFactoryWithHandlers<LongPressGestureRecognizer>(
            () => LongPressGestureRecognizer(),
            (LongPressGestureRecognizer instance) {
              instance.onLongPressStart = onLongPressStart;
            },
          ),
      },
    );
  }
}
