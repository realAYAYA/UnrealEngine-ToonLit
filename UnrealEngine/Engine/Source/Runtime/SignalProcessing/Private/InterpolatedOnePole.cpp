// Copyright Epic Games, Inc. All Rights Reserved.

#include "DSP/InterpolatedOnePole.h"

#include "AudioDefines.h"


namespace Audio
{

	// INTERPOLATED ONE-POLE LOW-PASS IMPLEMENTATION
	FInterpolatedLPF::FInterpolatedLPF()
	{
		Reset();
	}

	void FInterpolatedLPF::Init(float InSampleRate, int32 InNumChannels)
	{
		SampleRate = InSampleRate;
		NumChannels = InNumChannels;
		CutoffFrequency = -1.0f;
		Z1.Init(0.0f, NumChannels);
		Z1Data = Z1.GetData();
		Reset();
	}

	void FInterpolatedLPF::StartFrequencyInterpolation(const float InTargetFrequency, const int32 InInterpLength)
	{
		CurrInterpLength = InInterpLength;

		if (bIsFirstFrequencyChange)
		{
			CurrInterpLength = 0;
			bIsFirstFrequencyChange = false;
		}

		if (!FMath::IsNearlyEqual(InTargetFrequency, CutoffFrequency))
		{
			CutoffFrequency = InTargetFrequency;

			const float NormalizedFreq = FMath::Clamp(2.0f * GetCutoffFrequency() / SampleRate, 0.0f, 1.0f);
			B1Target = FMath::Exp(-PI * NormalizedFreq);
			B1Delta = (B1Target - B1Curr) / static_cast<float>(CurrInterpLength);
		}

		if (CurrInterpLength <= 1)
		{
			StopFrequencyInterpolation();
		}
	}

	void FInterpolatedLPF::ProcessAudioFrame(const float* RESTRICT InputFrame, float* RESTRICT OutputFrame)
	{
		B1Curr += B1Delta; // step forward coefficient

		/*
			[absorbing A0 coefficient]
			-----------------------------%
			Yn = Xn*A0 + B1*Z1;                <- old way
			A0 = (1-B1)

			Yn = Xn*(1-B1) + B1*Z1             <- (1 add, 1 sub, 2 mult)
			Yn = Xn - B1*Xn + B1*Z1
			Yn = Xn + B1*Z1 - B1*Xn
			Yn = Xn + B1*(Z1 - Xn)             <- (1 add, 1 sub, 1 mult)
		*/

		for (int32 i = 0; i < NumChannels; ++i)
		{
			const float InputSample = InputFrame[i];
			float Yn = InputSample + B1Curr * (Z1Data[i] - InputSample); // LPF
			Yn = UnderflowClamp(Yn);
			Z1Data[i] = Yn;
			OutputFrame[i] = Yn;
		}
	}

	void FInterpolatedLPF::ProcessAudioBuffer(const float *RESTRICT InputBuffer, float *RESTRICT OutputBuffer, const int32 NumSamples)
	{
		for (int SampleIndex = 0; SampleIndex < NumSamples; ++SampleIndex)
		{
			// cache which delay term we should be using
			const int32 ChannelIndex = SampleIndex % NumChannels;

			// step forward coefficient
			// Multiply delta by !ChannelIndex so the coefficient only accumulates at the beginning of each frame (on channel 0)
			B1Curr += B1Delta * !ChannelIndex;

			const float InputSample = InputBuffer[SampleIndex];
			float Yn = InputSample + B1Curr * (Z1Data[ChannelIndex] - InputSample); // LPF
			Yn = UnderflowClamp(Yn);
			Z1Data[ChannelIndex] = Yn;
			OutputBuffer[SampleIndex] = Yn;
		}
	}

	void FInterpolatedLPF::ProcessBufferInPlace(float* InOutBuffer, const int32 NumSamples)
	{
		for (int32 SampleIndex = 0; SampleIndex < NumSamples; ++SampleIndex)
		{
			// cache which delay term we should be using
			const int32 ChannelIndex = SampleIndex % NumChannels;

			// step forward coefficient
			// Multiply delta by !ChannelIndex so the coefficient only accumulates at the beginning of each frame (on channel 0)
			B1Curr += B1Delta * !ChannelIndex;

			const float InputSample = InOutBuffer[SampleIndex];
			float Yn = InputSample + B1Curr * (Z1Data[ChannelIndex] - InputSample); // LPF
			Yn = UnderflowClamp(Yn);
			Z1Data[ChannelIndex] = Yn;
			InOutBuffer[SampleIndex] = Yn;
		}
	}

	void FInterpolatedLPF::Reset()
	{
		B1Curr = 0.0f;
		B1Delta = 0.0f;
		B1Target = B1Curr;
		CurrInterpLength = 0;
		ClearMemory();
		Z1Data = Z1.GetData();
		bIsFirstFrequencyChange = true;
	}

	void FInterpolatedLPF::ClearMemory()
	{
		Z1.Reset();
		Z1.AddZeroed(NumChannels);
	}


	// INTERPOLATED ONE-POLE HIGH-PASS IMPLEMENTATION
	FInterpolatedHPF::FInterpolatedHPF()
	{
		Reset();
	}

	void FInterpolatedHPF::Init(float InSampleRate, int32 InNumChannels)
	{
		SampleRate = InSampleRate;
		NyquistLimit = 0.5f * SampleRate - 1.0f;
		NumChannels = InNumChannels;
		CutoffFrequency = -1.0f;
		Z1.Init(0.0f, NumChannels);
		Z1Data = Z1.GetData();
		Reset();
	}

	void FInterpolatedHPF::StartFrequencyInterpolation(const float InTargetFrequency, const int32 InterpLength)
	{
		CurrInterpLength = InterpLength;

		if (bIsFirstFrequencyChange)
		{
			CurrInterpLength = 0;
			bIsFirstFrequencyChange = false;
		}

		if (!FMath::IsNearlyEqual(InTargetFrequency, CutoffFrequency))
		{
			CutoffFrequency = FMath::Clamp(InTargetFrequency, 0.f, NyquistLimit);

			// G computation is a reduced form of the following set of equations:
			// OmegaDigital = 2.0f * PI * CutoffFrequency;
			// OmegaAnalog = 2.0f * SampleRate * Audio::FastTan(0.5f * OmegaDigital / SampleRate);
			// G = 0.5f * OmegaAnalog / SampleRate;
			const float G = Audio::FastTan(PI * GetCutoffFrequency() / SampleRate);

			A0Target = G / (1.0f + G);
			A0Delta = (A0Target - A0Curr) / static_cast<float>(CurrInterpLength);
		}

		if (CurrInterpLength <= 1)
		{
			StopFrequencyInterpolation();
		}
	}

	void FInterpolatedHPF::ProcessAudioFrame(const float* RESTRICT InputFrame, float* RESTRICT OutputFrame)
	{
		A0Curr += A0Delta; // step forward coefficient

		for (int32 i = 0; i < NumChannels; ++i)
		{
			const float InputSample = InputFrame[i];
			const float Vn = (InputSample - Z1Data[i]) * A0Curr;
			const float LPF = Vn + Z1Data[i];
			Z1Data[i] = Vn + LPF;

			OutputFrame[i] = InputSample - LPF;
		}
	}

	void FInterpolatedHPF::ProcessAudioBuffer(const float *RESTRICT InputBuffer, float *RESTRICT OutputBuffer, const int32 NumSamples)
	{
		for (int SampleIndex = 0; SampleIndex < NumSamples; ++SampleIndex)
		{
			// cache which delay term we should be using
			const int32 ChannelIndex = SampleIndex % NumChannels;

			// step forward coefficient
			// Multiply delta by !ChannelIndex so the coefficient only accumulates at the beginning of each frame (on channel 0)
			A0Curr += A0Delta * !ChannelIndex;

			const float InputSample = InputBuffer[SampleIndex];
			const float Vn = (InputSample - Z1Data[ChannelIndex]) * A0Curr;
			const float LPF = Vn + Z1Data[ChannelIndex];
			Z1Data[ChannelIndex] = Vn + LPF;

			OutputBuffer[SampleIndex] = InputSample - LPF;
		}
	}

	void FInterpolatedHPF::Reset()
	{
		A0Curr = 0.0f;
		A0Delta = 0.0f;
		CurrInterpLength = 0;
		ClearMemory();
		Z1Data = Z1.GetData();
		bIsFirstFrequencyChange = true;
	}

	void FInterpolatedHPF::ClearMemory()
	{
		Z1.Reset();
		Z1.AddZeroed(NumChannels);
	}


} // namespace Audio