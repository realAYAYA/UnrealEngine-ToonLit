// Copyright Epic Games, Inc. All Rights Reserved.

#include "PixelCaptureCapturerRHI.h"
#include "PixelCaptureInputFrameRHI.h"
#include "PixelCaptureOutputFrameRHI.h"
#include "PixelCaptureBufferFormat.h"
#include "PixelCaptureUtils.h"

#include "Async/Async.h"

TSharedPtr<FPixelCaptureCapturerRHI> FPixelCaptureCapturerRHI::Create(float InScale)
{
	return TSharedPtr<FPixelCaptureCapturerRHI>(new FPixelCaptureCapturerRHI(InScale));
}

FPixelCaptureCapturerRHI::FPixelCaptureCapturerRHI(float InScale)
	: Scale(InScale)
{
	Fence = GDynamicRHI->RHICreateGPUFence(TEXT("FPixelCaptureCapturerRHI Fence"));
}

IPixelCaptureOutputFrame* FPixelCaptureCapturerRHI::CreateOutputBuffer(int32 InputWidth, int32 InputHeight)
{
	const int32 Width = InputWidth * Scale;
	const int32 Height = InputHeight * Scale;

	FRHITextureCreateDesc TextureDesc =
		FRHITextureCreateDesc::Create2D(TEXT("FFrameDataH264 Texture"), Width, Height, EPixelFormat::PF_B8G8R8A8)
			.SetClearValue(FClearValueBinding::None)
			.SetFlags(ETextureCreateFlags::RenderTargetable)
			.SetInitialState(ERHIAccess::Present)
			.DetermineInititialState();

	if (RHIGetInterfaceType() == ERHIInterfaceType::Vulkan)
	{
		TextureDesc.AddFlags(ETextureCreateFlags::External);
	}
	else
	{
		TextureDesc.AddFlags(ETextureCreateFlags::Shared);
	}

	return new FPixelCaptureOutputFrameRHI(GDynamicRHI->RHICreateTexture(TextureDesc));
}

void FPixelCaptureCapturerRHI::BeginProcess(const IPixelCaptureInputFrame& InputFrame, IPixelCaptureOutputFrame* OutputBuffer)
{
	checkf(InputFrame.GetType() == StaticCast<int32>(PixelCaptureBufferFormat::FORMAT_RHI), TEXT("Incorrect source frame coming into frame capture process."));

	MarkCPUWorkStart();

	FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
	RHICmdList.EnqueueLambda([this](FRHICommandListImmediate&) { MarkGPUWorkStart(); });

	const FPixelCaptureInputFrameRHI& RHISourceFrame = StaticCast<const FPixelCaptureInputFrameRHI&>(InputFrame);
	FPixelCaptureOutputFrameRHI* OutputH264Buffer = StaticCast<FPixelCaptureOutputFrameRHI*>(OutputBuffer);
	CopyTexture(RHICmdList, RHISourceFrame.FrameTexture, OutputH264Buffer->GetFrameTexture(), Fence);

	// by adding this shared ref to the rhi lambda we can ensure that 'this' will not be destroyed
	// until after the rhi thread is done with it, so all the commands will still have valid references.
	TSharedRef<FPixelCaptureCapturerRHI> ThisRHIRef = StaticCastSharedRef<FPixelCaptureCapturerRHI>(AsShared());
	RHICmdList.EnqueueLambda([ThisRHIRef](FRHICommandListImmediate&) { ThisRHIRef->CheckComplete(); });

	MarkCPUWorkEnd();
}

void FPixelCaptureCapturerRHI::CheckComplete()
{
	// in lieu of a proper callback we need to capture a thread to poll the fence
	// so we know as quickly as possible when we can readback.
	TSharedRef<FPixelCaptureCapturerRHI> ThisRHIRef = StaticCastSharedRef<FPixelCaptureCapturerRHI>(AsShared());
	AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [ThisRHIRef]() {
		while (!ThisRHIRef->Fence->Poll())
		{
			// we want to check quickly but we dont want to burn CPU.
			FPlatformProcess::Sleep(0.0001f);
		}
		ThisRHIRef->OnRHIStageComplete();
	});
}

void FPixelCaptureCapturerRHI::OnRHIStageComplete()
{
	checkf(Fence->Poll(), TEXT("Fence was not set. Backbuffer copy may not have completed."));
	Fence->Clear();
	MarkGPUWorkEnd();
	EndProcess();
}
