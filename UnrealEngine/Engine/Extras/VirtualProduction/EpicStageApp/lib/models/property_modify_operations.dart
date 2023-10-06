// Copyright Epic Games, Inc. All Rights Reserved.

import 'dart:ui';

import '../utilities/math_utilities.dart';
import '../utilities/unreal_utilities.dart';
import 'color.dart';
import 'unreal_types.dart';

/// How a property's value should be treated when it pushes past the min/max for the property.
enum PropertyMinMaxBehaviour {
  /// Ignore the engine min/max values entirely.
  ignore,

  /// Clamp the value to the min/max value.
  clamp,

  /// Use a modulo operation to "loop" back to the other side, i.e. pushing past max will return to min and vice versa.
  loop,
}

/// Data about a modification event for an Unreal property. This data is generated when we're ready to send a message
/// back to the engine about the change.
class PropertyModifyEvent {
  const PropertyModifyEvent({
    required this.property,
    required this.currentValue,
    required this.deltaValue,
    required this.presetName,
    required this.presetPropertyLabel,
    required this.typeName,
    required this.sequenceNumber,
    required this.transactionId,
  });

  /// The property being modified.
  final UnrealProperty property;

  /// The current value of the property.
  final dynamic currentValue;

  /// The delta value for this change event.
  final dynamic deltaValue;

  /// The name of the preset that the property is exposed on.
  final String presetName;

  /// The label of the property as exposed on the preset.
  final String presetPropertyLabel;

  /// The type name of the property being modified.
  final String typeName;

  /// The sequence number of this change.
  final int sequenceNumber;

  /// The ID of the transaction with which to associate the changes.
  final int transactionId;
}

/// Defines logic for a type of property modification, including applying the change locally, clamping it to reasonable
/// bounds, and sending the result to the engine.
abstract class PropertyModifyOperation {
  const PropertyModifyOperation();

  /// If true, this operation will always be considered significant even if the clamp returns null.
  bool get bIsAlwaysSignificant => false;

  /// The name of the WebSocket message to send for this operation.
  String get messageName;

  /// Returns the result of applying a change of [deltaValue] to a base [value] using this operation.
  dynamic apply(dynamic value, dynamic deltaValue);

  /// Implements clamping behaviour for this operation. Given a [value] that the [deltaValue] will be applied to, return
  /// a new delta value which won't exceed the constraints by clamping the result value between min and max.
  /// If the delta is effectively a no-op as a result of the clamp, return null.
  /// [hardMin] and [hardMax] contain the min/max values allowed for this property, though they may be null meaning
  /// the property has no limit in the corresponding direction.
  dynamic clamp(dynamic value, dynamic deltaValue, dynamic hardMin, dynamic hardMax);

  /// Implements looping behaviour for this operation.  Given a [value] that the [deltaValue] will be applied to, return
  /// a new delta value which won't exceed the clamp constraints by looping the result value from min to max and vice
  /// versa.
  /// If the value can't be looped, return null.
  dynamic loop(dynamic value, dynamic deltaValue, dynamic hardMin, dynamic hardMax);

  /// Returns the parameters for a WebSocket message that will apply the given [modifyEvent] in the engine using this
  /// operation.
  dynamic makeMessageParameters(PropertyModifyEvent modifyEvent);
}

/// A generic arithmetic operation.
abstract class GenericArithmeticModifyOperation extends PropertyModifyOperation {
  const GenericArithmeticModifyOperation() : super();

  /// The name of the operation type to send to the engine.
  String get operationName;

  /// Returns the delta value that, when applied with this operation, would change [oldValue] into [newValue].
  dynamic makeDeltaValue(dynamic oldValue, dynamic newValue);

  @override
  String get messageName => 'preset.property.modify';

  @override
  dynamic apply(dynamic value, dynamic deltaValue) => value + deltaValue;

  @override
  dynamic clamp(dynamic value, dynamic deltaValue, dynamic hardMin, dynamic hardMax) {
    if (hardMin == null && hardMax == null) {
      // No clamping needed
      return deltaValue;
    }

    if (hardMin != null && hardMax != null && hardMin > hardMax) {
      throw Exception('Cannot clamp when min > max ($hardMin > $hardMax)');
    }

    final dynamic changedValue = apply(value, deltaValue);

    // Determine what the changed value should be after clamping
    dynamic targetValue;
    if (hardMax != null && changedValue > hardMax) {
      targetValue = hardMax;
    } else if (hardMin != null && changedValue < hardMin) {
      targetValue = hardMin;
    } else {
      // No clamping needed
      return deltaValue;
    }

    // If the target value is the same as the original value, then we were already at the clamp limit and this delta
    // is a no-op.
    if (targetValue == value) {
      return null;
    }

    return targetValue - value;
  }

  @override
  dynamic loop(value, deltaValue, hardMin, hardMax) {
    if (hardMin == null || hardMax == null) {
      // Can't loop without a min and max
      return null;
    }

    if (hardMin >= hardMax) {
      // Can't loop when min >= max
      return null;
    }

    final dynamic range = hardMax - hardMin;
    dynamic targetValue = apply(value, deltaValue);

    while (targetValue > hardMax) {
      targetValue -= range;
      deltaValue -= range;
    }

    while (targetValue < hardMin) {
      targetValue += range;
      deltaValue += range;
    }

    assert(targetValue >= hardMin && targetValue <= hardMax);

    return targetValue - value;
  }

  @override
  dynamic makeMessageParameters(PropertyModifyEvent modifyEvent) {
    return {
      'PropertyValue': convertUnrealTypeDartToJson(modifyEvent.typeName, modifyEvent.deltaValue),
      'PresetName': modifyEvent.presetName,
      'PropertyLabel': modifyEvent.presetPropertyLabel,
      'Operation': operationName,
      'SequenceNumber': modifyEvent.sequenceNumber,
      'TransactionMode': 'MANUAL',
      'TransactionId': modifyEvent.transactionId,
    };
  }
}

/// An additive operation on a generic value.
class AddOperation extends GenericArithmeticModifyOperation {
  const AddOperation() : super();

  @override
  makeDeltaValue(oldValue, newValue) => newValue - oldValue;

  @override
  String get operationName => 'ADD';
}

/// A multiplicative operation on a generic value.
class MultiplyOperation extends GenericArithmeticModifyOperation {
  const MultiplyOperation() : super();

  @override
  makeDeltaValue(oldValue, newValue) => (oldValue != 0.0) ? (newValue / oldValue) : null;

  @override
  String get operationName => 'MULTIPLY';
}

/// An operation that resets the value to its default. The engine doesn't expose the default value to the app, so this
/// operation doesn't actually change the value locally; instead, we'll get the update from the engine later.
class ResetOperation extends PropertyModifyOperation {
  const ResetOperation() : super();

  @override
  bool get bIsAlwaysSignificant => true;

  @override
  String get messageName => 'preset.property.modify';

  @override
  apply(value, deltaValue) => value;

  @override
  clamp(value, deltaValue, hardMin, hardMax) => deltaValue;

  @override
  loop(value, deltaValue, hardMin, hardMax) => null;

  @override
  makeMessageParameters(PropertyModifyEvent modifyEvent) {
    return {
      'PropertyValue': convertUnrealTypeDartToJson(modifyEvent.typeName, modifyEvent.deltaValue),
      'PresetName': modifyEvent.presetName,
      'PropertyLabel': modifyEvent.presetPropertyLabel,
      'SequenceNumber': modifyEvent.sequenceNumber,
      'ResetToDefault': true,
      'TransactionMode': 'MANUAL',
      'TransactionId': modifyEvent.transactionId,
    };
  }
}

/// An operation that sets a value directly. deltaValue is treated as an absolute value instead of a relative one.
/// Note that no clamping is applied.
class SetOperation extends PropertyModifyOperation {
  const SetOperation() : super();

  @override
  bool get bIsAlwaysSignificant => true;

  @override
  String get messageName => 'preset.property.modify';

  @override
  apply(value, deltaValue) => deltaValue;

  @override
  clamp(value, deltaValue, hardMin, hardMax) => deltaValue;

  @override
  loop(value, deltaValue, hardMin, hardMax) => null;

  @override
  makeMessageParameters(PropertyModifyEvent modifyEvent) {
    return {
      'PropertyValue': convertUnrealTypeDartToJson(modifyEvent.typeName, modifyEvent.deltaValue),
      'PresetName': modifyEvent.presetName,
      'PropertyLabel': modifyEvent.presetPropertyLabel,
      'SequenceNumber': modifyEvent.sequenceNumber,
      'TransactionMode': 'MANUAL',
      'TransactionId': modifyEvent.transactionId,
    };
  }
}

/// An additive operation on a [WheelColor]. The value will be clamped within the range of the color wheel.
class WheelColorAddOperation extends PropertyModifyOperation {
  const WheelColorAddOperation() : super();

  @override
  String get messageName => 'object.call';

  @override
  dynamic apply(dynamic value, dynamic deltaValue) => value + deltaValue;

  @override
  dynamic clamp(dynamic value, dynamic deltaValue, dynamic hardMin, dynamic hardMax) {
    // Clamp the value and alpha components in the valid [0,1] range
    final double clampedDeltaValueComponent =
        _clampAdditiveDelta(value.value, deltaValue.value, min: 0.0, max: 1.0).toDouble();
    final double clampedDeltaAlpha = _clampAdditiveDelta(value.alpha, deltaValue.alpha, min: 0.0, max: 1.0).toDouble();

    // Clamp position to a radius of 1
    final Offset resultPosition = value.position + deltaValue.position;
    Offset clampedDeltaPosition = deltaValue.position;

    if (resultPosition.distanceSquared > 1) {
      final Offset clampedPosition = resultPosition / resultPosition.distance;
      clampedDeltaPosition = clampedPosition - value.position;
    }

    // Create the new delta value, but return null if it's a no-op
    final newDeltaValue = WheelColor(clampedDeltaPosition, clampedDeltaValueComponent, clampedDeltaAlpha);
    if (newDeltaValue == WheelColor.zero) {
      return null;
    }

    return newDeltaValue;
  }

  @override
  loop(value, deltaValue, hardMin, hardMax) => null;

  @override
  dynamic makeMessageParameters(PropertyModifyEvent modifyEvent) {
    return {
      'FunctionName': 'ApplyColorWheelDelta',
      'ObjectPath': '/Script/RemoteControl.Default__RemoteControlFunctionLibrary',
      'Parameters': {
        'TargetObject': modifyEvent.property.objectPath,
        'PropertyName': modifyEvent.property.propertyName,
        'DeltaValue': convertUnrealTypeDartToJson(modifyEvent.typeName, modifyEvent.deltaValue),
        'ReferenceColor': convertUnrealTypeDartToJson(modifyEvent.typeName, modifyEvent.currentValue),
        'bIsInteractive': true,
      },
      'SequenceNumber': modifyEvent.sequenceNumber,
      'TransactionMode': 'MANUAL',
      'TransactionId': modifyEvent.transactionId,
    };
  }
}

/// An additive operation on a [WheelGradingColor]. The value will be clamped within the range of the color wheel.
class WheelColorGradingAddOperation extends PropertyModifyOperation {
  const WheelColorGradingAddOperation({
    required this.saturationExponent,
    required this.minValue,
    required this.maxValue,
  }) : super();

  /// Exponent applied to the saturation component of the wheel, making it less sensitive towards the center.
  final double saturationExponent;

  /// The minimum value of the slider. The value will be clamped in this range and scaled based on the min/max range
  /// when received in the engine.
  final double minValue;

  /// The minimum value of the slider. The value will be clamped in this range and scaled based on the min/max range
  /// when received in the engine.
  final double maxValue;

  @override
  String get messageName => 'object.call';

  @override
  dynamic apply(dynamic value, dynamic deltaValue) => value + deltaValue;

  @override
  dynamic clamp(dynamic value, dynamic deltaValue, dynamic hardMin, dynamic hardMax) {
    // Clamp the value and alpha components in the valid range
    final double clampedDeltaValueComponent =
        _clampAdditiveDelta(value.value, deltaValue.value, min: minValue, max: maxValue).toDouble();
    final double clampedDeltaAlpha =
        _clampAdditiveDelta(value.alpha, deltaValue.alpha, min: minValue, max: maxValue).toDouble();

    // Delta is applied by user in exponential space, so find the delta in linear space
    final Offset exponentialPosition = exponentiateCirclePosition(value.position, saturationExponent, true);
    final Offset exponentialResultPosition = exponentialPosition + deltaValue.position;
    final Offset linearResultPosition =
        exponentiateCirclePosition(exponentialResultPosition, saturationExponent, false);
    final Offset deltaPosition = linearResultPosition - value.position;

    // Clamp position to a radius of 1 (this is unaffected by min/maxValue since the position represents saturation,
    // which is always in range [0, 1])
    Offset clampedDeltaPosition = deltaPosition;

    if (linearResultPosition.distanceSquared > 1) {
      final Offset clampedPosition = linearResultPosition / linearResultPosition.distance;
      clampedDeltaPosition = clampedPosition - value.position;
    }

    // Create the new delta value, but return null if it's a no-op
    final newDeltaValue = WheelColor(clampedDeltaPosition, clampedDeltaValueComponent, clampedDeltaAlpha);
    if (newDeltaValue == WheelColor.zero) {
      return null;
    }

    return newDeltaValue;
  }

  @override
  loop(value, deltaValue, hardMin, hardMax) => null;

  @override
  dynamic makeMessageParameters(PropertyModifyEvent modifyEvent) {
    return {
      'FunctionName': 'ApplyColorGradingWheelDelta',
      'ObjectPath': '/Script/RemoteControl.Default__RemoteControlFunctionLibrary',
      'Parameters': {
        'TargetObject': modifyEvent.property.objectPath,
        'PropertyName': modifyEvent.property.propertyName,
        'DeltaValue': modifyEvent.deltaValue.toJson(bUseLuminance: true),
        'ReferenceColor': modifyEvent.currentValue.toJson(bUseLuminance: true),
        'bIsInteractive': true,
        'MinValue': minValue,
        'MaxValue': maxValue,
      },
      'SequenceNumber': modifyEvent.sequenceNumber,
      'TransactionMode': 'MANUAL',
      'TransactionId': modifyEvent.transactionId,
    };
  }
}

/// Given a base [value] and a [deltaValue] to add to it, return the clamped delta such that the final sum won't exceed
/// the [min] or [max] values.
num _clampAdditiveDelta(num value, num deltaValue, {num? min, num? max}) {
  num clampedResult = value + deltaValue;

  if (min != null && clampedResult < min) {
    clampedResult = min;
  } else if (max != null && clampedResult > max) {
    clampedResult = max;
  } else {
    return deltaValue;
  }

  return clampedResult - value;
}
