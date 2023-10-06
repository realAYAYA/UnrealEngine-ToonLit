// Copyright Epic Games, Inc. All Rights Reserved.

import 'dart:async';
import 'dart:isolate';
import 'dart:ui';

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_gen/gen_l10n/app_localizations.dart';
import 'package:logging/logging.dart';
import 'package:provider/provider.dart';
import 'package:streaming_shared_preferences/streaming_shared_preferences.dart';

import 'models/engine_connection.dart';
import 'models/navigator_keys.dart';
import 'models/preview_render_manager.dart';
import 'models/settings/delta_widget_settings.dart';
import 'models/settings/main_screen_settings.dart';
import 'models/settings/recent_actor_settings.dart';
import 'models/settings/selected_actor_settings.dart';
import 'models/settings/stage_map_settings.dart';
import 'models/unreal_actor_creator.dart';
import 'models/unreal_actor_manager.dart';
import 'models/unreal_property_manager.dart';
import 'models/unreal_transaction_manager.dart';
import 'routes.dart';
import 'utilities/constants.dart';
import 'utilities/logging.dart';
import 'utilities/preferences_bundle.dart';
import 'utilities/transient_preference.dart';
import 'utilities/unreal_colors.dart';
import 'widgets/elements/epic_scroll_view.dart';
import 'widgets/screens/connect/connect.dart';
import 'widgets/screens/eula/eula_screen.dart';

final _log = Logger('Main');

void main() async {
  WidgetsFlutterBinding.ensureInitialized();

  Logging.instance.initialize();

  final asyncLog = Logger('Async');
  final flutterLog = Logger('Flutter');
  final isolateLog = Logger('Isolate');

  preloadShaders();

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

  // Catch any asynchronous errors
  runZonedGuarded(() {
    // Run the app itself
    runApp(EpicStageApp(preferences: preferences));
  }, (error, stack) {
    asyncLog.severe(error, error, stack);
  });
}

void preloadShaders() async {
  await EpicScrollView.preloadShaders();
}

void unloadShaders() {
  EpicScrollView.unloadShaders();
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

    const colorScheme = ColorScheme.dark(
      primary: UnrealColors.highlightBlue,
      secondary: Color(0xff575757),
      onPrimary: UnrealColors.white,
      onSecondary: UnrealColors.white,
      background: UnrealColors.gray06,
      surfaceVariant: UnrealColors.gray10,
      surface: UnrealColors.gray14,
      surfaceTint: UnrealColors.gray18,
      onSurface: UnrealColors.gray75,
      shadow: Colors.transparent,
    );

    const textTheme = const TextTheme(
      bodyMedium: TextStyle(
        color: UnrealColors.gray75,
        decorationColor: UnrealColors.gray75,
        fontVariations: [FontVariation('wght', 400)],
        fontSize: 14,
      ),
      displayLarge: TextStyle(
        color: UnrealColors.white,
        decorationColor: UnrealColors.white,
        fontVariations: [FontVariation('wght', 700)],
        fontSize: 14,
      ),
      displayMedium: TextStyle(
        color: UnrealColors.white,
        decorationColor: UnrealColors.white,
        fontVariations: [FontVariation('wght', 400)],
        fontSize: 14,
      ),
      headlineSmall: TextStyle(
        color: UnrealColors.gray75,
        decorationColor: UnrealColors.gray75,
        fontVariations: [FontVariation('wght', 600)],
        letterSpacing: 0.25,
        fontSize: 14,
      ),
      titleLarge: TextStyle(
        color: UnrealColors.gray90,
        decorationColor: UnrealColors.gray90,
        fontVariations: [FontVariation('wght', 700)],
        letterSpacing: 1,
        fontSize: 12,
      ),
      labelMedium: TextStyle(
        color: UnrealColors.gray56,
        decorationColor: UnrealColors.gray56,
        fontVariations: [FontVariation('wght', 400)],
        letterSpacing: 0.5,
        fontSize: 12,
      ),
    );

    return MultiProvider(
      providers: [
        Provider<PreferencesBundle>(create: (_) => _preferenceBundle),
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
      ],
      child: MaterialApp(
        theme: ThemeData(
          fontFamily: 'Inter',

          textTheme: textTheme,
          colorScheme: colorScheme,

          // Disable Material "ink" splash effects
          splashFactory: NoSplash.splashFactory,

          scaffoldBackgroundColor: UnrealColors.gray10,
          hoverColor: Colors.transparent,
          highlightColor: Colors.transparent,

          textButtonTheme: TextButtonThemeData(
            style: ButtonStyle(
              overlayColor: MaterialStateProperty.all(Colors.transparent),
            ),
          ),

          checkboxTheme: CheckboxThemeData(
            overlayColor: MaterialStateProperty.all(Colors.transparent),
          ),

          appBarTheme: const AppBarTheme(
            shadowColor: Colors.transparent,
          ),

          tooltipTheme: const TooltipThemeData(
            waitDuration: Duration(milliseconds: 700),
            decoration: BoxDecoration(
              image: DecorationImage(
                image: AssetImage('assets/images/decoration/tooltip.png'),
                centerSlice: Rect.fromLTRB(4, 4, 60, 60),
              ),
            ),
          ),

          textSelectionTheme: const TextSelectionThemeData(
            cursorColor: UnrealColors.white,
          ),

          inputDecorationTheme: InputDecorationTheme(
            filled: true,
            outlineBorder: BorderSide.none,
            border: OutlineInputBorder(
              borderRadius: BorderRadius.circular(4),
              borderSide: BorderSide.none,
            ),
            enabledBorder: OutlineInputBorder(
              borderRadius: BorderRadius.circular(4),
              borderSide: BorderSide.none,
            ),
            focusedBorder: OutlineInputBorder(
              borderRadius: BorderRadius.circular(4),
              borderSide: BorderSide.none,
            ),
            errorBorder: OutlineInputBorder(
              borderRadius: BorderRadius.circular(4),
              borderSide: BorderSide.none,
            ),
            fillColor: colorScheme.background,
            hoverColor: colorScheme.background,
            contentPadding: const EdgeInsets.symmetric(
              horizontal: 12,
            ),
            hintStyle: textTheme.bodyMedium!.copyWith(color: UnrealColors.gray56),
          ),

          listTileTheme: const ListTileThemeData(
            textColor: UnrealColors.gray75,
            iconColor: UnrealColors.gray75,
            selectedColor: UnrealColors.white,
            selectedTileColor: UnrealColors.highlightBlue,
          ),

          scrollbarTheme: const ScrollbarThemeData(
            thumbColor: MaterialStatePropertyAll(UnrealColors.gray22),
            thickness: MaterialStatePropertyAll(8),
            radius: Radius.circular(4),
            mainAxisMargin: cardCornerRadius,
            crossAxisMargin: cardCornerRadius,
          ),

          cardTheme: CardTheme(
            color: UnrealColors.gray10,
            clipBehavior: Clip.antiAlias,
            shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(cardCornerRadius)),
            margin: EdgeInsets.zero,
            shadowColor: Colors.transparent,
          ),
        ),
        localizationsDelegates: AppLocalizations.localizationsDelegates,
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
