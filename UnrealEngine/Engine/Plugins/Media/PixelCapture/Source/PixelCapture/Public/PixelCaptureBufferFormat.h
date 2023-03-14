// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * This is a list of all internal buffer formats supported by Pixel Capture. To implement
 * your own buffer format value you must start your values at FORMAT_USER.
 */
namespace PixelCaptureBufferFormat
{
    static const int32 FORMAT_UNKNOWN = 0;
    static const int32 FORMAT_RHI = 1;
    static const int32 FORMAT_I420 = 2;

    static const int32 FORMAT_USER = 128;
}
