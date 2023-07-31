// Copyright Epic Games, Inc. All Rights Reserved.

#include "PixelStreamingVideoInputI420.h"
#include "PixelStreamingPrivate.h"

#include "PixelCaptureBufferFormat.h"
#include "PixelCaptureCapturerI420.h"
#include "PixelCaptureCapturerI420ToRHI.h"

TSharedPtr<FPixelCaptureCapturer> FPixelStreamingVideoInputI420::CreateCapturer(int32 FinalFormat, float FinalScale)
{
	switch (FinalFormat)
	{
		case PixelCaptureBufferFormat::FORMAT_RHI:
		{
			return FPixelCaptureCapturerI420ToRHI::Create();
		}
		case PixelCaptureBufferFormat::FORMAT_I420:
		{
			return MakeShared<FPixelCaptureCapturerI420>();
		}
		default:
			UE_LOG(LogPixelStreaming, Error, TEXT("Unsupported final format %d"), FinalFormat);
			return nullptr;
	}
}
