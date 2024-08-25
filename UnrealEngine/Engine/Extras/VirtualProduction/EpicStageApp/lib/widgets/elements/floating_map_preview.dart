// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:epic_common/widgets.dart';
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import 'floating_tab_base.dart';
import 'lightcard_map.dart';

/// A floating preview of the stage map that the user can drag to any corner of the screen and dock to the side.
class FloatingMapPreview extends StatefulWidget {
  const FloatingMapPreview({Key? key}) : super(key: key);

  static GlobalKey map = GlobalKey(debugLabel: 'floating_map');

  @override
  State<FloatingMapPreview> createState() => _FloatingMapPreviewState();
}

class _FloatingMapPreviewState extends State<FloatingMapPreview> with SingleTickerProviderStateMixin, FloatingTabBase {
  @override
  void initState() {
    super.initState();

    tabController = manager.mapTabController;

    initStationaryController(manager.trackpadTabController);

    tabController.ticker = createTicker(onTick);

    /// init dragged tab relative position.
    tabRelativePosition =
        manager.getTabRelativePosition(tabController.tabPosition, stationaryTabController.tabPosition);
  }

  @override
  Widget build(BuildContext context) {
    return ChangeNotifierProvider.value(
      value: tabController,
      builder: (context, child) {
        return FloatingTabBody(
          AssetIcon(path: 'packages/epic_common/assets/icons/map.svg', size: 24),
          StageMap(bIsControllable: false, bShowOnlySelectedPins: true, bForceShowFullMap: true, pinSize: 32),
          bShouldUseStackLayout: true,
          onTap: onTap,
          onPanStart: onStartDrag,
          onPanUpdate: updateDrag,
          onPanCancel: endDrag,
          onPanEnd: endDrag,
          children: [],
        );
      },
    );
  }
}
