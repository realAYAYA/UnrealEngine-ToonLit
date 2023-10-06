// Copyright Epic Games, Inc. All Rights Reserved.

import 'dart:io';

import 'package:flutter/material.dart';
import 'package:flutter_gen/gen_l10n/app_localizations.dart';

import '../../../../../../utilities/guarded_refresh_state.dart';
import '../../../../../../utilities/logging.dart';
import '../settings_generic.dart';
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
    final Directory logDirectory = await Logging.instance.getLogDirectory();
    final List<File> files =
        await logDirectory.list(recursive: false).map((FileSystemEntity entity) => File(entity.path)).toList();

    // Sort so the most recent files appear first. We assume that files are named identically aside from their dates,
    // which are assumed to be in lexicographical order (YYYY_MM_DD)
    files.sort((File fileA, File fileB) => -fileA.path.compareTo(fileB.path));

    setState(() {
      _logFiles = files;
    });
  }

  @override
  Widget build(BuildContext context) {
    return SettingsPageScaffold(
      title: AppLocalizations.of(context)!.settingsDialogApplicationLogLabel,
      body: SizedBox(
        height: 240,
        child: MediaQuery.removePadding(
          removeTop: true,
          context: context,
          child: Scrollbar(
            controller: _scrollController,
            child: ListView.builder(
              padding: EdgeInsets.zero,
              controller: _scrollController,
              itemCount: _logFiles.length,
              itemBuilder: (context, index) {
                final File file = _logFiles[index];

                return SettingsMenuItem(
                  title: Logging.getNameForLog(file),
                  iconPath: 'assets/images/icons/log.svg',
                  onTap: () => Navigator.of(context).pushNamed(
                    SettingsLogView.route,
                    arguments: SettingsLogViewArguments(
                      file: file,
                    ),
                  ),
                );
              },
            ),
          ),
        ),
      ),
    );
  }
}
