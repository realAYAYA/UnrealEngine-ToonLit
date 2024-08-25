// Copyright Epic Games, Inc. All Rights Reserved.

#include "MediaSourceRenderer.h"

#include "MediaPlayer.h"
#include "MediaSource.h"
#include "MediaTexture.h"
#include "UObject/Package.h"

UMediaTexture* UMediaSourceRenderer::Open(UMediaSource* InMediaSource)
{
	if ((InMediaSource != nullptr) && (InMediaSource->Validate()))
	{
		// Set up the player.
		if (MediaPlayer == nullptr)
		{
			MediaPlayer = NewObject<UMediaPlayer>(GetTransientPackage());
			MediaPlayer->OnSeekCompleted.AddDynamic(this, &UMediaSourceRenderer::OnSeekCompleted);
		}
		else
		{
			MediaPlayer->Close();
		}

		// Set up the texture.
		if (MediaTexture == nullptr)
		{
			MediaTexture = NewObject<UMediaTexture>(GetTransientPackage());
			MediaTexture->NewStyleOutput = true;
		}
		MediaTexture->CurrentAspectRatio = 0.0f;
		MediaTexture->SetMediaPlayer(MediaPlayer.Get());
		MediaTexture->UpdateResource();
		MediaSource = InMediaSource;

		// For image media, we avoid filling the global cache which will needlessly hold onto frame data.
		FMediaSourceCacheSettings OriginalCacheSettings;
		MediaSource->GetCacheSettings(OriginalCacheSettings);
		MediaSource->SetCacheSettings(FMediaSourceCacheSettings{ true, 0.2f });

		// Start playing the media.
		bIsSeekActive = false;
		FMediaPlayerOptions Options;
		Options.PlayOnOpen = EMediaPlayerOptionBooleanOverride::Enabled;
		Options.Loop = EMediaPlayerOptionBooleanOverride::Disabled;
		bool bIsPlaying = MediaPlayer->OpenSourceWithOptions(MediaSource, Options);
		if (bIsPlaying == false)
		{
			Close();
		}

		MediaSource->SetCacheSettings(OriginalCacheSettings);
	}

	return MediaTexture.Get();
}

void UMediaSourceRenderer::Tick(float DeltaTime)
{
	if (MediaPlayer != nullptr)
	{
		// Is the texture ready?
		// The aspect ratio will change when we have something.
		if (MediaTexture->CurrentAspectRatio != 0.0f)
		{
			// Send this event so the content browser can update our thumbnail.
			if (MediaSource != nullptr)
			{
				FPropertyChangedEvent EmptyPropertyChangedEvent(nullptr);
				FCoreUObjectDelegates::OnObjectPropertyChanged.Broadcast(MediaSource,
					EmptyPropertyChangedEvent);
			}
			Close();
		}
		else
		{
			if (MediaPlayer->GetUrl().IsEmpty())
			{
				Close();
			}
			else if (MediaPlayer->IsPreparing() == false)
			{
				// Don't advance playback.
				if (MediaPlayer->IsPlaying())
				{
					MediaPlayer->SetRate(0.0f);
				}

				// If we can seek, then seek.
				if ((MediaPlayer->IsBuffering() == false) && (MediaPlayer->IsReady()))
				{
					if (bIsSeekActive == false)
					{
						bIsSeekActive = true;
						const FTimespan MediaDuration = MediaPlayer->GetDuration();
						FTimespan SeekTime = MediaDuration * 0.3f;
						MediaPlayer->Seek(SeekTime);
					}
				}
			}
		}
	}
}

void UMediaSourceRenderer::OnSeekCompleted()
{
	bIsSeekActive = false;
}

void UMediaSourceRenderer::Close()
{
	if (MediaTexture != nullptr)
	{
		MediaTexture->SetMediaPlayer(nullptr);
	}
	if (MediaPlayer != nullptr)
	{
		MediaPlayer->Close();
		MediaPlayer = nullptr;
	}
}
