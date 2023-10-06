// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/ImagePrivate.h"
#include "MuR/Platform.h"

#include "Math/VectorRegister.h"

namespace mu
{

	/** Create a new image inverting the colour (RGB or L) components of an image. Leave alpha untouched. */
	inline ImagePtr ImageInvert(const Image* pA)
	{
		MUTABLE_CPUPROFILER_SCOPE(ImageInvert);

		ImagePtr pDest = new Image(pA->GetSizeX(), pA->GetSizeY(), pA->GetLODCount(), pA->GetFormat(), EInitializationType::NotInitialized);

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
	template<bool bUseVectorImpl = false>
	inline void ImageInvertInPlace(Image* pA)
	{
		MUTABLE_CPUPROFILER_SCOPE(ImageInvertInPlace);
			
		if (!pA)
		{
			return;
		}

		uint8* pABuf = pA->GetData();

		//Generic implementation
		const int32 pixelCount = pA->CalculatePixelCount();

		switch (pA->GetFormat())
		{
		case EImageFormat::IF_L_UBYTE:
		{
			if constexpr (!bUseVectorImpl)
			{
				for (int32 i = 0; i < pixelCount; ++i)
				{
					pABuf[i] = 255 - pABuf[i];
				}
			}
			else
			{
				const int32 PixelRem = pixelCount % 4;

				constexpr VectorRegister4Int One255 = MakeVectorRegisterIntConstant(255, 255, 255, 255);
				for (int32 i = 0; i < pixelCount - PixelRem; i += 4)
				{
					VectorRegister4Int Value = MakeVectorRegisterInt(pABuf[i], pABuf[i + 1], pABuf[i + 2], pABuf[i + 3]);
					Value = VectorIntSubtract(One255, Value);

					alignas(alignof(VectorRegister4Int)) int32 IndexableValue[4];
					VectorIntStoreAligned(Value, IndexableValue);

					pABuf[i + 0] = static_cast<uint8>(IndexableValue[0]);
					pABuf[i + 1] = static_cast<uint8>(IndexableValue[1]);
					pABuf[i + 2] = static_cast<uint8>(IndexableValue[2]);
					pABuf[i + 3] = static_cast<uint8>(IndexableValue[3]);
				}

				for (int32 i = pixelCount - PixelRem; i < pixelCount; ++i)
				{
					pABuf[i] = 255 - pABuf[i];
				}
			}
			break;
		}

		case EImageFormat::IF_RGB_UBYTE:
		{
			if constexpr (!bUseVectorImpl)
			{
				for (int32 i = 0; i < pixelCount; ++i)
				{
					for (int32 c = 0; c < 3; ++c)
					{
						pABuf[i * 3 + c] = 255 - pABuf[i * 3 + c];
					}
				}
			}
			else
			{
				const int32 ElemCount = pixelCount * 3;
				const int32 ElemRem = ElemCount % 4;

				constexpr VectorRegister4Int One255 = MakeVectorRegisterIntConstant(255, 255, 255, 255);
				for (int32 i = 0; i < ElemCount - ElemRem; i += 4)
				{
					VectorRegister4Int Value = MakeVectorRegisterInt(pABuf[i], pABuf[i + 1], pABuf[i + 2], pABuf[i + 3]);
					Value = VectorIntSubtract(One255, Value);

					alignas(alignof(VectorRegister4Int)) int32 IndexableValue[4];
					VectorIntStoreAligned(Value, IndexableValue);

					pABuf[i + 0] = static_cast<uint8>(IndexableValue[0]);
					pABuf[i + 1] = static_cast<uint8>(IndexableValue[1]);
					pABuf[i + 2] = static_cast<uint8>(IndexableValue[2]);
					pABuf[i + 3] = static_cast<uint8>(IndexableValue[3]);
				}

				for (int32 i = ElemCount - ElemRem; i < ElemCount; ++i)
				{
					pABuf[i] = 255 - pABuf[i];
				}
			}
			break;
		}
		case EImageFormat::IF_RGBA_UBYTE:
		case EImageFormat::IF_BGRA_UBYTE:
		{
			if constexpr (!bUseVectorImpl)
			{
				for (int32 i = 0; i < pixelCount; ++i)
				{
					for (int32 c = 0; c < 3; ++c)
					{
						pABuf[i * 4 + c] = 255 - pABuf[i * 4 + c];
					}
				}
			}
			else
			{
				const int32 ElemCount = pixelCount * 4;

				constexpr VectorRegister4Int One255 = MakeVectorRegisterIntConstant(255, 255, 255, 255);

				for (int32 i = 0; i < ElemCount; i += 4)
				{
					VectorRegister4Int Value = MakeVectorRegisterInt(pABuf[i], pABuf[i + 1], pABuf[i + 2], pABuf[i + 3]);
					Value = VectorIntSubtract(One255, Value);

					alignas(alignof(VectorRegister4Int)) int32 IndexableValue[4];
					VectorIntStoreAligned(Value, IndexableValue);

					pABuf[i + 0] = static_cast<uint8>(IndexableValue[0]);
					pABuf[i + 1] = static_cast<uint8>(IndexableValue[1]);
					pABuf[i + 2] = static_cast<uint8>(IndexableValue[2]);
					//pABuf[i + 3] = static_cast<uint8>(IndexableValue[3]);
				}
			}
			break;
		}
		default:
			check(false);
		}
	}
}
