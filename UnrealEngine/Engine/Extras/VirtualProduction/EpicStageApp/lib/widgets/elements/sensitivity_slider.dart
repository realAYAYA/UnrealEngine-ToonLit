// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:epic_common/theme.dart';
import 'package:epic_common/widgets.dart';
import 'package:flutter/material.dart';
import 'package:flutter_gen/gen_l10n/app_localizations.dart';
import 'package:provider/provider.dart';
import 'package:streaming_shared_preferences/streaming_shared_preferences.dart';

import '../../models/settings/delta_widget_settings.dart';
import 'delta_slider.dart';
import 'delta_widget_base.dart';
import 'dropdown_button.dart';

const double _minSensitivity = 0.2;
const double _maxSensitivity = 2.0;

/// Convert a sensitivity value (multiplier on base widget speed) to a display value (range of 1-10 for readability).
double _sensitivityToDisplayValue(double sensitivity) {
  return sensitivity / _minSensitivity;
}

/// Inverse of [_sensitivityToDisplayValue].
double _displayValueToSensitivity(double displayValue) {
  return displayValue * _minSensitivity;
}

/// Widget to control the sensitivity of all other delta sliders.
class DeltaWidgetSensitivitySlider extends StatelessWidget {
  const DeltaWidgetSensitivitySlider({Key? key}) : super(key: key);

  @override
  Widget build(BuildContext context) {
    final deltaSettings = Provider.of<DeltaWidgetSettings>(context, listen: false);

    return PreferenceBuilder(
      preference: deltaSettings.sensitivity,
      builder: (context, final double sensitivity) => DrivenDeltaSlider(
        min: _sensitivityToDisplayValue(_minSensitivity),
        max: _sensitivityToDisplayValue(_maxSensitivity),
        bIgnoreSensitivity: true,
        baseSensitivity: 1.0,
        label: AppLocalizations.of(context)!.sliderSpeed,
        values: [DeltaWidgetValueData(value: _sensitivityToDisplayValue(sensitivity))],
        onChanged: (List<double> values) => deltaSettings.sensitivity.setValue(
          (sensitivity + _displayValueToSensitivity(values.first)).clamp(_minSensitivity, _maxSensitivity),
        ),
        onReset: () => deltaSettings.sensitivity.setValue(1.0),
      ),
    );
  }
}

/// Button that opens the sensitivity slider menu.
class SensitivitySliderButton extends StatefulWidget {
  const SensitivitySliderButton({Key? key}) : super(key: key);

  @override
  State<SensitivitySliderButton> createState() => _SensitivitySliderButtonState();
}

class _SensitivitySliderButtonState extends State<SensitivitySliderButton> {
  @override
  Widget build(BuildContext context) {
    final labelStyle = Theme.of(context).textTheme.bodyMedium!.copyWith(
          color: UnrealColors.white,
        );

    return ModalDropdownButton(
      tooltipMessage: AppLocalizations.of(context)!.sliderSpeed,
      buttonBuilder: (context, state) => Material(
        color: Colors.transparent,
        child: Stack(
          alignment: Alignment.center,
          children: [
            EpicIconButton(
              iconPath: 'packages/epic_common/assets/icons/slider_speed.svg',
              bIsToggledOn: state != ModalDropdownButtonState.closed,
              bIsVisualOnly: true,
            ),
            // Sensitivity value overlay
            IgnorePointer(
              child: SizedBox(
                width: 24,
                height: 24,
                child: Align(
                  alignment: Alignment.bottomRight,
                  child: PreferenceBuilder(
                    preference: Provider.of<DeltaWidgetSettings>(context, listen: false).sensitivity,
                    builder: (context, double sensitivity) => Text(
                      _sensitivityToDisplayValue(sensitivity).toStringAsFixed(0),
                      style: labelStyle,
                    ),
                  ),
                ),
              ),
            ),
          ],
        ),
      ),
      menuBuilder: (context, originTabBuilder) => DropDownListMenu(originTabBuilder: originTabBuilder, children: [
        Material(
          color: Colors.transparent,
          child: Container(
            width: 300,
            padding: EdgeInsets.only(
              top: 8,
              bottom: 4,
            ),
            child: DeltaWidgetSensitivitySlider(),
          ),
        ),
      ]),
    );
  }
}
