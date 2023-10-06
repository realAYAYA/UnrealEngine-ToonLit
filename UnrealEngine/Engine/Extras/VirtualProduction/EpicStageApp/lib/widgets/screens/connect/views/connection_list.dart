// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:flutter/material.dart';

import '../../../../utilities/net_utilities.dart';
import 'connection_tile.dart';

/// A widget showing a list of available UE connection instances.
class ConnectionList extends StatelessWidget {
  const ConnectionList({Key? key, required this.connections}) : super(key: key);

  /// List of UE connection instances to be rendered.
  final List<ConnectionData> connections;

  @override
  Widget build(BuildContext context) {
    return SingleChildScrollView(
      padding: EdgeInsets.all(32),
      child: Wrap(
        direction: Axis.horizontal,
        alignment: WrapAlignment.center,
        runAlignment: WrapAlignment.start,
        crossAxisAlignment: WrapCrossAlignment.start,
        spacing: 16,
        runSpacing: 32,
        children: [
          for (final ConnectionData connection in connections)
            ConnectionTile(
              connectionData: connection,
              bFilled: false,
            ),
        ],
      ),
    );
  }
}
