// Copyright Epic Games, Inc. All Rights Reserved.

import 'dart:math' as math;
import 'package:epic_common/preferences.dart';
import 'package:flutter/material.dart';
import 'package:flutter/physics.dart';
import 'package:flutter/scheduler.dart';

import '../../models/navigator_keys.dart';
import '../../models/settings/floating_tab_settings.dart';
import '../widgets/elements/floating_tab_base.dart';

/// Simple manager for dockable tabs.
class UnrealDockableTabManager {
  UnrealDockableTabManager(BuildContext context) {
    mapTabController = UnrealFloatingTabController(mapPreferencePrefix);
    trackpadTabController = UnrealFloatingTabController(trackpadPreferencePrefix);
  }

  /// Controller for dockable map tab.
  late final UnrealFloatingTabController mapTabController;

  /// Controller for dockable trackpad tab.
  late final UnrealFloatingTabController trackpadTabController;

  /// Get relative tab position
  TabPosition getTabRelativePosition(Offset draggedTabPosition, Offset stationaryTabPosition) {
    return draggedTabPosition.dy > stationaryTabPosition.dy ? TabPosition.below : TabPosition.above;
  }

  void dispose() {
    mapTabController.dispose();
    trackpadTabController.dispose();
  }
}

/// Controller for a Dockable tab, a string [persistentPrefix] for prefixing state stored on our persistent store is
/// required.
class UnrealFloatingTabController with ChangeNotifier {
  /// prefix for controllers used when persisting state
  final String persistentPrefix;

  UnrealFloatingTabController(this.persistentPrefix);

  /// Instance of tab settings.
  FloatingTabSettings get _settings => FloatingTabSettings(
        PreferencesBundle.of(rootNavigatorKey.currentContext!),
        persistentPrefix,
      );

  /// Physics settings for the spring simulation when the user is dragging the tab.
  final draggedSpringDescription = SpringDescription(mass: 40, stiffness: 1, damping: 1.2);

  /// Physics settings for the spring simulation when the user releases the tab.
  final releasedSpringDescription = SpringDescription(mass: 40, stiffness: 1, damping: 1);

  /// Physics settings for the spring simulation when the tab is being docked.
  final dockedSpringDescription = SpringDescription(mass: 60, stiffness: 1, damping: 1);

  /// When this portion of the tab's width is offscreen, dock it.
  static const double _dockThreshold = 1 / 3;

  /// Which side the tab was last docked on.
  DockSide dockSide = DockSide.left;

  /// State of the tab window's docking.
  DockState dockState = DockState.undocked;

  /// The position that the tab's top-left corner should move towards with spring motion.
  Offset targetTabPosition = Offset.zero;

  /// The current position of the tab's top-left corner.
  Offset tabPosition = Offset.zero;

  /// The current velocity of the tab.
  Offset tabVelocity = Offset.zero;

  /// The user's last drag position in global space.
  Offset lastDragPosition = Offset.zero;

  /// The last known size of the containing render box.
  Size containerSize = Size.zero;

  /// The elapsed ticker time in seconds when the springs were last updated.
  double lastSpringTime = 0;

  /// The last value of [lastSpringTime] when the springs' target positions were moved.
  double lastSpringMoveTime = 0;

  /// Spring controlling the X axis.
  SpringSimulation? xSpring;

  /// Spring controlling the Y axis.
  SpringSimulation? ySpring;

  /// Ticker used to update springs.
  Ticker? ticker;

  /// The size of the tab to display given the containing widget's size.
  Size get tabSize {
    final Size screenSize = MediaQuery.of(rootNavigatorKey.currentContext!).size;
    final double previewHeight = math.min(screenSize.width, screenSize.height) * tabSizeScale;

    return Size(previewHeight * tabAspectRatio, previewHeight);
  }

  /// True if the tab is currently docked or in the process of docking.
  bool get bIsDockedOrDocking => dockState == DockState.docking || dockState == DockState.docked;

  /// True if the tab is currently undocked or in the process of undocking.
  bool get bIsUndockedOrUndocking => dockState == DockState.undocking || dockState == DockState.undocked;

  /// True if the tab is not docked and offscreen.
  bool get bIsTabVisible {
    if (dockState != DockState.docked) {
      return true;
    }

    // Check if the tab is actually within visible bounds (e.g. still moving to docked position or being dragged out
    // of it).
    return ((tabPosition.dx + tabSize.width - dockTabSize.width) > 0 &&
        (tabPosition.dx + dockTabSize.width) < containerSize.width);
  }

  /// Initialize the position of the tab once we a valid size to reference.
  /// This should NOT call setState, as it's called directly from the first build().
  void initPosition() {
    final restoredYPosition = _settings.yAxis.getValue() * containerSize.height;
    final TabCurrentSide mapSide = _settings.tabSide.getValue();

    if (_settings.bIsTabDocked.getValue()) {
      switch (mapSide) {
        case TabCurrentSide.left:
          dockSide = DockSide.left;
          break;

        case TabCurrentSide.right:
          dockSide = DockSide.right;
          break;
      }

      dockState = DockState.docked;
      targetTabPosition = tabPosition = getDockedTabPosition(dockY: restoredYPosition);
    } else {
      // Make an approximate position for the tab, which we'll then snap to the nearest corner
      late final double tempXPosition;
      switch (mapSide) {
        case TabCurrentSide.left:
          tempXPosition = 0;
          break;

        case TabCurrentSide.right:
          tempXPosition = containerSize.width - tabSize.width;
          break;
      }

      final tempPosition = Offset(tempXPosition, restoredYPosition);

      dockState = DockState.undocked;
      targetTabPosition = tabPosition = getTabNearestCornerPosition(tempPosition);
    }
  }

  /// Called when the containing widget is resized and we need to readjust accordingly.
  void onContainerResized(Size oldContainerSize) {
    if (containerSize.height == 0 || containerSize.width == 0) {
      return;
    }

    final Offset scaledPosition = Offset(
      targetTabPosition.dx * (containerSize.width / oldContainerSize.width),
      targetTabPosition.dy * (containerSize.height / oldContainerSize.height),
    );

    if (bIsDockedOrDocking) {
      // Stay docked and snap to the new side
      targetTabPosition = tabPosition = getDockedTabPosition(dockY: scaledPosition.dy);
      notifyListeners();
    } else {
      // Move smoothly to the new position
      moveTabToNearestCorner(scaledPosition);
    }
  }

  /// Update spring tab position using springs.
  void onTick(Duration elapsed) {
    if (xSpring == null || ySpring == null) {
      return;
    }

    lastSpringTime = elapsed.inMilliseconds / 1000.0;

    final double springTime = lastSpringTime - lastSpringMoveTime;

    tabVelocity = Offset(xSpring!.dx(springTime), ySpring!.dx(springTime));
    tabPosition = Offset(xSpring!.x(springTime), ySpring!.x(springTime));
    notifyListeners();

    // If both springs are at rest, stop ticking to save performance
    if (xSpring!.isDone(springTime) && ySpring!.isDone(springTime)) {
      ticker?.stop();
      lastSpringMoveTime = 0;
      lastSpringTime = 0;
    }
  }

  /// Called when the animation to reveal/hide a dock tab completes.
  void onDockTabAnimationEnd() {
    switch (dockState) {
      case DockState.undocking:
        dockState = DockState.undocked;
        notifyListeners();
        break;

      case DockState.docking:
        dockState = DockState.docked;
        notifyListeners();
        break;

      default:
        break;
    }
  }

  /// Move the target position for the tab to a new location and adjust springs accordingly.
  void moveTab(Offset newTarget, {required SpringDescription springDescription}) {
    // Clamp both axes in a valid range
    targetTabPosition = Offset(
      newTarget.dx.clamp(-tabSize.width + dockTabSize.width, containerSize.width - dockTabSize.width),
      getClampedTabYPosition(newTarget.dy),
    );

    xSpring = SpringSimulation(
      springDescription,
      tabPosition.dx,
      targetTabPosition.dx,
      tabVelocity.dx,
    );

    ySpring = SpringSimulation(
      springDescription,
      tabPosition.dy,
      targetTabPosition.dy,
      tabVelocity.dy,
    );

    lastSpringMoveTime = lastSpringTime;

    if (ticker?.isActive == false) {
      ticker?.start();
    }

    _updateTabSettingsYAxis(newTarget.dy);
  }

  /// Move the tab target to the corner nearest to a local [position].
  void moveTabToNearestCorner(Offset position) {
    moveTab(
      getTabNearestCornerPosition(position),
      springDescription: releasedSpringDescription,
    );

    _settings.tabSide.setValue(
      (targetTabPosition.dx < containerSize.width / 2) ? TabCurrentSide.left : TabCurrentSide.right,
    );

    undockTab();
  }

  /// Move the tab to the corner farthest to a local [position].
  void moveTabToFarthestCorner(Offset position) {
    dockTab(side, dockY: getFarthestCornerPosition(position).dy);
    _settings.tabSide
        .setValue((targetTabPosition.dx < containerSize.width / 2) ? TabCurrentSide.left : TabCurrentSide.right);
  }

  /// Reveal the dock tab and move to the docked state to the given [side].
  /// If [moveOffscreen] is true, move the tab off-screen, optionally using the Y position specified by [dockY].
  void dockTab(DockSide side, {bool moveOffscreen = true, double? dockY}) {
    dockY = dockY ?? tabPosition.dy;

    if (bIsUndockedOrUndocking) {
      // Trigger the docking animation
      dockSide = side;
      dockState = DockState.docking;

      notifyListeners();

      // Update user settings
      switch (side) {
        case DockSide.left:
          _settings.tabSide.setValue(TabCurrentSide.left);
          break;

        case DockSide.right:
          _settings.tabSide.setValue(TabCurrentSide.right);
          break;

        default:
          break;
      }

      _settings.bIsTabDocked.setValue(true);
      _updateTabSettingsYAxis(dockY);
    }

    if (!moveOffscreen) {
      return;
    }

    moveTab(
      getDockedTabPosition(dockY: dockY),
      springDescription: dockedSpringDescription,
    );
  }

  /// Hide the docked tab and move to the undocking state.
  void undockTab() {
    if (bIsDockedOrDocking) {
      dockState = DockState.undocking;
      notifyListeners();

      _settings.bIsTabDocked.setValue(false);
    }
  }

  /// Check whether the tab should be docked if it's at the given X position.
  /// Returns the side to dock to, or null if it shouldn't dock.
  DockSide? checkDockSideAtPosition(Offset position) {
    if (position.dx + ((1 - _dockThreshold) * tabSize.width) > containerSize.width) {
      return DockSide.right;
    }

    if (position.dx + (_dockThreshold * tabSize.width) < 0) {
      return DockSide.left;
    }

    return null;
  }

  /// Update the saved Y position of the docked tab.
  void _updateTabSettingsYAxis(double dockY) {
    if (containerSize.height != 0) {
      _settings.yAxis.setValue(dockY / containerSize.height);
    }
  }

  /// Clamp a Y position for the tab to a valid range.
  double getClampedTabYPosition(double y) {
    final double minY = edgeInset;
    final double maxY = containerSize.height - tabSize.height - edgeInset;

    return math.max(math.min(y, maxY), minY);
  }

  /// Get the position of the tab when moved to the nearest corner to a local [position].
  Offset getTabNearestCornerPosition(Offset position) {
    late final double targetX;
    if (position.dx < containerSize.width / 2) {
      targetX = edgeInset;
    } else {
      targetX = containerSize.width - tabSize.width - edgeInset;
    }

    late final double targetY;
    if (position.dy < containerSize.height / 2) {
      targetY = edgeInset;
    } else {
      targetY = containerSize.height - tabSize.height - edgeInset;
    }

    return Offset(targetX, targetY);
  }

  /// Get the position of the tab when moved to the farthest corner to a local [position].
  Offset getFarthestCornerPosition(Offset position) {
    late final double targetX;
    if (position.dx < containerSize.width / 2) {
      targetX = edgeInset;
    } else {
      targetX = containerSize.width + tabSize.width - edgeInset;
    }

    late final double targetY;
    if (position.dy < containerSize.height / 2) {
      targetY = containerSize.height + tabSize.height - edgeInset;
    } else {
      targetY = edgeInset;
    }

    return Offset(targetX, targetY);
  }

  /// Get the docked position of the tab when its Y position is [dockY].
  Offset getDockedTabPosition({required double dockY}) {
    // Extra amount to move just to make sure it's fully off-screen so we can stop rendering the tab
    const double offsetFudge = 0.5;

    // Move the tab to the appropriate side
    late final double targetX;
    switch (dockSide) {
      case DockSide.left:
        targetX = -tabSize.width + dockTabSize.width - offsetFudge;
        break;

      case DockSide.right:
        targetX = containerSize.width - dockTabSize.width + offsetFudge;
        break;
    }

    return Offset(targetX, getClampedTabYPosition(dockY));
  }

  ///Get current side of the screen where tab is currently seated, whether docked or undocked;
  DockSide get side => tabPosition.dx > containerSize.width / 2 ? DockSide.right : DockSide.left;
}
