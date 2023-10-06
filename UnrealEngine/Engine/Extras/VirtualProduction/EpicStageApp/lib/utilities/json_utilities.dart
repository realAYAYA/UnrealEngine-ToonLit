// Copyright Epic Games, Inc. All Rights Reserved.

/// Convert an enum to a value for JSON serialization.
String enumToJsonValue<T extends Enum>(T value) {
  return value.name;
}

/// Convert a serialized JSON value to an enum value.
T? jsonToEnumValue<T extends Enum>(dynamic value, List<T> enumValues) {
  if (!(value is String)) {
    return null;
  }

  return enumValues.firstWhere((enumValue) => enumValue.name == value, orElse: null);
}
