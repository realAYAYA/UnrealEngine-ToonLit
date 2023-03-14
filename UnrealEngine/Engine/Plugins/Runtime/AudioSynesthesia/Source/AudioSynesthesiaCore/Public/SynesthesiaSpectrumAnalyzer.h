// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DSP/AlignedBuffer.h"
#include "DSP/AudioFFT.h"
#include "DSP/FFTAlgorithm.h"

namespace Audio
{
	enum class ESynesthesiaSpectrumType : uint8
	{
		// Spectrum frequency values are equal to magnitude of frequency.
		MagnitudeSpectrum,

		// Spectrum frequency values are equal to magnitude squared.
		PowerSpectrum,

		// Returns decibels with a noise floor of -90
		Decibel,
	};

	struct AUDIOSYNESTHESIACORE_API FSynesthesiaSpectrumAnalyzerSettings 
	{	
		int32 FFTSize = 512;

		ESynesthesiaSpectrumType SpectrumType = ESynesthesiaSpectrumType::PowerSpectrum;
		
		EWindowType WindowType = EWindowType::None;
		
		bool bDownmixToMono = true;
	};

	class AUDIOSYNESTHESIACORE_API FSynesthesiaSpectrumAnalyzer
	{
	public:

		/** Construct analyzer */
		FSynesthesiaSpectrumAnalyzer(float InSampleRate, const FSynesthesiaSpectrumAnalyzerSettings& InSettings);

		/**
		 * Calculate the spectrum values for the input samples. OutSpectrum should have (num input samples / 2) + 1 values. 
		 */
		void ProcessAudio(TArrayView<const float> InSampleView, TArrayView<float> OutSpectrum);

		/**
		 * Return const reference to settings used inside this analyzer.
		 */
		const FSynesthesiaSpectrumAnalyzerSettings& GetSettings() const;

	protected:
		FSynesthesiaSpectrumAnalyzerSettings Settings;
		float SampleRate = 0.0f;
		float FFTScaling = 0.0f;

		FWindow Window;
		Audio::FAlignedFloatBuffer WindowedBuffer;
		Audio::FAlignedFloatBuffer FFTOutput;
		TUniquePtr<IFFTAlgorithm> FFT;
	};
}
