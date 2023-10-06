// Copyright Epic Games, Inc. All Rights Reserved.

import 'dart:math';
import 'dart:ui';

/// Given a [value], apply the exponential curve produced by the [exponent].
/// If [bInverse] is true, apply the inverse of the exponent.
double exponentiateValue(double value, double exponent, bool bInverse) {
  assert(exponent > 0);

  if (exponent == 1.0) {
    return value;
  }

  if (bInverse) {
    if (exponent == 0) {
      throw Exception('Can\'t apply inverse of exponent $exponent}');
    }
    exponent = 1.0 / exponent;
  }

  return pow(value, exponent) as double;
}

/// Given a [position] relative to a circle's center, apply the exponential curve produced by the [exponent] to the
/// position's distance from the center. If [bInverse] is true, apply the inverse of the exponent.
Offset exponentiateCirclePosition(Offset position, double exponent, bool bInverse) {
  if (exponent == 1.0) {
    return position;
  }

  final double distance = position.distance;
  if (distance <= 0) {
    return Offset.zero;
  }

  final double scale = exponentiateValue(distance, exponent, bInverse) / distance;
  return position.scale(scale, scale);
}
