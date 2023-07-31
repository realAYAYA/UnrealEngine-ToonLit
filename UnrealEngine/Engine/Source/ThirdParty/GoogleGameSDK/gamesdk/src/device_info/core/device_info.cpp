/*
 * Copyright 2018 The Android Open Source Project
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

#include "device_info/device_info.h"
#include "basic_texture_renderer.h"
#include "stream_util/getdelim.h"
#include "texture_test_cases.h"

#include <EGL/egl.h>
// clang-format off
#include <GLES3/gl32.h>
#include <GLES2/gl2ext.h>
// clang-format on
#include <sys/system_properties.h>

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "third_party/nanopb/pb_encode.h"

namespace {
using androidgamesdk_deviceinfo::ProtoDataHolder;
using String = ProtoDataHolder::String;
using StringVector = ProtoDataHolder::StringVector;
using Int32Array = ProtoDataHolder::Array<int32_t>;
using Int64Array = ProtoDataHolder::Array<int64_t>;
using FloatArray = ProtoDataHolder::Array<float>;

namespace string_util {
bool startsWith(const char* text, const char* start) {
  return strncmp(text, start, strlen(start)) == 0;
}

void copyAndTerminate(char* to, const char* from, size_t len) {
  memcpy(to, from, len);
  to[len] = '\0';
}

// Mutates toSplit.
// Splits toSpit with given delimiters into tokens.
// Each char in delimiters is a possible delimiter.
// Adds unique tokens into result.
void splitAddUnique(char* toSplit, const char* delimiters,
                    StringVector& result) {
  char* strtokState;
  for (char* it = strtok_r(toSplit, delimiters, &strtokState); it != nullptr;
       it = strtok_r(nullptr, delimiters, &strtokState)) {
    if (strlen(it) > 0 && !result.has(it)) {
      result.addCopy(it);
    }
  }
}

// Returns a pointer to the position where toSkip chars are skipped.
const char* skipChars(const char* begin, const char* toSkip) {
  const char* result = begin;
  while (strchr(toSkip, *result)) result++;
  return result;
}
}  // namespace string_util

// Returns nullptr on failure, including EOF
// Result is allocated via new[]
// Caller is responsible for delete[]
char* readFileLine(FILE* file) {
  // getline uses malloc/free.
  // Rest of the program uses new/delete.
  // So we copy the buffer and return the new[] result.
  char* mallocBuffer = nullptr;
  size_t mallocBufferSize = 0;

  // Use our own implementation of getline as some C libraries are missing it.
  int resultLen = stream_util::getline(&mallocBuffer, &mallocBufferSize, file);

  if (resultLen < 0) {  // Error.
    free(mallocBuffer);
    return nullptr;
  }

  // Trim the result.
  if (resultLen > 0 && mallocBuffer[resultLen - 1] == '\n') {
    resultLen--;
  }
  char* result = new char[resultLen + 1];
  ::string_util::copyAndTerminate(result, mallocBuffer, resultLen);
  free(mallocBuffer);
  return result;
}

// Returns nullptr on failure.
// Caller is responsible for delete[]
char* readFileSingleLine(const char* fileName) {
  FILE* file = fopen(fileName, "r");
  if (file == nullptr) return nullptr;
  return readFileLine(file);
}

// Returns true on success.
bool readFileInt32(const char* fileName, int32_t& result) {
  std::unique_ptr<char[]> fileContent(readFileSingleLine(fileName));
  if (fileContent.get() == nullptr) return false;
  // Using strto32/64 increases binary file size beyond limit.
  // NOLINTNEXTLINE
  result = strtol(fileContent.get(), nullptr, 10);
  return true;
}

// Returns true on success.
bool readFileInt64(const char* fileName, int64_t& result) {
  std::unique_ptr<char[]> fileContent(readFileSingleLine(fileName));
  if (fileContent.get() == nullptr) return false;
  // Using strto32/64 increases binary file size beyond limit.
  // NOLINTNEXTLINE
  result = strtoll(fileContent.get(), nullptr, 10);
  return true;
}

// Returns true on success.
bool readCpuPresent(String& result) {
  result.data.reset(readFileSingleLine("/sys/devices/system/cpu/present"));
  return result.data.get() != nullptr;
}

// Returns true on success.
bool readCpuPossible(String& result) {
  result.data.reset(readFileSingleLine("/sys/devices/system/cpu/possible"));
  return result.data.get() != nullptr;
}

// Returns true on success.
bool readCpuIndexMax(int& result) {
  return readFileInt32("/sys/devices/system/cpu/kernel_max", result);
}

// Returns true on success.
bool readCpuFreqMax(int cpuIndex, int64_t& result) {
  char fileName[64];
  snprintf(fileName, sizeof(fileName), "%s%d%s", "/sys/devices/system/cpu/cpu",
           cpuIndex, "/cpufreq/cpuinfo_max_freq");
  return readFileInt64(fileName, result);
}

// Returns true on success.
// Reads the system file for lines starting with "Hardware".
// Puts the rest of the lines into dataHolder.
bool readHardware(ProtoDataHolder& dataHolder) {
  FILE* file = fopen("/proc/cpuinfo", "r");
  if (file == nullptr) return false;

  const char* FIELD_KEY = "Hardware";
  while (true) {
    std::unique_ptr<char[]> line(readFileLine(file));
    if (line.get() == nullptr) break;  // Error or EOF
    if (::string_util::startsWith(line.get(), FIELD_KEY)) {
      const char* value = line.get() + strlen(FIELD_KEY);
      // Skip space, tab and colon.
      value = ::string_util::skipChars(value, " \t:");
      dataHolder.hardware.addCopy(value);
    }
  }
  return true;
}

// Returns true on success.
// Reads the system file for lines starting with "Features".
// Splits the rest of the lines.
// Puts the values into dataHolder.
bool readCpuExtensions(ProtoDataHolder& dataHolder) {
  FILE* file = fopen("/proc/cpuinfo", "r");
  if (file == nullptr) return false;

  const char* FIELD_KEY = "Features";
  while (true) {
    std::unique_ptr<char[]> line(readFileLine(file));
    if (line.get() == nullptr) break;  // Error or EOF
    if (::string_util::startsWith(line.get(), FIELD_KEY)) {
      const char* value = line.get() + strlen(FIELD_KEY);
      // Skip space, tab and colon.
      value = ::string_util::skipChars(value, " \t:");
      // It is safe to cast away const, because it will be deleted right after.
      ::string_util::splitAddUnique(const_cast<char*>(value), " ",
                                    dataHolder.cpu_extension);
    }
  }
  return true;
}

// 26 is the required android api version for __system_property_read_callback
// In the time of writing it is 18.
#if __ANDROID_API__ >= 26
#error "Time to switch to __system_property_read_callback"
// Time to move from deprecated __system_property_get
// To __system_property_read_callback
// Do it together with a run-time check of "ro.build.version.sdk"
#endif
// Returns number of errors.
int getSystemProp(const char* key, String& result, StringVector& errors) {
  char buffer[PROP_VALUE_MAX + 1];  // +1 for terminator
  int bufferLen = __system_property_get(key, buffer);
  if (bufferLen > PROP_VALUE_MAX) {
    char error[1024];
    snprintf(error, sizeof(error), "__system_property_get: Overflow: %s", key);
    errors.addCopy(error);
    return 1;
  }
  assert(result.data == nullptr);
  result.data.reset(new char[bufferLen + 1]);
  ::string_util::copyAndTerminate(result.data.get(), buffer, bufferLen);
  return 0;
}

// returns number of errors
int addSystemProperties(ProtoDataHolder& dataHolder) {
  int numErrors = 0;
  numErrors +=
      getSystemProp("ro.build.version.sdk", dataHolder.ro_build_version_sdk,
                    dataHolder.errors);
  numErrors +=
      getSystemProp("ro.build.fingerprint", dataHolder.ro_build_fingerprint,
                    dataHolder.errors);
  return numErrors;
}

// Returns number of errors.
int checkEglError(const char* title, StringVector& errors) {
  EGLint eglError = eglGetError();
  if (eglError == EGL_SUCCESS) return 0;

  char newError[1024];
  snprintf(newError, sizeof(newError), "EGL: %s: %#x", title, eglError);
  errors.addCopy(newError);
  return 1;
}

// Returns number of errors.
int flushGlErrors(const char* title, StringVector& errors) {
  int numErrors = 0;
  char buffer[1024];
  while (GLenum e = glGetError() != GL_NO_ERROR) {
    numErrors++;
    snprintf(buffer, sizeof(buffer), "OpenGL: %s: %#x", title, e);
    errors.addCopy(buffer);
  }
  return numErrors;
}

// Returns number of errors.
int setupEGl(EGLDisplay& display, EGLContext& context, EGLSurface& surface,
             StringVector& errors) {
  display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if (int numErrors = checkEglError("eglGetDisplay", errors)) {
    return numErrors;
  }

  // We do not care about egl version.
  eglInitialize(display, nullptr, nullptr);
  if (int numErrors = checkEglError("eglInitialize", errors)) {
    return numErrors;
  }

  EGLint configAttribs[] = {EGL_SURFACE_TYPE,
                            EGL_PBUFFER_BIT,
                            EGL_RENDERABLE_TYPE,
                            EGL_OPENGL_ES2_BIT,
                            EGL_RED_SIZE,
                            8,
                            EGL_GREEN_SIZE,
                            8,
                            EGL_BLUE_SIZE,
                            8,
                            EGL_ALPHA_SIZE,
                            8,
                            EGL_NONE};
  EGLConfig config;
  EGLint numConfigs = -1;
  eglChooseConfig(display, configAttribs, &config, 1, &numConfigs);
  if (int numErrors = checkEglError("eglChooseConfig", errors)) {
    return numErrors;
  }

  EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
  context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttribs);
  if (int numErrors = checkEglError("eglCreateContext", errors)) {
    return numErrors;
  }

  EGLint pbufferAttribs[] = {
      EGL_WIDTH, androidgamesdk_deviceinfo::OPENGL_TEST_VIEW_WIDTH, EGL_HEIGHT,
      androidgamesdk_deviceinfo::OPENGL_TEST_VIEW_HEIGHT, EGL_NONE};
  surface = eglCreatePbufferSurface(display, config, pbufferAttribs);
  if (int numErrors = checkEglError("eglCreatePbufferSurface", errors)) {
    return numErrors;
  }

  eglMakeCurrent(display, surface, surface, context);
  if (int numErrors = checkEglError("eglMakeCurrent", errors)) {
    return numErrors;
  }

  return 0;
}

// OpenGL utility functions.
namespace ogl_util {
const char* getString(GLenum e) {
  return reinterpret_cast<const char*>(glGetString(e));
}
GLfloat getFloat(GLenum e) {
  GLfloat result = -1;
  glGetFloatv(e, &result);
  return result;
}
GLint getInt(GLenum e) {
  GLint result = -1;
  glGetIntegerv(e, &result);
  return result;
}
GLboolean getBool(GLenum e) {
  GLboolean result = false;
  glGetBooleanv(e, &result);
  return result;
}

// Below functions will be dynamically loaded via eglGetProcAddress if
// we have the necessary OpenGL versions.
typedef const GLubyte* GlStr;
typedef GlStr (*FuncTypeGlGetstringi)(GLenum, GLint);
FuncTypeGlGetstringi glGetStringi = 0;
const char* getStringIndexed(GLenum e, GLuint index) {
  return reinterpret_cast<const char*>(::ogl_util::glGetStringi(e, index));
}

typedef void (*FuncTypeGlGetInteger64v)(GLenum, GLint64*);
FuncTypeGlGetInteger64v glGetInteger64v = 0;
GLint64 getInt64(GLenum e) {
  GLint64 result = -1;
  ::ogl_util::glGetInteger64v(e, &result);
  return result;
}

typedef void (*FuncTypeGlGetIntegeri_v)(GLenum, GLuint, GLint*);
FuncTypeGlGetIntegeri_v glGetIntegeri_v = 0;
GLint getIntIndexed(GLenum e, GLuint index) {
  GLint result = -1;
  ::ogl_util::glGetIntegeri_v(e, index, &result);
  return result;
}
}  // namespace ogl_util

// OpenGL constants requiring minimum version 2.0
void addOglConstsV2_0(androidgamesdk_deviceinfo_GameSdkDeviceInfo_OpenGl& proto,
                      ProtoDataHolder::OpenGl& dataHolder) {
  dataHolder.gl_aliased_line_width_range.setSize(2);
  glGetFloatv(GL_ALIASED_LINE_WIDTH_RANGE,
              dataHolder.gl_aliased_line_width_range.data.get());

  dataHolder.gl_aliased_point_size_range.setSize(2);
  glGetFloatv(GL_ALIASED_POINT_SIZE_RANGE,
              dataHolder.gl_aliased_point_size_range.data.get());

  proto.has_gl_max_combined_texture_image_units = true;
  proto.gl_max_combined_texture_image_units =
      ::ogl_util::getInt(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS);
  proto.has_gl_max_cube_map_texture_size = true;
  proto.gl_max_cube_map_texture_size =
      ::ogl_util::getInt(GL_MAX_CUBE_MAP_TEXTURE_SIZE);
  proto.has_gl_max_fragment_uniform_vectors = true;
  proto.gl_max_fragment_uniform_vectors =
      ::ogl_util::getInt(GL_MAX_FRAGMENT_UNIFORM_VECTORS);
  proto.has_gl_max_renderbuffer_size = true;
  proto.gl_max_renderbuffer_size = ::ogl_util::getInt(GL_MAX_RENDERBUFFER_SIZE);
  proto.has_gl_max_texture_image_units = true;
  proto.gl_max_texture_image_units =
      ::ogl_util::getInt(GL_MAX_TEXTURE_IMAGE_UNITS);
  proto.has_gl_max_texture_size = true;
  proto.gl_max_texture_size = ::ogl_util::getInt(GL_MAX_TEXTURE_SIZE);
  proto.has_gl_max_varying_vectors = true;
  proto.gl_max_varying_vectors = ::ogl_util::getInt(GL_MAX_VARYING_VECTORS);
  proto.has_gl_max_vertex_attribs = true;
  proto.gl_max_vertex_attribs = ::ogl_util::getInt(GL_MAX_VERTEX_ATTRIBS);
  proto.has_gl_max_vertex_texture_image_units = true;
  proto.gl_max_vertex_texture_image_units =
      ::ogl_util::getInt(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS);
  proto.has_gl_max_vertex_uniform_vectors = true;
  proto.gl_max_vertex_uniform_vectors =
      ::ogl_util::getInt(GL_MAX_VERTEX_UNIFORM_VECTORS);

  dataHolder.gl_max_viewport_dims.setSize(2);
  glGetIntegerv(GL_MAX_VIEWPORT_DIMS,
                dataHolder.gl_max_viewport_dims.data.get());

  proto.has_gl_shader_compiler = true;
  proto.gl_shader_compiler = ::ogl_util::getBool(GL_SHADER_COMPILER);
  proto.has_gl_subpixel_bits = true;
  proto.gl_subpixel_bits = ::ogl_util::getInt(GL_SUBPIXEL_BITS);

  GLint numCompressedFormats =
      ::ogl_util::getInt(GL_NUM_COMPRESSED_TEXTURE_FORMATS);
  proto.has_gl_num_compressed_texture_formats = true;
  proto.gl_num_compressed_texture_formats = numCompressedFormats;
  dataHolder.compTexFormats.setSize(numCompressedFormats);
  glGetIntegerv(GL_COMPRESSED_TEXTURE_FORMATS,
                dataHolder.compTexFormats.data.get());

  GLint numShaderBinFormats = ::ogl_util::getInt(GL_NUM_SHADER_BINARY_FORMATS);
  proto.has_gl_num_shader_binary_formats = true;
  proto.gl_num_shader_binary_formats = numShaderBinFormats;
  dataHolder.shaderBinFormats.setSize(numShaderBinFormats);
  glGetIntegerv(GL_SHADER_BINARY_FORMATS,
                dataHolder.shaderBinFormats.data.get());

  // shader precision formats
  GLint spfr[2] = {-1, -1};  // range
  GLint spfp = -1;           // precision
  glGetShaderPrecisionFormat(GL_VERTEX_SHADER, GL_LOW_FLOAT, spfr, &spfp);
  proto.has_spf_vertex_float_low_range_min = true;
  proto.has_spf_vertex_float_low_range_max = true;
  proto.spf_vertex_float_low_range_min = spfr[0];
  proto.spf_vertex_float_low_range_max = spfr[1];
  proto.has_spf_vertex_float_low_prec = true;
  proto.spf_vertex_float_low_prec = spfp;
  glGetShaderPrecisionFormat(GL_VERTEX_SHADER, GL_MEDIUM_FLOAT, spfr, &spfp);
  proto.has_spf_vertex_float_med_range_min = true;
  proto.has_spf_vertex_float_med_range_max = true;
  proto.spf_vertex_float_med_range_min = spfr[0];
  proto.spf_vertex_float_med_range_max = spfr[1];
  proto.has_spf_vertex_float_med_prec = true;
  proto.spf_vertex_float_med_prec = spfp;
  glGetShaderPrecisionFormat(GL_VERTEX_SHADER, GL_HIGH_FLOAT, spfr, &spfp);
  proto.has_spf_vertex_float_hig_range_min = true;
  proto.has_spf_vertex_float_hig_range_max = true;
  proto.spf_vertex_float_hig_range_min = spfr[0];
  proto.spf_vertex_float_hig_range_max = spfr[1];
  proto.has_spf_vertex_float_hig_prec = true;
  proto.spf_vertex_float_hig_prec = spfp;
  glGetShaderPrecisionFormat(GL_VERTEX_SHADER, GL_LOW_INT, spfr, &spfp);
  proto.has_spf_vertex_int_low_range_min = true;
  proto.has_spf_vertex_int_low_range_max = true;
  proto.spf_vertex_int_low_range_min = spfr[0];
  proto.spf_vertex_int_low_range_max = spfr[1];
  proto.has_spf_vertex_int_low_prec = true;
  proto.spf_vertex_int_low_prec = spfp;
  glGetShaderPrecisionFormat(GL_VERTEX_SHADER, GL_MEDIUM_INT, spfr, &spfp);
  proto.has_spf_vertex_int_med_range_min = true;
  proto.has_spf_vertex_int_med_range_max = true;
  proto.spf_vertex_int_med_range_min = spfr[0];
  proto.spf_vertex_int_med_range_max = spfr[1];
  proto.has_spf_vertex_int_med_prec = true;
  proto.spf_vertex_int_med_prec = spfp;
  glGetShaderPrecisionFormat(GL_VERTEX_SHADER, GL_HIGH_INT, spfr, &spfp);
  proto.has_spf_vertex_int_hig_range_min = true;
  proto.has_spf_vertex_int_hig_range_max = true;
  proto.spf_vertex_int_hig_range_min = spfr[0];
  proto.spf_vertex_int_hig_range_max = spfr[1];
  proto.has_spf_vertex_int_hig_prec = true;
  proto.spf_vertex_int_hig_prec = spfp;
  glGetShaderPrecisionFormat(GL_FRAGMENT_SHADER, GL_LOW_FLOAT, spfr, &spfp);
  proto.has_spf_fragment_float_low_range_min = true;
  proto.has_spf_fragment_float_low_range_max = true;
  proto.spf_fragment_float_low_range_min = spfr[0];
  proto.spf_fragment_float_low_range_max = spfr[1];
  proto.has_spf_fragment_float_low_prec = true;
  proto.spf_fragment_float_low_prec = spfp;
  glGetShaderPrecisionFormat(GL_FRAGMENT_SHADER, GL_MEDIUM_FLOAT, spfr, &spfp);
  proto.has_spf_fragment_float_med_range_min = true;
  proto.has_spf_fragment_float_med_range_max = true;
  proto.spf_fragment_float_med_range_min = spfr[0];
  proto.spf_fragment_float_med_range_max = spfr[1];
  proto.has_spf_fragment_float_med_prec = true;
  proto.spf_fragment_float_med_prec = spfp;
  glGetShaderPrecisionFormat(GL_FRAGMENT_SHADER, GL_HIGH_FLOAT, spfr, &spfp);
  proto.has_spf_fragment_float_hig_range_min = true;
  proto.has_spf_fragment_float_hig_range_max = true;
  proto.spf_fragment_float_hig_range_min = spfr[0];
  proto.spf_fragment_float_hig_range_max = spfr[1];
  proto.has_spf_fragment_float_hig_prec = true;
  proto.spf_fragment_float_hig_prec = spfp;
  glGetShaderPrecisionFormat(GL_FRAGMENT_SHADER, GL_LOW_INT, spfr, &spfp);
  proto.has_spf_fragment_int_low_range_min = true;
  proto.has_spf_fragment_int_low_range_max = true;
  proto.spf_fragment_int_low_range_min = spfr[0];
  proto.spf_fragment_int_low_range_max = spfr[1];
  proto.has_spf_fragment_int_low_prec = true;
  proto.spf_fragment_int_low_prec = spfp;
  glGetShaderPrecisionFormat(GL_FRAGMENT_SHADER, GL_MEDIUM_INT, spfr, &spfp);
  proto.has_spf_fragment_int_med_range_min = true;
  proto.has_spf_fragment_int_med_range_max = true;
  proto.spf_fragment_int_med_range_min = spfr[0];
  proto.spf_fragment_int_med_range_max = spfr[1];
  proto.has_spf_fragment_int_med_prec = true;
  proto.spf_fragment_int_med_prec = spfp;
  glGetShaderPrecisionFormat(GL_FRAGMENT_SHADER, GL_HIGH_INT, spfr, &spfp);
  proto.has_spf_fragment_int_hig_range_min = true;
  proto.has_spf_fragment_int_hig_range_max = true;
  proto.spf_fragment_int_hig_range_min = spfr[0];
  proto.spf_fragment_int_hig_range_max = spfr[1];
  proto.has_spf_fragment_int_hig_prec = true;
  proto.spf_fragment_int_hig_prec = spfp;
}
// OpenGL constants requiring minimum version 3.0
void addOglConstsV3_0(androidgamesdk_deviceinfo_GameSdkDeviceInfo_OpenGl& proto,
                      ProtoDataHolder::OpenGl& dataHolder) {
  proto.has_gl_max_3d_texture_size = true;
  proto.gl_max_3d_texture_size = ::ogl_util::getInt(GL_MAX_3D_TEXTURE_SIZE);
  proto.has_gl_max_array_texture_layers = true;
  proto.gl_max_array_texture_layers =
      ::ogl_util::getInt(GL_MAX_ARRAY_TEXTURE_LAYERS);
  proto.has_gl_max_color_attachments = true;
  proto.gl_max_color_attachments = ::ogl_util::getInt(GL_MAX_COLOR_ATTACHMENTS);
  proto.has_gl_max_combined_uniform_blocks = true;
  proto.gl_max_combined_uniform_blocks =
      ::ogl_util::getInt(GL_MAX_COMBINED_UNIFORM_BLOCKS);
  proto.has_gl_max_draw_buffers = true;
  proto.gl_max_draw_buffers = ::ogl_util::getInt(GL_MAX_DRAW_BUFFERS);
  proto.has_gl_max_elements_indices = true;
  proto.gl_max_elements_indices = ::ogl_util::getInt(GL_MAX_ELEMENTS_INDICES);
  proto.has_gl_max_elements_vertices = true;
  proto.gl_max_elements_vertices = ::ogl_util::getInt(GL_MAX_ELEMENTS_VERTICES);
  proto.has_gl_max_fragment_input_components = true;
  proto.gl_max_fragment_input_components =
      ::ogl_util::getInt(GL_MAX_FRAGMENT_INPUT_COMPONENTS);
  proto.has_gl_max_fragment_uniform_blocks = true;
  proto.gl_max_fragment_uniform_blocks =
      ::ogl_util::getInt(GL_MAX_FRAGMENT_UNIFORM_BLOCKS);
  proto.has_gl_max_fragment_uniform_components = true;
  proto.gl_max_fragment_uniform_components =
      ::ogl_util::getInt(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS);
  proto.has_gl_max_program_texel_offset = true;
  proto.gl_max_program_texel_offset =
      ::ogl_util::getInt(GL_MAX_PROGRAM_TEXEL_OFFSET);
  proto.has_gl_max_transform_feedback_interleaved_components = true;
  proto.gl_max_transform_feedback_interleaved_components =
      ::ogl_util::getInt(GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS);
  proto.has_gl_max_transform_feedback_separate_attribs = true;
  proto.gl_max_transform_feedback_separate_attribs =
      ::ogl_util::getInt(GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS);
  proto.has_gl_max_transform_feedback_separate_components = true;
  proto.gl_max_transform_feedback_separate_components =
      ::ogl_util::getInt(GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS);
  proto.has_gl_max_uniform_buffer_bindings = true;
  proto.gl_max_uniform_buffer_bindings =
      ::ogl_util::getInt(GL_MAX_UNIFORM_BUFFER_BINDINGS);
  proto.has_gl_max_varying_components = true;
  proto.gl_max_varying_components =
      ::ogl_util::getInt(GL_MAX_VARYING_COMPONENTS);
  proto.has_gl_max_vertex_output_components = true;
  proto.gl_max_vertex_output_components =
      ::ogl_util::getInt(GL_MAX_VERTEX_OUTPUT_COMPONENTS);
  proto.has_gl_max_vertex_uniform_blocks = true;
  proto.gl_max_vertex_uniform_blocks =
      ::ogl_util::getInt(GL_MAX_VERTEX_UNIFORM_BLOCKS);
  proto.has_gl_max_vertex_uniform_components = true;
  proto.gl_max_vertex_uniform_components =
      ::ogl_util::getInt(GL_MAX_VERTEX_UNIFORM_COMPONENTS);
  proto.has_gl_min_program_texel_offset = true;
  proto.gl_min_program_texel_offset =
      ::ogl_util::getInt(GL_MIN_PROGRAM_TEXEL_OFFSET);
  proto.has_gl_uniform_buffer_offset_alignment = true;
  proto.gl_uniform_buffer_offset_alignment =
      ::ogl_util::getInt(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT);
  proto.has_gl_max_samples = true;
  proto.gl_max_samples = ::ogl_util::getInt(GL_MAX_SAMPLES);

  proto.has_gl_max_texture_lod_bias = true;
  proto.gl_max_texture_lod_bias = ::ogl_util::getFloat(GL_MAX_TEXTURE_LOD_BIAS);

  ::ogl_util::glGetInteger64v =
      reinterpret_cast<::ogl_util::FuncTypeGlGetInteger64v>(
          eglGetProcAddress("glGetInteger64v"));
  proto.has_gl_max_combined_fragment_uniform_components = true;
  proto.gl_max_combined_fragment_uniform_components =
      ::ogl_util::getInt64(GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS);
  proto.has_gl_max_element_index = true;
  proto.gl_max_element_index = ::ogl_util::getInt64(GL_MAX_ELEMENT_INDEX);
  proto.has_gl_max_server_wait_timeout = true;
  proto.gl_max_server_wait_timeout =
      ::ogl_util::getInt64(GL_MAX_SERVER_WAIT_TIMEOUT);
  proto.has_gl_max_uniform_block_size = true;
  proto.gl_max_uniform_block_size =
      ::ogl_util::getInt64(GL_MAX_UNIFORM_BLOCK_SIZE);

  GLint numProgBinFormats = ::ogl_util::getInt(GL_NUM_PROGRAM_BINARY_FORMATS);
  proto.has_gl_num_program_binary_formats = true;
  proto.gl_num_program_binary_formats = numProgBinFormats;
  dataHolder.progBinFormats.setSize(numProgBinFormats);
  glGetIntegerv(GL_PROGRAM_BINARY_FORMATS,
                dataHolder.progBinFormats.data.get());
}
// OpenGL constants requiring minimum version 3.1
void addOglConstsV3_1(androidgamesdk_deviceinfo_GameSdkDeviceInfo_OpenGl& proto) {
  proto.has_gl_max_atomic_counter_buffer_bindings = true;
  proto.gl_max_atomic_counter_buffer_bindings =
      ::ogl_util::getInt(GL_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS);
  proto.has_gl_max_atomic_counter_buffer_size = true;
  proto.gl_max_atomic_counter_buffer_size =
      ::ogl_util::getInt(GL_MAX_ATOMIC_COUNTER_BUFFER_SIZE);
  proto.has_gl_max_color_texture_samples = true;
  proto.gl_max_color_texture_samples =
      ::ogl_util::getInt(GL_MAX_COLOR_TEXTURE_SAMPLES);
  proto.has_gl_max_combined_atomic_counters = true;
  proto.gl_max_combined_atomic_counters =
      ::ogl_util::getInt(GL_MAX_COMBINED_ATOMIC_COUNTERS);
  proto.has_gl_max_combined_atomic_counter_buffers = true;
  proto.gl_max_combined_atomic_counter_buffers =
      ::ogl_util::getInt(GL_MAX_COMBINED_ATOMIC_COUNTER_BUFFERS);
  proto.has_gl_max_combined_compute_uniform_components = true;
  proto.gl_max_combined_compute_uniform_components =
      ::ogl_util::getInt(GL_MAX_COMBINED_COMPUTE_UNIFORM_COMPONENTS);
  proto.has_gl_max_combined_image_uniforms = true;
  proto.gl_max_combined_image_uniforms =
      ::ogl_util::getInt(GL_MAX_COMBINED_IMAGE_UNIFORMS);
  proto.has_gl_max_combined_shader_output_resources = true;
  proto.gl_max_combined_shader_output_resources =
      ::ogl_util::getInt(GL_MAX_COMBINED_SHADER_OUTPUT_RESOURCES);
  proto.has_gl_max_combined_shader_storage_blocks = true;
  proto.gl_max_combined_shader_storage_blocks =
      ::ogl_util::getInt(GL_MAX_COMBINED_SHADER_STORAGE_BLOCKS);
  proto.has_gl_max_compute_atomic_counters = true;
  proto.gl_max_compute_atomic_counters =
      ::ogl_util::getInt(GL_MAX_COMPUTE_ATOMIC_COUNTERS);
  proto.has_gl_max_compute_atomic_counter_buffers = true;
  proto.gl_max_compute_atomic_counter_buffers =
      ::ogl_util::getInt(GL_MAX_COMPUTE_ATOMIC_COUNTER_BUFFERS);
  proto.has_gl_max_compute_image_uniforms = true;
  proto.gl_max_compute_image_uniforms =
      ::ogl_util::getInt(GL_MAX_COMPUTE_IMAGE_UNIFORMS);
  proto.has_gl_max_compute_shader_storage_blocks = true;
  proto.gl_max_compute_shader_storage_blocks =
      ::ogl_util::getInt(GL_MAX_COMPUTE_SHADER_STORAGE_BLOCKS);
  proto.has_gl_max_compute_shared_memory_size = true;
  proto.gl_max_compute_shared_memory_size =
      ::ogl_util::getInt(GL_MAX_COMPUTE_SHARED_MEMORY_SIZE);
  proto.has_gl_max_compute_texture_image_units = true;
  proto.gl_max_compute_texture_image_units =
      ::ogl_util::getInt(GL_MAX_COMPUTE_TEXTURE_IMAGE_UNITS);
  proto.has_gl_max_compute_uniform_blocks = true;
  proto.gl_max_compute_uniform_blocks =
      ::ogl_util::getInt(GL_MAX_COMPUTE_UNIFORM_BLOCKS);
  proto.has_gl_max_compute_uniform_components = true;
  proto.gl_max_compute_uniform_components =
      ::ogl_util::getInt(GL_MAX_COMPUTE_UNIFORM_COMPONENTS);
  proto.has_gl_max_compute_work_group_invocations = true;
  proto.gl_max_compute_work_group_invocations =
      ::ogl_util::getInt(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS);
  proto.has_gl_max_depth_texture_samples = true;
  proto.gl_max_depth_texture_samples =
      ::ogl_util::getInt(GL_MAX_DEPTH_TEXTURE_SAMPLES);
  proto.has_gl_max_fragment_atomic_counters = true;
  proto.gl_max_fragment_atomic_counters =
      ::ogl_util::getInt(GL_MAX_FRAGMENT_ATOMIC_COUNTERS);
  proto.has_gl_max_fragment_atomic_counter_buffers = true;
  proto.gl_max_fragment_atomic_counter_buffers =
      ::ogl_util::getInt(GL_MAX_FRAGMENT_ATOMIC_COUNTER_BUFFERS);
  proto.has_gl_max_fragment_image_uniforms = true;
  proto.gl_max_fragment_image_uniforms =
      ::ogl_util::getInt(GL_MAX_FRAGMENT_IMAGE_UNIFORMS);
  proto.has_gl_max_fragment_shader_storage_blocks = true;
  proto.gl_max_fragment_shader_storage_blocks =
      ::ogl_util::getInt(GL_MAX_FRAGMENT_SHADER_STORAGE_BLOCKS);
  proto.has_gl_max_framebuffer_height = true;
  proto.gl_max_framebuffer_height =
      ::ogl_util::getInt(GL_MAX_FRAMEBUFFER_HEIGHT);
  proto.has_gl_max_framebuffer_samples = true;
  proto.gl_max_framebuffer_samples =
      ::ogl_util::getInt(GL_MAX_FRAMEBUFFER_SAMPLES);
  proto.has_gl_max_framebuffer_width = true;
  proto.gl_max_framebuffer_width = ::ogl_util::getInt(GL_MAX_FRAMEBUFFER_WIDTH);
  proto.has_gl_max_image_units = true;
  proto.gl_max_image_units = ::ogl_util::getInt(GL_MAX_IMAGE_UNITS);
  proto.has_gl_max_integer_samples = true;
  proto.gl_max_integer_samples = ::ogl_util::getInt(GL_MAX_INTEGER_SAMPLES);
  proto.has_gl_max_program_texture_gather_offset = true;
  proto.gl_max_program_texture_gather_offset =
      ::ogl_util::getInt(GL_MAX_PROGRAM_TEXTURE_GATHER_OFFSET);
  proto.has_gl_max_sample_mask_words = true;
  proto.gl_max_sample_mask_words = ::ogl_util::getInt(GL_MAX_SAMPLE_MASK_WORDS);
  proto.has_gl_max_shader_storage_buffer_bindings = true;
  proto.gl_max_shader_storage_buffer_bindings =
      ::ogl_util::getInt(GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS);
  proto.has_gl_max_uniform_locations = true;
  proto.gl_max_uniform_locations = ::ogl_util::getInt(GL_MAX_UNIFORM_LOCATIONS);
  proto.has_gl_max_vertex_atomic_counters = true;
  proto.gl_max_vertex_atomic_counters =
      ::ogl_util::getInt(GL_MAX_VERTEX_ATOMIC_COUNTERS);
  proto.has_gl_max_vertex_atomic_counter_buffers = true;
  proto.gl_max_vertex_atomic_counter_buffers =
      ::ogl_util::getInt(GL_MAX_VERTEX_ATOMIC_COUNTER_BUFFERS);
  proto.has_gl_max_vertex_attrib_bindings = true;
  proto.gl_max_vertex_attrib_bindings =
      ::ogl_util::getInt(GL_MAX_VERTEX_ATTRIB_BINDINGS);
  proto.has_gl_max_vertex_attrib_relative_offset = true;
  proto.gl_max_vertex_attrib_relative_offset =
      ::ogl_util::getInt(GL_MAX_VERTEX_ATTRIB_RELATIVE_OFFSET);
  proto.has_gl_max_vertex_attrib_stride = true;
  proto.gl_max_vertex_attrib_stride =
      ::ogl_util::getInt(GL_MAX_VERTEX_ATTRIB_STRIDE);
  proto.has_gl_max_vertex_image_uniforms = true;
  proto.gl_max_vertex_image_uniforms =
      ::ogl_util::getInt(GL_MAX_VERTEX_IMAGE_UNIFORMS);
  proto.has_gl_max_vertex_shader_storage_blocks = true;
  proto.gl_max_vertex_shader_storage_blocks =
      ::ogl_util::getInt(GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS);
  proto.has_gl_min_program_texture_gather_offset = true;
  proto.gl_min_program_texture_gather_offset =
      ::ogl_util::getInt(GL_MIN_PROGRAM_TEXTURE_GATHER_OFFSET);
  proto.has_gl_shader_storage_buffer_offset_alignment = true;
  proto.gl_shader_storage_buffer_offset_alignment =
      ::ogl_util::getInt(GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT);

  proto.has_gl_max_shader_storage_block_size = true;
  proto.gl_max_shader_storage_block_size =
      ::ogl_util::getInt64(GL_MAX_SHADER_STORAGE_BLOCK_SIZE);

  ::ogl_util::glGetIntegeri_v =
      reinterpret_cast<::ogl_util::FuncTypeGlGetIntegeri_v>(
          eglGetProcAddress("glGetIntegeri_v"));
  proto.has_gl_max_compute_work_group_count_0 = true;
  proto.gl_max_compute_work_group_count_0 =
      ::ogl_util::getIntIndexed(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0);
  proto.has_gl_max_compute_work_group_count_1 = true;
  proto.gl_max_compute_work_group_count_1 =
      ::ogl_util::getIntIndexed(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 1);
  proto.has_gl_max_compute_work_group_count_2 = true;
  proto.gl_max_compute_work_group_count_2 =
      ::ogl_util::getIntIndexed(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 2);
  proto.has_gl_max_compute_work_group_size_0 = true;
  proto.gl_max_compute_work_group_size_0 =
      ::ogl_util::getIntIndexed(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0);
  proto.has_gl_max_compute_work_group_size_1 = true;
  proto.gl_max_compute_work_group_size_1 =
      ::ogl_util::getIntIndexed(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1);
  proto.has_gl_max_compute_work_group_size_2 = true;
  proto.gl_max_compute_work_group_size_2 =
      ::ogl_util::getIntIndexed(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2);
}
// OpenGL constants requiring minimum version 3.2
void addOglConstsV3_2(androidgamesdk_deviceinfo_GameSdkDeviceInfo_OpenGl& proto,
                      ProtoDataHolder::OpenGl& dataHolder) {
  proto.has_gl_context_flags = true;
  proto.gl_context_flags = ::ogl_util::getInt(GL_CONTEXT_FLAGS);
  proto.has_gl_fragment_interpolation_offset_bits = true;
  proto.gl_fragment_interpolation_offset_bits =
      ::ogl_util::getInt(GL_FRAGMENT_INTERPOLATION_OFFSET_BITS);
  proto.has_gl_layer_provoking_vertex = true;
  proto.gl_layer_provoking_vertex =
      ::ogl_util::getInt(GL_LAYER_PROVOKING_VERTEX);
  proto.has_gl_max_combined_geometry_uniform_components = true;
  proto.gl_max_combined_geometry_uniform_components =
      ::ogl_util::getInt(GL_MAX_COMBINED_GEOMETRY_UNIFORM_COMPONENTS);
  proto.has_gl_max_combined_tess_control_uniform_components = true;
  proto.gl_max_combined_tess_control_uniform_components =
      ::ogl_util::getInt(GL_MAX_COMBINED_TESS_CONTROL_UNIFORM_COMPONENTS);
  proto.has_gl_max_combined_tess_evaluation_uniform_components = true;
  proto.gl_max_combined_tess_evaluation_uniform_components =
      ::ogl_util::getInt(GL_MAX_COMBINED_TESS_EVALUATION_UNIFORM_COMPONENTS);
  proto.has_gl_max_debug_group_stack_depth = true;
  proto.gl_max_debug_group_stack_depth =
      ::ogl_util::getInt(GL_MAX_DEBUG_GROUP_STACK_DEPTH);
  proto.has_gl_max_debug_logged_messages = true;
  proto.gl_max_debug_logged_messages = true;
  ::ogl_util::getInt(GL_MAX_DEBUG_LOGGED_MESSAGES);
  proto.has_gl_max_debug_message_length = true;
  proto.gl_max_debug_message_length =
      ::ogl_util::getInt(GL_MAX_DEBUG_MESSAGE_LENGTH);
  proto.has_gl_max_framebuffer_layers = true;
  proto.gl_max_framebuffer_layers =
      ::ogl_util::getInt(GL_MAX_FRAMEBUFFER_LAYERS);
  proto.has_gl_max_geometry_atomic_counters = true;
  proto.gl_max_geometry_atomic_counters =
      ::ogl_util::getInt(GL_MAX_GEOMETRY_ATOMIC_COUNTERS);
  proto.has_gl_max_geometry_atomic_counter_buffers = true;
  proto.gl_max_geometry_atomic_counter_buffers =
      ::ogl_util::getInt(GL_MAX_GEOMETRY_ATOMIC_COUNTER_BUFFERS);
  proto.has_gl_max_geometry_image_uniforms = true;
  proto.gl_max_geometry_image_uniforms =
      ::ogl_util::getInt(GL_MAX_GEOMETRY_IMAGE_UNIFORMS);
  proto.has_gl_max_geometry_input_components = true;
  proto.gl_max_geometry_input_components =
      ::ogl_util::getInt(GL_MAX_GEOMETRY_INPUT_COMPONENTS);
  proto.has_gl_max_geometry_output_components = true;
  proto.gl_max_geometry_output_components =
      ::ogl_util::getInt(GL_MAX_GEOMETRY_OUTPUT_COMPONENTS);
  proto.has_gl_max_geometry_output_vertices = true;
  proto.gl_max_geometry_output_vertices =
      ::ogl_util::getInt(GL_MAX_GEOMETRY_OUTPUT_VERTICES);
  proto.has_gl_max_geometry_shader_invocations = true;
  proto.gl_max_geometry_shader_invocations =
      ::ogl_util::getInt(GL_MAX_GEOMETRY_SHADER_INVOCATIONS);
  proto.has_gl_max_geometry_shader_storage_blocks = true;
  proto.gl_max_geometry_shader_storage_blocks =
      ::ogl_util::getInt(GL_MAX_GEOMETRY_SHADER_STORAGE_BLOCKS);
  proto.has_gl_max_geometry_texture_image_units = true;
  proto.gl_max_geometry_texture_image_units =
      ::ogl_util::getInt(GL_MAX_GEOMETRY_TEXTURE_IMAGE_UNITS);
  proto.has_gl_max_geometry_total_output_components = true;
  proto.gl_max_geometry_total_output_components =
      ::ogl_util::getInt(GL_MAX_GEOMETRY_TOTAL_OUTPUT_COMPONENTS);
  proto.has_gl_max_geometry_uniform_blocks =
      proto.gl_max_geometry_uniform_blocks = true;
  ::ogl_util::getInt(GL_MAX_GEOMETRY_UNIFORM_BLOCKS);
  proto.has_gl_max_geometry_uniform_components = true;
  proto.gl_max_geometry_uniform_components =
      ::ogl_util::getInt(GL_MAX_GEOMETRY_UNIFORM_COMPONENTS);
  proto.has_gl_max_label_length = true;
  proto.gl_max_label_length = ::ogl_util::getInt(GL_MAX_LABEL_LENGTH);
  proto.has_gl_max_patch_vertices = true;
  proto.gl_max_patch_vertices = ::ogl_util::getInt(GL_MAX_PATCH_VERTICES);
  proto.has_gl_max_tess_control_atomic_counters = true;
  proto.gl_max_tess_control_atomic_counters =
      ::ogl_util::getInt(GL_MAX_TESS_CONTROL_ATOMIC_COUNTERS);
  proto.has_gl_max_tess_control_atomic_counter_buffers = true;
  proto.gl_max_tess_control_atomic_counter_buffers =
      ::ogl_util::getInt(GL_MAX_TESS_CONTROL_ATOMIC_COUNTER_BUFFERS);
  proto.has_gl_max_tess_control_image_uniforms = true;
  proto.gl_max_tess_control_image_uniforms =
      ::ogl_util::getInt(GL_MAX_TESS_CONTROL_IMAGE_UNIFORMS);
  proto.has_gl_max_tess_control_input_components = true;
  proto.gl_max_tess_control_input_components =
      ::ogl_util::getInt(GL_MAX_TESS_CONTROL_INPUT_COMPONENTS);
  proto.has_gl_max_tess_control_output_components = true;
  proto.gl_max_tess_control_output_components =
      ::ogl_util::getInt(GL_MAX_TESS_CONTROL_OUTPUT_COMPONENTS);
  proto.has_gl_max_tess_control_shader_storage_blocks = true;
  proto.gl_max_tess_control_shader_storage_blocks =
      ::ogl_util::getInt(GL_MAX_TESS_CONTROL_SHADER_STORAGE_BLOCKS);
  proto.has_gl_max_tess_control_texture_image_units = true;
  proto.gl_max_tess_control_texture_image_units =
      ::ogl_util::getInt(GL_MAX_TESS_CONTROL_TEXTURE_IMAGE_UNITS);
  proto.has_gl_max_tess_control_total_output_components = true;
  proto.gl_max_tess_control_total_output_components =
      ::ogl_util::getInt(GL_MAX_TESS_CONTROL_TOTAL_OUTPUT_COMPONENTS);
  proto.has_gl_max_tess_control_uniform_blocks = true;
  proto.gl_max_tess_control_uniform_blocks =
      ::ogl_util::getInt(GL_MAX_TESS_CONTROL_UNIFORM_BLOCKS);
  proto.has_gl_max_tess_control_uniform_components = true;
  proto.gl_max_tess_control_uniform_components =
      ::ogl_util::getInt(GL_MAX_TESS_CONTROL_UNIFORM_COMPONENTS);
  proto.has_gl_max_tess_evaluation_atomic_counters = true;
  proto.gl_max_tess_evaluation_atomic_counters =
      ::ogl_util::getInt(GL_MAX_TESS_EVALUATION_ATOMIC_COUNTERS);
  proto.has_gl_max_tess_evaluation_atomic_counter_buffers = true;
  proto.gl_max_tess_evaluation_atomic_counter_buffers =
      ::ogl_util::getInt(GL_MAX_TESS_EVALUATION_ATOMIC_COUNTER_BUFFERS);
  proto.has_gl_max_tess_evaluation_image_uniforms = true;
  proto.gl_max_tess_evaluation_image_uniforms =
      ::ogl_util::getInt(GL_MAX_TESS_EVALUATION_IMAGE_UNIFORMS);
  proto.has_gl_max_tess_evaluation_input_components = true;
  proto.gl_max_tess_evaluation_input_components =
      ::ogl_util::getInt(GL_MAX_TESS_EVALUATION_INPUT_COMPONENTS);
  proto.has_gl_max_tess_evaluation_output_components = true;
  proto.gl_max_tess_evaluation_output_components =
      ::ogl_util::getInt(GL_MAX_TESS_EVALUATION_OUTPUT_COMPONENTS);
  proto.has_gl_max_tess_evaluation_shader_storage_blocks = true;
  proto.gl_max_tess_evaluation_shader_storage_blocks =
      ::ogl_util::getInt(GL_MAX_TESS_EVALUATION_SHADER_STORAGE_BLOCKS);
  proto.has_gl_max_tess_evaluation_texture_image_units = true;
  proto.gl_max_tess_evaluation_texture_image_units =
      ::ogl_util::getInt(GL_MAX_TESS_EVALUATION_TEXTURE_IMAGE_UNITS);
  proto.has_gl_max_tess_evaluation_uniform_blocks = true;
  proto.gl_max_tess_evaluation_uniform_blocks =
      ::ogl_util::getInt(GL_MAX_TESS_EVALUATION_UNIFORM_BLOCKS);
  proto.has_gl_max_tess_evaluation_uniform_components = true;
  proto.gl_max_tess_evaluation_uniform_components =
      ::ogl_util::getInt(GL_MAX_TESS_EVALUATION_UNIFORM_COMPONENTS);
  proto.has_gl_max_tess_gen_level = true;
  proto.gl_max_tess_gen_level = ::ogl_util::getInt(GL_MAX_TESS_GEN_LEVEL);
  proto.has_gl_max_tess_patch_components = true;
  proto.gl_max_tess_patch_components =
      ::ogl_util::getInt(GL_MAX_TESS_PATCH_COMPONENTS);
  proto.has_gl_max_texture_buffer_size = true;
  proto.gl_max_texture_buffer_size =
      ::ogl_util::getInt(GL_MAX_TEXTURE_BUFFER_SIZE);
  proto.has_gl_texture_buffer_offset_alignment = true;
  proto.gl_texture_buffer_offset_alignment =
      ::ogl_util::getInt(GL_TEXTURE_BUFFER_OFFSET_ALIGNMENT);
  proto.has_gl_reset_notification_strategy = true;
  proto.gl_reset_notification_strategy =
      ::ogl_util::getInt(GL_RESET_NOTIFICATION_STRATEGY);
  proto.has_gl_max_fragment_interpolation_offset = true;
  proto.gl_max_fragment_interpolation_offset =
      ::ogl_util::getFloat(GL_MAX_FRAGMENT_INTERPOLATION_OFFSET);
  proto.has_gl_min_fragment_interpolation_offset = true;
  proto.gl_min_fragment_interpolation_offset =
      ::ogl_util::getFloat(GL_MIN_FRAGMENT_INTERPOLATION_OFFSET);
  proto.has_gl_multisample_line_width_granularity = true;
  proto.gl_multisample_line_width_granularity =
      ::ogl_util::getFloat(GL_MULTISAMPLE_LINE_WIDTH_GRANULARITY);

  dataHolder.gl_multisample_line_width_range.setSize(2);
  glGetFloatv(GL_MULTISAMPLE_LINE_WIDTH_RANGE,
              dataHolder.gl_multisample_line_width_range.data.get());

  proto.has_gl_primitive_restart_for_patches_supported = true;
  proto.gl_primitive_restart_for_patches_supported =
      ::ogl_util::getBool(GL_PRIMITIVE_RESTART_FOR_PATCHES_SUPPORTED);
}

// returns number of errors
int addGl(androidgamesdk_deviceinfo_GameSdkDeviceInfo_OpenGl& proto,
          ProtoDataHolder::OpenGl& dataHolder, StringVector& errors) {
  int numErrors = 0;

  dataHolder.renderer.copy(::ogl_util::getString(GL_RENDERER));
  dataHolder.vendor.copy(::ogl_util::getString(GL_VENDOR));
  dataHolder.version.copy(::ogl_util::getString(GL_VERSION));
  dataHolder.shading_language_version.copy(
      ::ogl_util::getString(GL_SHADING_LANGUAGE_VERSION));

  numErrors += flushGlErrors("generic information", errors);

  GLint glVerMajor = -1;
  GLint glVerMinor = -1;
  glGetIntegerv(GL_MAJOR_VERSION, &glVerMajor);
  // if GL_MAJOR_VERSION is not recognized, assume version 2.0
  if (glGetError() != GL_NO_ERROR) {
    glVerMajor = 2;
    glVerMinor = 0;
  } else {
    glGetIntegerv(GL_MINOR_VERSION, &glVerMinor);
  }
  proto.has_version_major = true;
  proto.version_major = glVerMajor;
  proto.has_version_minor = true;
  proto.version_minor = glVerMinor;

  // gl extensions
  if (glVerMajor >= 3) {
    int numExts = -1;
    glGetIntegerv(GL_NUM_EXTENSIONS, &numExts);
    ::ogl_util::glGetStringi =
        reinterpret_cast<::ogl_util::FuncTypeGlGetstringi>(
            eglGetProcAddress("glGetStringi"));
    for (int i = 0; i < numExts; i++) {
      dataHolder.extension.addCopy(
          ::ogl_util::getStringIndexed(GL_EXTENSIONS, i));
    }
  } else {
    String exts;
    exts.copy(::ogl_util::getString(GL_EXTENSIONS));
    ::string_util::splitAddUnique(exts.data.get(), " ", dataHolder.extension);
  }

  if (glVerMajor > 2 || (glVerMajor == 2 && glVerMinor >= 0)) {  // >= 2.0
    addOglConstsV2_0(proto, dataHolder);
  }
  if (glVerMajor > 3 || (glVerMajor == 3 && glVerMinor >= 0)) {  // >= 3.0
    addOglConstsV3_0(proto, dataHolder);
  }
  if (glVerMajor > 3 || (glVerMajor == 3 && glVerMinor >= 1)) {  // >= 3.1
    addOglConstsV3_1(proto);
  }
  if (glVerMajor > 3 || (glVerMajor == 3 && glVerMinor >= 2)) {  // >= 3.2
    addOglConstsV3_2(proto, dataHolder);
  }

  numErrors += flushGlErrors("OpenGL consts", errors);

  return numErrors;
}

// Below are nano proto callback functions for encoding.
bool protoEncodeString(pb_ostream_t* stream, const pb_field_t* field,
                       void* const* arg) {
  const String& string = **(String**)(arg);
  const char* chars = string.data.get();
  if (chars == nullptr) return true;  // Valid case of unset string.
  if (!pb_encode_tag_for_field(stream, field)) return false;
  return pb_encode_string(stream, (pb_byte_t*)chars, strlen(chars));
}
bool protoEncodeStringVector(pb_ostream_t* stream, const pb_field_t* field,
                             void* const* arg) {
  const StringVector& strings = **(StringVector**)(arg);
  for (int i = 0; i < strings.size; i++) {
    if (!pb_encode_tag_for_field(stream, field)) return false;
    const char* chars = strings.data[i].data.get();
    if (!pb_encode_string(stream, (pb_byte_t*)chars, strlen(chars)))
      return false;
  }
  return true;
}
bool protoEncodeInt32Vector(pb_ostream_t* stream, const pb_field_t* field,
                            void* const* arg) {
  const Int32Array& ints = **(Int32Array**)(arg);
  for (int i = 0; i < ints.size; i++) {
    if (!pb_encode_tag_for_field(stream, field)) return false;
    if (!pb_encode_varint(stream, ints.data[i])) return false;
  }
  return true;
}
bool protoEncodeFloatVector(pb_ostream_t* stream, const pb_field_t* field,
                            void* const* arg) {
  const FloatArray& floats = **(FloatArray**)(arg);
  for (int i = 0; i < floats.size; i++) {
    if (!pb_encode_tag_for_field(stream, field)) return false;
    if (!pb_encode_fixed32(stream, &floats.data[i])) return false;
  }
  return true;
}
bool protoEncodeCpuFreqs(pb_ostream_t* stream, const pb_field_t* field,
                         void* const* arg) {
  const ProtoDataHolder& dataHolder = **(ProtoDataHolder**)(arg);
  const Int64Array& cpuFreqs = dataHolder.cpuFreqs;
  for (size_t i = 0; i < cpuFreqs.size; i++) {
    androidgamesdk_deviceinfo_GameSdkDeviceInfo_CpuCore core =
        androidgamesdk_deviceinfo_GameSdkDeviceInfo_CpuCore_init_zero;
    int freq = cpuFreqs.data[i];
    if (freq > 0) {
      core.has_frequency_max = true;
      core.frequency_max = freq;
    }

    if (!pb_encode_tag_for_field(stream, field)) return false;
    if (!pb_encode_submessage(
            stream,
            androidgamesdk_deviceinfo_GameSdkDeviceInfo_CpuCore_fields, &core))
      return false;
  }
  return true;
}

}  // namespace

namespace androidgamesdk_deviceinfo {

/**
 * Using BasicTextureRenderer, load and render the specified texture on the
 * specified EGLSurface, then compares that the output pixels match the expected
 * pixels. This is done to ensure that the compressed texture was properly
 * loaded and rendered.
 */
bool testCompressedTextureRendering(StringVector& errors, EGLDisplay eglDisplay,
                                    EGLSurface eglSurface,
                                    TextureTestCase* config) {
  if (!config) {
    errors.addCopy(
        "testCompressedTextureRendering: a test case is misconfigured.");
    return false;
  }

  // Load and render the texture on the surface
  BasicTextureRenderer basicTextureRenderer(errors);
  if (!basicTextureRenderer.loadProgramObject()) {
    errors.addCopy(
        "testCompressedTextureRendering: Unable to load program object.");
    return false;
  }

  if (!basicTextureRenderer.loadTexture(config->internalformat, config->width,
                                        config->height, config->dataSize,
                                        config->data)) {
    errors.addCopy("testCompressedTextureRendering: Unable to load texture.");
    return false;
  }

  if (!basicTextureRenderer.draw(OPENGL_TEST_VIEW_WIDTH,
                                 OPENGL_TEST_VIEW_HEIGHT)) {
    errors.addCopy("testCompressedTextureRendering: Unable to draw.");
    return false;
  }

  eglSwapBuffers(eglDisplay, eglSurface);
  checkEglError("testCompressedTextureRendering", errors);

  // Read the rendered pixels and make sure that texture was properly drawn
  bool deltaCheckSuccess = true;
  bool exactCheckSuccess = true;
  RGBA pixelsData[OPENGL_TEST_VIEW_WIDTH * OPENGL_TEST_VIEW_HEIGHT];
  glReadPixels(0, 0, OPENGL_TEST_VIEW_WIDTH, OPENGL_TEST_VIEW_HEIGHT, GL_RGBA,
               GL_UNSIGNED_BYTE, pixelsData);
  if (flushGlErrors("testCompressedTextureRendering (glReadPixels)", errors) !=
      0) {
    return false;
  }

  // Delta check is an approximate (or exact if delta is all 0) check of
  // the rendered pixels
  for (size_t i = 0; i < OPENGL_TEST_VIEW_WIDTH * OPENGL_TEST_VIEW_HEIGHT;
       i++) {
    if (abs(pixelsData[i].r - config->expectedPixelsColor.r) >
            config->allowedPixelsDelta.r ||
        abs(pixelsData[i].g - config->expectedPixelsColor.g) >
            config->allowedPixelsDelta.g ||
        abs(pixelsData[i].b - config->expectedPixelsColor.b) >
            config->allowedPixelsDelta.b) {
      char renderingError[1024];
      snprintf(renderingError, sizeof(renderingError),
               "testCompressedTextureRendering (delta check): Bad pixel "
               "at index %zu for format: %#x. "
               "Got: %d;%d;%d.",
               i, config->internalformat, pixelsData[i].r, pixelsData[i].g,
               pixelsData[i].b);
      errors.addCopy(renderingError);

      deltaCheckSuccess = false;
    }
  }

  // Exact check is an optional verification of the rendered pixels, compared
  // to expected pixels.
  if (config->expectedRenderedPixels != nullptr) {
    for (size_t i = 0; i < OPENGL_TEST_VIEW_WIDTH * OPENGL_TEST_VIEW_HEIGHT;
         i++) {
      if (config->expectedRenderedPixels[i].r != pixelsData[i].r ||
          config->expectedRenderedPixels[i].g != pixelsData[i].g ||
          config->expectedRenderedPixels[i].b != pixelsData[i].b) {
        char renderingError[1024];
        snprintf(renderingError, sizeof(renderingError),
                 "testCompressedTextureRendering (exact check): Bad pixel "
                 "at index %zu for format: %#x. "
                 "Got: %d;%d;%d.",
                 i, config->internalformat, pixelsData[i].r, pixelsData[i].g,
                 pixelsData[i].b);
        errors.addCopy(renderingError);

        exactCheckSuccess = false;
      }
    }
  }

  return deltaCheckSuccess && exactCheckSuccess;
}

void addGlCompressedTexturesRendering(
    EGLDisplay eglDisplay, EGLSurface eglSurface,
    androidgamesdk_deviceinfo_GameSdkDeviceInfo_OpenGl& openglProto,
    ProtoDataHolder::OpenGl& openGlDataHolder, StringVector& errors) {
  openglProto.has_rendered_compressed_texture_formats = true;
  auto& rendered_compressed_texture_formats =
      openglProto.rendered_compressed_texture_formats;

  // ASTC tests
  if (openGlDataHolder.compTexFormats.includes(
          GL_COMPRESSED_RGBA_ASTC_4x4_KHR)) {
    rendered_compressed_texture_formats.has_gl_compressed_rgba_astc_4x4_khr =
        true;
    rendered_compressed_texture_formats.gl_compressed_rgba_astc_4x4_khr =
        testCompressedTextureRendering(
            errors, eglDisplay, eglSurface,
            getCompressedTextureTestCase(GL_COMPRESSED_RGBA_ASTC_4x4_KHR));
  }
  if (openGlDataHolder.compTexFormats.includes(
          GL_COMPRESSED_RGBA_ASTC_6x6_KHR)) {
    rendered_compressed_texture_formats.has_gl_compressed_rgba_astc_6x6_khr =
        true;
    rendered_compressed_texture_formats.gl_compressed_rgba_astc_6x6_khr =
        testCompressedTextureRendering(
            errors, eglDisplay, eglSurface,
            getCompressedTextureTestCase(GL_COMPRESSED_RGBA_ASTC_6x6_KHR));
  }
  if (openGlDataHolder.compTexFormats.includes(
          GL_COMPRESSED_RGBA_ASTC_8x8_KHR)) {
    rendered_compressed_texture_formats.has_gl_compressed_rgba_astc_8x8_khr =
        true;
    rendered_compressed_texture_formats.gl_compressed_rgba_astc_8x8_khr =
        testCompressedTextureRendering(
            errors, eglDisplay, eglSurface,
            getCompressedTextureTestCase(GL_COMPRESSED_RGBA_ASTC_8x8_KHR));
  }

  // PVRTC tests
  if (openGlDataHolder.compTexFormats.includes(
          GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG)) {
    rendered_compressed_texture_formats.has_gl_compressed_rgb_pvrtc_2bppv1_img =
        true;
    rendered_compressed_texture_formats.gl_compressed_rgb_pvrtc_2bppv1_img =
        testCompressedTextureRendering(
            errors, eglDisplay, eglSurface,
            getCompressedTextureTestCase(GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG));
  }
  if (openGlDataHolder.compTexFormats.includes(
          GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG)) {
    rendered_compressed_texture_formats
        .has_gl_compressed_rgba_pvrtc_2bppv1_img = true;
    rendered_compressed_texture_formats.gl_compressed_rgba_pvrtc_2bppv1_img =
        testCompressedTextureRendering(
            errors, eglDisplay, eglSurface,
            getCompressedTextureTestCase(GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG));
  }
  if (openGlDataHolder.compTexFormats.includes(
          GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG)) {
    rendered_compressed_texture_formats.has_gl_compressed_rgb_pvrtc_4bppv1_img =
        true;
    rendered_compressed_texture_formats.gl_compressed_rgb_pvrtc_4bppv1_img =
        testCompressedTextureRendering(
            errors, eglDisplay, eglSurface,
            getCompressedTextureTestCase(GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG));
  }
  if (openGlDataHolder.compTexFormats.includes(
          GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG)) {
    rendered_compressed_texture_formats
        .has_gl_compressed_rgba_pvrtc_4bppv1_img = true;
    rendered_compressed_texture_formats.gl_compressed_rgba_pvrtc_4bppv1_img =
        testCompressedTextureRendering(
            errors, eglDisplay, eglSurface,
            getCompressedTextureTestCase(GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG));
  }

  // DXT tests
  if (openGlDataHolder.compTexFormats.includes(
          GL_COMPRESSED_RGBA_S3TC_DXT1_EXT)) {
    rendered_compressed_texture_formats.has_gl_compressed_rgba_s3tc_dxt1_ext =
        true;
    rendered_compressed_texture_formats.gl_compressed_rgba_s3tc_dxt1_ext =
        testCompressedTextureRendering(
            errors, eglDisplay, eglSurface,
            getCompressedTextureTestCase(GL_COMPRESSED_RGBA_S3TC_DXT1_EXT));
  }
  if (openGlDataHolder.compTexFormats.includes(
          GL_COMPRESSED_RGBA_S3TC_DXT5_EXT)) {
    rendered_compressed_texture_formats.has_gl_compressed_rgba_s3tc_dxt5_ext =
        true;
    rendered_compressed_texture_formats.gl_compressed_rgba_s3tc_dxt5_ext =
        testCompressedTextureRendering(
            errors, eglDisplay, eglSurface,
            getCompressedTextureTestCase(GL_COMPRESSED_RGBA_S3TC_DXT5_EXT));
  }

  // ETC2
  if (openGlDataHolder.compTexFormats.includes(GL_COMPRESSED_RGB8_ETC2)) {
    rendered_compressed_texture_formats.has_gl_compressed_rgb8_etc2 = true;
    rendered_compressed_texture_formats.gl_compressed_rgb8_etc2 =
        testCompressedTextureRendering(
            errors, eglDisplay, eglSurface,
            getCompressedTextureTestCase(GL_COMPRESSED_RGB8_ETC2));
  }

  // ETC1
  if (openGlDataHolder.compTexFormats.includes(GL_ETC1_RGB8_OES)) {
    rendered_compressed_texture_formats.has_gl_etc1_rgb8_oes = true;
    rendered_compressed_texture_formats.gl_etc1_rgb8_oes =
        testCompressedTextureRendering(
            errors, eglDisplay, eglSurface,
            getCompressedTextureTestCase(GL_ETC1_RGB8_OES));
  }
}

void String::copy(const char* from) {
  size_t len = strlen(from);
  char* dataNew = new char[len + 1];
  ::string_util::copyAndTerminate(dataNew, from, len);
  data.reset(dataNew);
}

void StringVector::addCopy(const char* orig) {
  if (size == sizeMax) {
    sizeMax *= 2;
    String* dataNew = new String[sizeMax]();
    for (int i = 0; i < size; i++) {
      dataNew[i].data.swap(data[i].data);
    }
    data.reset(dataNew);
  }
  String& newElement = data[size++];
  newElement.copy(orig);
}

bool StringVector::has(const char* element) {
  for (int i = 0; i < size; i++) {
    if (strcmp(data[i].data.get(), element) == 0) return true;
  }
  return false;
}

int createProto(androidgamesdk_deviceinfo_GameSdkDeviceInfoWithErrors& proto,
                ProtoDataHolder& dataHolder) {
  int numErrors = 0;

  // Zero out everything in proto.
  proto = androidgamesdk_deviceinfo_GameSdkDeviceInfoWithErrors_init_zero;

  proto.has_info = true;
  androidgamesdk_deviceinfo_GameSdkDeviceInfo& info = proto.info;

  if (readCpuIndexMax(dataHolder.cpuIndexMax)) {
    info.has_cpu_max_index = true;
    info.cpu_max_index = dataHolder.cpuIndexMax;
    dataHolder.cpuFreqs.setSize(dataHolder.cpuIndexMax + 1);
    for (int cpuIndex = 0; cpuIndex <= dataHolder.cpuIndexMax; cpuIndex++) {
      dataHolder.cpuFreqs.data[cpuIndex] = 0;

      if (!readCpuFreqMax(cpuIndex, dataHolder.cpuFreqs.data[cpuIndex])) {
        // Don't mark a missing cpu frequency as an error, as there might be CPUs
        // with non sequential indexes. The frequency will stay to 0 and omitted
        // when encoded to the proto.
      }
    }
    info.cpu_core.arg = &dataHolder;
    info.cpu_core.funcs.encode = &protoEncodeCpuFreqs;
  } else {
    numErrors++;
    dataHolder.errors.addCopy("Cpu index max: Could not read file.");
  }

  if (readCpuPresent(dataHolder.cpu_present)) {
    info.cpu_present.arg = &dataHolder.cpu_present;
    info.cpu_present.funcs.encode = &protoEncodeString;
  } else {
    numErrors++;
    dataHolder.errors.addCopy("Cpu present: Could not read file.");
  }

  if (readCpuPossible(dataHolder.cpu_possible)) {
    info.cpu_possible.arg = &dataHolder.cpu_possible;
    info.cpu_possible.funcs.encode = &protoEncodeString;
  } else {
    numErrors++;
    dataHolder.errors.addCopy("Cpu possible: Could not read file.");
  }

  if (readHardware(dataHolder)) {
    info.hardware.arg = &dataHolder.hardware;
    info.hardware.funcs.encode = &protoEncodeStringVector;
  } else {
    numErrors++;
    dataHolder.errors.addCopy("Hardware: Could not read file.");
  }

  if (readCpuExtensions(dataHolder)) {
    info.cpu_extension.arg = &dataHolder.cpu_extension;
    info.cpu_extension.funcs.encode = &protoEncodeStringVector;
  } else {
    numErrors++;
    dataHolder.errors.addCopy("Features: Could not read file.");
  }

  numErrors += addSystemProperties(dataHolder);
  info.ro_build_version_sdk.arg = &dataHolder.ro_build_version_sdk;
  info.ro_build_version_sdk.funcs.encode = &protoEncodeString;
  info.ro_build_fingerprint.arg = &dataHolder.ro_build_fingerprint;
  info.ro_build_fingerprint.funcs.encode = &protoEncodeString;

  EGLDisplay eglDisplay = nullptr;
  EGLContext eglContext = nullptr;
  EGLSurface eglSurface = nullptr;
  int numErrorsEgl =
      setupEGl(eglDisplay, eglContext, eglSurface, dataHolder.errors);
  numErrors += numErrorsEgl;
  if (numErrorsEgl == 0) {
    info.has_open_gl = true;
    androidgamesdk_deviceinfo_GameSdkDeviceInfo_OpenGl& protoGl = info.open_gl;
    numErrors += addGl(protoGl, dataHolder.ogl, dataHolder.errors);

    protoGl.renderer.arg = &dataHolder.ogl.renderer;
    protoGl.renderer.funcs.encode = &protoEncodeString;

    protoGl.vendor.arg = &dataHolder.ogl.vendor;
    protoGl.vendor.funcs.encode = &protoEncodeString;

    protoGl.version.arg = &dataHolder.ogl.version;
    protoGl.version.funcs.encode = &protoEncodeString;

    protoGl.shading_language_version.arg =
        &dataHolder.ogl.shading_language_version;
    protoGl.shading_language_version.funcs.encode = &protoEncodeString;

    protoGl.extension.arg = &dataHolder.ogl.extension;
    protoGl.extension.funcs.encode = &protoEncodeStringVector;

    protoGl.gl_compressed_texture_formats.arg = &dataHolder.ogl.compTexFormats;
    protoGl.gl_compressed_texture_formats.funcs.encode =
        &protoEncodeInt32Vector;

    protoGl.gl_shader_binary_formats.arg = &dataHolder.ogl.shaderBinFormats;
    protoGl.gl_shader_binary_formats.funcs.encode = &protoEncodeInt32Vector;

    protoGl.gl_aliased_line_width_range.arg =
        &dataHolder.ogl.gl_aliased_line_width_range;
    protoGl.gl_aliased_line_width_range.funcs.encode = &protoEncodeFloatVector;

    protoGl.gl_aliased_point_size_range.arg =
        &dataHolder.ogl.gl_aliased_point_size_range;
    protoGl.gl_aliased_point_size_range.funcs.encode = &protoEncodeFloatVector;

    protoGl.gl_max_viewport_dims.arg = &dataHolder.ogl.gl_max_viewport_dims;
    protoGl.gl_max_viewport_dims.funcs.encode = &protoEncodeInt32Vector;

    protoGl.gl_program_binary_formats.arg = &dataHolder.ogl.progBinFormats;
    protoGl.gl_program_binary_formats.funcs.encode = &protoEncodeInt32Vector;

    protoGl.gl_multisample_line_width_range.arg =
        &dataHolder.ogl.gl_multisample_line_width_range;
    protoGl.gl_multisample_line_width_range.funcs.encode =
        &protoEncodeFloatVector;
  }

  if (numErrorsEgl == 0 && eglDisplay != nullptr && eglContext != nullptr &&
      eglSurface != nullptr) {
    addGlCompressedTexturesRendering(eglDisplay, eglSurface, info.open_gl,
                                     dataHolder.ogl, dataHolder.errors);
  } else {
    numErrors++;
    dataHolder.errors.addCopy(
        "Compressed textures: skipping because EGL errors");
  }

  proto.error.arg = &dataHolder.errors;
  proto.error.funcs.encode = &protoEncodeStringVector;

  return numErrors;
}
}  // namespace androidgamesdk_deviceinfo
