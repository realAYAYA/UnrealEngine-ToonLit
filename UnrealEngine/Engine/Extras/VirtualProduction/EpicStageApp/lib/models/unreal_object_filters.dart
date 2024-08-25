// Copyright Epic Games, Inc. All Rights Reserved.

import 'dart:collection';

import 'package:flutter/material.dart';
import 'package:flutter_gen/gen_l10n/app_localizations.dart';

import 'actor_data/light_card_actor_data.dart';
import 'unreal_class_data.dart';
import 'unreal_types.dart';

/// Data about a filter that can be applied to [UnrealObject]s.
abstract class UnrealObjectFilter {
  const UnrealObjectFilter({
    required this.internalName,
    required this.getDisplayName,
    required this.iconPath,
  });

  /// Unique name to refer to this in code.
  final String internalName;

  /// Function that returns the name shown to the user.
  final String Function(BuildContext) getDisplayName;

  /// Path to the image file for the icon representing this category.
  final String? iconPath;

  /// Returns true if the [object] passes this filter.
  bool passes(UnrealObject object);
}

/// An [UnrealObjectFilter] with a custom filtering function.
class _CustomUnrealObjectFilter extends UnrealObjectFilter {
  const _CustomUnrealObjectFilter({
    required super.internalName,
    required super.getDisplayName,
    required super.iconPath,
    required bool Function(UnrealObject object) passFunction,
  }) : _passFunction = passFunction;

  /// Function that returns true if the [object] is a member of this category.
  final bool Function(UnrealObject object) _passFunction;

  @override
  bool passes(UnrealObject object) => _passFunction(object);
}

/// An [UnrealObjectFilter] that checks against a specific Unreal class.
class _ClassBasedUnrealObjectFilter extends UnrealObjectFilter {
  _ClassBasedUnrealObjectFilter(
    this.unrealClass, {
    bool Function(UnrealObject object)? passFunction,
  })  : _passFunction = passFunction,
        super(
          internalName: 'class${unrealClass.internalName}',
          getDisplayName: unrealClass.getDisplayName,
          iconPath: unrealClass.defaultIconPath,
        );

  /// The class to which this will filter. Objects must be an exact member of this class, not a descendant.
  final UnrealClass unrealClass;

  /// Optional function that returns true if the [object] is a member of this category, checked in addition to the
  /// filter's class constraint.
  final bool Function(UnrealObject object)? _passFunction;

  @override
  bool passes(UnrealObject object) {
    if (object.unrealClass != unrealClass) {
      return false;
    }

    if (_passFunction != null && !_passFunction!(object)) {
      return false;
    }

    return true;
  }
}

/// Holds data about all possible types of object filter.
class UnrealObjectFilterRegistry {
  static final _instance = UnrealObjectFilterRegistry._();

  /// List of all possible object filters.
  final UnmodifiableListView<UnrealObjectFilter> _filters = UnmodifiableListView([
    _ClassBasedUnrealObjectFilter(
      UnrealClassRegistry.lightCard,
      passFunction: (UnrealObject object) {
        final lightcardData = object.getPerClassData<LightCardActorData>();
        return lightcardData?.bIsDataReady == true && lightcardData?.bIsFlag == false;
      },
    ),
    _CustomUnrealObjectFilter(
      internalName: 'Flag',
      getDisplayName: (context) => AppLocalizations.of(context)!.actorNameFlag,
      iconPath: 'packages/epic_common/assets/icons/light_card_flag.svg',
      passFunction: (UnrealObject object) {
        final lightcardData = object.getPerClassData<LightCardActorData>();
        return lightcardData?.bIsFlag == true;
      },
    ),
    _ClassBasedUnrealObjectFilter(UnrealClassRegistry.chromakeyCard),
    _ClassBasedUnrealObjectFilter(UnrealClassRegistry.colorCorrectWindow),
    _ClassBasedUnrealObjectFilter(UnrealClassRegistry.colorCorrectRegion),
  ]);

  /// Map from filter internal name to filter.
  final Map<String, UnrealObjectFilter> _filtersByName = {};

  UnrealObjectFilterRegistry._() {
    for (final UnrealObjectFilter filter in _filters) {
      _filtersByName[filter.internalName] = filter;
    }
  }

  /// Get a list of all available filters.
  static UnmodifiableListView<UnrealObjectFilter> get filters => _instance._filters;

  /// Get the filter associated with the given internal name.
  static UnrealObjectFilter? getFilterByName(String internalName) => _instance._filtersByName[internalName];
}
