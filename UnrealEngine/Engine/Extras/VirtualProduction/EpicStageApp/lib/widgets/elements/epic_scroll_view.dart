// Copyright Epic Games, Inc. All Rights Reserved.

import 'dart:ui';

import 'package:flutter/material.dart';

import '../../utilities/preloadable_shader.dart';

const double _scrollbarPadding = 24;

/// A custom-styled scrolling view which includes a fixed scrollbar, padding to fit it, and a gradient fading items
/// near the edges of the scroll area.
class EpicScrollView extends StatefulWidget {
  const EpicScrollView({
    Key? key,
    required this.child,
  }) : super(key: key);

  final Widget child;

  static Future preloadShaders() => _EpicScrollViewShader.load();
  static void unloadShaders() => _EpicScrollViewShader.unload();

  @override
  State<EpicScrollView> createState() => _EpicScrollViewState();
}

class _EpicScrollViewState extends State<EpicScrollView> with SingleTickerProviderStateMixin {
  final ScrollController _controller = ScrollController();
  late final _EpicScrollViewShader shader;

  @override
  void initState() {
    super.initState();
    shader = _EpicScrollViewShader(
      scrollController: _controller,
      vsync: this,
    );
  }

  @override
  void dispose() {
    shader.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return Theme(
      data: theme.copyWith(
        scrollbarTheme: theme.scrollbarTheme.copyWith(
          thumbVisibility: MaterialStatePropertyAll(true),
        ),
      ),
      child: ShaderMask(
        shaderCallback: shader.shaderMaskCallback,
        child: SingleChildScrollView(
          controller: _controller,
          padding: EdgeInsets.only(
            right: _scrollbarPadding,
          ),
          child: widget.child,
        ),
        blendMode: BlendMode.modulate,
      ),
    );
  }

  @override
  void reassemble() {
    super.reassemble();

    shader.reload();
  }
}

/// Indices for each uniform value in the shader used by [_EpicScrollViewShader].
enum _ScrollViewShaderVars {
  uScrollbarPadding,
  uStrength,
  uScrollOffset,
  uMaxScrollOffset,
  uSizeX,
  uSizeY,
}

/// Shader that fades the top and bottom of the scroll view when there's more to scroll in that direction.
class _EpicScrollViewShader extends PreloadableShader {
  _EpicScrollViewShader({
    required this.scrollController,
    required TickerProvider vsync,
  }) : _fadeInController = AnimationController(
          value: 0,
          upperBound: 1,
          duration: const Duration(milliseconds: 100),
          vsync: vsync,
        ) {
    scrollController.addListener(_updateOffset);
    _fadeInController.addListener(_updateStrength);

    // Scroll controller won't be ready immediately, so try updating and fading in the effect after build
    WidgetsBinding.instance.addPostFrameCallback((_) {
      _updateOffset();
      _fadeInEffectIfReady();
    });
  }

  static final PreloadableShaderProgram _program = PreloadableShaderProgram('assets/shaders/scroll_view_fade.frag');

  /// The scroll controller for the corresponding scroll view.
  final ScrollController scrollController;

  /// The animation controller used to fade in the effect when ready.
  final AnimationController _fadeInController;

  /// Whether this has been disposed.
  bool _bIsDisposed = false;

  @override
  PreloadableShaderProgram get program => _program;

  /// Load or re-load the shader program.
  static Future<FragmentProgram> load() => _program.load();

  /// Unload the shader program.
  static void unload() => _program.unload();

  /// Callback function passed to a [ShaderMask] which updates the bounding rectangle.
  Shader shaderMaskCallback(Rect rect) {
    shader!.setFloat(_ScrollViewShaderVars.uSizeX.index, rect.width);
    shader!.setFloat(_ScrollViewShaderVars.uSizeY.index, rect.height);

    return shader!;
  }

  @override
  void dispose() {
    super.dispose();

    if (_bIsDisposed) {
      return;
    }

    scrollController.removeListener(_updateOffset);
    _fadeInController.dispose();
    _bIsDisposed = true;
  }

  @override
  void initUniforms() {
    shader!.setFloat(_ScrollViewShaderVars.uScrollbarPadding.index, _scrollbarPadding);
    _updateStrength();
    _updateOffset();
  }

  /// Fade in the effect if it hasn't already and the scroll controller is ready to be accessed.
  void _fadeInEffectIfReady() {
    if (_bIsDisposed ||
        _fadeInController.isCompleted ||
        _fadeInController.isAnimating ||
        scrollController.positions.isEmpty) {
      return;
    }

    _fadeInController.animateTo(1.0);
  }

  /// Update the effect's strength based on the fade in animation.
  void _updateStrength() {
    shader!.setFloat(_ScrollViewShaderVars.uStrength.index, _fadeInController.value);
  }

  /// Update the shader's properties based on the [scrollController]'s position.
  void _updateOffset() {
    if (scrollController.positions.isEmpty) {
      return;
    }

    _fadeInEffectIfReady();

    shader!.setFloat(_ScrollViewShaderVars.uScrollOffset.index, scrollController.offset);
    shader!.setFloat(_ScrollViewShaderVars.uMaxScrollOffset.index, scrollController.position.maxScrollExtent);
  }
}
