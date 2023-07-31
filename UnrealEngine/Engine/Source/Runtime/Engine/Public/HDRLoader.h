// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"

// http://radsite.lbl.gov/radiance/refer/Notes/picture_format.html
// http://paulbourke.net/dataformats/pic/

/** To load the HDR file image format. Does not support all possible types HDR formats (e.g. xyze is not supported) */
class UE_DEPRECATED(5.0, "HDR Loader as been replaced by the FHdrImageWrapper which can be found in the image wrapper module.") FHDRLoadHelper;
class FHDRLoadHelper
{
public:
	ENGINE_API FHDRLoadHelper(const uint8* Buffer, uint32 BufferLength);

	ENGINE_API bool IsValid() const;

	ENGINE_API uint32 GetWidth() const;

	ENGINE_API uint32 GetHeight() const;

	/** @param OutDDSFile order in bytes: RGBE */
	ENGINE_API void ExtractDDSInRGBE(TArray<uint8>& OutDDSFile) const;

private:
	/** 0 if not valid */
	const uint8* RGBDataStart;
	/** 0 if not valid */
	uint32 Width;
	/** 0 if not valid */
	uint32 Height;

	/** @param OutData[Width * Height * 4] will be filled in this order: R,G,B,0, R,G,B,0,... */
	void DecompressWholeImage(uint32* OutData) const;

	void GetHeaderLine(const uint8*& BufferPos, char Line[256]) const;

	/** @param Out order in bytes: RGBE */
	void DecompressScanline(uint8* Out, const uint8*& In) const;

	static void OldDecompressScanline(uint8* Out, const uint8*& In, uint32 Len);
};
