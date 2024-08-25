// Copyright Epic Games, Inc. All Rights Reserved.

import 'dart:async';

import 'package:epic_common/localizations.dart';
import 'package:epic_common/widgets.dart';
import 'package:flutter/material.dart';
import 'package:flutter_gen/gen_l10n/app_localizations.dart';
import 'package:provider/provider.dart';

import '../../models/engine_connection.dart';
import '../../models/unreal_actor_creator.dart';
import '../../models/unreal_types.dart';

/// Drop-down menu shown to select a template to create an actor from.
class TemplatePickerMenu extends StatefulWidget {
  const TemplatePickerMenu({Key? key, this.actorMapPosition}) : super(key: key);

  /// If provided, override the actor's spawn position (in coordinates normalized to the preview map's size).
  final Offset? actorMapPosition;

  /// Show the template selection menu.
  static void show(
    BuildContext context, {
    Offset? actorScreenPosition,
  }) {
    showDialog(
      context: context,
      builder: (context) => TemplatePickerMenu(actorMapPosition: actorScreenPosition),
      barrierDismissible: true,
      barrierColor: Colors.black38,
    );
  }

  @override
  State<TemplatePickerMenu> createState() => _TemplatePickerMenuState();
}

class _TemplatePickerMenuState extends State<TemplatePickerMenu> {
  /// How long to wait before showing a loading spinner.
  static const Duration _showSpinnerDelay = Duration(milliseconds: 500);

  /// Whether to show a spinner to indicate that we're waiting to receive data.
  bool _bShowSpinner = true;

  /// A timer that will show the loading spinner when it completes.
  Timer? _spinnerTimer = null;

  /// List of available templates.
  final List<UnrealTemplateData> _templates = [];

  final TextEditingController _searchTextController = TextEditingController();

  @override
  void initState() {
    super.initState();

    _fetchTemplates();

    _searchTextController.addListener(_onSearchTextChanged);
  }

  /// Called when a menu item with the given [templateIndex] is pressed.
  void _onItemPressed(int templateIndex) {
    final UnrealActorCreator actorCreator = Provider.of<UnrealActorCreator>(context, listen: false);

    if (templateIndex >= 0 && templateIndex < _templates.length) {
      final UnrealTemplateData templateData = _templates[templateIndex];
      actorCreator.createLightcard(ActorCreationSettings(
        template: templateData,
        mapPosition: widget.actorMapPosition,
      ));
    }

    _closeMenu();
  }

  /// Close the menu.
  void _closeMenu() {
    Navigator.of(context).pop();
  }

  /// Fetch the list of available templates from the engine.
  void _fetchTemplates() async {
    final EngineConnectionManager connectionManager = Provider.of<EngineConnectionManager>(context, listen: false);

    // Don't immediately show the spinner to avoid flickering, but start a timer to show it
    _spinnerTimer?.cancel();
    _spinnerTimer = Timer(_showSpinnerDelay, _showSpinner);

    final UnrealHttpResponse response = await connectionManager.sendHttpRequest(UnrealHttpRequest(
      url: '/remote/search/assets',
      verb: 'PUT',
      body: {
        'Query': _searchTextController.text,
        'Filter': {
          'ClassNames': ['/Script/DisplayClusterLightCardEditor.DisplayClusterLightCardTemplate'],
          'Limit': 100,
        },
      },
    ));

    // No need for spinner now that we've received data.
    _spinnerTimer?.cancel();

    _handleTemplatesResponse(response);

    setState(() {
      _bShowSpinner = false;
    });
  }

  /// Handle a response from the engine containing the list of templates.
  void _handleTemplatesResponse(UnrealHttpResponse response) {
    _templates.clear();

    if (response.code != HttpResponseCode.ok) {
      return;
    }

    final dynamic assets = response.body['Assets'];
    if (assets == null || assets is! List<dynamic>) {
      return;
    }

    for (final dynamic entry in assets) {
      final String? name = entry['Name'];
      if (name == null) {
        continue;
      }

      final String? path = entry['Path'];
      if (path == null) {
        continue;
      }

      _templates.add(UnrealTemplateData(name: name, path: path));
    }
  }

  /// Called when the user's search query text has changed.
  void _onSearchTextChanged() {
    _fetchTemplates();
  }

  @override
  Widget build(BuildContext context) {
    final Widget itemsWidget;

    if (_bShowSpinner) {
      itemsWidget = Center(
        child: SizedBox.square(
          dimension: 60,
          child: CircularProgressIndicator(
            strokeWidth: 6,
          ),
        ),
      );
    } else {
      if (_templates.length == 0) {
        itemsWidget = Padding(
          padding: EdgeInsets.only(top: 16),
          child: Text(
            AppLocalizations.of(context)!.templateMenuEmptyMessage,
            textAlign: TextAlign.center,
          ),
        );
      } else {
        itemsWidget = EpicListView(
          padding: EdgeInsets.only(top: 4),
          itemCount: _templates.length,
          itemBuilder: (context, index) => _TemplateMenuItem(
            templateData: _templates[index],
            onPressed: () => _onItemPressed(index),
          ),
        );
      }
    }

    return MediaQuery.removeViewInsets(
      context: context,
      removeLeft: true,
      removeTop: true,
      removeRight: true,
      removeBottom: true,
      child: Center(
        child: Card(
          child: ListMenu(
            maxWidth: 270,
            minWidth: 270,
            children: [
              CardLargeHeader(
                iconPath: 'packages/epic_common/assets/icons/viewport.svg',
                title: AppLocalizations.of(context)!.placeActorMenuAllTemplates,
                trailing: EpicIconButton(
                  iconPath: 'packages/epic_common/assets/icons/close.svg',
                  tooltipMessage: EpicCommonLocalizations.of(context)!.menuButtonCancel,
                  iconSize: 20,
                  onPressed: _closeMenu,
                ),
              ),
              CardSubHeader(
                padding: EdgeInsets.symmetric(vertical: 8, horizontal: 16),
                child: EpicSearchBar(controller: _searchTextController),
              ),
              ConstrainedBox(
                constraints: BoxConstraints(
                  minHeight: 400,
                  maxHeight: 400,
                ),
                child: itemsWidget,
              ),
            ],
          ),
        ),
      ),
    );
  }

  /// Enable the spinner if safe.
  void _showSpinner() {
    if (!mounted) {
      return;
    }

    setState(() {
      _bShowSpinner = true;
    });
  }
}

/// An item in the menu representing a placeable template.
class _TemplateMenuItem extends StatelessWidget {
  const _TemplateMenuItem({
    Key? key,
    required this.templateData,
    required this.onPressed,
  }) : super(key: key);

  final UnrealTemplateData templateData;
  final VoidCallback onPressed;

  @override
  Widget build(BuildContext context) {
    return ListMenuSimpleItem(
      title: templateData.name,
      iconPath: 'packages/epic_common/assets/icons/template.svg',
      onTap: onPressed,
    );
  }
}
