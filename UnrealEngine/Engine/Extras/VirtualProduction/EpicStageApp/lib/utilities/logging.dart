// Copyright Epic Games, Inc. All Rights Reserved.

import 'dart:io';

import 'package:intl/intl.dart';
import 'package:logging/logging.dart';
import 'package:path_provider/path_provider.dart';

final _timeFormatter = DateFormat('HH:mm:ss.sss');
final _fileDateFormatter = DateFormat('yyyy_MM_dd');
final _friendlyDateFormatter = DateFormat.yMMMMd();
final _logDateFromName = RegExp(r'epic_stage_app_(.+)\.log');

final _log = Logger('Logging');

/// Color tokens to prepend to logs at each level.
final Map<Level, String> _logColorTokens = {
  Level.CONFIG: '\x1B[32m',
  Level.INFO: '\x1B[34m',
  Level.WARNING: '\x1B[93m',
  Level.SEVERE: '\x1B[91m',
  Level.SHOUT: '\x1B[95m',
};

class Logging {
  /// Singleton instance of this class.
  static Logging instance = Logging();

  /// Whether logging has been initialized.
  bool _bIsInitialized = false;

  /// The sink to which log data should be written to send it to a file.
  IOSink? _logFileSink;

  /// A future that will return [_logFileSink] once it's created.
  Future<IOSink>? _logFileSinkFuture;

  /// The date when the log file was last updated.
  DateTime? _lastLogChangeTime;

  /// If true, it's time to change to a new log file.
  bool get _bNeedsLogChange {
    // We haven't started or finished creating a log
    if (_logFileSinkFuture == null) {
      return true;
    }

    // We have a file, so check that it isn't an outdated path
    if (_logFileSink != null) {
      assert(_lastLogChangeTime != null);

      final now = DateTime.now();
      return _fileDateFormatter.format(now) != _fileDateFormatter.format(_lastLogChangeTime!);
    }

    return false;
  }

  /// Given a log file, get the date on which it was created, or null if it couldn't be determined.
  static DateTime? getDateForLog(File logFile) {
    final name = logFile.uri.pathSegments.last;
    final String? match = _logDateFromName.firstMatch(name)?[1];

    if (match != null) {
      try {
        return _fileDateFormatter.parse(match);
      } catch (error) {
        return null;
      }
    }

    return null;
  }

  /// Given a log file, make a user-friendly string to refer to it by.
  static String getNameForLog(File logFile) {
    String title = logFile.uri.pathSegments.last;

    final DateTime? logDate = Logging.getDateForLog(logFile);
    if (logDate != null) {
      title += ' (${_friendlyDateFormatter.format(logDate)})';
    }

    return title;
  }

  /// Initialize logging for the app.
  void initialize() {
    if (_bIsInitialized) {
      _log.warning('Tried to initialize logging more than once');
      return;
    }

    _bIsInitialized = true;

    Logger.root.level = Level.ALL;
    Logger.root.onRecord.listen(_onLogEvent);
  }

  /// Clean up logging for the app.
  void cleanUp() {
    _logFileSink?.close();

    _bIsInitialized = false;
  }

  /// Get the directory in which to store logs.
  Future<Directory> getLogDirectory() async {
    final Directory documentRoot = await getApplicationSupportDirectory();

    return Directory('${documentRoot.path}/logs');
  }

  /// Get a reference to the current sink to which log data should be written.
  Future<IOSink> _getActiveLogFileSink() {
    if (!_bNeedsLogChange) {
      return _logFileSinkFuture!;
    }

    // Flush and close the old sink if it exists
    if (_logFileSink != null) {
      final IOSink previousSink = _logFileSink!;
      previousSink.flush().then((value) => previousSink.close());
    }

    _logFileSinkFuture = getLogDirectory().then((Directory directory) async {
      try {
        await directory.create(recursive: true);
      } catch (e) {
        return Future.error(e);
      }

      final fileName = 'epic_stage_app_${_fileDateFormatter.format(DateTime.now())}.log';
      final logFile = File('${directory.path}/$fileName');

      _logFileSink = logFile.openWrite(mode: FileMode.append);

      if (_logFileSink != null) {
      	_lastLogChangeTime = DateTime.now();
        return _logFileSink!;
      }

      return Future.error('Failed to open log "$fileName"');
    });

    return _logFileSinkFuture!;
  }

  /// Called when a log event is received.
  void _onLogEvent(LogRecord record) {
    String formatted =
        '[${_timeFormatter.format(record.time)}] ${record.level.name}/${record.loggerName}: ${record.message}';
    if (record.error != null) {
      formatted += '\n${record.error}';
    }

    if (record.stackTrace != null) {
      formatted += '\n${record.stackTrace}';
    }

    _logToConsole(formatted, record);
    _logToFile(formatted, record);
  }

  /// Send a log to the debug console.
  void _logToConsole(String formatted, LogRecord record) {
    final resetToken = '\x1B[0m';
    final String colorToken = _logColorTokens[record.level] ?? '';
    print('$colorToken$formatted$resetToken');
  }

  /// Send a log to the log file.
  void _logToFile(String formatted, LogRecord record) async {
    final IOSink sink = await _getActiveLogFileSink();
    sink.writeln(formatted);
  }
}
