// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/CoreMiscDefines.h"
#include "DSP/BufferVectorOperations.h"
#include "DSP/Dsp.h"
#include "DSP/FFTAlgorithm.h"

namespace Audio
{
	// Types of spectrums which can be directly derived from FFTFreqDomainData
	enum class ESpectrumType : uint8
	{
		MagnitudeSpectrum,
		PowerSpectrum
	};

	namespace FFTIntrinsics
	{
		SIGNALPROCESSING_API uint32 NextPowerOf2(uint32 Input);
	}

	enum class EWindowType : uint8
	{
		None, // No window is applied. Technically a boxcar window.
		Hamming, // Mainlobe width of -3 dB and sidelove attenuation of ~-40 dB. Good for COLA.
		Hann, // Mainlobe width of -3 dB and sidelobe attenuation of ~-30dB. Good for COLA.
		Blackman // Mainlobe width of -3 dB and sidelobe attenuation of ~-60db. Tricky for COLA.
	};

	// Utility functions for generating different types of windows. Called in FWindow::Generate.
	SIGNALPROCESSING_API void GenerateHammingWindow(float* WindowBuffer, int32 NumFrames, int32 NumChannels, bool bIsPeriodic);
	SIGNALPROCESSING_API void GenerateHannWindow(float* WindowBuffer, int32 NumFrames, int32 NumChannels, bool bIsPeriodic);
	SIGNALPROCESSING_API void GenerateBlackmanWindow(float* WindowBuffer, int32 NumFrames, int32 NumChannels, bool bIsPeriodic);

	// Returns the hop size in samples necessary to maintain constant overlap add.
	// For more information on COLA, see the following page:
	// https://ccrma.stanford.edu/~jos/sasp/Overlap_Add_OLA_STFT_Processing.html
	SIGNALPROCESSING_API uint32 GetCOLAHopSizeForWindow(EWindowType InType, uint32 WindowLength);

	/**
	 * Class used to generate, contain and apply a DSP window of a given type.
	 */
	class FWindow
	{
	public:
		FWindow() = delete;

		/**
		 * Constructor. Allocates buffer and generates window inside of it.
		 * @param InType: The type of window that should be generated.
		 * @param InNumFrames: The number of samples that should be generated divided by the number of channels.
		 * @param InNumChannels: The amount of channels that will be used in the signal this is applied to.
		 * @param bIsPeriodic: If false, the window will be symmetrical. If true, the window will be periodic.
		 *                     Generally, set this to false if using this window with an STFT, but use true
		 *                     if this window will be used on an entire, self-contained signal.
		 */
		SIGNALPROCESSING_API FWindow(EWindowType InType, int32 InNumFrames, int32 InNumChannels, bool bIsPeriodic);

		// Apply this window to InBuffer, which is expected to be an interleaved buffer with the same amount of frames
		// and channels this window was constructed with.
		SIGNALPROCESSING_API void ApplyToBuffer(float* InBuffer);

		SIGNALPROCESSING_API EWindowType GetWindowType() const;

	private:
		EWindowType WindowType;
		FAlignedFloatBuffer WindowBuffer;
		int32 NumSamples;

		// Generate the window. Called on constructor.
		SIGNALPROCESSING_API void Generate(int32 NumFrames, int32 NumChannels, bool bIsPeriodic);
	};

	struct FFTTimeDomainData_DEPRECATED
	{
		float* Buffer; // Pointer to a single channel of floats.
		int32 NumSamples; // Number of samples in InBuffer divided by the number of channels. must be a power of 2.
	};

	/** AudioFFT Deprecation
	 *
	 * The AudioFFT is deprecated in favor of optimized software implementations 
	 * and hardware implementations. The FFT implemenented in "PerformFFT(...)" 
	 * has poor CPU performance as it computes the FFT radix weights each time
	 * "PerformFFT(...)" is executed. 
	 *
	 * Better optimized and hardward FFT implementations can be accessed through the 
	 * FFFTFactory class in FFTAlgorithm.h. That class will attempt to return the 
	 * best FFT algorithm implementation available for the given FFFTSettings.
	 */

	struct UE_DEPRECATED(5.1, "Use IFFTAlgorithm and TArray for time domain data") FFTTimeDomainData;
	struct FFTTimeDomainData : FFTTimeDomainData_DEPRECATED 
	{
		FFTTimeDomainData() = default;

		FFTTimeDomainData(float* InBuffer, int32 InNumSamples)
		{
			Buffer = InBuffer;
			NumSamples = InNumSamples;
		}
	};

	struct FFTFreqDomainData_DEPRECATED
	{
		// arrays in which real and imaginary values will be populated.
		float* OutReal; // Should point to an already allocated array of floats that is FFTInputParams::NumSamples long.
		float* OutImag; // Should point to an already allocated array of floats that is FFTInputParams::NumSamples long.
	};

	struct UE_DEPRECATED(5.1, "Use IFFTAlgorithm and TArray for time freq domain data") FFTFreqDomainData;
	struct FFTFreqDomainData : FFTFreqDomainData_DEPRECATED 
	{
		FFTFreqDomainData() = default;

		FFTFreqDomainData(float* Real, float* Imag)
		{
			OutReal = Real;
			OutImag = Imag;
		}
	};

	// Performs a one-time FFT on a float buffer. Does not support complex signals.
	// This function assumes that, if you desire a window for your FFT, that window was already
	// applied to FFTInputParams.InBuffer.

 	UE_DEPRECATED(5.1, "Use FVectorFFTRealToComplex or FFFTFactory instead.")
	SIGNALPROCESSING_API void PerformFFT(const FFTTimeDomainData_DEPRECATED& InputParams, FFTFreqDomainData_DEPRECATED& OutputParams);

 	UE_DEPRECATED(5.1, "Use FVectorFFTRealToComplex or FFFTFactory instead.")
	SIGNALPROCESSING_API void PerformIFFT(FFTFreqDomainData_DEPRECATED& InputParams, FFTTimeDomainData_DEPRECATED& OutputParams);


	// FFT Algorithm factory for this FFT implementation
	class UE_DEPRECATED(5.1, "Use FVectorFFTFactory instead.") FAudioFFTAlgorithmFactory;
	class FAudioFFTAlgorithmFactory : public IFFTAlgorithmFactory
	{
		public:
			SIGNALPROCESSING_API virtual ~FAudioFFTAlgorithmFactory();

			// Name of this fft algorithm factory. 
			SIGNALPROCESSING_API virtual FName GetFactoryName() const override;

			// If true, this implementation uses hardware acceleration.
			SIGNALPROCESSING_API virtual bool IsHardwareAccelerated() const override;

			// If true, this implementation requires input and output arrays to be 128 bit aligned.
			SIGNALPROCESSING_API virtual bool Expects128BitAlignedArrays() const override;

			// Returns true if the input settings are supported by this factory.
			SIGNALPROCESSING_API virtual bool AreFFTSettingsSupported(const FFFTSettings& InSettings) const override;

			// Create a new FFT algorithm.
			SIGNALPROCESSING_API virtual TUniquePtr<IFFTAlgorithm> NewFFTAlgorithm(const FFFTSettings& InSettings) override;
	};

	struct FrequencyBuffer
 	
	{
		FAlignedFloatBuffer Real;
		FAlignedFloatBuffer Imag;

		void InitZeroed(int32 Num)
		{
			Real.Reset();
			Real.AddZeroed(Num);

			Imag.Reset();
			Imag.AddZeroed(Num);
		}

		void CopyFrom(const float* InReal, const float* InImag, int32 Num)
		{
			check(Num == Real.Num() && Num == Imag.Num());
			FMemory::Memcpy(Real.GetData(), InReal, Num * sizeof(float));
			FMemory::Memcpy(Imag.GetData(), InImag, Num * sizeof(float));
		}

		void CopyFrom(const FrequencyBuffer& Other)
		{
			check(Other.Real.Num() == Real.Num() && Other.Imag.Num() == Imag.Num());
			FMemory::Memcpy(Real.GetData(), Other.Real.GetData(), Other.Real.Num() * sizeof(float));
			FMemory::Memcpy(Imag.GetData(), Other.Imag.GetData(), Other.Imag.Num() * sizeof(float));
		}
	};

	// Performs an acyclic FFT correlation on FirstBuffer and Second buffer and stores the output in OutCorrelation.
	// If bCyclic is false, This function may zero pad FirstBuffer and Second Buffer as needed.
	// If bCyclic is true, FirstBuffer and SecondBuffer should have the same length, and that length should be a power of two.
	UE_DEPRECATED(5.1, "Cross correlate is no longer supported.")
	SIGNALPROCESSING_API void CrossCorrelate(FAlignedFloatBuffer& FirstBuffer, FAlignedFloatBuffer& SecondBuffer, FAlignedFloatBuffer& OutCorrelation, bool bZeroPad = true);
	UE_DEPRECATED(5.1, "Cross correlate is no longer supported.")
	SIGNALPROCESSING_API void CrossCorrelate(FAlignedFloatBuffer& FirstBuffer, FAlignedFloatBuffer& SecondBuffer, FrequencyBuffer& OutCorrelation, bool bZeroPad = true);
	UE_DEPRECATED(5.1, "Cross correlate is no longer supported.")
	SIGNALPROCESSING_API void CrossCorrelate(const float* FirstBuffer, const float* SecondBuffer, int32 NumSamples, int32 FFTSize, float* OutCorrelation, int32 OutCorrelationSamples);
	UE_DEPRECATED(5.1, "Cross correlate is no longer supported.")
	SIGNALPROCESSING_API void CrossCorrelate(const float* FirstBuffer, const float* SecondBuffer, int32 NumSamples, int32 FFTSize, FrequencyBuffer& OutCorrelation);

	// These variations do not allocate any additional memory during the function, provided that the FrequencyBuffers are already allocated.
	UE_DEPRECATED(5.1, "Cross correlate is no longer supported.")
	SIGNALPROCESSING_API void CrossCorrelate(const float* FirstBuffer, const float* SecondBuffer, int32 NumSamples, int32 FFTSize, FrequencyBuffer& FirstBufferFrequencies, FrequencyBuffer& SecondBufferFrequencies, FrequencyBuffer& OutCorrelation);
	UE_DEPRECATED(5.1, "Cross correlate is no longer supported.")
	SIGNALPROCESSING_API void CrossCorrelate(FrequencyBuffer& FirstBufferFrequencies, FrequencyBuffer& SecondBufferFrequencies, int32 NumSamples, FrequencyBuffer& OutCorrelation);

	// Deprecated in 5.1
	class FFFTConvolver_DEPRECATED
	{
	public:
		SIGNALPROCESSING_API FFFTConvolver_DEPRECATED();

		/*
		 * Applies the convolver's internal window to InputAudio. Until SetWindow is called, ProcessAudio will not affect InputAudio.
		 * InputAudio must be a power of two.
		 */
		SIGNALPROCESSING_API void ProcessAudio(float* InputAudio, int32 NumSamples);

		/**
		 * Resets the filter window. NOT thread safe to call during ProcessAudio.
		 * This function can be called with a time domain impulse response, or precomputed frequency values. 
		 * FilterSize must be a power of two.
		 */ 
		SIGNALPROCESSING_API void SetFilter(const float* InFilterReal, const float* InFilterImag, int32 FilterSize, int32 FFTSize);
		SIGNALPROCESSING_API void SetFilter(const FrequencyBuffer& InFilterFrequencies, int32 FilterSize);
		SIGNALPROCESSING_API void SetFilter(const float* TimeDomainBuffer, int32 FilterSize);
		SIGNALPROCESSING_API void SetFilter(const FAlignedFloatBuffer& TimeDomainBuffer);

	private:
		void ConvolveBlock(float* InputAudio, int32 NumSamples);
		void SumInCOLABuffer(float* InputAudio, int32 NumSamples);
		void SetCOLABuffer(float* InAudio, int32 NumSamples);

		FrequencyBuffer FilterFrequencies;
		FrequencyBuffer InputFrequencies;
		int32 BlockSize;


		FAlignedFloatBuffer TimeDomainInputBuffer;
		FAlignedFloatBuffer COLABuffer;
	};

	class UE_DEPRECATED(5.1, "Use FConvolutionFactory or UniformPartitionConvolutionFactory.") FFFTConvolver : public FFFTConvolver_DEPRECATED
	{
	};

	// Computes the power spectrum from FFTFreqDomainData. Applies a 1/(FFTSize^2) scaling to the output to 
	// maintain equal energy between original time domain data and output spectrum.  Only the first 
	// (FFTSize / 2 + 1) spectrum values are calculated. These represent the frequencies from 0 to Nyquist.
	//
	// InFrequencyData is the input frequency domain data. Generally this is created by calling PerformFFT(...)
	// FFTSize is the number of samples used when originally calculating the FFT
	// OutBuffer is an aligned buffer which will contain spectrum data. It will constain (FFTSize / 2 + 1) elements.
	UE_DEPRECATED(5.1, "Use equivalent methods in FloatArrayMath.h")
	SIGNALPROCESSING_API void ComputePowerSpectrum(const FFTFreqDomainData_DEPRECATED& InFrequencyData, int32 FFTSize, FAlignedFloatBuffer& OutBuffer);

	// Computes the magnitude spectrum from FFTFreqDomainData. Applies a 1/FFTSize scaling to the output to 
	// maintain equal energy between original time domain data and output spectrum.  Only the first 
	// (FFTSize / 2 + 1) spectrum values are calculated. These represent the frequencies from 0 to Nyquist.
	//
	// InFrequencyData is the input frequency domain data. Generally this is created by calling PerformFFT(...)
	// FFTSize is the number of samples used when originally calculating the FFT
	// OutBuffer is an aligned buffer which will contain spectrum data. It will constain (FFTSize / 2 + 1) elements.
	UE_DEPRECATED(5.1, "Use equivalent methods in FloatArrayMath.h")
	SIGNALPROCESSING_API void ComputeMagnitudeSpectrum(const FFTFreqDomainData_DEPRECATED& InFrequencyData, int32 FFTSize, FAlignedFloatBuffer& OutBuffer);
	
	// Computes the spectrum from FFTFreqDomainData. Applies a scaling to the output to maintain equal 
	// energy between original time domain data and output spectrum.  Only the first (FFTSize / 2 + 1)
	// spectrum values are calculated. These represent the frequencies from 0 to Nyquist.
	//
	// InSpectrumType denotes which spectrum type to calculate.
	// InFrequencyData is the input frequency domain data. Generally this is created by calling PerformFFT(...)
	// FFTSize is the number of samples used when originally calculating the FFT
	// OutBuffer is an aligned buffer which will contain spectrum data. It will constain (FFTSize / 2 + 1) elements.
	UE_DEPRECATED(5.1, "Use equivalent methods in FloatArrayMath.h")
	SIGNALPROCESSING_API void ComputeSpectrum(ESpectrumType InSpectrumType, const FFTFreqDomainData_DEPRECATED& InFrequencyData, int32 FFTSize, FAlignedFloatBuffer& OutBuffer);

	// Return the ceiling of the log2 of InNum
	SIGNALPROCESSING_API int32 CeilLog2(int32 InNum);

	// Return the scaling factor needed to apply to a power spectrum given a current
	// and target FFT scaling. 
	SIGNALPROCESSING_API float GetPowerSpectrumScaling(int32 FFTSize, EFFTScaling InCurrentScaling, EFFTScaling InTargetScaling);

	// Scale the power spectrum to remove any scaling introduced by the FFT algorithm
	// implementation.
	SIGNALPROCESSING_API void ScalePowerSpectrumInPlace(int32 FFTSize, EFFTScaling InCurrentScaling, EFFTScaling InTargetScaling, TArrayView<float> InPowerSpectrum);
}
