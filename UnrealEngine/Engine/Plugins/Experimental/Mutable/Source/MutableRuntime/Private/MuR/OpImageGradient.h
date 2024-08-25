// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/ImagePrivate.h"
#include "MuR/Platform.h"


namespace mu
{

	inline void ImageGradient( Image* pDest, const FVector4f& c0, const FVector4f& c1 )
	{
		check(pDest && pDest->GetFormat()==EImageFormat::IF_RGBA_UBYTE);

        uint8* pDestBuf = pDest->GetLODData(0);

		int32 sizeX = pDest->GetSizeX();
		int32 sizeY = pDest->GetSizeX();

		for ( int i=0; i< sizeX; ++i )
		{
			float delta = float(i)/float(sizeX -1);
			FVector3f c = c0 * (1.0f-delta) + c1 * delta;

            uint8 colour[4];
            colour[0] = (uint8)FMath::Max( 0, FMath::Min( 255, int(c[0]*255.0f) ) );
            colour[1] = (uint8)FMath::Max( 0, FMath::Min( 255, int(c[1]*255.0f) ) );
            colour[2] = (uint8)FMath::Max( 0, FMath::Min( 255, int(c[2]*255.0f) ) );
			colour[3] = 255;

			for ( int j=0; j< sizeY; ++j )
			{
				pDestBuf[ (sizeX*j+i)*3 + 0 ] = colour[0];
				pDestBuf[ (sizeX*j+i)*3 + 1 ] = colour[1];
				pDestBuf[ (sizeX*j+i)*3 + 2 ] = colour[2];
			}
		}
	}

}
