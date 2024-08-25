// Copyright Epic Games, Inc. All Rights Reserved.

import 'dart:math';

import 'package:flutter/material.dart';
import 'package:vector_math/vector_math.dart';

/// A color represented as floating-point values as opposed to Dart's native, int-based [Color].
@immutable
class FloatColor {
  const FloatColor(this.red, this.green, this.blue, [this.alpha = 1]);

  final double red;
  final double green;
  final double blue;
  final double alpha;

  static const FloatColor zero = FloatColor(0, 0, 0, 0);

  /// Create a color from an existing HSVColor.
  static FloatColor fromHSVColor(NegatableHSVColor hsvColor) {
    return fromHSV(hsvColor.hue, hsvColor.saturation, hsvColor.value, hsvColor.alpha);
  }

  /// Create a color from hue, saturation, and value.
  static FloatColor fromHSV(double hue, double saturation, double value, [double alpha = 1]) {
    final double hDiv60 = hue / 60;
    final double hDiv60Fraction = hDiv60 - hDiv60.floorToDouble();

    final List<double> rgbValues = [
      value,
      value * (1 - saturation),
      value * (1 - (hDiv60Fraction * saturation)),
      value * (1 - ((1 - hDiv60Fraction) * saturation)),
    ];

    const List<List<int>> rgbSwizzle = [
      [0, 3, 1],
      [2, 0, 1],
      [1, 0, 3],
      [1, 2, 0],
      [3, 1, 0],
      [0, 1, 2],
    ];

    final int swizzleIndex = hDiv60.floor() % 6;

    return FloatColor(
      rgbValues[rgbSwizzle[swizzleIndex][0]],
      rgbValues[rgbSwizzle[swizzleIndex][1]],
      rgbValues[rgbSwizzle[swizzleIndex][2]],
      alpha,
    );
  }

  /// Convert to a Dart native color.
  Color toColor() {
    return Color.fromARGB(
      _doubleValueToInt(alpha),
      _doubleValueToInt(red),
      _doubleValueToInt(green),
      _doubleValueToInt(blue),
    );
  }

  /// Copy the color, overriding its [alpha] value.
  FloatColor withAlpha(double newAlpha) {
    return FloatColor(red, green, blue, newAlpha);
  }

  /// Piecewise addition of each color component.
  FloatColor operator +(FloatColor other) {
    return FloatColor(red + other.red, green + other.green, blue + other.blue, alpha + other.alpha);
  }

  /// Piecewise subtraction of each color component.
  FloatColor operator -(FloatColor other) {
    return FloatColor(red - other.red, green - other.green, blue - other.blue, alpha - other.alpha);
  }

  /// Piecewise multiplication of each color component.
  FloatColor operator *(double multiplier) {
    return FloatColor(red * multiplier, green * multiplier, blue * multiplier, alpha * multiplier);
  }

  /// Piecewise division of each color component.
  FloatColor operator /(double divisor) {
    return FloatColor(red / divisor, green / divisor, blue / divisor, alpha / divisor);
  }

  /// Convert this to a JSON representation compatible with Unreal Engine.
  Map<String, dynamic> toJson() {
    return {'R': red, 'G': green, 'B': blue, 'A': alpha};
  }

  /// Convert this to a user-friendly string representation.
  @override
  String toString() {
    return '(${red.toStringAsFixed(2)},'
        '${green.toStringAsFixed(2)},'
        '${blue.toStringAsFixed(2)},'
        '${alpha.toStringAsFixed(2)})';
  }

  /// Convert a floating-point color value to an 8-bit integer.
  int _doubleValueToInt(double value) {
    return (value * 255).round().clamp(0, 255);
  }
}

/// An HSV color that imposes no limits on the value or alpha components, allowing for negative/subtractive colors.
@immutable
class NegatableHSVColor {
  /// Creates a color.
  const NegatableHSVColor.fromAHSV(this.alpha, this.hue, this.saturation, this.value)
      : assert(hue >= 0.0),
        assert(hue <= 360.0),
        assert(saturation >= 0.0),
        assert(saturation <= 1.0);

  /// Creates a [NegatableHSVColor] from an RGB [FloatColor].
  factory NegatableHSVColor.fromColor(FloatColor color) {
    final List<double> rgbList = [color.red, color.green, color.blue];
    final double rgbMin = rgbList.reduce(min);
    final double rgbMax = rgbList.reduce(max);
    final double rgbRange = rgbMax - rgbMin;

    double hue;
    if (rgbMax == rgbMin) {
      hue = 0;
    } else if (rgbMax == color.red) {
      hue = ((((color.green - color.blue) / rgbRange) * 60) + 360) % 360;
    } else if (rgbMax == color.green) {
      hue = (((color.blue - color.red) / rgbRange) * 60) + 120;
    } else {
      // rgbMax == blue
      hue = (((color.red - color.green) / rgbRange) * 60) + 240;
    }

    final double saturation = (rgbMax == 0) ? 0 : (rgbRange / rgbMax).clamp(0, 1);
    final double value = rgbMax;

    return NegatableHSVColor.fromAHSV(color.alpha, hue, saturation, value);
  }

  /// Copy the color, overriding its [alpha] value.
  NegatableHSVColor withAlpha(double newAlpha) {
    return NegatableHSVColor.fromAHSV(newAlpha, hue, saturation, value);
  }

  /// Copy the color, overriding its [hue] value.
  NegatableHSVColor withHue(double newHue) {
    return NegatableHSVColor.fromAHSV(alpha, newHue, saturation, value);
  }

  /// Copy the color, overriding its [saturation] value.
  NegatableHSVColor withSaturation(double newSaturation) {
    return NegatableHSVColor.fromAHSV(alpha, hue, newSaturation, value);
  }

  /// Copy the color, overriding its [value] value.
  NegatableHSVColor withValue(double newValue) {
    return NegatableHSVColor.fromAHSV(alpha, hue, saturation, newValue);
  }

  final double alpha;
  final double hue;
  final double saturation;
  final double value;
}

/// A color represented by its position on an HSV color wheel. We use this representation internally since it reduces
/// imprecision caused by RGB (ambiguous hue/saturation at low value) or HSV (huge hue delta values when the position
/// is near the center of the wheel, i.e. low saturation) representations.
@immutable
class WheelColor {
  /// Create a [WheelColor] from its individual components.
  const WheelColor(this.position, this.value, this.alpha);

  /// Create a [WheelColor] from hue/saturation/value.
  factory WheelColor.fromHSV(double hue, double saturation, double value, double alpha) {
    final Offset position = Offset.fromDirection(radians(hue), saturation);
    return WheelColor(position, value, alpha);
  }

  /// Create a [WheelColor] from an [HSVColor].
  factory WheelColor.fromHSVColor(NegatableHSVColor color) {
    return WheelColor.fromHSV(color.hue, color.saturation, color.value, color.alpha);
  }

  /// Create a [WheelColor] from a [FloatColor].
  factory WheelColor.fromFloatColor(FloatColor color) {
    return WheelColor.fromHSVColor(NegatableHSVColor.fromColor(color));
  }

  /// A [WheelColor] with all values set to 0.
  static const WheelColor zero = WheelColor(Offset.zero, 0, 0);

  /// Position of the color on a color wheel of radius 1.
  final Offset position;

  /// The value component of the color's HSV value.
  final double value;

  /// The alpha component of the color.
  final double alpha;

  /// Get the hue of the color this represents.
  double get hue => (degrees(position.direction) + 360.0) % 360.0;

  /// Get the saturation of the color this represents.
  double get saturation => position.distance;

  /// Piecewise addition of each color component.
  WheelColor operator +(WheelColor other) =>
      WheelColor(position + other.position, value + other.value, alpha + other.alpha);

  /// Piecewise subtraction of each color component.
  WheelColor operator -(WheelColor other) =>
      WheelColor(position - other.position, value - other.value, alpha - other.alpha);

  /// Convert this to an HSV color.
  NegatableHSVColor toHSVColor() => NegatableHSVColor.fromAHSV(
        alpha,
        ((hue % 360) + 360) % 360,
        saturation.clamp(0, 1),
        value,
      );

  /// Convert this to a floating-point RGB color.
  FloatColor toFloatColor() => FloatColor.fromHSVColor(toHSVColor());

  /// Convert this to a JSON representation compatible with Unreal Engine.
  /// If [bUseLuminance] is true, replace Alpha with Luminance.
  Map<String, dynamic> toJson({bool bUseLuminance = false}) {
    return {
      'Position': {
        'X': position.dx,
        'Y': position.dy,
      },
      'Value': value,
      (bUseLuminance ? 'Luminance' : 'Alpha'): alpha,
    };
  }
}
