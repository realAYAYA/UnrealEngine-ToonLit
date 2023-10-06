// Copyright Epic Games, Inc. All Rights Reserved.

import 'dart:math' as math;
import 'dart:ui';

import 'package:logging/logging.dart';
import 'package:vector_math/vector_math_64.dart' as vec;

import '../models/color.dart';
import '../models/unreal_types.dart';

/// Type name to use to represent all Unreal enums, regardless of their actual type name.
String unrealEnumTypeName = '_Enum';

final _log = Logger('UnrealUtilities');

/// Data about how to convert an Unreal type into/out of JSON.
class _UnrealTypeConversionData {
  const _UnrealTypeConversionData({required this.jsonToDart, required this.dartToJson});

  /// Function that converts the type's JSON data ([jsonValue]) to its native Dart equivalent.
  /// [previousValue] is the previous value before [jsonValue] was received, or null if no previous value.
  /// [conversionMetadata] contains arbitrary key/value data which may be used in some cases to disambiguate how the
  /// data should be converted.
  final dynamic Function(dynamic jsonValue, dynamic previousValue, Map<String, dynamic> conversionMetadata) jsonToDart;

  /// Function that converts the type's native Dart type to its JSON equivalent.
  /// [conversionMetadata] contains arbitrary key/value data which may be used in some cases to disambiguate how the
  /// data should be converted.
  final dynamic Function(dynamic dartValue, Map<String, dynamic> conversionMetadata) dartToJson;
}

_UnrealTypeConversionData _noConversion = _UnrealTypeConversionData(
  jsonToDart: (dynamic jsonValue, dynamic previousValue, Map<String, dynamic> conversionMetadata) => jsonValue,
  dartToJson: (dynamic dartValue, Map<String, dynamic> conversionMetadata) => dartValue,
);

/// Map from Unreal type name to conversion data for the associated class.
Map<String, _UnrealTypeConversionData> _typeConverters = {
  'bool': _noConversion,
  'uint8': _UnrealTypeConversionData(
    jsonToDart: (dynamic jsonValue, dynamic previousValue, Map<String, dynamic> conversionMetadata) {
      if (jsonValue is bool) {
        // Boolean values are often represented as a single uint8 bit
        return jsonValue;
      }

      return jsonNumberToInt(jsonValue);
    },
    dartToJson: _noConversion.dartToJson,
  ),
  'float': _UnrealTypeConversionData(
    jsonToDart: (dynamic jsonValue, dynamic previousValue, Map<String, dynamic> conversionMetadata) =>
        jsonNumberToDouble(jsonValue, null),
    dartToJson: _noConversion.dartToJson,
  ),
  'double': _UnrealTypeConversionData(
    jsonToDart: (dynamic jsonValue, dynamic previousValue, Map<String, dynamic> conversionMetadata) =>
        jsonNumberToDouble(jsonValue, null),
    dartToJson: _noConversion.dartToJson,
  ),
  'FLinearColor': _UnrealTypeConversionData(
    jsonToDart: (dynamic jsonValue, dynamic previousValue, Map<String, dynamic> conversionMetadata) {
      final floatColor = FloatColor(
        jsonNumberToDouble(jsonValue['R'], 0.0)!,
        jsonNumberToDouble(jsonValue['G'], 0.0)!,
        jsonNumberToDouble(jsonValue['B'], 0.0)!,
        jsonNumberToDouble(jsonValue['A'], 0.0)!,
      );

      final hsvColor = NegatableHSVColor.fromColor(floatColor);

      const minValueForPosition = 0.0001;
      if (previousValue != null && hsvColor.value <= minValueForPosition) {
        // The RGBA color's value is too low to determine the position on the color wheel. Instead, we'll use the
        // last known position on our end.
        return WheelColor(previousValue.position, hsvColor.value, hsvColor.alpha);
      }

      return WheelColor.fromHSVColor(hsvColor);
    },
    dartToJson: (dynamic dartValue, Map<String, dynamic> conversionMetadata) => dartValue.toJson(),
  ),
  'FVector2D': _UnrealTypeConversionData(
    jsonToDart: (dynamic jsonValue, dynamic previousValue, Map<String, dynamic> conversionMetadata) => vec.Vector2(
      jsonNumberToDouble(jsonValue['X'], 0.0)!,
      jsonNumberToDouble(jsonValue['Y'], 0.0)!,
    ),
    dartToJson: (dynamic dartValue, Map<String, dynamic> conversionMetadata) => {'X': dartValue.x, 'Y': dartValue.y},
  ),
  'FVector4_Color': _UnrealTypeConversionData(
    jsonToDart: (dynamic jsonValue, dynamic previousValue, Map<String, dynamic> conversionMetadata) {
      final floatColor = FloatColor(
        jsonNumberToDouble(jsonValue['X'], 0.0)!,
        jsonNumberToDouble(jsonValue['Y'], 0.0)!,
        jsonNumberToDouble(jsonValue['Z'], 0.0)!,
        jsonNumberToDouble(jsonValue['W'], 0.0)!,
      );

      final double minValue = conversionMetadata['minValue'] ?? 0.0;
      final double maxValue = conversionMetadata['maxValue'] ?? 1.0;
      final double valueRange = maxValue - minValue;
      final double actualValue = math.max(floatColor.red, math.max(floatColor.green, floatColor.blue));

      assert(valueRange != 0);

      // Convert to a [0, 1] range first in order to produce a valid HSV color, but preserve the original value and
      // alpha (luminance) components.
      final FloatColor scaledColor =
          ((floatColor - FloatColor(minValue, minValue, minValue, minValue)) / valueRange).withAlpha(floatColor.alpha);
      final hsvColor = NegatableHSVColor.fromColor(scaledColor).withValue(actualValue);

      const minValueEpsilonForPosition = 0.0001;
      if (previousValue != null && actualValue <= minValue + minValueEpsilonForPosition) {
        // The RGBA color's value is too small to determine the position on the color wheel. Instead, we'll use the
        // last known position on our end.
        return WheelColor(previousValue.position, hsvColor.value, floatColor.alpha);
      }

      return WheelColor.fromHSVColor(hsvColor);
    },
    dartToJson: (dynamic dartValue, Map<String, dynamic> conversionMetadata) => dartValue.toJson(),
  ),
  unrealEnumTypeName: _noConversion,
};

/// Check whether the given Unreal [typeName] can be converted to a native Dart type.
bool canConvertUnrealType(String typeName) {
  return _typeConverters.containsKey(typeName);
}

/// Convert JSON [data] received from Unreal to the corresponding native Dart type based its [typeName].
/// You may provide the [previousValue] as a hint. For example when converting RGBA data to a color, we use the
/// previous value to fill in ambiguous hue/saturation values when the value is too low to calculate them unambiguously.
/// Some conversion functions may also make make use of arbitrary data stored in [conversionMetadata].
dynamic convertUnrealTypeJsonToDart(String typeName, dynamic data,
    {dynamic previousValue, Map<String, dynamic>? conversionMetadata}) {
  if (data == null) {
    return null;
  }

  return _findConverter(typeName)?.jsonToDart(data, previousValue, conversionMetadata ?? {});
}

/// Convert native Dart [data] to the expected JSON format for Unreal based its [typeName].
dynamic convertUnrealTypeDartToJson(String typeName, dynamic data, {Map<String, dynamic>? conversionMetadata}) {
  if (data == null) {
    return null;
  }

  return _findConverter(typeName)?.dartToJson(data, conversionMetadata ?? {});
}

/// Create a message in standard Unreal WebSocket format.
dynamic createUnrealWebSocketMessage(String messageName, Object parameters) {
  return {'MessageName': messageName, 'Parameters': parameters};
}

/// Find the converter for the given Unreal type name, or log the failure and return null if it doesn't exist.
_UnrealTypeConversionData? _findConverter(String typeName) {
  final _UnrealTypeConversionData? conversionData = _typeConverters[typeName];

  if (conversionData == null) {
    _log.warning('No converter exists for Unreal type "$typeName"', null, StackTrace.current);
    return null;
  }

  return conversionData;
}

/// Convert a number received in JSON format (which could be a string, int, double, or null) to a double.
/// If the number is null or can't be determined, defaultValue will be returned.
double? jsonNumberToDouble(dynamic number, [double? defaultValue = 0]) {
  if (number == null) {
    return defaultValue;
  }

  if (number is String) {
    return double.tryParse(number) ?? defaultValue;
  }

  try {
    return number.toDouble();
  } catch (e) {
    return defaultValue;
  }
}

/// Convert a number received in JSON format (which could be a string, int, double, or null) to an int.
/// If the number is null or can't be determined, defaultValue will be returned.
int? jsonNumberToInt(dynamic number, [int? defaultValue = 0]) {
  if (number == null) {
    return defaultValue;
  }

  if (number is String) {
    return int.tryParse(number) ?? defaultValue;
  }

  try {
    return number.toInt();
  } catch (e) {
    return defaultValue;
  }
}

/// Convert an offset to the JSON format Unreal expects for a 2D vector.
dynamic offsetToJson(Offset offset) {
  return {
    'X': offset.dx,
    'Y': offset.dy,
  };
}

/// Given a list of [properties], create a list of properties with [suffix] appended to their names.
List<UnrealProperty> getPropertiesWithSuffix(List<UnrealProperty> properties, String suffix,
    {String? typeNameOverride}) {
  return properties
      .map(
        (UnrealProperty property) => UnrealProperty(
          objectPath: property.objectPath,
          propertyName: property.propertyName + suffix,
          typeNameOverride: typeNameOverride,
        ),
      )
      .toList();
}

/// Given a list of [properties], create a list of properties residing within those properties.
List<UnrealProperty> getSubproperties(List<UnrealProperty> properties, String subpropertyName,
    {String? typeNameOverride}) {
  return getPropertiesWithSuffix(properties, '.$subpropertyName', typeNameOverride: typeNameOverride);
}
