// Copyright Epic Games, Inc. All Rights Reserved.

#include "PixelCaptureCapturer.h"
#include "OutputFrameBuffer.h"
#include "PixelCapturePrivate.h"

// defined here so we can delete the FOutputFrameBuffer
FPixelCaptureCapturer::FPixelCaptureCapturer() = default;
FPixelCaptureCapturer::~FPixelCaptureCapturer() = default;

TSharedPtr<IPixelCaptureOutputFrame> FPixelCaptureCapturer::ReadOutput()
{
	if (bHasOutput)
	{
		return Buffer->GetConsumeBuffer();
	}
	return nullptr;
}

void FPixelCaptureCapturer::Capture(const IPixelCaptureInputFrame& InputFrame)
{
	if (IsBusy())
	{
		return;
	}

	bBusy = true;

	const int32 InputWidth = InputFrame.GetWidth();
	const int32 InputHeight = InputFrame.GetHeight();

	if (!IsInitialized())
	{
		Initialize(InputWidth, InputHeight);
	}

	checkf(InputWidth == ExpectedInputWidth && InputHeight == ExpectedInputHeight, TEXT("Capturer input resolution changes are not supported"));

	CurrentOutputBuffer = Buffer->LockProduceBuffer();
	if (CurrentOutputBuffer == nullptr)
	{
		UE_LOG(LogPixelCapture, Error, TEXT("Failed to obtain a produce buffer."));
		return;
	}

	InitMetadata(InputFrame.Metadata.Copy());
	StartTime = FPlatformTime::Cycles64();
	CPUStartTime = 0;
	GPUStartTime = 0;

	BeginProcess(InputFrame, CurrentOutputBuffer.Get());
}

void FPixelCaptureCapturer::Initialize(int32 InputWidth, int32 InputHeight)
{
	Buffer = MakeUnique<UE::PixelCapture::FOutputFrameBuffer>();
	Buffer->Reset(3, 10, [=]() { return TSharedPtr<IPixelCaptureOutputFrame>(CreateOutputBuffer(InputWidth, InputHeight)); });
	ExpectedInputWidth = InputWidth;
	ExpectedInputHeight = InputHeight;
	bHasOutput = false;
	bInitialized = true;
}

void FPixelCaptureCapturer::MarkCPUWorkStart()
{
	if (CPUStartTime != 0)
	{
		MarkCPUWorkEnd();
	}
	CPUStartTime = FPlatformTime::Cycles64();
}

void FPixelCaptureCapturer::MarkCPUWorkEnd()
{
	CurrentOutputBuffer->Metadata.CaptureProcessCPUTime += FPlatformTime::Cycles64() - CPUStartTime;
	CPUStartTime = 0;
}

void FPixelCaptureCapturer::MarkGPUWorkStart()
{
	if (GPUStartTime != 0)
	{
		MarkGPUWorkEnd();
	}
	GPUStartTime = FPlatformTime::Cycles64();
}

void FPixelCaptureCapturer::MarkGPUWorkEnd()
{
	CurrentOutputBuffer->Metadata.CaptureProcessGPUTime += FPlatformTime::Cycles64() - GPUStartTime;
	GPUStartTime = 0;
}

void FPixelCaptureCapturer::InitMetadata(FPixelCaptureFrameMetadata Metadata)
{
	Metadata.ProcessName = GetCapturerName();
	Metadata.CaptureTime = 0;
	Metadata.CaptureProcessCPUTime = 0;
	Metadata.CaptureProcessGPUTime = 0;
	CurrentOutputBuffer->Metadata = Metadata;
}

void FPixelCaptureCapturer::FinalizeMetadata()
{
	if (CPUStartTime != 0)
	{
		MarkCPUWorkEnd();
	}

	if (GPUStartTime != 0)
	{
		MarkGPUWorkEnd();
	}

	CurrentOutputBuffer->Metadata.CaptureTime += FPlatformTime::Cycles64() - StartTime;
}

void FPixelCaptureCapturer::EndProcess()
{
	checkf(bBusy, TEXT("Capture process EndProcess called but we're not busy. Maybe double called?"));

	FinalizeMetadata();
	
	CurrentOutputBuffer = nullptr;
	Buffer->ReleaseProduceBuffer();
	bBusy = false;
	bHasOutput = true;

	OnComplete.Broadcast();
}
