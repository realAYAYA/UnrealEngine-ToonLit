// Copyright Epic Games, Inc. All Rights Reserved.

import 'dart:ui';

import 'package:epic_common/preferences.dart';
import 'package:epic_common/theme.dart';
import 'package:flutter/material.dart';
import 'package:flutter_gen/gen_l10n/app_localizations.dart';
import 'package:provider/provider.dart';

import '../../models/settings/delta_widget_settings.dart';
import 'delta_widget_base.dart';
import 'unreal_widget_base.dart';

/// Configuration for a step button on an [UnrealStepper] widget.
class StepperStepConfig {
  const StepperStepConfig({
    required this.stepSize,
    required this.label,
    this.color,
    this.textColor,
  });

  /// Delta value to apply for this step.
  final double stepSize;

  /// Label to show on the step button.
  final String label;

  /// Color of the button's background.
  final Color? color;

  /// Color of the text in the button.
  final Color? textColor;

  /// Step configs for an exposure stepper.
  static List<StepperStepConfig> exposureSteps = [
    StepperStepConfig(stepSize: -1, label: '-1', color: Color(0xff1f1f1f)),
    StepperStepConfig(stepSize: -0.5, label: '-1/2', color: Color(0xff363636)),
    StepperStepConfig(stepSize: -0.25, label: '-1/4', color: Color(0xff4a4a4a)),
    StepperStepConfig(stepSize: 0.25, label: '+1/4', color: Color(0xff7a7a7a), textColor: UnrealColors.gray06),
    StepperStepConfig(stepSize: 0.5, label: '+1/2', color: Color(0xff979797), textColor: UnrealColors.gray06),
    StepperStepConfig(stepSize: 1, label: '+1', color: Color(0xffc0c0c0), textColor: UnrealColors.gray06),
  ];
}

/// A stepper that remotely increments/decrements a property in Unreal Engine.
class UnrealStepper extends UnrealWidget {
  const UnrealStepper({
    Key? key,
    super.overrideName,
    super.minMaxBehaviour,
    super.enableProperties,
    required super.unrealProperties,
    required this.steps,
  }) : super(key: key);

  /// Configurations for each button to display in this widget.
  final List<StepperStepConfig> steps;

  @override
  State<StatefulWidget> createState() => _UnrealStepperState();
}

class _UnrealStepperState extends State<UnrealStepper> with UnrealWidgetStateMixin<UnrealStepper, double> {
  @override
  Widget build(BuildContext context) {
    final List<Widget> children = [];

    for (int stepIndex = 0; stepIndex < widget.steps.length; ++stepIndex) {
      final bool bIsLast = stepIndex == widget.steps.length - 1;

      final StepperStepConfig config = widget.steps[stepIndex];
      children.add(Expanded(
        child: TransientPreferenceBuilder(
          preference: Provider.of<DeltaWidgetSettings>(context, listen: false).bIsInResetMode,
          builder: (context, final bool bIsInResetMode) => _StepButton(
            config: config,
            onPressed: _onStepButtonPressed,
            propertyLabel: propertyLabel,
            bIsEnabled: !bIsInResetMode,
            bIsFirst: stepIndex == 0,
            bIsLast: bIsLast,
          ),
        ),
      ));
    }

    return Padding(
      padding: EdgeInsets.symmetric(horizontal: DeltaWidgetConstants.widgetOuterXPadding),
      child: Container(
        decoration: BoxDecoration(
          color: Theme.of(context).colorScheme.background,
          borderRadius: BorderRadius.circular(30),
        ),
        padding: EdgeInsets.symmetric(horizontal: 4),
        child: Row(
          mainAxisAlignment: MainAxisAlignment.spaceEvenly,
          children: children,
        ),
      ),
    );
  }

  /// Called when a step button is pressed.
  void _onStepButtonPressed(double stepSize) {
    if (beginTransaction(AppLocalizations.of(context)!.transactionStepProperty(propertyLabel, stepSize))) {
      final List<double> deltaValues = List.filled(propertyCount, stepSize);
      handleOnChangedByUser(deltaValues);

      endTransaction();
    }
  }
}

/// A button to apply an increase/decrease in value.
class _StepButton extends StatelessWidget {
  const _StepButton({
    Key? key,
    required this.config,
    required this.onPressed,
    this.propertyLabel,
    this.bIsEnabled = true,
    this.bIsFirst = false,
    this.bIsLast = false,
  }) : super(key: key);

  /// The configuration for this step button.
  final StepperStepConfig config;

  /// The label of the property this is modifying.
  final String? propertyLabel;

  /// Callback for when the button is pressed. Passes the size of the step.
  final void Function(double stepSize) onPressed;

  /// Whether the owning widget has its controls enabled.
  final bool bIsEnabled;

  /// Whether this is the first listed button.
  final bool bIsFirst;

  /// Whether this is the last listed button.
  final bool bIsLast;

  @override
  Widget build(BuildContext context) {
    final TextStyle baseTextStyle = Theme.of(context).textTheme.bodyMedium!;
    final Color textColor =
        (config.textColor ?? baseTextStyle.color ?? UnrealColors.gray75).withOpacity(bIsEnabled ? 1 : 0.5);

    final String stepSize = '${config.stepSize > 0 ? '+' : ''}${config.stepSize}';

    return Tooltip(
      message: AppLocalizations.of(context)!.stepPropertyButtonTooltip(propertyLabel ?? '', stepSize),
      child: Semantics(
        button: true,
        child: MouseRegion(
          cursor: bIsEnabled ? MaterialStateMouseCursor.clickable : MouseCursor.defer,
          child: GestureDetector(
            onTap: bIsEnabled ? () => onPressed(config.stepSize) : null,
            child: Padding(
              padding: EdgeInsets.symmetric(vertical: 8, horizontal: 4),
              child: Container(
                decoration: BoxDecoration(
                  color: config.color ?? Theme.of(context).colorScheme.secondary,
                  borderRadius: BorderRadius.horizontal(
                    left: bIsFirst ? const Radius.circular(12) : Radius.zero,
                    right: bIsLast ? const Radius.circular(12) : Radius.zero,
                  ),
                ),
                height: 20,
                child: Center(
                  child: Text(
                    config.label,
                    style: baseTextStyle.copyWith(
                      color: textColor,
                      fontVariations: [FontVariation('wght', 600)],
                      height: 1.05,
                    ),
                  ),
                ),
              ),
            ),
          ),
        ),
      ),
    );
  }
}
