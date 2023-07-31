// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

class FBufferReader;
struct FGeometryCacheMeshData;

/**
* Interface to the internal mesh compressor's decoder in order to hide implementation
*/
class ICodecDecoder
{
public:
	virtual ~ICodecDecoder() {}

	/**
	* Read a frame's bit stream and decode the frame
	*
	* @param Reader - Reader holding the frame's bit stream
	* @param OutMeshData - Decoded mesh
	*/
	virtual bool DecodeFrameData(FBufferReader& Reader, FGeometryCacheMeshData& OutMeshData) = 0;
};