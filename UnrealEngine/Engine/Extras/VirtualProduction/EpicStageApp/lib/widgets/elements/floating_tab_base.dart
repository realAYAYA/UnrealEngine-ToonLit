// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:epic_common/widgets.dart';
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../../models/unreal_dockable_tab_manager.dart';

/// Sides of the screen on which the tab can be docked.
enum DockSide {
  left,
  right,
}

/// Tabs Relative position.
enum TabPosition { above, below }

/// Possible states for the floating tab in terms of docking.
enum DockState {
  /// The tab is on-screen and no dock tabs are visible.
  undocked,

  /// The tab has entered the docking range and is revealing its tab.
  docking,

  /// The tab has revealed its docking tab. It may not actually be off-screen yet.
  docked,

  /// The tab has exited the docking range and is hiding its tab.
  undocking,
}

/// The smallest dimension of the container will be multiplied by this value to determine the height of the tab.
const double tabSizeScale = 0.25;

/// The height of the tab will be multiplied by this value to determine its width.
const double tabAspectRatio = 16.0 / 9.0;

/// How far to inset the tab from the edge of the screen.
const double edgeInset = 8.0;

/// The size of the tab shown when the tab is docked.
const dockTabSize = Size(32, 80);

/// Map persistence prefix.
const String mapPreferencePrefix = 'floatingMapSettings';

/// Trackpad persistence prefix.
const String trackpadPreferencePrefix = 'floatingTrackpadSettings';

/// The visible part of the tab shown when it is docked (controller).
class DockTab extends StatelessWidget {
  const DockTab(this.side, this.indicator, {Key? key}) : super(key: key);

  /// Which side the preview is docked to.
  final DockSide side;

  /// Indicator widget showing what the tab contains.
  final Widget indicator;

  @override
  Widget build(BuildContext context) {
    const Radius borderRadius = Radius.circular(8);
    final bool bIsFacingLeft = side == DockSide.right;

    return Container(
      width: dockTabSize.width,
      height: dockTabSize.height,
      decoration: BoxDecoration(
        color: Theme.of(context).colorScheme.surfaceTint,
        borderRadius: bIsFacingLeft
            ? BorderRadius.only(topLeft: borderRadius, bottomLeft: borderRadius)
            : BorderRadius.only(topRight: borderRadius, bottomRight: borderRadius),
        boxShadow: [
          BoxShadow(
            color: Color(0x40000000),
            offset: Offset(bIsFacingLeft ? -4 : 4, 4),
            blurRadius: 4,
          ),
        ],
      ),
      child: Column(mainAxisAlignment: MainAxisAlignment.center, children: [
        indicator,
        SizedBox(height: 6),
        AssetIcon(
          path: bIsFacingLeft
              ? 'packages/epic_common/assets/icons/chevron_left.svg'
              : 'packages/epic_common/assets/icons/chevron_right.svg',
          size: 24,
        ),
      ]),
    );
  }
}

/// The static tab container which moves around within the larger widget.
class FloatingTabContainer extends StatelessWidget {
  const FloatingTabContainer({
    Key? key,
    required this.size,
    required this.bShowShadow,
    required this.child,
    this.bUseStackedLayout = false,
  }) : super(key: key);

  /// size of the static container.
  final Size size;

  /// Whether the container has a shadow or not.
  final bool bShowShadow;

  /// Content of the container.
  final Widget child;

  /// Whether we want to use a stacked layout a more simpler layout.
  final bool bUseStackedLayout;

  @override
  Widget build(BuildContext context) {
    final borderRadius = BorderRadius.circular(6);

    final Widget content = AnimatedContainer(
      clipBehavior: Clip.antiAlias,
      duration: Duration(milliseconds: 400),
      decoration: BoxDecoration(
        borderRadius: borderRadius,
        border: Border.all(
          color: Color(0x50ffffff),
          width: 0.5,
        ),
        boxShadow: [
          BoxShadow(
            color: Color(0xff000000).withOpacity(bShowShadow ? 0.4 : 0),
            spreadRadius: 2,
            blurRadius: 5,
          ),
        ],
      ),
      child: child,
    );

    return SizedBox(
      width: size.width,
      height: size.height,
      child: bUseStackedLayout
          ? Stack(
              children: [
                content,
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
            )
          : content,
    );
  }
}

/// Container for tab controls & content
class FloatingTabBody extends StatelessWidget {
  const FloatingTabBody(
    this.icon,
    this.child, {
    super.key,
    this.children = const [],
    this.bIsDraggable = false,
    this.bShouldUseStackLayout = false,
    this.bCanInteract = false,
    this.onTap,
    this.onPanStart,
    this.onPanUpdate,
    this.onPanEnd,
    this.onPanCancel,
  });

  /// List of widgets to be stacked above the [child] (main content) of the tab.
  final List<Widget> children;

  /// tab indicator icon.
  final Widget icon;

  /// whether or not the tab's position can be changed via dragging.
  final bool bIsDraggable;

  /// whether or not the tabs main content can be interacted with.
  final bool bCanInteract;

  /// main content of the tab.
  final Widget child;

  /// whether to use a stack layout in the [FloatingTabContainer] or not.
  final bool bShouldUseStackLayout;

  /// handles tap gestures on the tab body.
  final VoidCallback? onTap;

  /// handles pan start gestures.
  final Function(DragStartDetails)? onPanStart;

  /// handles pan update gestures.
  final Function(DragUpdateDetails)? onPanUpdate;

  /// handles pan end gestures.
  final Function(Velocity)? onPanEnd;

  /// handles pan cancel gestures.
  final Function(Velocity)? onPanCancel;

  @override
  Widget build(BuildContext context) {
    return LayoutBuilder(builder: (context, BoxConstraints constraints) {
      final Size newSize = Size(constraints.maxWidth, constraints.maxHeight);

      final _tabController = context.watch<UnrealFloatingTabController>();

      // Handle first size/resize
      if (newSize != _tabController.containerSize) {
        final Size oldSize = _tabController.containerSize;
        _tabController.containerSize = newSize;

        if (oldSize == Size.zero) {
          _tabController.initPosition();
        } else {
          WidgetsBinding.instance.addPostFrameCallback((_) => _tabController.onContainerResized(oldSize));
        }
      }

      // Determine the offset of the main preview window, which shifts to the side to reveal dock tabs when docked.
      double dockOffset = 0;
      if (_tabController.bIsDockedOrDocking) {
        switch (_tabController.dockSide) {
          case DockSide.left:
            dockOffset = -dockTabSize.width;
            break;

          case DockSide.right:
            dockOffset = dockTabSize.width;
            break;
        }
      }

      return Stack(
        clipBehavior: Clip.hardEdge,
        children: [
          // Position a box containing the preview, dock tabs, and gesture detector
          Positioned(
            top: _tabController.tabPosition.dy,
            left: _tabController.tabPosition.dx,
            child: SizedBox(
              width: _tabController.tabSize.width,
              height: _tabController.tabSize.height,
              child: Stack(
                clipBehavior: Clip.none,
                children: [
                  // Tab shown on the left when docked on the right
                  if (_tabController.dockState != DockState.undocked && _tabController.dockSide == DockSide.right)
                    Positioned(
                      key: Key('LeftDockTab'),
                      left: 0,
                      top: (_tabController.tabSize.height - dockTabSize.height) / 2,
                      child: DockTab(DockSide.right, icon),
                    ),

                  // Tab shown on the right when docked on the left
                  if (_tabController.dockState != DockState.undocked && _tabController.dockSide == DockSide.left)
                    Positioned(
                      key: Key('RightDockTab'),
                      left: _tabController.tabSize.width - dockTabSize.width,
                      top: (_tabController.tabSize.height - dockTabSize.height) / 2,
                      child: DockTab(DockSide.left, icon),
                    ),

                  // Preview window, which shifts to reveal tab when docked and hides when offscreen
                  AnimatedPositioned(
                    left: dockOffset,
                    duration: const Duration(milliseconds: 200),
                    child: _tabController.bIsTabVisible
                        ? IgnorePointer(
                            ignoring: bIsDraggable,
                            child: FloatingTabContainer(
                              size: _tabController.tabSize,
                              bShowShadow: _tabController.bIsUndockedOrUndocking,
                              child: child,
                              bUseStackedLayout: bShouldUseStackLayout,
                            ),
                          )
                        : SizedBox.shrink(),
                    onEnd: _tabController.onDockTabAnimationEnd,
                  ),

                  // Gesture detector, which matches the preview rectangle when undocked and the revealed tab when docked
                  Positioned(
                    // When docked to left, offset to line up with the tab on the right side of the preview
                    left: (_tabController.dockState == DockState.docked && _tabController.dockSide == DockSide.left)
                        ? (_tabController.tabSize.width - dockTabSize.width)
                        : 0,
                    // When docked, offset to line up with the top of the tab
                    top: _tabController.dockState == DockState.docked
                        ? ((_tabController.tabSize.height - dockTabSize.height) / 2)
                        : 0,
                    child: SizedBox(
                      // When docked, match size to the tab
                      width: _tabController.dockState == DockState.docked
                          ? dockTabSize.width
                          : _tabController.tabSize.width,
                      height: _tabController.dockState == DockState.docked
                          ? dockTabSize.height
                          : _tabController.tabSize.height,

                      child: IgnorePointer(
                        ignoring: bCanInteract,
                        child: GestureDetector(
                          onTap: onTap,
                          onPanStart: onPanStart,
                          onPanUpdate: onPanUpdate,
                          onPanEnd: (details) => onPanEnd?.call(details.velocity),
                          onPanCancel: () => onPanCancel?.call(Velocity.zero),
                        ),
                      ),
                    ),
                  ),

                  ...children.map((e) => e).toList(),
                ],
              ),
            ),
          ),
        ],
      );
    });
  }
}

/// Base mixin to manage state of a floating tab.
mixin FloatingTabBase<T extends StatefulWidget> on State<T> {
  /// An instance of unreal tab manager.
  late final UnrealDockableTabManager manager;

  late final UnrealFloatingTabController tabController;
  late final UnrealFloatingTabController stationaryTabController;

  /// Relative position of the tab.
  TabPosition tabRelativePosition = TabPosition.above;

  @override
  void initState() {
    manager = Provider.of<UnrealDockableTabManager>(context, listen: false);
    super.initState();
  }

  /// To be called from the [initState] of our user class, allows us to instantiate out stationary tab.
  void initStationaryController(UnrealFloatingTabController controller) {
    stationaryTabController = controller;
  }

  /// init tab controller should be called once but within the [initState] of our user class.
  void initTab(String preferencePrefix) {
    tabController = UnrealFloatingTabController(preferencePrefix);
  }

  /// Handle a tap on the preview.
  void onTap() {
    if (tabController.bIsUndockedOrUndocking) {
      return;
    }

    // Currently docked, so undock by moving to the nearest corner.
    tabController.moveTabToNearestCorner(tabController.targetTabPosition);
    _moveStationaryTab();
  }

  /// Update the dragged preview position.
  void updateDrag(DragUpdateDetails details) {
    tabController.lastDragPosition = details.globalPosition;

    tabController.moveTab(
      tabController.targetTabPosition + Offset(details.delta.dx, details.delta.dy),
      springDescription: tabController.draggedSpringDescription,
    );

    final DockSide? newDockSide = tabController.checkDockSideAtPosition(tabController.targetTabPosition);
    if (newDockSide != null) {
      // If this will dock, show the tab
      tabController.dockTab(newDockSide, moveOffscreen: false);
    } else {
      // No longer docked, so move to undocking state
      tabController.undockTab();
    }
  }

  /// Stop dragging the preview and let it come to rest.
  void endDrag(Velocity velocity) {
    final RenderBox? renderBox = context.findRenderObject() as RenderBox?;
    if (renderBox == null) {
      tabController.moveTab(
        Offset(edgeInset, edgeInset),
        springDescription: tabController.releasedSpringDescription,
      );
      return;
    }

    // Check ahead from the preview's position so the user can "fling" the preview off the screen
    const dockCheckAheadSeconds = 0.1;
    final Offset dockCheckPosition = tabController.tabPosition + velocity.pixelsPerSecond * dockCheckAheadSeconds;

    // If near the edges, dock the preview
    final DockSide? newDockSide = tabController.checkDockSideAtPosition(dockCheckPosition);
    if (newDockSide != null) {
      tabController.dockTab(newDockSide, dockY: dockCheckPosition.dy);
      _moveStationaryTab(dockCheckPosition);
      return;
    }

    // Otherwise, move to the nearest corner. In this case, use the user's drag position instead of the preview
    // position since flinging feels more accurate that way.
    const cornerCheckAheadSeconds = 0.15;
    final Offset cornerCheckPosition =
        tabController.lastDragPosition + velocity.pixelsPerSecond * cornerCheckAheadSeconds;
    final Offset finalLocalPosition = renderBox.globalToLocal(cornerCheckPosition);

    tabController.moveTabToNearestCorner(finalLocalPosition);
    _moveStationaryTab(finalLocalPosition);
  }

  /// Update spring preview position using springs.
  void onTick(Duration elapsed) {
    if (tabController.xSpring == null || tabController.ySpring == null) {
      return;
    }

    tabController.lastSpringTime = elapsed.inMilliseconds / 1000.0;

    final double springTime = tabController.lastSpringTime - tabController.lastSpringMoveTime;

    setState(() {
      tabController.tabVelocity = Offset(tabController.xSpring!.dx(springTime), tabController.ySpring!.dx(springTime));
      tabController.tabPosition = Offset(tabController.xSpring!.x(springTime), tabController.ySpring!.x(springTime));
    });

    // If both springs are at rest, stop ticking to save performance
    if (tabController.xSpring!.isDone(springTime) && tabController.ySpring!.isDone(springTime)) {
      tabController.ticker?.stop();
      tabController.lastSpringMoveTime = 0;
      tabController.lastSpringTime = 0;
    }
  }

  /// Start dragging the floating tab.
  void onStartDrag(DragStartDetails details) {
    tabRelativePosition =
        manager.getTabRelativePosition(tabController.tabPosition, stationaryTabController.tabPosition);
  }

  /// tab avoidance logic on first app run only.
  void tabAvoidanceOnFirstTimeRun() {
    if (tabController.tabPosition.dy == stationaryTabController.tabPosition.dy) {
      tabController.moveTabToFarthestCorner(
        Offset(
          tabController.tabPosition.dx,
          tabController.tabPosition.dy - tabController.tabSize.height,
        ),
      );
    }
  }

  /// Move a stationary tab out of the way for an incoming tab that's being dragged by providing the expected position
  /// [position] of the dragged tab.
  /// if [position] is not provided we'd use the current [draggedTabPosition] instead.
  void _moveStationaryTab([Offset? position]) {
    // Relative position of the dragged tab.

    TabPosition relativePosition = tabRelativePosition;

    final stationaryTabPosition = stationaryTabController.tabPosition;

    final draggedTabPosition = position != null ? position : tabController.tabPosition;

    double oneTabHeight = tabController.tabSize.height;

    /// whether a fling would result in dragged tabs side being same with the stationary tabs side based off of the
    /// tabs [tabController.targetPosition].
    bool bIsDockSideToBeTheSame = tabController.checkDockSideAtPosition(position ?? tabController.targetTabPosition) ==
        stationaryTabController.side;

    void _moveTabBasedOnBottomAndTopEdges() {
      // Center position of the stationary tab
      double stationaryTabCenter = stationaryTabPosition.dy + oneTabHeight / 2;

      // Bottom edge position of the currently being dragged tab
      double draggedTabBottomEdge = (position != null ? position.dy : draggedTabPosition.dy) + oneTabHeight;

      // Check if the drag started below the stationary tab and that the bottomLeft corner position is below the
      // center position of the stationary tab.
      if (relativePosition == TabPosition.above && draggedTabBottomEdge > stationaryTabCenter) {
        double newPosition = stationaryTabPosition.dy - oneTabHeight;

        stationaryTabController.dockTab(stationaryTabController.side, dockY: newPosition);

        if (stationaryTabPosition.dy.round() == draggedTabPosition.dy.round()) {
          stationaryTabController.moveTabToFarthestCorner(stationaryTabPosition);
        }
      } else if (relativePosition == TabPosition.below && draggedTabPosition.dy < stationaryTabCenter) {
        double newPosition = stationaryTabPosition.dy + oneTabHeight;
        stationaryTabController.dockTab(stationaryTabController.side, dockY: newPosition);

        if (stationaryTabPosition.dy.round() == draggedTabPosition.dy.round()) {
          stationaryTabController.moveTabToFarthestCorner(stationaryTabPosition);
        }
      }
    }

    void _handleEqualTabPositions() {
      if (stationaryTabPosition.dy.round() == draggedTabPosition.dy.round()) {
        stationaryTabController.moveTabToFarthestCorner(stationaryTabPosition);
      }

      if (stationaryTabPosition.dy.round() == tabController.targetTabPosition.dy.round()) {
        stationaryTabController.moveTabToFarthestCorner(stationaryTabPosition);
      }
    }

    // Check to make sure both tabs are on the same side
    if (stationaryTabController.side == tabController.side) {
      _moveTabBasedOnBottomAndTopEdges();
      _handleEqualTabPositions();
    }

    if (bIsDockSideToBeTheSame) {
      _handleEqualTabPositions();
      _moveTabBasedOnBottomAndTopEdges();
    }
  }
}
