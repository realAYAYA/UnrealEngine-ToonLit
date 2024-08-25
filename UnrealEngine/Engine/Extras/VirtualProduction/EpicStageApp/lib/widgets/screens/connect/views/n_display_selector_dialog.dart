// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:epic_common/localizations.dart';
import 'package:epic_common/theme.dart';
import 'package:epic_common/widgets.dart';
import 'package:flutter/material.dart';
import 'package:flutter_gen/gen_l10n/app_localizations.dart';
import 'package:provider/provider.dart';

import '../../../../models/unreal_actor_manager.dart';
import '../../../../models/unreal_types.dart';
import '../../../../utilities/constants.dart';

/// Displays the list of currently available actors which users can select from.
class NDisplaySelectorDialog extends StatefulWidget {
  const NDisplaySelectorDialog({Key? key, this.bIsAlreadyConnected = false}) : super(key: key);

  /// Whether the user was already connected to the engine when this was shown.
  final bool bIsAlreadyConnected;

  /// The future for when the currently active dialog closes.
  static Future<UnrealObject?>? activeDialogFuture;

  /// Helper function to show this dialog while guaranteeing that only one is open at a time.
  /// If [bIsAlreadyConnected] is true, adjust buttons and menu behaviour to reflect that the next step will not be a
  /// new engine connection.
  static Future<UnrealObject?> showIfNotOpen(
    BuildContext context, {
    bool bIsAlreadyConnected = false,
  }) {
    if (activeDialogFuture == null) {
      activeDialogFuture = GenericModalDialogRoute.showDialog(
        context: context,
        builder: (context) => NDisplaySelectorDialog(
          bIsAlreadyConnected: bIsAlreadyConnected,
        ),
        bIsBarrierDismissible: !bIsAlreadyConnected,
      );

      activeDialogFuture!.then((_) => activeDialogFuture = null);
    }

    return activeDialogFuture!;
  }

  @override
  State<NDisplaySelectorDialog> createState() => _NDisplaySelectorDialogState();
}

class _NDisplaySelectorDialogState extends State<NDisplaySelectorDialog> {
  /// currently selected actor, value will null when no actor is selected.
  UnrealObject? _selectedActor = null;

  /// Manager used to retrieve the list of root actors.
  late final UnrealActorManager _actorManager;

  @override
  void initState() {
    super.initState();

    _actorManager = Provider.of<UnrealActorManager>(context, listen: false);
    _actorManager.watchClassName(nDisplayRootActorClassName, _onRootActorUpdate);
  }

  @override
  void dispose() {
    _actorManager.stopWatchingClassName(nDisplayRootActorClassName, _onRootActorUpdate);

    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return ModalDialogCard(
      child: Container(
        width: MediaQuery.of(context).size.width * .4,
        padding: EdgeInsets.symmetric(horizontal: 16, vertical: 8),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            Row(
              mainAxisSize: MainAxisSize.min,
              mainAxisAlignment: MainAxisAlignment.center,
              children: [
                AssetIcon(
                  path: 'packages/epic_common/assets/icons/unreal_u_logo.svg',
                  size: 24,
                ),
                SizedBox(width: 20),
                Text(
                  AppLocalizations.of(context)!.connectScreenRootActorDialogTitle,
                  style: Theme.of(context).textTheme.displayLarge,
                ),
              ],
            ),
            SizedBox(height: 32),
            Text(
              AppLocalizations.of(context)!.connectScreenRootActorDialogMessage,
            ),
            SizedBox(height: 32),
            ..._actorManager.getActorsOfClass(nDisplayRootActorClassName).map(
                  (e) => NDisplayItem(
                    e,
                    bSelected: _selectedActor == e,
                    onChanged: (value) => setState(() => _selectedActor = value),
                  ),
                ),
            if (!widget.bIsAlreadyConnected || _selectedActor != null)
              Padding(
                padding: EdgeInsets.only(top: 20),
                child: Row(
                  mainAxisAlignment: MainAxisAlignment.end,
                  children: [
                    if (!widget.bIsAlreadyConnected)
                      EpicLozengeButton(
                        onPressed: () => Navigator.pop(context),
                        label: EpicCommonLocalizations.of(context)!.menuButtonCancel,
                        color: (_selectedActor != null) ? Colors.transparent : Theme.of(context).colorScheme.secondary,
                      ),
                    if (_selectedActor != null)
                      Padding(
                        padding: EdgeInsets.only(left: 10),
                        child: EpicLozengeButton(
                            onPressed: () => Navigator.of(context).pop(_selectedActor),
                            label: widget.bIsAlreadyConnected
                                ? EpicCommonLocalizations.of(context)!.menuButtonProceed
                                : AppLocalizations.of(context)!.nDisplaySelectorModalConnectButtonLabel),
                      ),
                  ],
                ),
              ),
          ],
        ),
      ),
    );
  }

  /// Called when the list of root actors changes
  void _onRootActorUpdate(ActorUpdateDetails details) {
    if (!mounted) {
      return;
    }

    final Set<UnrealObject> rootActors = _actorManager.getActorsOfClass(nDisplayRootActorClassName);
    if (rootActors.length == 1) {
      // Only one root actor, so use that one
      Navigator.pop(context, rootActors.first);
      return;
    }

    if (rootActors.length == 0) {
      // No root actors, so nothing to show anymore
      Navigator.pop(context);
      return;
    }

    // Refresh to show new list of actors
    setState(() {});
  }
}

///Visual representation/Item for available root actors to be rendered on [NDisplaySelectorDialog].
class NDisplayItem extends StatelessWidget {
  const NDisplayItem(this.nDisplay, {this.bSelected = false, required this.onChanged, Key? key}) : super(key: key);

  /// NDisplay actor being rendered for selection.
  final UnrealObject nDisplay;

  /// Whether the currently represented actor with name value of [nDisplay.name] is currently selected by the user.
  final bool bSelected;

  /// Value change callback to notify parent widget of currently selected actor, returns [nDisplay].
  final ValueChanged<UnrealObject> onChanged;

  @override
  Widget build(BuildContext context) {
    return InkWell(
      onTap: () => onChanged(nDisplay),
      child: Container(
        decoration: BoxDecoration(
          border: Border.all(width: 3, color: bSelected ? Theme.of(context).colorScheme.primary : UnrealColors.gray42),
          borderRadius: BorderRadius.circular(8),
          color: bSelected ? Theme.of(context).colorScheme.primary : null,
        ),
        padding: EdgeInsets.all(10),
        margin: EdgeInsets.symmetric(horizontal: 30, vertical: 5),
        child: Row(
          children: [
            AssetIcon(
              path: 'packages/epic_common/assets/icons/ndisplay.svg',
              size: 32,
            ),
            SizedBox(width: 16),
            Text(
              nDisplay.name,
              style: Theme.of(context)
                  .textTheme
                  .titleLarge!
                  .copyWith(color: bSelected ? Theme.of(context).colorScheme.onPrimary : UnrealColors.gray56),
            ),
          ],
        ),
      ),
    );
  }
}
