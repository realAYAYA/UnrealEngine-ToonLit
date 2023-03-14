// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Queue.h"
#include "HAL/PlatformAtomics.h"
#include "IMediaSamples.h"
#include "HAL/CriticalSection.h"
#include "Math/Interval.h"
#include "Misc/App.h"
#include "Misc/ScopeLock.h"
#include "Misc/Timespan.h"
#include "Templates/SharedPointer.h"

#include "MediaSampleSink.h"
#include "MediaSampleSource.h"

#include "IMediaTimeSource.h"
#include "IMediaTextureSample.h"

/**
 * Template for media sample queues.
 */
template<typename SampleType, typename SinkType=TMediaSampleSink<SampleType>>
class TMediaSampleQueue
	: public SinkType
	, public TMediaSampleSource<SampleType>
{
public:

	/** Default constructor. */
	TMediaSampleQueue()
	{ }

	/** Virtual destructor. */
	virtual ~TMediaSampleQueue() { }

public:

	/**
	 * Get the number of samples in the queue.
	 *
	 * Note: The value returned by this function is only eventually consistent. It
	 * can be called by both consumer and producer threads, but it should not be used
	 * to query the actual state of the queue. Always use Dequeue and Peek instead!
	 *
	 * @return Number of samples.
	 * @see Enqueue, Dequeue, Peek
	 */
	int32 Num() const
	{
		return Samples.Num();
	}

public:

	//~ TMediaSampleSource interface (to be called only from consumer thread)

	virtual bool Dequeue(TSharedPtr<SampleType, ESPMode::ThreadSafe>& OutSample) override
	{
		FScopeLock Lock(&CriticalSection);

		if (Samples.Num() == 0)
		{
			return false; // empty queue
		}

		TSharedPtr<SampleType, ESPMode::ThreadSafe> Sample(Samples[0]);
			
		if (!Sample.IsValid())
		{
			return false; // pending flush
		}

		Samples.RemoveAt(0);

		OutSample = Sample;

		return true;
	}

	virtual bool Peek(TSharedPtr<SampleType, ESPMode::ThreadSafe>& OutSample) override
	{
		FScopeLock Lock(&CriticalSection);

		if (Samples.Num() == 0)
		{
			return false; // empty queue
		}

		TSharedPtr<SampleType, ESPMode::ThreadSafe> Sample(Samples[0]);

		if (!Sample.IsValid())
		{
			return false; // pending flush
		}

		OutSample = Sample;

		return true;
	}

	virtual bool Pop() override
	{
		FScopeLock Lock(&CriticalSection);

		if (Samples.Num() == 0)
		{
			return false; // empty queue
		}

		if (!Samples[0].IsValid())
		{
			return false; // pending flush
		}

		Samples.RemoveAt(0);

		return true;
	}

	bool FetchBestSampleForTimeRange(const TRange<FMediaTimeStamp> & TimeRange, TSharedPtr<SampleType, ESPMode::ThreadSafe>& OutSample, bool bReverse)
	{
		// Code below assumes a fully specified range, no open bounds!
		check(TimeRange.HasLowerBound() && TimeRange.HasUpperBound());

		OutSample.Reset();

		FScopeLock Lock(&CriticalSection);

		int32 Num = Samples.Num();
		if (Num == 0)
		{
			return false;
		}

		int32 FirstPossibleIndex = -1;
		int32 LastPossibleIndex = -1;
		int32 NumOldSamplesAtBegin = 0;
		for (int32 Idx = 0; Idx < Num; ++Idx)
		{
			const TSharedPtr<SampleType, ESPMode::ThreadSafe> & Sample = Samples[Idx];
			TRange<FMediaTimeStamp> SampleTimeRange = !bReverse ? TRange<FMediaTimeStamp>(Sample->GetTime(), Sample->GetTime() + Sample->GetDuration())
																: TRange<FMediaTimeStamp>(Sample->GetTime() - Sample->GetDuration(), Sample->GetTime());

			if (TimeRange.Overlaps(SampleTimeRange))
			{
				// Sample is at least partially inside the requested range, recall the range of samples we find...
				if (FirstPossibleIndex < 0)
				{
					FirstPossibleIndex = Idx;
				}
				LastPossibleIndex = Idx;
			}
			else
			{
				if (!bReverse ? (SampleTimeRange.GetLowerBoundValue() >= TimeRange.GetUpperBoundValue()) :
								(SampleTimeRange.GetUpperBoundValue() <= TimeRange.GetLowerBoundValue()))
				{
					// Sample is entirely past requested time range, we can stop
					// (we assume monotonically increasing time stamps here)
					break;
				}

				// If the incoming data it not monotonically increasing we migth get here after we already found the first overlapping sample
				// -> we do not count further non-overlapping, older samples into this range
				if (FirstPossibleIndex < 0)
				{
					// Sample is before time range, we will delete is later, no reason to keep it
					++NumOldSamplesAtBegin;
				}
				else
				{
					// If we find an older non-verlapping sample after an overlapping one, we move the last possible index on to ensure these samples die ASAP
					LastPossibleIndex = Idx;
				}
			}
		}

		// Found anything?
		if (FirstPossibleIndex >= 0)
		{
			if (FirstPossibleIndex != LastPossibleIndex)
			{
				// More then one sample. Find the one that fits the bill, best...
				// (we look for the one with the largest overlap & newest time)
				FMediaTimeStamp BestDuration(FTimespan::Zero(), -1);
				int32 BestIndex = FirstPossibleIndex;
				for (int32 Idx = FirstPossibleIndex; Idx <= LastPossibleIndex; ++Idx)
				{
					const TSharedPtr<SampleType, ESPMode::ThreadSafe>& Sample = Samples[Idx];

					// Check once more if this sample is actually overlapping as we may get non-monotonically increasing data...
					TRange<FMediaTimeStamp> SampleTimeRange = !bReverse ? TRange<FMediaTimeStamp>(Sample->GetTime(), Sample->GetTime() + Sample->GetDuration())
																		: TRange<FMediaTimeStamp>(Sample->GetTime() - Sample->GetDuration(), Sample->GetTime());

					if (TimeRange.Overlaps(SampleTimeRange))
					{
						// Ok. This one is real, see if it is a better fit than the last one...
						TRange<FMediaTimeStamp> SampleInRangeRange(TRange<FMediaTimeStamp>::Intersection(SampleTimeRange, TimeRange));

						FMediaTimeStamp SampleDuration(SampleInRangeRange.Size<FMediaTimeStamp>());
						if (SampleDuration >= BestDuration)
						{
							BestDuration = SampleDuration;
							BestIndex = Idx;
						}
					}
				}

				check(BestIndex >= NumOldSamplesAtBegin);

				// Found the best. Return it & delete all candidate samples up and including it from the queue
				OutSample = Samples[BestIndex];
				Samples.RemoveAt(FirstPossibleIndex, BestIndex - FirstPossibleIndex + 1);
			}
			else
			{
				// Single sample found: we just take it!
				OutSample = Samples[FirstPossibleIndex]; //-V781 PVS-Studio triggers incorrectly here: Variable checked after being used (likely a template code issue, but harmless)
				Samples.RemoveAt(FirstPossibleIndex);
			}
		}

		// Any frames considered outdated?
		if (NumOldSamplesAtBegin != 0)
		{
			// In case we got no new frame that fits into the current frame, return the newest of the "old" frames
			if (!OutSample.IsValid())
			{
				OutSample = Samples[NumOldSamplesAtBegin - 1];
			}
			// Cleanup samples that are now considered outdated...
			Samples.RemoveAt(0, NumOldSamplesAtBegin);
		}

		// Return true if we got a sample...
		return (OutSample.IsValid());
	}


	uint32 PurgeOutdatedSamples(const FMediaTimeStamp & ReferenceTime, bool bReversed)
	{
		FScopeLock Lock(&CriticalSection);

		int32 Num = Samples.Num();
		if (Num > 0)
		{
			int32 Idx = 0;
			if (!bReversed)
			{
				for (; Idx < Num; ++Idx)
				{
					const TSharedPtr<SampleType, ESPMode::ThreadSafe> & Sample = Samples[Idx];
					if ((Sample->GetTime().Time + Sample->GetDuration()) > ReferenceTime.Time)
					{
						break;
					}
				}
			}
			else
			{
				for (; Idx < Num; ++Idx)
				{
					const TSharedPtr<SampleType, ESPMode::ThreadSafe> & Sample = Samples[Idx];
					if ((Sample->GetTime().Time - Sample->GetDuration()) < ReferenceTime.Time)
					{
						break;
					}
				}
			}
			if (Idx > 0)
			{
				Samples.RemoveAt(0, Idx);
			}
			return Idx;
		}

		return 0;
	}

public:

	//~ TMediaSampleSink interface (to be called only from producer thread)

	virtual bool Enqueue(const TSharedRef<SampleType, ESPMode::ThreadSafe>& Sample) override
	{
		FScopeLock Lock(&CriticalSection);

		Samples.Push(Sample);
		return true;
	}

	virtual void RequestFlush() override
	{
		FScopeLock Lock(&CriticalSection);
		Samples.Empty();
	}

protected:
	mutable FCriticalSection CriticalSection;
	TArray<TSharedPtr<SampleType, ESPMode::ThreadSafe>> Samples;
};


/** audio sample queue. */
class FMediaAudioSampleQueue : public TMediaSampleQueue<class IMediaAudioSample, class FMediaAudioSampleSink>
{
public:
	FMediaAudioSampleQueue(int32 InMaxAudioSamplesInQueue = -1)
		: MaxAudioSamplesInQueue(InMaxAudioSamplesInQueue) {}

	void SetAudioTime(const FMediaTimeStampSample & InAudioTime)
	{
		FScopeLock Lock(&CriticalSection);
		AudioTime = InAudioTime;
	}

	FMediaTimeStampSample GetAudioTime() const override
	{
		FScopeLock Lock(&CriticalSection);
		return AudioTime;
	}

	void InvalidateAudioTime() override
	{
		FScopeLock Lock(&CriticalSection);
		AudioTime.Invalidate();
	}

	virtual void RequestFlush() override
	{
		FScopeLock Lock(&CriticalSection);
		TMediaSampleQueue<class IMediaAudioSample, class FMediaAudioSampleSink>::RequestFlush();
		AudioTime.Invalidate();
	}

	virtual bool Enqueue(const TSharedRef<IMediaAudioSample, ESPMode::ThreadSafe>& Sample) override
	{
		FScopeLock Lock(&CriticalSection);

		if (MaxAudioSamplesInQueue > 0 && Samples.Num() >= MaxAudioSamplesInQueue)
		{
			return false;
		}

		Samples.Push(Sample);
		return true;
	}

	virtual bool CanAcceptSamples(int32 NumSamples) const
	{
		return (Samples.Num() + NumSamples) <= MaxAudioSamplesInQueue;
	}
private:
	int32 MaxAudioSamplesInQueue;
	FMediaTimeStampSample AudioTime;
};

/** Type definition for binary sample queue. */
typedef TMediaSampleQueue<class IMediaBinarySample> FMediaBinarySampleQueue;

/** Type definition for overlay sample queue. */
typedef TMediaSampleQueue<class IMediaOverlaySample> FMediaOverlaySampleQueue;

/** Type definition for texture sample queue. */
typedef TMediaSampleQueue<class IMediaTextureSample> FMediaTextureSampleQueue;
