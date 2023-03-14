/*
 * Copyright 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#pragma once

// clang-format off
#include <GLES3/gl32.h>
#include <GLES2/gl2ext.h>
// clang-format on
#include "device_info/device_info.h"

namespace androidgamesdk_deviceinfo {

using StringVector = ProtoDataHolder::StringVector;

/**
 * Load and draw a texture, using the specified GL internal format.
 * Used to verify that rendering of textures is working propperly.
 */
class BasicTextureRenderer {
 public:
  BasicTextureRenderer(StringVector &errors);
  virtual ~BasicTextureRenderer();

  size_t getGlErrorsCount() const;

  /**
   * Load the given texture, returning true on success.
   */
  GLuint loadTexture(GLenum internalformat, GLsizei width, GLsizei height,
                     GLsizei imageSize, const void *data);

  /**
   * Initialize the shaders and the program object
   */
  GLuint loadProgramObject();

  /**
   * Draw a rectangle with the loaded texture.
   */
  bool draw(GLint width, GLint height);

 private:
  /*
   * Return the loaded and compiled shader, or 0 if an error occurred.
   */
  GLuint loadShader(GLenum type, const char *shaderSrc);

  bool checkGlErrors(const char *title);

  GLint positionLocation_;
  GLint texCoordLocation_;
  GLint samplerLocation_;
  GLuint textureId_;
  GLuint programObject_;

  // Store errors:
  StringVector &errors_;
  size_t errorsCount_;
};

}  // namespace androidgamesdk_deviceinfo
