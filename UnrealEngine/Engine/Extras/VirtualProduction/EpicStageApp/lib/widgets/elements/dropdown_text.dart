// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../../models/property_modify_operations.dart';
import '../../models/settings/delta_widget_settings.dart';
import '../../utilities/transient_preference.dart';
import '../../utilities/unreal_colors.dart';
import 'asset_icon.dart';
import 'dropdown_button.dart';
import 'unreal_widget_base.dart';

/// Padding used inside the origin tab when the dropdown is open.
const EdgeInsets _originTabPadding = EdgeInsets.only(
  left: 8,
  right: 4,
  top: 12,
  bottom: 12,
);

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
      builder: (context, final bool bIsInResetMode) => DrivenDropdownText<SingleSharedValue<String>>(
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
      return 'Multiple Values';
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

/// Text which acts as a dropdown, but otherwise remains inline.
class DrivenDropdownText<ItemType> extends StatelessWidget {
  const DrivenDropdownText({
    Key? key,
    required this.value,
    required this.items,
    required this.onChanged,
    this.makeItemName,
    this.bIsEnabled = true,
  }) : super(key: key);

  /// The currently selected value.
  final ItemType value;

  /// The list of selectable items in the order in which they should be displayed.
  final List<ItemType> items;

  /// Function called when a new value is selected.
  final void Function(ItemType newValue) onChanged;

  /// Optional function to convert items to a display string. If not provided, the items will be converted implicitly.
  final String? Function(ItemType item)? makeItemName;

  /// If false, grey out the widget and disable its controls.
  final bool bIsEnabled;

  @override
  Widget build(BuildContext context) {
    // Align the row based on text directionality, respecting the text direction.
    late final MainAxisAlignment alignment;
    switch (DefaultTextStyle.of(context).textAlign) {
      case TextAlign.start:
        alignment = MainAxisAlignment.start;
        break;

      case TextAlign.end:
        alignment = MainAxisAlignment.end;
        break;

      case TextAlign.center:
        alignment = MainAxisAlignment.center;
        break;

      case TextAlign.left:
        alignment = (Directionality.of(context) == TextDirection.ltr) ? MainAxisAlignment.start : MainAxisAlignment.end;
        break;

      case TextAlign.right:
        alignment = (Directionality.of(context) == TextDirection.ltr) ? MainAxisAlignment.end : MainAxisAlignment.start;
        break;

      default:
        alignment = MainAxisAlignment.start;
    }

    if (!bIsEnabled) {
      return DefaultTextStyle(
        style: DefaultTextStyle.of(context).style.copyWith(
              color: DefaultTextStyle.of(context).style.color!.withOpacity(0.4),
            ),
        child: _DrivenDropdownTextInner(
          text: _makeItemName(value),
          alignment: alignment,
          bHideChevron: true,
        ),
      );
    }

    return ModalTextDropdownButton(
      value: value,
      items: items,
      onChanged: onChanged,
      makeItemName: _makeItemName,
      openRectExtraSpace: _originTabPadding,
      builder: (BuildContext innerContext, ModalDropdownButtonState state) {
        final String name = _makeItemName(value);

        if (state == ModalDropdownButtonState.closed) {
          return _DrivenDropdownTextInner(
            text: name,
            alignment: alignment,
          );
        }

        final bool bIsOnTop = state == ModalDropdownButtonState.openOnTop;

        return Container(
          decoration: BoxDecoration(
            color: UnrealColors.gray22,
            borderRadius: BorderRadius.vertical(
              top: bIsOnTop ? Radius.circular(8) : Radius.zero,
              bottom: !bIsOnTop ? Radius.circular(8) : Radius.zero,
            ),
          ),
          padding: _originTabPadding,
          child: DefaultTextStyle(
            style: DefaultTextStyle.of(context).style,
            child: _DrivenDropdownTextInner(
              text: name,
              alignment: MainAxisAlignment.start,
              color: UnrealColors.white,
            ),
          ),
        );
      },
    );
  }

  /// Convert an item to a display name, using the user-provided function if possible.
  String _makeItemName(ItemType item) {
    if (item == null) {
      return '...';
    }

    if (makeItemName != null) {
      return makeItemName!(item) ?? '...';
    }

    return item.toString();
  }
}

/// The inner contents of a [DrivenDropdownText], showing the text and the chevron icon.
class _DrivenDropdownTextInner extends StatelessWidget {
  const _DrivenDropdownTextInner({
    Key? key,
    required this.text,
    required this.alignment,
    this.color,
    this.bHideChevron = false,
  }) : super(key: key);

  final String text;
  final MainAxisAlignment alignment;
  final Color? color;
  final bool bHideChevron;

  @override
  Widget build(BuildContext context) {
    TextStyle textStyle = DefaultTextStyle.of(context).style;
    if (color != null) {
      textStyle = textStyle.copyWith(color: color);
    }

    return Row(
      mainAxisSize: MainAxisSize.min,
      mainAxisAlignment: alignment,
      children: [
        Flexible(
          child: Text(
            text,
            overflow: TextOverflow.ellipsis,
            softWrap: false,
            style: textStyle,
          ),
        ),
        if (!bHideChevron)
          SizedOverflowBox(
            size: const Size(24, 15),
            alignment: Alignment.center,
            child: SizedBox(
              width: 24,
              child: AssetIcon(
                path: 'assets/images/icons/chevron_down.svg',
                size: 24,
                color: textStyle.color,
                fit: BoxFit.cover,
              ),
            ),
          ),
      ],
    );
  }
}
