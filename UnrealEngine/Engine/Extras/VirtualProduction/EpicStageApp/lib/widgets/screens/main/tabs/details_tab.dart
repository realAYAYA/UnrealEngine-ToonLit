// Copyright Epic Games, Inc. All Rights Reserved.

import 'dart:async';
import 'dart:collection';

import 'package:epic_common/preferences.dart';
import 'package:epic_common/theme.dart';
import 'package:epic_common/widgets.dart';
import 'package:flutter/material.dart';
import 'package:flutter_gen/gen_l10n/app_localizations.dart';
import 'package:provider/provider.dart';
import 'package:streaming_shared_preferences/streaming_shared_preferences.dart';

import '../../../../models/actor_data/light_card_actor_data.dart';
import '../../../../models/property_modify_operations.dart';
import '../../../../models/settings/details_tab_settings.dart';
import '../../../../models/settings/main_screen_settings.dart';
import '../../../../models/settings/selected_actor_settings.dart';
import '../../../../models/unreal_actor_manager.dart';
import '../../../../models/unreal_types.dart';
import '../../../../utilities/constants.dart';
import '../../../../utilities/guarded_refresh_state.dart';
import '../../../elements/delta_slider.dart';
import '../../../elements/dropdown_button.dart';
import '../../../elements/dropdown_text.dart';
import '../../../elements/reset_mode_button.dart';
import '../../../elements/stepper.dart';
import '../../../elements/unreal_property_builder.dart';
import '../sidebar/outliner_panel.dart';
import 'base_color_tab.dart';

/// Which type of actor is being shown in the details tab.
enum _DetailsActorType {
  lightCard,
  colorCorrectWindow,
  colorCorrectRegion,
  multiple,
}

/// A tab that lets the user edit basic details of the selected actors.
class DetailsTab extends StatefulWidget {
  const DetailsTab({Key? key}) : super(key: key);

  static const String iconPath = 'packages/epic_common/assets/icons/details.svg';

  static String getTitle(BuildContext context) => AppLocalizations.of(context)!.tabTitleDetails;

  @override
  State<DetailsTab> createState() => _DetailsTabState();
}

class _DetailsTabState extends State<DetailsTab> with GuardedRefreshState {
  late final UnrealActorManager _actorManager;
  late final SelectedActorSettings _selectedActorSettings;
  late final DetailsTabSettings _tabSettings;

  /// Stream subscriptions to user settings.
  final List<StreamSubscription> _settingSubscriptions = [];

  /// Classes for which this tab can modify properties.
  Set<String> get validClasses {
    final Set<String> classes = {lightCardClassName, colorCorrectRegionClassName};
    classes.addAll(colorCorrectWindowClassNames);
    return classes;
  }

  @override
  void initState() {
    super.initState();

    _actorManager = Provider.of<UnrealActorManager>(context, listen: false);
    _actorManager.watchClassName(lightCardClassName, _onActorUpdate);

    _selectedActorSettings = Provider.of<SelectedActorSettings>(context, listen: false);
    _settingSubscriptions.addAll([
      _selectedActorSettings.selectedActors.listen(refreshOnData),
    ]);

    _tabSettings = DetailsTabSettings(PreferencesBundle.of(context));
  }

  @override
  void dispose() {
    _actorManager.stopWatchingClassName(lightCardClassName, _onActorUpdate);

    for (final StreamSubscription subscription in _settingSubscriptions) {
      subscription.cancel();
    }

    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final _DetailsActorType? actorType = _getActorType();

    return Padding(
      padding: EdgeInsets.all(UnrealTheme.cardMargin),
      child: Row(
        children: [
          Expanded(
            child: Card(
              child: Column(children: [
                CardLargeHeader(
                  title: _getTitle(),
                  subtitle: DetailsTab.getTitle(context),
                  iconPath: _getIconPath(),
                  trailing: const ResetModeButton(),
                ),
                Expanded(child: _createInnerContents(actorType)),
              ]),
            ),
          ),
          PreferenceBuilder(
              preference: Provider.of<MainScreenSettings>(context, listen: false).bIsOutlinerPanelOpen,
              builder: (context, final bool bIsOutlinerPanelOpen) {
                return Row(children: [
                  if (bIsOutlinerPanelOpen) const SizedBox(width: UnrealTheme.cardMargin),
                  if (bIsOutlinerPanelOpen) OutlinerPanel(),
                ]);
              }),
        ],
      ),
    );
  }

  /// Get the title to display at the top of the tab.
  String? _getTitle() {
    final Set<String> selectedActors = _selectedActorSettings.selectedActors.getValue();

    if (selectedActors.length == 1) {
      final UnrealObject? actor = _actorManager.getActorAtPath(selectedActors.first);

      if (actor != null) {
        return actor.name;
      }
    } else if (selectedActors.length > 1) {
      return AppLocalizations.of(context)!.detailsTabMultipleActorsTitle;
    }

    return null;
  }

  /// Get the path for the icon to display at the top of the tab.
  String? _getIconPath() {
    String? iconPath;

    if (_selectedActorSettings.selectedActors.getValue().length == 1) {
      final UnrealObject? actor = _actorManager.getActorAtPath(_selectedActorSettings.selectedActors.getValue().first);

      if (actor != null) {
        iconPath = actor.getIconPath();
      }
    }

    if (iconPath != null) {
      return iconPath;
    }

    return 'packages/epic_common/assets/icons/details.svg';
  }

  /// Called when an actor we're editing has an update from the actor manager.
  void _onActorUpdate(ActorUpdateDetails details) {
    if (mounted && (details.renamedActors.isNotEmpty || details.addedActors.isNotEmpty)) {
      // Force redraw in case we need to update the name/just got the name for an actor we were awaiting
      setState(() {});
    }
  }

  /// Determine the type of actors we're editing.
  _DetailsActorType? _getActorType() {
    _DetailsActorType? actorType;

    final Set<String> selectedActors = _selectedActorSettings.selectedActors.getValue();
    for (final String actorPath in selectedActors) {
      final UnrealObject? actor = _actorManager.getActorAtPath(actorPath);
      if (actor == null) {
        continue;
      }

      _DetailsActorType? newActorType;

      if (actor.isA(lightCardClassName)) {
        newActorType = _DetailsActorType.lightCard;
      }

      if (actor.isA(colorCorrectRegionClassName)) {
        newActorType = _DetailsActorType.colorCorrectRegion;
      }

      if (actor.isAny(colorCorrectWindowClassNames)) {
        newActorType = _DetailsActorType.colorCorrectWindow;
      }

      if (actorType != null && newActorType != null && newActorType != actorType) {
        return _DetailsActorType.multiple;
      }

      actorType = newActorType;
    }

    return actorType;
  }

  /// Create the inner contents of the tab (i.e. inside the panel UI).
  Widget _createInnerContents(_DetailsActorType? actorType) {
    if (actorType == null) {
      final mainScreenSettings = Provider.of<MainScreenSettings>(context, listen: false);
      return PreferenceBuilder(
        preference: mainScreenSettings.bIsOutlinerPanelOpen,
        builder: (context, final bool bIsOutlinerPanelOpen) => EmptyPlaceholder(
          message: AppLocalizations.of(context)!.detailsTabEmptyMessage,
          button: bIsOutlinerPanelOpen
              ? null
              : EpicWideButton(
                  text: AppLocalizations.of(context)!.detailsTabShowOutlinerButtonLabel,
                  iconPath: 'packages/epic_common/assets/icons/outliner.svg',
                  onPressed: () => mainScreenSettings.bIsOutlinerPanelOpen.setValue(true),
                ),
        ),
      );
    }

    if (actorType == _DetailsActorType.multiple) {
      return EmptyPlaceholder(
        message: AppLocalizations.of(context)!.detailsTabMixedActorsMessage,
      );
    }

    // Buttons to toggle between property display types
    Widget propertyDisplayTypeToggleButtons = PreferenceBuilder(
      preference: _tabSettings.detailsPropertyDisplayType,
      builder: (context, DetailsPropertyDisplayType detailsPropertyDisplayType) => SelectorBar(
        value: detailsPropertyDisplayType,
        onSelected: (DetailsPropertyDisplayType value) => _tabSettings.detailsPropertyDisplayType.setValue(value),
        valueNames: {
          DetailsPropertyDisplayType.orientation: AppLocalizations.of(context)!.colorTabOrientationPropertiesLabel,
          DetailsPropertyDisplayType.appearance: AppLocalizations.of(context)!.colorTabAppearancePropertiesLabel,
        },
      ),
    );

    late final BaseColorTabMode mode;
    late final List<UnrealProperty> colorProperties;
    late final Widget otherPropertiesColumn;

    switch (actorType) {
      case _DetailsActorType.lightCard:
        mode = BaseColorTabMode.color;
        colorProperties = _getPropertiesOnValidActors('Color');
        otherPropertiesColumn = _createLightCardPropertiesColumn();
        break;

      // We handle the OR case here because all CCWs are also CCRs, so they may be flagged as both
      case _DetailsActorType.colorCorrectWindow:
        mode = BaseColorTabMode.colorGrading;
        colorProperties = _getPropertiesOnValidActors('ColorGradingSettings');
        otherPropertiesColumn = _createColorCorrectWindowPropertiesColumn();
        break;

      case _DetailsActorType.colorCorrectRegion:
        mode = BaseColorTabMode.colorGrading;
        colorProperties = _getPropertiesOnValidActors('ColorGradingSettings');
        otherPropertiesColumn = _createColorCorrectRegionPropertiesColumn();

        // Can't control CCR orientation properties
        propertyDisplayTypeToggleButtons =
            FakeSelectorBar(AppLocalizations.of(context)!.colorTabAppearancePropertiesLabel);
        break;

      default:
        throw 'Invalid actor type for top widget in details panel';
    }

    return BaseColorTab(
      colorProperties: colorProperties,
      mode: mode,
      rightSideHeader: Center(child: propertyDisplayTypeToggleButtons),
      rightSideContents: otherPropertiesColumn,
    );
  }

  /// Create property widgets for a lightcard.
  Widget _createLightCardPropertiesColumn() {
    return PreferenceBuilder(
      preference: _tabSettings.detailsPropertyDisplayType,
      builder: (context, DetailsPropertyDisplayType detailsPropertyDisplayType) {
        late List<Widget> widgets;

        switch (detailsPropertyDisplayType) {
          case DetailsPropertyDisplayType.orientation:
            widgets = [
              UnrealDropdownSelector(
                overrideName: AppLocalizations.of(context)!.propertyLightCardMask,
                unrealProperties: _getPropertiesOnValidActors('Mask'),
              ),
              for (final Widget slider in _createCommonStageActorOrientationPropertyWidgets()) slider,
            ];
            break;

          case DetailsPropertyDisplayType.appearance:
            widgets = [
              UnrealDeltaSlider(
                unrealProperties: _getPropertiesOnValidActors('Temperature'),
              ),
              UnrealDeltaSlider(
                unrealProperties: _getPropertiesOnValidActors('Tint'),
              ),
              UnrealDeltaSlider(
                unrealProperties: _getPropertiesOnValidActors('Gain'),
                softMin: 0,
                softMax: 10,
              ),
              UnrealDeltaSlider(
                unrealProperties: _getPropertiesOnValidActors('Opacity'),
              ),
              UnrealDeltaSlider(
                unrealProperties: _getPropertiesOnValidActors('Feathering'),
              ),
              UnrealDeltaSlider(
                unrealProperties: _getPropertiesOnValidActors('Exposure'),
                softMin: -10,
                softMax: 10,
              ),
              UnrealStepper(
                unrealProperties: _getPropertiesOnValidActors('Exposure'),
                steps: StepperStepConfig.exposureSteps,
              ),
            ];
            break;

          default:
            widgets = [];
        }

        return Column(children: widgets);
      },
    );
  }

  /// Create property widgets for a CCR.
  Widget _createColorCorrectRegionPropertiesColumn() {
    final temperatureTypeProperties = _getPropertiesOnValidActors('TemperatureType');

    return Column(children: [
      UnrealMultiPropertyBuilder<String>(
        properties: temperatureTypeProperties,
        fallbackValue: AppLocalizations.of(context)!.mismatchedValuesLabel,
        builder: (_, String? sharedValue, __) => UnrealDeltaSlider(
          overrideName: sharedValue,
          unrealProperties: _getPropertiesOnValidActors('Temperature'),
          buildLabel: (name) => UnrealDropdownText(unrealProperties: temperatureTypeProperties),
        ),
      ),
      UnrealDeltaSlider(
        unrealProperties: _getPropertiesOnValidActors('Tint'),
      ),
      UnrealDeltaSlider(
        unrealProperties: _getPropertiesOnValidActors('Intensity'),
      ),
      UnrealDeltaSlider(
        unrealProperties: _getPropertiesOnValidActors('Inner'),
      ),
      UnrealDeltaSlider(
        unrealProperties: _getPropertiesOnValidActors('Outer'),
      ),
      UnrealDeltaSlider(
        unrealProperties: _getPropertiesOnValidActors('Falloff'),
        hardMax: 1,
      ),
    ]);
  }

  /// Create property widgets for a CCW.
  Widget _createColorCorrectWindowPropertiesColumn() {
    return PreferenceBuilder(
      preference: _tabSettings.detailsPropertyDisplayType,
      builder: (context, DetailsPropertyDisplayType detailsPropertyDisplayType) {
        late List<Widget> widgets;

        switch (detailsPropertyDisplayType) {
          case DetailsPropertyDisplayType.orientation:
            widgets = [
              UnrealDropdownSelector(
                overrideName: AppLocalizations.of(context)!.propertyLightCardMask,
                unrealProperties: _getPropertiesOnValidActors('WindowType'),
              ),
              for (final Widget slider in _createCommonStageActorOrientationPropertyWidgets()) slider,
              UnrealDeltaSlider(
                unrealProperties: _getPropertiesOnValidActors(
                  'RadialOffset',
                  modifierFunction: _modifyPositionalPropertyNameBasedOnClass,
                ),
              ),
            ];
            break;

          default:
            return _createColorCorrectRegionPropertiesColumn();
        }

        return Column(children: widgets);
      },
    );
  }

  /// Create property widgets for controlling the selected actors' orientations.
  List<Widget> _createCommonStageActorOrientationPropertyWidgets() {
    return _createScalePropertyWidgets() +
        _createPositionPropertyWidgets() +
        [
          UnrealDeltaSlider(
            key: Key('Spin'),
            unrealProperties: _getPropertiesOnValidActors(
              'Spin',
              modifierFunction: _modifyPositionalPropertyNameBasedOnClass,
            ),
            minMaxBehaviour: PropertyMinMaxBehaviour.loop,
          ),
        ];
  }

  /// Create sliders for controlling the position of the selected actors.
  List<Widget> _createPositionPropertyWidgets() {
    // Classes for actor that can be positioned
    final List<String> positionedActorClasses = [lightCardClassName];
    positionedActorClasses.addAll(colorCorrectWindowClassNames);

    // Determine which actors are UV/non-UV so we know which position sliders to show.
    final List<String> uvActorPaths = [];
    final List<String> nonUVActorPaths = [];

    for (final String actorPath in _selectedActorSettings.selectedActors.getValue()) {
      final UnrealObject? actor = _actorManager.getActorAtPath(actorPath);
      if (!(actor?.isAny(positionedActorClasses) ?? false)) {
        // Actor is not a class with position properties, so leave it out entirely
        continue;
      }

      final LightCardActorData? lightCardActorData = actor!.getPerClassData<LightCardActorData>();
      if (lightCardActorData?.bIsUV == true) {
        uvActorPaths.add(actorPath);
      } else {
        nonUVActorPaths.add(actorPath);
      }
    }

    final List<Widget> positionSliders = [];
    if (nonUVActorPaths.isNotEmpty) {
      positionSliders.addAll([
        UnrealDeltaSlider(
          key: Key('Latitude'),
          unrealProperties: _getPropertiesOnActors(
            'Latitude',
            nonUVActorPaths,
            modifierFunction: _modifyPositionalPropertyNameBasedOnClass,
          ),
          minMaxBehaviour: PropertyMinMaxBehaviour.ignore,
        ),
        UnrealDeltaSlider(
          key: Key('Longitude'),
          unrealProperties: _getPropertiesOnActors(
            'Longitude',
            nonUVActorPaths,
            modifierFunction: _modifyPositionalPropertyNameBasedOnClass,
          ),
          minMaxBehaviour: PropertyMinMaxBehaviour.ignore,
        ),
      ]);
    } else if (uvActorPaths.isNotEmpty) {
      positionSliders.addAll([
        UnrealDeltaSlider(
          key: Key('UV X'),
          unrealProperties: _getPropertiesOnActors('UVCoordinates.X', uvActorPaths),
          overrideName: AppLocalizations.of(context)!.propertyLightCardUVX,
          minMaxBehaviour: PropertyMinMaxBehaviour.clamp,
        ),
        UnrealDeltaSlider(
          key: Key('UV Y'),
          unrealProperties: _getPropertiesOnActors('UVCoordinates.Y', uvActorPaths),
          overrideName: AppLocalizations.of(context)!.propertyLightCardUVY,
          minMaxBehaviour: PropertyMinMaxBehaviour.clamp,
        ),
      ]);
    }

    return positionSliders;
  }

  /// Create sliders for controlling the scale of the selected actors.
  List<Widget> _createScalePropertyWidgets() {
    // Get actors that can be scaled
    final List<String> scalableActorClasses = [lightCardClassName];
    scalableActorClasses.addAll(colorCorrectWindowClassNames);

    final List<String> scalableActorPaths = _getValidActorPathsOfClasses(scalableActorClasses);

    final List<Widget> scaleSliders = [];
    if (scalableActorPaths.isNotEmpty) {
      scaleSliders.addAll([
        UnrealDeltaSlider(
          key: Key('Scale X'),
          unrealProperties: _getPropertiesOnActors(
            'Scale.X',
            scalableActorPaths,
            modifierFunction: _modifyPositionalPropertyNameBasedOnClass,
          ),
          overrideName: AppLocalizations.of(context)!.propertyLightCardScaleX,
          hardMin: 0,
          softMax: 10,
        ),
        UnrealDeltaSlider(
          key: Key('Scale Y'),
          unrealProperties: _getPropertiesOnActors(
            'Scale.Y',
            scalableActorPaths,
            modifierFunction: _modifyPositionalPropertyNameBasedOnClass,
          ),
          overrideName: AppLocalizations.of(context)!.propertyLightCardScaleY,
          hardMin: 0,
          softMax: 10,
        ),
      ]);
    }

    return scaleSliders;
  }

  /// Given an [actorPath] and a [propertyName], return a modified positional property name accounting for the actor's
  /// type.
  String _modifyPositionalPropertyNameBasedOnClass(String actorPath, String propertyName) {
    final UnrealObject? actor = _actorManager.getActorAtPath(actorPath);

    if (actor != null && actor.isAny(colorCorrectWindowClassNames)) {
      // CCWs store their positional properties in a sub-structure.
      return 'PositionalParams.' + propertyName;
    }

    return propertyName;
  }

  /// Get a list of properties with the given [name] for all of the actors with paths in [actorPaths].
  /// If [modifierFunction] is provided, it will be called for each [actorPath] and the [propertyName] of the property,
  /// and its return value will be used in place of [name].
  List<UnrealProperty> _getPropertiesOnActors(
    String name,
    List<String> actorPaths, {
    String Function(String actorPath, String propertyName)? modifierFunction,
    String? typeNameOverride,
  }) {
    return actorPaths
        .map(
          (actorPath) => UnrealProperty(
            objectPath: actorPath,
            propertyName: (modifierFunction != null) ? modifierFunction(actorPath, name) : name,
            typeNameOverride: typeNameOverride,
          ),
        )
        .toList();
  }

  /// Get a list of properties with the given [name] for all selected actors that belong to a valid class.
  /// If [modifierFunction] is provided, it will be called for each [actorPath] and the [propertyName] of the property,
  /// and its return value will be used in place of [name].
  List<UnrealProperty> _getPropertiesOnValidActors(
    String name, {
    String Function(String actorPath, String propertyName)? modifierFunction,
    String? typeNameOverride,
  }) {
    return _getPropertiesOnActors(
      name,
      _getValidActorPaths(),
      modifierFunction: modifierFunction,
      typeNameOverride: typeNameOverride,
    );
  }

  /// Check if an actor is in the set of valid classes.
  bool _isActorOfValidClass(String actorPath) {
    final UnrealObject? actor = _actorManager.getActorAtPath(actorPath);
    if (actor == null) {
      return false;
    }

    final UnmodifiableSetView<String> actorClasses = actor.classNames;
    return validClasses.any((className) => actorClasses.contains(className));
  }

  /// Get a list of selected actor paths that we want to edit (i.e. have valid classes).
  List<String> _getValidActorPaths() {
    return _selectedActorSettings.selectedActors
        .getValue()
        .where((actorPath) => _isActorOfValidClass(actorPath))
        .toList();
  }

  /// Return all valid selected actors actors that are a member of at least one class in [classNames].
  List<String> _getValidActorPathsOfClasses(List<String> classNames) => _getValidActorPaths()
      .where((path) => _actorManager.getActorAtPath(path)?.isAny(classNames) ?? false)
      .toList(growable: false);
}
