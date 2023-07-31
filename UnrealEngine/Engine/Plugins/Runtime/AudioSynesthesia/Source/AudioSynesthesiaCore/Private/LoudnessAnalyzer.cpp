// Copyright Epic Games, Inc. All Rights Reserved.


#include "LoudnessAnalyzer.h"
#include "AudioMixer.h"
#include "DSP/AlignedBuffer.h"
#include "DSP/DeinterleaveView.h"
#include "DSP/FloatArrayMath.h"

namespace LoudnessAnalyzerPrivate
{
	static const float Log10Scale = 1.0f / FMath::Loge(10.f);
}

namespace Audio
{
    /* Equal loudness curves from: ANSI Standards S1.4-1983 and S1.42-2001 */
    /* https://en.wikipedia.org/wiki/A-weighting */
    float GetEqualLoudnessAWeightForFrequency(const float Freq)
    {
        const float FreqSqrd = Freq * Freq;

        return 1.258925411794167f * ((148840000.f * FreqSqrd * FreqSqrd) / ((FreqSqrd + 424.36f) * (FreqSqrd + 148840000.f) * FMath::Sqrt((FreqSqrd + 11599.29f) * (FreqSqrd + 544496.41f))));
    }

    float GetEqualLoudnessBWeightForFrequency(const float Freq)
    {
        const float FreqSqrd = Freq * Freq;

        return 1.019764760044717f * ((148840000.f * Freq * FreqSqrd) / ((FreqSqrd + 424.36f) * (FreqSqrd + 148840000.f) * FMath::Sqrt(FreqSqrd + 25122.25f)));
    }

    float GetEqualLoudnessCWeightForFrequency(const float Freq)
    {
        const float FreqSqrd = Freq * Freq;

        return 1.006931668851804f * ((148840000.f * FreqSqrd) / ((FreqSqrd + 424.36f) * (FreqSqrd + 148840000.f)));
    }

    float GetEqualLoudnessDWeightForFrequency(const float Freq)
    {
        const float FreqSqrd = Freq * Freq;
        const float H1 = (1037918.48f - FreqSqrd);
        const float H2 = (9837328.f - FreqSqrd);
        const float H = (H1 + 1080768.16f * FreqSqrd) / (H2 + 11723776.f * FreqSqrd);
		return (Freq / 6.8966888496476e-5f) * FMath::Sqrt(H / ((FreqSqrd + 79919.29f) * (FreqSqrd + 1345600.f)));
    }

    float GetEqualLoudnessNoneWeightForFrequency(const float Freq)
    {
        return 1.0f;
    }
    

	/****************************************************************************/
	/***********************    FLoudnessAnalyzer    ****************************/
	/****************************************************************************/

    FLoudnessAnalyzer::FLoudnessAnalyzer(float InSampleRate, const FLoudnessAnalyzerSettings& InSettings)
    :   Settings(InSettings)
    ,   Window(Settings.WindowType, Settings.FFTSize, 1, false)
    {
		checkf(FMath::IsPowerOfTwo(Settings.FFTSize), TEXT("FFT size must be a power of two"));
		checkf(Settings.FFTSize >= 8, TEXT("FFT size must be atleast 8"))
        check(InSampleRate > 0);

		// Create FFT
		FFFTSettings FFTSettings;
		FFTSettings.Log2Size = CeilLog2(Settings.FFTSize);
		FFTSettings.bArrays128BitAligned = true;
		FFTSettings.bEnableHardwareAcceleration = true;

		ActualFFTSize = 1 << FFTSettings.Log2Size;

		checkf(FFFTFactory::AreFFTSettingsSupported(FFTSettings), TEXT("No fft algorithm supports fft settings."));
		FFT = FFFTFactory::NewFFTAlgorithm(FFTSettings);
		if (FFT.IsValid())
		{
			// Size internal buffers
			WindowedSamples.AddZeroed(FFT->NumInputFloats()); // Zero samples to apply zero buffer in case actual FFT is larger than provided settings.
			ComplexSpectrum.AddUninitialized(FFT->NumOutputFloats());
			RealSpectrum.AddUninitialized(ComplexSpectrum.Num() / 2);

			FFTFreqSize = RealSpectrum.Num();

			// Determine which fft bins will be used for calculating loudness
			const float FreqPerBin = InSampleRate / FFT->NumOutputFloats();
			MinFreqIndex = static_cast<int32>(FMath::RoundHalfFromZero(Settings.MinAnalysisFrequency / FreqPerBin));
			MinFreqIndex = FMath::Max(0, MinFreqIndex);
			MaxFreqIndex = static_cast<int32>(FMath::RoundHalfFromZero(Settings.MaxAnalysisFrequency / FreqPerBin)) + 1;
			MaxFreqIndex = FMath::Min(MaxFreqIndex, FFTFreqSize);

			CurveWeights.Reset(FFTFreqSize);
			CurveWeights.AddUninitialized(FFTFreqSize);

			float(*LoudnessMapFn)(const float);
			switch (Settings.LoudnessCurveType)
			{
				case ELoudnessCurveType::A:
					LoudnessMapFn = GetEqualLoudnessAWeightForFrequency;
					break;

				case ELoudnessCurveType::B:
					LoudnessMapFn = GetEqualLoudnessBWeightForFrequency;
					break;

				case ELoudnessCurveType::C:
					LoudnessMapFn = GetEqualLoudnessCWeightForFrequency;
					break;

				case ELoudnessCurveType::D:
					LoudnessMapFn = GetEqualLoudnessDWeightForFrequency;
					break;

				case ELoudnessCurveType::None:
				default:
					LoudnessMapFn = GetEqualLoudnessNoneWeightForFrequency;
					break;
			}

			/* Calculate equal loudness weighting curve */

			// Convention is to normalize 1kHz to have a loudness of 1.
			float EqualLoudnessNorm = 1.0f / LoudnessMapFn(1000.0f);

			float* CurveWeightsPtr = CurveWeights.GetData();

			const float FFTSize = static_cast<float>(ActualFFTSize);

			for (int32 i = 0; i < FFTFreqSize; i++)
			{
				const float Freq = static_cast<float>(i) * InSampleRate / FFTSize;
				float Weighting = EqualLoudnessNorm;

				switch (Settings.LoudnessCurveType)
				{
					case ELoudnessCurveType::A:
						Weighting *= GetEqualLoudnessAWeightForFrequency(Freq);
						break;

					case ELoudnessCurveType::B:
						Weighting *= GetEqualLoudnessBWeightForFrequency(Freq);
						break;

					case ELoudnessCurveType::C:
						Weighting *= GetEqualLoudnessCWeightForFrequency(Freq);
						break;

					case ELoudnessCurveType::D:
						Weighting *= GetEqualLoudnessDWeightForFrequency(Freq);
						break;

					case ELoudnessCurveType::None:
					default:
						Weighting *= GetEqualLoudnessNoneWeightForFrequency(Freq);
						break;
				}

				// Curve designed for magnitude domain, but applied to power spectrum. The curve is squared to be applied to power spectrum.
				CurveWeightsPtr[i] = Weighting * Weighting;
			}

			// Normalize by FFT Scaling (1 / FFTSize) and by number of samples (1 / WindowSize) to calculate mean squared value.
			EnergyScale = GetPowerSpectrumScaling(ActualFFTSize, FFT->ForwardScaling(), EFFTScaling::None);
			EnergyScale *= 1.f / static_cast<float>(Settings.FFTSize);
		}
    }

    const FLoudnessAnalyzerSettings& FLoudnessAnalyzer::GetSettings() const
    {
        return Settings;
    }

    float FLoudnessAnalyzer::CalculatePerceptualEnergy(TArrayView<const float> InView)
    {
		check(InView.Num() == Settings.FFTSize);

		float Total = 0.f;

		if (FFT.IsValid())
		{
			// Copy input samples and apply window
			FMemory::Memcpy(WindowedSamples.GetData(), InView.GetData(), sizeof(float) * Settings.FFTSize);
			Window.ApplyToBuffer(WindowedSamples.GetData());

			FFT->ForwardRealToComplex(WindowedSamples.GetData(), ComplexSpectrum.GetData());

			ArrayComplexToPower(ComplexSpectrum, RealSpectrum);
		
			const float* PowerSpectrumPtr = RealSpectrum.GetData();
			const float* CurveWeightsPtr = CurveWeights.GetData();
			for (int32 i = MinFreqIndex; i < MaxFreqIndex; i++)
			{
				Total += PowerSpectrumPtr[i] * CurveWeightsPtr[i];
			}

			// Normalize by FFT Scaling and by number of samples to calculate mean squared value.
			Total *= EnergyScale;
		}

        return Total;
    }

    float FLoudnessAnalyzer::CalculateLoudness(TArrayView<const float> InView)
    {
        float Energy = CalculatePerceptualEnergy(InView);
		return ConvertPerceptualEnergyToLoudness(Energy);
    }

	float FLoudnessAnalyzer::ConvertPerceptualEnergyToLoudness(float InPerceptualEnergy)
	{
        return 10.0f * FMath::Loge(InPerceptualEnergy) * LoudnessAnalyzerPrivate::Log10Scale;
	}

	/****************************************************************************/
	/****************    FMultichannelLoudnessAnalyzer    ***********************/
	/****************************************************************************/

    FMultichannelLoudnessAnalyzer::FMultichannelLoudnessAnalyzer(float InSampleRate, const FLoudnessAnalyzerSettings& InSettings)
    :   Analyzer(InSampleRate, InSettings)
    {
        // Initialize all channel weights to default to 1.0f
        ChannelWeights.Init(1.0f, EAudioMixerChannel::ChannelTypeCount);

        // If elevation angle is less than 30 degrees and azimuth is between 60 and 120, set channel weight to 1.5f. 
        ChannelWeights[EAudioMixerChannel::BackLeft] = 1.5f;
        ChannelWeights[EAudioMixerChannel::BackRight] = 1.5f;

        MonoBuffer.Reset(InSettings.FFTSize);
        MonoBuffer.AddUninitialized(InSettings.FFTSize);
    }

    float FMultichannelLoudnessAnalyzer::CalculatePerceptualEnergy(TArrayView<const float> InView, const int32 InNumChannels, TArray<float>& OutChannelEnergies)
    {
		check(InView.Num() == (InNumChannels * Analyzer.GetSettings().FFTSize));

        OutChannelEnergies.Reset(InNumChannels);
        OutChannelEnergies.AddUninitialized(InNumChannels);

        // Deinterleave audio and send to mono loudness analyzer.
		TAutoDeinterleaveView<float, FAudioBufferAlignedAllocator> DeinterleaveView(InView, MonoBuffer, InNumChannels);
		for (TAutoDeinterleaveView<float>::TChannel<FAudioBufferAlignedAllocator> Channel : DeinterleaveView)
		{
            OutChannelEnergies[Channel.ChannelIndex] = Analyzer.CalculatePerceptualEnergy(Channel.Values);
		}
				
        // Combine channel energies into overall perceptual energy
        float Energy = 0.0f;
        for (int32 ChannelNum = 0; ChannelNum < InNumChannels; ChannelNum++)
        {
            Energy += ChannelWeights[ChannelNum] * OutChannelEnergies[ChannelNum];
        }

        return Energy;
    }

    float FMultichannelLoudnessAnalyzer::CalculateLoudness(TArrayView<const float> InView, const int32 InNumChannels, TArray<float>& OutChannelLoudness)
    {
        // Convert perceptual energy to loudness 
        float Energy = CalculatePerceptualEnergy(InView, InNumChannels, OutChannelLoudness);
        for (float& Value : OutChannelLoudness)
        {
            Value = FLoudnessAnalyzer::ConvertPerceptualEnergyToLoudness(Value);
        }
        return FLoudnessAnalyzer::ConvertPerceptualEnergyToLoudness(Energy);
    }

    const FLoudnessAnalyzerSettings& FMultichannelLoudnessAnalyzer::GetSettings() const
    {
        return Analyzer.GetSettings();
    }
}

