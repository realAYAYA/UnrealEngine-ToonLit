// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HLMediaPrivate.h"

#include "BaseMediaSource.h"

#include "HLMediaSource.generated.h"

UCLASS(BlueprintType)
class HLMEDIA_API UHLMediaSource
	: public UBaseMediaSource
{
	GENERATED_BODY()

public:
	/** Default constructor. */
	UHLMediaSource();

	/** The URL property is an Adaptive Streaming playlist. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Stream, AdvancedDisplay)
	bool IsAdaptiveSource;

	/** The URL to the media stream to be played. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Stream, AssetRegistrySearchable)
	FString StreamUrl;

	/**  IMediaOptions */
	virtual FName GetDesiredPlayerName() const override;
	virtual bool GetMediaOption(const FName& Key, bool DefaultValue) const override;
	virtual bool HasMediaOption(const FName& Key) const override;

public:

	//~ UMediaSource interface
	virtual FString GetUrl() const override;
	virtual bool Validate() const override;
};
