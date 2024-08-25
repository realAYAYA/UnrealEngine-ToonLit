// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ILiveLinkSource.h"
#include "LiveLinkSourceSettings.h"

#include "LiveLinkPlaybackSource.generated.h"

class ILiveLinkClient;

/**
 * Completely empty "source" displayed in the UI when playing back a recording.
 */
class FLiveLinkPlaybackSource : public ILiveLinkSource
{
public:
	FLiveLinkPlaybackSource() = default;
	virtual ~FLiveLinkPlaybackSource() = default;

	//~ Begin ILiveLinkSource interface
	virtual bool CanBeDisplayedInUI() const override
	{
		return true;
	}
	
	virtual void ReceiveClient(ILiveLinkClient* InClient, FGuid InSourceGuid) override
	{
	}
	
	virtual void InitializeSettings(ULiveLinkSourceSettings* Settings) override
	{
		SourceName = TEXT("Playback source.");
	}
	
	virtual bool IsSourceStillValid() const override
	{
		return true; 
	}
	
	virtual bool RequestSourceShutdown() override
	{
		return true; 
	}

	virtual FText GetSourceType() const override
	{
		return FText::FromName(SourceName);
	}
	
	virtual FText GetSourceMachineName() const override
	{
		return FText();
	}
	
	virtual FText GetSourceStatus() const override
	{
		return NSLOCTEXT("LiveLinkPlaybackSource", "PlaybackSourceStatus", "Playback");
	}
	//~ End ILiveLinkSource interface

protected:
	/** Source name. */
	FName SourceName;
};

/** PlaybackSourceSettings to be able to differentiate from live sources and keep a name associated to the source */
UCLASS()
class ULiveLinkPlaybackSourceSettings : public ULiveLinkSourceSettings
{
	GENERATED_BODY()

public:

	/** Source name. */
	UPROPERTY()
	FName SourceName;
};
