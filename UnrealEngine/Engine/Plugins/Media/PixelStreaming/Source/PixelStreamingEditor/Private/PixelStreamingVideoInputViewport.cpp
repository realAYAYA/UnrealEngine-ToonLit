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


TSharedPtr<FPixelStreamingVideoInputViewport> FPixelStreamingVideoInputViewport::Create()
{
	TSharedPtr<FPixelStreamingVideoInputViewport> NewInput = TSharedPtr<FPixelStreamingVideoInputViewport>(new FPixelStreamingVideoInputViewport());
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

void FPixelStreamingVideoInputViewport::OnViewportRendered(FViewport* InViewport)
{
	IPixelStreamingModule& Module = IPixelStreamingModule::Get();
	TSharedPtr<IPixelStreamingStreamer> Streamer = Module.GetStreamer(Module.GetDefaultStreamerID());
	if (!Streamer.IsValid() || !Streamer->IsStreaming())
	{
		return;
	}

	TSharedPtr<SViewport> TargetScene = Streamer->GetTargetViewport().Pin();
	if (!TargetScene.IsValid())
	{
		return;
	}

	if (InViewport == nullptr)
	{
		return;
	}

	if (InViewport->GetViewportType() != TargetViewportType)
	{
		return;
	}

	// Bit dirty to do a static cast here, but we check viewport type just above so it is somewhat "safe".
	TSharedPtr<SViewport> InScene = static_cast<FSceneViewport*>(InViewport)->GetViewportWidget().Pin();

	// If the viewport we were passed is not our target viewport we are not interested in getting its texture.
	if (TargetScene != InScene)
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