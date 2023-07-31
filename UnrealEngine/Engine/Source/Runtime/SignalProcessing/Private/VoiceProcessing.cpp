// Copyright Epic Games, Inc. All Rights Reserved.

#include "DSP/VoiceProcessing.h"

#include "DSP/AudioDebuggingUtilities.h"
#include "DSP/FloatArrayMath.h"

namespace Audio
{

	FMovingAverager::FMovingAverager(uint32 NumSamples)
		: BufferCursor(0)
		, AccumulatedSum(0.0f)
	{
		AudioBuffer.Reset();
		AudioBuffer.AddZeroed(FMath::Max<uint32>(NumSamples, 1));
	}

	float FMovingAverager::ProcessInput(const float& Input, float& Output)
	{
		FScopeLock ScopeLock(&ProcessCriticalSection);

		float* BufferPtr = AudioBuffer.GetData();
		Output = BufferPtr[BufferCursor];
		BufferPtr[BufferCursor] = Input;
		BufferCursor = (BufferCursor + 1) % AudioBuffer.Num();

		// Instead of summing our entire buffer every tick, we simply add the incoming sample amplitude, and subtract the outgoing amplitude.
		// TODO: With this approach AccumulatedSum will start to slowly drift over time from accumulated rounding error. Every so often we will need to
		// reset AccumulatedSum to the actual sum of AudioBuffer.
		AccumulatedSum += (FMath::Abs(Input) - FMath::Abs(Output));

		return AccumulatedSum;
	}

	void FMovingAverager::SetNumSamples(uint32 NumSamples)
	{
		FScopeLock ScopeLock(&ProcessCriticalSection);

		AudioBuffer.Reset();
		AudioBuffer.AddZeroed(NumSamples);
		AccumulatedSum = 0.0f;
	}

	FMovingVectorAverager::FMovingVectorAverager(uint32 NumSamples)
		: BufferCursor(0)
		, AccumulatedSum(VectorZero())
	{
		checkf(NumSamples % 4 == 0, TEXT("NumSamples must be divisible by 4!"));
		const uint32 NumVectors = NumSamples / 4;
		AudioBuffer.Reset();
		AudioBuffer.AddZeroed(NumVectors);
	}

	float FMovingVectorAverager::ProcessAudio(const VectorRegister4Float& Input, VectorRegister4Float& Output)
	{
		VectorRegister4Float* BufferPtr = AudioBuffer.GetData();
		Output = BufferPtr[BufferCursor];
		BufferPtr[BufferCursor] = Input;
		BufferCursor = (BufferCursor + 1) % AudioBuffer.Num();

		// Instead of summing our entire buffer every tick, we simply add the incoming sample amplitude, and subtract the outgoing amplitude.
		// TODO: With this approach AccumulatedSum will start to slowly drift over time from accumulated rounding error. Every so often we will need to
		// reset AccumulatedSum to the actual sum of AudioBuffer.
		const VectorRegister4Float AbsInput = VectorAbs(Input);
		const VectorRegister4Float AbsOutput = VectorAbs(Output);
		const VectorRegister4Float TotalAccumulation = VectorSubtract(AbsInput, AbsOutput);
		AccumulatedSum = VectorAdd(AccumulatedSum, TotalAccumulation);

		alignas(16) float PartionedSums[4];
		VectorStore(AccumulatedSum, PartionedSums);

		return (PartionedSums[0] + PartionedSums[1] + PartionedSums[2] + PartionedSums[3]) / (AudioBuffer.Num() * 4);
	}

	FSilenceDetection::FSilenceDetection(float InOnsetThreshold, float InReleaseThreshold,  int32 AttackDurationInSamples, int32 ReleaseDurationInSamples)
		: Averager(AttackDurationInSamples)
		, ReleaseTau(FMath::Exp(-1.0f / (ReleaseDurationInSamples / 4)))
		, OnsetThreshold(InOnsetThreshold)
		, ReleaseThreshold(InReleaseThreshold)
		, CurrentAmplitude(0.0f)
		, bOnsetWasInLastBuffer(false)
	{
		ensureMsgf(InOnsetThreshold > InReleaseThreshold, TEXT("The onset threshold should always be louder than the release threshold."));
	}

	int32 FSilenceDetection::ProcessBuffer(const float* InAudio, float* OutAudio, int32 NumSamples)
	{
		if (bOnsetWasInLastBuffer || CurrentAmplitude > ReleaseThreshold)
		{
			bOnsetWasInLastBuffer = false;

			// If we've been outputting audio up until the start of this callback, we are going to continue outputting audio
			// Until the end of this buffer. If the current amplitude is below our threshold at the end of this callback, we will
			// stop outputting audio then.
			for (int32 InSampleIndex = 0; InSampleIndex < NumSamples; InSampleIndex += 4)
			{
				const VectorRegister4Float InputVector = VectorLoad(&InAudio[InSampleIndex]);
				VectorRegister4Float OutputVector;
				float InstantaneousAmplitude = Averager.ProcessAudio(InputVector, OutputVector);
				CurrentAmplitude = ReleaseTau * (CurrentAmplitude - InstantaneousAmplitude) + InstantaneousAmplitude;
				VectorStore(OutputVector, &OutAudio[InSampleIndex]);
			}

			// If we are releasing back to silence at the end of this buffer callback, we perform a short fadeout here.
			if (CurrentAmplitude < ReleaseThreshold)
			{
				static const int32 DefaultNumSamplesToFadeOutOver = 32;
				const int32 NumSamplesToFadeOutOver = FMath::Min(NumSamples, DefaultNumSamplesToFadeOutOver);

				const int32 Offset = NumSamples - NumSamplesToFadeOutOver;
				TArrayView<float> OutAudioView(&OutAudio[Offset], NumSamplesToFadeOutOver);
				Audio::ArrayFade(OutAudioView, 1.0f, 0.0f);
			}

			return NumSamples;
		}
		else
		{
			// If we started this callback in a silent state, we simply buffer audio until we've detected an onset,
			// At which point we begin outputting audio from the Averager.
			int32 OutSampleIndex = 0;
			bool bHitThreshold = false;
			float InstantaneousAmplitude = 0.0f;
			for (int32 InSampleIndex = 0; InSampleIndex < NumSamples; InSampleIndex += 4)
			{
				const VectorRegister4Float InputVector = VectorLoad(&InAudio[InSampleIndex]);
				VectorRegister4Float OutputVector;
				InstantaneousAmplitude = Averager.ProcessAudio(InputVector, OutputVector);

				if (bHitThreshold)
				{	
					VectorStore(OutputVector, &OutAudio[OutSampleIndex]);
					OutSampleIndex += 4;
				}
				else
				{
					bHitThreshold = InstantaneousAmplitude > OnsetThreshold;
				}
			}

			CurrentAmplitude = InstantaneousAmplitude;
			bOnsetWasInLastBuffer = bHitThreshold;
			check(CurrentAmplitude < 100.0f);
			return OutSampleIndex;
		}
	}

	void FSilenceDetection::SetThreshold(float InThreshold)
	{
		OnsetThreshold = InThreshold;
	}

	float FSilenceDetection::GetCurrentAmplitude()
	{
		return CurrentAmplitude;
	}

	FSlowAdaptiveGainControl::FSlowAdaptiveGainControl(float InGainTarget, int32 InAdaptiveRate, float InGainMin /*= 0.5f*/, float InGainMax /*= 2.0f*/)
		: PeakDetector(InAdaptiveRate)
		, GainTarget(InGainTarget)
		, PreviousGain(1.0f)
		, GainMin(InGainMin)
		, GainMax(InGainMax)
	{
	}

	float FSlowAdaptiveGainControl::ProcessAudio(float* InAudio, int32 NumSamples, float InAmplitude)
	{
		float PeakDetectorOutput = 0.0f; // unused
		const float EstimatedPeak = PeakDetector.ProcessInput(InAmplitude, PeakDetectorOutput);
		const float TargetGain = GetTargetGain(EstimatedPeak);
		TArrayView<float> InAudioView(InAudio, NumSamples);
		Audio::ArrayFade(InAudioView, PreviousGain, TargetGain);
		PreviousGain = TargetGain;

		return TargetGain;
	}

	void FSlowAdaptiveGainControl::SetAdaptiveRate(int32 InAdaptiveRate)
	{
		PeakDetector.SetNumSamples(InAdaptiveRate);
	}

	float FSlowAdaptiveGainControl::GetTargetGain(float InAmplitude)
	{
		const float UnclampedGain = GainTarget / InAmplitude;
		return FMath::Clamp(UnclampedGain, GainMin, GainMax);
	}

	FAdaptiveFilter_DEPRECATED::FAdaptiveFilter_DEPRECATED(int32 FilterLength, int32 AudioCallbackSize)
		: WindowSize(FilterLength)
		, CurrentStepsUntilConvergence(0)
	{
		const int32 FFTSize = FFTIntrinsics::NextPowerOf2(FilterLength * 2 - 1);

		//Initialize our weights to 0:
		WeightDeltas.InitZeroed(FFTSize);
		CurrentWeights.InitZeroed(FFTSize);
		InputFrequencies.InitZeroed(FFTSize);

		// For passthrough, we initialize our current weights to 1:
		for (int32 Index = 0; Index < CurrentWeights.Real.Num(); Index++)
		{
			CurrentWeights.Real[Index] = 1.0f;
		}

		Convolver.SetFilter(CurrentWeights, FilterLength);
	}

	void FAdaptiveFilter_DEPRECATED::AdaptFilter()
	{
		// If we've suitably converged, we avoid incrementing our weights.
		if (CurrentStepsUntilConvergence <= 0)
		{
			return;
		}

		IncrementWeights();
		CurrentStepsUntilConvergence--;
		Convolver.SetFilter(CurrentWeights, WindowSize);
	}

	void FAdaptiveFilter_DEPRECATED::SetWeightDeltas(const float* InWeightsReal, const float* InWeightsImag, int32 NumWeights, float InLearningRate)
	{
		// ProcessAudio::NumWeights needs to use the same 
		checkSlow(NumWeights == WeightDeltas.Real.Num());

		// If NumWeights is not divisible by four, something is terribly wrong, since we are using an FFT with this.
		checkSlow(NumWeights % 4 == 0);
		const float* CurrentRealBuffer = CurrentWeights.Real.GetData();
		const float* CurrentImagBuffer = CurrentWeights.Imag.GetData();

		float* DeltaRealBuffer = WeightDeltas.Real.GetData();
		float* DeltaImagBuffer = WeightDeltas.Imag.GetData();

		// Our convergence rate is 1 / our NumConvergence steps.
		const VectorRegister4Float ConvergenceRate = VectorSetFloat1(InLearningRate);
		CurrentStepsUntilConvergence = FMath::CeilToInt(1.0f / InLearningRate);
		
		for (int32 WeightIndex = 0; WeightIndex < NumWeights; WeightIndex+= 4)
		{
			VectorRegister4Float TargetReal = VectorLoad(&InWeightsReal[WeightIndex]);
			VectorRegister4Float CurrentReal = VectorLoad(&CurrentRealBuffer[WeightIndex]);
			const VectorRegister4Float DeltaReal = VectorMultiply(VectorSubtract(TargetReal, CurrentReal), ConvergenceRate);
			VectorStore(DeltaReal, &DeltaRealBuffer[WeightIndex]);

			VectorRegister4Float TargetImag = VectorLoad(&InWeightsImag[WeightIndex]);
			VectorRegister4Float CurrentImag = VectorLoad(&CurrentImagBuffer[WeightIndex]);
			const VectorRegister4Float DeltaImag = VectorMultiply(VectorSubtract(TargetImag, CurrentImag), ConvergenceRate);
			VectorStore(DeltaImag, &DeltaImagBuffer[WeightIndex]);
		}
	}

	void FAdaptiveFilter_DEPRECATED::IncrementWeights()
	{
		const int32 NumWeights = CurrentWeights.Real.Num();
		float* CurrentRealBuffer = CurrentWeights.Real.GetData();
		float* CurrentImagBuffer = CurrentWeights.Imag.GetData();

		const float* DeltasRealBuffer = WeightDeltas.Real.GetData();
		const float* DeltasImagBuffer = WeightDeltas.Imag.GetData();

		for (int32 WeightIndex = 0; WeightIndex < NumWeights; WeightIndex += 4)
		{
			VectorRegister4Float DeltaReal = VectorLoad(&DeltasRealBuffer[WeightIndex]);
			VectorRegister4Float CurrentReal = VectorLoad(&CurrentRealBuffer[WeightIndex]);
			CurrentReal = VectorAdd(CurrentReal, DeltaReal);
			VectorStore(CurrentReal, &CurrentRealBuffer[WeightIndex]);

			VectorRegister4Float DeltaImag = VectorLoad(&DeltasImagBuffer[WeightIndex]);
			VectorRegister4Float CurrentImag = VectorLoad(&CurrentImagBuffer[WeightIndex]);
			CurrentImag = VectorAdd(CurrentImag, DeltaImag);
			VectorStore(CurrentImag, &CurrentImagBuffer[WeightIndex]);
		}
	}

	void FAdaptiveFilter_DEPRECATED::ProcessAudio(float* InAudio, int32 NumSamples)
	{
		AdaptFilter();
		Convolver.ProcessAudio(InAudio, NumSamples);
	}

	void FAdaptiveFilter_DEPRECATED::SetWeights(const FrequencyBuffer& InFilterWeights, int32 FilterLength, float InLearningRate)
	{
		SetWeightDeltas(InFilterWeights.Real.GetData(), InFilterWeights.Imag.GetData(), InFilterWeights.Real.Num(), InLearningRate);
	}

	FFDAPFilterComputer_DEPRECATED::FFDAPFilterComputer_DEPRECATED()
	{

	}

	void FFDAPFilterComputer_DEPRECATED::GenerateWeights(const float* IncomingSignal, int32 NumIncomingSamples, const float* OutgoingSignal, int32 NumOutgoingSamples, FrequencyBuffer& OutWeights)
	{
		int32 FFTSize = FFTIntrinsics::NextPowerOf2(NumIncomingSamples + NumOutgoingSamples - 1);
		
		// zero pad inputs appropriately:
		ZeroPaddedIncomingBuffer.Reset();
		ZeroPaddedIncomingBuffer.AddZeroed(FFTSize);
		FMemory::Memcpy(ZeroPaddedIncomingBuffer.GetData(), IncomingSignal, NumIncomingSamples * sizeof(float));

		ZeroPaddedOutgoingBuffer.Reset();
		ZeroPaddedOutgoingBuffer.AddZeroed(FFTSize);
		FMemory::Memcpy(ZeroPaddedOutgoingBuffer.GetData(), OutgoingSignal, NumOutgoingSamples * sizeof(float));

		int32 FilterLength = FMath::Max(NumIncomingSamples, NumOutgoingSamples);
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		CrossCorrelate(ZeroPaddedIncomingBuffer.GetData(), ZeroPaddedOutgoingBuffer.GetData(), FilterLength, FFTSize, IncomingFrequencies, OutgoingFrequencies, OutWeights);
PRAGMA_ENABLE_DEPRECATION_WARNINGS

		// TODO: Perform linear phase conversion on OutWeights.

	}

	FAcousticEchoCancellation_DEPRECATED::FAcousticEchoCancellation_DEPRECATED(float InConvergenceRate, int32 CallbackSize, int32 InFilterLength, int32 InFilterUpdateRate /*= 1*/)
		: AdaptiveFilter(InFilterLength, CallbackSize)
		, ConvergenceRate(InConvergenceRate)
		, FilterLength(InFilterLength)
		, FilterUpdateRate(InFilterUpdateRate)
		, FitlerUpdateCounter(InFilterUpdateRate)
	{
		checkSlow(FMath::IsPowerOfTwo(FilterLength));
	}

	void FAcousticEchoCancellation_DEPRECATED::ProcessAudio(float* InAudio, int32 NumSamples)
	{
		checkSlow(FMath::IsPowerOfTwo(NumSamples));

		int32 NumBlocks = NumSamples / FilterLength;
		for (int32 Index = 0; Index < NumBlocks; Index++)
		{
			float* BlockAudio = &InAudio[Index * FilterLength];

			if (FitlerUpdateCounter == 0)
			{
				FilterComputerInput.Reset();
				FilterComputerInput.AddZeroed(FilterLength);

				// Since we are likely sampling audio buffers much less often than we are rendering them,
				// we always skip ahead to the latest audio to compute our filter coefficients.
				const bool bUseLatestAudio = true;

				// Grab our audio from our taps:
				int32 PopResult = PatchMixer.PopAudio(FilterComputerInput.GetData(), FilterLength, bUseLatestAudio);

				if (PopResult != 0)
				{
					// Compute our new filter target and pass it to the adaptive filter:
					FilterComputer.GenerateWeights(FilterComputerInput.GetData(), FilterLength, BlockAudio, FilterLength, FilterComputerOutput);
					AdaptiveFilter.ProcessAudio(BlockAudio, NumSamples);
					AdaptiveFilter.SetWeights(FilterComputerOutput, FilterLength, ConvergenceRate);
				}
				else
				{
					// If our taps were empty, don't both updating the filter.
					AdaptiveFilter.ProcessAudio(BlockAudio, FilterLength);
				}

				FitlerUpdateCounter = FilterUpdateRate - 1;
			}
			else
			{
				AdaptiveFilter.ProcessAudio(BlockAudio, FilterLength);
				FitlerUpdateCounter--;
			}
		}
	}

	Audio::FPatchInput FAcousticEchoCancellation_DEPRECATED::AddNewSignalPatch(int32 ExpectedLatency, float Gain /*= 1.0f*/)
	{
		return PatchMixer.AddNewInput(ExpectedLatency, Gain);
	}

	void FAcousticEchoCancellation_DEPRECATED::RemoveSignalPatch(const FPatchInput& Patch)
	{
		PatchMixer.RemovePatch(Patch);
	}
}
