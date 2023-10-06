// Copyright Epic Games, Inc. All Rights Reserved.

import 'package:flutter/material.dart';
import 'package:flutter_svg/svg.dart';

/// An icon from an image asset (either a PNG or an SVG).
class AssetIcon extends StatelessWidget {
  const AssetIcon({
    Key? key,
    required this.path,
    this.size,
    this.width,
    this.height,
    this.color,
    this.fit,
  })  : assert(
          size == null || (width == null && height == null),
          'size parameter overrides width and height parameters',
        ),
        super(key: key);

  /// The path of the asset. Must be an image or an SVG.
  final String path;

  /// If provided, use this as both the [width] and the [height].
  final double? size;

  /// The width of the icon.
  final double? width;

  /// The height of the icon.
  final double? height;

  /// The color of the icon.
  final Color? color;

  /// How to inscribe the image into the space allocated during layout.
  final BoxFit? fit;

  @override
  Widget build(BuildContext context) {
    if (path.endsWith('.svg')) {
      return SvgPicture.asset(
        path,
        width: size ?? width,
        height: size ?? height,
        color: color,
        colorBlendMode: BlendMode.modulate,
        fit: fit ?? BoxFit.contain,
      );
    } else {
      return Image.asset(
        path,
        width: size ?? width,
        height: size ?? height,
        color: color,
        colorBlendMode: BlendMode.modulate,
        fit: fit,
      );
    }
  }
}
