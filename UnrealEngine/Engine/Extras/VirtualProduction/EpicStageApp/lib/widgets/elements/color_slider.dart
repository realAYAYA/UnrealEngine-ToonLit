// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:flutter/material.dart';
import 'package:flutter_gen/gen_l10n/app_localizations.dart';

import '../../models/color.dart';
import '../../models/property_modify_operations.dart';
import 'delta_slider.dart';
import 'delta_widget_base.dart';
import 'unreal_widget_base.dart';

typedef ColorSliderValueData = DeltaWidgetValueData<WheelColor>;

/// A delta-based slider that controls the red component of one or more colors in Unreal Editor.
class UnrealColorGradingRedSlider extends _UnrealColorComponentSlider {
  const UnrealColorGradingRedSlider({
    required super.unrealProperties,
    required super.enableProperties,
    required super.colorMin,
    required super.colorMax,
    super.key,
    super.overrideName,
  });

  @override
  State<StatefulWidget> createState() => _UnrealColorGradingRedSliderState();
}

class _UnrealColorGradingRedSliderState extends State<UnrealColorGradingRedSlider>
    with
        UnrealWidgetStateMixin<UnrealColorGradingRedSlider, WheelColor>,
        _UnrealColorComponentSliderState<UnrealColorGradingRedSlider> {
  @override
  Widget build(BuildContext context) {
    return DrivenColorRedSlider(
      values: makeValueDataList(),
      onChanged: handleOnChangedByUser,
      onInteractionFinished: endTransaction,
      min: widget.colorMin,
      max: widget.colorMax,
      label: widget.overrideName,
    );
  }
}

/// A delta-based slider that controls the green component of one or more colors in Unreal Editor.
class UnrealColorGradingGreenSlider extends _UnrealColorComponentSlider {
  const UnrealColorGradingGreenSlider({
    required super.unrealProperties,
    required super.enableProperties,
    required super.colorMin,
    required super.colorMax,
    super.key,
    super.overrideName,
  });

  @override
  State<StatefulWidget> createState() => _UnrealColorGradingGreenSliderState();
}

class _UnrealColorGradingGreenSliderState extends State<UnrealColorGradingGreenSlider>
    with
        UnrealWidgetStateMixin<UnrealColorGradingGreenSlider, WheelColor>,
        _UnrealColorComponentSliderState<UnrealColorGradingGreenSlider> {
  @override
  Widget build(BuildContext context) {
    return DrivenColorGreenSlider(
      values: makeValueDataList(),
      onChanged: handleOnChangedByUser,
      onInteractionFinished: endTransaction,
      min: widget.colorMin,
      max: widget.colorMax,
      label: widget.overrideName,
    );
  }
}

/// A delta-based slider that controls the blue component of one or more colors in Unreal Editor.
class UnrealColorGradingBlueSlider extends _UnrealColorComponentSlider {
  const UnrealColorGradingBlueSlider({
    required super.unrealProperties,
    required super.enableProperties,
    required super.colorMin,
    required super.colorMax,
    super.key,
    super.overrideName,
  });

  @override
  State<StatefulWidget> createState() => _UnrealColorGradingBlueSliderState();
}

class _UnrealColorGradingBlueSliderState extends State<UnrealColorGradingBlueSlider>
    with
        UnrealWidgetStateMixin<UnrealColorGradingBlueSlider, WheelColor>,
        _UnrealColorComponentSliderState<UnrealColorGradingBlueSlider> {
  @override
  Widget build(BuildContext context) {
    return DrivenColorBlueSlider(
      values: makeValueDataList(),
      onChanged: handleOnChangedByUser,
      onInteractionFinished: endTransaction,
      min: widget.colorMin,
      max: widget.colorMax,
      label: widget.overrideName,
    );
  }
}

/// A delta-based slider that controls the hue component of one or more colors in Unreal Editor.
class UnrealColorGradingHueSlider extends _UnrealColorComponentSlider {
  const UnrealColorGradingHueSlider({
    required super.unrealProperties,
    required super.enableProperties,
    required super.colorMin,
    required super.colorMax,
    super.key,
    super.overrideName,
  });

  @override
  State<StatefulWidget> createState() => _UnrealColorGradingHueSliderState();
}

class _UnrealColorGradingHueSliderState extends State<UnrealColorGradingHueSlider>
    with
        UnrealWidgetStateMixin<UnrealColorGradingHueSlider, WheelColor>,
        _UnrealColorComponentSliderState<UnrealColorGradingHueSlider> {
  @override
  Widget build(BuildContext context) {
    return DrivenColorHueSlider(
      values: makeValueDataList(),
      onChanged: handleOnChangedByUser,
      onInteractionFinished: endTransaction,
      min: widget.colorMin,
      max: widget.colorMax,
      label: widget.overrideName,
    );
  }
}

/// A delta-based slider that controls the saturation component of one or more colors in Unreal Editor.
class UnrealColorGradingSaturationSlider extends _UnrealColorComponentSlider {
  const UnrealColorGradingSaturationSlider({
    required super.unrealProperties,
    required super.enableProperties,
    required super.colorMin,
    required super.colorMax,
    super.key,
    super.overrideName,
  });

  @override
  State<StatefulWidget> createState() => _UnrealColorGradingSaturationSliderState();
}

class _UnrealColorGradingSaturationSliderState extends State<UnrealColorGradingSaturationSlider>
    with
        UnrealWidgetStateMixin<UnrealColorGradingSaturationSlider, WheelColor>,
        _UnrealColorComponentSliderState<UnrealColorGradingSaturationSlider> {
  @override
  Widget build(BuildContext context) {
    return DrivenColorSaturationSlider(
      values: makeValueDataList(),
      onChanged: handleOnChangedByUser,
      onInteractionFinished: endTransaction,
      min: widget.colorMin,
      max: widget.colorMax,
      label: widget.overrideName,
    );
  }
}

/// A delta-based slider that controls the value component of one or more colors in Unreal Editor.
class UnrealColorGradingValueSlider extends _UnrealColorComponentSlider {
  const UnrealColorGradingValueSlider({
    required super.unrealProperties,
    required super.enableProperties,
    required super.colorMin,
    required super.colorMax,
    super.key,
    super.overrideName,
  });

  @override
  State<StatefulWidget> createState() => _UnrealColorGradingValueSliderState();
}

class _UnrealColorGradingValueSliderState extends State<UnrealColorGradingValueSlider>
    with
        UnrealWidgetStateMixin<UnrealColorGradingValueSlider, WheelColor>,
        _UnrealColorComponentSliderState<UnrealColorGradingValueSlider> {
  @override
  Widget build(BuildContext context) {
    return DrivenColorValueSlider(
      values: makeValueDataList(),
      onChanged: handleOnChangedByUser,
      onInteractionFinished: endTransaction,
      min: widget.colorMin,
      max: widget.colorMax,
      label: widget.overrideName,
    );
  }
}

/// A delta-based slider widget that controls the alpha component of one or more color properties.
class UnrealColorGradingAlphaSlider extends _UnrealColorComponentSlider {
  const UnrealColorGradingAlphaSlider({
    required super.unrealProperties,
    required super.enableProperties,
    required super.colorMin,
    required super.colorMax,
    super.key,
    super.overrideName,
  });

  @override
  State<StatefulWidget> createState() => _UnrealColorGradingAlphaSliderState();
}

class _UnrealColorGradingAlphaSliderState extends State<UnrealColorGradingAlphaSlider>
    with
        UnrealWidgetStateMixin<UnrealColorGradingAlphaSlider, WheelColor>,
        _UnrealColorComponentSliderState<UnrealColorGradingAlphaSlider> {
  @override
  Widget build(BuildContext context) {
    return DrivenColorAlphaSlider(
      values: makeValueDataList(),
      onChanged: handleOnChangedByUser,
      onInteractionFinished: endTransaction,
      min: widget.colorMin,
      max: widget.colorMax,
      label: widget.overrideName,
    );
  }
}

/// Mixin for state of widgets that extend [_UnrealColorComponentSlider].
mixin _UnrealColorComponentSliderState<WidgetType extends _UnrealColorComponentSlider>
    on UnrealWidgetStateMixin<WidgetType, WheelColor> {
  @override
  PropertyModifyOperation get modifyOperation => WheelColorGradingAddOperation(
        saturationExponent: 1.0,
        minValue: widget.colorMin,
        maxValue: widget.colorMax,
      );
}

/// A delta-based slider widget that controls the red component of one or more color properties.
class DrivenColorRedSlider extends _DrivenColorComponentSlider {
  const DrivenColorRedSlider({
    super.key,
    required super.values,
    required super.onChanged,
    super.bIsEnabled,
    super.min = 0.0,
    super.max = 1.0,
    super.label,
    super.onInteractionFinished,
  });

  @override
  State<StatefulWidget> createState() => _DrivenColorRedSliderState();
}

class _DrivenColorRedSliderState extends _DrivenRGBComponentSliderState {
  @override
  String get defaultLabel => AppLocalizations.of(context)!.colorComponentRed;

  @override
  Color? get fillColor => Color.fromARGB(255, 255, 0, 0);

  @override
  double getLinearValue(FloatColor color) => color.red;

  @override
  FloatColor makeColorWithComponentValue(FloatColor color, double newComponentValue) =>
      FloatColor(newComponentValue, color.green, color.blue, color.alpha);
}

/// A delta-based slider widget that controls the green component of one or more color properties.
class DrivenColorGreenSlider extends _DrivenColorComponentSlider {
  const DrivenColorGreenSlider({
    super.key,
    required super.values,
    required super.onChanged,
    super.bIsEnabled,
    super.min = 0.0,
    super.max = 1.0,
    super.label,
    super.onInteractionFinished,
  });

  @override
  State<StatefulWidget> createState() => _DrivenColorGreenSliderState();
}

class _DrivenColorGreenSliderState extends _DrivenRGBComponentSliderState {
  @override
  String get defaultLabel => AppLocalizations.of(context)!.colorComponentGreen;

  @override
  Color? get fillColor => Color.fromARGB(255, 0, 255, 0);

  @override
  double getLinearValue(FloatColor color) => color.green;

  @override
  FloatColor makeColorWithComponentValue(FloatColor color, double newComponentValue) =>
      FloatColor(color.red, newComponentValue, color.blue, color.alpha);
}

/// A delta-based slider widget that controls the blue component of one or more color properties.
class DrivenColorBlueSlider extends _DrivenColorComponentSlider {
  const DrivenColorBlueSlider({
    super.key,
    required super.values,
    required super.onChanged,
    super.bIsEnabled,
    super.min = 0.0,
    super.max = 1.0,
    super.label,
    super.onInteractionFinished,
  });

  @override
  State<StatefulWidget> createState() => _DrivenColorBlueSliderState();
}

class _DrivenColorBlueSliderState extends _DrivenRGBComponentSliderState {
  @override
  String get defaultLabel => AppLocalizations.of(context)!.colorComponentBlue;

  @override
  Color? get fillColor => Color.fromARGB(255, 0, 0, 255);

  @override
  double getLinearValue(FloatColor color) => color.blue;

  @override
  FloatColor makeColorWithComponentValue(FloatColor color, double newComponentValue) =>
      FloatColor(color.red, color.green, newComponentValue, color.alpha);
}

/// A delta-based slider widget that controls the value component of one or more color properties.
class DrivenColorHueSlider extends _DrivenColorComponentSlider {
  const DrivenColorHueSlider({
    super.key,
    required super.values,
    required super.onChanged,
    super.bIsEnabled,
    super.min = 0.0,
    super.max = 1.0,
    super.label,
    super.onInteractionFinished,
  });

  @override
  State<StatefulWidget> createState() => _DrivenColorHueSliderState();
}

class _DrivenColorHueSliderState extends State<DrivenColorHueSlider>
    with DeltaWidgetStateMixin<DrivenColorHueSlider>, _DrivenColorComponentSliderState<DrivenColorHueSlider> {
  @override
  String get defaultLabel => AppLocalizations.of(context)!.colorComponentHue;

  @override
  double get sliderMin => 0.0;

  @override
  double get sliderMax => 360.0;

  @override
  NegatableHSVColor applyLinearDelta(NegatableHSVColor currentColor, double delta) {
    // Apply the change in hue to the wheel color's hue
    final double deltaHue = delta;
    final double newHue = (currentColor.hue + deltaHue + sliderMax) % sliderMax;

    // Convert back to a wheel color, then subtract the existing color to produce a delta
    return currentColor.withHue(newHue);
  }

  @override
  double convertToLinearValue(WheelColor color) => color.toHSVColor().hue;
}

/// A delta-based slider widget that controls the saturation component of one or more color properties.
class DrivenColorSaturationSlider extends _DrivenColorComponentSlider {
  const DrivenColorSaturationSlider({
    super.key,
    required super.values,
    required super.onChanged,
    super.bIsEnabled,
    super.min = 0.0,
    super.max = 1.0,
    super.label,
    super.onInteractionFinished,
  });

  @override
  State<StatefulWidget> createState() => _DrivenColorSaturationSliderState();
}

class _DrivenColorSaturationSliderState extends _DrivenHSVComponentSliderState {
  @override
  double get sliderMin => 0.0;

  @override
  double get sliderMax => 1.0;

  @override
  String get defaultLabel => AppLocalizations.of(context)!.colorComponentSaturation;

  @override
  double getLinearValue(NegatableHSVColor color) => color.saturation;

  @override
  NegatableHSVColor makeColorWithComponentValue(NegatableHSVColor color, double newComponentValue) =>
      NegatableHSVColor.fromAHSV(color.alpha, color.hue, newComponentValue, color.value);
}

/// A delta-based slider widget that controls the value component of one or more color properties.
class DrivenColorValueSlider extends _DrivenColorComponentSlider {
  const DrivenColorValueSlider({
    super.key,
    required super.values,
    required super.onChanged,
    super.bIsEnabled,
    super.min = 0.0,
    super.max = 1.0,
    super.label,
    super.onInteractionFinished,
  });

  @override
  State<StatefulWidget> createState() => _DrivenColorValueSliderState();
}

class _DrivenColorValueSliderState extends _DrivenHSVComponentSliderState {
  @override
  double get sliderMin => widget.min;

  @override
  double get sliderMax => widget.max;

  @override
  String get defaultLabel => AppLocalizations.of(context)!.colorComponentValue;

  @override
  double getLinearValue(NegatableHSVColor color) => color.value;

  @override
  NegatableHSVColor makeColorWithComponentValue(NegatableHSVColor color, double newComponentValue) =>
      NegatableHSVColor.fromAHSV(color.alpha, color.hue, color.saturation, newComponentValue);
}

/// A delta-based slider widget that controls the alpha component of one or more color properties.
class DrivenColorAlphaSlider extends _DrivenColorComponentSlider {
  const DrivenColorAlphaSlider({
    super.key,
    required super.values,
    required super.onChanged,
    super.bIsEnabled,
    super.min = 0.0,
    super.max = 1.0,
    super.label,
    super.onInteractionFinished,
  });

  @override
  State<StatefulWidget> createState() => _DrivenColorAlphaSliderState();
}

class _DrivenColorAlphaSliderState extends _DrivenHSVComponentSliderState {
  @override
  double get sliderMin => widget.min;

  @override
  double get sliderMax => widget.max;

  @override
  String get defaultLabel => AppLocalizations.of(context)!.colorComponentAlpha;

  @override
  double getLinearValue(NegatableHSVColor color) => color.alpha;

  @override
  NegatableHSVColor makeColorWithComponentValue(NegatableHSVColor color, double newComponentValue) =>
      NegatableHSVColor.fromAHSV(newComponentValue, color.hue, color.saturation, color.value);
}

/// Base state class for sliders that operate on RGBA component values.
abstract class _DrivenRGBComponentSliderState<WidgetType extends _DrivenColorComponentSlider> extends State<WidgetType>
    with DeltaWidgetStateMixin<WidgetType>, _DrivenColorComponentSliderState<WidgetType> {
  @override
  double get sliderMin => widget.min;

  @override
  double get sliderMax => widget.max;

  /// Get the linear value this slider modifies from the given color.
  double getLinearValue(FloatColor color);

  /// Create a new color by replacing the value that this slider modifies.
  FloatColor makeColorWithComponentValue(FloatColor color, double newComponentValue);

  @override
  NegatableHSVColor applyLinearDelta(NegatableHSVColor currentColor, double delta) {
    // Apply the change to the relevant component
    final currentRGBA = FloatColor.fromHSVColor(currentColor);
    final double newValue = (getLinearValue(currentRGBA) + delta).clamp(sliderMin, sliderMax);

    return NegatableHSVColor.fromColor(makeColorWithComponentValue(currentRGBA, newValue));
  }

  @override
  double convertToLinearValue(WheelColor color) => getLinearValue(color.toFloatColor());
}

/// Base state class for sliders that operate on clamped HSV component values.
abstract class _DrivenHSVComponentSliderState<WidgetType extends _DrivenColorComponentSlider> extends State<WidgetType>
    with DeltaWidgetStateMixin<WidgetType>, _DrivenColorComponentSliderState<WidgetType> {
  /// Get the linear value this slider modifies from the given color.
  double getLinearValue(NegatableHSVColor color);

  /// Create a new color by replacing the value that this slider modifies.
  NegatableHSVColor makeColorWithComponentValue(NegatableHSVColor color, double newComponentValue);

  @override
  NegatableHSVColor applyLinearDelta(NegatableHSVColor currentColor, double delta) {
    // Apply the change to the relevant component
    final double newValue = (getLinearValue(currentColor) + delta).clamp(sliderMin, sliderMax);

    return makeColorWithComponentValue(currentColor, newValue);
  }

  @override
  double convertToLinearValue(WheelColor color) => getLinearValue(color.toHSVColor());
}

/// Base class for sliders that control a component of a color that needs to be calculated based on the full color data,
/// and which control these properties in Unreal Editor.
abstract class _UnrealColorComponentSlider extends UnrealWidget {
  const _UnrealColorComponentSlider({
    required super.unrealProperties,
    required super.enableProperties,
    super.key,
    super.overrideName,
    required this.colorMin,
    required this.colorMax,
  });

  /// The minimum value of the color's components.
  final double colorMin;

  /// The maximum value of the color's components.
  final double colorMax;
}

/// Base class for sliders that control a component of a color that needs to be calculated based on the full color data.
abstract class _DrivenColorComponentSlider extends StatefulWidget {
  const _DrivenColorComponentSlider({
    Key? key,
    required this.values,
    required this.onChanged,
    this.bIsEnabled = true,
    this.min = 0.0,
    this.max = 1.0,
    this.label,
    this.onInteractionFinished,
  }) : super(key: key);

  /// Colors to display on the slider.
  final List<ColorSliderValueData?> values;

  /// Name of the property this widget controls.
  final String? label;

  /// If false, grey out the widget and disable its controls.
  final bool bIsEnabled;

  /// The minimum value of the color's components.
  final double min;

  /// The maximum value of the color's components.
  final double max;

  /// Function called when the user changes the value of the slider.
  /// Passes the amount by which each value changed.
  final void Function(List<WheelColor>) onChanged;

  /// Function called when the user is done interacting with the widget.
  final void Function()? onInteractionFinished;
}

/// Mixin for state of widgets that extend [_DrivenColorComponentSlider].
mixin _DrivenColorComponentSliderState<WidgetType extends _DrivenColorComponentSlider>
    on DeltaWidgetStateMixin<WidgetType> {
  /// The label to show on the slider if no override is provided.
  String get defaultLabel;

  /// The minimum value of the color's components.
  double get sliderMin;

  /// The maximum value of the color's components.
  double get sliderMax;

  /// The fill color of the slider.
  Color? get fillColor => null;

  @override
  Widget build(BuildContext context) {
    return DrivenDeltaSlider(
      values: _makeSliderValueList(),
      bShowResetButton: false,
      onChanged: _handleOnSliderChangedByUser,
      onInteractionFinished: widget.onInteractionFinished,
      min: sliderMin,
      max: sliderMax,
      label: widget.label ?? defaultLabel,
      fillColor: fillColor,
      bIsEnabled: widget.bIsEnabled,
    );
  }

  /// Convert a color value to the linear value the slider controls.
  double convertToLinearValue(WheelColor color);

  /// Apply the linear delta value from the slider to the calculated component of the color.
  NegatableHSVColor applyLinearDelta(NegatableHSVColor currentColor, double delta);

  /// Generate a list of linear values corresponding to each controlled property.
  List<DeltaWidgetValueData<double>?> _makeSliderValueList() {
    return widget.values.map((final DeltaWidgetValueData<WheelColor>? color) {
      if (color == null) {
        return null;
      }

      return DeltaWidgetValueData<double>(value: convertToLinearValue(color.value));
    }).toList(growable: false);
  }

  /// Called when the user changes the controlled linear values.
  void _handleOnSliderChangedByUser(List<double> deltaValues) {
    assert(deltaValues.length == widget.values.length);

    final List<DeltaWidgetValueData<WheelColor>?> currentColors = widget.values;
    final List<WheelColor> colorDeltas = [];

    for (int propertyIndex = 0; propertyIndex < deltaValues.length; ++propertyIndex) {
      final WheelColor? currentColor = currentColors[propertyIndex]?.value;
      if (currentColor == null) {
        colorDeltas.add(WheelColor.zero);
        continue;
      }

      // Apply the change in hue, then subtract the old color from the new one to determine the delta
      final NegatableHSVColor newHsvColor = applyLinearDelta(currentColor.toHSVColor(), deltaValues[propertyIndex]);
      final WheelColor newColor = WheelColor.fromHSVColor(newHsvColor);
      colorDeltas.add(newColor - currentColor);
    }

    widget.onChanged(colorDeltas);
  }
}
