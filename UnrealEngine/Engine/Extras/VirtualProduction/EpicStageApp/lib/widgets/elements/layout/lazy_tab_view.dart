// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:flutter/material.dart';

import 'lazy_indexed_stack.dart';

/// A view that lazily creates the contents of each tab as needed and can optionally discard those contents when the
/// tab is no longer selected.
class LazyTabView extends StatefulWidget {
  const LazyTabView({Key? key, required this.controller, required this.builder, this.keepAlive}) : super(key: key);

  /// The tab controller that controls which tab to show.
  final TabController controller;

  /// A builder function that creates the tab contents.
  final Function(BuildContext context, int tabIndex) builder;

  /// An optional function that determines whether a tab should be kept alive when its contents are invisible.
  final bool Function(int tabIndex)? keepAlive;

  @override
  State<LazyTabView> createState() => _LazyTabViewState();
}

class _LazyTabViewState extends State<LazyTabView> {
  /// The index of the currently displayed tab.
  int _tabIndex = -1;
  List<Function(BuildContext context)> _childBuilders = [];
  List<bool> _keepAliveChildren = [];

  @override
  void initState() {
    super.initState();

    widget.controller.addListener(_onTabControllerChanged);
    _tabIndex = widget.controller.index;

    _updateChildBuilders();
  }

  @override
  void dispose() {
    widget.controller.removeListener(_onTabControllerChanged);

    super.dispose();
  }

  @override
  void didUpdateWidget(covariant LazyTabView oldWidget) {
    super.didUpdateWidget(oldWidget);

    if (oldWidget.builder != widget.builder) {
      _updateChildBuilders();
    }
  }

  @override
  Widget build(BuildContext context) {
    return LazyIndexedStack(
      childBuilders: _childBuilders,
      index: _tabIndex,
      keepAliveChildren: _keepAliveChildren,
    );
  }

  /// Update the list of functions used to build children of the tab view.
  void _updateChildBuilders() {
    _childBuilders = [];
    _keepAliveChildren = [];

    for (int childIndex = 0; childIndex < widget.controller.length; ++childIndex) {
      _childBuilders.add((BuildContext context) => widget.builder(context, childIndex));
      _keepAliveChildren.add(widget.keepAlive != null ? widget.keepAlive!(childIndex) : false);
    }
  }

  /// Called when the tab controller updates and may have changed tabs.
  void _onTabControllerChanged() {
    if (widget.controller.index == _tabIndex) {
      return;
    }

    setState(() {
      _tabIndex = widget.controller.index;
    });
  }
}
