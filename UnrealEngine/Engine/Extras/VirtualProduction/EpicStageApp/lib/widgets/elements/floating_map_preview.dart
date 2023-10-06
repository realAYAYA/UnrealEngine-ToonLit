// Copyright Epic Games, Inc. All Rights Reserved.

import 'dart:math' as math;

import 'package:flutter/material.dart';
import 'package:flutter/physics.dart';
import 'package:flutter/scheduler.dart';

import '../../models/settings/floating_map_settings.dart';
import '../../utilities/preferences_bundle.dart';
import 'asset_icon.dart';
import 'lightcard_map.dart';

/// Sides of the screen on which the preview can be docked.
enum _DockSide {
  left,
  right,
}

/// Possible states for the floating preview in terms of docking.
enum _DockState {
  /// The preview is on-screen and no dock tabs are visible.
  undocked,

  /// The preview has entered the docking range and is revealing its tab.
  docking,

  /// The preview has revealed its docking tab. It may not actually be off-screen yet.
  docked,

  /// The preview has exited the docking range and is hiding its tab.
  undocking,
}

/// The smallest dimension of the container will be multiplied by this value to determine the height of the preview.
const double _previewSizeScale = 0.25;

/// The height of the preview will be multiplied by this value to determine its width.
const double _previewAspectRatio = 16.0 / 9.0;

/// How far to inset the preview from the edge of the screen.
const double _edgeInset = 8.0;

/// The size of the tab shown when the preview is docked.
const _dockTabSize = Size(32, 80);

/// A floating preview of the stage map that the user can drag to any corner of the screen and dock to the side.
class FloatingMapPreview extends StatefulWidget {
  const FloatingMapPreview({Key? key}) : super(key: key);

  @override
  State<FloatingMapPreview> createState() => _FloatingMapPreviewState();
}

class _FloatingMapPreviewState extends State<FloatingMapPreview> with SingleTickerProviderStateMixin {
  /// Physics settings for the spring simulation when the user is dragging the preview.
  static const _draggedSpringDescription = SpringDescription(mass: 40, stiffness: 1, damping: 1.2);

  /// Physics settings for the spring simulation when the user releases the preview.
  static const _releasedSpringDescription = SpringDescription(mass: 40, stiffness: 1, damping: 1);

  /// Physics settings for the spring simulation when the preview is being docked.
  static const _dockedSpringDescription = SpringDescription(mass: 60, stiffness: 1, damping: 1);

  /// When this portion of the preview's width is offscreen, dock it.
  static const double _dockThreshold = 1 / 3;

  /// Which side the preview was last docked on.
  _DockSide _dockSide = _DockSide.left;

  /// State of the preview window's docking.
  _DockState _dockState = _DockState.undocked;

  /// The position that the preview's top-left corner should move towards with spring motion.
  Offset _targetPreviewPosition = Offset.zero;

  /// The current position of the preview's top-left corner.
  Offset _previewPosition = Offset.zero;

  /// The current velocity of the preview.
  Offset _previewVelocity = Offset.zero;

  /// The user's last drag position in global space.
  Offset _lastDragPosition = Offset.zero;

  /// The last known size of the containing render box.
  Size _containerSize = Size.zero;

  /// The elapsed ticker time in seconds when the springs were last updated.
  double _lastSpringTime = 0;

  /// The last value of [_lastSpringTime] when the springs' target positions were moved.
  double _lastSpringMoveTime = 0;

  /// Spring controlling the X axis.
  SpringSimulation? _xSpring;

  /// Spring controlling the Y axis.
  SpringSimulation? _ySpring;

  /// Ticker used to update springs.
  late final Ticker _ticker;

  /// Settings for this preview map.
  late final FloatingMapSettings _settings;

  /// The size of the preview to display given the containing widget's size.
  Size get _previewSize {
    final Size screenSize = MediaQuery.of(context).size;
    final double previewHeight = math.min(screenSize.width, screenSize.height) * _previewSizeScale;

    return Size(previewHeight * _previewAspectRatio, previewHeight);
  }

  /// True if the preview is currently docked or in the process of docking.
  bool get _bIsDockedOrDocking => _dockState == _DockState.docking || _dockState == _DockState.docked;

  /// True if the preview is currently undocked or in the process of undocking.
  bool get _bIsUndockedOrUndocking => _dockState == _DockState.undocking || _dockState == _DockState.undocked;

  /// True if the preview is not docked and offscreen.
  bool get _bIsPreviewVisible {
    if (_dockState != _DockState.docked) {
      return true;
    }

    // Check if the preview is actually within visible bounds (e.g. still moving to docked position or being dragged out
    // of it).
    return ((_previewPosition.dx + _previewSize.width - _dockTabSize.width) > 0 &&
        (_previewPosition.dx + _dockTabSize.width) < _containerSize.width);
  }

  @override
  Widget build(BuildContext context) {
    return LayoutBuilder(builder: (BuildContext context, BoxConstraints constraints) {
      final Size newSize = Size(constraints.maxWidth, constraints.maxHeight);

      // Handle first size/resize
      if (newSize != _containerSize) {
        final Size oldSize = _containerSize;
        _containerSize = newSize;

        if (oldSize == Size.zero) {
          _initPosition();
        } else {
          WidgetsBinding.instance.addPostFrameCallback((_) => _onContainerResized(oldSize));
        }
      }

      // Determine the offset of the main preview window, which shifts to the side to reveal dock tabs when docked.
      double dockOffset = 0;
      if (_bIsDockedOrDocking) {
        switch (_dockSide) {
          case _DockSide.left:
            dockOffset = -_dockTabSize.width;
            break;

          case _DockSide.right:
            dockOffset = _dockTabSize.width;
            break;
        }
      }

      return Stack(
        clipBehavior: Clip.hardEdge,
        children: [
          // Position a box containing the preview, dock tabs, and gesture detector
          Positioned(
            top: _previewPosition.dy,
            left: _previewPosition.dx,
            child: SizedBox(
              width: _previewSize.width,
              height: _previewSize.height,
              child: Stack(
                clipBehavior: Clip.none,
                children: [
                  // Tab shown on the left when docked on the right
                  if (_dockState != _DockState.undocked && _dockSide == _DockSide.right)
                    Positioned(
                      key: Key('LeftDockTab'),
                      left: 0,
                      top: (_previewSize.height - _dockTabSize.height) / 2,
                      child: _DockTab(_DockSide.right),
                    ),

                  // Tab shown on the right when docked on the left
                  if (_dockState != _DockState.undocked && _dockSide == _DockSide.left)
                    Positioned(
                      key: Key('RightDockTab'),
                      left: _previewSize.width - _dockTabSize.width,
                      top: (_previewSize.height - _dockTabSize.height) / 2,
                      child: _DockTab(_DockSide.left),
                    ),

                  // Preview window, which shifts to reveal a tab when docked and hides when offscreen
                  AnimatedPositioned(
                    key: Key('PreviewMover'),
                    left: dockOffset,
                    duration: const Duration(milliseconds: 200),
                    child: _bIsPreviewVisible
                        ? _MapPreview(
                            key: Key('Preview'),
                            size: _previewSize,
                            bShowShadow: _bIsUndockedOrUndocking,
                          )
                        : SizedBox(),
                    onEnd: _onDockTabAnimationEnd,
                  ),

                  // Gesture detector, which matches the preview rectangle when undocked and the revealed tab when docked
                  Positioned(
                    key: Key('GestureDetector'),

                    // When docked to left, offset to line up with the tab on the right side of the preview
                    left: (_dockState == _DockState.docked && _dockSide == _DockSide.left)
                        ? (_previewSize.width - _dockTabSize.width)
                        : 0,

                    // When docked, offset to line up with the top of the tab
                    top: _dockState == _DockState.docked ? ((_previewSize.height - _dockTabSize.height) / 2) : 0,

                    child: SizedBox(
                      // When docked, match size to the tab
                      width: _dockState == _DockState.docked ? _dockTabSize.width : _previewSize.width,
                      height: _dockState == _DockState.docked ? _dockTabSize.height : _previewSize.height,

                      child: GestureDetector(
                        onTap: _onTap,
                        onPanUpdate: _updateDrag,
                        onPanEnd: (details) => _endDrag(details.velocity),
                        onPanCancel: () => _endDrag(Velocity.zero),
                      ),
                    ),
                  ),
                ],
              ),
            ),
          ),
        ],
      );
    });
  }

  @override
  void initState() {
    super.initState();

    _ticker = createTicker(_onTick);
    _settings = FloatingMapSettings(PreferencesBundle.of(context));
  }

  @override
  void dispose() {
    _ticker.dispose();

    super.dispose();
  }

  /// Initialize the position of the preview once we a valid size to reference.
  /// This should NOT call setState, as it's called directly from the first build().
  void _initPosition() {
    final restoredYPosition = _settings.mapY.getValue() * _containerSize.height;
    final PreviewMapSide mapSide = _settings.mapSide.getValue();

    if (_settings.bIsMapDocked.getValue()) {
      switch (mapSide) {
        case PreviewMapSide.left:
          _dockSide = _DockSide.left;
          break;

        case PreviewMapSide.right:
          _dockSide = _DockSide.right;
          break;
      }

      _dockState = _DockState.docked;
      _targetPreviewPosition = _previewPosition = _getDockedPreviewPosition(dockY: restoredYPosition);
    } else {
      // Make an approximate position for the preview, which we'll then snap to the nearest corner
      late final double tempXPosition;
      switch (mapSide) {
        case PreviewMapSide.left:
          tempXPosition = 0;
          break;

        case PreviewMapSide.right:
          tempXPosition = _containerSize.width - _previewSize.width;
          break;
      }

      final tempPosition = Offset(tempXPosition, restoredYPosition);

      _dockState = _DockState.undocked;
      _targetPreviewPosition = _previewPosition = _getNearestPreviewCornerPosition(tempPosition);
    }
  }

  /// Called when the containing widget is resized and we need to readjust accordingly.
  void _onContainerResized(Size oldContainerSize) {
    if (_containerSize.height == 0 || _containerSize.width == 0) {
      return;
    }

    final Offset scaledPosition = Offset(
      _targetPreviewPosition.dx * (_containerSize.width / oldContainerSize.width),
      _targetPreviewPosition.dy * (_containerSize.height / oldContainerSize.height),
    );

    if (_bIsDockedOrDocking) {
      // Stay docked and snap to the new side
      setState(() {
        _targetPreviewPosition = _previewPosition = _getDockedPreviewPosition(dockY: scaledPosition.dy);
      });
    } else {
      // Move smoothly to the new position
      _movePreviewTargetToNearestCorner(scaledPosition);
    }
  }

  /// Handle a tap on the preview.
  void _onTap() {
    if (_bIsUndockedOrUndocking) {
      return;
    }

    // Currently docked, so undock by moving to the nearest corner.
    _movePreviewTargetToNearestCorner(_targetPreviewPosition);
  }

  /// Update the dragged preview position.
  void _updateDrag(DragUpdateDetails details) {
    _lastDragPosition = details.globalPosition;

    _movePreviewTarget(
      _targetPreviewPosition + Offset(details.delta.dx, details.delta.dy),
      springDescription: _draggedSpringDescription,
    );

    final _DockSide? newDockSide = _checkDockSideAtPosition(_targetPreviewPosition);
    if (newDockSide != null) {
      // If this will dock, show the tab
      _dockPreview(newDockSide, moveOffscreen: false);
    } else {
      // No longer docked, so move to undocking state
      _undockPreview();
    }
  }

  /// Stop dragging the preview and let it come to rest.
  void _endDrag(Velocity velocity) {
    final RenderBox? renderBox = context.findRenderObject() as RenderBox?;
    if (renderBox == null) {
      _movePreviewTarget(
        Offset(_edgeInset, _edgeInset),
        springDescription: _releasedSpringDescription,
      );
      return;
    }

    // Check ahead from the preview's position so the user can "fling" the preview off the screen
    const dockCheckAheadSeconds = 0.1;
    final Offset dockCheckPosition = _previewPosition + velocity.pixelsPerSecond * dockCheckAheadSeconds;

    // If near the edges, dock the preview
    final _DockSide? newDockSide = _checkDockSideAtPosition(dockCheckPosition);
    if (newDockSide != null) {
      _dockPreview(newDockSide, dockY: dockCheckPosition.dy);
      return;
    }

    // Otherwise, move to the nearest corner. In this case, use the user's drag position instead of the preview
    // position since flinging feels more accurate that way.
    const cornerCheckAheadSeconds = 0.15;
    final Offset cornerCheckPosition = _lastDragPosition + velocity.pixelsPerSecond * cornerCheckAheadSeconds;
    final Offset finalLocalPosition = renderBox.globalToLocal(cornerCheckPosition);

    _movePreviewTargetToNearestCorner(finalLocalPosition);
  }

  /// Update spring preview position using springs.
  void _onTick(Duration elapsed) {
    if (_xSpring == null || _ySpring == null) {
      return;
    }

    _lastSpringTime = elapsed.inMilliseconds / 1000.0;

    final double springTime = _lastSpringTime - _lastSpringMoveTime;

    setState(() {
      _previewVelocity = Offset(_xSpring!.dx(springTime), _ySpring!.dx(springTime));
      _previewPosition = Offset(_xSpring!.x(springTime), _ySpring!.x(springTime));
    });

    // If both springs are at rest, stop ticking to save performance
    if (_xSpring!.isDone(springTime) && _ySpring!.isDone(springTime)) {
      _ticker.stop();
      _lastSpringMoveTime = 0;
      _lastSpringTime = 0;
    }
  }

  /// Called when the animation to reveal/hide a dock tab completes.
  void _onDockTabAnimationEnd() {
    switch (_dockState) {
      case _DockState.undocking:
        setState(() {
          _dockState = _DockState.undocked;
        });
        break;

      case _DockState.docking:
        setState(() {
          _dockState = _DockState.docked;
        });
        break;

      default:
        break;
    }
  }

  /// Move the target position for the preview to a new location and adjust springs accordingly.
  void _movePreviewTarget(Offset newTarget, {required SpringDescription springDescription}) {
    // Clamp both axes in a valid range
    _targetPreviewPosition = Offset(
      newTarget.dx.clamp(-_previewSize.width + _dockTabSize.width, _containerSize.width - _dockTabSize.width),
      _getClampedPreviewYPosition(newTarget.dy),
    );

    _xSpring = SpringSimulation(
      springDescription,
      _previewPosition.dx,
      _targetPreviewPosition.dx,
      _previewVelocity.dx,
    );

    _ySpring = SpringSimulation(
      springDescription,
      _previewPosition.dy,
      _targetPreviewPosition.dy,
      _previewVelocity.dy,
    );

    _lastSpringMoveTime = _lastSpringTime;

    if (!_ticker.isActive) {
      _ticker.start();
    }

    _updateSettingsPreviewMapY(newTarget.dy);
  }

  /// Move the preview target to the corner nearest to a local [position].
  void _movePreviewTargetToNearestCorner(Offset position) {
    _movePreviewTarget(
      _getNearestPreviewCornerPosition(position),
      springDescription: _releasedSpringDescription,
    );

    _settings.mapSide.setValue(
      (_targetPreviewPosition.dx < _containerSize.width / 2) ? PreviewMapSide.left : PreviewMapSide.right,
    );

    _undockPreview();
  }

  /// Reveal the dock tab and move to the docked state to the given [side].
  /// If [moveOffscreen] is true, move the preview off-screen, optionally using the Y position specified by [dockY].
  void _dockPreview(_DockSide side, {bool moveOffscreen = true, double? dockY}) {
    dockY = dockY ?? _previewPosition.dy;

    if (_bIsUndockedOrUndocking) {
      // Trigger the docking animation
      setState(() {
        _dockSide = side;
        _dockState = _DockState.docking;
      });

      // Update user settings
      switch (side) {
        case _DockSide.left:
          _settings.mapSide.setValue(PreviewMapSide.left);
          break;

        case _DockSide.right:
          _settings.mapSide.setValue(PreviewMapSide.right);
          break;

        default:
          break;
      }

      _settings.bIsMapDocked.setValue(true);
      _updateSettingsPreviewMapY(dockY);
    }

    if (!moveOffscreen) {
      return;
    }

    _movePreviewTarget(
      _getDockedPreviewPosition(dockY: dockY),
      springDescription: _dockedSpringDescription,
    );
  }

  /// Hide the docked tab and move to the undocking state.
  void _undockPreview() {
    if (_bIsDockedOrDocking) {
      setState(() {
        _dockState = _DockState.undocking;
      });

      _settings.bIsMapDocked.setValue(false);
    }
  }

  /// Check whether the preview should be docked if it's at the given X position.
  /// Returns the side to dock to, or null if it shouldn't dock.
  _DockSide? _checkDockSideAtPosition(Offset position) {
    if (position.dx + ((1 - _dockThreshold) * _previewSize.width) > _containerSize.width) {
      return _DockSide.right;
    }

    if (position.dx + (_dockThreshold * _previewSize.width) < 0) {
      return _DockSide.left;
    }

    return null;
  }

  /// Update the saved Y position of the docked preview.
  void _updateSettingsPreviewMapY(double dockY) {
    _settings.mapY.setValue(dockY / _containerSize.height);
  }

  /// Clamp a Y position for the preview to a valid range.
  double _getClampedPreviewYPosition(double y) {
    final double minY = _edgeInset;
    final double maxY = _containerSize.height - _previewSize.height - _edgeInset;

    return math.max(math.min(y, maxY), minY);
  }

  /// Get the position of the preview when moved to the nearest corner to a local [position].
  Offset _getNearestPreviewCornerPosition(Offset position) {
    late final double targetX;
    if (position.dx < _containerSize.width / 2) {
      targetX = _edgeInset;
    } else {
      targetX = _containerSize.width - _previewSize.width - _edgeInset;
    }

    late final double targetY;
    if (position.dy < _containerSize.height / 2) {
      targetY = _edgeInset;
    } else {
      targetY = _containerSize.height - _previewSize.height - _edgeInset;
    }

    return Offset(targetX, targetY);
  }

  /// Get the docked position of the preview when its Y position is [dockY].
  Offset _getDockedPreviewPosition({required double dockY}) {
    // Extra amount to move just to make sure it's fully off-screen so we can stop rendering the preview
    const double offsetFudge = 0.5;

    // Move the preview to the appropriate side
    late final double targetX;
    switch (_dockSide) {
      case _DockSide.left:
        targetX = -_previewSize.width + _dockTabSize.width - offsetFudge;
        break;

      case _DockSide.right:
        targetX = _containerSize.width - _dockTabSize.width + offsetFudge;
        break;
    }

    return Offset(targetX, _getClampedPreviewYPosition(dockY));
  }
}

/// The static map preview which moves around within the larger widget.
class _MapPreview extends StatelessWidget {
  const _MapPreview({Key? key, required this.size, required this.bShowShadow}) : super(key: key);

  final Size size;
  final bool bShowShadow;

  @override
  Widget build(BuildContext context) {
    final borderRadius = BorderRadius.circular(6);

    return SizedBox(
      width: size.width,
      height: size.height,
      child: Stack(
        children: [
          AnimatedContainer(
            clipBehavior: Clip.antiAlias,
            duration: Duration(milliseconds: 400),
            decoration: BoxDecoration(
              borderRadius: borderRadius,
              boxShadow: [
                BoxShadow(
                  color: Color(0xff000000).withOpacity(bShowShadow ? 0.4 : 0),
                  spreadRadius: 2,
                  blurRadius: 5,
                ),
              ],
            ),
            child: StageMap(
              bIsControllable: false,
              bShowOnlySelectedPins: true,
              bForceShowFullMap: true,
              pinSize: 32,
            ),
          ),
          Container(
            decoration: BoxDecoration(
              borderRadius: borderRadius,
              border: Border.all(
                color: Color(0x50ffffff),
                width: 0.5,
              ),
            ),
          ),
        ],
      ),
    );
  }
}

/// The tab shown when the preview is docked.
class _DockTab extends StatelessWidget {
  const _DockTab(this.side, {Key? key}) : super(key: key);

  /// Which side the preview is docked to.
  final _DockSide side;

  @override
  Widget build(BuildContext context) {
    const Radius borderRadius = Radius.circular(8);
    final bool bIsFacingLeft = side == _DockSide.right;

    return Container(
      width: _dockTabSize.width,
      height: _dockTabSize.height,
      decoration: BoxDecoration(
        color: Theme.of(context).colorScheme.surfaceTint,
        borderRadius: bIsFacingLeft
            ? BorderRadius.only(
                topLeft: borderRadius,
                bottomLeft: borderRadius,
              )
            : BorderRadius.only(
                topRight: borderRadius,
                bottomRight: borderRadius,
              ),
        boxShadow: [
          BoxShadow(
            color: Color(0x40000000),
            offset: Offset(bIsFacingLeft ? -4 : 4, 4),
            blurRadius: 4,
          ),
        ],
      ),
      child: Column(mainAxisAlignment: MainAxisAlignment.center, children: [
        AssetIcon(
          path: 'assets/images/icons/map.svg',
          size: 24,
        ),
        SizedBox(
          height: 6,
        ),
        AssetIcon(
          path: bIsFacingLeft ? 'assets/images/icons/chevron_left.svg' : 'assets/images/icons/chevron_right.svg',
          size: 24,
        ),
      ]),
    );
  }
}
