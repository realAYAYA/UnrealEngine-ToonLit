// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/ImagePrivate.h"
#include "MuR/OpImagePixelFormat.h"
#include "Async/ParallelFor.h"
#include "MuR/MutableTrace.h"

#include "Math/Vector.h"
#include "Math/IntVector.h"

namespace mu
{

    //---------------------------------------------------------------------------------------------
    //! Image transform, translate, scale, rotate 
    //---------------------------------------------------------------------------------------------

    template<int32 NC>
	inline void ImageTransform( Image* pDest, const Image* pBase, FVector2f Offset, FVector2f Scale, float RotationRad)
    {
		FIntVector2 BaseSize = FIntVector2(pBase->GetSizeX(), pBase->GetSizeY());
		FIntVector2 DestSize = FIntVector2(pDest->GetSizeX(), pDest->GetSizeY());

		const FVector2f BaseSizeF = FVector2f(BaseSize.X, BaseSize.Y);
		const FVector2f DestSizeF = FVector2f(DestSize.X, DestSize.Y);

		float SinRot, CosRot;
		FMath::SinCos(&SinRot, &CosRot, RotationRad);

		const FVector2f DestNormFactor = FVector2f(1.0f) / DestSizeF;
		const FVector2f BaseNormFactor = FVector2f(1.0f) / BaseSizeF;

		Scale.X = FMath::IsNearlyZero(Scale.X) ? UE_SMALL_NUMBER : Scale.X;
		Scale.Y = FMath::IsNearlyZero(Scale.Y) ? UE_SMALL_NUMBER : Scale.Y;

		FVector2f ScaleReciprocal =  FVector2f(1.0f) / Scale; 

		for (int32 Y = 0; Y < DestSize.Y; ++Y)
		{
			for (int32 X = 0; X < DestSize.X; ++X)
			{
				FVector2f DestUV = (FVector2f(X, Y) + 0.5f) * DestNormFactor - 0.5f;	

				DestUV = FVector2f( CosRot * DestUV.X + SinRot * DestUV.Y,
									CosRot * DestUV.Y - SinRot * DestUV.X);
				DestUV *= ScaleReciprocal;
				DestUV += -Offset + 0.5f;

				const FVector2f Coord = FVector2f(DestUV.X - FMath::Floor(DestUV.X), DestUV.Y - FMath::Floor(DestUV.Y)) * BaseSizeF;

				const FVector2f CoordFloor = FVector2f( FMath::Floor( Coord.X ), FMath::Floor( Coord.Y ) );
				const FVector2f CoordFract = Coord - CoordFloor;

				const FVector2f UV00 = CoordFloor * BaseNormFactor;
				const FVector2f UV01 = ( CoordFloor + FVector2f(0.0f, 1.0f) ) * BaseNormFactor;
				const FVector2f UV10 = ( CoordFloor + FVector2f(1.0f, 0.0f) ) * BaseNormFactor;
				const FVector2f UV11 = ( CoordFloor + FVector2f(1.0f) ) * BaseNormFactor;

				const FVector2f Coord00 =
						FVector2f( UV00.X - FMath::Floor(UV00.X), UV00.Y - FMath::Floor(UV00.Y) ) * BaseSizeF - FVector2f(0.5f);
				const FVector2f Coord01 = 
						FVector2f( UV01.X - FMath::Floor(UV01.X), UV01.Y - FMath::Floor(UV01.Y) ) * BaseSizeF - FVector2f(0.5f);
				const FVector2f Coord10 =
						FVector2f( UV10.X - FMath::Floor(UV10.X), UV10.Y - FMath::Floor(UV10.Y) ) * BaseSizeF - FVector2f(0.5f);
				const FVector2f Coord11 =
						FVector2f( UV11.X - FMath::Floor(UV11.X), UV11.Y - FMath::Floor(UV11.Y) ) * BaseSizeF - FVector2f(0.5f);

				const FIntVector2 S00 = FIntVector2(Coord00.X, Coord00.Y);
				const FIntVector2 S01 = FIntVector2(Coord01.X, Coord01.Y);
				const FIntVector2 S10 = FIntVector2(Coord10.X, Coord10.Y);
				const FIntVector2 S11 = FIntVector2(Coord11.X, Coord11.Y);

				for (int32 Channel = 0; Channel < NC; ++Channel)
				{
					const float V00 = float(pBase->GetData()[(S00.Y*BaseSize.X + S00.X) * NC + Channel]);
					const float V01 = float(pBase->GetData()[(S01.Y*BaseSize.X + S01.X) * NC + Channel]);
					const float V10 = float(pBase->GetData()[(S10.Y*BaseSize.X + S10.X) * NC + Channel]);
					const float V11 = float(pBase->GetData()[(S11.Y*BaseSize.X + S11.X) * NC + Channel]);

					const float V = FMath::Lerp(
							FMath::Lerp(V00, V10, CoordFract.X), 
						    FMath::Lerp(V01, V11, CoordFract.X), CoordFract.Y);

					pDest->GetData()[(Y*DestSize.X + X)*NC + Channel] = static_cast<uint8>(V);
				}	
			}
		}
    }
    
	inline void ImageTransform( Image* pDestImage, const Image* pImage, FVector2f Offset, FVector2f Scale, float RotationRad)

    {
		MUTABLE_CPUPROFILER_SCOPE(ImageTransform)

		switch (pDestImage->GetFormat())
		{
		
		case EImageFormat::IF_L_UBYTE:
		{
			ImageTransform<1>( pDestImage, pImage, Offset, Scale, RotationRad );
			break;
		}
		
		case EImageFormat::IF_RGB_UBYTE:
		{
			ImageTransform<3>( pDestImage, pImage, Offset, Scale, RotationRad );
			break;
		}

		case EImageFormat::IF_BGRA_UBYTE:
		case EImageFormat::IF_RGBA_UBYTE:
		{
			ImageTransform<4>( pDestImage, pImage, Offset, Scale, RotationRad );
			break;
		}

		default:
			// Case not implemented
            check( false );
		}
    }

}
