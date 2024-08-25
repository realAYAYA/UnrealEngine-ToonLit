// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:epic_common/theme.dart';
import 'package:epic_common/widgets.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_gen/gen_l10n/app_localizations.dart';

/// Widget housing for locking or unlocking the screen, either ignoring touch input on allowing them.
class ControlLock extends StatefulWidget {
  const ControlLock({Key? key}) : super(key: key);

  @override
  State<ControlLock> createState() => _ControlLockState();
}

class _ControlLockState extends State<ControlLock> {
  /// Global key for the lock button, allows easy reference to the button.
  final GlobalKey _buttonKey = GlobalKey();

  /// whether the screen is locked or not.
  bool _bIsLocked = false;

  /// The context of the overlay shown when locked.
  BuildContext? _lockOverlayContext;

  @override
  Widget build(BuildContext context) {
    return _ControlLockButton(
      key: _buttonKey,
      bIsLocked: false,
      onLongPressed: _onLocked,
      bIsVisible: !_bIsLocked,
    );
  }

  /// callback to initiate screen lock and negate all touch input.
  void _onLocked() {
    if (_bIsLocked) {
      return;
    }

    final RenderBox? buttonBox = _buttonKey.currentContext?.findRenderObject() as RenderBox;

    if (buttonBox == null) {
      return;
    }

    showDialog(
      context: context,
      builder: (context) {
        final Offset buttonPosition = buttonBox.localToGlobal(Offset.zero);
        _lockOverlayContext = context;

        // Create the overlay
        return Material(
          color: Colors.transparent,
          surfaceTintColor: Colors.transparent,
          child: Stack(
            children: [
              Positioned(
                left: buttonPosition.dx,
                child: _ControlLockButton(
                  bIsLocked: true,
                  onLongPressed: _onUnlocked,
                ),
              ),
            ],
          ),
        );
      },
      barrierDismissible: false,
      barrierColor: const Color(0xC0000000),
    );

    HapticFeedback.mediumImpact();

    setState(() {
      _bIsLocked = true;
    });
  }

  /// callback to initiate screen unlock and restore touch input on the app.
  void _onUnlocked() {
    if (!_bIsLocked) {
      return;
    }

    if (_lockOverlayContext != null) {
      Navigator.of(_lockOverlayContext!).pop();
      _lockOverlayContext = null;
    }

    HapticFeedback.mediumImpact();

    setState(() {
      _bIsLocked = false;
    });
  }
}

/// lock button
class _ControlLockButton extends StatelessWidget {
  const _ControlLockButton({
    Key? key,
    required this.onLongPressed,
    required this.bIsLocked,
    this.bIsVisible = true,
  }) : super(key: key);

  /// Function to be executed when the button is long pressed.
  final Function() onLongPressed;

  /// whether the button is locked or not.
  final bool bIsLocked;

  /// Whether the lock icon on the button is visible when the button is locked vs opened.
  final bool bIsVisible;

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      width: 48,
      height: 48,
      child: !bIsVisible
          ? null
          : EpicIconButton(
              bIsToggledOn: bIsLocked,
              activeBackgroundColor: UnrealColors.highlightRed,
              iconPath: bIsLocked
                  ? 'packages/epic_common/assets/icons/lock_locked.svg'
                  : 'packages/epic_common/assets/icons/lock_unlocked.svg',
              onLongPressed: onLongPressed,
              tooltipMessage:
                  bIsLocked ? AppLocalizations.of(context)!.unlockControls : AppLocalizations.of(context)!.lockControls,
            ),
    );
  }
}
