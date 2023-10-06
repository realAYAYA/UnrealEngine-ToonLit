// Copyright Epic Games, Inc. All Rights Reserved.

import 'dart:math' as math;

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_gen/gen_l10n/app_localizations.dart';
import 'package:provider/provider.dart';

import '../../models/navigator_keys.dart';
import '../../models/property_modify_operations.dart';
import '../../models/settings/delta_widget_settings.dart';
import '../../utilities/math_utilities.dart';
import '../../utilities/transient_preference.dart';
import '../../utilities/unreal_colors.dart';
import 'delta_widget_base.dart';
import 'epic_icon_button.dart';
import 'modal.dart';
import 'unreal_widget_base.dart';

const double _dotSize = 17.0;
const double _trackPadding = DeltaWidgetConstants.widgetOuterXPadding + DeltaWidgetConstants.widgetInnerXPadding;
const double _trackHeight = 3.0;

typedef DeltaSliderValueData = DeltaWidgetValueData<double>;

/// A delta-based slider widget that controls a property remotely in Unreal Engine.
class UnrealDeltaSlider extends UnrealWidget {
  const UnrealDeltaSlider({
    super.key,
    super.overrideName,
    super.minMaxBehaviour,
    super.enableProperties,
    required super.unrealProperties,
    this.buildLabel,
    this.softMin,
    this.softMax,
    this.hardMin,
    this.hardMax,
    this.bShowResetButton = true,
    this.fillColor,
  });

  /// If provided, call this function to build the widget displayed as the slider's label. Otherwise, the label will be
  /// automatically generated.
  final Widget Function(String name)? buildLabel;

  /// If provided, ignore the engine-provided minimum value for the property and use this instead for display.
  /// Note that this changes the min value displayed on the slider, but the user can continue sliding to reach lower
  /// values.
  final double? softMin;

  /// If provided, ignore the engine-provided maximum value for the property and use this instead for display.
  /// Note that this changes the max value displayed on the slider, but the user can continue sliding to reach higher
  /// values.
  final double? softMax;

  /// If provided, ignore the engine-provided minimum value for the property and use this instead for clamping.
  /// This changes both the max value displayed on the slider and prevents sliding past this value.
  final double? hardMin;

  /// If provided, ignore the engine-provided maximum value for the property and use this instead for clamping.
  /// This changes both the max value displayed on the slider and prevents sliding past this value.
  final double? hardMax;

  /// If true, show the reset button.
  final bool bShowResetButton;

  /// The color to show in the filled portion of the bar (left of the value indicators).
  final Color? fillColor;

  @override
  _UnrealDeltaSliderState createState() => _UnrealDeltaSliderState();
}

class _UnrealDeltaSliderState extends State<UnrealDeltaSlider> with UnrealWidgetStateMixin<UnrealDeltaSlider, double> {
  @override
  double? get overrideMin => widget.hardMin ?? engineMin;

  @override
  double? get overrideMax => widget.hardMax ?? engineMax;

  @override
  Widget build(BuildContext context) {
    return TransientPreferenceBuilder(
      preference: Provider.of<DeltaWidgetSettings>(context, listen: false).bIsInResetMode,
      builder: (context, final bool bIsInResetMode) => DrivenDeltaSlider(
        values: makeValueDataList(),
        bShowResetButton: bIsInResetMode && widget.bShowResetButton,
        bIsEnabled: !bIsInResetMode,
        onChanged: handleOnChangedByUser,
        onValuesSet: _onValuesSet,
        onReset: handleOnResetByUser,
        onInteractionFinished: endTransaction,
        min: widget.softMin ?? widget.hardMin ?? engineMin,
        max: widget.softMax ?? widget.hardMax ?? engineMax,
        label: propertyLabel,
        buildLabel: widget.buildLabel,
        fillColor: widget.fillColor,
      ),
    );
  }

  /// Replace all existing values with a new list of values.
  void _onValuesSet(List<double> newValues) {
    modifyProperties(const SetOperation(), values: newValues, bIgnoreLimits: true);
  }
}

/// A DeltaSlider whose values are controlled from outside of the widget itself.
class DrivenDeltaSlider extends StatefulWidget {
  const DrivenDeltaSlider({
    Key? key,
    required this.values,
    required this.onChanged,
    this.onValuesSet,
    this.label = '',
    this.buildLabel,
    this.min = 0.0,
    this.max = 1.0,
    this.exponent = 1.0,
    this.bShowResetButton = true,
    this.bIgnoreSensitivity = false,
    this.bIsEnabled = true,
    this.defaultValue,
    this.onReset,
    this.onInteractionFinished,
    this.fillColor,
    this.baseSensitivity,
  }) : super(key: key);

  /// Values to display on the slider.
  final List<DeltaSliderValueData?> values;

  /// Name of the property this widget controls.
  final String label;

  /// If provided, call this function to build the widget displayed as the slider's label. Otherwise, the label will be
  /// automatically generated.
  final Widget Function(String name)? buildLabel;

  /// Minimum value the user can select.
  final double? min;

  /// Maximum value the user can select.
  final double? max;

  /// Exponent value applied to the slider's linear position. Higher exponents make the slider's effective delta value
  /// smaller at smaller slider values.
  final double exponent;

  /// If true, show the reset button.
  final bool bShowResetButton;

  /// If true, don't take sensitivity settings into account when calculating delta values.
  final bool bIgnoreSensitivity;

  /// If false, grey out the widget and don't accept inputs. Note that this doesn't affect the reset button.
  final bool bIsEnabled;

  /// Default value to reset to when the reset button is pressed.
  final double? defaultValue;

  /// Function called when the reset button is pressed. Called after the value has been reset.
  final void Function()? onReset;

  /// Function called when the user is done interacting with the widget.
  final void Function()? onInteractionFinished;

  /// Function called when the user changes the value of the slider.
  /// Passes the amount by which each value changed.
  final void Function(List<double>) onChanged;

  /// Function called when the user directly sets the values of the slider.
  /// Passes the new list of values.
  /// If not set, these will be converted to deltas and passed to onChanged instead.
  final void Function(List<double>)? onValuesSet;

  /// The color to show in the filled portion of the bar (left of the value indicators).
  final Color? fillColor;

  /// How sensitive the slider should be when the user's sensitivity is set to 1.0.
  final double? baseSensitivity;

  @override
  _DrivenDeltaSliderState createState() => _DrivenDeltaSliderState();
}

class _DrivenDeltaSliderState extends State<DrivenDeltaSlider> with DeltaWidgetStateMixin {
  /// How long the min/max labels take to fade in/out.
  static const _minMaxFadeDuration = Duration(milliseconds: 100);

  bool _bIsBeingDragged = false;

  @override
  bool get bIgnoreSensitivity => widget.bIgnoreSensitivity;

  @override
  double get baseDeltaMultiplier => widget.baseSensitivity ?? super.baseDeltaMultiplier;

  /// Get the slider's minimum value.
  /// If the widget has a set minimum, return it. Otherwise, default to a minimum of 0, but extend the minimum to match
  /// the lowest controlled value.
  double get _min {
    if (widget.min != null) {
      return widget.min!;
    }

    double minValue = 0.0;
    for (final DeltaSliderValueData? valueData in widget.values) {
      if (valueData != null) {
        minValue = math.min(valueData.value, minValue);
      }
    }

    return minValue;
  }

  /// Get the slider's maximum value.
  /// If the widget has a set maximum, return it. Otherwise, default to a maximum of 1, but extend the maximum to match
  /// the highest controlled value.
  double get _max {
    if (widget.max != null) {
      return widget.max!;
    }

    double maxValue = 1.0;
    for (final DeltaSliderValueData? valueData in widget.values) {
      if (valueData != null) {
        maxValue = math.max(valueData.value, maxValue);
      }
    }

    return maxValue;
  }

  double get _valueSpan => _max - _min;

  double get _labelOpacity => widget.bIsEnabled ? 1.0 : 0.4;

  TextStyle get minMaxLabelStyle => Theme.of(context).textTheme.labelMedium!.copyWith(
        fontSize: 10,
        color: UnrealColors.gray42.withOpacity(_labelOpacity),
      );

  @override
  Widget build(BuildContext context) {
    final String? valueString =
        (widget.values.length == 1 && widget.values[0] != null) ? _getStringForValue(widget.values[0]!.value) : null;

    // Build the list of slider values to pass to the slider widget
    final List<_DeltaSliderUIValue> uiValues = [];
    for (final DeltaSliderValueData? valueData in widget.values) {
      if (valueData != null) {
        uiValues.add(
            _DeltaSliderUIValue(valueData: valueData, normalizedValue: _getSliderPositionForValue(valueData.value)));
      }
    }

    return DefaultTextStyle(
      style: Theme.of(context).textTheme.labelMedium!.copyWith(
            color: Theme.of(context).textTheme.labelMedium!.color!.withOpacity(_labelOpacity),
          ),
      child: Padding(
        padding: const EdgeInsets.only(bottom: 5),
        child: Row(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Expanded(
              child: Column(children: [
                Row(children: [
                  // Main slider
                  Expanded(
                    child: GestureDetector(
                      onHorizontalDragStart: _onDragStart,
                      onHorizontalDragUpdate: _onDragUpdate,
                      onHorizontalDragEnd: ((_) => _onDragEnd()),
                      onHorizontalDragCancel: _onDragEnd,
                      onDoubleTap: _showDirectEntryModal,
                      behavior: HitTestBehavior.opaque,
                      child: Column(
                        children: [
                          // Labels
                          Padding(
                            padding: const EdgeInsets.symmetric(horizontal: DeltaWidgetConstants.widgetOuterXPadding),
                            child: ConstrainedBox(
                              constraints: BoxConstraints.tightForFinite(),
                              child: Row(
                                mainAxisAlignment: MainAxisAlignment.spaceBetween,
                                children: [
                                  Expanded(child: _buildLabel(widget.label)),
                                  if (valueString != null)
                                    Padding(
                                      padding: EdgeInsets.only(left: 6),
                                      child: Text(valueString),
                                    ),
                                ],
                              ),
                            ),
                          ),

                          const SizedBox(height: 2),

                          // Slider
                          _DeltaSliderSlider(
                            uiValues: uiValues,
                            fillColor: widget.fillColor,
                            bIsEnabled: widget.bIsEnabled,
                          ),

                          // Min/max labels
                          AnimatedOpacity(
                            opacity: _bIsBeingDragged ? 1 : 0,
                            duration: _minMaxFadeDuration,
                            child: Padding(
                              padding: const EdgeInsets.only(
                                left: DeltaWidgetConstants.widgetOuterXPadding,
                                right: DeltaWidgetConstants.widgetOuterXPadding,
                              ),
                              child: Row(
                                mainAxisAlignment: MainAxisAlignment.spaceBetween,
                                children: [
                                  Text(_getStringForValue(_min), style: minMaxLabelStyle),
                                  Text(_getStringForValue(_max), style: minMaxLabelStyle),
                                ],
                              ),
                            ),
                          ),
                        ],
                      ),
                    ),
                  ),
                ]),
              ]),
            ),
            if (widget.bShowResetButton)
              Padding(
                padding: const EdgeInsets.only(right: DeltaWidgetConstants.widgetOuterXPadding),
                child: Center(
                  child: ResetValueButton(onPressed: widget.onReset),
                ),
              ),
          ],
        ),
      ),
    );
  }

  /// Normalize and clamp a value to the range of the slider.
  double _normalizeValue(double value) => ((value - _min) / _valueSpan).clamp(0.0, 1.0);

  /// Apply the slider's exponent to a normalized value.
  double _exponentiateNormalizedValue(double normalized) {
    if (widget.exponent == 1.0) {
      return normalized;
    }

    return exponentiateValue(normalized, widget.exponent, true);
  }

  /// Convert a controlled value to the actual range of the slider, including normalization, clamping, and exponent.
  double _getSliderPositionForValue(double value) => _exponentiateNormalizedValue(_normalizeValue(value));

  /// Format a value on the slider as a string for display to the user.
  String _getStringForValue(double value) {
    return value.toStringAsFixed(2);
  }

  /// Called when the user starts dragging on the slider.
  void _onDragStart(DragStartDetails details) {
    setState(() {
      _bIsBeingDragged = true;
    });
  }

  /// Called when the user drags on the slider.
  void _onDragUpdate(DragUpdateDetails details) {
    if (!widget.bIsEnabled) {
      return;
    }

    final double delta = details.primaryDelta! * _valueSpan * deltaMultiplier / (context.size?.width ?? 1.0);
    final List<double> deltaValues = List.filled(widget.values.length, delta);

    if (widget.exponent != 1.0) {
      for (int valueIndex = 0; valueIndex < deltaValues.length; ++valueIndex) {
        final double? currentValue = widget.values[valueIndex]?.value;
        if (currentValue == null) {
          continue;
        }

        final double normalizedValue = _normalizeValue(currentValue);
        final double normalizedDelta = deltaValues[valueIndex] / _valueSpan;

        // Delta is applied by user in exponential space, so find the delta in linear space
        final double exponentialValue = _exponentiateNormalizedValue(normalizedValue);
        final double exponentialResultValue = (exponentialValue + normalizedDelta).clamp(0, 1);
        final double linearResultValue = exponentiateValue(exponentialResultValue, widget.exponent, false);
        deltaValues[valueIndex] = linearResultValue - normalizedValue;
      }
    }

    widget.onChanged(deltaValues);
  }

  /// Called when the user stops dragging on the slider.
  void _onDragEnd() {
    widget.onInteractionFinished?.call();
    setState(() {
      _bIsBeingDragged = false;
    });
  }

  /// Build the widget to display as the slider's label.
  Widget _buildLabel(String name) {
    if (widget.buildLabel != null) {
      return widget.buildLabel!(name);
    }

    return Text(name);
  }

  /// Show the modal dialog to directly enter a value via keyboard.
  void _showDirectEntryModal() {
    if (widget.values.isEmpty || widget.values[0] == null) {
      return;
    }

    final route = GenericModalDialogRoute(
      bResizeToAvoidBottomInset: true,
      builder: (_) => _DeltaSliderDirectInputDialog(
        label: widget.label,
        initialValue: widget.values[0]!.value,
        onValueSet: _setAllValues,
        onReset: widget.onReset,
      ),
    );

    Navigator.of(rootNavigatorKey.currentContext!, rootNavigator: true).push(route);
  }

  /// Change every value to a single, new value.
  void _setAllValues(double newValue) {
    _setValues(List.filled(widget.values.length, newValue));
    widget.onInteractionFinished?.call();
  }

  /// Set the values to a list of new values.
  void _setValues(List<double> newValues) {
    assert(newValues.length == widget.values.length);

    if (widget.onValuesSet != null) {
      widget.onValuesSet!(newValues);
    } else {
      final List<double> deltas = [];

      for (int valueIndex = 0; valueIndex < widget.values.length; ++valueIndex) {
        final value = widget.values[valueIndex];
        if (value == null) {
          deltas.add(0);
          continue;
        }

        deltas.add(newValues[valueIndex] - value.value);
      }

      widget.onChanged(deltas);
    }
  }
}

/// The interactive slider within the slider widget.
class _DeltaSliderSlider extends StatelessWidget {
  const _DeltaSliderSlider({
    Key? key,
    required this.uiValues,
    required this.fillColor,
    this.bIsEnabled = true,
  }) : super(key: key);

  final List<_DeltaSliderUIValue> uiValues;
  final Color? fillColor;
  final bool bIsEnabled;

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      width: double.infinity,
      height: _dotSize,
      child: CustomPaint(
        painter: _DeltaSliderSliderPainter(
          context: context,
          uiValues: uiValues,
          fillColor: fillColor,
          bIsEnabled: bIsEnabled,
        ),
      ),
    );
  }
}

/// Painter for the track and "handle" rings of a delta slider.
class _DeltaSliderSliderPainter extends CustomPainter {
  const _DeltaSliderSliderPainter({
    required this.context,
    required this.uiValues,
    required this.fillColor,
    this.bIsEnabled = true,
  }) : super();

  final BuildContext context;
  final List<_DeltaSliderUIValue> uiValues;
  final Color? fillColor;
  final bool bIsEnabled;

  @override
  void paint(Canvas canvas, Size size) {
    const double ringThickness = 2.8;
    const double disabledOpacity = 0.4;

    final Rect canvasRect = Offset.zero & size;
    canvas.saveLayer(canvasRect, Paint());

    // Determine positions of rings
    final trackWidth = size.width - (_trackPadding * 2);
    final ringSize = Size.square(_dotSize - ringThickness);
    final double ringYOffset = (size.height - ringSize.height) / 2;

    final Iterable<Rect> ringRects = uiValues.map((uiValue) {
      final double ringX = _trackPadding + (uiValue.normalizedValue * trackWidth) - (ringSize.width / 2);
      return Offset(ringX, ringYOffset) & ringSize;
    });

    // Draw the track colors
    final double filledWidth = uiValues.fold(
          0.0,
          (double maxValue, _DeltaSliderUIValue uiValue) => math.max(maxValue, uiValue.normalizedValue),
        ) *
        size.width;

    final Paint trackFillPaint = Paint()
      ..style = PaintingStyle.fill
      ..color = bIsEnabled
          ? (fillColor ?? Theme.of(context).colorScheme.primary)
          : UnrealColors.gray75.withOpacity(disabledOpacity);
    canvas.drawRect(Offset.zero & Size(filledWidth, size.height), trackFillPaint);

    final Paint trackEmptyPaint = Paint()
      ..style = PaintingStyle.fill
      ..color = UnrealColors.gray31.withOpacity(bIsEnabled ? 1.0 : disabledOpacity);
    canvas.drawRect(Offset(filledWidth, 0) & Size(size.width - filledWidth, size.height), trackEmptyPaint);

    canvas.saveLayer(canvasRect, Paint()..blendMode = BlendMode.dstIn);

    // Create a mask in the shape of the track
    final Paint trackPaint = Paint()
      ..style = PaintingStyle.fill
      ..color = Colors.white;

    final trackRect = RRect.fromRectAndRadius(
      Rect.fromLTWH(
        _trackPadding,
        (size.height - _trackHeight) / 2,
        trackWidth,
        _trackHeight,
      ),
      Radius.circular(_trackHeight),
    );
    canvas.drawRRect(trackRect, trackPaint);

    // Erase the mask around the rings
    final Paint ringMaskPaint = Paint()
      ..style = PaintingStyle.fill
      ..color = Colors.transparent
      ..blendMode = BlendMode.clear;

    const double trackRingPadding = 5.5;

    for (final Rect ringRect in ringRects) {
      canvas.drawRect(ringRect.inflate(trackRingPadding), ringMaskPaint);
    }

    // Restore the track color layer, showing only the part overlapping with the mask
    canvas.restore();

    // Draw rings
    final Paint ringStrokePaint = Paint()
      ..style = PaintingStyle.stroke
      ..strokeWidth = ringThickness
      ..color = UnrealColors.gray56.withOpacity(bIsEnabled ? 1.0 : disabledOpacity);

    final Paint ringFillPaint = Paint()
      ..style = PaintingStyle.fill
      ..color = Colors.white
      ..blendMode = BlendMode.dstOut;

    for (final Rect ringRect in ringRects) {
      canvas.drawArc(ringRect, 0, math.pi * 2, false, ringFillPaint);
      canvas.drawArc(ringRect, 0, math.pi * 2, false, ringStrokePaint);
    }

    // Blend combined changes down to the canvas so transparency is preserved
    canvas.restore();
  }

  @override
  bool shouldRepaint(covariant CustomPainter oldDelegate) {
    return oldDelegate != this;
  }
}

/// Represents a value displayed on the delta slider in a format easier to use for UI purposes.
class _DeltaSliderUIValue {
  const _DeltaSliderUIValue({required this.valueData, required this.normalizedValue});

  /// The value of the dot.
  final DeltaSliderValueData valueData;

  /// The value of the dot normalized to the slider's range.
  final double normalizedValue;
}

/// Modal dialog to directly enter the value of a delta slider.
class _DeltaSliderDirectInputDialog extends StatefulWidget {
  const _DeltaSliderDirectInputDialog({
    Key? key,
    required this.label,
    required this.initialValue,
    required this.onValueSet,
    this.onReset,
  }) : super(key: key);

  /// The label of the property to modify.
  final String label;

  /// The property's initial value before editing.
  final double initialValue;

  /// Called when a new value is set.
  final Function(double) onValueSet;

  /// Function called when the reset button is pressed.
  final void Function()? onReset;

  @override
  State<StatefulWidget> createState() => _DeltaSliderDirectInputDialogState();
}

class _DeltaSliderDirectInputDialogState extends State<_DeltaSliderDirectInputDialog> {
  final _textController = TextEditingController();
  double? _value;

  @override
  void initState() {
    super.initState();

    _textController.text = widget.initialValue.toStringAsFixed(6);
    _textController.selection = TextSelection(baseOffset: 0, extentOffset: _textController.text.length);

    _value = widget.initialValue;
  }

  @override
  void dispose() {
    _textController.dispose();

    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    const double buttonWidth = 110;

    final localizations = AppLocalizations.of(context)!;

    return Form(
      autovalidateMode: AutovalidateMode.onUserInteraction,
      child: ModalDialogCard(
        child: IntrinsicHeight(
          child: IntrinsicWidth(
            child: Column(
              children: [
                ModalDialogTitle(title: localizations.sliderKeyboardInputModalTitle(widget.label)),
                ModalDialogSection(
                  child: Row(children: [
                    Expanded(
                      child: SizedBox(
                        height: 36,
                        child: TextField(
                          autofocus: true,
                          maxLines: 1,
                          cursorWidth: 1,
                          controller: _textController,
                          keyboardAppearance: Brightness.dark,
                          style: Theme.of(context).textTheme.bodyMedium,
                          decoration: InputDecoration(hintText: '0'),
                          keyboardType: TextInputType.numberWithOptions(decimal: true),
                          inputFormatters: [TextInputFormatter.withFunction(_allowDouble)],
                          onEditingComplete: _applyValue,
                          onChanged: (text) => _value = _parseToDouble(text),
                        ),
                      ),
                    ),
                    if (widget.onReset != null)
                      Padding(
                        padding: const EdgeInsets.only(left: 8),
                        child: ResetValueButton(onPressed: _resetValues),
                      ),
                  ]),
                ),
                ModalDialogSection(
                  child: Row(
                    mainAxisAlignment: MainAxisAlignment.spaceAround,
                    children: [
                      EpicLozengeButton(
                        label: localizations.menuButtonCancel,
                        width: buttonWidth,
                        color: Colors.transparent,
                        onPressed: _closeModal,
                      ),
                      EpicLozengeButton(
                        label: localizations.menuButtonOK,
                        width: buttonWidth,
                        onPressed: _applyValue,
                      ),
                    ],
                  ),
                ),
              ],
            ),
          ),
        ),
      ),
    );
  }

  /// Close the modal.
  void _closeModal() {
    Navigator.of(context).pop();
  }

  /// Apply the newly entered value.
  void _applyValue() {
    if (_value != null) {
      widget.onValueSet(_value!);
      _closeModal();
    }
  }

  /// Reset the values.
  void _resetValues() {
    if (widget.onReset == null) {
      return;
    }

    widget.onReset!();
    _closeModal();
  }

  /// Text editing function to only allow text that can be parsed to a double or an empty string.
  TextEditingValue _allowDouble(TextEditingValue oldValue, TextEditingValue newValue) {
    if (newValue.text.length == 0) {
      return newValue;
    }

    if (_parseToDouble(newValue.text) == null) {
      return oldValue;
    }

    return newValue;
  }

  /// Try to convert the text value to a double.
  double? _parseToDouble(String text) {
    double? result = double.tryParse(text);

    if (result == null) {
      // Try prepending a 0 so that an empty string or fractional shorthand work
      result = double.tryParse('0$text');
    }

    return result;
  }
}
