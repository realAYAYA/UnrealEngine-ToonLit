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
	if (!CurrentOutputBuffer)
	{
		UE_LOG(LogPixelCapture, Error, TEXT("Failed to obtain a produce buffer."));
		return;
	}

	InitMetadata(InputFrame.Metadata.Copy());
	StartTime = rtc::TimeMillis();
	CPUStartTime = 0;
	GPUEnqueueTime = 0;
	GPUStartTime = 0;

	// Todo (Luke.Bermingham) - Converting output buffer to raw ptr here seems error prone considering how Buffer->LockProduceBuffer() works.
	BeginProcess(InputFrame, CurrentOutputBuffer.Get());
}

void FPixelCaptureCapturer::Initialize(int32 InputWidth, int32 InputHeight)
{
	checkf(InputWidth > 0 && InputHeight > 0, TEXT("Capture should be initialized with non-zero resolution."));
	Buffer = MakeUnique<UE::PixelCapture::FOutputFrameBuffer>();
	Buffer->Reset(3, 10, [this, InputWidth, InputHeight]() { return TSharedPtr<IPixelCaptureOutputFrame>(CreateOutputBuffer(InputWidth, InputHeight)); });
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
	CPUStartTime = rtc::TimeMillis();
}

void FPixelCaptureCapturer::MarkCPUWorkEnd()
{
	check(CurrentOutputBuffer != nullptr);
	CurrentOutputBuffer->Metadata.CaptureProcessCPUTime += rtc::TimeMillis() - CPUStartTime;
	CPUStartTime = 0;

	GPUEnqueueTime = rtc::TimeMillis();
}

void FPixelCaptureCapturer::MarkGPUWorkStart()
{
	if (GPUStartTime != 0)
	{
		MarkGPUWorkEnd();
	}
	GPUStartTime = rtc::TimeMillis();

	CurrentOutputBuffer->Metadata.CaptureProcessGPUDelay += GPUStartTime - GPUEnqueueTime;
	GPUEnqueueTime = 0;
}

void FPixelCaptureCapturer::MarkGPUWorkEnd()
{
	CurrentOutputBuffer->Metadata.CaptureProcessGPUTime += rtc::TimeMillis() - GPUStartTime;
	GPUStartTime = 0;
}

void FPixelCaptureCapturer::InitMetadata(FPixelCaptureFrameMetadata Metadata)
{
	Metadata.Id = FrameId.Increment();
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

	CurrentOutputBuffer->Metadata.CaptureTime += rtc::TimeMillis() - StartTime;
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
