// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:flutter/material.dart';
import 'package:flutter_gen/gen_l10n/app_localizations.dart';

import '../../models/navigator_keys.dart';
import 'epic_icon_button.dart';

/// Box shadow to show behind modals.
const List<BoxShadow> modalBoxShadow = [
  BoxShadow(
    color: Color(0x66000000),
    offset: Offset(4, 8),
    blurRadius: 8,
  )
];

/// A route that displays a generic modal dialog centered in the screen.
class GenericModalDialogRoute<T> extends PopupRoute<T> {
  GenericModalDialogRoute({
    required this.builder,
    this.bResizeToAvoidBottomInset = false,
    this.bIsBarrierDismissible = true,
  });

  /// A function that will build the widget to display within this route.
  final Widget Function(BuildContext context) builder;

  /// If true, resize the route to stay within the bottom inset of the screen (e.g. where the OS keyboard opens on
  /// mobile devices).
  final bool bResizeToAvoidBottomInset;

  /// If true, the user can dismiss this modal by tapping outside of its body.
  final bool bIsBarrierDismissible;

  @override
  Color? get barrierColor => Colors.black38;

  @override
  bool get barrierDismissible => bIsBarrierDismissible;

  @override
  String? get barrierLabel => AppLocalizations.of(rootNavigatorKey.currentContext!)!.modalDismissLabel;

  @override
  Duration get transitionDuration => Duration(milliseconds: 200);

  @override
  Duration get reverseTransitionDuration => Duration(milliseconds: 100);

  @override
  Widget buildPage(BuildContext context, Animation<double> animation, Animation<double> secondaryAnimation) {
    return CustomSingleChildLayout(
      delegate: _GenericModalDialogRouteLayout<T>(
        context: context,
        bResizeToAvoidBottomInset: bResizeToAvoidBottomInset,
      ),
      child: Center(
        child: Builder(builder: builder),
      ),
    );
  }

  /// Show a dialog using a [GenericModalDialogRoute] as the route.
  /// If [bIsBarrierDismissible] is true, the user can dismiss this modal by tapping outside of its body.
  /// Returns a future that will return the result when the dialog was popped from the navigator.
  static Future<T?> showDialog<T>({
    required BuildContext context,
    required WidgetBuilder builder,
    bool bIsBarrierDismissible = true,
  }) {
    final route = GenericModalDialogRoute<T>(
      bResizeToAvoidBottomInset: true,
      builder: builder,
      bIsBarrierDismissible: bIsBarrierDismissible,
    );

    return Navigator.of(context, rootNavigator: true).push<T>(route);
  }
}

class _GenericModalDialogRouteLayout<T> extends SingleChildLayoutDelegate {
  _GenericModalDialogRouteLayout({
    required this.context,
    required this.bResizeToAvoidBottomInset,
  });

  final BuildContext context;

  /// If true, resize the route to stay within the bottom inset of the screen (e.g. where the OS keyboard opens on
  /// mobile devices).
  final bool bResizeToAvoidBottomInset;

  @override
  BoxConstraints getConstraintsForChild(BoxConstraints constraints) {
    double maxHeight = double.infinity;

    final MediaQueryData mediaQuery = MediaQuery.of(context);
    if (bResizeToAvoidBottomInset) {
      maxHeight = mediaQuery.size.height - mediaQuery.viewInsets.bottom;
    } else {
      maxHeight = mediaQuery.size.height;
    }

    return constraints.copyWith(
      minHeight: 0,
      maxHeight: maxHeight,
    );
  }

  @override
  bool shouldRelayout(covariant SingleChildLayoutDelegate oldDelegate) {
    return oldDelegate != this;
  }
}

/// A generic card wrapper for a modal dialog.
class ModalDialogCard extends StatelessWidget {
  const ModalDialogCard({
    Key? key,
    required this.child,
    this.color,
    this.shape,
  }) : super(key: key);

  /// Contents of the card.
  final Widget child;

  /// Color of the card.
  final Color? color;

  /// Shape of the card.
  final ShapeBorder? shape;

  @override
  Widget build(BuildContext context) {
    return Container(
      decoration: BoxDecoration(boxShadow: modalBoxShadow),
      constraints: BoxConstraints(minWidth: 300),
      child: Card(
        shape: shape,
        color: color ?? Theme.of(context).colorScheme.surfaceTint,
        child: child,
      ),
    );
  }
}

/// Standard title formatting for a modal dialog.
class ModalDialogTitle extends StatelessWidget {
  const ModalDialogTitle({Key? key, required this.title}) : super(key: key);

  final String title;

  @override
  Widget build(BuildContext context) {
    return ModalDialogSection(
      child: Padding(
        padding: const EdgeInsets.symmetric(vertical: 4),
        child: Text(
          style: Theme.of(context).textTheme.displayLarge,
          textAlign: TextAlign.center,
          this.title,
        ),
      ),
    );
  }
}

/// Wrapper to apply standard padding for a section of a modal dialog.
class ModalDialogSection extends StatelessWidget {
  const ModalDialogSection({Key? key, required this.child}) : super(key: key);

  final Widget child;

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.symmetric(
        horizontal: 16,
        vertical: 8,
      ),
      child: child,
    );
  }
}

/// A generic modal containing information and an "OK" button.
class InfoModalDialog extends StatelessWidget {
  const InfoModalDialog({Key? key, required this.message}) : super(key: key);

  /// The localized message to show.
  final String message;

  /// Show an info dialog containing a [message].
  static void show(String message) {
    final BuildContext context = rootNavigatorKey.currentContext!;

    final route = GenericModalDialogRoute(
      bResizeToAvoidBottomInset: true,
      builder: (_) => InfoModalDialog(message: message),
    );

    Navigator.of(context, rootNavigator: true).push(route);
  }

  /// Show an info dialog containing a message created using a [getMessage] function which will be passed the context in
  /// which the modal is to be created.
  static void showInContext(String Function(BuildContext) getMessage) {
    final BuildContext context = rootNavigatorKey.currentContext!;
    InfoModalDialog.show(getMessage(context));
  }

  @override
  Widget build(BuildContext context) {
    final localizations = AppLocalizations.of(context)!;

    return ModalDialogCard(
      child: IntrinsicWidth(
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            SizedBox(height: 8),
            ModalDialogSection(
              child: Text(message),
            ),
            ModalDialogSection(
              child: Row(
                mainAxisAlignment: MainAxisAlignment.end,
                children: [
                  EpicLozengeButton(
                    label: localizations.menuButtonOK,
                    width: 110,
                    onPressed: () => Navigator.of(context).pop(),
                  ),
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }
}
