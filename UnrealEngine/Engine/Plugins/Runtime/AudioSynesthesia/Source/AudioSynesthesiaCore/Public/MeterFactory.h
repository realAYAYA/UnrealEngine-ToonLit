// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MeterAnalyzer.h"
#include "IAudioAnalyzerInterface.h"
#include "DSP/SlidingWindow.h"

namespace Audio
{
	/**
	 * Contains settings for meter  analyzer.
	 */
	class AUDIOSYNESTHESIACORE_API FMeterSettings : public IAnalyzerSettings, public FMeterAnalyzerSettings
	{	
	public:
		/** Number of seconds between loudness measurements */
		float AnalysisPeriod = 0.01f;
	};

	/**
	 * Holds the meter results per a time step for each channel
	 */
	struct FMeterEntry
	{
		int32 Channel = 0;
		float Timestamp = 0.0f;
		float MeterValue = 0.0f;
		float PeakValue = 0.0f;
		float ClippingValue = 0.0f;
		int32 NumSamplesClipping = 0;
	};

	/** 
	 * FMeterResult contains the temporal evolution of loudness for the last buffer of audio analyzed.
	 */
	class AUDIOSYNESTHESIACORE_API FMeterResult : public IAnalyzerResult
	{
	public:
		/**
		 * Denotes the overall loudness as opposed individual channel indices.
		 */
		static const int32 ChannelIndexOverall;

		FMeterResult() {}

		/** Appends an FMeterEntry to the container. */
		void Add(const FMeterEntry& InEntry);

		/** Returns const reference to FMeterEntry array for individual channel. */
		const TArray<FMeterEntry>& GetChannelMeterArray(int32 ChannelIdx) const;

		/** Returns const reference to FMeterEntry array associated with overall loudness. */
		const TArray<FMeterEntry>& GetMeterArray() const;

		/** Returns the number of channels. */
		int32 GetNumChannels() const;

	private:
		float DurationInSeconds = 0.0f;
		TMap<int32, TArray<FMeterEntry> > ChannelMeterArrays;
	};

	/**
	 * FMeterWorker performs meter analysis on input sample buffers.
	 */
	class AUDIOSYNESTHESIACORE_API FMeterWorker : public IAnalyzerWorker
	{
	public:
		/** Construct a worker */
		FMeterWorker(const FAnalyzerParameters& InParams, const FMeterSettings& InAnalyzerSettings);

		/** Analyzes input sample buffer and updates result. */
		virtual void Analyze(TArrayView<const float> InAudio, IAnalyzerResult* OutResult) override;

	private:

		int32 NumChannels = 0;
		int32 NumAnalyzedBuffers = 0;
		int32 NumHopFrames = 0;
		int32 SampleRate = 0;
		TArray<float> InternalWindow;
		TUniquePtr<TSlidingBuffer<float>> InternalBuffer;
		TUniquePtr<FMeterAnalyzer> MeterAnalyzer;
	};

	/**
	 * Defines the meter analyzer and creates related classes.
	 */
	class AUDIOSYNESTHESIACORE_API FMeterFactory : public IAnalyzerFactory
	{
		public:

		/** Name of specific analyzer type. */
		virtual FName GetName() const override;

		/** Human readable name of analyzer. */
		virtual FString GetTitle() const override;

		/** Creates a new FLoudnessNRTResult */
		virtual TUniquePtr<IAnalyzerResult> NewResult() const override;

		/**  Creates a new FLoudnessWorker. This expects IAnalyzerNRTSettings to be a valid pointer to a FLoudnessSettings object. */
		virtual TUniquePtr<IAnalyzerWorker> NewWorker(const FAnalyzerParameters& InParams, const IAnalyzerSettings* InSettings) const override;
	};
}

