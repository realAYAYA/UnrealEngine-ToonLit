// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMediaPlayerEditorOutput.h"

#include "AudioDevice.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "IMediaEventSink.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "MediaPlayer.h"
#include "MediaSoundComponent.h"
#include "MediaTexture.h"
#include "Styling/SlateBrush.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SMediaImage.h"


/* SMediaPlayerEditorOutput structors
 *****************************************************************************/

SMediaPlayerEditorOutput::SMediaPlayerEditorOutput()
	: MediaPlayer(nullptr)
	, MediaTexture(nullptr)
	, SoundComponent(nullptr)
	, bIsOurMediaTexture(false)
{ }


SMediaPlayerEditorOutput::~SMediaPlayerEditorOutput()
{
	if (MediaPlayer.IsValid())
	{
		MediaPlayer->OnMediaEvent().RemoveAll(this);
		MediaPlayer.Reset();
	}

	// Did we create the media texture?
	if (bIsOurMediaTexture)
	{
		bIsOurMediaTexture = false;
		if (MediaTexture != nullptr)
		{
			MediaTexture->RemoveFromRoot();
			MediaTexture = nullptr;
		}
	}

	if (SoundComponent != nullptr)
	{
		SoundComponent->Stop();
		SoundComponent->RemoveFromRoot();
		SoundComponent->SetMediaPlayer(nullptr);
		SoundComponent->UpdatePlayer();
		SoundComponent = nullptr;
	}
}


/* SMediaPlayerEditorOutput interface
 *****************************************************************************/

void SMediaPlayerEditorOutput::Construct(const FArguments& InArgs, UMediaPlayer& InMediaPlayer,
	UMediaTexture* InMediaTexture, bool bInIsSoundEnabled)
{
	MediaPlayer = &InMediaPlayer;

	// create media sound component
	if ((GEngine != nullptr) && GEngine->UseSound() && (bInIsSoundEnabled))
	{
		SoundComponent = NewObject<UMediaSoundComponent>(GetTransientPackage(), NAME_None, RF_Transient | RF_Public);

		if (SoundComponent != nullptr)
		{
			SoundComponent->bIsUISound = true;
			SoundComponent->bIsPreviewSound = true;
			SoundComponent->SetMediaPlayer(&InMediaPlayer);
			SoundComponent->Initialize();
			SoundComponent->AddToRoot();
		}
	}

	// Did we get media texture passed in?
	if (InMediaTexture != nullptr)
	{
		MediaTexture = InMediaTexture;
		bIsOurMediaTexture = false;
	}
	else
	{
		// create media texture
		MediaTexture = NewObject<UMediaTexture>(GetTransientPackage(), NAME_None, RF_Transient | RF_Public);
		bIsOurMediaTexture = true;

		if (MediaTexture != nullptr)
		{
			MediaTexture->AutoClear = true;
			MediaTexture->SetMediaPlayer(&InMediaPlayer);
			MediaTexture->UpdateResource();
			MediaTexture->AddToRoot();
		}
	}

	TSharedRef<SMediaImage> MediaImage = SNew(SMediaImage, MediaTexture)
		.BrushImageSize_Lambda([&]()
			{
				if (MediaTexture)
					return FVector2D(MediaTexture->GetSurfaceWidth(), MediaTexture->GetSurfaceHeight());
				else
					return FVector2D::ZeroVector;
			});

	ChildSlot
	[
		MediaImage
	];

	MediaPlayer->OnMediaEvent().AddRaw(this, &SMediaPlayerEditorOutput::HandleMediaPlayerMediaEvent);
}


/* SWidget interface
 *****************************************************************************/

void SMediaPlayerEditorOutput::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	if (SoundComponent != nullptr)
	{
		SoundComponent->UpdatePlayer();
	}
}


/* SMediaPlayerEditorOutput callbacks
 *****************************************************************************/

void SMediaPlayerEditorOutput::HandleMediaPlayerMediaEvent(EMediaEvent Event)
{
	if (SoundComponent == nullptr)
	{
		return;
	}

	if (Event == EMediaEvent::PlaybackSuspended)
	{
		SoundComponent->Stop();
	}
	else if (Event == EMediaEvent::PlaybackResumed)
	{
		if (GEditor->PlayWorld == nullptr)
		{
			SoundComponent->Start();
		}
	}
}
