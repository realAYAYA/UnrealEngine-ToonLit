// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:flutter/material.dart';
import 'package:flutter_gen/gen_l10n/app_localizations.dart';
import 'package:provider/provider.dart';
import 'package:streaming_shared_preferences/streaming_shared_preferences.dart';
import 'package:epic_common/theme.dart';

import '../../../../models/settings/main_screen_settings.dart';
import '../../../../models/unreal_types.dart';
import '../../../elements/lightcard_map.dart';
import '../sidebar/outliner_panel.dart';

class StageMapTab extends StatefulWidget {
  const StageMapTab({Key? key}) : super(key: key);
  static const String iconPath = 'packages/epic_common/assets/icons/world.svg';

  static String getTitle(BuildContext context) => AppLocalizations.of(context)!.tabTitleStageMap;

  @override
  State<StageMapTab> createState() => _StageMapTabState();
}

class _StageMapTabState extends State<StageMapTab> {
  final _mapKey = GlobalKey();

  /// The state of the map this is displaying.
  StageMapState get _mapState => _mapKey.currentState as StageMapState;

  @override
  Widget build(BuildContext context) {
    return PreferenceBuilder(
      preference: Provider.of<MainScreenSettings>(context).bIsOutlinerPanelOpen,
      builder: (context, final bool bIsOutlinerPanelOpen) => Container(
        color: UnrealColors.black,
        child: Row(children: [
          Expanded(
            key: const Key('StageMap'),
            child: StageMap(key: _mapKey),
          ), // Outliner panel

          if (bIsOutlinerPanelOpen)
            Container(
              key: const Key('OutlinerPanel'),
              padding: EdgeInsets.all(UnrealTheme.cardMargin),
              decoration: BoxDecoration(
                borderRadius: const BorderRadius.only(
                  topLeft: Radius.circular(UnrealTheme.outerCornerRadius),
                  bottomLeft: Radius.circular(UnrealTheme.outerCornerRadius),
                ),
                color: Theme.of(context).colorScheme.background,
              ),
              child: OutlinerPanel(
                focusActor: _focusActor,
                canFocusActor: _getCanFocusActor,
              ),
            ),
        ]),
      ),
    );
  }

  /// Focus the map on a specific [actor].
  void _focusActor(UnrealObject actor) => _mapState.focusActor(actor);

  /// Check whether the map can focus on an [actor].
  bool _getCanFocusActor(UnrealObject actor) => _mapState.getCanFocusActor(actor);
}
