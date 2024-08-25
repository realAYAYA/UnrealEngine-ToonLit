// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:epic_common/preferences.dart';
import 'package:epic_common/widgets.dart';
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../../models/property_modify_operations.dart';
import '../../models/settings/delta_widget_settings.dart';
import 'delta_widget_base.dart';
import 'unreal_widget_base.dart';

/// A dropdown that controls one or more enum properties in Unreal Engine.
class UnrealDropdownSelector extends UnrealWidget {
  const UnrealDropdownSelector({
    super.key,
    super.overrideName,
    super.enableProperties,
    required super.unrealProperties,
  });

  @override
  _UnrealDropdownButtonState createState() => _UnrealDropdownButtonState();
}

class _UnrealDropdownButtonState extends State<UnrealDropdownSelector>
    with UnrealWidgetStateMixin<UnrealDropdownSelector, String> {
  @override
  PropertyModifyOperation get modifyOperation => const SetOperation();

  @override
  Widget build(BuildContext context) {
    final SingleSharedValue<String> sharedValue = getSingleSharedValue();
    final TextStyle textStyle = Theme.of(context).textTheme.labelMedium!;

    return Padding(
      padding: EdgeInsets.only(bottom: 8),
      child: TransientPreferenceBuilder(
        preference: Provider.of<DeltaWidgetSettings>(context, listen: false).bIsInResetMode,
        builder: (context, final bool bIsInResetMode) => Row(
          children: [
            Expanded(
              child: Padding(
                padding: EdgeInsets.symmetric(horizontal: DeltaWidgetConstants.widgetOuterXPadding),
                child: Column(
                  mainAxisAlignment: MainAxisAlignment.start,
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(
                      propertyLabel,
                      style: textStyle.copyWith(
                        color: textStyle.color!.withOpacity(bIsInResetMode ? 0.4 : 1.0),
                      ),
                    ),
                    Padding(
                      padding: const EdgeInsets.only(
                        left: DeltaWidgetConstants.widgetInnerXPadding,
                        right: DeltaWidgetConstants.widgetInnerXPadding,
                        top: 4,
                      ),
                      child: DropdownSelector<String>(
                        items: propertyEnumValues,
                        value: sharedValue.value,
                        onChanged: _onChanged,
                        hint: sharedValue.bHasMultipleValues ? 'Multiple values' : '',
                        bIsEnabled: !bIsInResetMode,
                      ),
                    ),
                  ],
                ),
              ),
            ),
            if (bIsInResetMode)
              Padding(
                padding: const EdgeInsets.only(right: DeltaWidgetConstants.widgetOuterXPadding),
                child: ResetValueButton(onPressed: handleOnResetByUser),
              ),
          ],
        ),
      ),
    );
  }

  void _onChanged(dynamic newValue) {
    assert(newValue is String);
    assert(propertyEnumValues.contains(newValue));

    handleOnChangedByUser(List.filled(widget.unrealProperties.length, newValue));
    endTransaction();
  }
}
