// Copyright Epic Games, Inc. All Rights Reserved.

import 'dart:async';
import 'dart:isolate';

import 'package:epic_common/localizations.dart';
import 'package:epic_common/logging.dart';
import 'package:epic_common/preferences.dart';
import 'package:epic_common/theme.dart';
import 'package:epic_common/utilities/version.dart';
import 'package:epic_common/widgets.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_gen/gen_l10n/app_localizations.dart';
import 'package:logging/logging.dart';
import 'package:provider/provider.dart';
import 'package:streaming_shared_preferences/streaming_shared_preferences.dart';

import 'models/engine_connection.dart';
import 'models/engine_passphrase_manager.dart';
import 'models/navigator_keys.dart';
import 'models/preview_render_manager.dart';
import 'models/settings/connection_settings.dart';
import 'models/settings/delta_widget_settings.dart';
import 'models/settings/main_screen_settings.dart';
import 'models/settings/recent_actor_settings.dart';
import 'models/settings/selected_actor_settings.dart';
import 'models/settings/stage_map_settings.dart';
import 'models/unreal_actor_creator.dart';
import 'models/unreal_actor_manager.dart';
import 'models/unreal_dockable_tab_manager.dart';
import 'models/unreal_property_manager.dart';
import 'models/unreal_transaction_manager.dart';
import 'routes.dart';
import 'widgets/screens/connect/connect.dart';
import 'widgets/screens/eula/eula_screen.dart';

final _log = Logger('Main');

void main() async {
  Logging.instance.initialize('epic_stage_app');

  final asyncLog = Logger('Async');

  // Catch any asynchronous errors
  runZonedGuarded(
    () => initAndRunApp(),
    (error, stack) {
      asyncLog.severe(error, error, stack);
    },
  );
}

/// Set up the app and run it. Encapsulated so we can wrap it in [runZonedGuarded] in [main] for better error handling.
void initAndRunApp() async {
  WidgetsFlutterBinding.ensureInitialized();

  _log.info(await getVerbosePackageVersion());

  final flutterLog = Logger('Flutter');
  final isolateLog = Logger('Isolate');

  await preloadShaders();

  // Catch any errors originating from or otherwise passed on by Flutter
  FlutterError.onError = (FlutterErrorDetails details) {
    flutterLog.severe(details.exception, details.exception, details.stack);
  };

  // Catch errors outside the Flutter context
  Isolate.current.addErrorListener(RawReceivePort((errorStackPair) {
    isolateLog.severe(errorStackPair.first, errorStackPair.first, StackTrace.fromString(errorStackPair.second));
  }).sendPort);

  // Load user settings
  final preferences = await StreamingSharedPreferences.instance;

  // Run the app itself
  runApp(EpicStageApp(preferences: preferences));
}

/// Preload any shaders we expect to use regularly.
Future preloadShaders() async {
  await EpicCommonWidgets.preloadShaders();
}

class EpicStageApp extends StatefulWidget {
  const EpicStageApp({Key? key, required this.preferences}) : super(key: key);

  final StreamingSharedPreferences preferences;

  @override
  _EpicStageAppState createState() => _EpicStageAppState();
}

class _EpicStageAppState extends State<EpicStageApp> with TickerProviderStateMixin, WidgetsBindingObserver {
  late final PreferencesBundle _preferenceBundle;

  /// Whether or not the user has accepted the EULA agreement.
  bool bHasAcceptedEula = false;

  @override
  void initState() {
    super.initState();

    _log.info('App initialized');

    _preferenceBundle = PreferencesBundle(widget.preferences, TransientSharedPreferences());

    bHasAcceptedEula = widget.preferences.getBool('common.bHasAcceptedEula', defaultValue: false).getValue();

    WidgetsBinding.instance.addObserver(this);
  }

  @override
  void dispose() {
    WidgetsBinding.instance.removeObserver(this);

    _log.info('App disposed');

    super.dispose();
  }

  @override
  void didChangeAppLifecycleState(AppLifecycleState state) {
    super.didChangeAppLifecycleState(state);

    switch (state) {
      case AppLifecycleState.detached:
        _log.info('App detached');
        break;

      case AppLifecycleState.inactive:
        _log.info('App inactive');
        break;

      case AppLifecycleState.paused:
        _log.info('App paused');
        break;

      case AppLifecycleState.resumed:
        _log.info('App resumed');
        break;
    }
  }

  @override
  Widget build(BuildContext context) {
    // Force landscape layout on mobile devices
    SystemChrome.setPreferredOrientations([
      DeviceOrientation.landscapeLeft,
      DeviceOrientation.landscapeRight,
    ]);

    var routes = {for (var entry in RouteData.allRoutes.entries) entry.key: entry.value.createScreen};

    if (!bHasAcceptedEula) {
      routes['/'] = routes[EulaScreen.route]!;
    } else {
      routes['/'] = routes[ConnectScreen.route]!;
    }

    return MultiProvider(
      providers: [
        Provider<EnginePassphraseManager>(create: (_) => EnginePassphraseManager()),
        Provider<PreferencesBundle>(create: (_) => _preferenceBundle),
        Provider<ConnectionSettings>(create: (_) => ConnectionSettings(_preferenceBundle)),
        Provider<SelectedActorSettings>(create: (_) => SelectedActorSettings(_preferenceBundle)),
        Provider<StageMapSettings>(create: (_) => StageMapSettings(_preferenceBundle)),
        Provider<RecentActorSettings>(create: (_) => RecentActorSettings(_preferenceBundle)),
        Provider<MainScreenSettings>(create: (_) => MainScreenSettings(_preferenceBundle)),
        Provider<DeltaWidgetSettings>(create: (_) => DeltaWidgetSettings(_preferenceBundle)),
        Provider<EngineConnectionManager>(
          create: (context) => EngineConnectionManager(context),
          dispose: (_, value) => value.dispose(),
        ),
        Provider<UnrealTransactionManager>(create: (context) => UnrealTransactionManager(context)),
        Provider<UnrealPropertyManager>(create: (context) => UnrealPropertyManager(context)),
        Provider<UnrealActorManager>(create: (context) => UnrealActorManager(context)),
        Provider<PreviewRenderManager>(
          create: (context) => PreviewRenderManager(context),
          dispose: (context, value) => value.dispose(),
        ),
        Provider<UnrealActorCreator>(
          create: (context) => UnrealActorCreator(context),
          dispose: (context, value) => value.dispose(),
        ),
        Provider<UnrealDockableTabManager>(
          create: (context) => UnrealDockableTabManager(context),
          dispose: (context, value) => value.dispose(),
        )
      ],
      child: MaterialApp(
        theme: UnrealTheme.makeThemeData(),
        localizationsDelegates: [
          ...AppLocalizations.localizationsDelegates,
          ...EpicCommonLocalizations.localizationsDelegates,
        ],
        supportedLocales: AppLocalizations.supportedLocales,
        navigatorKey: rootNavigatorKey,
        onGenerateTitle: (context) => AppLocalizations.of(context)!.appTitle,
        onGenerateRoute: (RouteSettings settings) {
          Widget Function(BuildContext)? pageFunction = routes[settings.name];
          if (pageFunction != null) {
            return PageRouteBuilder(
              settings: settings,
              // Add safe area on all pages to avoid overlapping the status bar
              pageBuilder: (context, animation, secondaryAnimation) => Container(
                color: Theme.of(context).colorScheme.background,
                child: SafeArea(
                  left: false,
                  right: false,
                  bottom: false,
                  top: true,
                  child: pageFunction(context),
                ),
              ),
              transitionDuration: Duration.zero,
            );
          }

          return MaterialPageRoute(builder: (_) => Text("Invalid page ${settings.name ?? "(null)"}"));
        },
      ),
    );
  }
}
