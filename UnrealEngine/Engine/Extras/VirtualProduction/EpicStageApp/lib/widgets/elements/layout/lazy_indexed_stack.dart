// Copyright Epic Games, Inc. All Rights Reserved.

import 'dart:math';

import 'package:flutter/material.dart';

/// An IndexedStack that doesn't create its child widgets until they need to be displayed.
class LazyIndexedStack extends StatefulWidget {
  const LazyIndexedStack({
    Key? key,
    this.alignment = AlignmentDirectional.topStart,
    this.textDirection,
    this.sizing = StackFit.loose,
    this.clipBehavior = Clip.hardEdge,
    this.index = 0,
    this.keepAliveChildren,
    required this.childBuilders,
  }) : super(key: key);

  /// How to align the non-positioned and partially-positioned children in the
  /// stack.
  final AlignmentGeometry alignment;

  /// The text direction with which to resolve [alignment].
  final TextDirection? textDirection;

  /// How to size the non-positioned children in the stack.
  final StackFit sizing;

  /// How to handle clipping for contents of the stack.
  final Clip clipBehavior;

  /// The index into [childBuilders] for the widget to display.
  final int index;

  /// Functions used to build each child.
  /// They won't be built until their index has been set as [childIndex] at least once.
  final List<Function(BuildContext context)> childBuilders;

  /// An optional list corresponding to [childBuilders], indicating which children should be kept alive when their index
  /// is no longer selected. If not provided, all children will be disposed when their index is not selected.
  final List<bool>? keepAliveChildren;

  @override
  State<LazyIndexedStack> createState() => _LazyIndexedStackState();
}

class _LazyIndexedStackState extends State<LazyIndexedStack> {
  static const Widget _placeholderWidget = SizedBox();

  List<Widget> _displayedChildren = [];

  @override
  void initState() {
    super.initState();

    _displayedChildren = widget.childBuilders.map((_) => _placeholderWidget).toList();
    _buildChild(widget.index);
  }

  @override
  void didUpdateWidget(covariant LazyIndexedStack oldWidget) {
    super.didUpdateWidget(oldWidget);

    // Shrink to match new children count
    while (_displayedChildren.length > widget.childBuilders.length) {
      _displayedChildren.removeAt(_displayedChildren.length);
    }

    // Grow to match new children count
    while (_displayedChildren.length < widget.childBuilders.length) {
      _displayedChildren.add(_placeholderWidget);
    }

    // Replace any changed children with placeholders until they're loaded
    final maxChangedChildIndex = min(widget.childBuilders.length, oldWidget.childBuilders.length);
    for (int childIndex = 0; childIndex < maxChangedChildIndex; ++childIndex) {
      if (widget.childBuilders[childIndex] != oldWidget.childBuilders[childIndex]) {
        _displayedChildren[childIndex] = _placeholderWidget;
      }
    }

    if (oldWidget.index != widget.index) {
      // Build and remember the current child
      _buildChild(widget.index);

      // Forget the old child
      if (widget.keepAliveChildren == null ||
          oldWidget.index >= widget.keepAliveChildren!.length ||
          !widget.keepAliveChildren![oldWidget.index]) {
        _displayedChildren[oldWidget.index] = _placeholderWidget;
      }
    }
  }

  @override
  Widget build(BuildContext context) {
    return IndexedStack(
      alignment: widget.alignment,
      textDirection: widget.textDirection,
      sizing: widget.sizing,
      index: widget.index,
      children: _displayedChildren,
    );
  }

  void _buildChild(int childIndex) {
    _displayedChildren[widget.index] = widget.childBuilders[widget.index](context);
  }
}
