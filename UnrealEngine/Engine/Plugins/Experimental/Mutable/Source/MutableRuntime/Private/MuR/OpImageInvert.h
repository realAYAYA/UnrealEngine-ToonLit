// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/ImagePrivate.h"
#include "MuR/Platform.h"

namespace mu
{

	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------
	inline ImagePtr ImageInvert(const Image* pA)
	{
		MUTABLE_CPUPROFILER_SCOPE(ImageInvert)

		ImagePtr pDest = new Image(pA->GetSizeX(), pA->GetSizeY(), pA->GetLODCount(), pA->GetFormat());

		uint8_t* pDestBuf = pDest->GetData();
		const uint8_t* pABuf = pA->GetData();

		//Generic implementation
		int pixelCount = (int)pA->CalculatePixelCount();

		switch (pA->GetFormat())
		{
		case EImageFormat::IF_L_UBYTE:
		{
			for (int i = 0; i < pixelCount; ++i)
			{
				pDestBuf[i] = 255 - pABuf[i];
			}
			break;
		}
		
		case EImageFormat::IF_RGB_UBYTE:
		{
			for (int i = 0; i < pixelCount; ++i)
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
			for (int i = 0; i < pixelCount; ++i)
			{
				for (int c = 0; c < 3; ++c)
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
}
