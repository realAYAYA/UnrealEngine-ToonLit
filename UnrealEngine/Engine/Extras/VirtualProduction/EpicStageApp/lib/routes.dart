// Copyright Epic Games, Inc. All Rights Reserved.

import 'widgets/screens/eula/eula_screen.dart';
import 'package:flutter/widgets.dart';

import 'widgets/screens/connect/connect.dart';
import 'widgets/screens/main/stage_app_main_screen.dart';
import 'widgets/screens/reconnect_screen.dart';

/// Contains data about a route in the app
class RouteData {
  const RouteData({required this.createScreen});

  /// A function to create the screen widget for the route.
  final Widget Function(BuildContext context) createScreen;

  /// A list of all routes in the app.
  static final Map<String, RouteData> allRoutes = {
    ConnectScreen.route: RouteData(
      createScreen: (context) => const ConnectScreen(),
    ),
    ReconnectScreen.route: RouteData(
      createScreen: (context) => const ReconnectScreen(),
    ),
    StageAppMainScreen.route: RouteData(
      createScreen: (context) => const StageAppMainScreen(),
    ),
    EulaScreen.route: RouteData(createScreen: (context) => const EulaScreen())
  };
}

final Map<String, String> routes = {};
