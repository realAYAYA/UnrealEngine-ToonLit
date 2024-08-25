// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DisplayClusterMediaBase.h"
#include "IMediaEventSink.h"
#include "UObject/GCObject.h"

class FRHICommandListImmediate;

class UMediaSource;
class UMediaPlayer;
class UMediaTexture;


/**
 * Base media input class
 */
class FDisplayClusterMediaInputBase
	: public FDisplayClusterMediaBase
	, public FGCObject
{
public:
	FDisplayClusterMediaInputBase(const FString& MediaId, const FString& ClusterNodeId, UMediaSource* MediaSource);

public:
	//~ Begin FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("FDisplayClusterMediaInputBase");
	}
	//~ End FGCObject interface

public:
	virtual bool Play();
	virtual void Stop();

public:
	UMediaSource* GetMediaSource() const
	{
		return MediaSource;
	}

	UMediaPlayer* GetMediaPlayer() const
	{
		return MediaPlayer;
	}

	UMediaTexture* GetMediaTexture() const
	{
		return MediaTexture;
	}

protected:
	void ImportMediaData(FRHICommandListImmediate& RHICmdList, const FMediaTextureInfo& TextureInfo);

private:
	void OnMediaEvent(EMediaEvent MediaEvent);
	bool StartPlayer();
	void OnPlayerClosed();

	void OverrideTextureRegions_RenderThread(FRHITexture* const SrcTexture, FRHITexture* const DstTexture, FIntRect& InOutSrcRect, FIntRect& InOutDstRect) const;

private:
	//~ Begin GC by AddReferencedObjects
	TObjectPtr<UMediaSource>  MediaSource = nullptr;
	TObjectPtr<UMediaPlayer>  MediaPlayer = nullptr;
	TObjectPtr<UMediaTexture> MediaTexture = nullptr;
	//~ End GC by AddReferencedObjects

	// Used to restart media player in the case it falls in error
	bool bWasPlayerStarted = false;

	// Used to control the rate at which we try to restart the player
	double LastRestartTimestamp = 0;

	// [Temp workaround] Whether current media is Rivermax
	bool bRunningRivermaxMedia = false;
};
