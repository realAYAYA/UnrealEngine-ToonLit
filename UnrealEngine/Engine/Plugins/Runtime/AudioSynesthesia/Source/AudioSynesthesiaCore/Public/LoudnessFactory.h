// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LoudnessAnalyzer.h"
#include "IAudioAnalyzerInterface.h"
#include "DSP/SlidingWindow.h"

namespace Audio
{
	/**
	 * Contains settings for loudness analyzer.
	 */
	class AUDIOSYNESTHESIACORE_API FLoudnessSettings : public IAnalyzerSettings, public FLoudnessAnalyzerSettings
	{
		public:

		/** Number of seconds between loudness measurements */
		float AnalysisPeriod = 0.01f;
	};

	/**
	 * Holds the loudness values per a time step.
	 */
	struct FLoudnessEntry
	{
		int32 Channel = 0;
		float Timestamp = 0.f;
		float Energy = 0.f;
		float Loudness = 0.f;
	};

	/** 
	 * FLoudnessResult contains the temporal evolution of loudness for the last buffer of audio analyzed.
	 */
	class AUDIOSYNESTHESIACORE_API FLoudnessResult : public IAnalyzerResult
	{
	public:
		/**
		 * Denotes the overall loudness as opposed individual channel indices.
		 */
		static const int32 ChannelIndexOverall;

		FLoudnessResult() {}

		/** Appends an FLoudnessDatum to the container. */
		void Add(const FLoudnessEntry& InDatum);

		/** Returns const reference to FLoudnessDatum array for individual channel. */
		const TArray<FLoudnessEntry>& GetChannelLoudnessArray(int32 ChannelIdx) const;

		/** Returns const reference to FLoudnessDatum array associated with overall loudness. */
		const TArray<FLoudnessEntry>& GetLoudnessArray() const;

		/** Returns the number of channels. */
		int32 GetNumChannels() const;

	private:
		TMap<int32, TArray<FLoudnessEntry> > ChannelLoudnessArrays;
	};

	/**
	 * FLoudnessWorker performs loudness analysis on input sample buffers.
	 */
	class AUDIOSYNESTHESIACORE_API FLoudnessWorker : public IAnalyzerWorker
	{
	public:
		/** Construct a worker */
		FLoudnessWorker(const FAnalyzerParameters& InParams, const FLoudnessSettings& InAnalyzerSettings);

		/** Analyzes input sample buffer and updates result. */
		virtual void Analyze(TArrayView<const float> InAudio, IAnalyzerResult* OutResult) override;

	private:

		/** Analyze a single window. */
		void AnalyzeWindow(TArrayView<const float> InWindow, FLoudnessResult& OutResult);

		int32 NumChannels = 0;
		int32 NumAnalyzedBuffers = 0;
		int32 NumHopFrames = 0;
		int32 SampleRate = 0;
		TArray<float> InternalWindow;
		TUniquePtr<TSlidingBuffer<float>> InternalBuffer;
		TUniquePtr<FMultichannelLoudnessAnalyzer> Analyzer;
	};

	/**
	 * Defines the Loudness analyzer and creates related classes.
	 */
	class AUDIOSYNESTHESIACORE_API FLoudnessFactory : public IAnalyzerFactory
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

