// Copyright Epic Games, Inc. All Rights Reserved.

#include "PixelStreamingMediaOutput.h"
#include "PixelStreamingMediaCapture.h"
#include "IPixelStreamingModule.h"
#include "PixelStreamingPlayerId.h"
#include "IPixelStreamingStreamer.h"
#include "CineCameraComponent.h"
#include "IPixelStreamingEditorModule.h"
#include "PixelStreamingEditorUtils.h"
#include "PixelStreamingVideoInputRHI.h"
#include "PixelStreamingUtils.h"

UPixelStreamingMediaOutput* UPixelStreamingMediaOutput::Create(UObject* Outer, const FString& StreamerId)
{
	IPixelStreamingModule& Module = FModuleManager::LoadModuleChecked<IPixelStreamingModule>("PixelStreaming");
	UPixelStreamingMediaOutput* MediaOutput = NewObject<UPixelStreamingMediaOutput>(Outer, UPixelStreamingMediaOutput::StaticClass());
	MediaOutput->Streamer = Module.CreateStreamer(StreamerId);
	MediaOutput->RegisterRemoteResolutionCommandHandler();
	return MediaOutput;
}

void UPixelStreamingMediaOutput::BeginDestroy()
{
	StopStreaming();
	Streamer = nullptr;
	Super::BeginDestroy();
}

UMediaCapture* UPixelStreamingMediaOutput::CreateMediaCaptureImpl()
{
	Capture = nullptr;
	Capture = NewObject<UPixelStreamingMediaCapture>();
	Capture->SetMediaOutput(this);

	if (!VideoInput)
	{
		VideoInput = FPixelStreamingVideoInputVCam::Create();
	}

	Capture->SetVideoInput(VideoInput);

	return Capture;
}

void UPixelStreamingMediaOutput::RegisterRemoteResolutionCommandHandler()
{
	if (Streamer)
	{
		// Override resolution command as we this to set the output provider override resolution
		TSharedPtr<IPixelStreamingInputHandler> InputHandler = Streamer->GetInputHandler().Pin();
		if (InputHandler)
		{
			InputHandler->SetCommandHandler(TEXT("Resolution.Width"), [this](FString Descriptor, FString WidthString) {
				bool bSuccess = false;
				FString HeightString;
				UE::PixelStreaming::ExtractJsonFromDescriptor(Descriptor, TEXT("Resolution.Height"), HeightString, bSuccess);
				if (bSuccess)
				{
					const int Width = FCString::Atoi(*WidthString);
					const int Height = FCString::Atoi(*HeightString);
					if (Width > 0 && Height > 0)
					{
						RemoteResolutionChangedEvent.Broadcast(FIntPoint(Width, Height));
					}
				}
			});
		}
	}
}

void UPixelStreamingMediaOutput::StartStreaming()
{
	if (Streamer)
	{
		const FString SignallingDomain = IPixelStreamingEditorModule::Get().GetSignallingDomain();
		const int32 StreamerPort = IPixelStreamingEditorModule::Get().GetStreamerPort();
		const FString SignallingServerURL = FString::Printf(TEXT("%s:%s"), *SignallingDomain, *FString::FromInt(StreamerPort));
		Streamer->SetSignallingServerURL(SignallingServerURL);

		// Only update streamer's video input if we don't have one or it is different than the one we already have.
		if (VideoInput.IsValid())
		{
			TSharedPtr<FPixelStreamingVideoInput> StreamerVideoInput = Streamer->GetVideoInput().Pin();
			if (!StreamerVideoInput.IsValid() || StreamerVideoInput != VideoInput)
			{
				Streamer->SetVideoInput(VideoInput);
			}
		}

		if (!Streamer->IsStreaming())
		{
			Streamer->StartStreaming();
		}
	}
}

void UPixelStreamingMediaOutput::StopStreaming()
{
	if (Streamer)
	{
		Streamer->StopStreaming();
		Streamer->SetTargetWindow(nullptr);
	}
}
