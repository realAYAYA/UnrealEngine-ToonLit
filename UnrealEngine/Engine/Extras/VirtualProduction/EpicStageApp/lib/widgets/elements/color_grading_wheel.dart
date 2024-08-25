// Copyright Epic Games, Inc. All Rights Reserved.

import 'dart:async';
import 'dart:math' as math;

import 'package:async/async.dart';
import 'package:epic_common/preferences.dart';
import 'package:epic_common/theme.dart';
import 'package:epic_common/widgets.dart';
import 'package:flutter/material.dart';
import 'package:flutter_gen/gen_l10n/app_localizations.dart';
import 'package:provider/provider.dart';

import '../../../models/color.dart';
import '../../../models/property_modify_operations.dart';
import '../../../utilities/math_utilities.dart';
import '../../models/settings/color_wheel_settings.dart';
import '../../models/settings/delta_widget_settings.dart';
import '../../utilities/guarded_refresh_state.dart';
import '../../utilities/unreal_utilities.dart';
import 'color_slider.dart';
import 'delta_slider.dart';
import 'delta_widget_base.dart';
import 'unreal_widget_base.dart';

/// Diameter of a dot on the color wheel indicating a value.
const double _dotDiameter = 22.0;

/// Fraction of the available horizontal space that the wheel should attempt to take up with its diameter.
const double _wheelWidthFraction = 0.75;

/// Maximum diameter of the color wheel regardless of available space.
const double _maxWheelDiameter = 320;

/// Minimum screen height to display all the sliders under the color grading wheel.
const double _minScreenHeightForFullSize = 800;

typedef ColorWheelValueData = DeltaWidgetValueData<WheelColor>;

/// How to display the alpha value of a color.
enum AlphaDisplayMode {
  /// Don't let the user modify the alpha value at all.
  hide,

  /// Show the alpha value as an alpha slider.
  alpha,

  /// Show the alpha value as a luminance slider.
  luminance,
}

/// Configuration data indicating how to display and control a color grading subproperty.
class ColorGradingSubpropertyConfig {
  const ColorGradingSubpropertyConfig({
    required this.name,
    required this.getDisplayName,
    this.minValue = 0.0,
    this.maxValue = 2.0,
    this.saturationExponent = 2.4,
  });

  /// The name of the subproperty to control.
  final String name;

  /// A function returning the localized display name of the property.
  final String Function(BuildContext context) getDisplayName;

  /// The minimum value of the color's components.
  final double minValue;

  /// The maximum value of the color's components.
  final double maxValue;

  /// Exponent applied to the saturation component of the wheel, making it less sensitive towards the center.
  final double saturationExponent;
}

/// A delta-based color wheel widget that controls a LinearColor property remotely in Unreal Engine.
class UnrealColorWheel extends UnrealWidget {
  const UnrealColorWheel({
    super.key,
    super.overrideName,
    required super.unrealProperties,
    this.extraTopWidget,
    this.extraBottomWidget,
    this.bShowAlpha = true,
    this.bShowValueWithWheel = false,
    this.maxRadius,
  });

  /// An extra widget to show next to the RGB/HSV and wheel/slider toggles.
  final Widget? extraTopWidget;

  /// An extra widget to show under the wheel, but above the value slider.
  final Widget? extraBottomWidget;

  /// If false, hide the alpha slider.
  final bool bShowAlpha;

  /// If false, hide the value slider when in wheel mode.
  final bool bShowValueWithWheel;

  /// If provided, don't let the wheel grow larger than this.
  final double? maxRadius;

  @override
  _UnrealColorWheelState createState() => _UnrealColorWheelState();
}

class _UnrealColorWheelState extends State<UnrealColorWheel>
    with UnrealWidgetStateMixin<UnrealColorWheel, WheelColor>, _UnrealRgbaComponentSliderCreator<UnrealColorWheel> {
  @override
  List<String> get rgbaComponentNames => const ['R', 'G', 'B', 'A'];

  @override
  double? get minComponentValue => null;

  @override
  double? get maxComponentValue => null;

  @override
  PropertyModifyOperation get modifyOperation => const WheelColorAddOperation();

  @override
  Widget build(BuildContext context) {
    return TransientPreferenceBuilder(
      preference: Provider.of<DeltaWidgetSettings>(context, listen: false).bIsInResetMode,
      builder: (context, final bool bIsInResetMode) => DrivenColorWheel(
        wheelValues: makeValueDataList(),
        onValueChanged: handleOnChangedByUser,
        onReset: handleOnResetByUser,
        onInteractionFinished: endTransaction,
        extraHeaderWidget: widget.extraTopWidget,
        extraTopWidget: widget.extraBottomWidget,
        alphaMode: widget.bShowAlpha ? AlphaDisplayMode.alpha : AlphaDisplayMode.hide,
        createRgbaSlider: createComponentSlider,
        maxRadius: widget.maxRadius,
        bIsEnabled: !bIsInResetMode,
      ),
    );
  }
}

/// A delta-based color grading wheel widget that controls a Vector4 color property remotely in Unreal Engine.
class UnrealColorGradingWheel extends UnrealWidget {
  const UnrealColorGradingWheel({
    super.key,
    super.overrideName,
    super.enableProperties,
    required super.unrealProperties,
    this.extraTopWidget,
    this.extraBottomWidget,
    this.saturationExponent = 2.4,
    this.minValue = 0.0,
    this.maxValue = 2.0,
    this.bShowLuminance = true,
    this.bShowValueWithWheel = false,
    this.maxRadius,
  });

  /// An extra widget to show next to the RGB/HSV and wheel/slider toggles.
  final Widget? extraTopWidget;

  /// An extra widget to show under the wheel, but above the value slider.
  final Widget? extraBottomWidget;

  /// Exponent applied to the saturation component of the wheel, making it less sensitive towards the center.
  final double saturationExponent;

  /// The minimum value of the color's components. The value will be clamped in this range and scaled based on the
  /// min/max range when received in the engine.
  final double minValue;

  /// The maximum value of the color's components. The value will be clamped in this range and scaled based on the
  /// min/max range when received in the engine.
  final double maxValue;

  /// If false, hide the value slider.
  final bool bShowLuminance;

  /// If false, hide the value slider when in wheel mode.
  final bool bShowValueWithWheel;

  /// If provided, don't let the wheel grow larger than this.
  final double? maxRadius;

  @override
  _UnrealColorGradingWheelState createState() => _UnrealColorGradingWheelState();
}

class _UnrealColorGradingWheelState extends State<UnrealColorGradingWheel>
    with
        UnrealWidgetStateMixin<UnrealColorGradingWheel, WheelColor>,
        _UnrealRgbaComponentSliderCreator<UnrealColorGradingWheel> {
  @override
  List<String> get rgbaComponentNames => const ['X', 'Y', 'Z', 'W'];

  @override
  double? get minComponentValue => widget.minValue;

  @override
  double? get maxComponentValue => widget.maxValue;

  @override
  PropertyModifyOperation get modifyOperation => WheelColorGradingAddOperation(
        saturationExponent: widget.saturationExponent,
        minValue: widget.minValue,
        maxValue: widget.maxValue,
      );

  @override
  Map<String, dynamic>? get conversionMetadata => {
        'minValue': widget.minValue,
        'maxValue': widget.maxValue,
      };

  @override
  Widget build(BuildContext context) {
    return TransientPreferenceBuilder(
      preference: Provider.of<DeltaWidgetSettings>(context, listen: false).bIsInResetMode,
      builder: (context, final bool bIsInResetMode) => DrivenColorWheel(
        wheelValues: makeValueDataList(),
        onValueChanged: handleOnChangedByUser,
        onReset: handleOnResetByUser,
        onInteractionFinished: endTransaction,
        extraHeaderWidget: widget.extraTopWidget,
        extraTopWidget: widget.extraBottomWidget,
        saturationExponent: widget.saturationExponent,
        minValue: widget.minValue,
        maxValue: widget.maxValue,
        alphaMode: widget.bShowLuminance ? AlphaDisplayMode.luminance : AlphaDisplayMode.hide,
        createRgbaSlider: createComponentSlider,
        maxRadius: widget.maxRadius,
        bIsEnabled: !bIsInResetMode,
      ),
    );
  }
}

mixin _UnrealRgbaComponentSliderCreator<WidgetType extends UnrealWidget> on State<WidgetType> {
  /// Fill colors for the sliders corresponding to each RGBA color component.
  static const List<Color?> _rgbaComponentFillColors = [
    Color(0xffff0000),
    Color(0xff00ff00),
    Color(0xff0000ff),
    null,
  ];

  /// The names of the 4 subproperties used to access color components.
  List<String> get rgbaComponentNames;

  /// The minimum value of a component slider.
  double? get minComponentValue;

  /// The maximum value of a component slider.
  double? get maxComponentValue;

  /// Create a slider that directly controls color component values. This lets us avoid converting from a
  /// Vector4/LinearColor to a WheelColor and back (which causes precision loss) and instead directly accessing the
  /// color's components by tracking subproperties of the color properties.
  Widget createComponentSlider(int componentIndex, String componentLabel) {
    return UnrealDeltaSlider(
      unrealProperties: getSubproperties(widget.unrealProperties, rgbaComponentNames[componentIndex]),
      hardMin: minComponentValue,
      hardMax: maxComponentValue,
      bShowResetButton: false,
      overrideName: componentLabel,
      enableProperties: widget.enableProperties,
      fillColor: _rgbaComponentFillColors[componentIndex],
    );
  }
}

/// A color wheel which supports editing multiple objects and uses delta-based modification.
/// Its values are controlled from outside of the widget itself.
class DrivenColorWheel extends StatefulWidget {
  const DrivenColorWheel({
    Key? key,
    required this.wheelValues,
    required this.onValueChanged,
    this.extraHeaderWidget,
    this.extraTopWidget,
    this.onReset,
    this.onInteractionFinished,
    this.saturationExponent = 1.0,
    this.minValue = 0.0,
    this.maxValue = 1.0,
    this.alphaMode = AlphaDisplayMode.alpha,
    this.createRgbaSlider,
    this.maxRadius,
    this.bIsEnabled = true,
  }) : super(key: key);

  /// An extra widget to show next to the RGB/HSV and wheel/slider toggles in the header.
  final Widget? extraHeaderWidget;

  /// An extra widget to show under the header, but above the wheel.
  final Widget? extraTopWidget;

  /// List of wheel colors for each property this widget is controlling.
  final List<ColorWheelValueData?> wheelValues;

  /// Function called when the user changes the color values.
  /// Passes the amount by which each value changed. Note that color values may be negative to indicate a decrease.
  final void Function(List<WheelColor>) onValueChanged;

  /// Function called when the user is done interacting with the widget.
  final void Function()? onInteractionFinished;

  /// Function called when the reset button is pressed. Called after the value has been reset.
  final void Function()? onReset;

  /// Exponent applied to the saturation component of the wheel, making it less sensitive towards the center.
  final double saturationExponent;

  /// Minimum value of color's value range.
  final double minValue;

  /// Maximum value of color's value range.
  final double maxValue;

  /// How to display the color's alpha value to the user.
  final AlphaDisplayMode alphaMode;

  /// An optional function used to create the RGBA component slider widgets. If provided, changes to the sliders will
  /// not result in [onValueChanged] callbacks, so they are responsible for reporting any changes.
  /// [componentIndex] is an index into [rgbaSubpropertyNames] and [componentLabel] is the label to display on the
  /// widget.
  final Widget Function(int componentIndex, String componentLabel)? createRgbaSlider;

  /// If provided, don't let the wheel grow larger than this.
  final double? maxRadius;

  /// If false, grey out the sliders and don't accept inputs. Note that this doesn't affect the reset button.
  final bool bIsEnabled;

  @override
  _DrivenColorWheelState createState() => _DrivenColorWheelState();
}

class _DrivenColorWheelState extends State<DrivenColorWheel> with DeltaWidgetStateMixin, GuardedRefreshState {
  late final ColorWheelSettings _settings;
  StreamSubscription? _settingsSubscription;

  /// Labels to show for the first three components of an RGB value.
  List<String> get _rgbComponentLabels {
    final localizations = AppLocalizations.of(context)!;
    return [
      localizations.colorComponentRed,
      localizations.colorComponentGreen,
      localizations.colorComponentBlue,
    ];
  }

  @override
  void initState() {
    super.initState();

    _settings = ColorWheelSettings(PreferencesBundle.of(context));

    // The entire widget needs to redraw when any of these settings change, so handle them as a single subscription
    // instead of PreferenceBuilders.
    _settingsSubscription = StreamGroup.merge([
      _settings.colorMode,
      _settings.editMode,
    ]).listen(refreshOnData);
  }

  @override
  void dispose() {
    _settingsSubscription?.cancel();

    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return LayoutBuilder(builder: (BuildContext context, BoxConstraints constraints) {
      final localizations = AppLocalizations.of(context)!;
      final bIsInResetModePref = Provider.of<DeltaWidgetSettings>(context, listen: false).bIsInResetMode;

      final editMode = _settings.editMode.getValue();
      final colorMode = _settings.colorMode.getValue();

      final bool isFullHeight = MediaQuery.of(context).size.height > _minScreenHeightForFullSize;

      final List<Widget> sliders = []; // List of sliders to display
      Widget? colorWheel = null; // The color wheel (if it will be displayed)

      late final Widget mainBody; // The main content of the widget (either a wheel or color component sliders)
      late final List<Widget> otherWidgets = []; // Any other widgets we need to display under the main body

      final double wheelDiameter = _getWheelDiameter(constraints);

      // Create slider widgets, used both in full-height or slider-only modes
      if (editMode == ColorWheelEditMode.sliders || isFullHeight) {
        switch (colorMode) {
          case ColorWheelColorMode.rgb:
            if (widget.createRgbaSlider != null) {
              // Create custom widgets for the first 3 components (R/G/B)
              for (int componentIndex = 0; componentIndex < 3; ++componentIndex) {
                sliders.add(widget.createRgbaSlider!(componentIndex, _rgbComponentLabels[componentIndex]));
              }
            } else {
              // Use sliders that convert to components from the wheel value
              sliders.addAll([
                DrivenColorRedSlider(
                  key: Key('Red'),
                  values: widget.wheelValues,
                  onChanged: widget.onValueChanged,
                  onInteractionFinished: widget.onInteractionFinished,
                  min: widget.minValue,
                  max: widget.maxValue,
                  bIsEnabled: widget.bIsEnabled,
                ),
                DrivenColorGreenSlider(
                  key: Key('Green'),
                  values: widget.wheelValues,
                  onChanged: widget.onValueChanged,
                  onInteractionFinished: widget.onInteractionFinished,
                  min: widget.minValue,
                  max: widget.maxValue,
                  bIsEnabled: widget.bIsEnabled,
                ),
                DrivenColorBlueSlider(
                  key: Key('Blue'),
                  values: widget.wheelValues,
                  onChanged: widget.onValueChanged,
                  onInteractionFinished: widget.onInteractionFinished,
                  min: widget.minValue,
                  max: widget.maxValue,
                  bIsEnabled: widget.bIsEnabled,
                ),
              ]);
            }

            break;

          case ColorWheelColorMode.hsv:
            sliders.addAll([
              DrivenColorHueSlider(
                key: Key('Hue'),
                values: widget.wheelValues,
                onChanged: widget.onValueChanged,
                onInteractionFinished: widget.onInteractionFinished,
                min: widget.minValue,
                max: widget.maxValue,
                bIsEnabled: widget.bIsEnabled,
              ),
              DrivenColorSaturationSlider(
                key: Key('Saturation'),
                values: widget.wheelValues,
                onChanged: widget.onValueChanged,
                onInteractionFinished: widget.onInteractionFinished,
                min: widget.minValue,
                max: widget.maxValue,
                bIsEnabled: widget.bIsEnabled,
              ),
              DrivenColorValueSlider(
                key: Key('Value'),
                values: widget.wheelValues,
                onChanged: widget.onValueChanged,
                onInteractionFinished: widget.onInteractionFinished,
                min: widget.minValue,
                max: widget.maxValue,
                bIsEnabled: widget.bIsEnabled,
              ),
            ]);
            break;
        }

        // Add alpha/luminance slider
        final Widget? alphaSlider = _makeAlphaSlider();
        if (alphaSlider != null) {
          sliders.add(alphaSlider);
        }
      }

      // Create color wheel, used in full-height or wheel-only mode
      if (editMode == ColorWheelEditMode.wheel || isFullHeight) {
        colorWheel = _ColorWheelWheel(
          wheelSize: wheelDiameter,
          uiValues: widget.wheelValues,
          saturationExponent: widget.saturationExponent,
          onPanUpdate: _onWheelDragUpdate,
          onPanEnd: (_) => widget.onInteractionFinished?.call(),
          onPanCancel: () => widget.onInteractionFinished?.call(),
        );
      }

      if (isFullHeight) {
        mainBody = colorWheel!;
      } else {
        switch (editMode) {
          case ColorWheelEditMode.sliders:
            mainBody = TransientPreferenceBuilder(
              preference: bIsInResetModePref,
              builder: (context, final bool bIsInResetMode) => Padding(
                // Extra padding at top to fit reset button
                padding: EdgeInsets.only(top: bIsInResetMode ? 48 : 8),
                child: Column(
                  mainAxisAlignment: MainAxisAlignment.start,
                  children: sliders,
                ),
              ),
            );
            break;

          case ColorWheelEditMode.wheel:
            mainBody = colorWheel!;

            // Add value and luminance sliders under the wheel, since we're hiding sliders and those components can't
            // be controlled by a wheel alone
            otherWidgets.addAll([
              DrivenDeltaSlider(
                label: AppLocalizations.of(context)!.colorComponentValue,
                defaultValue: (widget.minValue + widget.maxValue) / 2,
                min: widget.minValue,
                max: widget.maxValue,
                onChanged: _handleValueSliderChanged,
                onInteractionFinished: widget.onInteractionFinished,
                values: widget.wheelValues
                    .map((ColorWheelValueData? valueData) =>
                        valueData != null ? DeltaSliderValueData(value: valueData.value.value) : null)
                    .toList(),
                bShowResetButton: false,
                bIsEnabled: widget.bIsEnabled,
              ),
              if (widget.alphaMode == AlphaDisplayMode.luminance) _makeAlphaSlider()!,
            ]);

            break;
        }
      }

      return Column(children: [
        Container(
          height: 52,
          color: Theme.of(context).colorScheme.surfaceTint,
          padding: EdgeInsets.symmetric(horizontal: 16),
          child:
              // Header controls
              Row(
            mainAxisAlignment: MainAxisAlignment.start,
            children: [
              if (widget.extraHeaderWidget != null) widget.extraHeaderWidget!,

              const Spacer(),

              // Color mode toggle
              SelectorBar(
                value: colorMode,
                onSelected: _onColorModeChangedByUser,
                valueNames: {
                  ColorWheelColorMode.rgb: localizations.colorWheelColorModeRGB,
                  ColorWheelColorMode.hsv: localizations.colorWheelColorModeHSV,
                },
              ),

              if (!isFullHeight)
                // Edit mode toggle
                Padding(
                  padding: EdgeInsets.only(left: 16),
                  child: SizedBox(
                    width: 36,
                    height: 36,
                    child: Tooltip(
                      message: editMode == ColorWheelEditMode.wheel
                          ? localizations.colorWheelShowSliders
                          : localizations.colorWheelShowWheel,
                      child: IconButton(
                        onPressed: _toggleEditMode,
                        color: Colors.transparent,
                        icon: AssetIcon(
                          size: 20,
                          path: editMode == ColorWheelEditMode.wheel
                              ? 'packages/epic_common/assets/icons/sliders.svg'
                              : 'packages/epic_common/assets/icons/color_wheel.svg',
                        ),
                      ),
                    ),
                  ),
                ),
            ],
          ),
        ),
        SizedBox(height: 8),
        if (widget.extraTopWidget != null)
          Padding(
            padding: EdgeInsets.only(top: 8, bottom: 8),
            child: widget.extraTopWidget!,
          ),
        Padding(
          padding: EdgeInsets.symmetric(horizontal: UnrealTheme.cardMargin),
          child: Column(
            children: [
              // Color wheel
              Stack(children: [
                mainBody,

                // Reset button
                TransientPreferenceBuilder(
                  preference: bIsInResetModePref,
                  builder: (context, final bool bIsInResetMode) => bIsInResetMode
                      ? Align(
                          alignment: Alignment.topRight,
                          child: ResetValueButton(onPressed: widget.onReset),
                        )
                      : const SizedBox(),
                ),
              ]),

              if (isFullHeight)
                for (Widget slider in sliders) slider,

              if (otherWidgets.length > 0) Column(children: otherWidgets),
            ],
          ),
        ),
      ]);
    });
  }

  /// Make the alpha or luminance slider for the controlled color.
  Widget? _makeAlphaSlider() {
    final localizations = AppLocalizations.of(context)!;
    if (widget.alphaMode != AlphaDisplayMode.hide) {
      final String alphaLabel = (widget.alphaMode == AlphaDisplayMode.luminance)
          ? localizations.colorComponentLuminance
          : localizations.colorComponentAlpha;

      if (widget.createRgbaSlider != null) {
        return widget.createRgbaSlider!(3, alphaLabel);
      } else {
        return DrivenColorAlphaSlider(
          key: Key('Alpha'),
          label: alphaLabel,
          values: widget.wheelValues,
          onChanged: widget.onValueChanged,
          onInteractionFinished: widget.onInteractionFinished,
          min: widget.minValue,
          max: widget.maxValue,
        );
      }
    }

    return null;
  }

  /// Called when the color wheel portion of the widget is dragged.
  void _onWheelDragUpdate(DragUpdateDetails details) {
    if (!widget.bIsEnabled) {
      return;
    }

    final renderBox = context.findRenderObject() as RenderBox?;
    if (renderBox == null) {
      return;
    }

    // Color value below which the color is effectively black
    const double lowValue = 0.00001;

    final Offset scaledDeltaOffset = details.delta * deltaMultiplier / _getWheelDiameter(renderBox.constraints);
    final List<WheelColor> deltaColors = widget.wheelValues
        .map((prevColor) => WheelColor(
              scaledDeltaOffset,
              // If the color's value is nearly 0, force it to 1 since otherwise the user won't see a color change.
              // This imitates Unreal Editor's color editing behavior.
              ((prevColor?.value.value ?? 0) < lowValue) ? 1 : 0,
              0,
            ))
        .toList(growable: false);
    widget.onValueChanged(deltaColors);
  }

  /// Called when the user changes the value of the value slider.
  void _handleValueSliderChanged(List<double> deltaValues) {
    assert(deltaValues.length == widget.wheelValues.length);

    widget.onValueChanged(
      deltaValues
          .map(
            (delta) => WheelColor(Offset.zero, delta, 0),
          )
          .toList(growable: false),
    );
  }

  /// Toggle between wheel and slider edit modes.
  void _toggleEditMode() {
    final ColorWheelEditMode lastEditMode = _settings.editMode.getValue();

    _settings.editMode.setValue(
      lastEditMode == ColorWheelEditMode.wheel ? ColorWheelEditMode.sliders : ColorWheelEditMode.wheel,
    );
  }

  /// Called when the user changes the color mode.
  void _onColorModeChangedByUser(ColorWheelColorMode newColorMode) {
    _settings.colorMode.setValue(newColorMode);
  }

  /// Return the largest wheel diameter that fits into the widget's constraints.
  double _getWheelDiameter(BoxConstraints constraints) {
    final double targetRadius =
        math.min(math.min(constraints.maxHeight, constraints.maxWidth * _wheelWidthFraction), _maxWheelDiameter);

    if (widget.maxRadius != null) {
      return math.min(targetRadius, widget.maxRadius!);
    }

    return targetRadius;
  }
}

/// The wheel portion of the color wheel widget, which maps hue and saturation onto a plane.
class _ColorWheelWheel extends StatelessWidget {
  const _ColorWheelWheel({
    required this.wheelSize,
    required this.uiValues,
    this.onPanUpdate,
    this.onPanEnd,
    this.onPanCancel,
    required this.saturationExponent,
    Key? key,
  }) : super(key: key);

  final double wheelSize;
  final Function(DragUpdateDetails)? onPanUpdate;
  final Function(DragEndDetails)? onPanEnd;
  final Function()? onPanCancel;
  final List<ColorWheelValueData?> uiValues;
  final double saturationExponent;

  @override
  Widget build(BuildContext context) {
    // Create the widgets for each of the dots on the wheel
    final List<_ColorWheelValueDot> dotWidgets = [];

    for (int i = 0; i < uiValues.length; ++i) {
      if (uiValues[i] != null) {
        dotWidgets.add(_ColorWheelValueDot(
          valueData: uiValues[i]!,
          saturationExponent: saturationExponent,
          label: (uiValues.length > 1) ? (i + 1).toString() : '',
        ));
      }
    }

    final double wheelAndDotSize = wheelSize + _dotDiameter;

    // Use a clipper so we can ignore touches outside of the circle
    return Center(
      child: SizedBox(
        width: wheelAndDotSize,
        height: wheelAndDotSize,
        child: Stack(
          alignment: Alignment.center,
          children: [
            // Color wheel
            SizedBox(
              width: wheelSize,
              height: wheelSize,
              child: CustomPaint(
                painter: _ColorWheelPainter(context),
              ),
            ),

            // Value dots
            Stack(children: dotWidgets),

            // Gesture detector clipped to circle shape
            SizedBox(
              width: wheelAndDotSize,
              height: wheelAndDotSize,
              child: ClipOval(
                clipBehavior: Clip.hardEdge,
                clipper: _CenteredSquareClipper(),
                child: GestureDetector(
                  onPanUpdate: onPanUpdate,
                  onPanEnd: onPanEnd,
                  onPanCancel: onPanCancel,
                  behavior: HitTestBehavior.translucent,
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }
}

/// Dot shown on the color wheel to indicate the value of properties being edited.
class _ColorWheelValueDot extends StatelessWidget {
  const _ColorWheelValueDot({
    required this.valueData,
    required this.saturationExponent,
    this.label = '',
    Key? key,
  }) : super(key: key);

  final String label;
  final ColorWheelValueData valueData;
  final double saturationExponent;

  @override
  Widget build(BuildContext context) {
    /// Apply the inverse of the exponent to convert our linear position to the correct position on the circle
    final Offset position = exponentiateCirclePosition(
      valueData.value.position,
      saturationExponent,
      true,
    );

    return Align(
      alignment: Alignment(position.dx, position.dy),
      child: Container(
        width: _dotDiameter,
        height: _dotDiameter,
        child: Stack(
          children: [
            SizedBox(
              width: _dotDiameter,
              height: _dotDiameter,
              child: CustomPaint(
                painter: _ColorWheelDotPainter(context),
              ),
            ),
          ],
        ),
      ),
    );
  }
}

/// Painter for the wheel image of the color wheel.
class _ColorWheelPainter extends CustomPainter {
  const _ColorWheelPainter(this.context) : super();

  final BuildContext context;

  /// Paint the wheel widget.
  @override
  void paint(Canvas canvas, Size size) {
    final Rect canvasRect = Offset.zero & size;
    final double radius = math.min(size.width, size.height) / 2;
    final Offset center = canvasRect.center;

    const rainbowSweepGradient = SweepGradient(colors: [
      Color.fromARGB(255, 255, 0, 0),
      Color.fromARGB(255, 255, 255, 0),
      Color.fromARGB(255, 0, 255, 0),
      Color.fromARGB(255, 0, 255, 255),
      Color.fromARGB(255, 0, 0, 255),
      Color.fromARGB(255, 255, 0, 255),
      Color.fromARGB(255, 255, 0, 0),
    ]);

    const whiteRadialGradient = RadialGradient(colors: [
      Color.fromARGB(255, 255, 255, 255),
      Color.fromARGB(0, 255, 255, 255),
    ]);

    final Paint rainbowSweepPaint = Paint()
      ..shader = rainbowSweepGradient.createShader(canvasRect)
      ..style = PaintingStyle.fill;

    final Paint whiteRadialPaint = Paint()
      ..shader = whiteRadialGradient.createShader(canvasRect)
      ..style = PaintingStyle.fill;

    canvas.drawCircle(center, radius, rainbowSweepPaint);
    canvas.drawCircle(center, radius, whiteRadialPaint);
  }

  /// Only repaint if the widget has been replaced.
  @override
  bool shouldRepaint(covariant CustomPainter oldDelegate) {
    return oldDelegate != this;
  }
}

/// Painter for the value dots indicating a value being edited in the color wheel.
class _ColorWheelDotPainter extends CustomPainter {
  const _ColorWheelDotPainter(this.context) : super();

  final BuildContext context;

  @override
  void paint(Canvas canvas, Size size) {
    const double ringWidth = 1;

    final Rect canvasRect = Offset.zero & size;

    // Draw the ring
    final Paint ringPaint = Paint()
      ..style = PaintingStyle.stroke
      ..strokeWidth = ringWidth
      ..color = UnrealColors.black;

    final Rect ringRect = canvasRect.inflate(-ringWidth / 2);
    canvas.drawArc(ringRect, 0, math.pi * 2, false, ringPaint);
  }

  @override
  bool shouldRepaint(covariant CustomPainter oldDelegate) {
    return oldDelegate != this;
  }
}

/// A clipper that returns a square in the center of the canvas area.
class _CenteredSquareClipper extends CustomClipper<Rect> {
  @override
  getClip(Size size) {
    final double squareSize = math.min(size.width, size.height);
    return Rect.fromCenter(center: size.center(Offset.zero), width: squareSize, height: squareSize);
  }

  @override
  bool shouldReclip(covariant CustomClipper<Rect> oldClipper) {
    return this != oldClipper;
  }
}
