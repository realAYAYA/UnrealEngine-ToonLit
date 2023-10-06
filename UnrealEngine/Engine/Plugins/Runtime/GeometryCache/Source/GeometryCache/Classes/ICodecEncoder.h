// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

class FMemoryWriter;
struct FGeometryCacheCodecEncodeArguments;

/**
* Interface to the internal mesh compressor's encoder in order to hide implementation
*/
class ICodecEncoder
{
public:
	virtual ~ICodecEncoder() {}

	/**
	* Encode a frame and write the bitstream
	*
	* @param Writer - Writer for the output bit stream
	* @param Args - Encoding arguments
	*/
	virtual bool EncodeFrameData(FMemoryWriter& Writer, const FGeometryCacheCodecEncodeArguments &Args) = 0;
};