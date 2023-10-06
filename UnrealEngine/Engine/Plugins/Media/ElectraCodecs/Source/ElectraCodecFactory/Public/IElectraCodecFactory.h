// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"
#include "Containers/Map.h"
#include "Misc/Variant.h"

#include "IElectraDecoderResourceDelegate.h"
class IElectraDecoder;


class IElectraCodecFactory
{
public:
	virtual ~IElectraCodecFactory() = default;

	/**
	 * Queries whether or not this codec factory can create a decoder or encoder of the specified format.
	 * The codec format string is the "codecs" part (RFC 6381) of a MIME type like "video/mp4" (RFC 6838).
	 * 
	 * For example (back quote characters `` are for clarity only, they do not appear in actual strings):
	 *   `avc1.4d002a`  if the full MIME type were `video/mp4; codecs="avc1.4d002a"`
	 *   `mp4a.40.2`    if the full MIME type were `audio/mp4; codecs="mp4a.40.2"`
	 * 
	 * Note that only a single format must be specified. Should there be several codecs specified in a
	 * MIME type they must be separated and each format queried for individually.
	 * 
	 * Format profiles may be omitted if they are not known. Asking for an `avc1` decoder might result
	 * in a supported decoder, but there is no guarantee that it is capable of decoding every profile.
	 * 
	 * Format names are case sensitive. Profile case depends on the format.
	 * 
	 * Additional options may be provided to specify the format more closely, which may help selecting
	 * the best suited implementation. See below.
	 * 
	 * Returns 0 if not supported.
	 * If supported the return value indicates a priority value. If multiple factories are registered
	 * that claim support for the format the one with the highest priority is chosen.
	 */
	virtual int32 SupportsFormat(const FString& InCodecFormat, bool bInEncoder, const TMap<FString, FVariant>& InOptions) const = 0;


	/**
	 * Populates the provided map with decoder configuration options (see IElectraDecoderFeature).
	 * Some required options must be available through the factory prior to creating a decoder instance.
	 */
	virtual void GetConfigurationOptions(TMap<FString, FVariant>& OutOptions) const = 0;


	/**
	 * Called to create a decoder for the given format.
	 */
	virtual TSharedPtr<IElectraDecoder, ESPMode::ThreadSafe> CreateDecoderForFormat(const FString& InCodecFormat, const TMap<FString, FVariant>& InOptions, TSharedPtr<IElectraDecoderResourceDelegate, ESPMode::ThreadSafe> InResourceDelegate) = 0;
};

/*
	Additional decoder options:

	General:
		Any option starting with a `$` character are those provided by the container (eg. the mp4 file)
		and are not exactly standardized.

	Audio decoder:

		"channel_configuration" (uint32)
			- AAC audio: channel configuration as per ISO 23008-1 Section 8.2

		"num_channels" (int32)
			- Number of input channels

		"csd" (ByteArray)
			- AAC audio: codec specific data
		
		"dcr" (ByteArray)
			- AAC audio: decoder configuration record

		"$chan_box" (ByteArray)
		    - The raw contents of the mp4 'chan' box if it exists without the first 8 bytes (size and atom)

		"$enda_box" (ByteArray)
		    - The raw contents of the mp4 'enda' box if it exists without the first 8 bytes (size and atom)
		
		"$FormatSpecificFlags" (int64)
			- The "FormatSpecificFlags" from a version 2 AudioSampleEntry

	
	Video decoder:

		"width" (uint32)
			Width in pixels of the decoded image (active pixels after cropping). May be 0 if unknown.

		"height" (uint32)
			Height in pixels of the decoded image (active pixels after cropping). May be 0 if unknown.
		
		"bps" (int64)
			Bits per second of the video stream. May be <=0 if unknown.

		"fps" (double)
			Frames per second of the video stream. May be <=0 if unknown.

		"max_width"
		"max_height"
		"max_bps"
		"max_fps"
			Same types as the ones without "max_". The max values that are expected to be used.
			When decoding an adaptive stream the max values are the ones of the highest stream
			whereas the non-max ones are that of the current stream.

		"max_codecprofile" (FString)
			A codec profile string like `avc1.4d002a` indicating the maximum decoder profile and level
			that is expected to be handled.

		"csd" (ByteArray)
			- H.264 & H.265: codec specific data

		"dcr" (ByteArray)
			- H.264 & H.265: decoder configuration record
*/
