// Copyright Epic Games, Inc. All Rights Reserved.

import 'dart:collection';

import 'package:flutter/material.dart';
import 'package:flutter_gen/gen_l10n/app_localizations.dart';
import 'package:logging/logging.dart';

import '../utilities/constants.dart';
import 'actor_data/light_card_actor_data.dart';
import 'unreal_types.dart';

final _log = Logger('UnrealClassData');

/// Data about a class that exists in Unreal Engine.
class UnrealClass {
  UnrealClass(
    this.internalName, {
    required this.getDisplayName,
    UnrealClass? parent,
    String? iconPath,
    Color? color,
    this.iconPathFunction,
    this.uiColorFunction,
    required List<String> classNames,
  })  : _parent = parent,
        _iconPath = iconPath,
        _uiColor = color,
        classNames = UnmodifiableListView(classNames) {
    if (parent != null) {
      parent._children.add(this);
    }
  }

  /// Function that returns a user-friendly display name for this class.
  final String Function(BuildContext) getDisplayName;

  /// Internal display name for this class.
  final String internalName;

  /// Path to the icon representing this class, or null to defer to parent class.
  final String? _iconPath;

  /// The color associated with this class, or null to defer to the parent class.
  final Color? _uiColor;

  /// Optional function to determine the icon based on an instance of this class.
  final String? Function(UnrealObject object)? iconPathFunction;

  /// Optional function to determine the color based on an instance of this class.
  final Color? Function(UnrealObject object)? uiColorFunction;

  /// Class names that map to this class.
  final UnmodifiableListView<String> classNames;

  /// The class this descends from.
  final UnrealClass? _parent;

  /// Descendants of this class.
  final List<UnrealClass> _children = [];

  /// The default icon path representing this class.
  String? get defaultIconPath => _iconPath ?? _parent?._iconPath;

  /// The default color representing this class.
  Color? get defaultColor => _uiColor ?? _parent?._uiColor;

  /// The class this descends from.
  UnrealClass? get parent => _parent;

  /// Descendants of this class.
  UnmodifiableListView<UnrealClass> get children => UnmodifiableListView(_children);

  /// Get the icon path for an instance of this class.
  String? getIconPathForInstance(UnrealObject object) {
    if (iconPathFunction == null || object.unrealClass?.isA(this) != true) {
      return defaultIconPath;
    }

    return iconPathFunction!(object) ?? defaultIconPath;
  }

  /// Get the associated color for an instance of this class.
  Color? getUIColorForInstance(UnrealObject object) {
    if (uiColorFunction == null || object.unrealClass?.isA(this) != true) {
      return defaultColor;
    }

    return uiColorFunction!(object) ?? defaultColor;
  }

  /// Check if this class is or descends from another class.
  bool isA(UnrealClass otherClass) {
    if (otherClass == this) {
      return true;
    }

    if (parent != null) {
      return parent!.isA(otherClass);
    }

    return false;
  }

  @override
  String toString() {
    return internalName;
  }
}

/// Holds data about the hierarchy of all known Unreal Engine classes.
class UnrealClassRegistry {
  static final actor = UnrealClass(
    'Actor',
    getDisplayName: (context) => AppLocalizations.of(context)!.actorNameActor,
    classNames: ['/Script/Engine.Actor'],
  );

  static final nDisplayRootActor = UnrealClass(
    'DisplayClusterRootActor',
    getDisplayName: (context) => AppLocalizations.of(context)!.actorNameDisplayClusterRoot,
    iconPath: 'packages/epic_common/assets/icons/ndisplay.svg',
    parent: actor,
    classNames: [nDisplayRootActorClassName],
  );

  static final lightCard = UnrealClass(
    'LightCard',
    getDisplayName: (context) => AppLocalizations.of(context)!.actorNameLightCard,
    iconPath: 'packages/epic_common/assets/icons/light_card.svg',
    color: const Color(0xffc4a300),
    iconPathFunction: (UnrealObject object) {
      final lightCardData = object.getPerClassData<LightCardActorData>();
      if (lightCardData != null) {
        if (!lightCardData.bIsDataReady) {
          return null;
        }

        if (lightCardData.bIsUV == true) {
          return 'packages/epic_common/assets/icons/light_card_uv.svg';
        }

        if (lightCardData.bIsFlag == true) {
          return 'packages/epic_common/assets/icons/light_card_flag.svg';
        }
      }

      return null;
    },
    uiColorFunction: (UnrealObject object) {
      final lightCardData = object.getPerClassData<LightCardActorData>();
      if (lightCardData != null) {
        if (!lightCardData.bIsDataReady) {
          return null;
        }

        if (lightCardData.bIsFlag == true) {
          return const Color(0xff6043d2);
        }
      }

      return null;
    },
    parent: actor,
    classNames: [lightCardClassName],
  );

  static final chromakeyCard = UnrealClass(
    'ChromakeyCard',
    getDisplayName: (context) => AppLocalizations.of(context)!.actorNameChromakeyCard,
    iconPath: 'packages/epic_common/assets/icons/chromakey_card.svg',
    color: const Color(0xff16884a),
    parent: lightCard,
    classNames: [chromakeyCardClassName],
  );

  static final colorCorrectRegion = UnrealClass(
    'ColorCorrectRegion',
    getDisplayName: (context) => AppLocalizations.of(context)!.actorNameColorCorrectRegion,
    color: const Color(0xffd72e80),
    iconPath: 'packages/epic_common/assets/icons/color_correct_region.svg',
    parent: actor,
    classNames: [colorCorrectRegionClassName],
  );

  static final colorCorrectWindow = UnrealClass(
    'ColorCorrectWindow',
    getDisplayName: (context) => AppLocalizations.of(context)!.actorNameColorCorrectWindow,
    iconPath: 'packages/epic_common/assets/icons/color_correct_window.svg',
    parent: colorCorrectRegion,
    classNames: colorCorrectWindowClassNames,
  );

  static final postProcessVolume = UnrealClass(
    'PostProcessVolume',
    getDisplayName: (context) => AppLocalizations.of(context)!.actorNamePostProcessVolume,
    iconPath: 'packages/epic_common/assets/icons/post_process_volume.svg',
    parent: actor,
    classNames: [postProcessVolumeClassName],
  );

  /// Determine the lowest-level Unreal class to which an Unreal [object] belongs based on its list of class names.
  static UnrealClass? getClassForObject(UnrealObject object) {
    UnrealClass? bestClass;

    for (final String className in object.classNames) {
      final UnrealClass? newClass = _instance._classNameToClassData[className];

      if (newClass == null) {
        continue;
      }

      if (bestClass == null || newClass.isA(bestClass)) {
        // Either we had no best class, or this class is a subset of the previous best class
        bestClass = newClass;
      } else if (!bestClass.isA(newClass)) {
        // The previous best class is unrelated to the latest class, so warn and ignore it
        _log.warning('Object $object has conflicting classes ($bestClass and $newClass)');
      }
    }

    return bestClass;
  }

  static final _instance = UnrealClassRegistry._();

  /// Map from class names to our data about that class.
  final Map<String, UnrealClass> _classNameToClassData = {};

  UnrealClassRegistry._() {
    _registerClass(actor);
    _registerClass(nDisplayRootActor);
    _registerClass(lightCard);
    _registerClass(chromakeyCard);
    _registerClass(colorCorrectRegion);
    _registerClass(colorCorrectWindow);
    _registerClass(postProcessVolume);
  }

  /// Register a class for lookup in the registry.
  void _registerClass(UnrealClass newClass) {
    for (final String className in newClass.classNames) {
      if (_classNameToClassData.containsKey(className)) {
        throw Exception('Tried to register a class with Unreal class name "$className", but another class with this '
            'path already exists (${_classNameToClassData[className]})');
      }

      _classNameToClassData[className] = newClass;
    }
  }
}
