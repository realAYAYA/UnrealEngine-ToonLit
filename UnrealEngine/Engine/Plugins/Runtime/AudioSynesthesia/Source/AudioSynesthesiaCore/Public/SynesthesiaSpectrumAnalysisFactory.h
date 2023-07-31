// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SynesthesiaSpectrumAnalyzer.h"
#include "IAudioAnalyzerInterface.h"
#include "DSP/SlidingWindow.h"

namespace Audio
{
	/**
	 * Contains settings for Spectrum analyzer.
	 */
	class AUDIOSYNESTHESIACORE_API FSynesthesiaSpectrumAnalysisSettings : public IAnalyzerSettings, public FSynesthesiaSpectrumAnalyzerSettings
	{	
	public:
		/** Number of seconds between spectrum measurements */
		float AnalysisPeriod = 0.01f;
	};

	/**
	 * Holds the spectrum results per a time step for each channel
	 */
	struct FSynesthesiaSpectrumEntry
	{
		int32 Channel = 0;
		float Timestamp = 0.0f;
		TArray<float> SpectrumValues;
	};

	/** 
	 * FSynesthesiaSpectrumResult contains the temporal evolution of loudness for the last buffer of audio analyzed.
	 */
	class AUDIOSYNESTHESIACORE_API FSynesthesiaSpectrumResult : public IAnalyzerResult
	{
	public:
		FSynesthesiaSpectrumResult() {}

		/** Appends an FSynesthesiaSpectrumEntry to the container. */
		void Add(FSynesthesiaSpectrumEntry&& InEntry);

		/** Returns const reference to FSynesthesiaSpectrumEntry array for individual channel. */
		const TArray<FSynesthesiaSpectrumEntry>& GetChannelSpectrumArray(int32 ChannelIdx) const;

		/** Returns the number of channels. */
		int32 GetNumChannels() const;

	private:
		float DurationInSeconds = 0.0f;
		TMap<int32, TArray<FSynesthesiaSpectrumEntry>> ChannelSpectrumArrays;
	};

	/**
	 * FSynesthesiaSpectrumWorker performs Spectrum analysis on input sample buffers.
	 */
	class AUDIOSYNESTHESIACORE_API FSynesthesiaSpectrumAnalysisWorker : public IAnalyzerWorker
	{
	public:
		/** Construct a worker */
		FSynesthesiaSpectrumAnalysisWorker(const FAnalyzerParameters& InParams, const FSynesthesiaSpectrumAnalysisSettings& InAnalyzerSettings);

		/** Analyzes input sample buffer and updates result. */
		virtual void Analyze(TArrayView<const float> InAudio, IAnalyzerResult* OutResult) override;

	private:

		int32 NumChannels = 0;
		bool bDownmixToMono = true;
		int32 NumOutputFrames = 0;
		int32 NumWindowFrames = 0;
		int32 NumWindowSamples = 0;
		int32 NumHopFrames = 0;
		int32 SampleRate = 0;
		int64 FrameCounter = 0;

		FAlignedFloatBuffer MonoBuffer; 
		FAlignedFloatBuffer ChannelBuffer;

		TArray<float> InternalWindow;
		TUniquePtr<TSlidingBuffer<float>> InternalBuffer;
		TUniquePtr<FSynesthesiaSpectrumAnalyzer> SpectrumAnalyzer;
	};

	/**
	 * Defines the Spectrum analyzer and creates related classes.
	 */
	class AUDIOSYNESTHESIACORE_API FSynesthesiaSpectrumAnalysisFactory : public IAnalyzerFactory
	{
		public:

		/** Name of specific analyzer type. */
		virtual FName GetName() const override;

		/** Human readable name of analyzer. */
		virtual FString GetTitle() const override;

		/** Creates a new FSynesthesiaSpectrumResult */
		virtual TUniquePtr<IAnalyzerResult> NewResult() const override;

		/**  Creates a new FSynesthesiaSpectrumAnalysisWorker. This expects IAnalyzerNRTSettings to be a valid pointer to a FSynesthesiaSpectrumAnalysisSettings object. */
		virtual TUniquePtr<IAnalyzerWorker> NewWorker(const FAnalyzerParameters& InParams, const IAnalyzerSettings* InSettings) const override;
	};
}

