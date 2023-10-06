// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:logging/logging.dart';

import '../models/navigator_keys.dart';
import '../widgets/elements/modal.dart';

final _log = Logger('DebugAlert');

/// Show an alert to the user with the given message.
void showDebugAlert(String message) {
  if (rootNavigatorKey.currentContext == null) {
    return;
  }

  _log.info(message);
  InfoModalDialog.show(message);
}
