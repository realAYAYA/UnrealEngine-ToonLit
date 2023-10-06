// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/ImagePrivate.h"
#include "MuR/Platform.h"
#include "Math/VectorRegister.h"

namespace mu
{

	template<bool bUseVectorImpl = false>
	inline void ImageSaturateInPlace(Image* pA, float Factor)
	{
		MUTABLE_CPUPROFILER_SCOPE(ImageSaturateInplace);

		if (!pA)
		{
			return;
		}

		// Clamp the factor
		// TODO: See what happens if we don't
		Factor = FMath::Max(0.0f, Factor);
        int32_t f_8 = (int32_t)(Factor*255);

        uint8_t* pABuf = pA->GetData();

		// Generic implementation
		const int32 PixelCount = (int32)pA->CalculatePixelCount();

		switch ( pA->GetFormat() )
		{
		case EImageFormat::IF_RGB_UBYTE:
		{
			if constexpr (!bUseVectorImpl)
			{
				for (int i = 0; i < PixelCount; ++i)
				{
					int32_t l_16 = 76 * pABuf[3 * i + 0] + 150 * pABuf[3 * i + 1] + 29 * pABuf[3 * i + 2];

					for (int c = 0; c < 3; ++c)
					{
						int32_t d_16 = (pABuf[3 * i + c] << 8) - l_16;
						int32_t r_16 = l_16 + ((d_16 * f_8) >> 8);
						pABuf[3 * i + c] = (uint8_t)FMath::Min((uint32_t)r_16 >> 8, 255u);
					}
				}
			}
			else // if constexpr bUseVectorImpl
			{
				constexpr VectorRegister4Int StaurationFactorRGB = MakeVectorRegisterIntConstant(76, 150, 29, 0);
				const VectorRegister4Int F = VectorIntSet1(static_cast<int32>(Factor * 255.0f));
				const VectorRegister4Int OneMinusF = VectorIntSet1(255 - static_cast<int32>(Factor * 255.0f));
				for (int32 PixelIndex = 0; PixelIndex < PixelCount; ++PixelIndex)
				{
					const VectorRegister4Int Value = MakeVectorRegisterInt(
						static_cast<int32>(pABuf[3 * PixelIndex + 0]), 
						static_cast<int32>(pABuf[3 * PixelIndex + 1]), 
						static_cast<int32>(pABuf[3 * PixelIndex + 2]), 
						0);

					const VectorRegister4Int ScaledValue = VectorIntMultiply(Value, StaurationFactorRGB);
					
					alignas(VectorRegister4Int) int32 IndexableScaledValue[4];
					VectorIntStoreAligned(ScaledValue, IndexableScaledValue);

					// Add scaled values and divide by 255
					const VectorRegister4Int L = VectorShiftRightImmLogical(
						VectorIntMultiply(
							VectorIntSet1(IndexableScaledValue[0] + IndexableScaledValue[1] + IndexableScaledValue[2]), 
							VectorIntSet1(32897)), 23);
					
					const VectorRegister4Int Lerp = VectorIntAdd(VectorIntMultiply(L, OneMinusF), VectorIntMultiply(Value, F));
				
					alignas(VectorRegister4Int) int32 IndexableResult[4];

					// divide by 255 and store. do we need to clamp?
					VectorIntStoreAligned(VectorShiftRightImmLogical(VectorIntMultiply(Lerp, VectorIntSet1(32897)), 23),  IndexableResult);

					pABuf[3 * PixelIndex + 0] = static_cast<uint8>(IndexableResult[0]);
					pABuf[3 * PixelIndex + 1] = static_cast<uint8>(IndexableResult[1]);
					pABuf[3 * PixelIndex + 2] = static_cast<uint8>(IndexableResult[2]);
				}
			}
			break;
		}

        case EImageFormat::IF_RGBA_UBYTE:
		{
			if constexpr (!bUseVectorImpl)
			{
				for (int i = 0; i < PixelCount; ++i)
				{
					int32_t l_16 = 76 * pABuf[4 * i + 0] + 150 * pABuf[4 * i + 1] + 29 * pABuf[4 * i + 2];

					for (int c = 0; c < 3; ++c)
					{
						int32_t d_16 = (pABuf[4 * i + c] << 8) - l_16;
						int32_t r_16 = l_16 + ((d_16 * f_8) >> 8);
						pABuf[4 * i + c] = (uint8_t)FMath::Min((uint32_t)r_16 >> 8, 255u);
					}

					//pABuf[4 * i + 3] = pABuf[4 * i + 3];
				}
			}
			else // if constexpr bUseVectorImpl
			{
				constexpr VectorRegister4Int StaurationFactorRGB = MakeVectorRegisterIntConstant(76, 150, 29, 0);
				const VectorRegister4Int F = VectorIntSet1(static_cast<int32>(Factor * 255.0f));
				const VectorRegister4Int OneMinusF = VectorIntSet1(255 - static_cast<int32>(Factor * 255.0f));
				for (int32 PixelIndex = 0; PixelIndex < PixelCount; ++PixelIndex)
				{
					const VectorRegister4Int Value = MakeVectorRegisterInt(
						static_cast<int32>(pABuf[4 * PixelIndex + 0]), 
						static_cast<int32>(pABuf[4 * PixelIndex + 1]), 
						static_cast<int32>(pABuf[4 * PixelIndex + 2]), 
						0);

					const VectorRegister4Int ScaledValue = VectorIntMultiply(Value, StaurationFactorRGB);
					
					alignas(VectorRegister4Int) int32 IndexableScaledValue[4];
					VectorIntStoreAligned(ScaledValue, IndexableScaledValue);

					// Add scaled values and divide by 255
					const VectorRegister4Int L = VectorShiftRightImmLogical(
						VectorIntMultiply(
							VectorIntSet1(IndexableScaledValue[0] + IndexableScaledValue[1] + IndexableScaledValue[2]), 
							VectorIntSet1(32897)), 23);
					
					const VectorRegister4Int Lerp = VectorIntAdd(VectorIntMultiply(L, OneMinusF), VectorIntMultiply(Value, F));
				
					alignas(VectorRegister4Int) int32 IndexableResult[4];
					// divide by 255 and store. do we need to clamp?
					VectorIntStoreAligned(VectorShiftRightImmLogical(VectorIntMultiply(Lerp, VectorIntSet1(32897)), 23),  IndexableResult);

					pABuf[4 * PixelIndex + 0] = static_cast<uint8>(IndexableResult[0]);
					pABuf[4 * PixelIndex + 1] = static_cast<uint8>(IndexableResult[1]);
					pABuf[4 * PixelIndex + 2] = static_cast<uint8>(IndexableResult[2]);
					//pABuf[4 * PixelIndex + 3] = pABuf[4 * PixelIndex + 3];
				}
			}

            break;
        }

        case EImageFormat::IF_BGRA_UBYTE:
        {
			if constexpr (!bUseVectorImpl)
			{
				for (int i = 0; i < PixelCount; ++i)
				{
					int32_t l_16 = 76 * pABuf[4 * i + 2] + 150 * pABuf[4 * i + 1] + 29 * pABuf[4 * i + 0];

					for (int c = 0; c < 3; ++c)
					{
						int32_t d_16 = (pABuf[4 * i + c] << 8) - l_16;
						int32_t r_16 = l_16 + ((d_16 * f_8) >> 8);
						pABuf[4 * i + c] = (uint8_t)FMath::Min((uint32_t)r_16 >> 8, 255u);
					}

					//pABuf[4 * i + 3] = pABuf[4 * i + 3];
				}
			}
			else // if constexpr bUseVectorImpl
			{
				constexpr VectorRegister4Int StaurationFactorBGR = MakeVectorRegisterIntConstant(29, 150, 76, 0);
				const VectorRegister4Int F = VectorIntSet1(static_cast<int32>(Factor * 255.0f));
				const VectorRegister4Int OneMinusF = VectorIntSet1(255 - static_cast<int32>(Factor * 255.0f));
				for (int32 PixelIndex = 0; PixelIndex < PixelCount; ++PixelIndex)
				{
					const VectorRegister4Int Value = MakeVectorRegisterInt(
						static_cast<int32>(pABuf[4 * PixelIndex + 0]), 
						static_cast<int32>(pABuf[4 * PixelIndex + 1]), 
						static_cast<int32>(pABuf[4 * PixelIndex + 2]), 
						0);

					const VectorRegister4Int ScaledValue = VectorIntMultiply(Value, StaurationFactorBGR);
					
					alignas(VectorRegister4Int) int32 IndexableScaledValue[4];
					VectorIntStoreAligned(ScaledValue, IndexableScaledValue);

					// Add scaled values and divide by 255
					const VectorRegister4Int L = VectorShiftRightImmLogical(
						VectorIntMultiply(
							VectorIntSet1(IndexableScaledValue[0] + IndexableScaledValue[1] + IndexableScaledValue[2]), 
							VectorIntSet1(32897)), 23);
					
					const VectorRegister4Int Lerp = VectorIntAdd(VectorIntMultiply(L, OneMinusF), VectorIntMultiply(Value, F));
				
					alignas(VectorRegister4Int) int32 IndexableResult[4];

					// divide by 255 and store. do we need to clamp?
					VectorIntStoreAligned(VectorShiftRightImmLogical(VectorIntMultiply(Lerp, VectorIntSet1(32897)), 23),  IndexableResult);

					pABuf[4 * PixelIndex + 0] = static_cast<uint8>(IndexableResult[0]);
					pABuf[4 * PixelIndex + 1] = static_cast<uint8>(IndexableResult[1]);
					pABuf[4 * PixelIndex + 2] = static_cast<uint8>(IndexableResult[2]);
					//pABuf[4 * PixelIndex + 3] = pABuf[4 * PixelIndex + 3];
				}
			}
            
			break;
        }

		default:
			check(false);
		}
	}
}
