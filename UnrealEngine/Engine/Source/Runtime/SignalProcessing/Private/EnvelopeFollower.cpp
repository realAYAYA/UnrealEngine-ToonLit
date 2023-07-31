// Copyright Epic Games, Inc. All Rights Reserved.

#include "DSP/EnvelopeFollower.h"
#include "DSP/FloatArrayMath.h"

namespace Audio
{
	namespace EnvelopeFollowerPrivate
	{
		FORCEINLINE float SmoothSample(float InSample, float InPriorSmoothedSample, float InAttackSamples, float InReleaseSamples)
		{
			float Value = InPriorSmoothedSample;
			float Diff = Value - InSample;

			if (Diff <= 0.f)
			{
				Value = (InAttackSamples * Diff) + InSample;
			}
			else
			{
				Value = (InReleaseSamples * Diff) + InSample;
			}

			return Audio::UnderflowClamp(Value);
		}
	}

	FAttackRelease::FAttackRelease(float InSampleRate, float InAttackTimeMsec, float InReleaseTimeMsec, bool bInIsAnalog)
	: SampleRate(InSampleRate)
	, AttackTimeSamples(1.f)
	, ReleaseTimeSamples(1.f)
	, bIsAnalog(bInIsAnalog)
	{
		if (!ensure(SampleRate > 0.f))
		{
			SampleRate = 48000.f;
		}
		// Set the attack and release times using the default values
		SetAttackTime(InAttackTimeMsec);
		SetReleaseTime(InReleaseTimeMsec);
	}

	void FAttackRelease::SetSampleRate(float InSampleRate)
	{
		if (ensure(InSampleRate > 0.f))
		{
			SampleRate = InSampleRate;
			SetAttackTime(AttackTimeMsec);
			SetReleaseTime(ReleaseTimeMsec);
		}
	}

	void FAttackRelease::SetAnalog(bool bInIsAnalog)
	{
		bIsAnalog = bInIsAnalog;
		SetAttackTime(AttackTimeMsec);
		SetReleaseTime(ReleaseTimeMsec);
	}

	void FAttackRelease::SetAttackTime(float InAttackTimeMsec)
	{
		AttackTimeMsec = InAttackTimeMsec;
		if (AttackTimeMsec > 0.f)
		{
			float TimeConstant = bIsAnalog ? AnalogTimeConstant : DigitalTimeConstant;
			AttackTimeSamples = FMath::Exp(-1000.0f * TimeConstant / (AttackTimeMsec * SampleRate));
		}
		else
		{
			AttackTimeSamples = 0.f;
		}
	}

	void FAttackRelease::SetReleaseTime(float InReleaseTimeMsec)
	{
		ReleaseTimeMsec = InReleaseTimeMsec;
		if (ReleaseTimeMsec > 0.f)
		{
			float TimeConstant = bIsAnalog ? AnalogTimeConstant : DigitalTimeConstant;
			ReleaseTimeSamples = FMath::Exp(-1000.0f * TimeConstant / (ReleaseTimeMsec * SampleRate));
		}
		else
		{
			ReleaseTimeSamples = 0.f;
		}
	}

	FAttackReleaseSmoother::FAttackReleaseSmoother(float InSampleRate, int32 InNumChannels, float InAttackTimeMsec, float InReleaseTimeMsec, bool bInIsAnalog)
	: FAttackRelease(InSampleRate, InAttackTimeMsec, InReleaseTimeMsec, bInIsAnalog)
	, NumChannels(0)
	{
		SetNumChannels(InNumChannels);
	}

	void FAttackReleaseSmoother::ProcessAudio(const float* InBuffer, int32 InNumFrames)
	{
		if (InNumFrames > 0)
		{
			const int32 NumSamples = InNumFrames * NumChannels;
			const float AttackSamples = GetAttackTimeSamples();
			const float ReleaseSamples = GetReleaseTimeSamples();

			for (int32 Channel = 0; Channel < NumChannels; Channel++)
			{
				float EnvelopeValue = PriorEnvelopeValues[Channel];

				for (int32 Pos = Channel; Pos < NumSamples; Pos += NumChannels)
				{
					EnvelopeValue = EnvelopeFollowerPrivate::SmoothSample(InBuffer[Pos], EnvelopeValue, AttackSamples, ReleaseSamples);
				}

				PriorEnvelopeValues[Channel] = EnvelopeValue;
			}
		}
	}

	void FAttackReleaseSmoother::ProcessAudio(const float* InBuffer, int32 InNumFrames, float* OutBuffer)
	{
		if (InNumFrames > 0)
		{
			const int32 NumSamples = InNumFrames * NumChannels;
			const float AttackSamples = GetAttackTimeSamples();
			const float ReleaseSamples = GetReleaseTimeSamples();

			for (int32 Channel = 0; Channel < NumChannels; Channel++)
			{
				float EnvelopeValue = PriorEnvelopeValues[Channel];

				for (int32 Pos = Channel; Pos < NumSamples; Pos += NumChannels)
				{
					EnvelopeValue = EnvelopeFollowerPrivate::SmoothSample(InBuffer[Pos], EnvelopeValue, AttackSamples, ReleaseSamples);
					OutBuffer[Pos] = EnvelopeValue;
				}

				PriorEnvelopeValues[Channel] = EnvelopeValue;
			}
		}
	}

	const TArray<float>& FAttackReleaseSmoother::GetEnvelopeValues() const
	{
		return PriorEnvelopeValues;
	}
	void FAttackReleaseSmoother::SetNumChannels(int32 InNumChannels)
	{
		if (InNumChannels != NumChannels)
		{
			PriorEnvelopeValues.Reset(InNumChannels);
			if (ensure(InNumChannels > 0))
			{
				PriorEnvelopeValues.AddZeroed(InNumChannels);
			}
			NumChannels = InNumChannels;
		}
	}

	void FAttackReleaseSmoother::Reset()
	{
		if (NumChannels > 0)
		{
			FMemory::Memset(PriorEnvelopeValues.GetData(), 0, sizeof(float) * NumChannels);
		}
	}


	FMeanSquaredFIR::FMeanSquaredFIR(float InSampleRate, int32 InNumChannels, float InWindowTimeMsec)
	: SampleRate(InSampleRate)
	, NumChannels(InNumChannels)
	, WindowTimeSamples(256)
	, NormFactor(1.f)
	{
		if (!ensure(SampleRate > 0.f))
		{
			SampleRate = 48000.f;
		}

		if (ensure(NumChannels > 0))
		{
			ChannelValues.AddZeroed(NumChannels);
		}

		SetWindowSize(InWindowTimeMsec);
	}

	void FMeanSquaredFIR::SetWindowSize(float InWindowTimeMsec)
	{
		if (ensure(InWindowTimeMsec > 0.f))
		{
			WindowTimeFrames = FMath::Max(1, FMath::RoundToInt(InWindowTimeMsec * 1000.f / SampleRate));
			NormFactor = 1.f / static_cast<float>(WindowTimeFrames);
			WindowTimeSamples = WindowTimeFrames * NumChannels;
		}

		Reset();
	}

	void FMeanSquaredFIR::Reset()
	{
		if (NumChannels > 0)
		{
			ChannelValues.Reset();
			ChannelValues.AddZeroed(NumChannels);

			WindowTimeSamples = WindowTimeFrames * NumChannels;
			HistorySquared.Reset(FMath::Max(2 * WindowTimeSamples, DefaultHistoryCapacity));
			if (WindowTimeSamples > 0)
			{
				HistorySquared.PushZeros(WindowTimeSamples);
			}
		}
	}

	void FMeanSquaredFIR::SetNumChannels(int32 InNumChannels)
	{
		if (NumChannels != InNumChannels)
		{
			NumChannels = InNumChannels;
			Reset(); // Update channel buffers and window buffers. 
		}
	}

	void FMeanSquaredFIR::ProcessAudio(const float* InBuffer, int32 InNumFrames, float* OutBuffer)
	{
		const int32 NumSamples = InNumFrames * NumChannels;
		SquaredHistoryBuffer.Reset(NumSamples);
		SquaredInputBuffer.Reset(NumSamples);

		if (NumSamples > 0)
		{
			// Square the input data
			SquaredInputBuffer.AddUninitialized(NumSamples);
			ArraySquare(MakeArrayView(InBuffer, NumSamples), SquaredInputBuffer);	

			// Save the squared data for later
			HistorySquared.Push(SquaredInputBuffer);

			// Get the historic input so it can be subtracted from 
			// the accumulated value as it leaves the window.
			SquaredHistoryBuffer.AddUninitialized(NumSamples);
			HistorySquared.Pop(SquaredHistoryBuffer.GetData(), NumSamples);
		}

		const float* HistorySquaredData = SquaredHistoryBuffer.GetData();
		const float* InputSquaredData = SquaredInputBuffer.GetData();
		float* ValueData = ChannelValues.GetData();

		// MS[n] = MS[n - 1] + x^2[n] - x^2[n - N]
		for (int32 ChannelIndex = 0; ChannelIndex < NumChannels; ChannelIndex++)
		{
			float Value = ValueData[ChannelIndex];

			for (int32 SampleIndex = ChannelIndex; SampleIndex < NumSamples; SampleIndex += NumChannels)
			{
				Value -= HistorySquaredData[SampleIndex];
				Value += InputSquaredData[SampleIndex];
				OutBuffer[SampleIndex] = Value * NormFactor;
			}

			ValueData[ChannelIndex] = Value;
		}
	}


	FMeanSquaredIIR::FMeanSquaredIIR(float InSampleRate, int32 InNumChannels, float InWindowTimeMsec)
	: SampleRate(InSampleRate)
	, NumChannels(0)
	, Alpha(0.f)
	, Beta(1.f)
	{
		if (!ensure(SampleRate > 0.f))
		{
			SampleRate = 48000.f;
		}

		SetNumChannels(InNumChannels);
		SetWindowSize(InWindowTimeMsec);
	}

	void FMeanSquaredIIR::SetWindowSize(float InWindowTimeMsec)
	{
		if (ensure(InWindowTimeMsec > 0.f))
		{
			// Exponential decay set so window is 1/e at end of window
			Alpha = FMath::Exp(-1000.f / (SampleRate * InWindowTimeMsec));
			Beta = 1.f - Alpha;
		}

		Reset();
	}

	void FMeanSquaredIIR::SetNumChannels(int32 InNumChannels)
	{
		if (NumChannels != InNumChannels)
		{
			ChannelValues.Reset();
			if (ensure(InNumChannels > 0))
			{
				ChannelValues.AddZeroed(InNumChannels);
			}
			NumChannels = InNumChannels;
		}
	}

	void FMeanSquaredIIR::Reset()
	{
		if (NumChannels > 0)
		{
			ChannelValues.Reset();
			ChannelValues.AddZeroed(NumChannels);
		}
	}

	// TODO: what's the convention. pass num frames or num samples?
	void FMeanSquaredIIR::ProcessAudio(const float* InBuffer, int32 InNumFrames, float* OutBuffer)
	{
		const int32 NumSamples = InNumFrames * NumChannels;

		// Square input and store in output.
		ArraySquare(MakeArrayView(InBuffer, NumSamples), MakeArrayView(OutBuffer, NumSamples));

		// MS[n] = Beta * x^2[n] + Alpha * MS[n - 1];
		for (int32 ChannelIndex = 0; ChannelIndex < NumChannels; ChannelIndex++)
		{
			// Get last output value
			float Value = ChannelValues[ChannelIndex];

			for (int32 SampleIndex = ChannelIndex; SampleIndex < NumSamples; SampleIndex += NumChannels)
			{
				Value = Beta * OutBuffer[SampleIndex] + Alpha * Value;
				Value = Audio::UnderflowClamp(Value);
				OutBuffer[SampleIndex] = Value;
			}

			// Store last output value for next call to ProcessAudio
			ChannelValues[ChannelIndex] = Value;
		}
	}


	// A simple utility that returns a smoothed value given audio input using an RC circuit.
	// Used for following the envelope of an audio stream.
	FEnvelopeFollower::FEnvelopeFollower()
	: FEnvelopeFollower(FEnvelopeFollowerInitParams{})
	{
	}

	FEnvelopeFollower::FEnvelopeFollower(const FEnvelopeFollowerInitParams& InParams)
	: MeanSquaredProcessor(InParams.SampleRate, InParams.NumChannels, InParams.AnalysisWindowMsec)
	, Smoother(InParams.SampleRate, InParams.NumChannels, InParams.AttackTimeMsec, InParams.ReleaseTimeMsec, InParams.bIsAnalog)
	, NumChannels(InParams.NumChannels)
	, EnvMode(InParams.Mode)
	{
	}

	// Initialize the envelope follower
	void FEnvelopeFollower::Init(const FEnvelopeFollowerInitParams& InParams)
	{
		Smoother = FAttackReleaseSmoother(InParams.SampleRate, InParams.NumChannels, InParams.AttackTimeMsec, InParams.ReleaseTimeMsec, InParams.bIsAnalog);
		MeanSquaredProcessor = FMeanSquaredIIR(InParams.SampleRate, InParams.NumChannels, InParams.AnalysisWindowMsec);
		NumChannels = InParams.NumChannels;
		EnvMode = InParams.Mode;
	}

	int32 FEnvelopeFollower::GetNumChannels() const
	{
		return NumChannels;
	}

	float FEnvelopeFollower::GetSampleRate() const
	{
		return Smoother.GetSampleRate();
	}

	float FEnvelopeFollower::GetAttackTimeMsec() const
	{
		return Smoother.GetAttackTimeMsec();
	}

	float FEnvelopeFollower::GetReleaseTimeMsec() const
	{
		return Smoother.GetReleaseTimeMsec();
	}

	bool FEnvelopeFollower::GetAnalog() const
	{
		return Smoother.GetAnalog();
	}

	EPeakMode::Type FEnvelopeFollower::GetMode() const
	{
		return EnvMode;
	}

	void FEnvelopeFollower::SetNumChannels(int32 InNumChannels)
	{
		if (InNumChannels != NumChannels)
		{
			Smoother.SetNumChannels(InNumChannels);
			MeanSquaredProcessor.SetNumChannels(NumChannels);
			NumChannels = InNumChannels;
		}
	}

	// Resets the state of the envelope follower
	void FEnvelopeFollower::Reset()
	{
		MeanSquaredProcessor.Reset();
		Smoother.Reset();
	}

	// Sets whether or not to use analog or digital time constants
	void FEnvelopeFollower::SetAnalog(bool bInIsAnalog)
	{
		Smoother.SetAnalog(bInIsAnalog);
	}

	// Sets the envelope follower attack time (how fast the envelope responds to input)
	void FEnvelopeFollower::SetAttackTime(float InAttackTimeMsec)
	{
		Smoother.SetAttackTime(InAttackTimeMsec);
	}

	// Sets the envelope follower release time (how slow the envelope dampens from input)
	void FEnvelopeFollower::SetReleaseTime(float InReleaseTimeMsec)
	{
		Smoother.SetReleaseTime(InReleaseTimeMsec);
	}

	// Sets the output mode of the envelope follower
	void FEnvelopeFollower::SetMode(EPeakMode::Type InMode)
	{
		EnvMode = InMode;
	}

	// Process the input audio buffer and returns the last envelope value
	void FEnvelopeFollower::ProcessAudio(const float* InBuffer, int32 InNumFrames, float* OutBuffer)
	{
		const int32 NumSamples = InNumFrames * NumChannels;
		if (NumSamples > 0)
		{
			ProcessWorkBuffer(InBuffer, InNumFrames);
			Smoother.ProcessAudio(WorkBuffer.GetData(), InNumFrames, OutBuffer);
		}
	}

	void FEnvelopeFollower::ProcessAudio(const float* InBuffer, int32 InNumFrames)
	{
		const int32 NumSamples = InNumFrames * NumChannels;
		if (NumSamples > 0)
		{
			ProcessWorkBuffer(InBuffer, InNumFrames);
			Smoother.ProcessAudio(WorkBuffer.GetData(), InNumFrames);
		}
	}

	void FEnvelopeFollower::ProcessWorkBuffer(const float* InBuffer, int32 InNumFrames)
	{
		const int32 NumSamples = InNumFrames * NumChannels;
		if (NumSamples > 0)
		{
			WorkBuffer.Reset();
			WorkBuffer.AddUninitialized(NumSamples);

			if (EPeakMode::RootMeanSquared == EnvMode)
			{
				MeanSquaredProcessor.ProcessAudio(InBuffer, InNumFrames, WorkBuffer.GetData());
				ArraySqrtInPlace(WorkBuffer);
			}
			else if (EPeakMode::MeanSquared == EnvMode)
			{
				MeanSquaredProcessor.ProcessAudio(InBuffer, InNumFrames, WorkBuffer.GetData());
			}
			else if (EPeakMode::Peak == EnvMode)
			{
				Audio::ArrayAbs(MakeArrayView(InBuffer, NumSamples), WorkBuffer);
			}
			else
			{
				check(false);
			}
		}
	}

	const TArray<float>& FEnvelopeFollower::GetEnvelopeValues() const
	{
		return Smoother.GetEnvelopeValues();
	}
}
