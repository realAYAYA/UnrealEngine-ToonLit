// Copyright Epic Games, Inc. All Rights Reserved.

#include "Input/DisplayClusterMediaInputBase.h"

#include "DisplayClusterMediaHelpers.h"
#include "DisplayClusterMediaLog.h"

#include "MediaPlayer.h"
#include "MediaSource.h"
#include "MediaTexture.h"


FDisplayClusterMediaInputBase::FDisplayClusterMediaInputBase(const FString& InMediaId, const FString& InClusterNodeId, UMediaSource* InMediaSource)
	: FDisplayClusterMediaBase(InMediaId, InClusterNodeId)
	, MediaSource(InMediaSource)
{
	checkSlow(MediaSource);

	// Instantiate media player
	MediaPlayer = NewObject<UMediaPlayer>();
	if (MediaPlayer)
	{
		MediaPlayer->SetLooping(false);
		MediaPlayer->PlayOnOpen = false;

		// Instantiate media texture
		MediaTexture = NewObject<UMediaTexture>();
		if (MediaTexture)
		{
			MediaTexture->NewStyleOutput = true;
			MediaTexture->SetMediaPlayer(MediaPlayer);
			MediaTexture->UpdateResource();
		}
	}
}


void FDisplayClusterMediaInputBase::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(MediaSource);
	Collector.AddReferencedObject(MediaPlayer);
	Collector.AddReferencedObject(MediaTexture);
}

bool FDisplayClusterMediaInputBase::Play()
{
	if (MediaSource && MediaPlayer && MediaTexture)
	{
		MediaPlayer->PlayOnOpen = true;
		MediaPlayer->OnMediaEvent().AddRaw(this, &FDisplayClusterMediaInputBase::OnMediaEvent);

		bWasPlayerStarted = MediaPlayer->OpenSource(MediaSource);
		
		return bWasPlayerStarted;
	}

	return false;
}

void FDisplayClusterMediaInputBase::Stop()
{
	if (MediaPlayer)
	{
		bWasPlayerStarted = false;
		MediaPlayer->Close();
		MediaPlayer->OnMediaEvent().RemoveAll(this);
	}
}

void FDisplayClusterMediaInputBase::ImportMediaData(FRHICommandListImmediate& RHICmdList, const FMediaTextureInfo& TextureInfo)
{
	FRHITexture* const SrcTexture = MediaTexture->GetResource()->GetTextureRHI();
	FRHITexture* const DstTexture = TextureInfo.Texture;

	if (SrcTexture && DstTexture)
	{
		const FIntPoint DstRegionSize = TextureInfo.Region.Size();

		if (SrcTexture->GetDesc().Format == DstTexture->GetDesc().Format &&
			SrcTexture->GetDesc().Extent == DstRegionSize)
		{
			FRHICopyTextureInfo CopyInfo;

			CopyInfo.DestPosition = FIntVector(TextureInfo.Region.Min.X, TextureInfo.Region.Min.Y, 0);
			CopyInfo.Size = FIntVector(DstRegionSize.X, DstRegionSize.Y, 0);

			TransitionAndCopyTexture(RHICmdList, SrcTexture, DstTexture, CopyInfo);
		}
		else
		{
			DisplayClusterMediaHelpers::ResampleTexture_RenderThread(
				RHICmdList, SrcTexture, DstTexture,
				FIntRect(FIntPoint(0, 0), SrcTexture->GetDesc().Extent),
				TextureInfo.Region);
		}
	}
}

void FDisplayClusterMediaInputBase::OnMediaEvent(EMediaEvent MediaEvent)
{
	switch (MediaEvent)
	{
	/** The player started connecting to the media source. */
	case EMediaEvent::MediaConnecting:
		UE_LOG(LogDisplayClusterMedia, Log, TEXT("Media event for '%s': Connection"), *GetMediaId());
		break;

	/** A new media source has been opened. */
	case EMediaEvent::MediaOpened:
		UE_LOG(LogDisplayClusterMedia, Log, TEXT("Media event for '%s': Opened"), *GetMediaId());
		break;

	/** The current media source has been closed. */
	case EMediaEvent::MediaClosed:
		UE_LOG(LogDisplayClusterMedia, Log, TEXT("Media event for '%s': Closed"), *GetMediaId());
		OnPlayerClosed();
		break;
		
	/** A media source failed to open. */
	case EMediaEvent::MediaOpenFailed:
		UE_LOG(LogDisplayClusterMedia, Log, TEXT("Media event for '%s': OpenFailed"), *GetMediaId());
		break;

	default:
		UE_LOG(LogDisplayClusterMedia, Log, TEXT("Media event for '%s': %d"), *GetMediaId(), static_cast<int32>(MediaEvent));
		break;
	}
}

bool FDisplayClusterMediaInputBase::StartPlayer()
{
	const bool bIsPlaying = MediaPlayer->OpenSource(MediaSource);
	if (bIsPlaying)
	{
		UE_LOG(LogDisplayClusterMedia, Log, TEXT("Started playing media: %s"), *GetMediaId());
	}
	else
	{
		UE_LOG(LogDisplayClusterMedia, Warning, TEXT("Couldn't start playing media: %s"), *GetMediaId());
	}

	return bIsPlaying;
}

void FDisplayClusterMediaInputBase::OnPlayerClosed()
{
	if (MediaPlayer && bWasPlayerStarted)
	{
		constexpr double Interval = 1.0;
		const double CurrentTime = FPlatformTime::Seconds();
		if (CurrentTime - LastRestartTimestamp > Interval)
		{
			UE_LOG(LogDisplayClusterMedia, Log, TEXT("MediaPlayer '%s' is in error, restarting it."), *GetMediaId());

			StartPlayer();
			LastRestartTimestamp = CurrentTime;
		}
	}
}
