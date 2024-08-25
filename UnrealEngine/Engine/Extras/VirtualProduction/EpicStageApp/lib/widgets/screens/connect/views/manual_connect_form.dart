// Copyright Epic Games, Inc. All Rights Reserved.

import 'dart:io';

import 'package:epic_common/localizations.dart';
import 'package:epic_common/widgets.dart';
import 'package:flutter/material.dart';
import 'package:flutter_gen/gen_l10n/app_localizations.dart';

import '../../../../utilities/constants.dart';
import '../../../../utilities/net_utilities.dart';

/// String which will trigger demo mode when entered as the IP address.
const String _demoModeString = 'demo.mode';

/// Manual connection form for manually connecting to an instance of UE.
class ManualConnectForm extends StatefulWidget {
  const ManualConnectForm({Key? key, required this.connect}) : super(key: key);

  /// A function that allows us connect to an instance of UE.
  final Function(ConnectionData) connect;

  @override
  State<ManualConnectForm> createState() => _ManualConnectFormState();
}

class _ManualConnectFormState extends State<ManualConnectForm> {
  /// Global key for managing form state & validating form input.
  final _formKey = GlobalKey<FormState>();

  /// Input controller for IP address on the form.
  final _ipTextController = TextEditingController(text: '');

  /// Input controller for PORT on the form.
  final _portTextController = TextEditingController(text: '30020');

  @override
  Widget build(BuildContext context) {
    return Form(
      key: _formKey,
      child: IntrinsicHeight(
        child: IntrinsicWidth(
          child: Column(
            children: [
              ModalDialogTitle(title: AppLocalizations.of(context)!.connectScreenConnectionFormTitle),
              ModalDialogSection(
                child: TextFormField(
                  cursorWidth: 1,
                  style: Theme.of(context).textTheme.bodyMedium,
                  keyboardType: const TextInputType.numberWithOptions(
                    decimal: true,
                    signed: false,
                  ),
                  decoration: collapsedInputDecoration.copyWith(
                    hintText: AppLocalizations.of(context)!.hintIPAddress,
                  ),
                  controller: _ipTextController,
                  validator: _validateAddress,
                ),
              ),
              ModalDialogSection(
                child: TextFormField(
                  cursorWidth: 1,
                  style: Theme.of(context).textTheme.bodyMedium,
                  keyboardType: const TextInputType.numberWithOptions(
                    decimal: false,
                    signed: false,
                  ),
                  decoration: collapsedInputDecoration.copyWith(
                    hintText: AppLocalizations.of(context)!.hintNetworkPort,
                  ),
                  controller: _portTextController,
                  validator: _validatePort,
                ),
              ),
              ModalDialogSection(
                child: Row(
                  mainAxisAlignment: MainAxisAlignment.end,
                  children: [
                    EpicLozengeButton(
                      label: EpicCommonLocalizations.of(context)!.menuButtonCancel,
                      color: Colors.transparent,
                      onPressed: () => Navigator.of(context).pop(),
                    ),
                    EpicLozengeButton(
                      label: EpicCommonLocalizations.of(context)!.menuButtonOK,
                      onPressed: _onConnect,
                    ),
                  ],
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }

  /// Callback for when the connect button is clicked or pressed.
  void _onConnect() {
    if (_formKey.currentState?.validate() != true) {
      return;
    }

    Navigator.of(context).pop(false);

    final ConnectionData connection;

    if (_ipTextController.text == _demoModeString) {
      connection = ConnectionData.forDemoMode();
    } else {
      connection = ConnectionData(
        name: AppLocalizations.of(context)!.connectScreenManualConnectionLabel,
        websocketAddress: InternetAddress(_ipTextController.text),
        websocketPort: int.parse(_portTextController.text),
      );
    }

    /// Form input validating has passed, we are calling [widget.connect] to initiate a connection with UE using
    /// [connection]
    widget.connect(connection);
  }

  /// String validation for validating IpAddresses.
  String? _validateAddress(String? text) {
    if (text == _demoModeString) {
      return null;
    }

    if (text == null) {
      return AppLocalizations.of(context)!.formErrorInvalidIPAddress;
    }

    try {
      InternetAddress(text);
    } catch (e) {
      return AppLocalizations.of(context)!.formErrorInvalidIPAddress;
    }
    return null;
  }

  /// String validation for validating ports.
  String? _validatePort(String? text) {
    if (text == null) {
      return AppLocalizations.of(context)!.formErrorInvalidNetworkPort;
    }

    try {
      final port = int.parse(text);
      if (port > 65535) {
        return AppLocalizations.of(context)!.formErrorInvalidNetworkPort;
      }
    } catch (e) {
      return AppLocalizations.of(context)!.formErrorInvalidNetworkPort;
    }
    return null;
  }
}
