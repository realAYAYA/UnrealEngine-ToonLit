// Copyright Epic Games, Inc. All Rights Reserved.

#include "PixelCaptureCapturerI420.h"
#include "PixelCaptureOutputFrameI420.h"
#include "PixelCaptureInputFrameI420.h"
#include "PixelCaptureBufferFormat.h"

void FPixelCaptureCapturerI420::Initialize(int32 InputWidth, int32 InputHeight)
{
	FPixelCaptureCapturer::Initialize(InputWidth, InputHeight);
}

IPixelCaptureOutputFrame* FPixelCaptureCapturerI420::CreateOutputBuffer(int32 InputWidth, int32 InputHeight)
{
	return new FPixelCaptureOutputFrameI420(MakeShared<FPixelCaptureI420Buffer>(InputWidth, InputHeight));
}

void FPixelCaptureCapturerI420::BeginProcess(const IPixelCaptureInputFrame& InputFrame, IPixelCaptureOutputFrame* OutputBuffer)
{
	checkf(InputFrame.GetType() == StaticCast<int32>(PixelCaptureBufferFormat::FORMAT_I420), TEXT("Incorrect source frame coming into frame capture process, expected FORMAT_I420 frame."));

	MarkCPUWorkStart();

	// just copy the input frame
	const FPixelCaptureInputFrameI420& SourceFrame = StaticCast<const FPixelCaptureInputFrameI420&>(InputFrame);
	FPixelCaptureOutputFrameI420* OutputI420Buffer = StaticCast<FPixelCaptureOutputFrameI420*>(OutputBuffer);
	OutputI420Buffer->GetI420Buffer()->Copy(*SourceFrame.GetBuffer());

	MarkCPUWorkEnd();
	EndProcess();
}
