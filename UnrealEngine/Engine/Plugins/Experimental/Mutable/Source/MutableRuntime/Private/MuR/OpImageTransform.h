// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/ImagePrivate.h"
#include "MuR/MutableTrace.h"

#include "MuR/OpImageTransformPrivate.h"

namespace mu
{
	void ImageTransform(Image* pDestImage, const Image* pImage, FVector2f Offset, FVector2f Scale, float RotationRad, EAddressMode AddressMode)
    {
		MUTABLE_CPUPROFILER_SCOPE(ImageTransform)

		switch (pDestImage->GetFormat())
		{
		
		case EImageFormat::IF_L_UBYTE:
		{
			switch (AddressMode)
			{
			case EAddressMode::ClampToBlack:
				ImageTransformImpl<1, EAddressMode::ClampToBlack>(pDestImage, pImage, Offset, Scale, RotationRad);
				break;
			case EAddressMode::ClampToEdge:
				ImageTransformImpl<1, EAddressMode::ClampToEdge>(pDestImage, pImage, Offset, Scale, RotationRad);
				break;
			case EAddressMode::Wrap:
				ImageTransformImpl<1, EAddressMode::Wrap>(pDestImage, pImage, Offset, Scale, RotationRad);
				break;
			default:
				check(false);
				break;
			}
			break;
		}
		
		case EImageFormat::IF_RGB_UBYTE:
		{
			switch (AddressMode)
			{
			case EAddressMode::ClampToBlack:
				ImageTransformImpl<3, EAddressMode::ClampToBlack>(pDestImage, pImage, Offset, Scale, RotationRad);
				break;
			case EAddressMode::ClampToEdge:
				ImageTransformImpl<3, EAddressMode::ClampToEdge>(pDestImage, pImage, Offset, Scale, RotationRad);
				break;
			case EAddressMode::Wrap:
				ImageTransformImpl<3, EAddressMode::Wrap>(pDestImage, pImage, Offset, Scale, RotationRad);
				break;
			default:
				check(false);
				break;
			}
			break;
		}

		case EImageFormat::IF_BGRA_UBYTE:
		case EImageFormat::IF_RGBA_UBYTE:
		{
			switch (AddressMode)
			{
			case EAddressMode::ClampToBlack:
				ImageTransformImpl<4, EAddressMode::ClampToBlack>(pDestImage, pImage, Offset, Scale, RotationRad);
				break;
			case EAddressMode::ClampToEdge:
				ImageTransformImpl<4, EAddressMode::ClampToEdge>(pDestImage, pImage, Offset, Scale, RotationRad);
				break;
			case EAddressMode::Wrap:
				ImageTransformImpl<4, EAddressMode::Wrap>(pDestImage, pImage, Offset, Scale, RotationRad);
				break;
			default:
				check(false);
				break;
			}
			break;
		}

		default:
			// Case not implemented
            check( false );
		}
    }

}
