// Copyright Epic Games, Inc. All Rights Reserved.

import 'dart:async';

import 'package:epic_common/theme.dart';
import 'package:epic_common/widgets.dart';
import 'package:flutter/material.dart';
import 'package:flutter_gen/gen_l10n/app_localizations.dart';

import '../../../../models/navigator_keys.dart';
import '../mixins/connect_mixin.dart';
import 'manual_connect_form.dart';

/// Button or Tile used for manually connecting to an instance of UE.
class ManualConnectTile extends StatefulWidget {
  const ManualConnectTile({Key? key}) : super(key: key);

  @override
  State<ManualConnectTile> createState() => _ManualConnectTileState();
}

class _ManualConnectTileState extends State<ManualConnectTile> with ConnectMixin {
  /// Show the input dialog/modal for connecting manually to an instance of UE in the current [context].
  Future<dynamic> _showManualConnectionInput(BuildContext context) async {
    final route = GenericModalDialogRoute(
      bResizeToAvoidBottomInset: true,
      builder: (_) => ModalDialogCard(
        child: ManualConnectForm(connect: connect),
      ),
    );

    return Navigator.of(rootNavigatorKey.currentContext!, rootNavigator: true).push(route);
  }

  @override
  Widget build(BuildContext context) {
    return InkWell(
      onTap: () => _showManualConnectionInput(context),
      child: Container(
        width: 170,
        height: 166,
        decoration: BoxDecoration(
          borderRadius: BorderRadius.circular(UnrealTheme.cardCornerRadius),
          color: Theme.of(context).colorScheme.secondary,
        ),
        padding: EdgeInsets.all(10),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.center,
          mainAxisAlignment: MainAxisAlignment.center,
          mainAxisSize: MainAxisSize.min,
          children: [
            AssetIcon(
              path: 'packages/epic_common/assets/icons/unreal_u.svg',
              size: 50,
            ),
            SizedBox(height: 16),
            Text(
              AppLocalizations.of(context)!.connectScreenNewConnectionButtonTitle,
              style: Theme.of(context).textTheme.titleLarge!.copyWith(
                    letterSpacing: 0.5,
                    color: Theme.of(context).colorScheme.onPrimary,
                    height: 1.5,
                  ),
              textAlign: TextAlign.center,
            ),
            SizedBox(height: 5),
            Text(
              AppLocalizations.of(context)!.connectScreenNewConnectionButtonLabel,
              style: Theme.of(context).textTheme.labelMedium!.copyWith(
                    color: Theme.of(context).colorScheme.onPrimary,
                    fontStyle: FontStyle.italic,
                  ),
            ),
          ],
        ),
      ),
    );
  }
}
