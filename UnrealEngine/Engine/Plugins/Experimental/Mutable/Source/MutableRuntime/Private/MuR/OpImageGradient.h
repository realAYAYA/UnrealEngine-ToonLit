// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/ImagePrivate.h"
#include "MuR/Platform.h"



namespace mu
{

	inline ImagePtr ImageGradient( const vec3<float>& c0,
								   const vec3<float>& c1,
								   int sizeX, int sizeY )
	{
		ImagePtr pDest = new Image((uint16)sizeX, (uint16)sizeY, 1, EImageFormat::IF_RGB_UBYTE );

        uint8_t* pDestBuf = pDest->GetData();

		for ( int i=0; i<sizeX; ++i )
		{
			float delta = float(i)/float(sizeX-1);
			vec3<float> c = c0 * (1.0f-delta) + c1 * delta;

            uint8_t colour[4];
            colour[0] = (uint8_t)FMath::Max( 0, FMath::Min( 255, int(c[0]*255.0f) ) );
            colour[1] = (uint8_t)FMath::Max( 0, FMath::Min( 255, int(c[1]*255.0f) ) );
            colour[2] = (uint8_t)FMath::Max( 0, FMath::Min( 255, int(c[2]*255.0f) ) );
			colour[3] = 255;

			for ( int j=0; j<sizeY; ++j )
			{
				pDestBuf[ (sizeX*j+i)*3 + 0 ] = colour[0];
				pDestBuf[ (sizeX*j+i)*3 + 1 ] = colour[1];
				pDestBuf[ (sizeX*j+i)*3 + 2 ] = colour[2];
			}
		}

		return pDest;
	}

}
