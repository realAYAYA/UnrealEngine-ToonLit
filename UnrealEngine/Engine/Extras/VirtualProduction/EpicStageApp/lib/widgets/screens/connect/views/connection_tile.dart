// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:epic_common/theme.dart';
import 'package:epic_common/widgets.dart';
import 'package:flutter/material.dart';
import 'package:flutter_gen/gen_l10n/app_localizations.dart';

import '../../../../utilities/net_utilities.dart';
import '../mixins/connect_mixin.dart';

/// Connection tile/item used to visually represent available instances of UE connections.
class ConnectionTile extends StatefulWidget {
  const ConnectionTile({
    Key? key,
    required this.connectionData,
    this.bFilled = true,
    this.bPrimary = false,
  }) : super(key: key);

  /// [connectionData] essential to connecting with an instance of UE.
  final ConnectionData connectionData;

  /// Whether the item is filled or has only border, default to `true`.
  final bool bFilled;

  /// Whether the item should have the primary color, default to `false`.
  final bool bPrimary;

  @override
  State<ConnectionTile> createState() => _ConnectionTileState();
}

class _ConnectionTileState extends State<ConnectionTile> with ConnectMixin {
  /// State instance for [widget.connectionData], for easy access across child widgets.
  late final ConnectionData data;

  @override
  void initState() {
    data = widget.connectionData;
    super.initState();
  }

  @override
  Widget build(BuildContext context) {
    late final Color fillColor;
    if (widget.bFilled) {
      if (widget.bPrimary) {
        fillColor = Theme.of(context).colorScheme.primary;
      } else {
        fillColor = Theme.of(context).colorScheme.secondary;
      }
    } else {
      fillColor = Colors.transparent;
    }

    late final Color textColor = widget.bFilled ? Theme.of(context).colorScheme.onPrimary : UnrealColors.gray56;

    return InkWell(
      onTap: () => connect(data),
      child: Container(
        width: 170,
        height: 166,
        padding: EdgeInsets.symmetric(horizontal: 10),
        decoration: BoxDecoration(
          borderRadius: BorderRadius.circular(UnrealTheme.cardCornerRadius),
          color: fillColor,
          border: widget.bFilled
              ? null
              : Border.all(
                  color: Theme.of(context).colorScheme.onSurface.withOpacity(0.2),
                  width: 2,
                ),
        ),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.center,
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            AssetIcon(
              path: 'packages/epic_common/assets/icons/unreal_u.svg',
              size: 50,
              color: widget.bFilled ? UnrealColors.white : UnrealColors.gray75,
            ),
            SizedBox(height: 16),
            Text(
              '${widget.connectionData.websocketAddress.address}:${widget.connectionData.websocketPort}',
              style: Theme.of(context).textTheme.titleLarge!.copyWith(
                    letterSpacing: 0.5,
                    color: textColor,
                  ),
              textAlign: TextAlign.center,
              overflow: TextOverflow.ellipsis,
            ),
            SizedBox(height: 5),
            Text(
              widget.connectionData.name,
              style: Theme.of(context).textTheme.labelMedium!.copyWith(
                    color: textColor,
                  ),
              textAlign: TextAlign.center,
              overflow: TextOverflow.ellipsis,
            ),
            SizedBox(height: 5),
            widget.bFilled
                ? Text(
                    AppLocalizations.of(context)!.connectScreenMostRecentButtonLabel,
                    style: Theme.of(context).textTheme.labelMedium!.copyWith(
                          color: textColor,
                          fontStyle: FontStyle.italic,
                        ),
                  )
                : const SizedBox(),
          ],
        ),
      ),
    );
  }
}
