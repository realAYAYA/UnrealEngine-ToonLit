// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:epic_common/theme.dart';
import 'package:epic_common/widgets.dart';
import 'package:flutter/material.dart';
import 'package:flutter_gen/gen_l10n/app_localizations.dart';
import 'package:provider/provider.dart';
import 'package:streaming_shared_preferences/streaming_shared_preferences.dart';

import '../../models/settings/recent_actor_settings.dart';
import '../../models/unreal_actor_creator.dart';
import '../../models/unreal_types.dart';
import 'dropdown_button.dart';
import 'template_menu.dart';

/// Button that opens the actor placement menu.
class PlaceActorButton extends StatelessWidget {
  const PlaceActorButton({Key? key}) : super(key: key);

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      // Extend the button to the full toolbar height to make it easier to tap even though the styling of the button
      // makes it appear smaller
      height: double.infinity,
      child: ModalDropdownButton(
        buttonBuilder: (_, __) => _PlaceActorButtonContents(),
        menuBuilder: (_, __) => PlaceActorDropDownMenu(),
      ),
    );
  }
}

/// The visible contents of a [PlaceActorButton].
class _PlaceActorButtonContents extends StatelessWidget {
  const _PlaceActorButtonContents({
    Key? key,
    this.bIsActive = false,
    this.bRoundTop = false,
    this.bRoundBottom = false,
  }) : super(key: key);

  /// Whether the button should be styled as an active dropdown.
  final bool bIsActive;

  /// If true, round the top corners of the container.
  final bool bRoundTop;

  /// If true, round the bottom corners of the container.
  final bool bRoundBottom;

  @override
  Widget build(BuildContext context) {
    return Container(
      height: 48,
      padding: EdgeInsets.symmetric(horizontal: 8),
      decoration: BoxDecoration(
        color: bIsActive ? Theme.of(context).colorScheme.surfaceTint : null,
        borderRadius: BorderRadius.vertical(
          top: bRoundTop ? Radius.circular(8) : Radius.zero,
          bottom: bRoundBottom ? Radius.circular(8) : Radius.zero,
        ),
      ),
      child: Align(
        widthFactor: 1,
        child: EpicWideButton(
          text: AppLocalizations.of(context)!.addActorButton,
          iconPath: 'packages/epic_common/assets/icons/plus.svg',
          color: bIsActive ? UnrealColors.highlightGreen : Theme.of(context).colorScheme.surfaceTint,
          iconColor: bIsActive ? null : UnrealColors.highlightGreen,
          border: Border.all(
            color: Theme.of(context).colorScheme.surfaceVariant,
            width: 1,
          ),
        ),
      ),
    );
  }
}

/// Drop-down menu shown to select an actor to place.
class PlaceActorDropDownMenu extends StatelessWidget {
  const PlaceActorDropDownMenu({
    Key? key,
    this.actorMapPosition,
    this.bIsFromLongPress = false,
  }) : super(key: key);

  /// If provided, override the actor's spawn position (in coordinates normalized to the preview map's size).
  final Offset? actorMapPosition;

  /// Whether the menu was created in-place from a user's long press action.
  final bool bIsFromLongPress;

  @override
  Widget build(BuildContext context) {
    final actorCreator = Provider.of<UnrealActorCreator>(context, listen: false);
    final actorCreationSettings = ActorCreationSettings(mapPosition: actorMapPosition);

    return DropDownListMenu(
      bCenterOriginTabOnPivot: bIsFromLongPress,
      minHeight: 250,
      originTabBuilder: (context, bIsOnTop) => _PlaceActorButtonContents(
        bIsActive: true,
        bRoundTop: bIsFromLongPress && bIsOnTop,
        bRoundBottom: bIsFromLongPress && !bIsOnTop,
      ),
      children: [
        ListMenuSimpleItem(
          title: AppLocalizations.of(context)!.actorNameLightCard,
          iconPath: 'packages/epic_common/assets/icons/light_card.svg',
          onTap: () {
            actorCreator.createLightcard(actorCreationSettings);
            _closeMenu(context);
          },
        ),
        ListMenuSimpleItem(
          title: AppLocalizations.of(context)!.actorNameFlag,
          iconPath: 'packages/epic_common/assets/icons/light_card_flag.svg',
          onTap: () {
            actorCreator.createFlag(actorCreationSettings);
            _closeMenu(context);
          },
        ),
        ListMenuSimpleItem(
          title: AppLocalizations.of(context)!.actorNameChromakeyCard,
          iconPath: 'packages/epic_common/assets/icons/chromakey_card.svg',
          onTap: () {
            actorCreator.createChromakeyCard(actorCreationSettings);
            _closeMenu(context);
          },
        ),
        ListMenuSimpleItem(
          title: AppLocalizations.of(context)!.actorNameColorCorrectWindow,
          iconPath: 'packages/epic_common/assets/icons/color_correct_window.svg',
          onTap: () {
            actorCreator.createColorCorrectionWindow(actorCreationSettings);
            _closeMenu(context);
          },
        ),
        ListMenuSimpleItem(
          title: AppLocalizations.of(context)!.placeActorMenuAllTemplates,
          iconPath: 'packages/epic_common/assets/icons/template.svg',
          bShowArrow: true,
          onTap: () {
            _closeMenu(context);
            TemplatePickerMenu.show(context, actorScreenPosition: actorMapPosition);
          },
        ),
        PreferenceBuilder(
          preference: Provider.of<RecentActorSettings>(context, listen: false).recentlyPlacedActors,
          builder: (BuildContext context, List<RecentlyPlacedActorData>? recentlyPlacedActors) {
            if (recentlyPlacedActors == null) {
              return SizedBox();
            }

            return Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                if (recentlyPlacedActors.isNotEmpty)
                  ListMenuHeader(AppLocalizations.of(context)!.placeActorMenuRecentlyPlaced),
                for (final RecentlyPlacedActorData actorData in recentlyPlacedActors)
                  _RecentlyPlacedMenuItem(actorData, actorMapPosition: actorMapPosition),
              ],
            );
          },
        ),
      ],
    );
  }

  /// Close the menu.
  void _closeMenu(BuildContext context) {
    Navigator.of(context).pop();
  }
}

/// An item in the menu representing a recently placed actor type.
class _RecentlyPlacedMenuItem extends StatelessWidget {
  const _RecentlyPlacedMenuItem(this.actorData, {Key? key, this.actorMapPosition}) : super(key: key);

  /// Data about the actor to place.
  final RecentlyPlacedActorData actorData;

  /// If provided, override the actor's spawn position (in coordinates normalized to the preview map's size).
  final Offset? actorMapPosition;

  /// The path of the icon to display for this item.
  String? get _iconPath {
    if (actorData.templatePath != null) {
      return 'packages/epic_common/assets/icons/template.svg';
    }

    switch (actorData.type) {
      case RecentlyPlacedActorType.lightCard:
        return 'packages/epic_common/assets/icons/light_card.svg';

      case RecentlyPlacedActorType.flag:
        return 'packages/epic_common/assets/icons/light_card_flag.svg';

      case RecentlyPlacedActorType.chromakeyCard:
        return 'packages/epic_common/assets/icons/chromakey_card.svg';

      case RecentlyPlacedActorType.colorCorrectWindow:
        return 'packages/epic_common/assets/icons/color_correct_window.svg';

      default:
        return null;
    }
  }

  @override
  Widget build(BuildContext context) {
    return ListMenuSimpleItem(
      title: _getActorName(context),
      iconPath: _iconPath,
      onTap: () => _createActor(context),
    );
  }

  /// The name to display for this view.
  String _getActorName(BuildContext context) {
    if (actorData.name != null) {
      return actorData.name!;
    }

    switch (actorData.type) {
      case RecentlyPlacedActorType.lightCard:
        return AppLocalizations.of(context)!.actorNameLightCard;

      case RecentlyPlacedActorType.flag:
        return AppLocalizations.of(context)!.actorNameFlag;

      case RecentlyPlacedActorType.chromakeyCard:
        return AppLocalizations.of(context)!.actorNameChromakeyCard;

      case RecentlyPlacedActorType.colorCorrectWindow:
        return AppLocalizations.of(context)!.actorNameColorCorrectWindow;
    }
  }

  /// Create an actor of the type indicated by this menu item.
  void _createActor(BuildContext context) {
    final UnrealActorCreator actorCreator = Provider.of<UnrealActorCreator>(context, listen: false);

    final actorSettings = ActorCreationSettings(mapPosition: actorMapPosition);

    if (actorData.templatePath != null) {
      actorCreator.createLightcard(actorSettings.copyWith(
        template: UnrealTemplateData(
          name: actorData.name ?? actorData.templatePath!,
          path: actorData.templatePath!,
        ),
      ));
    } else {
      switch (actorData.type) {
        case RecentlyPlacedActorType.lightCard:
          actorCreator.createLightcard(actorSettings);
          break;

        case RecentlyPlacedActorType.chromakeyCard:
          actorCreator.createChromakeyCard(actorSettings);
          break;

        case RecentlyPlacedActorType.flag:
          actorCreator.createFlag(actorSettings);
          break;

        case RecentlyPlacedActorType.colorCorrectWindow:
          actorCreator.createColorCorrectionWindow(actorSettings);
          break;

        default:
          throw 'Can\'t create actor of type ${actorData.type}';
      }
    }

    _closeMenu(context);
  }

  /// Close the menu.
  void _closeMenu(BuildContext context) {
    Navigator.of(context).pop();
  }
}
