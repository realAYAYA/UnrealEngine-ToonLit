// Copyright Epic Games, Inc. All Rights Reserved.

import 'dart:async';

import 'package:epic_common/preferences.dart';
import 'package:epic_common/theme.dart';
import 'package:epic_common/widgets.dart';
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../../models/settings/floating_tab_settings.dart';
import '../../models/settings/selected_actor_settings.dart';
import 'floating_tab_base.dart';
import 'lightcard_trackpad.dart';

/// A floating preview of the stage map that the user can drag to any corner of the screen and dock to the side.
class FloatingTrackpad extends StatefulWidget {
  const FloatingTrackpad({Key? key}) : super(key: key);

  static GlobalKey trackpad = GlobalKey(debugLabel: 'floating_trackpad');

  @override
  State<FloatingTrackpad> createState() => _FloatingTrackpadState();
}

class _FloatingTrackpadState extends State<FloatingTrackpad> with SingleTickerProviderStateMixin, FloatingTabBase {
  /// Whether trackpad is locked for LC positioning input or unlocked for trackpad positioning drag.
  bool _bIsTrackpadLocked = false;
  late final SelectedActorSettings _actorSettings;

  /// Instance of user base floating tab settings for trackpad.
  late final FloatingTabSettings _settings;

  /// Stream subscription for when the [tabController.dockState] changes.
  late final StreamSubscription _subscription;

  @override
  void initState() {
    super.initState();

    tabController = manager.trackpadTabController;

    initStationaryController(manager.mapTabController);

    tabController.ticker = createTicker(onTick);

    _actorSettings = Provider.of<SelectedActorSettings>(context, listen: false);

    _settings = FloatingTabSettings(PreferencesBundle.of(context), trackpadPreferencePrefix);

    /// init dragged tab relative position.
    tabRelativePosition =
        manager.getTabRelativePosition(tabController.tabPosition, stationaryTabController.tabPosition);

    /// This helps us listen for changes to our dockState, if this tab is ever undocked while [_bIsTrackpadLocked] is
    /// true, all drag and tap gestures would be ignored. to prevent this we want to know when the dock state changes
    /// and set [_bIsTrackpadLocked] to false.
    _subscription = _settings.bIsTabDocked.listen((value) => _handleOnDockStateChanged);
  }

  /// handle on dock state changed.
  void _handleOnDockStateChanged(bool value) {
    if (value && _bIsTrackpadLocked) {
      setState(() => _bIsTrackpadLocked = false);
    }
  }

  @override
  void dispose() {
    _subscription.cancel();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    tabAvoidanceOnFirstTimeRun();
    return ChangeNotifierProvider.value(
      value: tabController,
      builder: (context, child) {
        return FloatingTabBody(
          Icon(Icons.games_outlined),
          LightCardTrackPad(),
          bIsDraggable: !_bIsTrackpadLocked && _actorSettings.selectedActors.getValue().isNotEmpty,
          bCanInteract: _bIsTrackpadLocked,
          onTap: onTap,
          onPanStart: onStartDrag,
          onPanUpdate: _bIsTrackpadLocked ? null : updateDrag,
          onPanCancel: endDrag,
          onPanEnd: endDrag,
          children: [
            // Lock/unlock button which determines whether or not the trackpad is open modifying the position of a
            // selected lightcard.
            Visibility(
              visible: tabController.dockState == DockState.undocked,
              child: Padding(
                padding: const EdgeInsets.all(8.0),
                child: SizedBox(
                  width: 40,
                  height: 40,
                  child: EpicIconButton(
                    bIsToggledOn: _bIsTrackpadLocked,
                    activeBackgroundColor: UnrealColors.gray18.withOpacity(.1),
                    backgroundShape: BoxShape.circle,
                    iconPath: _bIsTrackpadLocked
                        ? 'packages/epic_common/assets/icons/lock_locked.svg'
                        : 'packages/epic_common/assets/icons/lock_unlocked.svg',
                    onPressed: () => setState(() {
                      _bIsTrackpadLocked = !_bIsTrackpadLocked;
                    }),
                  ),
                ),
              ),
            ),
          ],
        );
      },
    );
  }
}
