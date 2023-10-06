// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:flutter/material.dart';
import 'package:flutter_gen/gen_l10n/app_localizations.dart';

import '../../../../utilities/net_utilities.dart';
import '../../../elements/asset_icon.dart';
import '../../../elements/epic_icon_button.dart';
import '../../../elements/parsed_rich_text.dart';

/// Error modal for when there's an error connecting to an instance of UE.
class ErrorModal extends StatelessWidget {
  const ErrorModal({Key? key, required this.data, this.reconnect}) : super(key: key);

  /// connection data/details used for connecting to UE when the error occurred.
  final ConnectionData data;

  /// callback function for reconnecting to an instance of UE after an error/failure using [data].
  final VoidCallback? reconnect;

  @override
  Widget build(BuildContext context) {
    return Container(
      width: MediaQuery.of(context).size.height * 0.5,
      padding: EdgeInsets.symmetric(horizontal: 16, vertical: 8),
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          Row(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              AssetIcon(
                path: 'assets/images/icons/alert_triangle_large.svg',
                size: 24,
              ),
              SizedBox(width: 10),
              Flexible(
                child: Text(
                  AppLocalizations.of(context)!.connectScreenFailedDialogTitle,
                  style: Theme.of(context).textTheme.displayLarge,
                  overflow: TextOverflow.fade,
                ),
              ),
            ],
          ),
          const SizedBox(height: 24),
          ParsedRichText(
            AppLocalizations.of(context)!
                .connectScreenFailedDialogMessage('${data.websocketAddress.address}:${data.websocketPort}'),
            style: Theme.of(context).textTheme.bodyMedium,
          ),
          const SizedBox(height: 30),
          Row(
            mainAxisAlignment: MainAxisAlignment.end,
            children: [
              EpicLozengeButton(
                onPressed: () => Navigator.of(context).pop(),
                label: AppLocalizations.of(context)!.menuButtonCancel,
                color: Colors.transparent,
              ),
              SizedBox(width: 16),
              EpicLozengeButton(
                onPressed: () {
                  Navigator.of(context).pop();
                  reconnect?.call();
                },
                label: AppLocalizations.of(context)!.menuButtonRetry,
              ),
            ],
          ),
        ],
      ),
    );
  }
}
