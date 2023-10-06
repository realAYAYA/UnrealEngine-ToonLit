// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:flutter/material.dart';

/// Spinner overlay used to indicate that we're waiting to receive or load something.
class SpinnerOverlay extends StatelessWidget {
  const SpinnerOverlay({Key? key}) : super(key: key);

  @override
  Widget build(BuildContext context) {
    return Container(
      color: Colors.black.withOpacity(0.5),
      child: const Center(
        child: SizedBox.square(
          dimension: 80,
          child: CircularProgressIndicator(
            strokeWidth: 6,
          ),
        ),
      ),
    );
  }
}
