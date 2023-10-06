// Copyright Epic Games, Inc. All Rights Reserved.

#include "PixelStreamingVideoInputViewport.h"
#include "Settings.h"
#include "Utils.h"
#include "PixelCaptureInputFrameRHI.h"
#include "PixelCaptureBufferFormat.h"
#include "PixelCaptureCapturerRHI.h"
#include "PixelCaptureCapturerRHIToI420CPU.h"
#include "PixelCaptureCapturerRHIToI420Compute.h"
#include "UnrealClient.h"
#include "Engine/GameViewportClient.h"
#include "RenderingThread.h"
#include "Framework/Application/SlateApplication.h"
#include "Slate/SceneViewport.h"
#include "IPixelStreamingModule.h"

TSharedPtr<FPixelStreamingVideoInputViewport> FPixelStreamingVideoInputViewport::Create(TSharedPtr<IPixelStreamingStreamer> InAssociatedStreamer)
{
	TSharedPtr<FPixelStreamingVideoInputViewport> NewInput = TSharedPtr<FPixelStreamingVideoInputViewport>(new FPixelStreamingVideoInputViewport());
	NewInput->AssociatedStreamer = InAssociatedStreamer;
	TWeakPtr<FPixelStreamingVideoInputViewport> WeakInput = NewInput;

	// Set up the callback on the game thread since FSlateApplication::Get() can only be used there
	UE::PixelStreaming::DoOnGameThread([WeakInput]() {
		if (TSharedPtr<FPixelStreamingVideoInputViewport> Input = WeakInput.Pin())
		{
			Input->DelegateHandle = UGameViewportClient::OnViewportRendered().AddSP(Input.ToSharedRef(), &FPixelStreamingVideoInputViewport::OnViewportRendered);
		}
	});

	return NewInput;
}

FPixelStreamingVideoInputViewport::~FPixelStreamingVideoInputViewport()
{
	if (!IsEngineExitRequested())
	{
		UE::PixelStreaming::DoOnGameThread([HandleCopy = DelegateHandle]() {
			UGameViewportClient::OnViewportRendered().Remove(HandleCopy);
		});
	}
}

bool FPixelStreamingVideoInputViewport::FilterViewport(const FViewport* InViewport)
{
	TSharedPtr<IPixelStreamingStreamer> Streamer = AssociatedStreamer.Pin();
	if (!Streamer.IsValid() || !Streamer->IsStreaming())
	{
		return false;
	}

	TSharedPtr<SViewport> TargetScene = Streamer->GetTargetViewport().Pin();
	if (!TargetScene.IsValid())
	{
		return false;
	}

	if (InViewport == nullptr)
	{
		return false;
	}

	if (InViewport->GetViewportType() != TargetViewportType)
	{
		return false;
	}

	// Bit dirty to do a static cast here, but we check viewport type just above so it is somewhat "safe".
	TSharedPtr<SViewport> InScene = StaticCast<const FSceneViewport*>(InViewport)->GetViewportWidget().Pin();

	// If the viewport we were passed is not our target viewport we are not interested in getting its texture.
	if (TargetScene != InScene)
	{
		return false;
	}



	return true;
}

void FPixelStreamingVideoInputViewport::OnViewportRendered(FViewport* InViewport)
{
	if (!FilterViewport(InViewport))
	{
		return;
	}

	const FTextureRHIRef& FrameBuffer = InViewport->GetRenderTargetTexture();
	if (!FrameBuffer)
	{
		return;
	}

	ENQUEUE_RENDER_COMMAND(StreamViewportTextureCommand)
	([&, FrameBuffer](FRHICommandList& RHICmdList) {
		OnFrame(FPixelCaptureInputFrameRHI(FrameBuffer));
	});
}

FString FPixelStreamingVideoInputViewport::ToString()
{
	return TEXT("the Target Viewport");
}