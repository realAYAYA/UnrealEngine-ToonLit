// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:epic_common/preferences.dart';
import 'package:epic_common/widgets.dart';
import 'package:flutter/material.dart';
import 'package:flutter_gen/gen_l10n/app_localizations.dart';
import 'package:provider/provider.dart';

import '../../models/property_modify_operations.dart';
import '../../models/settings/delta_widget_settings.dart';
import 'unreal_widget_base.dart';

/// Text which acts as a dropdown, but otherwise remains inline.
/// Controls an enum value in Unreal Engine.
class UnrealDropdownText extends UnrealWidget {
  const UnrealDropdownText({
    super.key,
    super.overrideName,
    super.enableProperties,
    required super.unrealProperties,
    this.makeItemName,
  });

  /// Optional function to convert items to a display string. If not provided, the items will be converted implicitly.
  final String Function(String enumValueName)? makeItemName;

  @override
  State<UnrealDropdownText> createState() => _UnrealDropdownTextState();
}

class _UnrealDropdownTextState extends State<UnrealDropdownText>
    with UnrealWidgetStateMixin<UnrealDropdownText, String> {
  @override
  Widget build(BuildContext context) {
    final SingleSharedValue<String> sharedValue = getSingleSharedValue();

    return TransientPreferenceBuilder(
      preference: Provider.of<DeltaWidgetSettings>(context, listen: false).bIsInResetMode,
      builder: (context, final bool bIsInResetMode) => DropdownText<SingleSharedValue<String>>(
        value: sharedValue,
        items: propertyEnumValues
            .map((enumValue) => SingleSharedValue(
                  value: enumValue,
                  bHasMultipleValues: false,
                ))
            .toList(growable: false),
        makeItemName: _makeItemName,
        onChanged: _onChanged,
        bIsEnabled: !bIsInResetMode,
      ),
    );
  }

  @override
  PropertyModifyOperation get modifyOperation => const SetOperation();

  String? _makeItemName(SingleSharedValue<String> value) {
    if (value.bHasMultipleValues) {
      return AppLocalizations.of(context)!.mismatchedValuesLabel;
    }

    if (value.value == null) {
      return null;
    }

    return value.value!;
  }

  void _onChanged(dynamic newValue) {
    assert(newValue is SingleSharedValue<String>);

    final sharedValue = newValue as SingleSharedValue<String>;

    assert(propertyEnumValues.contains(sharedValue.value));

    handleOnChangedByUser(List.filled(widget.unrealProperties.length, sharedValue.value!));
    endTransaction();
  }
}
