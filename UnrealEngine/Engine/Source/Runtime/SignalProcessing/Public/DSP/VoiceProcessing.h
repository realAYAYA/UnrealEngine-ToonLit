// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DSP/Delay.h"
#include "DSP/AudioFFT.h"
#include "DSP/MultithreadedPatching.h"

namespace Audio
{
	/**
	 * This class buffers audio while maintaining a running average of the underlying buffer.
	 * This is useful for cases where we can't use a peak detector with asymptotic tracking.
	 * For example: lookahead limiters, silence detection, etc.
	 */
	class FMovingAverager
	{
	public:
		// Delay length in samples.
		SIGNALPROCESSING_API FMovingAverager(uint32 NumSamples);

		// Returns average amplitude across the internal buffer, and fills Output with the delay line output.
		SIGNALPROCESSING_API float ProcessInput(const float& Input, float& Output);

		SIGNALPROCESSING_API void SetNumSamples(uint32 NumSamples);

	private:
		FMovingAverager();

		TArray<float> AudioBuffer;
		int32 BufferCursor;

		float AccumulatedSum;

		// Contended by ProcessInput and SetNumSamples.
		FCriticalSection ProcessCriticalSection;
	};

	/**
	 * Vectorized version of FMovingAverager.
	 */
	class FMovingVectorAverager
	{
	public:
		// Delay length in samples. NumSamples must be divisible by four.
		SIGNALPROCESSING_API FMovingVectorAverager(uint32 NumSamples);

		// Returns average amplitude across the internal buffer, and fills Output with the delay line output.
		SIGNALPROCESSING_API float ProcessAudio(const VectorRegister4Float& Input, VectorRegister4Float& Output);

	private:
		FMovingVectorAverager();

		TArray<VectorRegister4Float> AudioBuffer;
		int32 BufferCursor;

		VectorRegister4Float AccumulatedSum;

		// Contended by ProcessInput and SetNumSamples.
		FCriticalSection ProcessCriticalSection;
	};

	/**
	 * This object will return buffered audio while the input signal is louder than the specified threshold,
	 * and buffer audio when the input signal otherwise.
	 */
	class FSilenceDetection
	{
	public:
		// InOnsetThreshold is the minimum amplitude of a signal before we begin outputting audio, in linear gain.
		// InReleaseThreshold is the amplitude of the signal before we stop outputting audio, in linear gain.
		// AttackDurationInSamples is the amount of samples we average over when calculating our amplitude when the in audio is below the threshold.
		// ReleaseDurationInSamples is the amount of samples we average over when calculating our amplitude when the input audio is above the threshold.
		SIGNALPROCESSING_API FSilenceDetection(float InOnsetThreshold, float InReleaseThreshold, int32 AttackDurationInSamples, int32 ReleaseDurationInSamples);

		// Buffers InAudio and renders any non-silent audio to OutAudio. Returns the number of samples written to OutAudio.
		// The number of samples returned will only be less than NumSamples if the signal becomes audible mid-buffer.
		// We do not return partial buffers when returning from an audible state to a silent state.
		// This should also work in place, i.e. if InAudio == OutAudio.
		SIGNALPROCESSING_API int32 ProcessBuffer(const float* InAudio, float* OutAudio, int32 NumSamples);

		// Set the threshold of audibility, in terms of linear gain.
		SIGNALPROCESSING_API void SetThreshold(float InThreshold);

		// Returns the current estimate of the current amplitude of the input signal, in linear gain.
		SIGNALPROCESSING_API float GetCurrentAmplitude();
	private:
		FSilenceDetection();

		FMovingVectorAverager Averager;
		float ReleaseTau;
		float OnsetThreshold;
		float ReleaseThreshold;
		float CurrentAmplitude;
		bool bOnsetWasInLastBuffer;
	};

	/**
	 * This object accepts an input buffer and current amplitude estimate of that input buffer,
	 * Then applies a computed gain target. Works like a standard feed forward limiter, with a threshold of 0.
	 */
	class FSlowAdaptiveGainControl
	{
	public:
		// InGainTarget is our target running linear gain.
		// InAdaptiveRate is the time it will take to respond to changes in amplitude, in numbers of buffer callbacks.
		// InGainMin is the most we will attenuate the input signal.
		// InGainMax is the most we will amplify the input signal.
		SIGNALPROCESSING_API FSlowAdaptiveGainControl(float InGainTarget, int32 InAdaptiveRate, float InGainMin = 0.01f, float InGainMax = 2.0f);
		
		// Takes an amplitude estimate and an input buffer, and attenuates InAudio based on it.
		// Returns the linear gain applied to InAudio.
		SIGNALPROCESSING_API float ProcessAudio(float* InAudio, int32 NumSamples, float InAmplitude);

		// Sets the responsiveness of the adaptive gain control, in number of buffer callbacks.
		SIGNALPROCESSING_API void SetAdaptiveRate(int32 InAdaptiveRate);

	private:
		FSlowAdaptiveGainControl();

		float GetTargetGain(float InAmplitude);

		FMovingAverager PeakDetector;
		float GainTarget;
		float PreviousGain;
		float GainMin;
		float GainMax;
	};
	
	/** VoiceProcessing.h Deprecation
	 *
	 * Several classes in VoiceProcessing are deprecated due to lack of support
	 * and lack of need. Current voice processing solutions are available through 
	 * EOS and WebRTC. 
	 */
	
	/**
	 * This filter takes a precomputed set of FIR weights in the frequency domain, and linearly converges to it.
	 * If no new weights are given and we've converged to our previous input weights, this works like a normal FFT-based FIR filter.
	 * Convergence is non-asymptotic: if no new weights are given after our number of steps until convergence, our filter is using the exact weights given.
	 */
	class FAdaptiveFilter_DEPRECATED // Deprecated in 5.1
	{
	public:
		SIGNALPROCESSING_API FAdaptiveFilter_DEPRECATED(int32 FilterLength, int32 AudioCallbackSize);

		/*
		* Applies current filter to InAudio in-place. If there is a new target set of weights, they can be input below.
		*/
		SIGNALPROCESSING_API void ProcessAudio(float* InAudio, int32 NumSamples);

		SIGNALPROCESSING_API void SetWeights(const FrequencyBuffer& InFilterWeights, int32 FilterLength, float InLearningRate);

	private:

		FAdaptiveFilter_DEPRECATED();

		/** This is called every process callback to (A) reset our weight deltas if we have a new target, and (B) increment our current filter weights if we haven't converged yet. */
		void AdaptFilter();

		/*
		 * Recomputes our weight deltas. Called in the ProcessAudio callback when ProcessAudio was called with a new set of weights.
		 * Delta is computed 
		*/
		void SetWeightDeltas(const float* InWeightsReal, const float* InWeightsImag, int32 NumWeights, float InLearningRate);
		void IncrementWeights();

		FrequencyBuffer WeightDeltas;
		FrequencyBuffer CurrentWeights;
		int32 WindowSize;

		FrequencyBuffer InputFrequencies;

		int32 CurrentStepsUntilConvergence;

		FFFTConvolver_DEPRECATED Convolver;
	};

	class UE_DEPRECATED(5.1, "FAdaptiveFilter will no longer be supported.") FAdaptiveFilter;
	class FAdaptiveFilter : public FAdaptiveFilter_DEPRECATED
	{
		public:
			using FAdaptiveFilter_DEPRECATED::FAdaptiveFilter_DEPRECATED;
	};

	/**
	 * This class takes an incoming signal and an outgoing signal, Correlates them, and returns the frequency values of the weight targets to pass to an adaptive filter.  
	 */
	class FFDAPFilterComputer_DEPRECATED // Deprecated in 5.1
	{
	public:
		SIGNALPROCESSING_API FFDAPFilterComputer_DEPRECATED();

		SIGNALPROCESSING_API void GenerateWeights(const float* IncomingSignal, int32 NumIncomingSamples, const float* OutgoingSignal, int32 NumOutgoingSamples, FrequencyBuffer& OutWeights);

	private:
		FrequencyBuffer IncomingFrequencies;
		FrequencyBuffer OutgoingFrequencies;

		FAlignedFloatBuffer ZeroPaddedIncomingBuffer;
		FAlignedFloatBuffer ZeroPaddedOutgoingBuffer;
	};

	class UE_DEPRECATED(5.1, "FAFDAPFFilterComputer will no longer be supported.") FFDAPFilterComputer;
	class FFDAPFilterComputer : public FFDAPFilterComputer_DEPRECATED 
	{
	};

	/*
	 * This class uses an adaptive filter to cancel out any rendered audio signal that might be picked up by the mic.
	 * To add a new patch to a rendered audio signal, user AddNewSignalPatch. See FPatchInput for how to push audio.
	 * ProcessAudio then filters the microphone signal accordingly.
	 */
	class FAcousticEchoCancellation_DEPRECATED // Deprecated in 5.1
	{
	public:
		/**
		 * Convergence Rate should be a number between 0 and 1. The higher the number, the quicker the adaptive filter reacts. 
		 */
		SIGNALPROCESSING_API FAcousticEchoCancellation_DEPRECATED(float InConvergenceRate, int32 CallbackSize, int32 InFilterLength, int32 InFilterUpdateRate = 1);

		/** Callback function for outgoing audio signal. This is where the filter is applied, and the bulk of the DSP work takes place. */
		SIGNALPROCESSING_API void ProcessAudio(float* InAudio, int32 NumSamples);

		/** This is how any signal that may be picked up by the microphone may be added to the echo cancellation here: */
		SIGNALPROCESSING_API FPatchInput AddNewSignalPatch(int32 ExpectedLatency, float Gain = 1.0f);
		SIGNALPROCESSING_API void RemoveSignalPatch(const FPatchInput& Patch);

	private:
		FPatchMixer PatchMixer;
		FFDAPFilterComputer_DEPRECATED FilterComputer;
		FAdaptiveFilter_DEPRECATED AdaptiveFilter;
		
		FAlignedFloatBuffer FilterComputerInput;
		FrequencyBuffer FilterComputerOutput;
		float ConvergenceRate;
		int32 FilterLength;
		int32 FilterUpdateRate;
		int32 FitlerUpdateCounter;
	};

	class UE_DEPRECATED(5.1, "FAcousticEchoCancellation will no longer be supported.") FAcousticEchoCancellation;
	class FAcousticEchoCancellation : public FAcousticEchoCancellation_DEPRECATED
	{
		public:
			using FAcousticEchoCancellation_DEPRECATED::FAcousticEchoCancellation_DEPRECATED;
	};
}
