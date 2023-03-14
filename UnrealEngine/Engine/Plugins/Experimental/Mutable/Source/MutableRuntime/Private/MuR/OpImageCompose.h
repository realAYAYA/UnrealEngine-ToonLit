// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/ImagePrivate.h"
#include "MuR/MutableMath.h"
#include "MuR/OpImagePixelFormat.h"


namespace mu
{

	inline void ImageCompose( Image* pBase, const Image* pBlock, const box< vec2<int> >& rect )
	{
		check(pBase && pBlock);
		check(pBase != pBlock);
		check( rect.min[0]>=0 && rect.min[1]>=0 );
		check( rect.size[0]>=0 && rect.size[1]>=0 );
		check( rect.min[0]+rect.size[0]<=pBase->GetSizeX() );
		check( rect.min[1]+rect.size[1]<=pBase->GetSizeY() );
		check( rect.size[0]==pBlock->GetSizeX() );
		check( rect.size[1]==pBlock->GetSizeY() );
        check( pBase->GetFormat()==pBlock->GetFormat() );

        // This assert may be acceptable if the missing lods are due to the image block format size
        // limiting it, or disadjustment due to image scaling down. It is handled below.
        //check( pBase->GetLODCount()<=pBlock->GetLODCount() );

		EImageFormat OriginalFormat = pBase->GetFormat();
		const FImageFormatData& finfo = GetImageFormatData( OriginalFormat );

		// If this is true, we are composing coordinates or sizes that don't fall on pixel format
		// block borders.
		bool bSubPixelBlockCompose = 
			rect.min[0] % finfo.m_pixelsPerBlockX != 0
			|| rect.min[1] % finfo.m_pixelsPerBlockY != 0
			|| rect.size[0] % finfo.m_pixelsPerBlockX != 0
			|| rect.size[1] % finfo.m_pixelsPerBlockY != 0;

		if (bSubPixelBlockCompose)
		{
			// Bad performance case, but likely a very small image anyway.
			MUTABLE_CPUPROFILER_SCOPE(ImageCompose_SubBlock_Fix);
			EImageFormat UncompressedFormat = GetUncompressedFormat(OriginalFormat);
			constexpr int32 Quality = 0;
			Ptr<Image> TempBase = ImagePixelFormat(Quality, pBase, UncompressedFormat);
			Ptr<Image> TempBlock = ImagePixelFormat(Quality, pBlock, UncompressedFormat);
			ImageCompose( TempBase.get(), TempBlock.get(), rect );
			Ptr<Image> NewBase = ImagePixelFormat(Quality, TempBase.get(), OriginalFormat);
			// ImageCompose above may have reduced the LODs because of the block lods.
			pBase->m_lods = NewBase->m_lods;
			pBase->m_data = MoveTemp(NewBase->m_data);
			return;
		}
		else
		{
			// Sizes in blocks of the current mips
			vec2<int> baseMipSize(pBase->GetSizeX() / finfo.m_pixelsPerBlockX,
				pBase->GetSizeY() / finfo.m_pixelsPerBlockY);
			vec2<int> blockMipPos(rect.min[0] / finfo.m_pixelsPerBlockX,
				rect.min[1] / finfo.m_pixelsPerBlockY);
			vec2<int> blockMipSize(rect.size[0] / finfo.m_pixelsPerBlockX,
				rect.size[1] / finfo.m_pixelsPerBlockY);

			int doneMips = 0;
			for (doneMips = 0; doneMips < pBase->GetLODCount() && doneMips < pBlock->GetLODCount(); ++doneMips)
			{
				//UE_LOG(LogMutableCore, Warning, "Block Mip Pos : %d, %d", blockMipPos[0], blockMipPos[1]);

				// Sizes of a row in bytes
				int baseRowSize = finfo.m_bytesPerBlock * baseMipSize[0];
				int blockRowSize = finfo.m_bytesPerBlock * blockMipSize[0];

				uint8_t* pBaseBuf = pBase->GetMipData(doneMips);
				const uint8_t* pBlockBuf = pBlock->GetMipData(doneMips);

				int skipBytes = baseRowSize * blockMipPos[1] + blockMipPos[0] * finfo.m_bytesPerBlock;

				pBaseBuf += skipBytes;
				for (int y = 0; y < blockMipSize[1]; ++y)
				{
					check(pBaseBuf + blockRowSize <= pBase->GetData() + pBase->GetDataSize());
					check(pBlockBuf + blockRowSize <= pBlock->GetData() + pBlock->GetDataSize());
					FMemory::Memcpy(pBaseBuf, pBlockBuf, blockRowSize);
					pBlockBuf += blockRowSize;
					pBaseBuf += baseRowSize;
				}

				// We may not proceed to the next mip, if the layout block size is smaller than the
				// pixel format block size.
				if (blockMipPos[0] % 2 || blockMipPos[1] % 2 || blockMipSize[0] % 2 || blockMipSize[1] % 2)
				{
					break;
				}

				baseMipSize[0] = FMath::DivideAndRoundUp(baseMipSize[0], 2);
				baseMipSize[1] = FMath::DivideAndRoundUp(baseMipSize[1], 2);
				blockMipPos[0] = FMath::DivideAndRoundUp(blockMipPos[0], 2);
				blockMipPos[1] = FMath::DivideAndRoundUp(blockMipPos[1], 2);
				blockMipSize[0] = FMath::DivideAndRoundUp(blockMipSize[0], 2);
				blockMipSize[1] = FMath::DivideAndRoundUp(blockMipSize[1], 2);
			}

			// Adjust the actually valid mips, maybe wasting some of the reserved memory.
			// TODO: Review this.
			pBase->m_lods = (uint8_t)doneMips;
			pBase->m_data.SetNum(pBase->CalculateDataSize());
		}
    }

}

