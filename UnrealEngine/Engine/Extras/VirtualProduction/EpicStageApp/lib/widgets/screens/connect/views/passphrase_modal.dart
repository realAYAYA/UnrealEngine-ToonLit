// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:epic_common/localizations.dart';
import 'package:epic_common/theme.dart';
import 'package:epic_common/widgets.dart';
import 'package:flutter/material.dart';
import 'package:flutter_gen/gen_l10n/app_localizations.dart';
import 'package:logging/logging.dart';
import 'package:provider/provider.dart';

import '../../../../models/engine_connection.dart';
import '../../../../models/engine_passphrase_manager.dart';
import '../../../../utilities/net_utilities.dart';
import '../mixins/connect_mixin.dart';

final _log = Logger('PassphraseModal');

/// A modal dialog that lets the user set a passphrase to use for the last attempted connection, then reconnects.
class PassphraseModalDialog extends StatefulWidget {
  const PassphraseModalDialog({Key? key, this.initialErrorMessage}) : super(key: key);

  /// The initial error message to show to the user, if any.
  final String? initialErrorMessage;

  @override
  State<StatefulWidget> createState() => _PassphraseModalDialogState();
}

class _PassphraseModalDialogState extends State<PassphraseModalDialog> with ConnectMixin {
  /// Text controller for the input field displayed to the user.
  final _textController = TextEditingController();

  /// The connection for which we're modifying the password.
  late final ConnectionData _connectionData;

  /// False if this is still retrieving connection and passphrase data.
  bool _bIsReady = false;

  /// Whether to show the passphrase to the user.
  bool _bShowPassphrase = false;

  /// Error message to show to the user, if any.
  String? _errorMessage;

  @override
  void initState() {
    super.initState();

    _errorMessage = widget.initialErrorMessage;
    _initAsync();
  }

  @override
  Widget build(BuildContext context) {
    const double buttonWidth = 110;
    final localizations = AppLocalizations.of(context)!;

    return ModalDialogCard(
      child: SizedBox(
        width: 300,
        child: IntrinsicHeight(
          child: _bIsReady
              ? Column(
                  children: [
                    ModalDialogTitle(title: localizations.connectScreenPasshphraseModalTitle),
                    if (_errorMessage != null)
                      ModalDialogSection(
                        child: Text(
                          _errorMessage!,
                          style: Theme.of(context).textTheme.bodyMedium!.copyWith(
                                color: UnrealColors.highlightRed,
                              ),
                          textAlign: TextAlign.start,
                        ),
                      ),
                    ModalDialogSection(
                      child: Column(
                        children: [
                          Row(
                            children: [
                              Expanded(
                                child: SizedBox(
                                  height: 36,
                                  child: TextField(
                                    autofocus: true,
                                    maxLines: 1,
                                    cursorWidth: 1,
                                    controller: _textController,
                                    keyboardAppearance: Brightness.dark,
                                    style: Theme.of(context).textTheme.bodyMedium,
                                    keyboardType: TextInputType.visiblePassword,
                                    onEditingComplete: _submit,
                                    obscureText: !_bShowPassphrase,
                                  ),
                                ),
                              ),
                            ],
                          ),
                          SizedBox(height: 12),
                          EpicCheckbox(
                            bChecked: _bShowPassphrase,
                            label: localizations.showPassphraseToggleLabel,
                            onPressed: () => setState(() {
                              _bShowPassphrase = !_bShowPassphrase;
                            }),
                          ),
                        ],
                      ),
                    ),
                    ModalDialogSection(
                      child: Row(
                        mainAxisAlignment: MainAxisAlignment.spaceAround,
                        children: [
                          EpicLozengeButton(
                            label: EpicCommonLocalizations.of(context)!.menuButtonCancel,
                            width: buttonWidth,
                            color: Colors.transparent,
                            onPressed: _cancel,
                          ),
                          EpicLozengeButton(
                            label: EpicCommonLocalizations.of(context)!.menuButtonSubmit,
                            width: buttonWidth,
                            onPressed: _submit,
                          ),
                        ],
                      ),
                    ),
                  ],
                )
              : ModalDialogSection(
                  child: Center(
                    child: SizedBox.square(
                      child: CircularProgressIndicator(),
                      dimension: 80,
                    ),
                  ),
                ),
        ),
      ),
    );
  }

  /// Retrieve the connection data and current password, then fill the text field with it.
  void _initAsync() async {
    final connectionManager = Provider.of<EngineConnectionManager>(context, listen: false);

    final ConnectionData? connectionData = await connectionManager.getLastConnectionData();
    if (connectionData == null) {
      _log.warning('Tried to set password but no last connection existed');
      Navigator.pop(context);
      return;
    }

    _connectionData = connectionData;

    final passphraseManager = Provider.of<EnginePassphraseManager>(context, listen: false);
    final String? passphrase = await passphraseManager.getPassphrase(connectionData);

    if (passphrase != null) {
      _textController.text = passphrase;
      _textController.selection = TextSelection(baseOffset: 0, extentOffset: _textController.text.length);
    }

    setState(() {
      _bIsReady = true;
    });
  }

  /// Save the new passphrase, then reconnect to the engine.
  void _submit() async {
    if (!_bIsReady) {
      return;
    }

    final passphraseManager = Provider.of<EnginePassphraseManager>(context, listen: false);
    passphraseManager.setPassphrase(_connectionData, _textController.text);

    final EngineConnectionResult result = await connect(
      _connectionData,
      // The user will just return to this modal, so we don't need to create another
      bShowPassphraseModalOnReject: false,
    );

    if (result == EngineConnectionResult.passphraseRejected) {
      setState(() {
        _errorMessage = AppLocalizations.of(context)!.connectScreenPassphraseIncorrectErrorMessage;
      });
    }
  }

  /// Cancel the interaction and pop the modal.
  void _cancel() {
    Navigator.pop(context);
  }
}
