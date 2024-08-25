// Copyright Epic Games, Inc. All Rights Reserved.

import 'dart:io';

import 'package:epic_common/logging.dart';
import 'package:epic_common/widgets.dart';
import 'package:flutter/material.dart';
import 'package:share_plus/share_plus.dart';

import '../../../../../../utilities/guarded_refresh_state.dart';

/// Arguments passed to the route when a [SettingsLogView] is pushed to the navigation stack.
class SettingsLogViewArguments {
  const SettingsLogViewArguments({required this.file, required this.bIsCurrent});

  /// The file to open.
  final File file;

  /// Whether this is the current log.
  final bool bIsCurrent;
}

/// Page showing the full view of a log.
class SettingsLogView extends StatefulWidget {
  const SettingsLogView({Key? key}) : super(key: key);

  static const String route = '/log_view';

  @override
  State<SettingsLogView> createState() => _SettingsLogViewState();
}

class _SettingsLogViewState extends State<SettingsLogView> with GuardedRefreshState {
  final shareButtonKey = GlobalKey();

  @override
  Widget build(BuildContext context) {
    final arguments = ModalRoute.of(context)?.settings.arguments as SettingsLogViewArguments;

    return SettingsPageScaffold(
      title: Logging.makeNameForLog(
        context: context,
        logFile: arguments.file,
        bIsCurrent: arguments.bIsCurrent,
      ),
      titleBarTrailing: EpicIconButton(
        key: shareButtonKey,
        iconPath: 'packages/epic_common/assets/icons/share.svg',
        color: Theme.of(context).colorScheme.primary,
        onPressed: () => _shareLog(arguments.file),
      ),
      body: SizedBox(
        height: MediaQuery.of(context).size.height - 120,
        child: LogFileViewer(arguments.file),
      ),
    );
  }

  /// Open a prompt to share the log's contents using the local operating system.
  void _shareLog(File log) async {
    // Need to provide this position for sharing to work on iPad
    final renderBox = shareButtonKey.currentContext?.findRenderObject() as RenderBox?;
    if (renderBox == null) {
      return;
    }

    final sharePosition = renderBox.localToGlobal(Offset.zero) & renderBox.size;

    Share.share(
      await log.readAsString(),
      subject: log.uri.pathSegments.last,
      sharePositionOrigin: sharePosition,
    );
  }
}
