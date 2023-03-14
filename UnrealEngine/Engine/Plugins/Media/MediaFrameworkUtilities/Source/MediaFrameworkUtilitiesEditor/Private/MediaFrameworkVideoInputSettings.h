// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MediaBundle.h"
#include "MediaSource.h"
#include "MediaTexture.h"
#include "UObject/SoftObjectPtr.h"

#include "MediaFrameworkVideoInputSettings.generated.h"

USTRUCT()
struct FMediaFrameworkVideoInputSourceSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Media")
	TSoftObjectPtr<UMediaSource> MediaSource;

	UPROPERTY(EditAnywhere, Category="Media")
	TSoftObjectPtr<UMediaTexture> MediaTexture;
};

/**
 * Settings for the video input tab.
 */
UCLASS(MinimalAPI, config = EditorPerProjectUserSettings)
class UMediaFrameworkVideoInputSettings : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY(config, EditAnywhere, Category="Media Bundle")
	TArray<TSoftObjectPtr<UMediaBundle>> MediaBundles;

	UPROPERTY(config, EditAnywhere, Category="Media Source")
	TArray<FMediaFrameworkVideoInputSourceSettings> MediaSources;

	UPROPERTY(config)
	bool bReopenMediaBundles;

	UPROPERTY(config)
	bool bReopenMediaSources;

	UPROPERTY(config)
	float ReopenDelay = 3.f;

	UPROPERTY(config)
	bool bIsVerticalSplitterOrientation = true;
};
