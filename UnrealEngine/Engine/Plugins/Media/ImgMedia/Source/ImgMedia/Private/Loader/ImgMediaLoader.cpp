// Copyright Epic Games, Inc. All Rights Reserved.

#include "Loader/ImgMediaLoader.h"
#include "ImgMediaGlobalCache.h"
#include "ImgMediaPrivate.h"

#include "Algo/Reverse.h"
#include "Containers/SortedMap.h"
#include "Misc/FrameRate.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "Misc/QueuedThreadPool.h"
#include "Misc/ScopeLock.h"
#include "Modules/ModuleManager.h"
#include "UObject/Class.h"
#include "UObject/UObjectGlobals.h"

#include "Readers/GenericImgMediaReader.h"
#include "IImageWrapperModule.h"
#include "IImgMediaModule.h"
#include "Readers/IImgMediaReader.h"
#include "ImgMediaLoaderWork.h"
#include "Scheduler/ImgMediaScheduler.h"
#include "Player/ImgMediaTextureSample.h"

#if IMGMEDIA_EXR_SUPPORTED_PLATFORM
	#include "Readers/ExrImgMediaReader.h"
#endif

/** Time spent loading a new image sequence. */
DECLARE_CYCLE_STAT(TEXT("ImgMedia Loader Load Sequence"), STAT_ImgMedia_LoaderLoadSequence, STATGROUP_Media);

/** Time spent releasing cache in image loader destructor. */
DECLARE_CYCLE_STAT(TEXT("ImgMedia Loader Release Cache"), STAT_ImgMedia_LoaderReleaseCache, STATGROUP_Media);

constexpr int32 FImgMediaLoader::MAX_MIPMAP_LEVELS;

static TAutoConsoleVariable<int32> CVarImgMediaFrameInvalidationMaxCount(
	TEXT("ImgMedia.FrameInvalidationMaxCount"),
	2,
	TEXT("Maximum number of cached frames that can be invalidated when missing the latest mips/tiles."));

namespace ImgMediaLoader
{
	void CheckAndUpdateImgDimensions(FIntPoint& InOutSequenceDim, const FIntPoint& InNewDim)
	{
		if (InOutSequenceDim != InNewDim)
		{
			// This means that image sequence has inconsistent dimension and user needs to be made aware.
			UE_LOG(LogImgMedia, Warning, TEXT("Image sequence has inconsistent dimensions. The original sequence dimension (%d%d) is changed to the new dimension (%d%d)."),
				InOutSequenceDim.X, InOutSequenceDim.Y, InNewDim.X, InNewDim.Y);

			InOutSequenceDim = InNewDim;
		}
	}

	// Check if [CurrentFrame] is contained in the [OffsetCount] number of frames after [OriginFrame], also taking into account looping and play direction.
	bool IsCachedFrameInRange(int32 CurrentFrame, int32 OriginFrame, int32 OffsetCount, int32 TotalNumFrames, float PlayRate)
	{
		if (OffsetCount <= 0)
		{
			return false;
		}

		ensure(CurrentFrame >= 0 && CurrentFrame < TotalNumFrames);
		ensure(OriginFrame >= 0 && OriginFrame < TotalNumFrames);

		const int32 PlayRateSign = (PlayRate >= 0.0f) ? 1 : -1;

		// We start one frame away, since it's generally to late to enqueue additional update work for the current cached frame.
		for (int32 Offset = 1; Offset <= OffsetCount; ++Offset)
		{
			int32 OffsetFrame = FMath::Wrap(OriginFrame + PlayRateSign * Offset, 0, TotalNumFrames - 1);
			if (OffsetFrame == CurrentFrame)
			{
				return true;
			}
		}

		return false;
	}
}

FImgMediaLoaderBandwidth::FImgMediaLoaderBandwidth()
	: Current(0)
	, Effective(0)
	, Required(0)
	, ReadTimeCache()
	, ReadTimeCacheIndex(0)
{

}

void FImgMediaLoaderBandwidth::Update(const TSharedPtr<FImgMediaFrame, ESPMode::ThreadSafe>& Frame, double WorkTime)
{
	static constexpr int32 READ_TIME_CACHE_MAX = 64;

	const FImgMediaFrameInfo FrameInfo = Frame->GetInfo();
	// Calculate the current uncompressed bandwidth
	SIZE_T BytesLoaded = FrameInfo.UncompressedSize;
	Current = (float)BytesLoaded / WorkTime;

	if (FrameInfo.bHasTiles)
	{
		int32 TotalNumTiles = 0;

		for (int32 MipLevel = 0; MipLevel < FrameInfo.NumMipLevels; ++MipLevel)
		{
			const int32 MipLevelDiv = 1 << MipLevel;
			int32 NumTilesX = FMath::Max(1, FMath::CeilToInt(float(FrameInfo.NumTiles.X) / MipLevelDiv));
			int32 NumTilesY = FMath::Max(1, FMath::CeilToInt(float(FrameInfo.NumTiles.Y) / MipLevelDiv));
			TotalNumTiles += NumTilesX * NumTilesY;
		}

		// Adjusted based on how many tiles were loaded
		Current *= (float)Frame->NumTilesRead / FMath::Max(1, TotalNumTiles);
	}

	// Store read times with their respectivate timestamps...
	if (ReadTimeCache.Num() == READ_TIME_CACHE_MAX)
	{
		ReadTimeCache[ReadTimeCacheIndex] = TPairInitializer(FPlatformTime::Seconds(), WorkTime);
		ReadTimeCacheIndex = (ReadTimeCacheIndex + 1) % READ_TIME_CACHE_MAX;
	}
	else
	{
		ReadTimeCache.Emplace(FPlatformTime::Seconds(), WorkTime);
	}

	// Calculate the effective/utilized bandwidth as the fraction of the elapsed time spent reading.
	double IntervalStart = TNumericLimits<double>::Max();
	double IntervalEnd = TNumericLimits<double>::Lowest();
	double TotalReadTime = 0.0;

	for (const TPair<double, double>& Pair : ReadTimeCache)
	{
		double CachedTimestamp = Pair.Key;
		double CachedReadTime = Pair.Value;
		IntervalStart = FMath::Min(IntervalStart, CachedTimestamp - CachedReadTime);
		IntervalEnd = FMath::Max(IntervalEnd, CachedTimestamp);
		TotalReadTime += CachedReadTime;
	}

	Effective = (TotalReadTime / (IntervalEnd - IntervalStart)) * Current;
}

void FImgMediaLoaderBandwidth::EmptyCache()
{
	ReadTimeCache.Empty(ReadTimeCache.Max());
	ReadTimeCacheIndex = 0;
}

/* FImgMediaLoader structors
 *****************************************************************************/

FImgMediaLoader::FImgMediaLoader(const TSharedRef<FImgMediaScheduler, ESPMode::ThreadSafe>& InScheduler,
	const TSharedRef<FImgMediaGlobalCache, ESPMode::ThreadSafe>& InGlobalCache,
	const TSharedPtr<FImgMediaMipMapInfo, ESPMode::ThreadSafe>& InMipMapInfo,
	bool bInFillGapsInSequence,
	const FImgMediaLoaderSmartCacheSettings& InSmartCacheSettings)
	: Frames(1)
	, ImageWrapperModule(FModuleManager::LoadModuleChecked<IImageWrapperModule>("ImageWrapper"))
	, Initialized(false)
	, bFillGapsInSequence(bInFillGapsInSequence)
	, TilingDescription()
	, NumLoadAhead(0)
	, NumLoadBehind(0)
	, Scheduler(InScheduler)
	, GlobalCache(InGlobalCache)
	, MipMapInfo(InMipMapInfo)
	, SequenceDim(FIntPoint::ZeroValue)
	, SequenceDuration(FTimespan::Zero())
	, SequenceFrameRate(0, 0)
	, LastRequestedFrame(INDEX_NONE)
	, RetryCount(0)
	, NextFrameNumberJobID(0)
	, UseGlobalCache(false)
	, SmartCacheSettings(InSmartCacheSettings)
	, bIsPlaybackBlocking(false)
	, bIsBandwidthThrottlingEnabled(true)
	, bIsSkipFramesEnabled(false)
	, SkipFramesCounter(0)
	, SkipFramesLevel(0)
#if WITH_EDITOR
	, Bandwidth()
#endif
{
	ResetFetchLogic();
	QueuedSampleFetch.LastTimeStamp.Time = FTimespan::Zero();
	QueuedSampleFetch.LastTimeStamp.SequenceIndex = 0;
	QueuedSampleFetch.LastDuration = FTimespan::Zero();

	UE_LOG(LogImgMedia, Verbose, TEXT("Loader %p: Created"), this);
}


FImgMediaLoader::~FImgMediaLoader()
{
	UE_LOG(LogImgMedia, Verbose, TEXT("Loader %p: Destroyed"), this);

	// clean up work item pool
	for (auto Work : WorkPool)
	{
		delete Work;
	}

	WorkPool.Empty();

	// release cache
	{
		SCOPE_CYCLE_COUNTER(STAT_ImgMedia_LoaderReleaseCache);

		Frames.Empty();
		PendingFrameNumbers.Empty();
	}
}


/* FImgMediaLoader interface
 *****************************************************************************/

uint64 FImgMediaLoader::GetBitRate() const
{
	FScopeLock Lock(&CriticalSection);
	return SequenceDim.X * SequenceDim.Y * sizeof(uint16) * 8 * SequenceFrameRate.AsDecimal();
}


void FImgMediaLoader::GetBusyTimeRanges(TRangeSet<FTimespan>& OutRangeSet) const
{
	FScopeLock Lock(&CriticalSection);
	TArray<int32> FrameNumbers;
	FrameNumbers.Reserve(QueuedFrameNumbers.Num());
	for (const auto& Entry : QueuedFrameNumbers)
	{
		FrameNumbers.Push(Entry.Frame);
	}
	FrameNumbersToTimeRanges(FrameNumbers, OutRangeSet);
}


void FImgMediaLoader::GetCompletedTimeRanges(TRangeSet<FTimespan>& OutRangeSet) const
{
	FScopeLock Lock(&CriticalSection);

	TArray<int32> CompletedFrames;
	if (UseGlobalCache)
	{
		GlobalCache->GetIndices(SequenceName, CompletedFrames);
	}
	else
	{
		Frames.GetKeys(CompletedFrames);
	}
	FrameNumbersToTimeRanges(CompletedFrames, OutRangeSet);
}


//note: use with V1 player version only!
TSharedPtr<FImgMediaTextureSample, ESPMode::ThreadSafe> FImgMediaLoader::GetFrameSample(FTimespan Time)
{
	const uint32 FrameIndex = TimeToFrameNumber(Time);

	if (FrameIndex == INDEX_NONE)
	{
		return nullptr;
	}

	FScopeLock ScopeLock(&CriticalSection);

	const TSharedPtr<FImgMediaFrame, ESPMode::ThreadSafe>* Frame = GetCachedFrame(FrameIndex);
	
	if (Frame == nullptr)
	{
		return nullptr;
	}

	const FTimespan FrameStartTime = FrameNumberToTime(FrameIndex);
	const FTimespan NextStartTime = FrameNumberToTime(FrameIndex + 1);

	auto Sample = MakeShared<FImgMediaTextureSample, ESPMode::ThreadSafe>();

	ImgMediaLoader::CheckAndUpdateImgDimensions(SequenceDim, Frame->Get()->GetDim());

	if (!Sample->Initialize(*Frame->Get(), SequenceDim, FMediaTimeStamp(FrameStartTime, 0), NextStartTime - FrameStartTime, GetNumMipLevels(), TilingDescription))
	{
		return nullptr;
	}

	return Sample;
}


const FString& FImgMediaLoader::GetImagePath(int32 FrameNumber, int32 MipLevel) const
{
	if ((MipLevel < 0) || (MipLevel >= ImagePaths.Num() ||
		(FrameNumber < 0) || (FrameNumber >= ImagePaths[MipLevel].Num())))
	{
		UE_LOG(LogImgMedia, Error, TEXT("Loader %p: GetImagePath has wrong parameters, FrameNumber:%d MipLevel:%d."),
			this, FrameNumber, MipLevel);
		static FString EmptyString(TEXT(""));
		return EmptyString;
	}
	else
	{
		return ImagePaths[MipLevel][FrameNumber];
	}
}


void FImgMediaLoader::ResetFetchLogic()
{
	QueuedSampleFetch.LoopIndex = 0;

	SkipFramesCounter = 0;
	SkipFramesLevel = 0;
}

void FImgMediaLoader::SetIsPlaybackBlocking(bool bIsBlocking)
{
	bIsPlaybackBlocking = bIsBlocking;
	UpdateBandwidthThrottling();
}

void FImgMediaLoader::HandlePause()
{
#if WITH_EDITOR
	Bandwidth.EmptyCache();
#endif
}

void FImgMediaLoader::Flush()
{
	ResetFetchLogic();
	// Adjust last timestamp / play head so it is within the current, reset sequence index range
	QueuedSampleFetch.LastTimeStamp.SequenceIndex = FMediaTimeStamp::MakeSequenceIndex(0, 0);
}

void FImgMediaLoader::Seek(const FMediaTimeStamp& SeekTarget, bool bReverse)
{
	ResetFetchLogic();
	QueuedSampleFetch.LastTimeStamp = { FMediaTimeStamp(FrameNumberToTime(TimeToFrameNumber(SeekTarget.Time)), SeekTarget.SequenceIndex) };
	QueuedSampleFetch.LastDuration = FTimespan::Zero();
}


bool FImgMediaLoader::DiscardVideoSamples(const TRange<FMediaTimeStamp>& TimeRange, bool bIsLoopingEnabled, float PlayRate)
{
	bool bOk = false;

	check(QueuedSampleFetch.LastTimeStamp.IsValid());

	if (PlayRate >= 0.0f)
	{
		if (QueuedSampleFetch.LastTimeStamp < TimeRange.GetUpperBoundValue())
		{
			QueuedSampleFetch.LastTimeStamp = TimeRange.GetUpperBoundValue();
			bOk = true;
		}
	}
	else
	{
		if (QueuedSampleFetch.LastTimeStamp >= TimeRange.GetLowerBoundValue())
		{
			QueuedSampleFetch.LastTimeStamp = TimeRange.GetLowerBoundValue() - FTimespan::FromSeconds(SequenceFrameRate.AsInterval());
			bOk = true;
		}
	}

	if (bOk)
	{
		ResetFetchLogic();
		QueuedSampleFetch.LastTimeStamp.Time = FrameNumberToTime(TimeToFrameNumber(QueuedSampleFetch.LastTimeStamp.Time));
		QueuedSampleFetch.LastDuration = FTimespan::Zero();
	}

	return bOk;
}


IMediaSamples::EFetchBestSampleResult FImgMediaLoader::FetchBestVideoSampleForTimeRange(const TRange<FMediaTimeStamp>& InTimeRange, TSharedPtr<IMediaTextureSample, ESPMode::ThreadSafe>& OutSample, bool bIsLoopingEnabled, float PlayRate)
{
	if (IsInitialized() && InTimeRange.HasLowerBound() && InTimeRange.HasUpperBound())
	{
		// A few sanity checks
		check(InTimeRange.GetLowerBoundValue() <= InTimeRange.GetUpperBoundValue());
		check(FMediaTimeStamp::GetPrimaryIndex(InTimeRange.GetLowerBoundValue().SequenceIndex) == FMediaTimeStamp::GetPrimaryIndex(InTimeRange.GetUpperBoundValue().SequenceIndex));
		check(QueuedSampleFetch.LastTimeStamp.IsValid());

		auto TimeRange = InTimeRange;

		// To avoid odd behavior with out of range time values, clamp them down just in case
		if (!bIsLoopingEnabled)
		{
			TimeRange.SetLowerBoundValue(FMediaTimeStamp(FMath::Clamp(TimeRange.GetLowerBoundValue().Time, FTimespan::Zero(), SequenceDuration), TimeRange.GetLowerBoundValue().SequenceIndex));
			TimeRange.SetUpperBoundValue(FMediaTimeStamp(FMath::Clamp(TimeRange.GetUpperBoundValue().Time, FTimespan::Zero(), SequenceDuration), TimeRange.GetUpperBoundValue().SequenceIndex));
		}

		// Clamp the current range requested against the last known timestamp to ensure we return the "next" sample at all times...
		if (PlayRate >= 0.0f)
		{
			FMediaTimeStamp NextTimeStamp = QueuedSampleFetch.LastTimeStamp + QueuedSampleFetch.LastDuration;
			if (NextTimeStamp >= TimeRange.GetUpperBoundValue())
			{
				return IMediaSamples::EFetchBestSampleResult::NoSample;
			}

			if (TimeRange.GetLowerBoundValue() < NextTimeStamp)
			{
				TimeRange = TRange<FMediaTimeStamp>(NextTimeStamp, InTimeRange.GetUpperBoundValue());
			}
		}
		else
		{
			FMediaTimeStamp NextTimeStampAtEnd = (QueuedSampleFetch.LastTimeStamp - QueuedSampleFetch.LastDuration) + FTimespan::FromSeconds(SequenceFrameRate.AsInterval());
			if (NextTimeStampAtEnd < TimeRange.GetLowerBoundValue())
			{
				return IMediaSamples::EFetchBestSampleResult::NoSample;
			}

			if (TimeRange.GetUpperBoundValue() > NextTimeStampAtEnd)
			{
				TimeRange = TRange<FMediaTimeStamp>(InTimeRange.GetLowerBoundValue(), NextTimeStampAtEnd);
			}
		}

		FTimespan StartTime = TimeRange.GetLowerBoundValue().Time;
		int32 StartLoopIndex = FMediaTimeStamp::GetSecondaryIndex(TimeRange.GetLowerBoundValue().SequenceIndex);
		FTimespan EndTime = TimeRange.GetUpperBoundValue().Time;
		int32 EndLoopIndex;

		// The end time is EXCLUSIVE, move the time by the smallest amount we can so we correctly select indices of frames
		if (EndTime > FTimespan::Zero())
		{
			EndTime -= FTimespan(1);
			EndLoopIndex = FMediaTimeStamp::GetSecondaryIndex(TimeRange.GetUpperBoundValue().SequenceIndex);
		}
		else
		{
			EndTime = SequenceDuration - FTimespan(1);
			EndLoopIndex = FMediaTimeStamp::GetSecondaryIndex(TimeRange.GetUpperBoundValue().SequenceIndex) - 1;
		}

		int32 NumFrames = GetNumImages();
		FTimespan FrameLength = FTimespan::FromSeconds(SequenceFrameRate.AsInterval());

		FTimespan UnwrappedStartTime = StartTime + FrameLength * NumFrames * StartLoopIndex;
		FTimespan UnwrappedEndTime = EndTime + FrameLength * NumFrames * EndLoopIndex;
		int64 UnwrappedStartIndex = TimeToFrameNumberUnbound(UnwrappedStartTime);
		int64 UnwrappedEndIndex = TimeToFrameNumberUnbound(UnwrappedEndTime);
		FTimespan UnwrappedStartTimeAligned = UnwrappedStartIndex * FrameLength;
		FTimespan UnwrappedEndTimeAligned = UnwrappedEndIndex * FrameLength;

		int64 MaxIdx;
		if (PlayRate >= 0.0f)
		{
			// Forward...

			// More than one possibility?
			if (UnwrappedStartIndex != UnwrappedEndIndex)
			{
				FTimespan FirstFrameDuration = FrameLength - (UnwrappedStartTime - UnwrappedStartTimeAligned);
				FTimespan LastFrameDuration = UnwrappedEndTime - UnwrappedEndTimeAligned;

				FTimespan MaxFrameDuration;
				if ((UnwrappedEndIndex - UnwrappedStartIndex) > 1)
				{
					MaxIdx = UnwrappedEndIndex - 1;
					MaxFrameDuration = FrameLength;
				}
				else
				{
					MaxIdx = UnwrappedStartIndex;
					MaxFrameDuration = FirstFrameDuration;
				}
				if (LastFrameDuration >= MaxFrameDuration)
				{
					MaxIdx = UnwrappedEndIndex;
					MaxFrameDuration = LastFrameDuration;
				}
			}
			else
			{
				MaxIdx = UnwrappedEndIndex;
			}
		}
		else
		{
			// Backward...

			// More than one possibility?
			if (UnwrappedStartIndex != UnwrappedEndIndex)
			{
				FTimespan FirstFrameDuration = FrameLength - (UnwrappedStartTime - UnwrappedStartTimeAligned);
				FTimespan LastFrameDuration = UnwrappedEndTime - UnwrappedEndTimeAligned;

				FTimespan MaxFrameDuration;
				if ((UnwrappedEndIndex - UnwrappedStartIndex) > 1)
				{
					MaxIdx = UnwrappedStartIndex + 1;
					MaxFrameDuration = FrameLength;
				}
				else
				{
					MaxIdx = UnwrappedEndIndex;
					MaxFrameDuration = LastFrameDuration;
				}
				if (FirstFrameDuration >= MaxFrameDuration)
				{
					MaxIdx = UnwrappedStartIndex;
					MaxFrameDuration = FirstFrameDuration;
				}
			}
			else
			{
				MaxIdx = UnwrappedStartIndex;
			}
		}

		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

		// Anything?
		int32 MaxLoopIdx;
		if (bIsLoopingEnabled)
		{
			MaxLoopIdx = int32(MaxIdx / NumFrames);
			bool bNegative = (MaxIdx < 0);
			MaxIdx = bNegative ? -MaxIdx : MaxIdx;
			MaxIdx = MaxIdx - (MaxIdx / NumFrames) * NumFrames;
			if (bNegative)
			{
				MaxIdx = (NumFrames - 1) - MaxIdx;
			}
		}
		else
		{
			MaxLoopIdx = 0;	// ignored below
			// Clamp this between [0..N-1], but keep a look out for any loop index that might persist from an earlier looping state
			MaxIdx = FMath::Clamp(MaxIdx, StartLoopIndex * NumFrames, StartLoopIndex * NumFrames + NumFrames - 1) - StartLoopIndex * NumFrames;
		}

		const TSharedPtr<FImgMediaFrame, ESPMode::ThreadSafe>* Frame;

		// Request data for the frame we would like... (in case it's not requested or in, yet)
		RequestFrame(MaxIdx, PlayRate, bIsLoopingEnabled);

		FScopeLock Lock(&CriticalSection);

		// Get a frame if we have one available right now...
		Frame = GetCachedFrame(MaxIdx);

		// Got a potential frame?
		if (Frame)
		{
			// Yes.

			/*
				The player facade will pass in a time range with current primary and secondary sequence indices.
				We need to make sure that we detect if we return a value "before" the last one (loop case) and
				adjust the secondary index accordingly.
			*/

			RetryCount = 0;

			FTimespan SampleTime = FrameNumberToTime(MaxIdx);

			// Set loop index if looping is active (otherwise we keep what was known before)
			if (bIsLoopingEnabled && !QueuedSampleFetch.LastDuration.IsZero())
			{
				QueuedSampleFetch.LoopIndex = MaxLoopIdx;
			}

			// Track state & setup sample for caller...
			QueuedSampleFetch.LastTimeStamp = FMediaTimeStamp(SampleTime, FMediaTimeStamp::MakeSequenceIndex(FMediaTimeStamp::GetPrimaryIndex(TimeRange.GetLowerBoundValue().SequenceIndex), QueuedSampleFetch.LoopIndex));
			QueuedSampleFetch.LastDuration = FTimespan::FromSeconds(Frame->Get()->GetFrameRate().AsInterval());

			// We are clear to return it as new result... Make a sample & initialize it...
			auto Sample = MakeShared<FImgMediaTextureSample, ESPMode::ThreadSafe>();

			ImgMediaLoader::CheckAndUpdateImgDimensions(SequenceDim, Frame->Get()->GetDim());

			if (Sample->Initialize(*Frame->Get(), SequenceDim, QueuedSampleFetch.LastTimeStamp, QueuedSampleFetch.LastDuration, GetNumMipLevels(), TilingDescription))
			{
				OutSample = Sample;
				CSV_EVENT(ImgMedia, TEXT("LoaderFetchHit %d %d-%d"), MaxIdx, UnwrappedStartIndex % NumFrames, UnwrappedEndIndex % NumFrames);
				return IMediaSamples::EFetchBestSampleResult::Ok;
			}
		}
		else
		{
			// We did not get a frame...
			// Could we have lost a frame that we previously loaded
			// due to the global cache being full?
			if (UseGlobalCache)
			{
				// Did we get a frame previously?
				if (LastRequestedFrame != INDEX_NONE)
				{
					// Are we loading any frames?
					if ((PendingFrameNumbers.Num() == 0) && (QueuedFrameNumbers.Num() == 0))
					{
						// Nope...
						// Wait for this to happen for one more frame.
						// If we have a 1 image sequence, we might have just missed the frame
						// so try again next time.
						RetryCount++;
						if (RetryCount > 1)
						{
							UE_LOG(LogImgMedia, Error, TEXT("Reloading frames. The global cache may be too small."));
							LastRequestedFrame = INDEX_NONE;
							RetryCount = 0;
						}
					}
				}
			}
		}
		CSV_EVENT(ImgMedia, TEXT("LoaderFetchMiss %d-%d"), UnwrappedStartIndex % NumFrames, UnwrappedEndIndex % NumFrames);
	}
	return IMediaSamples::EFetchBestSampleResult::NoSample;
}

 
bool FImgMediaLoader::PeekVideoSampleTime(FMediaTimeStamp &TimeStamp, bool bIsLoopingEnabled, float PlayRate, const FTimespan& CurrentTime)
{
	// Strictly speaking we never actual return a "frame" here as we do NOT involve any requests to the cache -- we simply assume we could deliver what a "queue" would contain on demand later...
	if (IsInitialized())
	{
		check(QueuedSampleFetch.LastTimeStamp.IsValid());

		if (PlayRate >= 0.0f)
		{
			TimeStamp = QueuedSampleFetch.LastTimeStamp + QueuedSampleFetch.LastDuration;
			if (TimeStamp.Time >= SequenceDuration)
			{
				TimeStamp.Time -= SequenceDuration;
				TimeStamp.SequenceIndex = FMediaTimeStamp::AdjustSecondaryIndex(QueuedSampleFetch.LastTimeStamp.SequenceIndex, 1);
			}
		}
		else
		{
			TimeStamp = QueuedSampleFetch.LastTimeStamp - QueuedSampleFetch.LastDuration;
			if (TimeStamp.Time.GetTicks() < 0)
			{
				TimeStamp.Time += SequenceDuration;
				TimeStamp.SequenceIndex = FMediaTimeStamp::AdjustSecondaryIndex(QueuedSampleFetch.LastTimeStamp.SequenceIndex, -1);
			}
		}

		TimeStamp.Time = FrameNumberToTime(TimeToFrameNumber(TimeStamp.Time));
		return true;
	}
	return false;
}


bool FImgMediaLoader::IsFrameFirst(const FTimespan& TimeStamp) const
{
	return TimeToFrameNumber(TimeStamp) == 0;
}


bool FImgMediaLoader::IsFrameLast(const FTimespan & TimeStamp) const
{
	return TimeToFrameNumber(TimeStamp) == (GetNumImages() - 1);
}


void FImgMediaLoader::GetPendingTimeRanges(TRangeSet<FTimespan>& OutRangeSet) const
{
	FScopeLock Lock(&CriticalSection);
	FrameNumbersToTimeRanges(PendingFrameNumbers, OutRangeSet);
}


IQueuedWork* FImgMediaLoader::GetWork()
{
	FScopeLock Lock(&CriticalSection);

	if (PendingFrameNumbers.IsEmpty())
	{
		return nullptr;
	}

	int32 FrameNumber = PendingFrameNumbers.Pop(EAllowShrinking::No);

	TMap<int32, FImgMediaTileSelection> DesiredMipsAndTiles;
	GetDesiredMipTiles(FrameNumber, DesiredMipsAndTiles);

	if (DesiredMipsAndTiles.IsEmpty())
	{
		// Still provide the cache with an empty frame to prevent blocking playback from stalling.
		TryAddEmptyFrame(FrameNumber);

		// No selection was visible, so we don't queue any work.
		return nullptr;
	}

	// Update minimum mip level to upscale
	MinimumMipLevelToUpscale = GetDesiredMinimumMipLevelToUpscale();

	if (MinimumMipLevelToUpscale >= 0 && NumMipLevels <= 1)
	{
		UE_CALL_ONCE([ImagePath = GetImagePath(0, 0)]
			{
				UE_LOG(LogImgMedia, Display, TEXT("No upscaling for sequence without mips: %s"), *ImagePath);
			});
	}

	FImgMediaLoaderWork* Work = (WorkPool.Num() > 0) ? WorkPool.Pop() : new FImgMediaLoaderWork(AsShared(), Reader.ToSharedRef());
	
	// Get the existing frame so we can add the mip level to it.
	TSharedPtr<FImgMediaFrame, ESPMode::ThreadSafe> ExistingFrame;
	if (const TSharedPtr<FImgMediaFrame, ESPMode::ThreadSafe>* ExistingFramePtr = GetCachedFrame(FrameNumber))
	{
		ExistingFrame = *ExistingFramePtr;
	}
	
	// Set up work.
	Work->Initialize(NextFrameNumberJobID, FrameNumber, DesiredMipsAndTiles, ExistingFrame);
	QueuedFrameNumbers.Emplace(FrameNumber, NextFrameNumberJobID++);

	return Work;
}


void FImgMediaLoader::Initialize(const FString& SequencePath, const FFrameRate& FrameRateOverride,
	bool Loop)
{
	UE_LOG(LogImgMedia, Verbose, TEXT("Loader %p: Initializing with %s (FrameRateOverride = %s, Loop = %i)"),
		this,
		*SequencePath,
		*FrameRateOverride.ToPrettyText().ToString(),
		Loop
	);

	check(!Initialized); // reinitialization not allowed for now

	FImgMediaFrameInfo FirstFrameInfo;
	
	if (LoadSequence(SequencePath, FrameRateOverride, Loop, FirstFrameInfo))
	{
		WarmupSequence(FirstFrameInfo, Loop);
	}
	
	FPlatformMisc::MemoryBarrier();

	Initialized = true;
}


bool FImgMediaLoader::RequestFrame(FTimespan Time, float PlayRate, bool Loop)
{
	return RequestFrame(TimeToFrameNumber(Time), PlayRate, Loop);
}


bool FImgMediaLoader::RequestFrame(int32 FrameNumber, float PlayRate, bool Loop)
{
	if ((FrameNumber == INDEX_NONE) || (FrameNumber == LastRequestedFrame))
	{
		// Make sure we call the reader even if we do no update - just in case it does anything
		Reader->OnTick();

		UE_LOG(LogImgMedia, VeryVerbose, TEXT("Loader %p: Skipping frame %i for time %s last %d"), this, FrameNumber, *FrameNumberToTime(FrameNumber).ToString(TEXT("%h:%m:%s.%t")), LastRequestedFrame);
		return false;
	}

	UE_LOG(LogImgMedia, VeryVerbose, TEXT("Loader %p: Requesting frame %i for time %s"), this, FrameNumber, *FrameNumberToTime(FrameNumber).ToString(TEXT("%h:%m:%s.%t")));

	Update(FrameNumber, PlayRate, Loop);
	LastRequestedFrame = FrameNumber;

	return true;
}


/* FImgMediaLoader implementation
 *****************************************************************************/

void FImgMediaLoader::FrameNumbersToTimeRanges(const TArray<int32>& FrameNumbers, TRangeSet<FTimespan>& OutRangeSet) const
{
	if (!SequenceFrameRate.IsValid() || (SequenceFrameRate.Numerator <= 0))
	{
		return;
	}

	for (const auto& FrameNumber : FrameNumbers)
	{
		const FTimespan FrameStartTime = FrameNumberToTime(FrameNumber);
		const FTimespan NextStartTime = FrameNumberToTime(FrameNumber + 1);

		OutRangeSet.Add(TRange<FTimespan>(FrameStartTime, NextStartTime));
	}
}


bool FImgMediaLoader::LoadSequence(const FString& SequencePath, const FFrameRate& FrameRateOverride, bool Loop, FImgMediaFrameInfo& FirstFrameInfo)
{
	SCOPE_CYCLE_COUNTER(STAT_ImgMedia_LoaderLoadSequence);

	if (SequencePath.IsEmpty())
	{
		return false;
	}

	// locate image sequence files
	TArray<FString> FoundPaths;
	FindFiles(SequencePath, FoundPaths);
	if (FoundPaths.Num() == 0)
	{
		UE_LOG(LogImgMedia, Error, TEXT("The directory %s does not contain any image files"), *SequencePath);
		return false;
	}
	ImagePaths.Emplace(FoundPaths);

	// Get mips.
	FindMips(SequencePath);

	// Default to a 1x1 tile
	TilingDescription.TileNum = FIntPoint(1, 1);

	FScopeLock Lock(&CriticalSection);

	// create image reader
	const FString FirstExtension = FPaths::GetExtension(ImagePaths[0][0]);

	if (FirstExtension == TEXT("exr"))
	{
#if IMGMEDIA_EXR_SUPPORTED_PLATFORM
		// Differentiate between Uncompressed exr and the rest.
		Reader = FExrImgMediaReader::GetReader(AsShared(), ImagePaths[0][0]);
#else
		UE_LOG(LogImgMedia, Error, TEXT("EXR image sequences are currently supported on macOS and Windows only"));
		return false;
#endif
	}
	else
	{
		Reader = MakeShareable(new FGenericImgMediaReader(ImageWrapperModule, AsShared()));
	}
	if (Reader.IsValid() == false)
	{
		UE_LOG(LogImgMedia, Error, TEXT("Reader is not valid for file %s."), *ImagePaths[0][0]);
		return false;
	}

	const UImgMediaSettings* Settings = GetDefault<UImgMediaSettings>();
	bIsBandwidthThrottlingEnabled = Settings->BandwidthThrottlingEnabled;
	UpdateBandwidthThrottling();
	UseGlobalCache = Settings->UseGlobalCache;
	if (SmartCacheSettings.bIsEnabled)
	{
		// Smart cache does not use the global cache.
		UseGlobalCache = false;
	}
	SequenceName = FName(*SequencePath);

	// fetch sequence attributes from first image
	{
		const TSharedPtr<FImgMediaFrame, ESPMode::ThreadSafe>* CachedFrame = GetCachedFrame(0);

		// Read info if the first frame is cached and not empty
		if (CachedFrame && !(*CachedFrame)->MipTilesPresent.IsEmpty())
		{
			FirstFrameInfo = CachedFrame->Get()->GetInfo();
		}
		else if (!Reader->GetFrameInfo(ImagePaths[0][0], FirstFrameInfo))
		{
			UE_LOG(LogImgMedia, Error, TEXT("Failed to get frame information from first image in %s"), *SequencePath);
			return false;
		}
	}
	if (FirstFrameInfo.UncompressedSize == 0)
	{
		UE_LOG(LogImgMedia, Error, TEXT("The first image in sequence %s does not have a valid frame size"), *SequencePath);
		return false;
	}

	if (FirstFrameInfo.Dim.GetMin() <= 0)
	{
		UE_LOG(LogImgMedia, Error, TEXT("The first image in sequence %s does not have a valid dimension"), *SequencePath);
		return false;
	}

	NumMipLevels = FMath::Max(FirstFrameInfo.NumMipLevels, ImagePaths.Num());
	MinimumMipLevelToUpscale = -1;

	SequenceDim = FirstFrameInfo.Dim;

	if (FrameRateOverride.IsValid() && (FrameRateOverride.Numerator > 0))
	{
		SequenceFrameRate = FrameRateOverride;
	}
	else
	{
		SequenceFrameRate = FirstFrameInfo.FrameRate;
	}

	SequenceDuration = FrameNumberToTime(GetNumImages());
	SIZE_T UncompressedSize = FirstFrameInfo.UncompressedSize;

	if (FirstFrameInfo.bHasTiles)
	{
		TilingDescription.TileBorderSize = FirstFrameInfo.TileBorder;
		TilingDescription.TileNum = FirstFrameInfo.NumTiles;
		TilingDescription.TileSize = FirstFrameInfo.TileDimensions;
	}

#if WITH_EDITOR
	// Get required bandwidth needed for all mips and tiles.
	float FrameTime = SequenceFrameRate.AsInterval();
	if (FrameTime > 0)
	{
		Bandwidth.Required = UncompressedSize / FrameTime;
	}
#endif

	// If we have no mips or tiles, then get rid of our MipMapInfoObject.
	// Otherwise, set it up.
	if (MipMapInfo.IsValid())
	{
		if ((GetNumMipLevels() == 1) && (IsTiled() == false))
		{
			MipMapInfo.Reset();
		}
		else
		{
			MipMapInfo->SetTextureInfo(SequenceName, GetNumMipLevels(), SequenceDim, TilingDescription);
			if (GetNumMipLevels() > 1)
			{
				UncompressedSize = (UncompressedSize * 4) / 3;
			}
		}
	}

	// initialize loader
	const FPlatformMemoryStats Stats = FPlatformMemory::GetStats();
	
	// We always need to cache at least 2 frames - current and the future frame.
	const int32 MinNumberOfFramesToLoad = 2;

	// Are we using the smart cache?
	if (SmartCacheSettings.bIsEnabled)
	{
		// Load frames that are a certain amount of time ahead of us.
		float TimeToLookAhead = SmartCacheSettings.TimeToLookAhead;
		
		NumFramesToLoad = TimeToLookAhead / SequenceFrameRate.AsInterval();
		// Clamp min frame to load to 1 so we always try to load something.
		int32 NumImages = GetNumImages();
		int32 MinFrames = FMath::Min(MinNumberOfFramesToLoad, NumImages);
		NumFramesToLoad = FMath::Clamp(NumFramesToLoad, MinFrames, NumImages);
		NumLoadBehind = 0;
		NumLoadAhead = NumFramesToLoad;
	}
	else
	{
		const SIZE_T DesiredCacheSize = Settings->CacheSizeGB * 1024 * 1024 * 1024;
		const SIZE_T CacheSize = FMath::Clamp(DesiredCacheSize, (SIZE_T)0, (SIZE_T)Stats.AvailablePhysical);

		const int32 MaxFramesToLoad = (int32)(CacheSize / UncompressedSize);
		int32 NumImages = GetNumImages();
		int32 MinFrames = FMath::Min(MinNumberOfFramesToLoad, NumImages);
		NumFramesToLoad = FMath::Clamp(MaxFramesToLoad, MinFrames, NumImages);
		const float LoadBehindScale = FMath::Clamp(Settings->CacheBehindPercentage, 0.0f, 100.0f) / 100.0f;

		NumLoadBehind = (int32)(LoadBehindScale * NumFramesToLoad);
		NumLoadAhead = FMath::CeilToInt32((1.0f - LoadBehindScale) * NumFramesToLoad);
	}

	Frames.Empty(NumFramesToLoad);

	// update info
	Info = TEXT("Image Sequence\n");
	Info += FString::Printf(TEXT("    Dimension: %i x %i\n"), SequenceDim.X, SequenceDim.Y);
	Info += FString::Printf(TEXT("    Format: %s\n"), *FirstFrameInfo.FormatName);
	Info += FString::Printf(TEXT("    Compression: %s\n"), *FirstFrameInfo.CompressionName);
	Info += FString::Printf(TEXT("    Frames: %i\n"), GetNumImages());
	Info += FString::Printf(TEXT("    Frame Rate: %.2f (%i/%i)\n"), SequenceFrameRate.AsDecimal(), SequenceFrameRate.Numerator, SequenceFrameRate.Denominator);

	return true;
}

void FImgMediaLoader::WarmupSequence(const FImgMediaFrameInfo& InFirstFrameInfo, bool Loop)
{
	// Giving our reader a chance to handle RAM allocation.
	// Not all readers use this, only those that need to handle large files 
	// or need to be as efficient as possible.
	Reader->PreAllocateMemoryPool(NumFramesToLoad, InFirstFrameInfo, InFirstFrameInfo.FormatName == TEXT("EXR CUSTOM"));

	FScopeLock Lock(&CriticalSection);

	Update(0, 0.0f, Loop);
}

void FImgMediaLoader::FindFiles(const FString& SequencePath, TArray<FString>& OutputPaths)
{
	// locate image sequence files
	TArray<FString> FoundFiles;
	IFileManager::Get().FindFiles(FoundFiles, *SequencePath, TEXT("*"));

	UE_LOG(LogImgMedia, Verbose, TEXT("Loader %p: Found %i image files in %s"), this, FoundFiles.Num(), *SequencePath);

	// Same list of extensions as FGenericImgMediaReader::LoadFrameImage() with the addition of exr
	const TSet<FString> SupportedImageFileExtensions = { TEXT("exr"), TEXT("jpg"), TEXT("jpeg"), TEXT("png"), TEXT("bmp") };

	TArray<FString> UnnumberedFiles;
	TSortedMap<int32, FString> SortedFoundFiles;
	SortedFoundFiles.Reserve(FoundFiles.Num());

	int32 FrameNumber;
	for (FString& File : FoundFiles)
	{
		const FString FileExtension = FPaths::GetExtension(File).ToLower();

		if (!SupportedImageFileExtensions.Contains(FileExtension))
		{
			continue;
		}

		if (GetNumberAtEndOfString(FrameNumber, File))
		{
			SortedFoundFiles.Add(FrameNumber, File);
		}
		else
		{
			UnnumberedFiles.Add(File);
		}
	}

	int32 LastIndex = -1;
	FString EmptyString;
	for (const auto& Pair : SortedFoundFiles)
	{
		// Can we fill in gaps in the sequence?
		if (bFillGapsInSequence)
		{
			// Get the index of this file.
			int32 ThisIndex = Pair.Key;

			// Fill in any gaps from the last frame.
			if ((LastIndex != -1) && (LastIndex < ThisIndex - 1))
			{
				for (int32 Index = LastIndex + 1; Index < ThisIndex; ++Index)
				{
					OutputPaths.Add(EmptyString);
				}
			}

			LastIndex = ThisIndex;
		}

		OutputPaths.Add(FPaths::Combine(SequencePath, Pair.Value));
	}

	// We still want to support unnumbered files, so we append them to the numbered list.
	UnnumberedFiles.Sort();

	for (const FString& UnnumberedFile : UnnumberedFiles)
	{
		OutputPaths.Add(FPaths::Combine(SequencePath, UnnumberedFile));
	}

}

void FImgMediaLoader::FindMips(const FString& SequencePath)
{
	// Remove trailing '/'.
	FString SequenceDir = FPaths::GetPath(SequencePath);

	// Get parent directory.
	FString BaseName = FPaths::GetCleanFilename(SequenceDir);
	FString ParentDir = FPaths::GetPath(SequenceDir);
	
	// Is this a mipmap level, i.e. something like 256x256?
	int32 Index = 0;
	bool FoundDelimeter = SequenceDir.FindLastChar(TEXT('x'), Index);
	if (FoundDelimeter)
	{
		// Loop over all mip levels.
		FString BaseMipDir = FPaths::GetPath(SequenceDir);
		int32 MipLevelWidth = 0;
		int32 MipLevelHeight = 0;

		// Getting both left and right components of resolution for mips.
		{
			FString MipLevelStringHeight = SequenceDir.RightChop(Index + 1);
			FString MipLevelStringWidth = SequenceDir.LeftChop(SequenceDir.Len() - Index);
			MipLevelHeight = FCString::Atoi(*MipLevelStringHeight);
			int32 IndexLeft = MipLevelStringWidth.Len() - 1;
			for (; IndexLeft > 0; IndexLeft--)
			{
				FString TempChop = MipLevelStringWidth.RightChop(IndexLeft);
				if (!FCString::IsNumeric(*TempChop))
				{
					break;
				}
			}
			MipLevelStringWidth = MipLevelStringWidth.RightChop(IndexLeft + 1);
			MipLevelWidth = FCString::Atoi(*MipLevelStringWidth);
		}

		while (MipLevelWidth > 1 && MipLevelHeight > 1)
		{
			// Next level down.
			MipLevelWidth /= 2;
			MipLevelHeight /= 2;

			// Try and find files for this mip level.
			FString MipDir = FPaths::Combine(BaseMipDir, FString::Printf(TEXT("%dx%d"), MipLevelWidth, MipLevelHeight));
			TArray<FString> MipFiles;
			FindFiles(MipDir, MipFiles);

			// Stop once we don't find any files.
			if (MipFiles.Num() == 0)
			{
				break;
			}

			// Make sure we have the same number of mip files over all levels.
			if (MipFiles.Num() != GetNumImages())
			{
				UE_LOG(LogImgMedia, Error, TEXT("Loader %p: Found %d images, expected %d, for %s"), 
					this, MipFiles.Num(), GetNumImages(), *MipDir);
				break;
			}

			// Make sure we don't have too many levels.
			if (ImagePaths.Num() >= MAX_MIPMAP_LEVELS)
			{
				UE_LOG(LogImgMedia, Error, TEXT("Loader %p: Found too many mipmap levels (max:%d) for %s"),
					this, MAX_MIPMAP_LEVELS, *MipDir);
				break;
			}

			// OK add this level to the list.
			ImagePaths.Emplace(MipFiles);
		}
	}
}


uint32 FImgMediaLoader::TimeToFrameNumber(FTimespan Time) const
{
	if ((Time < FTimespan::Zero()) || (Time >= SequenceDuration) || GetNumImages() < 1)
	{
		return INDEX_NONE;
	}

	/*
	* Older versions of this code attempted to help frame selection by using an Epsilon value
	* during frame selection to compensate for numerical issues. We now use frame vs. time range
	* requested coverage analysis to select the frames, which inherently will compensate for most / all
	* cases covered by the older code.
	*/

	// note: do NOT assume 100% reversability between TimeToFrameNumber() and FrameNumberToTime()! The FTimespan "only" has 100ns resolution and does some rounding when converting from seconds -> this migth shift things around!
	return static_cast<uint32>(FMath::FloorToInt32(Time.GetTotalSeconds() * (static_cast<double>(SequenceFrameRate.Numerator) / SequenceFrameRate.Denominator)));
}


FTimespan FImgMediaLoader::FrameNumberToTime(uint32 FrameNumber) const
{
	// note: do NOT assume 100% reversability between TimeToFrameNumber() and FrameNumberToTime()! The FTimespan "only" has 100ns resolution and does some rounding when converting from seconds -> this migth shift things around!
	return FTimespan::FromSeconds(FrameNumber * (static_cast<double>(SequenceFrameRate.Denominator) / SequenceFrameRate.Numerator));
}


int64 FImgMediaLoader::TimeToFrameNumberUnbound(FTimespan Time) const
{
	// note: do NOT assume 100% reversability between TimeToFrameNumber() and FrameNumberToTime()! The FTimespan "only" has 100ns resolution and does some rounding when converting from seconds -> this migth shift things around!
	return static_cast<int64>(FMath::FloorToInt(Time.GetTotalSeconds() * (static_cast<double>(SequenceFrameRate.Numerator) / SequenceFrameRate.Denominator)));
}


void FImgMediaLoader::Update(int32 PlayHeadFrame, float PlayRate, bool Loop)
{
	// In case reader needs to do something once per frame.
	// As an example return buffers back to the pool in ExrImgMediaReaderGpu.
	Reader->OnTick();

	// @todo gmp: ImgMedia: take PlayRate and DeltaTime into account when determining frames to load
	const int32 NumImagePaths = GetNumImages();

	// determine frame numbers to be loaded
	TArray<int32> FramesToLoad;
	{
		FramesToLoad.Empty(NumFramesToLoad);

		int32 FrameOffset = (PlayRate >= 0.0f) ? 1 : -1;

		int32 LoadAheadCount = NumLoadAhead;
		int32 LoadAheadIndex = PlayHeadFrame;

		int32 LoadBehindCount = NumLoadBehind;
		int32 LoadBehindIndex = PlayHeadFrame - FrameOffset;

		// Adjust if we are skipping frames.
		if (SkipFramesLevel > 0)
		{
			int32 Mult = (1 << SkipFramesLevel);
			LoadAheadIndex = GetSkipFrame(LoadAheadIndex);
			LoadBehindIndex = GetSkipFrame(LoadBehindIndex);
			FrameOffset *= Mult;
		}

		// alternate between look ahead and look behind
		
		while ((FramesToLoad.Num() < NumFramesToLoad) &&
			((LoadAheadCount > 0) || (LoadBehindCount > 0)))
		{
			if (LoadAheadCount > 0)
			{
				if (LoadAheadIndex < 0)
				{
					if (Loop)
					{
						LoadAheadIndex += NumImagePaths;
						LoadAheadIndex = GetSkipFrame(LoadAheadIndex);
					}
					else
					{
						LoadAheadCount = 0;
					}
				}
				else if (LoadAheadIndex >= NumImagePaths)
				{
					if (Loop)
					{
						LoadAheadIndex -= NumImagePaths;
						LoadAheadIndex = GetSkipFrame(LoadAheadIndex);
					}
					else
					{
						LoadAheadCount = 0;
					}
				}

				if (LoadAheadCount > 0)
				{
					FramesToLoad.Add(LoadAheadIndex);
				}

				LoadAheadIndex += FrameOffset;
				--LoadAheadCount;
			}

			if (LoadBehindCount > 0)
			{
				if (LoadBehindIndex < 0)
				{
					if (Loop)
					{
						LoadBehindIndex += NumImagePaths;
						LoadBehindIndex = GetSkipFrame(LoadBehindIndex);
					}
					else
					{
						LoadBehindCount = 0;
					}
				}
				else if (LoadBehindIndex >= NumImagePaths)
				{
					if (Loop)
					{
						LoadBehindIndex -= NumImagePaths;
						LoadBehindIndex = GetSkipFrame(LoadBehindIndex);
					}
					else
					{
						LoadBehindCount = 0;
					}
				}

				if (LoadBehindCount > 0)
				{
					FramesToLoad.Add(LoadBehindIndex);
				}

				LoadBehindIndex -= FrameOffset;
				--LoadBehindCount;
			}
		}
	}

	FScopeLock ScopeLock(&CriticalSection);

	// determine queued frame numbers that can be discarded
	for (int32 Idx = QueuedFrameNumbers.Num() - 1; Idx >= 0; --Idx)
	{
		const int32 FrameNumber = QueuedFrameNumbers[Idx].Frame;

		if (!FramesToLoad.Contains(FrameNumber))
		{
			UE_LOG(LogImgMedia, Verbose, TEXT("Loader %p: Removed Frame %i"), this, FrameNumber);
			QueuedFrameNumbers.RemoveAtSwap(Idx);
			Reader->CancelFrame(FrameNumber);
			
			// We failed to load this frame... are we throttling?
			if (bIsSkipFramesEnabled)
			{
				SkipFramesCounter++;
				if (SkipFramesCounter > SkipFramesCounterRaiseLevelThreshold)
				{
					SkipFramesLevel++;
					SkipFramesCounter = SkipFramesCounterNewLevelValue;
				}
			}
		}
	}

	// determine frame numbers that need to be cached
	PendingFrameNumbers.Empty();

	// Limit which frames can be invalidated for missing mips/tiles
	const int32 FrameInvalidationMaxCount = CVarImgMediaFrameInvalidationMaxCount.GetValueOnAnyThread();

	for (int32 FrameNumber : FramesToLoad)
	{
		// Get frame from cache.
		bool NeedFrame = false;
		const TSharedPtr<FImgMediaFrame, ESPMode::ThreadSafe>* FramePtr = GetCachedFrame(FrameNumber);

		// Did we get a frame?
		if ((FramePtr == nullptr) || ((*FramePtr).IsValid() == false))
		{
			// No, we need one.
			NeedFrame = true;
		}
		else if(ImgMediaLoader::IsCachedFrameInRange(FrameNumber, PlayHeadFrame, FrameInvalidationMaxCount, NumImagePaths, PlayRate))
		{
			// Yes. Check if we have all the desired tiles per mip level.
			TMap<int32, FImgMediaTileSelection> DesiredMipsAndTiles;
			GetDesiredMipTiles(FrameNumber, DesiredMipsAndTiles);

			NeedFrame = !(*FramePtr)->MipTilesPresent.ContainsMipTiles(DesiredMipsAndTiles);

			if (NeedFrame)
			{
				UE_LOG(LogImgMedia, VeryVerbose, TEXT("Loader %p: Invalidating frame %i at playhead %i due to missing mips or tiles."), this, FrameNumber, PlayHeadFrame);
			}
		}
		
		if (NeedFrame)
		{
			bool FoundFrame = false;
			for (const auto& Entry : QueuedFrameNumbers)
			{
				if (Entry.Frame == FrameNumber)
				{
					FoundFrame = true;
					break;
				}
			}

			if (!FoundFrame)
			{
				// Do we actually have a frame for this?
				if (ImagePaths[0][FrameNumber].Len() == 0)
				{
					TryAddEmptyFrame(FrameNumber);
				}
				else
				{
					PendingFrameNumbers.Add(FrameNumber);
				}
			}
		}
	}
	Algo::Reverse(PendingFrameNumbers);

	// Are we throttling and there no frames pending?
	if ((bIsSkipFramesEnabled) && (PendingFrameNumbers.Num() == 0))
	{
		// This implies that we have spare bandwidth, so lower the skip count.
		if (SkipFramesCounter > 0)
		{
			SkipFramesCounter--;
			if (SkipFramesCounter <= 0)
			{
				if (SkipFramesLevel > 0)
				{
					SkipFramesLevel--;
					SkipFramesCounter = SkipFramesCounterNewLevelValue;
				}
			}
		}
	}
	CSV_EVENT(ImgMedia, TEXT("LoaderUpdatePending %d %d"), (PendingFrameNumbers.Num() > 0) ? PendingFrameNumbers[0] : -1, (PendingFrameNumbers.Num() > 0) ? PendingFrameNumbers[PendingFrameNumbers.Num() - 1] : -1);
}


/* IImgMediaLoader interface
 *****************************************************************************/


bool FImgMediaLoader::TryAddEmptyFrame(int32 FrameNumber)
{
	FScopeLock Lock(&CriticalSection);

	bool bAddFrame = UseGlobalCache ? !GlobalCache->Contains(SequenceName, FrameNumber) : !Frames.Contains(FrameNumber);

	if (bAddFrame)
	{
		const int32 NumChannels = 3;
		const int32 PixelSize = sizeof(uint16) * NumChannels;
		TSharedPtr<FImgMediaFrame, ESPMode::ThreadSafe> Frame = MakeShared<FImgMediaFrame>();
		
		FImgMediaFrameInfo FrameInfo = {};
		FrameInfo.Dim = SequenceDim;
		FrameInfo.FrameRate = SequenceFrameRate;
		FrameInfo.NumChannels = NumChannels;
		FrameInfo.bHasTiles = false;
		Frame->SetInfo(FrameInfo);

		Frame->Format = EMediaTextureSampleFormat::FloatRGB;
		Frame->Stride = SequenceDim.X * PixelSize;

		AddFrameToCache(FrameNumber, Frame);
	}

	return bAddFrame;
}

void FImgMediaLoader::NotifyWorkComplete(FImgMediaLoaderWork& CompletedWork, int32 JobID, int32 FrameNumber,
	const TSharedPtr<FImgMediaFrame, ESPMode::ThreadSafe>& Frame, float WorkTime)
{
	FScopeLock Lock(&CriticalSection);

	// if frame is still needed, add it to the cache
	if (QueuedFrameNumbers.Remove({ FrameNumber, JobID }) > 0)
	{
		AddFrameToCache(FrameNumber, Frame);

		// Reduce SkipFramesCounter when at skip level 0, otherwise it will never go down
		// unless we have a lot more bandwidth than needed.
		if ((bIsSkipFramesEnabled) && (SkipFramesLevel == 0) && (SkipFramesCounter > 0))
		{
			SkipFramesCounter--;
		}
	}
	
	// Make sure the frame is no longer cancelled, otherwise we might end up cancelling this frame
	// later on inadvertently!
	Reader->UncancelFrame(FrameNumber);

	WorkPool.Push(&CompletedWork);

#if WITH_EDITOR
	// Update bandwidth.
	if (Frame.IsValid() && WorkTime > 0.0)
	{
		Bandwidth.Update(Frame, WorkTime);
	}
#endif
}

void FImgMediaLoader::AddFrameToCache(int32 FrameNumber, const TSharedPtr<FImgMediaFrame, ESPMode::ThreadSafe>& Frame)
{
	if (Frame.IsValid())
	{
		UE_LOG(LogImgMedia, VeryVerbose, TEXT("Loader %p: Loaded frame %i"), this, FrameNumber);
		if (UseGlobalCache)
		{
			const TSharedPtr<FImgMediaFrame, ESPMode::ThreadSafe>* ExistingFrame;
			ExistingFrame = GlobalCache->FindAndTouch(SequenceName, FrameNumber);
			if (ExistingFrame == nullptr)
			{
				GlobalCache->AddFrame(ImagePaths[0][FrameNumber], SequenceName, FrameNumber, Frame, MipMapInfo.IsValid());
			}
		}
		else
		{
			Frames.Add(FrameNumber, Frame);
		}
	}
}

void FImgMediaLoader::GetDesiredMipTiles(int32 FrameIndex, TMap<int32, FImgMediaTileSelection>& OutMipsAndTiles)
{
	// note: While currently unused, FrameIndex could become useful for recorded frames.

	if(MipMapInfo.IsValid() && MipMapInfo->HasObjects())
	{
		OutMipsAndTiles = MipMapInfo->GetVisibleTiles();
	}
	else
	{
		//Fallback case where we activate all mips and tiles.
		for (int32 MipLevel = 0; MipLevel < GetNumMipLevels(); ++MipLevel)
		{
			OutMipsAndTiles.Emplace(MipLevel, FImgMediaTileSelection::CreateForTargetMipLevel(SequenceDim, TilingDescription.TileSize, MipLevel, true));
		}
	}
}

int32 FImgMediaLoader::GetDesiredMinimumMipLevelToUpscale()
{
	if (MipMapInfo.IsValid())
	{
		return MipMapInfo->GetMinimumMipLevelToUpscale();
	}

	return -1;
}


FTimespan FImgMediaLoader::ModuloTime(FTimespan Time)
{
	bool IsNegative = Time < FTimespan::Zero();
	
	FTimespan NewTime = Time % SequenceDuration;
	if (IsNegative)
	{
		NewTime = SequenceDuration + NewTime;
	}

	return NewTime;
}


bool FImgMediaLoader::GetNumberAtEndOfString(int32 &Number, const FString& String) const
{
	bool bFoundNumber = false;

	// Find the first digit starting from the right.
	int32 Index = String.Len() - 1;
	for (; Index >= 0; --Index)
	{
		bool bIsDigit = FChar::IsDigit(String[Index]);
		if (bIsDigit)
		{
			break;
		}
	}
	
	// Did we find one?
	if (Index >= 0)
	{
		int32 LastNumberIndex = Index;
		// Find the next non digit...
		for (; Index >= 0; --Index)
		{
			bool bIsDigit = FChar::IsDigit(String[Index]);
			if (bIsDigit == false)
			{
				break;
			}
		}

		// Index now points to the next non digit.
		// Extract the number.
		Index++;
		if (Index < String.Len())
		{
			bFoundNumber = true;
			FString NumberString = String.Mid(Index, LastNumberIndex - Index + 1);
			Number = FCString::Atoi(*NumberString);
		}
	}
	
	return bFoundNumber;
}


int32 FImgMediaLoader::GetSkipFrame(int32 FrameIndex)
{
	int32 Mult = (1 << SkipFramesLevel);
	int32 Mask = ~(Mult - 1);
	FrameIndex = (FrameIndex & Mask) + (Mult - 1);
	
	const int32 NumImagePaths = GetNumImages();
	if (FrameIndex >= NumImagePaths)
	{
		FrameIndex = NumImagePaths - 1;
	}

	return FrameIndex;
}

void FImgMediaLoader::UpdateBandwidthThrottling()
{
	bIsSkipFramesEnabled = bIsBandwidthThrottlingEnabled && (bIsPlaybackBlocking == false);
	if (bIsSkipFramesEnabled == false)
	{
		SkipFramesCounter = 0;
		SkipFramesLevel = 0;
	}
}

const TSharedPtr<FImgMediaFrame, ESPMode::ThreadSafe>* FImgMediaLoader::GetCachedFrame(int32 InFrameNumber)
{
	if (UseGlobalCache)
	{
		return GlobalCache->FindAndTouch(SequenceName, InFrameNumber);
	}
	else
	{
		return Frames.FindAndTouch(InFrameNumber);
	}
}

