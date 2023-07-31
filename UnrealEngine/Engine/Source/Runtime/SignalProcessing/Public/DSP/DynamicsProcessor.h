// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DSP/EnvelopeFollower.h"
#include "DSP/Delay.h"
#include "Filter.h"

namespace Audio
{
	// What mode the compressor is in
	namespace EDynamicsProcessingMode
	{
		enum Type
		{
			Compressor,
			Limiter,
			Expander,
			Gate,
			UpwardsCompressor,
			Count
		};
	}

	enum class EDynamicsProcessorChannelLinkMode : uint8
	{
		Disabled,
		Average,
		Peak,
		Count
	};

	// Dynamic range compressor
	// https://en.wikipedia.org/wiki/Dynamic_range_compression
	class SIGNALPROCESSING_API FDynamicsProcessor
	{
	public:
		FDynamicsProcessor();
		~FDynamicsProcessor();

		void Init(const float InSampleRate, const int32 InNumChannels = 2);

		int32 GetNumChannels() const;
		int32 GetKeyNumChannels() const;
		float GetMaxLookaheadMsec() const;

		void SetLookaheadMsec(const float InLookAheadMsec);
		void SetAttackTime(const float InAttackTimeMsec);
		void SetReleaseTime(const float InReleaseTimeMsec);
		void SetThreshold(const float InThresholdDb);
		void SetRatio(const float InCompressionRatio);
		void SetKneeBandwidth(const float InKneeBandwidthDb);
		void SetInputGain(const float InInputGainDb);
		void SetKeyAudition(const bool InAuditionEnabled);
		void SetKeyGain(const float InKeyGain);
		void SetKeyHighshelfCutoffFrequency(const float InCutoffFreq);
		void SetKeyHighshelfEnabled(const bool bInEnabled);
		void SetKeyHighshelfGain(const float InGainDb);
		void SetKeyLowshelfCutoffFrequency(const float InCutoffFreq);
		void SetKeyLowshelfEnabled(const bool bInEnabled);
		void SetKeyLowshelfGain(const float InGainDb);
		void SetKeyNumChannels(const int32 InNumChannels);
		void SetNumChannels(const int32 InNumChannels);
		void SetOutputGain(const float InOutputGainDb);
		void SetChannelLinkMode(const EDynamicsProcessorChannelLinkMode InLinkMode);
		void SetAnalogMode(const bool bInIsAnalogMode);
		void SetPeakMode(const EPeakMode::Type InEnvelopeFollowerModeType);
		void SetProcessingMode(const EDynamicsProcessingMode::Type ProcessingMode);

		void ProcessAudioFrame(const float* InFrame, float* OutFrame, const float* InKeyFrame);
		void ProcessAudioFrame(const float* InFrame, float* OutFrame, const float* InKeyFrame, float* OutGain);
		void ProcessAudio(const float* InBuffer, const int32 InNumSamples, float* OutBuffer, const float* InKeyBuffer = nullptr);
		void ProcessAudio(const float* InBuffer, const int32 InNumSamples, float* OutBuffer, const float* InKeyBuffer, float* OutEnvelope);
		 

	protected:

		float ComputeGain(const float InEnvFollowerDb);

		// Process key frame, returning true if should continue processing
		// (Returns false in audition mode and writes straight to output).
		bool ProcessKeyFrame(const float* InKeyFrame, float* OutFrame, bool bKeyIsInput);

		bool IsInProcessingThreshold(const float InEnvFollowerDb) const;

		// (Optional) Low-pass filter for input signal
		FBiquadFilter InputLowshelfFilter;

		// (Optional) High-pass filter for input signal
		FBiquadFilter InputHighshelfFilter;

		EDynamicsProcessingMode::Type ProcessingMode;

		// Peak mode of envelope followers
		EPeakMode::Type EnvelopeFollowerPeakMode;

		// Lookahead delay lines
		TArray<FDelay> LookaheadDelay;

		// Envelope followers
		TArray<FInlineEnvelopeFollower> EnvFollower;

		// Points in the knee used for lagrangian interpolation
		TArray<FVector2D> KneePoints;

		// Channel values of cached detector sample
		TArray<float> DetectorOuts;

		// Channel values of cached gain sample
		TArray<float> Gain;

		// How far ahead to look in the audio
		float LookaheedDelayMsec;

		// The period of which the compressor decreases gain to the level determined by the compression ratio
		float AttackTimeMsec;

		// The period of which the compressor increases gain to 0 dB once level has fallen below the threshold
		float ReleaseTimeMsec;

		// Amplitude threshold above which gain will be reduced
		float ThresholdDb;

		// Amount of gain reduction
		float Ratio;

		// Defines how hard or soft the gain reduction blends from no gain reduction to gain reduction (determined by the ratio)
		float HalfKneeBandwidthDb;

		// Amount of input gain
		float InputGain;

		// Amount of output gain
		float OutputGain;

		// Gain of key detector signal in dB
		float KeyGain;

		// Sample rate of both key and input (must match)
		float SampleRate;

		// Whether or not input channels are linked, and if so, how to calculate gain
		EDynamicsProcessorChannelLinkMode LinkMode;

		// Whether or not we're in analog mode
		bool bIsAnalogMode;

		// Whether or not to bypass processor and only output key modulator
		bool bKeyAuditionEnabled;

		// Whether or not key high-pass filter is enabled
		bool bKeyHighshelfEnabled;

		// Whether or not key low-pass filter is enabled
		bool bKeyLowshelfEnabled;

		static constexpr float UpwardsCompressionMaxGain = 36.0f;

		static constexpr float MaxLookaheadMsec = 100.0f;
	};
}
