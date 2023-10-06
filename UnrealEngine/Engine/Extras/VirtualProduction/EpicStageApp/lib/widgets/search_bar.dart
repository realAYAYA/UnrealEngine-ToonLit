// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:flutter/material.dart';
import 'package:flutter_gen/gen_l10n/app_localizations.dart';

import '../utilities/unreal_colors.dart';
import 'elements/asset_icon.dart';

/// A text box used to search for things by name.
class SearchBar extends StatelessWidget {
  const SearchBar({
    Key? key,
    this.onChanged,
    this.controller,
  }) : super(key: key);

  /// Callback for when the search bar's text changes.
  final ValueChanged<String>? onChanged;

  /// Controller for text editing in the search bar.
  final TextEditingController? controller;

  static const double iconSize = 24;
  static const double iconRightPadding = 8;
  static const double iconLeftPadding = 8;

  @override
  Widget build(BuildContext context) {
    final border = OutlineInputBorder(borderRadius: BorderRadius.circular(21));

    return SizedBox(
      height: 36,
      child: TextField(
        onChanged: onChanged,
        onTapOutside: (_) {
          final FocusScopeNode currentFocus = FocusScope.of(context);

          // Flutter will throw an exception if we don't check this first
          if (currentFocus.hasPrimaryFocus) {
            return;
          }

          currentFocus.focusedChild?.unfocus();
        },
        controller: controller,
        style: Theme.of(context).textTheme.bodyMedium,
        textAlignVertical: TextAlignVertical.center,
        cursorColor: UnrealColors.white,
        cursorWidth: 1,
        decoration: InputDecoration(
          hintText: AppLocalizations.of(context)!.searchBarLabel,
          filled: true,
          fillColor: Theme.of(context).colorScheme.background,
          hoverColor: Theme.of(context).colorScheme.background,
          contentPadding: const EdgeInsets.symmetric(horizontal: 4),
          border: border,
          enabledBorder: border.copyWith(borderSide: const BorderSide(style: BorderStyle.none)),
          focusedBorder: border.copyWith(borderSide: BorderSide(color: Theme.of(context).colorScheme.primary)),
          prefixIcon: Padding(
            padding: const EdgeInsets.only(left: iconLeftPadding, right: iconRightPadding),
            child: AssetIcon(path: 'assets/images/icons/search_small.svg', size: iconSize),
          ),
          prefixIconConstraints: const BoxConstraints(maxWidth: iconSize + iconLeftPadding + iconRightPadding),
          hintStyle: Theme.of(context).textTheme.bodyMedium!.copyWith(color: UnrealColors.gray56),
        ),
      ),
    );
  }
}
