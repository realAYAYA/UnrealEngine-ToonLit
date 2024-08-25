// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "HarmonixDsp/PannerDetails.h"
#include "HarmonixDsp/Streaming/StandardStream.h"

#include "TrackChannelInfo.generated.h"

USTRUCT(BlueprintType)
struct HARMONIXDSP_API FStreamingChannelParamsArray
{
	GENERATED_BODY()

public:

	UPROPERTY(EditDefaultsOnly, Category = "Settings")
	TArray<FStreamingChannelParams> ChannelParams;

	FStreamingChannelParamsArray() {};
	FStreamingChannelParamsArray(const TArray<FStreamingChannelParams>& InChannelParams)
		: ChannelParams(InChannelParams)
	{
	}

	FStreamingChannelParams& operator[](int32 Index) { return ChannelParams[Index]; }
	const FStreamingChannelParams& operator[](int32 Index) const { return ChannelParams[Index]; }
};

USTRUCT(BlueprintType)
struct HARMONIXDSP_API FTrackChannelInfo
{
	GENERATED_BODY()

public:

	UPROPERTY(EditDefaultsOnly, Category = "Settings", Meta = (UIMin = -1, ClampMin = -1))
	int32 RealTrackIndex = -1;

	UPROPERTY(EditDefaultsOnly, Category = "Settings")
	FName Name;

	UPROPERTY(EditDefaultsOnly, Category = "Settings")
	FName Routing;

	UPROPERTY(EditDefaultsOnly, Category = "Settings")
	TArray<FStreamingChannelParams> Channels;

	UPROPERTY(EditDefaultsOnly, Category = "Settings")
	TMap<FName, FStreamingChannelParamsArray> PresetChannels;

	FTrackChannelInfo();
	FTrackChannelInfo(int32 InRealIndex);
	FTrackChannelInfo(FTrackChannelInfo&& Other);
	FTrackChannelInfo(const FTrackChannelInfo& Other);

	FTrackChannelInfo& operator=(const FTrackChannelInfo& Other)
	{
		RealTrackIndex = Other.RealTrackIndex;
		Name = Other.Name;
		Routing = Other.Routing;
		Channels = Other.Channels;
		PresetChannels = Other.PresetChannels;
		return *this;
	}

	int32  GetNumChannels() const;
	int32  GetStreamChannelIndex(int32 ChannelIndex) const;
	bool GetStreamIndexesPan(int32 StreamIndex, FPannerDetails& OutPan) const;
	bool GetStreamIndexesGain(int32 StreamIndex, float& OutGain) const;
	bool GetStreamIndexesPresetGain(FName PresetName, int32 StreamIndex, float& OutGain, bool DefaultIfNotFound = true) const;
	bool SetStreamIndexesPan(int32 StreamIndex, float Pan);
	bool SetStreamIndexesPan(int32 StreamIndex, const FPannerDetails& InPan);
	bool SetStreamIndexesGain(int32 StreamIndex, float InGain);
	bool SetStreamIndexesPresetGain(FName presetName, int32 StreamIndex, float InGain);

private:

	const FStreamingChannelParams* FindStreamingChannelParams(int32 StreamIndex) const;
	FStreamingChannelParams* FindStreamingChannelParams(int32 StreamIndex);
};