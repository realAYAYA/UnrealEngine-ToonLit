// Copyright Epic Games, Inc. All Rights Reserved.

import 'dart:collection';

import 'package:flutter/material.dart';

import 'color_grading_tab.dart';
import 'details_tab.dart';
import 'stage_map_tab.dart';
import 'web_browser_tab.dart';

/// The tab bar shown within the [MainScreenToolbar].
class MainScreenTabs {
  static final List<MainScreenTabConfig> _tabs = [
    MainScreenTabConfig(
      getTitle: StageMapTab.getTitle,
      iconPath: StageMapTab.iconPath,
      bShowMapPreview: false,
      createTabContents: (context) => const StageMapTab(),
    ),
    MainScreenTabConfig(
      getTitle: DetailsTab.getTitle,
      iconPath: DetailsTab.iconPath,
      createTabContents: (context) => const DetailsTab(),
    ),
    MainScreenTabConfig(
      getTitle: ColorGradingTab.getTitle,
      iconPath: ColorGradingTab.iconPath,
      createTabContents: (context) => const ColorGradingTab(),
    ),
    MainScreenTabConfig(
      getTitle: WebBrowserTab.getTitle,
      iconPath: WebBrowserTab.iconPath,
      bKeepAlive: true,
      bEnableOutlinerToggle: false,
      bShowMapPreview: false,
      createTabContents: (context) => const WebBrowserTab(),
    ),
  ];

  /// The list of tab configurations in display order.
  static UnmodifiableListView<MainScreenTabConfig> get tabConfigs => UnmodifiableListView(_tabs);

  /// Create the contents of one of the tabs by its index.
  static Widget createTabContents(BuildContext context, int index) {
    return _tabs[index].createTabContents(context);
  }

  /// Check whether the given tab's contents should be kept alive when not visible.
  static bool shouldKeepTabAlive(int index) {
    return _tabs[index].bKeepAlive;
  }

  /// Check whether the outliner can be toggled in the given tab.
  static bool shouldEnableOutlinerToggle(int index) {
    return _tabs[index].bEnableOutlinerToggle;
  }

  /// Check whether the given tab should show the map preview.
  static bool shouldShowMapPreview(int index) {
    return _tabs[index].bShowMapPreview;
  }
}

/// Configuration for an entry in the main screen's tab bar.
class MainScreenTabConfig {
  const MainScreenTabConfig({
    required this.getTitle,
    required this.iconPath,
    required this.createTabContents,
    this.bKeepAlive = false,
    this.bEnableOutlinerToggle = true,
    this.bShowMapPreview = true,
  });

  /// Function that returns the title of the tab as shown to the user.
  final String Function(BuildContext) getTitle;

  /// The path of the icon representing this tab.
  final String iconPath;

  /// A function to create the widget for the tab's contents.
  final Widget Function(BuildContext context) createTabContents;

  /// If true, keep this tab's contents alive even when it's not visible.
  /// If false, dispose of the tab's contents when it's not visible.
  final bool bKeepAlive;

  /// Whether the outliner can be toggled in this tab.
  final bool bEnableOutlinerToggle;

  /// Whether the floating map preview should be visible when this tab is open.
  final bool bShowMapPreview;
}
