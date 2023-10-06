// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:flutter/material.dart';

import '../../../../utilities/net_utilities.dart';
import 'connection_tile.dart';
import 'manual_connect_tile.dart';

/// Quick action column showing most recent connection instances of UE.
class QuickActionGrid extends StatelessWidget {
  const QuickActionGrid({Key? key, required this.recentConnections}) : super(key: key);

  ///List of most recent/saved instances of UE connection.
  final List<ConnectionData> recentConnections;

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: EdgeInsets.all(32),
      child: Column(
        children: [
          ...recentConnections.map((e) => Padding(
                padding: EdgeInsets.only(bottom: 32),
                child: ConnectionTile(
                  connectionData: e,
                  bPrimary: true,
                ),
              )),
          ManualConnectTile(),
        ],
      ),
    );
  }
}
