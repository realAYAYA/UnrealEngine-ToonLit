// Copyright Epic Games, Inc. All Rights Reserved.

#include "PixelCaptureInputFrameI420.h"
#include "PixelCaptureBufferFormat.h"

FPixelCaptureInputFrameI420::FPixelCaptureInputFrameI420(TSharedPtr<FPixelCaptureBufferI420> Buffer)
	: I420Buffer(Buffer)
{
	Metadata.SourceTime = rtc::TimeMillis();
}

int32 FPixelCaptureInputFrameI420::GetType() const
{
	return I420Buffer->GetFormat();
}

int32 FPixelCaptureInputFrameI420::GetWidth() const
{
	return I420Buffer->GetWidth();
}

int32 FPixelCaptureInputFrameI420::GetHeight() const
{
	return I420Buffer->GetHeight();
}

TSharedPtr<FPixelCaptureBufferI420> FPixelCaptureInputFrameI420::GetBuffer() const
{
	return I420Buffer;
}
