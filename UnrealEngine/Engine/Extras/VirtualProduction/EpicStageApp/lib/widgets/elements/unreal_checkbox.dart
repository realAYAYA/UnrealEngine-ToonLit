// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:epic_stage_app/widgets/elements/unreal_widget_base.dart';
import 'package:flutter/cupertino.dart';
import 'package:flutter/material.dart';

import '../../models/property_modify_operations.dart';
import 'delta_widget_base.dart';

/// A checkbox that controls one or more boolean properties in Unreal Engine.
class UnrealCheckbox extends UnrealWidget {
  const UnrealCheckbox({
    super.key,
    super.overrideName,
    required super.unrealProperties,
    this.bCompactLayout = false,
    this.bShowLabel = true,
    this.bShowResetButton = true,
    this.bIsInverted = false,
  });

  /// If false, hide the reset button.
  final bool bShowResetButton;

  /// If false, hide the label.
  final bool bShowLabel;

  /// If true, the value will be interpreted as its opposite (e.g. if the box is checked, interpret it as false).
  final bool bIsInverted;

  /// If true, lay out the widget in a compact way with the box right next to the label.
  final bool bCompactLayout;

  @override
  _UnrealCheckboxState createState() => _UnrealCheckboxState();
}

class _UnrealCheckboxState extends State<UnrealCheckbox> with UnrealWidgetStateMixin<UnrealCheckbox, bool> {
  @override
  PropertyModifyOperation get modifyOperation => const SetOperation();

  @override
  Widget build(BuildContext context) {
    final SingleSharedValue<bool> sharedValue = getSingleSharedValue(false);

    bool? bIsChecked = sharedValue.value;
    if (widget.bIsInverted && bIsChecked != null) {
      bIsChecked = !bIsChecked;
    }

    final Widget labelWidget = Text(
      propertyLabel,
      style: Theme.of(context).textTheme.bodyMedium,
    );
    final Widget checkboxWidget = Checkbox(
      value: bIsChecked,
      tristate: sharedValue.bHasMultipleValues,
      onChanged: _onChanged,
      activeColor: CupertinoDynamicColor.resolve(CupertinoColors.activeBlue, context),
    );

    if (widget.bCompactLayout) {
      return Wrap(
        direction: Axis.horizontal,
        crossAxisAlignment: WrapCrossAlignment.center,
        children: [
          if (widget.bShowLabel)
            TextButton(
              child: labelWidget,
              onPressed: () => _onChanged(bIsChecked == true ? false : true),
            ),
          checkboxWidget,
        ],
      );
    }

    return Padding(
      padding: EdgeInsets.only(bottom: 8),
      child: Row(children: [
        Expanded(
          child: Padding(
            padding: EdgeInsets.symmetric(horizontal: DeltaWidgetConstants.widgetOuterXPadding),
            child: Column(
              mainAxisAlignment: MainAxisAlignment.start,
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                labelWidget,
                checkboxWidget,
              ],
            ),
          ),
        ),
        if (widget.bShowResetButton) ResetValueButton(onPressed: handleOnResetByUser),
      ]),
    );
  }

  void _onChanged(dynamic newValue) {
    assert(newValue is bool);

    if (widget.bIsInverted) {
      newValue = !newValue;
    }

    handleOnChangedByUser(List.filled(widget.unrealProperties.length, newValue));
    endTransaction();
  }
}
