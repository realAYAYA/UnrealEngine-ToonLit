// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundWaveProxyReader.h"

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "DecoderInputFactory.h"
#include "DSP/BufferVectorOperations.h"
#include "DSP/Dsp.h"
#include "HAL/IConsoleManager.h"
#include "HAL/Platform.h"
#include "HAL/UnrealMemory.h"
#include "IAudioCodec.h"
#include "IAudioCodecRegistry.h"
#include "Math/UnrealMathUtility.h"
#include "MetasoundLog.h"
#include "MetasoundTrace.h"
#include "Misc/AssertionMacros.h"
#include "Sound/SoundWave.h"
#include "Templates/SharedPointer.h"
#include "Templates/UniquePtr.h"

static int32 GMetaSoundWaveProxyReaderSimulateSeekOnNonSeekable = 0;
FAutoConsoleVariableRef CVarMetaSoundWaveProxyReaderSimulateSeekOnNonSeekable(
	TEXT("au.MetaSound.WavePlayer.SimulateSeek"),
	GMetaSoundWaveProxyReaderSimulateSeekOnNonSeekable ,
	TEXT("If true, SoundWaves which are not of a seekable format will simulate seek calls by reading and discarding samples.\n")
	TEXT("0: Do not simulate seek, !0: Simulate seek"),
	ECVF_Default);

namespace Metasound
{
	namespace MetasoundWaveProxyReaderPrivate
	{
		/** Construct a FDecoderOutput
		 *
		 * @param InNumFramesPerDecode - Maximum number of frames pushed when
		 *                               audio is decoded. The buffer will be 
		 *                               able to hold twice the number of frames. 
		 */
		FDecoderOutput::FDecoderOutput(uint32 InNumFramesPerDecode)
		: NumFramesPerDecode(FMath::Max(InNumFramesPerDecode, MinNumFramesPerDecode))
		{
			Init();
		}

		/** Set the number of channels in the audio being decoded. */
		void FDecoderOutput::SetNumChannels(uint32 InNumChannels)
		{
			NumChannels = FMath::Max(MinNumChannels, InNumChannels);
			Init();
		}

		/** Removes all samples from the buffer. */
		void FDecoderOutput::Reset()
		{
			Buffer.SetNum(0);
		}

		/** Returns requirements used by the audio codec system. */
		Audio::IDecoderOutput::FRequirements FDecoderOutput::GetRequirements(const Audio::FDecodedFormatInfo& InFormat) const
		{
			FRequirements Requirements;

			Requirements.DownstreamFormat = Audio::EBitRepresentation::Float32_Interleaved;
			Requirements.NumSampleFramesWanted = NumFramesPerDecode;
			Requirements.NumSampleFramesPerSecond = InFormat.NumFramesPerSec;
			Requirements.NumChannels = InFormat.NumChannels;

			return Requirements;
		}

		/** Adds samples to the buffer. 
		 *
		 * This is called by the Decoder and should not be called otherwise.
		 */
		int32 FDecoderOutput::PushAudio(const Audio::IDecoderOutput::FPushedAudioDetails& InDetails, TArrayView<const int16> In16BitInterleave)
		{
			if (InDetails.NumChannels != NumChannels)
			{
				SetNumChannels(InDetails.NumChannels);
			}
			return PushAudioInternal(InDetails, In16BitInterleave);
		}

		/** Adds samples to the buffer. 
		 *
		 * This is called by the Decoder and should not be called otherwise.
		 */
		int32 FDecoderOutput::PushAudio(const FPushedAudioDetails& InDetails, TArrayView<const float> InFloat32Interleave) 
		{
			if (InDetails.NumChannels != NumChannels)
			{
				SetNumChannels(InDetails.NumChannels);
			}
			return PushAudioInternal(InDetails, InFloat32Interleave);
		}

		/** This should not be called. It removes 16 bit PCM samples from the buffer
		 * which is an unsupported operation of this class. 
		 */
		int32 FDecoderOutput::PopAudio(TArrayView<int16> InExternalInt16Buffer, FPushedAudioDetails& OutDetails)
		{
			// This buffer cannot produce 16 bit PCM audio.
			checkNoEntry();
			return 0;
		}

		/** Copy samples to OutBuffer and remove them from this objects internal
		 * buffer. 
		 *
		 *
		 * @param OutBuffer - A destination array to copy samples to.
		 * @param OutDetails - Unused. 
		 *
		 * @return The actual number of samples copied. 
		 */
		int32 FDecoderOutput::PopAudio(TArrayView<float> OutBuffer, FPushedAudioDetails& OutDetails) 
		{
			return Buffer.Pop(OutBuffer.GetData(), OutBuffer.Num());
		}

		// Initialize buffer size.
		void FDecoderOutput::Init()
		{
			// Allow it to hold two decode buffers max.
			uint32 MinBufferCapacity = NumChannels * NumFramesPerDecode * 2;
			constexpr bool bRetainExistingSamples = false;
			Buffer.Reserve(MinBufferCapacity, bRetainExistingSamples);
		}

		int32 FDecoderOutput::PushAudioInternal(const FPushedAudioDetails& InDetails, TArrayView<const float> InBuffer) 
		{
			return Buffer.Push(InBuffer.GetData(), InBuffer.Num());
		}

		int32 FDecoderOutput::PushAudioInternal(const FPushedAudioDetails& InDetails, TArrayView<const int16> InBuffer) 
		{
			SampleConversionBuffer.SetNumUninitialized(InBuffer.Num(), false /* bAllowShrinking */);

			// Convert 16 bit pcm to 32 bit float.
			constexpr float Scalar = 1.f / 32768.f;
			const int16* Src = InBuffer.GetData();
			float* Dst = SampleConversionBuffer.GetData();
			int32 Num = InBuffer.Num();

			// Convert 1 sample at a time, slow.
			for (int32 i = 0; i < Num; ++i)
			{
				*Dst++ = static_cast<float>(*Src++) * Scalar;
			}

			return PushAudioInternal(InDetails, SampleConversionBuffer);
		}

	}


	

	uint32 FWaveProxyReader::ConformDecodeSize(uint32 InMaxDesiredDecodeSizeInFrames)
	{
		static_assert(DefaultMinDecodeSizeInFrames >= DecodeSizeQuantizationInFrames, "Min decode size less than decode size quantization");
		static_assert((DefaultMinDecodeSizeInFrames % DecodeSizeQuantizationInFrames) == 0, "Min decode size must be equally divisible by decode size quantization");

		const uint32 QuantizedDecodeSizeInFrames = (InMaxDesiredDecodeSizeInFrames / DecodeSizeQuantizationInFrames) * DecodeSizeQuantizationInFrames;
		const uint32 ConformedDecodeSizeInFrames = FMath::Max(QuantizedDecodeSizeInFrames, DefaultMinDecodeSizeInFrames);

		check((ConformedDecodeSizeInFrames % QuantizedDecodeSizeInFrames) == 0);
		check(ConformedDecodeSizeInFrames > 0);

		return ConformedDecodeSizeInFrames;
	}

	/** Construct a wave proxy reader. 
	 *
	 * @param InWaveProxy - A TSharedRef of a FSoundWaveProxy which is to be played. 
	 * @param InSettings - Reader settings. 
	 */
	FWaveProxyReader::FWaveProxyReader(FSoundWaveProxyRef InWaveProxy, const FSettings& InSettings)
	: WaveProxy(InWaveProxy)
	, DecoderOutput(ConformDecodeSize(InSettings.MaxDecodeSizeInFrames))
	, Settings(InSettings)
	{
		// Get local copies of some values from the proxy. 
		SampleRate = WaveProxy->GetSampleRate();
		NumChannels = WaveProxy->GetNumChannels();
		NumFramesInWave = WaveProxy->GetNumFrames();
		DurationInSeconds = WaveProxy->GetDuration();
		MaxLoopStartTimeInSeconds = FMath::Max(0, DurationInSeconds - MinLoopDurationInSeconds);

		// Clamp times
		Settings.StartTimeInSeconds = FMath::Clamp(Settings.StartTimeInSeconds, 0.f, DurationInSeconds);
		Settings.LoopStartTimeInSeconds = ClampLoopStartTime(Settings.LoopStartTimeInSeconds);
		Settings.LoopDurationInSeconds = ClampLoopDuration(Settings.LoopDurationInSeconds);
		
		// Setup frame indices
		CurrentFrameIndex = 0;
		UpdateLoopBoundaries();

		// Determine max size of decode buffer
		Settings.MaxDecodeSizeInFrames = FMath::Max(DefaultMinDecodeSizeInFrames, Settings.MaxDecodeSizeInFrames);
		DecoderOutput.SetNumChannels(NumChannels);

		// Prepare to read audio
		bIsDecoderValid = InitializeDecoder(Settings.StartTimeInSeconds);
	}

	/** Create a wave proxy reader. 
	 *
	 * @param InWaveProxy - A TSharedRef of a FSoundWaveProxy which is to be played. 
	 * @param InSettings - Reader settings. 
	 */
	TUniquePtr<FWaveProxyReader> FWaveProxyReader::Create(FSoundWaveProxyRef InWaveProxy, const FSettings& InSettings)
	{
		if (InWaveProxy->GetSampleRate() <= 0.f)
		{
			UE_LOG(LogMetaSound, Warning, TEXT("Cannot create FWaveProxy reader due to invalid sample rate (%f). Package: %s"), InWaveProxy->GetSampleRate(), *InWaveProxy->GetPackageName().ToString());
			return TUniquePtr<FWaveProxyReader>();
		}

		if (InWaveProxy->GetNumChannels() <= 0)
		{
			UE_LOG(LogMetaSound, Warning, TEXT("Cannot create FWaveProxy reader due to invalid num channels (%d). Package: %s"), InWaveProxy->GetNumChannels(), *InWaveProxy->GetPackageName().ToString());
			return TUniquePtr<FWaveProxyReader>();
		}

		if (InWaveProxy->GetNumFrames() <= 0)
		{
			UE_LOG(LogMetaSound, Warning, TEXT("Cannot create FWaveProxy reader due to invalid num frames (%d). Package: %s"), InWaveProxy->GetNumFrames(), *InWaveProxy->GetPackageName().ToString());
			return TUniquePtr<FWaveProxyReader>();
		}

		return TUniquePtr<FWaveProxyReader>(new FWaveProxyReader(InWaveProxy, InSettings));
	}


	/** Set whether the reader should loop the audio or not. */
	void FWaveProxyReader::SetIsLooping(bool bInIsLooping)
	{
		if (Settings.bIsLooping != bInIsLooping)
		{
			Settings.bIsLooping = bInIsLooping;
			UpdateLoopBoundaries();
		}
	}

	/** Sets the beginning position of the loop. */
	void FWaveProxyReader::SetLoopStartTime(float InLoopStartTimeInSeconds)
	{
		InLoopStartTimeInSeconds = ClampLoopStartTime(InLoopStartTimeInSeconds);
		if (!FMath::IsNearlyEqual(Settings.LoopStartTimeInSeconds, InLoopStartTimeInSeconds))
		{
			Settings.LoopStartTimeInSeconds = InLoopStartTimeInSeconds;
			UpdateLoopBoundaries();
		}
	}

	/** Sets the duration of the loop in seconds. If the value is negative, the
	 * loop duration consists of the entire file. */
	void FWaveProxyReader::SetLoopDuration(float InLoopDurationInSeconds)
	{
		InLoopDurationInSeconds = ClampLoopDuration(InLoopDurationInSeconds);
		if (!FMath::IsNearlyEqual(Settings.LoopDurationInSeconds, InLoopDurationInSeconds))
		{
			Settings.LoopDurationInSeconds = InLoopDurationInSeconds;
			UpdateLoopBoundaries();
		}
	}

	bool FWaveProxyReader::SeekToTime(float InSeconds)
	{
		// Direct seeking is not supported. A new decoder must be created. 
		bIsDecoderValid = InitializeDecoder(InSeconds);
		return bIsDecoderValid;
	}

	/** Copies audio into OutBuffer. It returns the number of samples copied.
	 * Samples not written to will be set to zero.
	 */
	int32 FWaveProxyReader::PopAudio(Audio::FAlignedFloatBuffer& OutBuffer)
	{
		using namespace Audio;

		METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::FWaveProxyReader::PopAudio);

		if ((0 == NumChannels) || !bIsDecoderValid)
		{
			if (OutBuffer.Num() > 0)
			{
				FMemory::Memset(OutBuffer.GetData(), 0, sizeof(float) * OutBuffer.Num());
			}
			return 0;
		}

		checkf(0 == (OutBuffer.Num() % NumChannels), TEXT("Output buffer size must be evenly divisible by the number of channels"));

		TArrayView<float> OutBufferView{OutBuffer};
		int32 NumSamplesUnset = OutBuffer.Num();
		int32 NumSamplesCopied = 0;

		bool bDecoderOutputHasMoreData = DecoderOutput.Num() > 0;
		bool bDecoderCanDecodeMoreData = bIsDecoderValid && (EDecodeResult::MoreDataRemaining == DecodeResult);
		bool bCanProduceMoreAudio = bDecoderOutputHasMoreData || bDecoderCanDecodeMoreData;

		while ((NumSamplesUnset > 0) && bCanProduceMoreAudio)
		{
			int32 NumSamplesCopiedThisLoop = PopAudioFromDecoderOutput(OutBufferView);

			// Update sample counters and output buffer view
			NumSamplesCopied += NumSamplesCopiedThisLoop;
			NumSamplesUnset -= NumSamplesCopiedThisLoop;
			OutBufferView = OutBufferView.Slice(NumSamplesCopiedThisLoop, NumSamplesUnset);
			
			if (Settings.bIsLooping)
			{
				// Seek to loop start if we have used up all our decodable samples or 
				// are at the loop boundary.
				if (0 == DecoderOutput.Num())
				{
					if ((!bDecoderCanDecodeMoreData) || (CurrentFrameIndex >= LoopEndFrameIndex))
					{
						SeekToTime(Settings.LoopStartTimeInSeconds);
						bDecoderCanDecodeMoreData = bIsDecoderValid && (EDecodeResult::MoreDataRemaining == DecodeResult);
					}
				}
			}

			// Determine if we can / should decode more data. 
			if ((NumSamplesUnset > 0) && (DecoderOutput.Num() == 0) && bDecoderCanDecodeMoreData)
			{
				// Looping is handle within the FWaveProxyReader instead of 
				// within the decoder.
				DecodeResult = Decoder->Decode(false /* bIsLooping */); 
				bDecoderCanDecodeMoreData = EDecodeResult::MoreDataRemaining == DecodeResult;
			}

			bDecoderOutputHasMoreData = DecoderOutput.Num() > 0;
			bCanProduceMoreAudio = bDecoderOutputHasMoreData || bDecoderCanDecodeMoreData;
		}

		// Zero pad any unset samples. 
		if (OutBufferView.Num() > 0)
		{
			check(!bCanProduceMoreAudio);

			// Zero out audio that was not set.
			FMemory::Memset(OutBufferView.GetData(), 0, sizeof(float) * OutBufferView.Num());
			CurrentFrameIndex += OutBufferView.Num() / NumChannels;
		}

		return NumSamplesCopied;
	}

	int32 FWaveProxyReader::PopAudioFromDecoderOutput(TArrayView<float> OutBufferView)
	{
		using namespace MetasoundWaveProxyReaderPrivate;

		check(NumChannels > 0);

		int32 NumSamplesCopied = 0;
		if (DecoderOutput.Num() > 0)
		{
			// Get samples from the decoder buffer.
			FDecoderOutput::FPushedAudioDetails UnusedDetails;  
			NumSamplesCopied = DecoderOutput.PopAudio(OutBufferView, UnusedDetails);

			int32 NumFramesCopied = NumSamplesCopied / NumChannels;
			CurrentFrameIndex += NumFramesCopied;

			// Check whether the samples copied from the decoder extend 
			// past the end of the loop.
			if (Settings.bIsLooping)
			{
				const bool bDidOvershoot = CurrentFrameIndex >= LoopEndFrameIndex;
				if (bDidOvershoot)
				{
					// Rewind sample counters if the loop boundary was overshot.
					NumFramesCopied -= (CurrentFrameIndex - LoopEndFrameIndex);
					CurrentFrameIndex = LoopEndFrameIndex;
					// If Settings.bIsLooping info was altered, NumFramesCopied can end up 
					// negative if the current frame index is past the loop end frame. 
					NumFramesCopied = FMath::Max(0, NumFramesCopied); 
					NumSamplesCopied = NumFramesCopied * NumChannels;

					// Remove any remaining samples in the decoder because they 
					// are past the end of the loop.
					DecoderOutput.Reset();
				}

			}
		}

		return NumSamplesCopied;
	}

	bool FWaveProxyReader::InitializeDecoder(float InStartTimeInSeconds)
	{
		using namespace Audio;

		METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(MetaSound::FWaveProxyReader::InitializeDecoder);

		// Create decoder input
		FName Format = WaveProxy->GetRuntimeFormat();
		DecoderInput = Audio::CreateBackCompatDecoderInput(Format, WaveProxy);
		if (!DecoderInput.IsValid())
		{
			UE_LOG(LogMetaSound, Error, TEXT("Failed to create decoder input (format:%s) for wave (package:%s)"), *Format.ToString(), *WaveProxy->GetPackageName().ToString());
			return false;
		}

		// Seek input to start time.
		if (!FMath::IsNearlyEqual(0.0f, InStartTimeInSeconds))
		{
			if (WaveProxy->IsSeekable())
			{
				const bool bSeekSucceeded = DecoderInput->SeekToTime(InStartTimeInSeconds);
				if (!bSeekSucceeded)
				{
					UE_LOG(LogMetaSound, Warning, TEXT("Failed to seek decoder input during initialization: (format:%s) for wave (package:%s) to time '%.6f'"),
						*Format.ToString(),
						*WaveProxy->GetPackageName().ToString(),
						InStartTimeInSeconds);
				}
			}
			else
			{
				UE_LOG(LogMetaSound, Warning, TEXT("Attempt to seek on non-seekable wave: (format:%s) for wave (package:%s) to time '%.6f'"),
					*Format.ToString(),
					*WaveProxy->GetPackageName().ToString(),
					InStartTimeInSeconds);
			}
		}
		CurrentFrameIndex = InStartTimeInSeconds * GetSampleRate();

		// Get codec ptr by reading the header info from the decoder input.
		ICodecRegistry::FCodecPtr Codec = ICodecRegistry::Get().FindCodecByParsingInput(DecoderInput.Get());
		if (nullptr == Codec)
		{
			UE_LOG(LogMetaSound, Error, TEXT("Failed to find codec (format:%s) for wave (package:%s)"), *Format.ToString(), *WaveProxy->GetPackageName().ToString());
			return false;
		}

		// Create the decoder
		Decoder = Codec->CreateDecoder(DecoderInput.Get(), &DecoderOutput);
		if (!Decoder.IsValid())
		{
			UE_LOG(LogMetaSound, Error, TEXT("Failed to create decoder (format:%s) for wave (package:%s)"), *Format.ToString(), *WaveProxy->GetPackageName().ToString());
			DecodeResult = EDecodeResult::Fail;
			return false;
		}
		else
		{
			// The DecodeResult needs to be set to a valid state incase the prior
			// decoder finished or failed. 
			DecodeResult = EDecodeResult::MoreDataRemaining;
		}

		// For non-seekable streaming waves, use a fallback method to seek
		// to the start time. 
		const bool bUseFallbackSeekMethod = (GMetaSoundWaveProxyReaderSimulateSeekOnNonSeekable != 0) && (InStartTimeInSeconds != 0.f) && (!WaveProxy->IsSeekable());
		if (bUseFallbackSeekMethod)
		{
			if (!bFallbackSeekMethodWarningLogged)
			{
				UE_LOG(LogMetaSound, Warning, TEXT("Simulating seeking in wave which is not seekable (package:%s). For better performance, set wave to a seekable format"), *WaveProxy->GetPackageName().ToString()); 
				bFallbackSeekMethodWarningLogged = true;
			}

			int32 NumSamplesToDiscard = CurrentFrameIndex * NumChannels;
			DiscardSamples(NumSamplesToDiscard);
		}

		// return true if all the components were successfully create
		return true;
	}

	void FWaveProxyReader::DiscardSamples(int32 InNumSamplesToDiscard)
	{
		while (InNumSamplesToDiscard > 0)
		{
			DecodeResult = Decoder->Decode(false /* bIsLooping */); 

			int32 NumSamplesToDiscardThisLoop = FMath::Min(DecoderOutput.Num(), InNumSamplesToDiscard);

			if (NumSamplesToDiscardThisLoop < 1)
			{
				UE_LOG(LogMetaSound, Error, TEXT("Failed to decode samples (package: %s)"), *WaveProxy->GetPackageName().ToString());
				break;
			}

			int32 ActualNumSamplesDiscarded = DecoderOutput.PopAudio(NumSamplesToDiscardThisLoop);
			InNumSamplesToDiscard -= ActualNumSamplesDiscarded;

			if ((InNumSamplesToDiscard > 0) && (DecodeResult != EDecodeResult::MoreDataRemaining))
			{
				UE_LOG(LogMetaSound, Error, TEXT("Failed to simulate seek (seek to frame: %d, package: %s)"), CurrentFrameIndex, *WaveProxy->GetPackageName().ToString());
				break;
			}
		}
	}

	float FWaveProxyReader::ClampLoopStartTime(float InStartTimeInSeconds)
	{
		return FMath::Clamp(InStartTimeInSeconds, 0.f, MaxLoopStartTimeInSeconds);
	}

	float FWaveProxyReader::ClampLoopDuration(float InDurationInSeconds)
	{
		if (InDurationInSeconds <= 0.f)
		{
			return MaxLoopDurationInSeconds;
		}
		return FMath::Max(InDurationInSeconds, MinLoopDurationInSeconds);
	}

	void FWaveProxyReader::UpdateLoopBoundaries()
	{
		if (Settings.bIsLooping)
		{
			const int32 MinLoopDurationInFrames = FMath::CeilToInt(MinLoopDurationInSeconds * SampleRate);
			const float LoopEndTime = Settings.LoopStartTimeInSeconds + Settings.LoopDurationInSeconds;
			const int32 MinLoopEndFrameIndex = LoopStartFrameIndex + MinLoopDurationInFrames;
			const int32 MaxLoopEndFrameIndex = NumFramesInWave;

			LoopStartFrameIndex = FMath::FloorToInt(Settings.LoopStartTimeInSeconds * SampleRate);
			LoopEndFrameIndex = FMath::Clamp(LoopEndTime * SampleRate, MinLoopEndFrameIndex, MaxLoopEndFrameIndex);
		}
		else
		{
			LoopStartFrameIndex = 0;
			LoopEndFrameIndex = NumFramesInWave;
		}
	}
}
