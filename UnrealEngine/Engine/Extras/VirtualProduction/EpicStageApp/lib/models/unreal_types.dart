// Copyright Epic Games, Inc. All Rights Reserved.

import 'dart:collection';

import 'package:flutter/material.dart';
import 'package:logging/logging.dart';

import 'actor_data/per_class_actor_data.dart';
import 'unreal_actor_manager.dart';
import 'unreal_class_data.dart';

final _log = Logger('UnrealTypes');

/// Data about an object that exists in an Unreal Engine instance.
class UnrealObject extends ChangeNotifier {
  UnrealObject({required String path, required String name})
      : _name = name,
        _path = path;

  /// The full path of the object in the editor.
  final String _path;

  /// Classes to which the engine has reported this class belongs.
  final Set<String> _classNames = {};

  /// The lowest-level Unreal class that this belongs to based on its class names.
  UnrealClass? _unrealClass;

  /// Extra data retrieved from the engine based on this actor's classes.
  final Map<Type, UnrealPerClassActorData> _perClassData = {};

  /// The object's display name.
  String _name;

  /// Whether this object has been destroyed in the engine.
  bool _bIsDestroyed = false;

  /// Whether this object is no longer being watched, so may be out of date.
  bool _bIsStale = false;

  /// The full path of the object in the editor.
  String get path => _path;

  /// The object's display name.
  String get name => _name;

  set name(String value) {
    if (value != _name) {
      _name = value;
      notifyListeners();
    }
  }

  /// Whether this object has been destroyed in the engine.
  bool get bIsDestroyed => _bIsDestroyed;

  /// Whether this object is no longer being watched, so may be out of date.
  bool get bIsStale => _bIsStale;

  /// Whether this object still exists in the engine and is actively being watched.
  /// In either case, this object will never be valid again, and you should get a new reference from the
  /// [UnrealActorManager] once the actor is watched/created again.
  bool get bIsValid => !bIsDestroyed && !bIsStale;

  /// Classes to which the engine has reported this class belongs.
  UnmodifiableSetView<String> get classNames => UnmodifiableSetView(_classNames);

  /// The lowest-level Unreal class that this belongs to based on its class names.
  UnrealClass? get unrealClass => _unrealClass;

  /// Return true if the the engine has reported the actor as a member of a [className].
  /// Note that this does not handle inheritance; it will only check exact matches in list of classes for which
  /// [UnrealActorManager] is actively listening.
  bool isA(String className) {
    return _classNames.contains(className);
  }

  /// Return true if the the engine has reported the actor as a member of any of the given [classNames].
  /// Note that this does not handle inheritance; it will only check exact matches in list of classes for which
  /// [UnrealActorManager] is actively listening.
  bool isAny(List<String> classNames) {
    return classNames.any((className) => _classNames.contains(className));
  }

  /// Indicate that this actor has been destroyed.
  void onDestroyed() {
    if (!_bIsDestroyed) {
      _bIsDestroyed = true;
      notifyListeners();
    }
  }

  /// Indicate that this actor is no longer being watched.
  void onUnwatched() {
    if (!_bIsStale) {
      _bIsStale = true;
      notifyListeners();
    }
  }

  /// Add a class name that the actor belongs to.
  void addClassName(String className) {
    _classNames.add(className);
    _unrealClass = UnrealClassRegistry.getClassForObject(this);
    notifyListeners();
  }

  /// Add class-specific data to the actor.
  void addPerClassData(dynamic data) {
    if (_perClassData[data.runtimeType] != null) {
      _log.warning('Tried to add ${data.runtimeType} to $this, but it already had data of that type');
      return;
    }

    _perClassData[data.runtimeType] = data;
    data.addListener(notifyListeners);
    notifyListeners();
  }

  /// Get class-specific data of the given type from the actor (if it exists).
  T? getPerClassData<T extends UnrealPerClassActorData>() {
    return _perClassData[T] as T?;
  }

  /// Get class-specific data of the given type from the actor (if it exists).
  UnrealPerClassActorData? getPerClassDataOfType(Type type) {
    return _perClassData[type];
  }

  /// Get the path of the icon associated with this actor's type.
  String? getIconPath() {
    return _unrealClass?.getIconPathForInstance(this);
  }

  /// Get the color associated with this actor's type.
  Color? getUIColor() {
    return _unrealClass?.getUIColorForInstance(this);
  }

  @override
  void dispose() {
    for (final UnrealPerClassActorData data in _perClassData.values) {
      data.dispose();
    }
    _perClassData.clear();

    super.dispose();
  }
}

/// Data about a property on an object that exists in an Unreal Engine instance.
class UnrealProperty {
  const UnrealProperty({
    required this.objectPath,
    required this.propertyName,
    this.typeNameOverride,
    String? friendlyObjectName,
  }) : _friendlyObjectName = friendlyObjectName;

  /// A reference to no property.
  static const UnrealProperty empty = UnrealProperty(objectPath: '', propertyName: '');

  /// The full path of object in the editor.
  final String objectPath;

  /// The property's name.
  final String propertyName;

  /// An optional override for the property's type, which lets the app handle the property differently than
  /// the default for its actual type in the engine (e.g. interpret a Vector4 as a color instead).
  final String? typeNameOverride;

  /// The optional user-friendly name of the object the property belongs to.
  final String? _friendlyObjectName;

  /// Whether this references a property (though that property may not exist).
  bool get bIsNotEmpty => objectPath.isNotEmpty && propertyName.isNotEmpty;

  /// Get the name of the object to display to the user.
  String get objectName {
    if (_friendlyObjectName != null) {
      return _friendlyObjectName!;
    }

    final int dotIndex = objectPath.lastIndexOf('.');
    if (dotIndex == -1) {
      return objectPath;
    }

    return objectPath.substring(dotIndex + 1);
  }

  /// The name of the last property in the chain if this is a nested property.
  String get lastPropertyName {
    final lastDotIndex = propertyName.lastIndexOf('.');
    return (lastDotIndex == -1) ? propertyName : propertyName.substring(lastDotIndex + 1);
  }

  /// Make a property with a suffix applied to the [propertyName].
  UnrealProperty makeSubproperty(String suffix, {String? typeNameOverride, String? friendlyObjectName}) {
    return UnrealProperty(
      objectPath: objectPath,
      propertyName: '$propertyName.$suffix',
      typeNameOverride: typeNameOverride,
      friendlyObjectName: friendlyObjectName,
    );
  }

  @override
  bool operator ==(Object other) {
    if (identical(this, other)) {
      return true;
    }

    if (other.runtimeType != runtimeType) {
      return false;
    }

    return other is UnrealProperty &&
        other.objectPath == objectPath &&
        other.propertyName == propertyName &&
        other.typeNameOverride == typeNameOverride &&
        other._friendlyObjectName == _friendlyObjectName;
  }

  @override
  int get hashCode => Object.hash(objectPath, propertyName, typeNameOverride, _friendlyObjectName);
}

/// Data about an actor template available in the engine.
class UnrealTemplateData {
  const UnrealTemplateData({required this.name, required this.path});

  /// The user-friendly name of the template.
  final String name;

  /// The path of the template asset.
  final String path;
}
