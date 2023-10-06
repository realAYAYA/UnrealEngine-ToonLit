// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:flutter/cupertino.dart';

/// Radius of the checkbox' circle.
const double _checkboxRadius = 11.0;

/// Thickness of the ring around an inactive checkbox.
const double _checkboxRingWidth = 1.5;

/// Size of the check icon relative to the radius of the checkbox.
const double _checkboxIconSize = 1.2;

/// Offset added to the position of the check icon.
const Offset _checkboxIconOffset = Offset(0.0, -1);

/// A circular checkbox in the Cupertino style.
class CupertinoCheckbox extends StatelessWidget {
  const CupertinoCheckbox({required this.bIsChecked, this.onChanged, Key? key}) : super(key: key);

  /// Whether the box is checked.
  final bool bIsChecked;

  /// Called when the value of the checkbox should change.
  final ValueChanged<bool>? onChanged;

  @override
  Widget build(BuildContext context) {
    return CustomPaint(
      painter: _CupertinoCheckboxPainter(
        bIsChecked: bIsChecked,
        inactiveColor: CupertinoDynamicColor.resolve(CupertinoColors.inactiveGray, context),
        activeColor: CupertinoDynamicColor.resolve(CupertinoColors.activeBlue, context),
        checkColor: CupertinoDynamicColor.resolve(CupertinoColors.white, context),
      ),
    );
  }
}

class _CupertinoCheckboxPainter extends CustomPainter {
  const _CupertinoCheckboxPainter({
    required this.bIsChecked,
    required this.inactiveColor,
    required this.activeColor,
    required this.checkColor,
  });

  final bool bIsChecked;
  final Color inactiveColor;
  final Color activeColor;
  final Color checkColor;

  @override
  void paint(Canvas canvas, Size size) {
    final Offset center = size.center(Offset.zero);

    if (bIsChecked) {
      // Filled circle and checkmark when checked
      final Paint circlePaint = Paint()..color = activeColor;

      canvas.drawCircle(center, _checkboxRadius, circlePaint);

      const IconData icon = CupertinoIcons.check_mark;
      TextPainter textPainter = TextPainter(textDirection: TextDirection.ltr)
        ..text = TextSpan(
          text: String.fromCharCode(icon.codePoint),
          style: TextStyle(
            color: checkColor,
            fontSize: _checkboxRadius * _checkboxIconSize,
            fontFamily: icon.fontFamily,
            package: icon.fontPackage,
          ),
        )
        ..layout();

      textPainter.paint(canvas, center - textPainter.size.center(_checkboxIconOffset));
    } else {
      // Unfilled circle when unchecked
      final Paint ringPaint = Paint()
        ..color = inactiveColor
        ..style = PaintingStyle.stroke
        ..strokeWidth = _checkboxRingWidth;

      canvas.drawCircle(center, _checkboxRadius - (_checkboxRingWidth / 2), ringPaint);
    }
  }

  @override
  bool shouldRepaint(covariant CustomPainter oldDelegate) {
    return this != oldDelegate;
  }
}
