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

#include "texture_test_cases.h"
#include "device_info/device_info.h"

#include <assert.h>

// clang-format off
#include <GLES3/gl32.h>
#include <GLES2/gl2ext.h>
// clang-format on

// Dummy textures used for offscreen rendering tests:
// * .c files implementing these were generated with xxd.
// * Expected results were dumped from rendered pixels from a working device.
//
// Note: these externs are not in a header as they are not part of the public
// API. Use getCompressedTextureTestCase if you want to use them.
extern unsigned char green32_astc_4x4_astc[];
extern unsigned int green32_astc_4x4_astc_len;
extern unsigned char green32_astc_6x6_astc[];
extern unsigned int green32_astc_6x6_astc_len;
extern unsigned char green32_astc_8x8_astc[];
extern unsigned int green32_astc_8x8_astc_len;
extern unsigned char green32_pvrtci_2bpp_RGB_expected_rgba[];
extern unsigned int green32_pvrtci_2bpp_RGB_expected_rgba_len;
extern unsigned char green32_pvrtci_2bpp_RGB_pvr[];
extern unsigned int green32_pvrtci_2bpp_RGB_pvr_len;
extern unsigned char green32_pvrtci_2bpp_RGBA_pvr[];
extern unsigned int green32_pvrtci_2bpp_RGBA_pvr_len;
extern unsigned char green32_pvrtci_4bpp_RGB_expected_rgba[];
extern unsigned int green32_pvrtci_4bpp_RGB_expected_rgba_len;
extern unsigned char green32_pvrtci_4bpp_RGB_pvr[];
extern unsigned int green32_pvrtci_4bpp_RGB_pvr_len;
extern unsigned char green32_pvrtci_4bpp_RGBA_pvr[];
extern unsigned int green32_pvrtci_4bpp_RGBA_pvr_len;
extern unsigned char green32_dxt1_bc1_pvr[];
extern unsigned int green32_dxt1_bc1_pvr_len;
extern unsigned char green32_dxt5_bc3_expected_rgba[];
extern unsigned int green32_dxt5_bc3_expected_rgba_len;
extern unsigned char green32_dxt5_bc3_pvr[];
extern unsigned int green32_dxt5_bc3_pvr_len;
extern unsigned char green32_etc2_pvr[];
extern unsigned int green32_etc2_pvr_len;
extern unsigned char green32_etc1_rgb_pvr[];
extern unsigned int green32_etc1_rgb_pvr_len;

namespace androidgamesdk_deviceinfo {

constexpr GLsizei greenTextureWidth = 32;
constexpr GLsizei greenTextureHeight = 32;
constexpr int greenTextureRed = 34;
constexpr int greenTextureGreen = 177;
constexpr int greenTextureBlue = 76;

// An ideal rendering would be the exact color of the texture without deltas
RGB expectedPixelsColor{greenTextureRed, greenTextureGreen, greenTextureBlue};
RGB noDeltaAllowed{0, 0, 0};

constexpr size_t astcHeaderSize = 16;  // ASTC header is always 16 bytes
constexpr size_t pvrHeaderSize = 52;  // PVR header with no metadata is 52 bytes
RGB pvrtcAllowedDelta{2, 1, 1};
constexpr size_t pvrDxtHeaderSize =
    52;  // PVR header with no metadata is 52 bytes
RGB dxtAllowedDelta{1, 1, 2};
constexpr size_t pvrEtc2HeaderSize =
    52;  // PVR header with no metadata is 52 bytes
constexpr size_t pvrEtc1HeaderSize =
    52;  // PVR header with no metadata is 52 bytes

constexpr size_t allCompressedTextureTestCasesCount = 11;
TextureTestCase
    allCompressedTextureTestCases[allCompressedTextureTestCasesCount] = {
        {GL_COMPRESSED_RGBA_ASTC_4x4_KHR, greenTextureWidth, greenTextureHeight,
         green32_astc_4x4_astc_len - astcHeaderSize,
         &green32_astc_4x4_astc[astcHeaderSize], expectedPixelsColor,
         noDeltaAllowed},
        {GL_COMPRESSED_RGBA_ASTC_6x6_KHR, greenTextureWidth, greenTextureHeight,
         green32_astc_6x6_astc_len - astcHeaderSize,
         &green32_astc_6x6_astc[astcHeaderSize], expectedPixelsColor,
         noDeltaAllowed},
        {GL_COMPRESSED_RGBA_ASTC_8x8_KHR, greenTextureWidth, greenTextureHeight,
         green32_astc_8x8_astc_len - astcHeaderSize,
         &green32_astc_8x8_astc[astcHeaderSize], expectedPixelsColor,
         noDeltaAllowed},

        // PVRTC tests
        {GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG, greenTextureWidth,
         greenTextureHeight, green32_pvrtci_2bpp_RGB_pvr_len - pvrHeaderSize,
         &green32_pvrtci_2bpp_RGB_pvr[pvrHeaderSize], expectedPixelsColor,
         pvrtcAllowedDelta, (RGBA*)green32_pvrtci_2bpp_RGB_expected_rgba},
        {GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG, greenTextureWidth,
         greenTextureHeight, green32_pvrtci_2bpp_RGBA_pvr_len - pvrHeaderSize,
         &green32_pvrtci_2bpp_RGBA_pvr[pvrHeaderSize], expectedPixelsColor,
         pvrtcAllowedDelta, (RGBA*)green32_pvrtci_2bpp_RGB_expected_rgba},
        {GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG, greenTextureWidth,
         greenTextureHeight, green32_pvrtci_4bpp_RGB_pvr_len - pvrHeaderSize,
         &green32_pvrtci_4bpp_RGB_pvr[pvrHeaderSize], expectedPixelsColor,
         pvrtcAllowedDelta, (RGBA*)green32_pvrtci_4bpp_RGB_expected_rgba},
        {GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG, greenTextureWidth,
         greenTextureHeight, green32_pvrtci_4bpp_RGBA_pvr_len - pvrHeaderSize,
         &green32_pvrtci_4bpp_RGBA_pvr[pvrHeaderSize], expectedPixelsColor,
         pvrtcAllowedDelta, (RGBA*)green32_pvrtci_4bpp_RGB_expected_rgba},

        // DXT tests
        {GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, greenTextureWidth,
         greenTextureHeight, green32_dxt1_bc1_pvr_len - pvrDxtHeaderSize,
         &green32_dxt1_bc1_pvr[pvrDxtHeaderSize], expectedPixelsColor,
         dxtAllowedDelta},
        {GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, greenTextureWidth,
         greenTextureHeight, green32_dxt5_bc3_pvr_len - pvrDxtHeaderSize,
         &green32_dxt5_bc3_pvr[pvrDxtHeaderSize], expectedPixelsColor,
         dxtAllowedDelta, (RGBA*)green32_dxt5_bc3_expected_rgba},

        // ETC2 tests
        {GL_COMPRESSED_RGB8_ETC2,
         greenTextureWidth,
         greenTextureHeight,
         green32_etc2_pvr_len - pvrEtc2HeaderSize,
         &green32_etc2_pvr[pvrEtc2HeaderSize],
         {greenTextureRed - 2, greenTextureGreen, greenTextureBlue + 1},
         noDeltaAllowed},

        // ETC1 tests
        {GL_ETC1_RGB8_OES,
         greenTextureWidth,
         greenTextureHeight,
         green32_etc1_rgb_pvr_len - pvrEtc1HeaderSize,
         &green32_etc1_rgb_pvr[pvrEtc1HeaderSize],
         {greenTextureRed + 1, greenTextureGreen - 2, greenTextureBlue},
         noDeltaAllowed}};

TextureTestCase* getCompressedTextureTestCase(GLenum internalformat) {
  // Sanity check for the *_expected_rgba. If this is erroring, you either
  // modified OPENGL_TEST_VIEW_WIDTH or OPENGL_TEST_VIEW_HEIGHT without updating
  // *_expected_rgba, or wrongly updated *_expected_rgba (check the size of the
  // array).
  constexpr size_t expected_rgba_len =
      OPENGL_TEST_VIEW_WIDTH * OPENGL_TEST_VIEW_HEIGHT * sizeof(RGBA);
  unsigned int expected_rgba_size = expected_rgba_len * sizeof(unsigned char);
  assert(green32_dxt5_bc3_expected_rgba_len == expected_rgba_size);
  assert(green32_pvrtci_2bpp_RGB_expected_rgba_len == expected_rgba_size);
  assert(green32_pvrtci_4bpp_RGB_expected_rgba_len == expected_rgba_size);
  // expected_rgba_size could be seen as unused when compiled in opt mode.
  // Consider [[maybe_unused]] once C++17 is supported everywhere
  (void)(expected_rgba_size);

  for (size_t i = 0; i < allCompressedTextureTestCasesCount; ++i) {
    if (allCompressedTextureTestCases[i].internalformat == internalformat) {
      return &allCompressedTextureTestCases[i];
    }
  }

  return nullptr;
}

}  // namespace androidgamesdk_deviceinfo
