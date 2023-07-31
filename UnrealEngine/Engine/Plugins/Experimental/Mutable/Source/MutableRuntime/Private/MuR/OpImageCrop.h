// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/ImagePrivate.h"
#include "MuR/MutableMath.h"
#include "MuR/Platform.h"
#include "MuR/OpImagePixelFormat.h"


namespace mu
{

	inline ImagePtr ImageCrop( int imageCompressionQuality, const Image* pBaseReal, const box< vec2<int> >& rect )
	{
		ImagePtrConst pBase = pBaseReal;

		// RLE are not supported yet
		if ( pBaseReal->GetFormat()==EImageFormat::IF_RGBA_UBYTE_RLE )
		{
            pBase = ImagePixelFormat( imageCompressionQuality, pBase.get(), EImageFormat::IF_RGBA_UBYTE );
        }
		else if ( pBaseReal->GetFormat()==EImageFormat::IF_RGB_UBYTE_RLE )
		{
            pBase = ImagePixelFormat( imageCompressionQuality, pBase.get(), EImageFormat::IF_RGB_UBYTE );
        }
		else if ( pBaseReal->GetFormat()==EImageFormat::IF_L_UBYTE_RLE )
		{
            pBase = ImagePixelFormat( imageCompressionQuality, pBase.get(), EImageFormat::IF_L_UBYTE );
        }

		check( pBase );

		ImagePtr pCropped = new Image
				(
					rect.size[0], rect.size[1],
					1,
					pBase->GetFormat()
					);

//		check( rect.min[0]>=0 && rect.min[1]>=0 );
//		check( rect.size[0]>=0 && rect.size[1]>=0 );
//		check( rect.min[0]+rect.size[0]<=pBase->GetSizeX() );
//		check( rect.min[1]+rect.size[1]<=pBase->GetSizeY() );
		// TODO: better error control. This happens if some layouts are corrupt.
		bool correct =
				( rect.min[0]>=0 && rect.min[1]>=0 ) &&
				( rect.size[0]>=0 && rect.size[1]>=0 ) &&
				( rect.min[0]+rect.size[0]<=pBase->GetSizeX() ) &&
				( rect.min[1]+rect.size[1]<=pBase->GetSizeY() );
		if (!correct)
		{
			return pCropped;
		}


		const FImageFormatData& finfo = GetImageFormatData( pBase->GetFormat() );

		// For now
		check( finfo.m_pixelsPerBlockX == 1 );
		check( finfo.m_pixelsPerBlockY == 1 );

		checkf( rect.min[0] % finfo.m_pixelsPerBlockX == 0, TEXT("Rect must snap to blocks.") );
		checkf( rect.min[1] % finfo.m_pixelsPerBlockY == 0, TEXT("Rect must snap to blocks.") );
		checkf( rect.size[0] % finfo.m_pixelsPerBlockX == 0, TEXT("Rect must snap to blocks.") );
		checkf( rect.size[1] % finfo.m_pixelsPerBlockY == 0, TEXT("Rect must snap to blocks.") );

		int baseRowSize = finfo.m_bytesPerBlock * pBase->GetSizeX() / finfo.m_pixelsPerBlockX;
		int cropRowSize = finfo.m_bytesPerBlock * rect.size[0] / finfo.m_pixelsPerBlockX;

        const uint8_t* pBaseBuf = pBase->GetData();
        uint8_t* pCropBuf = pCropped->GetData();

		int skipPixels = pBase->GetSizeX() * rect.min[1] + rect.min[0];
		pBaseBuf += finfo.m_bytesPerBlock * skipPixels / finfo.m_pixelsPerBlockX;
		for ( int y=0; y<rect.size[1]; ++y )
		{
			memcpy( pCropBuf, pBaseBuf, cropRowSize );
			pCropBuf += cropRowSize;
			pBaseBuf += baseRowSize;
		}

		return pCropped;
	}

}
