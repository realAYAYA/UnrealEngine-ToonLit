// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"

#include "ImageWrapperBase.h"

#if WITH_UNREALEXR

THIRD_PARTY_INCLUDES_START
	#include "Imath/ImathBox.h"
	#include "OpenEXR/ImfArray.h"
	#include "OpenEXR/ImfChannelList.h"
	#include "OpenEXR/ImfHeader.h"
	#include "OpenEXR/ImfIO.h"
	#include "OpenEXR/ImfInputFile.h"
	#include "OpenEXR/ImfOutputFile.h"
	#include "OpenEXR/ImfRgbaFile.h"
	#include "OpenEXR/ImfStdIO.h"
	#include "OpenEXR/ImfVersion.h"
THIRD_PARTY_INCLUDES_END


/**
 * OpenEXR implementation of the helper class
 */
class FExrImageWrapper
	: public FImageWrapperBase
{
public:

	/**
	 * Default Constructor.
	 */
	FExrImageWrapper();

public:

	//~ FImageWrapper interface

	virtual bool SetRaw(const void* InRawData, int64 InRawSize, const int32 InWidth, const int32 InHeight, const ERGBFormat InFormat, const int32 InBitDepth, const int32 InBytesPerRow = 0) override;
	virtual bool SetCompressed(const void* InCompressedData, int64 InCompressedSize) override;
	virtual void Compress(int32 Quality) override;
	virtual void Uncompress(const ERGBFormat InFormat, int32 InBitDepth) override;
	
	virtual bool CanSetRawFormat(const ERGBFormat InFormat, const int32 InBitDepth) const override;
	virtual ERawImageFormat::Type GetSupportedRawFormat(const ERawImageFormat::Type InFormat) const override;
};

#endif // WITH_UNREALEXR
