// Copyright Epic Games, Inc. All Rights Reserved.

import 'dart:ui';

import 'package:flutter/foundation.dart';

/// Wrapper for a shader that can be loaded ahead of time to prevent flickering at runtime.
abstract class PreloadableShader {
  PreloadableShader() {
    _createShaderInstance();
  }

  /// The current shader instance
  FragmentShader? _shader;

  /// The current shader instance
  FragmentShader? get shader => _shader;

  /// The program, which should be loaded before an instance of this class is created.
  PreloadableShaderProgram get program;

  /// Reload the shader program and re-create the shader instance from it.
  void reload() {
    program.load().then((_) => _createShaderInstance());
  }

  /// Dispose of the underlying shader.
  @mustCallSuper
  void dispose() {
    _disposeShaderInstance();
  }

  /// Initialize the shader's uniform values when it's been freshly created.
  @protected
  void initUniforms() {}

  /// Create a shader instance from the loaded program and initialize it.
  void _createShaderInstance() {
    _disposeShaderInstance();

    if (program.program == null) {
      throw Exception('Tried to create shader ${this.runtimeType} but the program was not loaded');
    }

    _shader = program.program!.fragmentShader();
    initUniforms();
  }

  /// Dispose of the current shader instance, if any.
  void _disposeShaderInstance() {
    if (_shader != null) {
      _shader!.dispose();
      _shader = null;
    }
  }
}

/// Wrapper for a shader program to consistently load/unload it.
class PreloadableShaderProgram {
  PreloadableShaderProgram(this.assetPath);

  /// The path to the asset containing the shader code.
  final String assetPath;

  /// THe loaded program, or null if it hasn't been loaded.
  FragmentProgram? _program = null;

  /// The loaded program, or null if it hasn't been loaded.
  FragmentProgram? get program => _program;

  /// Load the program.
  Future<FragmentProgram> load() {
    return FragmentProgram.fromAsset(assetPath).then((loadedProgram) {
      _program = loadedProgram;
      return loadedProgram;
    });
  }

  /// Load the program.
  void unload() {
    if (program != null) {
      _program = null;
    }
  }
}
