// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"

#include "MediaPlayerOptions.generated.h"

UENUM(BlueprintType)
enum class EMediaPlayerOptionBooleanOverride : uint8
{
	UseMediaPlayerSetting,
	Enabled,
	Disabled
};

USTRUCT(BlueprintType)
struct FMediaPlayerTrackOptions
{
	GENERATED_BODY()

	FMediaPlayerTrackOptions() :
		Audio(0),
		Caption(-1),
		Metadata(-1),
		Script(-1),
		Subtitle(-1),
		Text(-1),
		Video(0)
	{
	}

	/** Check if this Media Player Track Options is not equal to another one. */
	bool operator!=(const FMediaPlayerTrackOptions& Other) const
	{
		return (Audio != Other.Audio
			|| Caption != Other.Caption
			|| Metadata != Other.Metadata
			|| Script != Other.Script
			|| Subtitle != Other.Subtitle
			|| Text != Other.Text
			|| Video != Other.Video);
	}

	UPROPERTY(BlueprintReadWrite, Category = "Tracks")
	int32 Audio;

	UPROPERTY(BlueprintReadWrite, Category = "Tracks")
	int32 Caption;

	UPROPERTY(BlueprintReadWrite, Category = "Tracks")
	int32 Metadata;

	UPROPERTY(BlueprintReadWrite, Category = "Tracks")
	int32 Script;

	UPROPERTY(BlueprintReadWrite, Category = "Tracks")
	int32 Subtitle;

	UPROPERTY(BlueprintReadWrite, Category = "Tracks")
	int32 Text;

	UPROPERTY(BlueprintReadWrite, Category = "Tracks")
	int32 Video;
};

USTRUCT(BlueprintType)
struct FMediaPlayerOptions
{
	GENERATED_BODY()

	FMediaPlayerOptions() :
		SeekTime(0),
		PlayOnOpen(EMediaPlayerOptionBooleanOverride::UseMediaPlayerSetting),
		Loop(EMediaPlayerOptionBooleanOverride::UseMediaPlayerSetting)
	{
	}

	/** Check if this Media Options is not equal to another one. */
	bool operator!=(const FMediaPlayerOptions& Other) const
	{
		return (Tracks != Other.Tracks
			|| SeekTime != Other.SeekTime
			|| PlayOnOpen != Other.PlayOnOpen
			|| Loop != Other.Loop);
	}

	UPROPERTY(BlueprintReadWrite, Category = "Tracks")
	FMediaPlayerTrackOptions Tracks;

	UPROPERTY(BlueprintReadWrite, Category = "Misc")
	FTimespan SeekTime;

	UPROPERTY(BlueprintReadWrite, Category = "Misc")
	EMediaPlayerOptionBooleanOverride PlayOnOpen;

	UPROPERTY(BlueprintReadWrite, Category = "Misc")
	EMediaPlayerOptionBooleanOverride Loop;
};
