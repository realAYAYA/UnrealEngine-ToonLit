// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

// ue
#include "Containers/ArrayView.h"
#include "Misc/Optional.h"

// eossdk
//#include "eos_platform_prereqs.h"
//#include "eos_base.h"


namespace LibRtc
{
	/** There is the declaration of the single audio sample */
	using FAudioChannelSample = int16_t;

	/** Describes the data format of the audio stream */
	struct FAudioStreamFormat
	{
		/** The number of samples per seconds */
		uint32_t SampleRate;
		/** The number of channels in the stream */
		uint32_t NumberChannels;
	};

	/**
	 * Describes the byte buffer of the audio stream
	 *
	 * One sample - one sample byte * NumberBytesPerSample
	 * One frame - one sample * NumberChannels
	 * One second - one frame * SampleRate
	 * The number of samples per seconds - one sample byte * NumberBytesPerSample * NumberChannels * SampleRate
	 */
	using FAudioByteBuffer = TArrayView<const uint8_t>;

	/**
	 * Describes the sample buffer of the audio stream
	 *
	 * One frame - one sample * NumberChannels
	 * One second - one frame * SampleRate
	 * The number of samples per seconds - one sample byte * NumberBytesPerSample * NumberChannels * SampleRate
	 */
	struct FAudioSampleBuffer
	{
		/** The sequence of audio samples */
		TArrayView<const FAudioChannelSample> AudioSamples;
		/** The format of audio samples */
		FAudioStreamFormat AudioStreamFormat;
	};

	struct FAudioCodecConfig
	{
		int32_t SampleRate;
		int32_t NumChannels;
	};

	/**
	 * The callback is called to encode the sample buffer to the byte-by-byte format.
	 *
	 * The unencoded audio sample buffer is not guaranteed to be available after callback finishes.
	 * The pointer of the encoded audio byte buffer should be valid until the next encode call will happen.
	 *
	 * @param AudioUnencodedSampleBuffer - The sample buffer which is being encoded.
	 * @param [out] OutEncodedAudioByteBuffer - The client side pointer of the byte buffer with encoded information.
	 */
	using OnAudioEncodeCallback = TFunction<void(const FAudioSampleBuffer&, const FAudioByteBuffer& AudioBytes)>;

	/**
	 * The callback is called to decode the byte-by-byte buffer to the sample buffer format.
	 *
	 * The undecoded audio byte buffer is not guaranteed to be available after callback finishes.
	 * The pointer of the decoded audio sample buffer should be valid until the next decode call will happen.
	 *
	 * @param AudioUndecodedByteBuffer - The byte buffer which is being decoded.
	 * @param [out] OutDecodedAudioSampleBuffer - The client side pointer of the sample buffer with decoded information.
	 */
	using OnAudioDecodeCallback = TFunction<void(const FAudioByteBuffer& AudioBytes, const FAudioSampleBuffer&)>;

	struct FAudioCodec
	{
		FAudioCodecConfig AudioCodecConfig;
		OnAudioEncodeCallback OnAudioEncodeCallback;
		OnAudioDecodeCallback OnAudioDecodeCallback;
	};

	/** Information which is used to initialize the incoming audio channel */
	struct FIncomingAudioChannelInfo
	{
		// nothing
	};

	/** Information which is used to initialize the outgoing audio channel */
	struct FOutgoingAudioChannelInfo
	{
		/** Initial muted state of the audio channel */
		bool bIsMuted;
	};

	enum class EAudioMuteReason
	{
		Unsupported,
		Manual,
		NotListening,
		AdminDisabled
	};

	struct FAudioMuteInfo
	{
		TOptional<EAudioMuteReason> Reason;
	};
}
