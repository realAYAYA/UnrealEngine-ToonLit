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

// clang-format off
#include <GLES3/gl32.h>
#include <GLES2/gl2ext.h>
// clang-format on
#include "basic_texture_renderer.h"
#include "device_info/device_info.h"

namespace androidgamesdk_deviceinfo {

using StringVector = ProtoDataHolder::StringVector;

BasicTextureRenderer::BasicTextureRenderer(StringVector &errors)
    : positionLocation_(0),
      texCoordLocation_(0),
      samplerLocation_(0),
      textureId_(0),
      programObject_(0),
      errors_(errors),
      errorsCount_(0){};

BasicTextureRenderer::~BasicTextureRenderer() {
  glDeleteTextures(1, &textureId_);
  glDeleteProgram(programObject_);
}

size_t BasicTextureRenderer::getGlErrorsCount() const { return errorsCount_; }

GLuint BasicTextureRenderer::loadTexture(GLenum internalformat, GLsizei width,
                                         GLsizei height, GLsizei imageSize,
                                         const void *data) {
  glGenTextures(1, &textureId_);
  glBindTexture(GL_TEXTURE_2D, textureId_);

  // Load the texture
  glCompressedTexImage2D(GL_TEXTURE_2D, 0, internalformat, width, height, 0,
                         imageSize, data);
  if (checkGlErrors("BasicTextureRenderer (glCompressedTexImage2D)"))
    return false;

  // Use nearest as filtering mode to get pixel colors without interpolation
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  if (checkGlErrors("BasicTextureRenderer (glTexParameteri)")) return false;

  return true;
}

GLuint BasicTextureRenderer::loadProgramObject() {
  char vShaderStr[] =
      "attribute vec4 a_position;   \n"
      "attribute vec2 a_texCoord;   \n"
      "varying vec2 v_texCoord;     \n"
      "void main() {                \n"
      "   gl_Position = a_position; \n"
      "   v_texCoord = a_texCoord;  \n"
      "}                            \n";

  char fShaderStr[] =
      "precision mediump float;                            \n"
      "varying vec2 v_texCoord;                            \n"
      "uniform sampler2D s_texture;                        \n"
      "void main() {                                       \n"
      "  gl_FragColor = texture2D(s_texture, v_texCoord);  \n"
      "}                                                   \n";

  GLuint vertexShader = loadShader(GL_VERTEX_SHADER, vShaderStr);
  GLuint fragmentShader = loadShader(GL_FRAGMENT_SHADER, fShaderStr);

  programObject_ = glCreateProgram();
  if (!programObject_) {
    errors_.addCopy("BasicTextureRenderer: glCreateProgram failed");
    return false;
  }

  glAttachShader(programObject_, vertexShader);
  if (checkGlErrors("BasicTextureRenderer (glAttachShader, vertexShader)")) {
    return false;
  }

  glAttachShader(programObject_, fragmentShader);
  if (checkGlErrors("BasicTextureRenderer (glAttachShader, fragmentShader)")) {
    return false;
  }

  glLinkProgram(programObject_);
  if (checkGlErrors("BasicTextureRenderer (glLinkProgram)")) {
    return false;
  }

  GLint linked;
  glGetProgramiv(programObject_, GL_LINK_STATUS, &linked);
  if (linked == GL_FALSE) {
    errors_.addCopy("BasicTextureRenderer: programObject is not linked");
    return false;
  }

  positionLocation_ = glGetAttribLocation(programObject_, "a_position");
  texCoordLocation_ = glGetAttribLocation(programObject_, "a_texCoord");
  samplerLocation_ = glGetUniformLocation(programObject_, "s_texture");
  return true;
}

bool BasicTextureRenderer::draw(GLint width, GLint height) {
  // Prepare viewport
  glViewport(0, 0, width, height);
  glClearColor(1.0f, 1.0f, 1.0f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  glUseProgram(programObject_);

  // Create the rectangle
  GLfloat positionVertices[] = {-1.0f, -1.0f, 0.0f, -1.0f, 1.0f, 0.0f,
                                1.0f,  -1.0f, 0.0f, 1.0f,  1.0f, 0.0f};
  glVertexAttribPointer(positionLocation_, 3, GL_FLOAT, GL_FALSE, 0,
                        positionVertices);
  glEnableVertexAttribArray(positionLocation_);

  // Coordinates in the texture for the rectangle
  GLfloat textCoordVertices[] = {0.0f, 0.0f, 0.0f, 1.0f,
                                 1.0f, 0.0f, 1.0f, 1.0f};
  glVertexAttribPointer(texCoordLocation_, 2, GL_FLOAT, GL_FALSE, 0,
                        textCoordVertices);
  glEnableVertexAttribArray(texCoordLocation_);

  // Bind the texture
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, textureId_);
  glUniform1i(samplerLocation_,
              0);  // Set the sampler texture unit to texture 0

  // Draw the rectangle
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

  return true;
}

GLuint BasicTextureRenderer::loadShader(GLenum type, const char *shaderSrc) {
  GLuint shader = glCreateShader(type);
  if (!shader) {
    errors_.addCopy("BasicTextureRenderer: glCreateShader failed");
    return 0;
  }

  glShaderSource(shader, 1, &shaderSrc, NULL);
  if (checkGlErrors("BasicTextureRenderer (glShaderSource)")) {
    return 0;
  }

  glCompileShader(shader);
  if (checkGlErrors("BasicTextureRenderer (glCompileShader)")) {
    return 0;
  }

  GLint compiled;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
  if (compiled == GL_FALSE) {
    errors_.addCopy("BasicTextureRenderer: shader is not compiled");
    return 0;
  }

  return shader;
}

bool BasicTextureRenderer::checkGlErrors(const char *title) {
  size_t newErrors = 0;
  char buffer[1024];
  while (GLenum e = glGetError() != GL_NO_ERROR) {
    errorsCount_++;
    newErrors++;
    snprintf(buffer, sizeof(buffer), "OpenGL: %s: %#x", title, e);
    errors_.addCopy(buffer);
  }

  return newErrors != 0;
}

}  // namespace androidgamesdk_deviceinfo
