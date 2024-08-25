// Copyright Epic Games, Inc. All Rights Reserved.

import 'dart:io';

import 'package:epic_common/logging.dart';
import 'package:epic_common/widgets.dart';
import 'package:flutter/material.dart';
import 'package:flutter_gen/gen_l10n/app_localizations.dart';

import '../../../../../../utilities/guarded_refresh_state.dart';
import 'settings_log_view.dart';

/// Page showing the list of log files.
class SettingsLogList extends StatefulWidget {
  const SettingsLogList({Key? key}) : super(key: key);

  static const String route = '/logs';

  @override
  State<SettingsLogList> createState() => _SettingsLogListState();
}

class _SettingsLogListState extends State<SettingsLogList> with GuardedRefreshState {
  /// Scroll controller for the scroll view of the logs.
  final _scrollController = ScrollController();

  /// List of files to display.
  List<File> _logFiles = [];

  @override
  void initState() {
    super.initState();

    _updateLogFileList();
  }

  @override
  void dispose() {
    _scrollController.dispose();
    super.dispose();
  }

  /// Update the list of log files.
  void _updateLogFileList() async {
    _logFiles = await Logging.getAppLogFiles();
    setState(() {});
  }

  @override
  Widget build(BuildContext context) {
    return SettingsPageScaffold(
      title: AppLocalizations.of(context)!.settingsDialogApplicationLogLabel,
      body: SizedBox(
        height: 240,
        child: MediaQuery.removePadding(
          removeTop: true,
          removeBottom: true,
          context: context,
          child: EpicListView(
            itemCount: _logFiles.length,
            itemBuilder: (context, index) {
              final File file = _logFiles[index];
              final bool bIsCurrent = index == 0;

              return SettingsMenuItem(
                title: Logging.makeNameForLog(
                  context: context,
                  logFile: file,
                  bIsCurrent: bIsCurrent,
                ),
                iconPath: 'packages/epic_common/assets/icons/log.svg',
                onTap: () => Navigator.of(context).pushNamed(
                  SettingsLogView.route,
                  arguments: SettingsLogViewArguments(
                    file: file,
                    bIsCurrent: bIsCurrent,
                  ),
                ),
              );
            },
          ),
        ),
      ),
    );
  }
}
