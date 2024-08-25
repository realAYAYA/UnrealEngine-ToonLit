// Copyright Epic Games, Inc. All Rights Reserved.

#include "PixelCaptureCapturerRHIToI420CPU.h"
#include "PixelCaptureInputFrameRHI.h"
#include "PixelCaptureOutputFrameI420.h"
#include "PixelCaptureBufferFormat.h"
#include "PixelCaptureUtils.h"

#include "Async/Async.h"
#include "Containers/UnrealString.h"

#include "libyuv/convert.h"

TSharedPtr<FPixelCaptureCapturerRHIToI420CPU> FPixelCaptureCapturerRHIToI420CPU::Create(float InScale)
{
	return TSharedPtr<FPixelCaptureCapturerRHIToI420CPU>(new FPixelCaptureCapturerRHIToI420CPU(InScale));
}

FPixelCaptureCapturerRHIToI420CPU::FPixelCaptureCapturerRHIToI420CPU(float InScale)
	: Scale(InScale)
{
}

FPixelCaptureCapturerRHIToI420CPU::~FPixelCaptureCapturerRHIToI420CPU()
{
}

void FPixelCaptureCapturerRHIToI420CPU::Initialize(int32 InputWidth, int32 InputHeight)
{
	const int32 Width = InputWidth * Scale;
	const int32 Height = InputHeight * Scale;

	FRHITextureCreateDesc TextureDesc =
		FRHITextureCreateDesc::Create2D(TEXT("FPixelCaptureCapturerRHIToI420CPU StagingTexture"), Width, Height, EPixelFormat::PF_B8G8R8A8)
			.SetClearValue(FClearValueBinding::None)
			.SetFlags(ETextureCreateFlags::RenderTargetable)
			.SetInitialState(ERHIAccess::CopySrc)
			.DetermineInititialState();

	if (RHIGetInterfaceType() == ERHIInterfaceType::Vulkan)
	{
		TextureDesc.AddFlags(ETextureCreateFlags::External);
	}
	else
	{
		TextureDesc.AddFlags(ETextureCreateFlags::Shared);
	}

	StagingTexture = RHICreateTexture(TextureDesc);

	TextureReader = MakeShared<FRHIGPUTextureReadback>(TEXT("FPixelCaptureCapturerRHIToI420CPUReadback"));

	FPixelCaptureCapturer::Initialize(InputWidth, InputHeight);
}

IPixelCaptureOutputFrame* FPixelCaptureCapturerRHIToI420CPU::CreateOutputBuffer(int32 InputWidth, int32 InputHeight)
{
	const int32 Width = InputWidth * Scale;
	const int32 Height = InputHeight * Scale;
	return new FPixelCaptureOutputFrameI420(MakeShared<FPixelCaptureBufferI420>(Width, Height));
}

void FPixelCaptureCapturerRHIToI420CPU::BeginProcess(const IPixelCaptureInputFrame& InputFrame, IPixelCaptureOutputFrame* OutputBuffer)
{
	checkf(InputFrame.GetType() == StaticCast<int32>(PixelCaptureBufferFormat::FORMAT_RHI), TEXT("Incorrect source frame coming into frame capture process."));

	MarkCPUWorkStart();

	const FPixelCaptureInputFrameRHI& RHISourceFrame = StaticCast<const FPixelCaptureInputFrameRHI&>(InputFrame);
	FTexture2DRHIRef SourceTexture = RHISourceFrame.FrameTexture;

	FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();

	RHICmdList.EnqueueLambda([this](FRHICommandListImmediate&) { MarkGPUWorkStart(); });

	RHICmdList.Transition(FRHITransitionInfo(SourceTexture, ERHIAccess::Unknown, ERHIAccess::CopySrc));
	RHICmdList.Transition(FRHITransitionInfo(StagingTexture, ERHIAccess::CopySrc, ERHIAccess::CopyDest));
	CopyTexture(RHICmdList, SourceTexture, StagingTexture, nullptr);
	RHICmdList.Transition(FRHITransitionInfo(StagingTexture, ERHIAccess::CopyDest, ERHIAccess::CopySrc));

	TextureReader->EnqueueCopy(RHICmdList, StagingTexture, FIntVector(0, 0, 0), 0, FIntVector(StagingTexture->GetSizeXY().X, StagingTexture->GetSizeXY().Y, 0));

	MarkCPUWorkEnd();

	// by adding this shared ref to the rhi lambda we can ensure that 'this' will not be destroyed
	// until after the rhi thread is done with it, so all the commands will still have valid references.
	TSharedRef<FPixelCaptureCapturerRHIToI420CPU> ThisRHIRef = StaticCastSharedRef<FPixelCaptureCapturerRHIToI420CPU>(AsShared());
	RHICmdList.EnqueueLambda([ThisRHIRef, OutputBuffer](FRHICommandListImmediate&) {
		ThisRHIRef->CheckComplete(OutputBuffer);
	});
}

void FPixelCaptureCapturerRHIToI420CPU::CheckComplete(IPixelCaptureOutputFrame* OutputBuffer)
{
	if (!TextureReader->IsReady())
	{
		TSharedRef<FPixelCaptureCapturerRHIToI420CPU> ThisRHIRef = StaticCastSharedRef<FPixelCaptureCapturerRHIToI420CPU>(AsShared());
		AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [ThisRHIRef, OutputBuffer]() { ThisRHIRef->CheckComplete(OutputBuffer); });
	}
	else
	{
		TSharedRef<FPixelCaptureCapturerRHIToI420CPU> ThisRHIRef = StaticCastSharedRef<FPixelCaptureCapturerRHIToI420CPU>(AsShared());
		AsyncTask(ENamedThreads::ActualRenderingThread, [ThisRHIRef, OutputBuffer]() { ThisRHIRef->OnRHIStageComplete(OutputBuffer); });
	}
}

void FPixelCaptureCapturerRHIToI420CPU::OnRHIStageComplete(IPixelCaptureOutputFrame* OutputBuffer)
{
	MarkGPUWorkEnd();
	MarkCPUWorkStart();

	int32 OutRowPitchInPixels = 0;
	void* PixelData = TextureReader->Lock(OutRowPitchInPixels);

	FPixelCaptureOutputFrameI420* OutputI420Buffer = StaticCast<FPixelCaptureOutputFrameI420*>(OutputBuffer);
	TSharedPtr<FPixelCaptureBufferI420> I420Buffer = OutputI420Buffer->GetI420Buffer();
	libyuv::ARGBToI420(
		static_cast<uint8*>(PixelData),
		OutRowPitchInPixels * 4,
		I420Buffer->GetMutableDataY(),
		I420Buffer->GetStrideY(),
		I420Buffer->GetMutableDataU(),
		I420Buffer->GetStrideUV(),
		I420Buffer->GetMutableDataV(),
		I420Buffer->GetStrideUV(),
		I420Buffer->GetWidth(),
		I420Buffer->GetHeight());

	TextureReader->Unlock();

	MarkCPUWorkEnd();
	EndProcess();
}
