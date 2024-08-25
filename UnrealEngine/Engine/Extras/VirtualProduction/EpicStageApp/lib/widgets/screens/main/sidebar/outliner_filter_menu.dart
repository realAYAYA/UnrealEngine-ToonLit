// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:epic_common/widgets.dart';
import 'package:flutter/material.dart';
import 'package:flutter_gen/gen_l10n/app_localizations.dart';
import 'package:provider/provider.dart';
import 'package:streaming_shared_preferences/streaming_shared_preferences.dart';

import '../../../../models/settings/outliner_panel_settings.dart';
import '../../../../models/unreal_object_filters.dart';
import '../../../elements/dropdown_button.dart';

/// Modal dropdown button that displays outliner filter settings.
class OutlinerFilterButton extends StatelessWidget {
  const OutlinerFilterButton({Key? key}) : super(key: key);

  @override
  Widget build(BuildContext context) {
    final settings = Provider.of<OutlinerPanelSettings>(context, listen: false);

    return ModalDropdownButton(
      buttonBuilder: (context, state) => Material(
        color: Colors.transparent,
        child: PreferenceBuilder(
          preference: settings.selectedFilters,
          builder: (context, Set<UnrealObjectFilter> filters) => CardSubHeaderButton(
            iconPath: filters.isEmpty
                ? 'packages/epic_common/assets/icons/filter.svg'
                : 'packages/epic_common/assets/icons/filter_on.svg',
            tooltipMessage: AppLocalizations.of(context)!.outlinerFiltersButtonTooltip,
            bIsToggledOn: state != ModalDropdownButtonState.closed,
            bIsVisualOnly: true,
          ),
        ),
      ),
      menuBuilder: (menuContext, originTabBuilder) => _OutlinerFilterDropDownMenu(
        originTabBuilder: originTabBuilder,
        settings: settings,
      ),
    );
  }
}

/// Dropdown menu shown to select which filters to apply to the Outliner panel.
class _OutlinerFilterDropDownMenu extends StatelessWidget {
  const _OutlinerFilterDropDownMenu({
    Key? key,
    required this.settings,
    this.originTabBuilder,
  }) : super(key: key);

  /// The [OutlinerPanelSettings] from the originating context.
  final OutlinerPanelSettings settings;

  /// Function that builds the origin tab for this menu.
  final Widget Function(BuildContext, bool)? originTabBuilder;

  @override
  Widget build(BuildContext context) {
    return PreferenceBuilder(
      preference: settings.selectedFilters,
      builder: (context, final Set<UnrealObjectFilter> selectedFilters) => DropDownListMenu(
        minHeight: 100,
        originTabBuilder: originTabBuilder,
        children: [
          for (UnrealObjectFilter filter in UnrealObjectFilterRegistry.filters)
            ListMenuSimpleItem(
              key: Key(filter.internalName),
              title: filter.getDisplayName(context),
              iconPath: filter.iconPath,
              bShowCheckbox: true,
              bIsChecked: selectedFilters.contains(filter),
              onTap: () => _toggleFilter(filter),
            ),
        ],
      ),
    );
  }

  /// Called when the user toggles a filter.
  void _toggleFilter(UnrealObjectFilter filter) {
    final Set<UnrealObjectFilter> filters = settings.selectedFilters.getValue();
    if (filters.contains(filter)) {
      filters.remove(filter);
    } else {
      filters.add(filter);
    }

    settings.selectedFilters.setValue(filters);
  }
}
