// Copyright Epic Games, Inc. All Rights Reserved.

#include "PixelStreamingStreamerComponent.h"
#include "IPixelStreamingModule.h"
#include "PixelStreamingVideoInputBackBuffer.h"
#include "Engine/GameEngine.h"
#include "Slate/SceneViewport.h"

UPixelStreamingStreamerComponent::UPixelStreamingStreamerComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	FCoreDelegates::OnFEngineLoopInitComplete.AddLambda([this]() {
		OnPostEngineInit();
	});
}

FString UPixelStreamingStreamerComponent::GetId()
{
	return Streamer->GetId();
}

bool UPixelStreamingStreamerComponent::IsSignallingConnected()
{
	return Streamer->IsSignallingConnected();
}

void UPixelStreamingStreamerComponent::StartStreaming()
{
	if (!Streamer)
	{
		CreateStreamer();
	}

	if (StreamerInput && StreamerInput->GetVideoInput())
	{
		Streamer->SetVideoInput(StreamerInput->GetVideoInput());
	}

	Streamer->SetCoupleFramerate(CoupleFramerate);
	Streamer->SetStreamFPS(StreamFPS);
	Streamer->SetSignallingServerURL(SignallingServerURL);
	Streamer->StartStreaming();
}

void UPixelStreamingStreamerComponent::StopStreaming()
{
	Streamer->StopStreaming();
}

bool UPixelStreamingStreamerComponent::IsStreaming()
{
	return Streamer->IsStreaming();
}

void UPixelStreamingStreamerComponent::ForceKeyFrame()
{
	Streamer->ForceKeyFrame();
}

void UPixelStreamingStreamerComponent::FreezeStream(UTexture2D* Texture)
{
	Streamer->FreezeStream(Texture);
}

void UPixelStreamingStreamerComponent::UnfreezeStream()
{
	Streamer->UnfreezeStream();
}

void UPixelStreamingStreamerComponent::SendPlayerMessage(uint8 Type, const FString& Descriptor)
{
	Streamer->SendPlayerMessage(Type, Descriptor);
}

void UPixelStreamingStreamerComponent::OnPostEngineInit()
{
	if (Streamer)
	{
		SetupStreamerInput();
	}
	bEngineStarted = true;
}

void UPixelStreamingStreamerComponent::CreateStreamer()
{
	Streamer = IPixelStreamingModule::Get().CreateStreamer(StreamerId);
	Streamer->OnStreamingStarted().AddUObject(this, &UPixelStreamingStreamerComponent::StreamingStarted);
	Streamer->OnStreamingStopped().AddUObject(this, &UPixelStreamingStreamerComponent::StreamingStopped);
	Streamer->OnInputReceived.AddUObject(this, &UPixelStreamingStreamerComponent::StreamingInput);

	if (bEngineStarted)
	{
		SetupStreamerInput();
	}
}

void UPixelStreamingStreamerComponent::StreamingStarted(IPixelStreamingStreamer*)
{
	OnStreamingStarted.Broadcast();
}

void UPixelStreamingStreamerComponent::StreamingStopped(IPixelStreamingStreamer*)
{
	OnStreamingStopped.Broadcast();
}

void UPixelStreamingStreamerComponent::StreamingInput(FPixelStreamingPlayerId PlayerId, uint8 Type, TArray<uint8> Data)
{
	OnInputReceived.Broadcast(PlayerId, Type, Data);
}

void UPixelStreamingStreamerComponent::SetupStreamerInput()
{
	if (!GIsEditor)
	{
		// default to the scene viewport if we have a game engine
		if (UGameEngine* GameEngine = Cast<UGameEngine>(GEngine))
		{
			TSharedPtr<FSceneViewport> TargetViewport = GameEngine->SceneViewport;
			if (TargetViewport.IsValid())
			{
				Streamer->SetTargetViewport(TargetViewport->GetViewportWidget());
				Streamer->SetTargetWindow(TargetViewport->FindWindow());
			}
		}
	}
}