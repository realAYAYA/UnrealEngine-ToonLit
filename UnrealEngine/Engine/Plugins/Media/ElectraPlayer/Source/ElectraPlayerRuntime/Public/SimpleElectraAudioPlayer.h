// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Containers/Map.h"
#include "Misc/Variant.h"
#include "Misc/Timespan.h"

class IElectraPlayerDataCache;
class IAnalyticsProviderET;

class ELECTRAPLAYERRUNTIME_API ISimpleElectraAudioPlayer
{
protected:
	virtual ~ISimpleElectraAudioPlayer() = default;
public:
	struct FCreateParams
	{
		/** A GUID to identify the new player instance. */
		FGuid InstanceGUID;
	};
	static ISimpleElectraAudioPlayer* Create(const FCreateParams& InCreateParams);
	static void CloseAndDestroy(ISimpleElectraAudioPlayer* InInstance);

	/**
	 * Delivers the aggregated metrics to the analytics provider and clears the internal list.
	 */
	static void SendAnalyticMetrics(const TSharedPtr<IAnalyticsProviderET>& InAnalyticsProvider);


	class ICacheElementBase : public TSharedFromThis<ICacheElementBase, ESPMode::ThreadSafe>
	{
	public:
		virtual ~ICacheElementBase() {};
	};
	static TSharedPtr<ICacheElementBase, ESPMode::ThreadSafe> GetCustomCacheElement(const FString& InForURL);
	static void SetCustomCacheElement(const FString& InForURL, const TSharedPtr<ICacheElementBase, ESPMode::ThreadSafe>& InElement);


	virtual bool Open(const TMap<FString, FVariant>& InOptions, const FString& ManifestURL, const FTimespan& StartPosition, const FTimespan& EncodedDuration, bool bAutoPlay, bool bSetLooping, TSharedPtr<IElectraPlayerDataCache, ESPMode::ThreadSafe> InPlayerDataCache) = 0;
	virtual void SeekTo(const FTimespan& NewPosition) = 0;
	virtual void PrepareToLoopToBeginning() = 0;
	virtual void Pause() = 0;
	virtual void Resume() = 0;
	virtual void Stop() = 0;

	virtual bool HasErrored() const = 0;
	virtual FString GetError() const = 0;

	struct FStreamFormat
	{
		int32 SampleRate = 0;
		int32 NumChannels = 0;
	};

	struct FDefaultSampleInfo
	{
		int64 NumTotalFrames = -1;
		int64 ExpectedCurrentFramePos = -1;
		int32 SampleRate = 0;
		int32 NumChannels = 0;
	};

	// Negative return values indicating reason for having no samples in GetNextSamples()
	enum
	{
		GetSamples_NotReady = -1,		// Player not ready, no samples available.
		GetSamples_AtEOS = -2,			// Reached end of stream
	};

	virtual int64 GetNextSamples(FTimespan& OutPTS, bool& bOutIsFirstBlock, int16* OutBuffer, int32 InBufferSizeInFrames, int32 InNumFramesToGet, const FDefaultSampleInfo& InDefaultSampleInfo) = 0;


	//-------------------------------------------------------------------------
	// State functions
	//
	virtual bool HaveMetadata() const = 0;
	virtual int32 GetBinaryMetadata(TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe>& OutMetadata) const = 0;
	virtual bool GetStreamFormat(FStreamFormat& OutFormat) const = 0;
	virtual FTimespan GetDuration() const = 0;
	virtual FTimespan GetPlayPosition() const = 0;
	virtual bool HasEnded() const = 0;
	virtual bool IsBuffering() const = 0;
	virtual bool IsSeeking() const = 0;
	virtual bool IsPlaying() const = 0;
	virtual bool IsPaused() const = 0;

	//-------------------------------------------------------------------------
	// Manual stream selection functions
	//

	//! Sets the highest bitrate when selecting a candidate stream.
	virtual void SetBitrateCeiling(int32 HighestSelectableBitrate) = 0;
};
