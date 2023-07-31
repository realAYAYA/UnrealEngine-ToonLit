// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/FrameRate.h"

//THIRDPARTY_INCLUDES_START
#include <dnx_uncompressed_sdk.h>
//THIRDPARTY_INCLUDES_END

// Forward Declares
struct _DNX_Encoder;
using DNX_Encoder = struct _DNX_Encoder*;
using DNXMXF_Writer = struct DNXMXF_Writer;

/** 
* Options to initialize the AvidDNx encoder with. Choosing compression will choose the AvidDNxHR HD compression.
*/
struct FAvidDNxEncoderOptions
{
	FAvidDNxEncoderOptions()
		: OutputFilename()
		, Width(0)
		, Height(0)
		, FrameRate(30, 1)
		, bCompress(true)
		, NumberOfEncodingThreads(0)
	{}

	/** The absolute path on disk to try and save the video file to. */
	FString OutputFilename;

	/** The width of the video file. */
	uint32 Width;

	/** The height of the video file. */
	uint32 Height;

	/** Frame Rate of the output video. */
	FFrameRate FrameRate;
	
	/** Should we use a compression codec or not */
	bool bCompress;
	
	/** Number of Encoding Threads. Must be at least 1. */
	uint32 NumberOfEncodingThreads;
};

/**
* Encoder class that takes sRGB 8-bit RGBA data and encodes it to AvidDNxHR or AvidDNxHD before placing it in an mxf container.
* The mxf container writer currently implemented does not support audio, so audio writing APIs have been omitted from this encoder.
*/
class AVIDDNXMEDIA_API FAvidDNxEncoder
{
public:
	FAvidDNxEncoder(const FAvidDNxEncoderOptions& InOptions);
	~FAvidDNxEncoder();

	/** Call to initialize the encoder. This must be done before attempting to write data to it. */
	bool Initialize();

	/** Finalize the video file and finish writing it to disk. Called by the destructor if not automatically called. */
	void Finalize();

	/** Appends a new frame onto the output file. */
	bool WriteFrame(const uint8* InFrameData);

private:
	bool InitializeCompressedEncoder();
	bool InitializeUncompressedEncoder();
	bool InitializeMXFWriter();
public:
	struct FY0CbY1Cr
	{
		uint8 Y0, Cb, Y1, Cr;
	};

private:
	FAvidDNxEncoderOptions Options;
	bool bInitialized;
	bool bFinalized;
	
	/** How big each video sample is after compression based on given settings. */
	int32 EncodedBufferSize;
	
	/** Encoder used for compressed output. */
	DNX_Encoder DNxHRencoder;

	/** Encoder used for uncompressed output. */
	DNXUncompressed_Encoder* DNxUncEncoder;

	DNXUncompressed_CompressedParams_t DNxUncCompressedParams;
	DNXUncompressed_UncompressedParams_t DNxUncUncompressedParams;

	DNXMXF_Writer* MXFwriter;
};