// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:flutter/material.dart';
import 'package:flutter_gen/gen_l10n/app_localizations.dart';
import 'package:provider/provider.dart';

import '../../../../utilities/constants.dart';
import '../../../../widgets/elements/empty_placeholder.dart';
import '../../../../widgets/elements/layout/card.dart';
import '../../../../widgets/screens/connect/views/connection_list.dart';
import '../../../../widgets/screens/main/toolbar/main_screen_toolbar.dart';
import '../../../models/engine_connection.dart';
import '../../../models/navigator_keys.dart';
import '../../../models/unreal_engine_beacon.dart';
import '../../../utilities/debug_utilities.dart';
import '../../../utilities/net_utilities.dart';
import '../../../utilities/unreal_colors.dart';
import '../../elements/epic_icon_button.dart';
import '../../elements/modal.dart';
import 'mixins/connect_mixin.dart';
import 'views/manual_connect_form.dart';
import 'views/quick_action_grid.dart';

/// A screen that uses UDP multicast to search for any local Unreal Engine instances and lists them for the user to
/// choose from.
class ConnectScreen extends StatefulWidget {
  const ConnectScreen({Key? key}) : super(key: key);

  static const String route = '/connect';

  @override
  _ConnectScreenState createState() => _ConnectScreenState();
}

class _ConnectScreenState extends State<ConnectScreen> with WidgetsBindingObserver, ConnectMixin {
  /// Beacon which will be used to detect instances of Unreal Engine.
  late final UnrealEngineBeacon _engineBeacon;

  /// Data for the last engine we were connected to, if any.
  ConnectionData? _lastConnection;

  /// List of valid engine instances to connect to.
  List<ConnectionData> _connections = [];

  /// Called when the beacon has failed for too long.
  void _onBeaconFailure() {
    // If there is an opened route that allows a user go back or pop the Navigation stack once,
    // we likely should not be showing this alert then.
    if (!Navigator.of(context).canPop()) {
      showDebugAlert(AppLocalizations.of(context)!.connectScreenBeaconFailedMessage);
    }
  }

  @override
  void initState() {
    super.initState();

    WidgetsBinding.instance.addObserver(this);

    _engineBeacon = UnrealEngineBeacon(
      context: context,
      onBeaconFailure: _onBeaconFailure,
      onEngineInstancesChanged: () => setState(() {}),
    );

    Provider.of<EngineConnectionManager>(context, listen: false)
        .getLastConnectionData()
        .then(_receiveLastConnectionData);
  }

  @override
  void dispose() {
    super.dispose();

    WidgetsBinding.instance.removeObserver(this);

    _engineBeacon.dispose();
  }

  @override
  void didChangeAppLifecycleState(AppLifecycleState state) {
    switch (state) {
      case AppLifecycleState.resumed:
        _engineBeacon.resume();
        break;

      case AppLifecycleState.paused:
      case AppLifecycleState.detached:
        // If the app is in the background, stop sending beacon messages
        _engineBeacon.pause();
        break;

      default:
        break;
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      resizeToAvoidBottomInset: false,
      appBar: const _ConnectScreenToolbar(),
      body: Container(
        color: UnrealColors.gray14,
        padding: EdgeInsets.only(left: cardMargin, right: cardMargin, bottom: cardMargin),
        child: Container(
          width: MediaQuery.of(context).size.width,
          padding: EdgeInsets.all(cardMargin),
          decoration: BoxDecoration(
            color: Theme.of(context).colorScheme.background,
            borderRadius: BorderRadius.circular(outerCornerRadius),
          ),
          child: Row(
            mainAxisSize: MainAxisSize.min,
            children: [
              Card(
                child: IntrinsicWidth(
                  child: Column(
                    children: [
                      CardLargeHeader(
                        title: AppLocalizations.of(context)!.connectScreenQuickConnectPanelTitle,
                        iconPath: 'assets/images/icons/light_card.svg',
                      ),
                      QuickActionGrid(recentConnections: _connections),
                    ],
                  ),
                ),
              ),
              SizedBox(width: cardMargin),
              Expanded(
                child: Card(
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.stretch,
                    children: [
                      CardLargeHeader(
                        title: AppLocalizations.of(context)!.connectScreenAllConnectionsPanelTitle,
                        iconPath: 'assets/images/icons/unreal_u_logo.svg',
                      ),
                      Expanded(
                        child: _engineBeacon.connections.length > 0
                            ? ConnectionList(
                                connections: _engineBeacon.connections,
                              )
                            : EmptyPlaceholder(
                                message: AppLocalizations.of(context)!.connectScreenAllConnectionsPanelEmptyMessage,
                                button: EpicWideButton(
                                  text: AppLocalizations.of(context)!.connectScreenAllConnectionsPanelEmptyButtonLabel,
                                  iconPath: 'assets/images/icons/plus.svg',
                                  color: UnrealColors.highlightGreen,
                                  onPressed: _showManualConnectDialog,
                                ),
                              ),
                      ),
                    ],
                  ),
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }

  /// Called when the data for the last connection is loaded.
  void _receiveLastConnectionData(ConnectionData? connectionData) {
    if (connectionData != null) {
      setState(() {
        _lastConnection = connectionData;
        _connections.removeWhere((existingConnection) => _isLastConnection(existingConnection));
      });
      _connections.add(connectionData);
    }
  }

  /// Check if the given [connectionData] is the last connection we made.
  bool _isLastConnection(ConnectionData connectionData) {
    // We can't compare on UUID because the engine may have restarted, so check everything else
    return _lastConnection != null &&
        connectionData.websocketAddress == _lastConnection!.websocketAddress &&
        connectionData.websocketPort == _lastConnection!.websocketPort &&
        connectionData.name == _lastConnection!.name;
  }

  /// Show the dialog for manually connecting to the engine.
  void _showManualConnectDialog() {
    GenericModalDialogRoute.showDialog(
      context: rootNavigatorKey.currentContext!,
      builder: (context) => ModalDialogCard(
        child: ManualConnectForm(connect: connect),
      ),
    );
  }
}

/// Toolbar shown only on the connect screen.
class _ConnectScreenToolbar extends StatelessWidget implements PreferredSizeWidget {
  const _ConnectScreenToolbar({Key? key}) : super(key: key);

  @override
  Size get preferredSize => const Size.fromHeight(48);

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.only(right: 8),
      decoration: BoxDecoration(
        color: Theme.of(context).colorScheme.surface,
      ),
      height: preferredSize.height,
      child: Row(
        mainAxisAlignment: MainAxisAlignment.end,
        crossAxisAlignment: CrossAxisAlignment.center,
        children: [
          SettingsButton(),
        ],
      ),
    );
  }
}
