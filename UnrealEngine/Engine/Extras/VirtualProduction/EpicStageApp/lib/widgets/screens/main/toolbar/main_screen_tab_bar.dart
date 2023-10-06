// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:flutter/material.dart';

import '../../../../utilities/unreal_colors.dart';
import '../../../elements/asset_icon.dart';
import '../tabs/main_screen_tabs.dart';

/// The tab bar shown within the [MainScreenToolbar].
class MainScreenTabBar extends StatefulWidget {
  const MainScreenTabBar({
    Key? key,
    this.tabController,
  }) : super(key: key);

  final TabController? tabController;

  @override
  State<MainScreenTabBar> createState() => _MainScreenTabBarState();
}

class _MainScreenTabBarState extends State<MainScreenTabBar> {
  @override
  Widget build(BuildContext context) {
    return TabBar(
      controller: widget.tabController,
      isScrollable: true, // This shrinks the tab buttons to fit their contents
      unselectedLabelColor: UnrealColors.white,
      padding: EdgeInsets.zero,
      indicatorPadding: EdgeInsets.zero,
      labelPadding: const EdgeInsets.symmetric(horizontal: 4),
      indicator: BoxDecoration(
        color: Theme.of(context).colorScheme.primary,
      ),
      indicatorSize: TabBarIndicatorSize.label,
      tabs: MainScreenTabs.tabConfigs.map((tabConfig) => _MainScreenTabBarTab(tabConfig: tabConfig)).toList(),
    );
  }
}

class _MainScreenTabBarTab extends StatelessWidget {
  const _MainScreenTabBarTab({Key? key, required this.tabConfig}) : super(key: key);

  final MainScreenTabConfig tabConfig;

  @override
  Widget build(BuildContext context) {
    return Tooltip(
      message: tabConfig.getTitle(context),
      child: SizedBox(
        width: 48,
        height: 48,
        child: Center(
          child: AssetIcon(
            path: tabConfig.iconPath,
            size: 24,
            color: DefaultTextStyle.of(context).style.color,
          ),
        ),
      ),
    );
  }
}
