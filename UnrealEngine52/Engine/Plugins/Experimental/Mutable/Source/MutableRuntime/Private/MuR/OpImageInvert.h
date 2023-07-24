// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/ImagePrivate.h"
#include "MuR/Platform.h"

namespace mu
{

	/** Create a new image inverting the colour (RGB or L) components of an image. Leave alpha untouched. */
	inline ImagePtr ImageInvert(const Image* pA)
	{
		MUTABLE_CPUPROFILER_SCOPE(ImageInvert);

		ImagePtr pDest = new Image(pA->GetSizeX(), pA->GetSizeY(), pA->GetLODCount(), pA->GetFormat());

		uint8* pDestBuf = pDest->GetData();
		const uint8* pABuf = pA->GetData();

		//Generic implementation
		int32 pixelCount = pA->CalculatePixelCount();

		switch (pA->GetFormat())
		{
		case EImageFormat::IF_L_UBYTE:
		{
			for (int32 i = 0; i < pixelCount; ++i)
			{
				pDestBuf[i] = 255 - pABuf[i];
			}
			break;
		}

		case EImageFormat::IF_RGB_UBYTE:
		{
			for (int32 i = 0; i < pixelCount; ++i)
			{
				for (int c = 0; c < 3; ++c)
				{
					pDestBuf[i * 3 + c] = 255 - pABuf[i * 3 + c];
				}
			}
			break;
		}
		case EImageFormat::IF_RGBA_UBYTE:
		case EImageFormat::IF_BGRA_UBYTE:
		{
			for (int32 i = 0; i < pixelCount; ++i)
			{
				for (int32 c = 0; c < 3; ++c)
				{
					pDestBuf[i * 4 + c] = 255 - pABuf[i * 4 + c];
				}

				pDestBuf[i * 4 + 3] = pABuf[i * 4 + 3];
			}
			break;
		}
		default:
			check(false);
		}

		return pDest;
	}

	/** Invert the colour (RGB or L) components of an image. Leave alpha untouched. */
	inline void ImageInvertInPlace(Image* pA)
	{
		MUTABLE_CPUPROFILER_SCOPE(ImageInvertInPlace);
			
		if (!pA)
		{
			return;
		}

		uint8* pABuf = pA->GetData();

		//Generic implementation
		int32 pixelCount = pA->CalculatePixelCount();

		switch (pA->GetFormat())
		{
		case EImageFormat::IF_L_UBYTE:
		{
			for (int32 i = 0; i < pixelCount; ++i)
			{
				pABuf[i] = 255 - pABuf[i];
			}
			break;
		}

		case EImageFormat::IF_RGB_UBYTE:
		{
			for (int32 i = 0; i < pixelCount; ++i)
			{
				for (int32 c = 0; c < 3; ++c)
				{
					pABuf[i * 3 + c] = 255 - pABuf[i * 3 + c];
				}
			}
			break;
		}
		case EImageFormat::IF_RGBA_UBYTE:
		case EImageFormat::IF_BGRA_UBYTE:
		{
			for (int32 i = 0; i < pixelCount; ++i)
			{
				for (int32 c = 0; c < 3; ++c)
				{
					pABuf[i * 4 + c] = 255 - pABuf[i * 4 + c];
				}
			}
			break;
		}
		default:
			check(false);
		}
	}
}
