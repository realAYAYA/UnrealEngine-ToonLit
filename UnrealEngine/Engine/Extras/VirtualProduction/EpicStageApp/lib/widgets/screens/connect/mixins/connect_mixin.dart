// Copyright Epic Games, Inc. All Rights Reserved.

import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter_gen/gen_l10n/app_localizations.dart';
import 'package:provider/provider.dart';
import 'package:web_socket_channel/web_socket_channel.dart';

import '../../../../models/engine_connection.dart';
import '../../../../models/navigator_keys.dart';
import '../../../../models/settings/selected_actor_settings.dart';
import '../../../../models/unreal_actor_manager.dart';
import '../../../../models/unreal_types.dart';
import '../../../../utilities/constants.dart';
import '../../../../utilities/net_utilities.dart';
import '../../../../utilities/preferences_bundle.dart';
import '../../../elements/asset_icon.dart';
import '../../../elements/epic_icon_button.dart';
import '../../../elements/modal.dart';
import '../../main/stage_app_main_screen.dart';
import '../views/error_modal.dart';
import '../views/n_display_selector_dialog.dart';

/// Connection Mixin for managing states related to UE connection.
mixin ConnectMixin<T extends StatefulWidget> on State<T> {
  /// A state instance for managing, listening/watching and disposing listeners for various class names.
  late final UnrealActorManager _actorManager;

  /// A state instance used to manage selected actors.
  late final SelectedActorSettings _actorSettings;

  /// A state instance used to establish and manage connection to UE.
  late final EngineConnectionManager _connectionManager;

  /// Whether the [_buildSpinner] is still currently being shown to the user.
  bool? bIsConnecting = false;

  /// Set of available actors found in the connected UE instance.
  Set<UnrealObject> actors = {};

  @override
  void initState() {
    _actorManager = Provider.of<UnrealActorManager>(context, listen: false);
    _actorSettings = Provider.of<SelectedActorSettings>(context, listen: false);
    _connectionManager = Provider.of<EngineConnectionManager>(context, listen: false);
    super.initState();
  }

  /// Callback function for connecting to an instance of UE, using [data].
  Future<void> connect(ConnectionData data) async {
    ConnectionData connection = data;

    _buildSpinner(context, data);
    setState(() => bIsConnecting = true);

    final WebSocketChannel? channel = await _connectionManager.connect(connection);

    if (channel == null) {
      if (bIsConnecting == true) {
        Navigator.of(context).maybePop();
        _buildError(context, data);
      }
      return null;
    }

    final Set<UnrealObject> actors = await _actorManager.getInitialActorsOfClass(nDisplayRootActorClassName);

    final store = Provider.of<PreferencesBundle>(context, listen: false).persistent;

    if (actors.length == 1) {
      // Use the only available root actor
      _actorSettings.displayClusterRootPath.setValue(actors.first.path);
    } else if (actors.length > 1) {
      // Multiple root actors available, so check if we'd previously selected one of them
      final String previousDisplayRootActor =
          store.getString('common.displayClusterRootPath', defaultValue: '').getValue();

      if (!actors.any((element) => element.path == previousDisplayRootActor)) {
        // If the progress dialog is still showing we want to stop it before showing the NDisplayDialogSelector.
        if (bIsConnecting == true) {
          Navigator.pop(context);
        }

        final UnrealObject? newActor = await NDisplaySelectorDialog.showIfNotOpen(context);

        if (newActor == null) {
          // User cancelled
          _connectionManager.disconnect();
          return;
        }

        _actorSettings.displayClusterRootPath.setValue(newActor.path);
      }
    }

    Navigator.of(context).pushReplacementNamed(StageAppMainScreen.route);
  }

  /// Show Circular progress indicator in the current [context] when connecting to UE using [data].
  _buildSpinner(BuildContext context, ConnectionData data) async {
    final route = GenericModalDialogRoute(
      bResizeToAvoidBottomInset: true,
      builder: (_) => ModalDialogCard(
        child: Container(
          width: MediaQuery.of(context).size.width * 0.5,
          padding: EdgeInsets.all(10),
          child: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              Row(
                mainAxisAlignment: MainAxisAlignment.center,
                mainAxisSize: MainAxisSize.min,
                children: [
                  AssetIcon(
                    path: 'assets/images/icons/unreal_u_logo.svg',
                    size: 24,
                  ),
                  SizedBox(width: 16),
                  Text(
                    AppLocalizations.of(context)!.connectScreenConnectDialogTitle,
                    style: Theme.of(context).textTheme.displayLarge,
                  ),
                ],
              ),
              const SizedBox(height: 16),
              Text(AppLocalizations.of(context)!
                  .connectScreenConnectDialogMessage('${data.websocketAddress.address}:${data.websocketPort}')),
              const SizedBox(height: 32),
              SizedBox(
                child: const CircularProgressIndicator(strokeWidth: 8.5),
                height: 175,
                width: 175,
              ),
              Row(mainAxisAlignment: MainAxisAlignment.end, children: [
                EpicLozengeButton(
                  label: AppLocalizations.of(context)!.menuButtonCancel,
                  color: Theme.of(context).colorScheme.secondary,
                  onPressed: () {
                    Navigator.pop(context, false);
                    _connectionManager.disconnect();
                    setState(() => bIsConnecting = false);
                  },
                ),
              ]),
            ],
          ),
        ),
      ),
    );

    return Navigator.of(rootNavigatorKey.currentContext!, rootNavigator: true).push(route);
  }

  /// Show error message dialog/modal in the current [context], when and if connection to UE fails using [data].
  _buildError(BuildContext context, ConnectionData data) async {
    return GenericModalDialogRoute.showDialog(
      context: context,
      builder: (_) => ModalDialogCard(
        child: ErrorModal(data: data, reconnect: () => connect(data)),
      ),
    );
  }
}
