// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "Templates/SharedPointer.h"

#include "MediaSampleSink.h"


/**
 * Collection of media sample sinks.
 *
 * @param SampleType The type of media samples that the sinks process.
 */
template<typename SampleType, typename SinkType=TMediaSampleSink<SampleType>>
class TMediaSampleSinks
{
public:

	/**
	 * Add the given media sample sink to the collection.
	 *
	 * @param SampleSink The sink to add.
	 * @see Num, Remove
	 */
	void Add(const TSharedRef<SinkType, ESPMode::ThreadSafe>& SampleSink)
	{
		Sinks.AddUnique(SampleSink);
	}

	/**
	 * Enqueue the given media samples to the registered sinks.
	 *
	 * This method will also remove expired sinks that haven't been removed yet.
	 *
	 * @param Sample The media sample to enqueue.
	 * @param MaxQueueDepth The maximum depth of the sink queues before overflow.
	 * @return true if the sample was enqueued to all sinks, false if one or more sinks overflowed.
	 * @see Flush
	 */
	bool Enqueue(const TSharedRef<SampleType, ESPMode::ThreadSafe>& Sample, int32 MaxDepth)
	{
		bool Overflowed = false;

		for (int32 SinkIndex = Sinks.Num() - 1; SinkIndex >= 0; --SinkIndex)
		{
			TSharedPtr<SinkType, ESPMode::ThreadSafe> Sink = Sinks[SinkIndex].Pin();

			if (Sink.IsValid())
			{
				if (Sink->Num() >= MaxDepth)
				{
					Overflowed = true;
				}
				else
				{
					Sink->Enqueue(Sample);
				}
			}
			else
			{
				Sinks.RemoveAtSwap(SinkIndex);
			}
		}

		return !Overflowed;
	}

	/**
	 * Flush all registered sinks.
	 *
	 * This method will also remove expired sinks that haven't been removed yet.
	 *
	 * @see Enqueue
	 */
	void Flush()
	{
		for (int32 SinkIndex = Sinks.Num() - 1; SinkIndex >= 0; --SinkIndex)
		{
			TSharedPtr<SinkType, ESPMode::ThreadSafe> Sink = Sinks[SinkIndex].Pin();

			if (Sink.IsValid())
			{
				Sink->RequestFlush();
			}
			else
			{
				Sinks.RemoveAtSwap(SinkIndex);
			}
		}
	}

	/**
	 * Get the number of sinks in this collection.
	 *
	 * @return Number of sinks.
	 * @see Add, Remove
	 */
	int32 Num() const
	{
		return Sinks.Num();
	}

	/**
	 * Remove the given media sample sink from the collection.
	 *
	 * @param SampleSink The sink to remove.
	 * @see Add, Num
	 */
	void Remove(const TSharedRef<SinkType, ESPMode::ThreadSafe>& SampleSink)
	{
		Sinks.Remove(SampleSink);
	}

	/**
	 * Remove any invalid sinks
	 */
	void Cleanup()
	{
		for (int32 SinkIndex = Sinks.Num() - 1; SinkIndex >= 0; --SinkIndex)
		{
			if (!Sinks[SinkIndex].IsValid())
			{
				Sinks.RemoveAtSwap(SinkIndex);
			}
		}
	}

protected:

	/** The collection of registered sinks. */
	TArray<TWeakPtr<SinkType, ESPMode::ThreadSafe>> Sinks;
};

typedef TMediaSampleSinks<IMediaTextureSample, FMediaTextureSampleSink> FMediaVideoSampleSinks;

class FMediaAudioSampleSinks : public TMediaSampleSinks<IMediaAudioSample, FMediaAudioSampleSink>
{
public:
	/**
	 * Get primary audio sink and cleanup any invalid sinks
	 */
	TSharedPtr<FMediaAudioSampleSink, ESPMode::ThreadSafe> GetPrimaryAudioSink()
	{
		Cleanup();
		if (Sinks.Num() == 0)
		{
			return TSharedPtr<FMediaAudioSampleSink, ESPMode::ThreadSafe>();
		}
		return Sinks[0].Pin();
	}
};

typedef TMediaSampleSinks<IMediaOverlaySample, FMediaOverlaySampleSink> FMediaOverlaySampleSinks;
typedef TMediaSampleSinks<IMediaBinarySample, FMediaBinarySampleSink> FMediaBinarySampleSinks;
