// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:epic_common/preferences.dart';
import 'package:flutter/material.dart';
import 'package:flutter_gen/gen_l10n/app_localizations.dart';
import 'package:logging/logging.dart';
import 'package:streaming_shared_preferences/streaming_shared_preferences.dart';

import '../../utilities/debug_utilities.dart';
import '../navigator_keys.dart';

final _log = Logger('DeltaWidgetSettings');

/// Types of limits the user can edit for a color range.
enum RangeLimitType {
  min,
  max,
}

/// Which type of actor properties to display in the details panel.
enum DetailsPropertyDisplayType {
  appearance,
  orientation,
}

/// User settings for delta-based widgets.
class DeltaWidgetSettings {
  DeltaWidgetSettings(PreferencesBundle preferences)
      : bIsInResetMode = preferences.transient.get('colorTab.bIsInResetMode', defaultValue: false),
        sensitivity = preferences.persistent.getDouble('deltaWidgets.sensitivity', defaultValue: 1.0) {
    bIsInResetMode.listen((_) => _validateResetMode());
  }

  /// Whether the Unreal widgets should be displayed in reset mode.
  final TransientPreference<bool> bIsInResetMode;

  /// The base sensitivity of all delta-based widgets.
  final Preference<double> sensitivity;

  /// Map from owners of reset mode blocks to the message to display when blocked.
  final Map<Object, String> _resetModeBlockers = {};

  /// Check whether reset mode is blocked.
  bool get bIsResetModeBlocked => _resetModeBlockers.length > 0;

  /// Add a blocker for reset mode. The [owner] is responsible for removing it when appropriate.
  /// When a blocker is present and reset mode is on, we will force it off and show the user a [message] explaining why.
  void addResetModeBlocker(Object owner, String message) {
    if (_resetModeBlockers.containsKey(owner)) {
      _log.warning('$owner tried to add a reset mode block, but it already had one');
      return;
    }

    _resetModeBlockers[owner] = message;

    _validateResetMode();
  }

  /// Remove a blocker for reset mode.
  void removeResetModeBlocker(Object owner) {
    _resetModeBlockers.remove(owner);
  }

  /// Check whether [owner] owns a reset mode blocker which is currently active.
  bool isResetModeBlockedBy(Object owner) {
    return _resetModeBlockers.containsKey(owner);
  }

  /// Check whether reset mode can be set, and if not, turn it off.
  void _validateResetMode() {
    if (!bIsInResetMode.getValue() || !bIsResetModeBlocked) {
      return;
    }

    final BuildContext rootContext = rootNavigatorKey.currentContext!;
    final String combinedMessage =
        '${AppLocalizations.of(rootContext)!.resetModeDisabledMessage}\n\n' + _resetModeBlockers.values.join('\n\n');

    showDebugAlert(combinedMessage);

    WidgetsBinding.instance.addPostFrameCallback((_) {
      bIsInResetMode.setValue(false);
    });
  }
}
