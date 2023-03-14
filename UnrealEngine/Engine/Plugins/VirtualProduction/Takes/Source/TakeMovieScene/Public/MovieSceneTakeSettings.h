// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MovieSceneTakeSettings.generated.h"

/**
 * Universal take recorder settings that apply to a whole take
 */
UCLASS(config=EditorSettings, MinimalAPI)
class UMovieSceneTakeSettings : public UObject
{
public:
	GENERATED_BODY()

	UMovieSceneTakeSettings();

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	/** Hours Name */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Take Recorder")
	FString HoursName;

	/** Minutes Name */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Take Recorder")
	FString MinutesName;

	/** Seconds Name */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Take Recorder")
	FString SecondsName;

	/** Frames Name */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Take Recorder")
	FString FramesName;

	/** SubFrames Name */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Take Recorder")
	FString SubFramesName;

	/** Slate Name */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Take Recorder")
	FString SlateName;
};
