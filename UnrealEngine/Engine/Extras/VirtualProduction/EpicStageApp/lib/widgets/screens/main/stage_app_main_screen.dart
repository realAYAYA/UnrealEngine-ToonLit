// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:epic_common/theme.dart';
import 'package:epic_common/widgets.dart';
import 'package:flutter/material.dart';
import 'package:flutter_gen/gen_l10n/app_localizations.dart';
import 'package:provider/provider.dart';

import '../../../models/settings/main_screen_settings.dart';
import '../../../models/settings/selected_actor_settings.dart';
import '../../../models/unreal_actor_manager.dart';
import '../../../models/unreal_types.dart';
import '../../../utilities/constants.dart';
import '../../../utilities/debug_utilities.dart';
import '../../elements/floating_map_preview.dart';
import '../../elements/floating_trackpad.dart';
import '../connect/views/n_display_selector_dialog.dart';
import 'tabs/main_screen_tabs.dart';
import 'toolbar/main_screen_toolbar.dart';

/// The main screen of the app shown once the user is connected to the engine.
class StageAppMainScreen extends StatefulWidget {
  const StageAppMainScreen({Key? key}) : super(key: key);

  static const String route = '/main';

  @override
  State<StageAppMainScreen> createState() => _StageAppMainScreenState();
}

class _StageAppMainScreenState extends State<StageAppMainScreen>
    with SingleTickerProviderStateMixin, WidgetsBindingObserver {
  late final UnrealActorManager _actorManager;
  late final TabController _tabController;

  /// Global key that refers to the toolbar's place actors button.
  final GlobalKey _placeActorsButtonKey = GlobalKey();

  /// Whether the outliner can be toggled in the current tab.
  bool _bEnableOutlinerToggle = true;

  /// Whether to show the floating stage map preview.
  bool _bShowMapPreview = false;

  @override
  void initState() {
    super.initState();

    _tabController = TabController(
      animationDuration: Duration.zero,
      length: MainScreenTabs.tabConfigs.length,
      vsync: this,
    );
    _tabController.addListener(_onActiveTabChanged);

    final int startTab = Provider.of<MainScreenSettings>(context, listen: false).selectedTab.getValue();

    if (startTab >= 0 && startTab < _tabController.length) {
      _tabController.index = startTab;
    } else {
      _tabController.index = 0;
    }

    _actorManager = Provider.of<UnrealActorManager>(context, listen: false);
    _actorManager.watchClassName(nDisplayRootActorClassName, _onRootActorUpdate);

    final List<Future<Set<UnrealObject>>> initialActorFutures = [];

    /// Subscribe to classes that we always want to know about regardless of the current tab.
    /// This prevents us from constantly sending unsubscribe and resubscribe messages when changing tabs.
    for (final String className in controllableClassNames) {
      _actorManager.watchClassName(className, _onControllableActorUpdate);
      initialActorFutures.add(_actorManager.getInitialActorsOfClass(className));
    }

    Future.wait(initialActorFutures).then(_handleInitialControllableActorList);
  }

  @override
  void dispose() {
    _tabController.dispose();
    _actorManager.stopWatchingClassName(nDisplayRootActorClassName, _onRootActorUpdate);

    for (final String className in controllableClassNames) {
      _actorManager.stopWatchingClassName(className, _onControllableActorUpdate);
    }

    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      resizeToAvoidBottomInset: false,
      appBar: MainScreenToolbar(
        tabController: _tabController,
        bEnableOutlinerToggle: _bEnableOutlinerToggle,
        placeActorButtonKey: _placeActorsButtonKey,
      ),
      body: Provider(
        create: (_) => StageAppMainScreenKeys(placeActorsButtonKey: _placeActorsButtonKey),
        child: Stack(
          clipBehavior: Clip.none,
          children: [
            Container(
              color: UnrealColors.gray14,
              padding: EdgeInsets.only(
                left: UnrealTheme.cardMargin,
                right: UnrealTheme.cardMargin,
                bottom: UnrealTheme.cardMargin,
              ),
              key: Key('Tab View'),
              child: Container(
                decoration: BoxDecoration(
                  color: Theme.of(context).colorScheme.background,
                  borderRadius: BorderRadius.circular(UnrealTheme.outerCornerRadius),
                ),
                clipBehavior: Clip.antiAlias,
                child: LazyTabView(
                  controller: _tabController,
                  builder: MainScreenTabs.createTabContents,
                  keepAlive: MainScreenTabs.shouldKeepTabAlive,
                ),
              ),
            ),
            if (_bShowMapPreview) const FloatingMapPreview(key: Key('Map Preview')),
            FloatingTrackpad()
          ],
        ),
      ),
    );
  }

  /// Called when the actors in the engine have changed.
  void _onRootActorUpdate(ActorUpdateDetails details) async {
    if (!mounted) {
      return;
    }

    final selectedActorSettings = Provider.of<SelectedActorSettings>(context, listen: false);
    final Set<UnrealObject> rootActors = _actorManager.getActorsOfClass(nDisplayRootActorClassName);

    if (rootActors.any((actor) => actor.path == selectedActorSettings.displayClusterRootPath.getValue())) {
      // Our selected actor is still valid
      return;
    }

    if (rootActors.isEmpty) {
      // Wait until end of frame so the root actor select menu, if any, has time to pop
      WidgetsBinding.instance.addPostFrameCallback((_) {
        if (mounted) {
          showDebugAlert(AppLocalizations.of(context)!.warningNoRootActors);
        }
      });
      return;
    }

    if (rootActors.length > 1) {
      // Prompt the user to select one of the new root actors
      final UnrealObject? newActor = await NDisplaySelectorDialog.showIfNotOpen(context, bIsAlreadyConnected: true);
      Provider.of<SelectedActorSettings>(context, listen: false).displayClusterRootPath.setValue(newActor?.path ?? '');
      return;
    }

    // There's only one new root actor, so use that one
    Provider.of<SelectedActorSettings>(context, listen: false).displayClusterRootPath.setValue(rootActors.first.path);
  }

  /// Called when the active tab changes.
  void _onActiveTabChanged() {
    Provider.of<MainScreenSettings>(context, listen: false).selectedTab.setValue(_tabController.index);

    final bool bNewEnableOutlinerToggle = MainScreenTabs.shouldEnableOutlinerToggle(_tabController.index);
    if (bNewEnableOutlinerToggle != _bEnableOutlinerToggle) {
      setState(() {
        _bEnableOutlinerToggle = bNewEnableOutlinerToggle;
      });
    }

    final bool bNewShowMapPreview = MainScreenTabs.shouldShowMapPreview(_tabController.index);
    if (bNewShowMapPreview != _bShowMapPreview) {
      setState(() {
        _bShowMapPreview = bNewShowMapPreview;
      });
    }
  }

  /// Called when a controllable actor has changed.
  void _onControllableActorUpdate(ActorUpdateDetails details) {
    if (!mounted || details.bIsDueToDisconnect) {
      // Don't deselect actors that are reported as deleted due to disconnect since we can re-select then on reconnect
      return;
    }

    // Deselect any deleted actors
    final selectedActorSettings = Provider.of<SelectedActorSettings>(context, listen: false);

    for (final UnrealObject actor in details.deletedActors) {
      if (selectedActorSettings.isActorSelected(actor.path)) {
        selectedActorSettings.selectActor(actor.path, bShouldSelect: false);
      }
    }
  }

  /// Handle the initial list of [actors] received from the engine.
  void _handleInitialControllableActorList(List<Set<UnrealObject>> actors) {
    if (!mounted) {
      return;
    }

    final selectedActorSettings = Provider.of<SelectedActorSettings>(context, listen: false);

    // Deselect any actors that no longer exist
    final staleActorPaths = Set<String>();

    for (final String actorPath in selectedActorSettings.selectedActors.getValue()) {
      if (_actorManager.getActorAtPath(actorPath) == null) {
        staleActorPaths.add(actorPath);
      }
    }

    for (final String actorPath in staleActorPaths) {
      selectedActorSettings.selectActor(actorPath, bShouldSelect: false);
    }
  }
}

/// Container for global keys that refer to elements of the main screen.
class StageAppMainScreenKeys {
  const StageAppMainScreenKeys({required this.placeActorsButtonKey});

  /// Key for the place actors button in the main screen's toolbar.
  final GlobalKey placeActorsButtonKey;
}
