// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DSP/BufferVectorOperations.h"
#include "HAL/Platform.h"
#include "IAudioCodec.h"
#include "Templates/SharedPointer.h"
#include "Templates/UniquePtr.h"

// Forward declare
class FSoundWaveProxy;

namespace Metasound
{
	namespace MetasoundWaveProxyReaderPrivate
	{
		/** FDecoderOutput maintains a circular buffer of audio which is produced
		 * from the decoder. 
		 */
		class FDecoderOutput : public Audio::IDecoderOutput
		{
			static constexpr uint32 MinNumFramesPerDecode = 1;
			static constexpr uint32 DefaultNumChannels = 1;
			static constexpr uint32 MinNumChannels = 1;

		public:
			using FPushedAudioDetails = Audio::IDecoderOutput::FPushedAudioDetails;

			/** Construct a FDecoderOutput
			 *
			 * @param InNumFramesPerDecode - Maximum number of frames pushed when
			 *                               audio is decoded. The buffer will be 
			 *                               able to hold twice the number of frames. 
			 */
			FDecoderOutput(uint32 InNumFramesPerDecode);

			/** Set the number of channels in the audio being decoded. */
			void SetNumChannels(uint32 InNumChannels);

			/** Returns the number of samples in the buffer. */
			FORCEINLINE int32 Num() const
			{
				return Buffer.Num();
			}

			/** Removes all samples from the buffer. */
			void Reset();

			/** Returns requirements used by the audio codec system. */
			virtual Audio::IDecoderOutput::FRequirements GetRequirements(const Audio::FDecodedFormatInfo& InFormat) const override;

			/** Adds samples to the buffer. 
			 *
			 * This is called by the Decoder and should not be called otherwise.
			 */
			virtual int32 PushAudio(const Audio::IDecoderOutput::FPushedAudioDetails& InDetails, TArrayView<const int16> In16BitInterleave) override;

			/** Adds samples to the buffer. 
			 *
			 * This is called by the Decoder and should not be called otherwise.
			 */
			virtual int32 PushAudio(const FPushedAudioDetails& InDetails, TArrayView<const float> InFloat32Interleave) override;

			/** This should not be called. It removes 16 bit PCM samples from the buffer
			 * which is an unsupported operation of this class. 
			 */
			virtual int32 PopAudio(TArrayView<int16> InExternalInt16Buffer, FPushedAudioDetails& OutDetails) override;

			/** Copy samples to OutBuffer and remove them from this objects internal
			 * buffer. 
			 *
			 *
			 * @param OutBuffer - A destination array to copy samples to.
			 * @param OutDetails - Unused. 
			 *
			 * @return The actual number of samples copied. 
			 */
			virtual int32 PopAudio(TArrayView<float> OutBuffer, FPushedAudioDetails& OutDetails) override;

			/** Remove samples from the internal buffer.
			 *
			 * @param InNumSamples - The desired number of samples to remove.
			 *
			 * @return The actual number of samples removed. 
			 */
			FORCEINLINE int32 PopAudio(int32 InNumSamples)
			{
				return Buffer.Pop(InNumSamples);
			}

		private:

			// Initialize buffer size.
			void Init();
			int32 PushAudioInternal(const FPushedAudioDetails& InDetails, TArrayView<const float> InBuffer);
			int32 PushAudioInternal(const FPushedAudioDetails& InDetails, TArrayView<const int16> InBuffer);

			uint32 NumFramesPerDecode = MinNumFramesPerDecode;
			uint32 NumChannels = DefaultNumChannels;
			Audio::TCircularAudioBuffer<float> Buffer;
			Audio::FAlignedFloatBuffer SampleConversionBuffer;
		};
	}
	
	/** FWaveProxyReader reads a FWaveProxy and outputs 32 bit interleaved audio.
	 *
	 * FWaveProxyReader provides controls for looping and relevant frame index
	 * values.
	 */
	class FWaveProxyReader
	{

	public:
		using FSoundWaveProxyRef = TSharedRef<FSoundWaveProxy, ESPMode::ThreadSafe>;
		using EDecodeResult = Audio::IDecoder::EDecodeResult;

		/** Minimum number of frames to decode per a call to the decoder.  */
		static constexpr uint32 DefaultMinDecodeSizeInFrames = 128;

		/** Some codecs have strict requirements on decode size. In order to be
		 * functional with all supported codecs, the decode size must be a multiple
		 * of 128.
		 */
		static constexpr uint32 DecodeSizeQuantizationInFrames = 128;

		static uint32 ConformDecodeSize(uint32 InMaxDesiredDecodeSizeInFrames);

		/** Minimum duration of a loop. */
		static constexpr float MinLoopDurationInSeconds = 0.05f;

		/** Maximum duration of a loop.
		 *
		 * 10 years. ridiculously high. This exists to prevent floating point 
		 * undefined overflow behavior if performing calculations with 
		 * TNumericLimits<float>::Max().
		 */
		static constexpr float MaxLoopDurationInSeconds = 60.f * 60.f * 365.f * 10.f; 

		/** Settings for a FWaveProxyReader. */
		struct FSettings
		{
			uint32 MaxDecodeSizeInFrames = 8192;
			float StartTimeInSeconds = 0.f;
			bool bIsLooping = false;
			float LoopStartTimeInSeconds = 0.f;
			float LoopDurationInSeconds = 0.f;
		};

	private:
		/** Construct a wave proxy reader. 
		 *
		 * @param InWaveProxy - A TSharedRef of a FSoundWaveProxy which is to be played. 
		 * @param InSettings - Reader settings. 
		 */
		FWaveProxyReader(FSoundWaveProxyRef InWaveProxy, const FSettings& InSettings);

	public:
		/** Create a wave proxy reader. 
		 *
		 * @param InWaveProxy - A TSharedRef of a FSoundWaveProxy which is to be played. 
		 * @param InSettings - Reader settings. 
		 */
		static TUniquePtr<FWaveProxyReader> Create(FSoundWaveProxyRef InWaveProxy, const FSettings& InSettings);

		/** Set whether the reader should loop the audio or not. */
		void SetIsLooping(bool bInIsLooping);

		/** Returns true if the audio will be looped, false otherwise. */
		FORCEINLINE bool IsLooping() const
		{
			return Settings.bIsLooping;
		}

		/** Sets the beginning position of the loop. */
		void SetLoopStartTime(float InLoopStartTimeInSeconds);

		/** Sets the duration of the loop in seconds. 
		 *
		 * If the value is negative, the MaxLoopDurationInSeconds will be used
		 * which will effectively loop at the end of the file. 
		 */
		void SetLoopDuration(float InLoopDurationInSeconds);

		FORCEINLINE float GetSampleRate() const
		{
			return SampleRate;
		}

		FORCEINLINE int32 GetNumChannels() const
		{
			return NumChannels;
		}

		/** Returns the index of the playhead within the complete wave. */
		FORCEINLINE int32 GetFrameIndex() const
		{
			return CurrentFrameIndex;
		}

		FORCEINLINE int32 GetNumFramesInWave() const
		{
			return NumFramesInWave;
		}

		FORCEINLINE int32 GetNumFramesInLoop() const
		{
			return LoopEndFrameIndex - LoopStartFrameIndex;
		}

		FORCEINLINE int32 GetLoopStartFrameIndex() const
		{
			return LoopStartFrameIndex;
		}

		FORCEINLINE int32 GetLoopEndFrameIndex() const
		{
			return LoopEndFrameIndex;
		}

		/** Seeks to position in wave. 
		 * 
		 * @param InSeconds - The location to seek the playhead
		 *
		 * @return true on success, false on failuer.
		 */
		bool SeekToTime(float InSeconds);

		/** Copies audio into OutBuffer. It returns the number of samples copied.
		 * Samples not written to will be set to zero.
		 */
		int32 PopAudio(Audio::FAlignedFloatBuffer& OutBuffer);

	private:

		int32 PopAudioFromDecoderOutput(TArrayView<float> OutBufferView);
		bool InitializeDecoder(float InStartTimeInSeconds);
		void DiscardSamples(int32 InNumSamplesToDiscard);
		float ClampLoopStartTime(float InStartTimeInSeconds);
		float ClampLoopDuration(float InDurationInSeconds);
		void UpdateLoopBoundaries();

		FSoundWaveProxyRef WaveProxy;
		TUniquePtr<Audio::IDecoderInput> DecoderInput;
		TUniquePtr<Audio::IDecoder> Decoder;
		MetasoundWaveProxyReaderPrivate::FDecoderOutput DecoderOutput;

		FSettings Settings;

		float SampleRate = 0.f;
		int32 NumChannels = 0;
		EDecodeResult DecodeResult = EDecodeResult::MoreDataRemaining;
		int32 NumFramesInWave = 0;
		int32 CurrentFrameIndex = 0;
		int32 LoopStartFrameIndex = 0;
		int32 LoopEndFrameIndex = -1;
		float DurationInSeconds = 0.f;
		float MaxLoopStartTimeInSeconds = 0.f;

		bool bIsDecoderValid = false;
		bool bFallbackSeekMethodWarningLogged = false;
	};
}
