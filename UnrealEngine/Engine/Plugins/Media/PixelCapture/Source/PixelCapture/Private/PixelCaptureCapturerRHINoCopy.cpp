// Copyright Epic Games, Inc. All Rights Reserved.

#include "PixelCaptureCapturerRHINoCopy.h"
#include "PixelCaptureInputFrameRHI.h"
#include "PixelCaptureOutputFrameRHI.h"
#include "PixelCaptureBufferFormat.h"
#include "PixelCaptureUtils.h"

#include "Async/Async.h"

TSharedPtr<FPixelCaptureCapturerRHINoCopy> FPixelCaptureCapturerRHINoCopy::Create(float InScale)
{
	return TSharedPtr<FPixelCaptureCapturerRHINoCopy>(new FPixelCaptureCapturerRHINoCopy(InScale));
}

FPixelCaptureCapturerRHINoCopy::FPixelCaptureCapturerRHINoCopy(float InScale)
	: Scale(InScale)
{
}

IPixelCaptureOutputFrame* FPixelCaptureCapturerRHINoCopy::CreateOutputBuffer(int32 InputWidth, int32 InputHeight)
{
	return new FPixelCaptureOutputFrameRHI(nullptr);
}

void FPixelCaptureCapturerRHINoCopy::BeginProcess(const IPixelCaptureInputFrame& InputFrame, IPixelCaptureOutputFrame* OutputBuffer)
{
	checkf(InputFrame.GetType() == StaticCast<int32>(PixelCaptureBufferFormat::FORMAT_RHI), TEXT("Incorrect source frame coming into frame capture process."));

	MarkCPUWorkStart();

	const FPixelCaptureInputFrameRHI& RHISourceFrame = StaticCast<const FPixelCaptureInputFrameRHI&>(InputFrame);
	FPixelCaptureOutputFrameRHI* OutputH264Buffer = StaticCast<FPixelCaptureOutputFrameRHI*>(OutputBuffer);
	OutputH264Buffer->SetFrameTexture(RHISourceFrame.FrameTexture);

	MarkCPUWorkEnd();

	EndProcess();
}