// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuR/ImagePrivate.h"
#include "MuR/MutableMath.h"


namespace mu
{

	void FImageOperator::ImageCompose( Image* pBase, const Image* pBlock, const box< UE::Math::TIntVector2<uint16> >& rect )
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
			rect.min[0] % finfo.PixelsPerBlockX != 0
			|| rect.min[1] % finfo.PixelsPerBlockY != 0
			|| rect.size[0] % finfo.PixelsPerBlockX != 0
			|| rect.size[1] % finfo.PixelsPerBlockY != 0;

		if (bSubPixelBlockCompose)
		{
			// Bad performance case, but likely a very small image anyway.
			MUTABLE_CPUPROFILER_SCOPE(ImageCompose_SubBlock_Fix);
			EImageFormat UncompressedFormat = GetUncompressedFormat(OriginalFormat);
			constexpr int32 Quality = 0;
			Ptr<Image> TempBase = ImagePixelFormat(Quality, pBase, UncompressedFormat);
			Ptr<Image> TempBlock = ImagePixelFormat(Quality, pBlock, UncompressedFormat);
			ImageCompose( TempBase.get(), TempBlock.get(), rect);
			ReleaseImage(TempBlock);
			Ptr<Image> NewBase = ImagePixelFormat(Quality, TempBase.get(), OriginalFormat);
			ReleaseImage(TempBase);
			// ImageCompose above may have reduced the LODs because of the block lods.
			pBase->CopyMove( NewBase.get() );
			ReleaseImage(NewBase);
			return;
		}
		else
		{
			// Sizes in blocks of the current mips
			UE::Math::TIntVector2<uint16> baseMipSize(
				pBase->GetSizeX() / finfo.PixelsPerBlockX,
				pBase->GetSizeY() / finfo.PixelsPerBlockY);

			UE::Math::TIntVector2<uint16> blockMipPos(
				rect.min[0] / finfo.PixelsPerBlockX,
				rect.min[1] / finfo.PixelsPerBlockY);

			UE::Math::TIntVector2<uint16> blockMipSize(
				rect.size[0] / finfo.PixelsPerBlockX,
				rect.size[1] / finfo.PixelsPerBlockY);

			int doneMips = 0;
			for (; doneMips < pBase->GetLODCount() && doneMips < pBlock->GetLODCount(); ++doneMips)
			{
				//UE_LOG(LogMutableCore, Warning, "Block Mip Pos : %d, %d", blockMipPos[0], blockMipPos[1]);

				// Sizes of a row in bytes
				int baseRowSize = finfo.BytesPerBlock * baseMipSize[0];
				int blockRowSize = finfo.BytesPerBlock * blockMipSize[0];

				uint8_t* pBaseBuf = pBase->GetMipData(doneMips);
				const uint8_t* pBlockBuf = pBlock->GetMipData(doneMips);

				int skipBytes = baseRowSize * blockMipPos[1] + blockMipPos[0] * finfo.BytesPerBlock;

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
					++doneMips;
					break;
				}

				baseMipSize[0] = FMath::DivideAndRoundUp(baseMipSize[0], uint16(2));
				baseMipSize[1] = FMath::DivideAndRoundUp(baseMipSize[1], uint16(2));
				blockMipPos[0] = FMath::DivideAndRoundUp(blockMipPos[0], uint16(2));
				blockMipPos[1] = FMath::DivideAndRoundUp(blockMipPos[1], uint16(2));
				blockMipSize[0] = FMath::DivideAndRoundUp(blockMipSize[0], uint16(2));
				blockMipSize[1] = FMath::DivideAndRoundUp(blockMipSize[1], uint16(2));
			}

			// Adjust the actually valid mips, maybe wasting some of the reserved memory.
			// TODO: Review this.
			pBase->m_lods = (uint8)doneMips;
			pBase->m_data.SetNum(pBase->CalculateDataSize());
		}
    }

}

