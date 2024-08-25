// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:epic_common/preferences.dart';
import 'package:epic_common/theme.dart';
import 'package:epic_common/widgets.dart';
import 'package:flutter/material.dart';
import 'package:flutter_gen/gen_l10n/app_localizations.dart';
import 'package:provider/provider.dart';
import 'package:streaming_shared_preferences/streaming_shared_preferences.dart';

import '../../../../models/settings/base_color_tab_settings.dart';
import '../../../../models/settings/color_wheel_settings.dart';
import '../../../../models/unreal_types.dart';
import '../../../../utilities/unreal_utilities.dart';
import '../../../elements/color_grading_wheel.dart';
import '../../../elements/delta_slider.dart';
import '../../../elements/dropdown_text.dart';

/// Modes the base color tab can operate in.
enum BaseColorTabMode {
  /// Edit FColor properties in Unreal.
  color,

  /// Edit FVector color grading property groups in Unreal.
  colorGrading,

  /// Edit post-process volumes in Unreal.
  postProcess,
}

/// Property names for each of the highlight color ranges.
const Map<RangeLimitType, String> _highlightRangeLimitPropertyNames = {
  RangeLimitType.min: 'Min',
  RangeLimitType.max: 'Max',
};

/// Property names for each color grading range.
const Map<ColorGradingRange, String> _colorGradingRangePropertyNames = {
  ColorGradingRange.global: 'Global',
  ColorGradingRange.shadows: 'Shadows',
  ColorGradingRange.midtones: 'Midtones',
  ColorGradingRange.highlights: 'Highlights',
};

/// Settings to use for each color grading subproperty.
final Map<ColorGradingSubproperty, ColorGradingSubpropertyConfig> _colorGradingSubpropertyConfig = {
  ColorGradingSubproperty.gain: ColorGradingSubpropertyConfig(
    name: 'Gain',
    getDisplayName: (context) => AppLocalizations.of(context)!.colorGradingSubpropertyGain,
  ),
  ColorGradingSubproperty.gamma: ColorGradingSubpropertyConfig(
    name: 'Gamma',
    getDisplayName: (context) => AppLocalizations.of(context)!.colorGradingSubpropertyGamma,
  ),
  ColorGradingSubproperty.saturation: ColorGradingSubpropertyConfig(
    name: 'Saturation',
    getDisplayName: (context) => AppLocalizations.of(context)!.colorGradingSubpropertySaturation,
  ),
  ColorGradingSubproperty.contrast: ColorGradingSubpropertyConfig(
    name: 'Contrast',
    getDisplayName: (context) => AppLocalizations.of(context)!.colorGradingSubpropertyContrast,
  ),
  ColorGradingSubproperty.offset: ColorGradingSubpropertyConfig(
    name: 'Offset',
    getDisplayName: (context) => AppLocalizations.of(context)!.colorGradingSubpropertyOffset,
    minValue: -1.0,
    maxValue: 1.0,
    saturationExponent: 3.0,
  ),
};

/// Maximum height of the color wheel relative to the total height of the screen.
const double maxWheelRadiusFromScreenHeight = 0.35;

/// Get the localized display name corresponding to a color grading range.
String getColorGradingRangeDisplayName(ColorGradingRange range, BuildContext context) {
  final AppLocalizations localizations = AppLocalizations.of(context)!;

  switch (range) {
    case ColorGradingRange.global:
      return localizations.colorGradingRangeGlobal;

    case ColorGradingRange.shadows:
      return localizations.colorGradingRangeShadows;

    case ColorGradingRange.midtones:
      return localizations.colorGradingRangeMidtones;

    case ColorGradingRange.highlights:
      return localizations.colorGradingRangeHighlights;
  }
}

/// Get the configuration data corresponding to a color grading range.
ColorGradingSubpropertyConfig? getColorGradingSubpropertyConfig(ColorGradingSubproperty subproperty) {
  return _colorGradingSubpropertyConfig[subproperty];
}

/// Base widget for tabs that let users change color properties.
class BaseColorTab extends StatefulWidget {
  const BaseColorTab({
    Key? key,
    required this.colorProperties,
    required this.mode,
    required this.rightSideHeader,
    required this.rightSideContents,
    this.miscPropertyPrefix = '',
    this.bUseEnableProperties = false,
  }) : super(key: key);

  /// Which mode to operate in. [colorProperties] must match the appropriate type.
  final BaseColorTabMode mode;

  /// Properties for the color values to be controlled.
  final List<UnrealProperty> colorProperties;

  /// Header for the right side column of the tab.
  final Widget rightSideHeader;

  /// Contents of the right side column of the tab.
  final Widget rightSideContents;

  /// Prefix for miscellaneous properties of the color grading settings.
  final String miscPropertyPrefix;

  /// Whether to control the enable/override properties for each property in the tab.
  final bool bUseEnableProperties;

  @override
  State<BaseColorTab> createState() => _BaseColorTabState();
}

class _BaseColorTabState extends State<BaseColorTab> {
  late final BaseColorTabSettings _tabSettings;

  @override
  void initState() {
    super.initState();
    _tabSettings = BaseColorTabSettings(PreferencesBundle.of(context));
  }

  Widget build(BuildContext context) {
    final maxWheelRadius = MediaQuery.of(context).size.height * maxWheelRadiusFromScreenHeight;

    late final Widget colorWheel; // The widget used to control the color property.
    late final Widget miscPropertyControls; // Extra controls shown under the color wheel.

    switch (widget.mode) {
      case BaseColorTabMode.color:
        colorWheel = UnrealColorWheel(
          unrealProperties: widget.colorProperties,
          bShowAlpha: false,
          maxRadius: maxWheelRadius,
        );
        miscPropertyControls = const SizedBox();
        break;

      case BaseColorTabMode.colorGrading:
      case BaseColorTabMode.postProcess:
        colorWheel = _buildColorGradingWheel(maxWheelRadius);
        miscPropertyControls = _buildColorGradingMiscControlsWidget();
        break;

      default:
        throw 'Invalid actor type for top widget in details panel';
    }

    return Provider<BaseColorTabSettings>(
      create: (_) => _tabSettings,
      child: Padding(
        padding: const EdgeInsets.only(top: UnrealTheme.sectionMargin),
        child: Row(
          crossAxisAlignment: CrossAxisAlignment.end,
          children: [
            Expanded(
              child: Container(
                color: Theme.of(context).colorScheme.surface,
                child: Column(children: [
                  colorWheel,
                  Padding(
                    padding: EdgeInsets.symmetric(horizontal: UnrealTheme.cardMargin),
                    child: miscPropertyControls,
                  ),
                ]),
              ),
            ),

            const SizedBox(width: UnrealTheme.sectionMargin),

            // Other properties
            Expanded(
              child: Container(
                color: Theme.of(context).colorScheme.surface,
                child: Column(children: [
                  Container(
                    color: Theme.of(context).colorScheme.surfaceTint,
                    height: 52,
                    child: widget.rightSideHeader,
                  ),
                  Expanded(
                    child: Container(
                      padding: EdgeInsets.only(left: UnrealTheme.cardMargin, right: UnrealTheme.cardMargin, top: 16),
                      color: Theme.of(context).colorScheme.surface,
                      child: widget.rightSideContents,
                    ),
                  ),
                ]),
              ),
            ),
          ],
        ),
      ),
    );
  }

  /// Get the display name of a range type.
  String _getHighlightsRangeDisplayName(RangeLimitType rangeLimitType) {
    switch (rangeLimitType) {
      case RangeLimitType.max:
        return AppLocalizations.of(context)!.propertyColorGradingHighlightsMax;

      case RangeLimitType.min:
        return AppLocalizations.of(context)!.propertyColorGradingHighlightsMin;
    }
  }

  /// Get the property name of a range type.
  String? _getHighlightsRangePropertyName(RangeLimitType rangeLimitType) {
    String? rangeLimitName = _highlightRangeLimitPropertyNames[rangeLimitType];
    if (rangeLimitName == null) {
      return null;
    }

    return 'Highlights$rangeLimitName';
  }

  /// Build the wheel used to edit color grading values.
  Widget _buildColorGradingWheel(double maxRadius) {
    return PreferenceBuilder(
      preference: _tabSettings.colorGradingRange,
      builder: (context, ColorGradingRange colorGradingRange) {
        final String rangeName = _colorGradingRangePropertyNames[colorGradingRange]!;

        return PreferenceBuilder(
          preference: _tabSettings.colorGradingSubproperty,
          builder: (context, ColorGradingSubproperty colorGradingSubproperty) {
            final ColorGradingSubpropertyConfig subpropertyConfig =
                getColorGradingSubpropertyConfig(colorGradingSubproperty)!;

            late final List<UnrealProperty> subproperties;
            List<UnrealProperty>? enableProperties;

            switch (widget.mode) {
              case BaseColorTabMode.postProcess:
                // For post-processing, "Global" is implied by no range prefix
                final String rangePropertyName =
                    (_tabSettings.colorGradingRange.getValue() == ColorGradingRange.global) ? '' : rangeName;

                subproperties = getSubproperties(
                  widget.colorProperties,
                  'Color${subpropertyConfig.name}$rangePropertyName',
                  typeNameOverride: 'FVector4_Color',
                );

                if (widget.bUseEnableProperties) {
                  enableProperties = getSubproperties(
                    widget.colorProperties,
                    'bOverride_Color${subpropertyConfig.name}$rangePropertyName',
                  );
                }
                break;

              default:
                subproperties = getSubproperties(
                  widget.colorProperties,
                  '$rangeName.${subpropertyConfig.name}',
                  typeNameOverride: 'FVector4_Color',
                );

                if (widget.bUseEnableProperties) {
                  enableProperties = getSubproperties(
                    widget.colorProperties,
                    '$rangeName.bOverride_${subpropertyConfig.name}',
                  );
                }
                break;
            }

            return UnrealColorGradingWheel(
              unrealProperties: subproperties,
              enableProperties: enableProperties,
              minValue: subpropertyConfig.minValue,
              maxValue: subpropertyConfig.maxValue,
              saturationExponent: subpropertyConfig.saturationExponent,
              extraTopWidget: SizedBox(
                width: 140,
                child: DropdownSelector(
                  value: colorGradingSubproperty,
                  items: ColorGradingSubproperty.values,
                  makeItemName: (value) =>
                      (value != null
                          ? getColorGradingSubpropertyConfig(value as ColorGradingSubproperty)?.getDisplayName(context)
                          : null) ??
                      '???',
                  onChanged: (ColorGradingSubproperty value) => _tabSettings.colorGradingSubproperty.setValue(value),
                ),
              ),
              extraBottomWidget: const _ColorGradingRangeToggleButtons(),
              maxRadius: maxRadius,
            );
          },
        );
      },
    );
  }

  /// Build the widgets to show under the wheel when editing color grading values.
  Widget _buildColorGradingMiscControlsWidget() {
    return PreferenceBuilder(
      preference: _tabSettings.colorGradingRange,
      builder: (context, ColorGradingRange colorGradingRange) {
        final List<Widget> widgets = [];

        // Add widgets under the wheel based on the grading range
        switch (colorGradingRange) {
          case ColorGradingRange.shadows:
            widgets.add(UnrealDeltaSlider(
              overrideName: AppLocalizations.of(context)!.propertyColorGradingShadowsMax,
              unrealProperties: getSubproperties(
                widget.colorProperties,
                '${widget.miscPropertyPrefix}ShadowsMax',
              ),
              enableProperties: widget.bUseEnableProperties
                  ? getSubproperties(
                      widget.colorProperties,
                      'bOverride_${widget.miscPropertyPrefix}ShadowsMax',
                    )
                  : null,
            ));
            break;

          case ColorGradingRange.highlights:
            widgets.add(
              PreferenceBuilder(
                preference: _tabSettings.colorGradingHighlightsRangeLimitType,
                builder: (context, final RangeLimitType highlightsRangeLimitType) {
                  final String? highlightsPropertyName = _getHighlightsRangePropertyName(highlightsRangeLimitType);

                  if (highlightsPropertyName == null) {
                    return const SizedBox();
                  }

                  return UnrealDeltaSlider(
                    overrideName: _getHighlightsRangeDisplayName(highlightsRangeLimitType),
                    buildLabel: (String name) => DropdownText(
                      value: highlightsRangeLimitType,
                      items: RangeLimitType.values,
                      makeItemName: (RangeLimitType item) => _getHighlightsRangeDisplayName(item),
                      onChanged: (RangeLimitType value) =>
                          _tabSettings.colorGradingHighlightsRangeLimitType.setValue(value),
                    ),
                    unrealProperties: getSubproperties(
                      widget.colorProperties,
                      '${widget.miscPropertyPrefix}$highlightsPropertyName',
                    ),
                    enableProperties: widget.bUseEnableProperties
                        ? getSubproperties(
                            widget.colorProperties,
                            'bOverride_${widget.miscPropertyPrefix}$highlightsPropertyName',
                          )
                        : null,
                  );
                },
              ),
            );
            break;

          default:
            break;
        }

        return Column(children: widgets);
      },
    );
  }
}

/// Toggle button selector for the color grading range to control.
class _ColorGradingRangeToggleButtons extends StatelessWidget {
  const _ColorGradingRangeToggleButtons({Key? key}) : super(key: key);

  @override
  Widget build(BuildContext context) {
    final BaseColorTabSettings colorTabSettings = Provider.of<BaseColorTabSettings>(context, listen: false);

    return PreferenceBuilder(
      preference: Provider.of<BaseColorTabSettings>(context, listen: false).colorGradingRange,
      builder: (context, ColorGradingRange colorGradingRange) => SelectorBar(
        value: colorGradingRange,
        onSelected: (newValue) => colorTabSettings.colorGradingRange.setValue(newValue as ColorGradingRange),
        valueNames: {
          for (final range in ColorGradingRange.values) range: getColorGradingRangeDisplayName(range, context),
        },
      ),
    );
  }
}
