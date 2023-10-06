// Copyright Epic Games, Inc. All Rights Reserved.

import 'dart:ui';

import 'package:flutter/material.dart';

import '../../utilities/unreal_colors.dart';

/// A tab-style horizontal bar that allows the user to select a single value from a list.
class SelectorBar<T> extends StatefulWidget {
  const SelectorBar({
    Key? key,
    required this.value,
    required this.onSelected,
    required this.valueNames,
  }) : super(key: key);

  /// The selected value.
  final T value;

  /// Function called when a new value is selected.
  final Function(T newValue) onSelected;

  /// Names to show for each possible value. Values not in this list will not be shown on the widget,
  /// which can be useful for e.g. a "none" value.
  final Map<T, String> valueNames;

  @override
  State<SelectorBar<T>> createState() => _SelectorBarState();
}

class _SelectorBarState<T> extends State<SelectorBar<T>> with TickerProviderStateMixin {
  late final TabController _controller;

  List<T> values = [];

  @override
  void initState() {
    super.initState();

    _initValues();

    _controller = TabController(
      initialIndex: values.indexOf(widget.value),
      length: widget.valueNames.length,
      vsync: this,
    );
  }

  @override
  void dispose() {
    _controller.dispose();
    super.dispose();
  }

  @override
  void didUpdateWidget(covariant SelectorBar<T> oldWidget) {
    super.didUpdateWidget(oldWidget);

    _initValues();
  }

  @override
  Widget build(BuildContext context) {
    final borderRadius = BorderRadius.circular(20);
    final labelStyle = Theme.of(context).textTheme.bodyMedium!.copyWith(
          color: UnrealColors.white,
          fontSize: 12,
          fontVariations: [FontVariation('wght', 600)],
          letterSpacing: 0.5,
        );

    return Container(
      decoration: BoxDecoration(
        color: Theme.of(context).colorScheme.background,
        borderRadius: borderRadius,
      ),
      height: 36,
      child: TabBar(
        controller: _controller,
        onTap: (int index) => widget.onSelected(values[index]),
        labelPadding: EdgeInsets.symmetric(horizontal: 12),
        isScrollable: true,
        indicator: BoxDecoration(
          color: Theme.of(context).colorScheme.secondary,
          borderRadius: borderRadius,
        ),
        labelStyle: labelStyle,
        unselectedLabelStyle: labelStyle.copyWith(color: UnrealColors.gray75),
        tabs: [
          for (final value in values)
            Tab(
              child: Text(
                widget.valueNames[value]!,
                overflow: TextOverflow.ellipsis,
              ),
            )
        ],
      ),
    );
  }

  /// Set up the list of possible values.
  void _initValues() {
    values = widget.valueNames.keys.toList();
  }
}

/// "Ornamental" toggle button selector when there's only one value to show.
class FakeSelectorBar extends StatelessWidget {
  const FakeSelectorBar(this.valueName, {Key? key}) : super(key: key);

  /// The name to show on the button.
  final String valueName;

  @override
  Widget build(BuildContext context) {
    return SelectorBar(
      value: true,
      valueNames: {true: valueName},
      onSelected: (_) {},
    );
  }
}
