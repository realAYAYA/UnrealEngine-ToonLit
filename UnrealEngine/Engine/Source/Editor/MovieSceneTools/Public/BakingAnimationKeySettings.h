// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "BakingAnimationKeySettings.generated.h"


UENUM(Blueprintable)
enum class EBakingKeySettings : uint8
{
	KeysOnly UMETA(DisplayName = "Keys Only"),
	AllFrames UMETA(DisplayName = "All Frames"),
};

USTRUCT(BlueprintType)
struct MOVIESCENETOOLS_API FBakingAnimationKeySettings
{
	GENERATED_BODY();

	FBakingAnimationKeySettings()
	{
		StartFrame = 0;
		EndFrame = 100;
		BakingKeySettings = EBakingKeySettings::KeysOnly;
		FrameIncrement = 1;
		bReduceKeys = false;
		Tolerance = 0.001f;
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bake")
	FFrameNumber StartFrame;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bake")
	FFrameNumber EndFrame;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bake")
	EBakingKeySettings BakingKeySettings;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bake", meta = (ClampMin = "1", UIMin = "1", EditCondition = "BakingKeySettings == EBakingKeySettings::AllFrames"))
	int32 FrameIncrement;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bake", meta = (EditCondition = "BakingKeySettings == EBakingKeySettings::AllFrames"))
	bool bReduceKeys;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bake", meta = (EditCondition = "BakingKeySettings == EBakingKeySettings::AllFrames || bReduceKeys"))
	float Tolerance;
};
