// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Math/UnrealMathSSE.h"
#include "MuR/Image.h"
/** This define enabled additional checks when using RLE compression. These checks have a lot of
* of overhead so it should usually be disabled.
*/
//#define MUTABLE_DEBUG_RLE


namespace mu
{

    //---------------------------------------------------------------------------------------------
    //! Compress to RLE if the compressed image fits in destData, which should be preallocated.
    //! If it doesn't fit, it returns 0, and the content of destData is undefined.
    //! This method doesn't allocate any memory.
    //!
    //! This RLE format compresses the data per row. At the beginning of the data there is the
    //! total compressed data size (uint32_t) followed by the row offsets for direct access.
    //! Every offset is a uint32_t starting at the begining of the compressed data.
    //! Every row consists of a series of header + data. The headers is:
    //!     - a UINT16 with how many equal pixels are there
    //!     - a UINT8 of how many different pixels follow the equal pixels
    //!		- the value of the "equal" pixels
    //! After the header there are as many pixel values as different pixels.
    //---------------------------------------------------------------------------------------------
	MUTABLERUNTIME_API extern uint32_t CompressRLE_L( int width, int rows,
                                   const uint8_t* pBaseData,
                                   uint8_t* destData,
                                   uint32_t destDataSize );

    //---------------------------------------------------------------------------------------------
	MUTABLERUNTIME_API extern uint32_t UncompressRLE_L( int width, int rows,
                                     const uint8_t* pBaseData,
                                     uint8_t* pDestData );

    //---------------------------------------------------------------------------------------------
    //! This RLE format compresses the data per row. At the beginning of the data there are the row
    //! offsets for direct access.
    //! Every row consists of a series of blocks. The blocks are:
    //!     - a UINT16 with how many 0 pixels are there
    //!     - a UINT16 with how many 1 pixels follow the 0 pixels
    //---------------------------------------------------------------------------------------------
	MUTABLERUNTIME_API extern uint32_t CompressRLE_L1( int width, int rows,
                                    const uint8_t* pBaseData,
                                    uint8_t* destData,
                                    uint32_t destDataSize );

    //---------------------------------------------------------------------------------------------
	MUTABLERUNTIME_API extern uint32_t UncompressRLE_L1( int width, int rows,
                                      const uint8_t* pBaseData,
                                      uint8_t* pDestData );


    //---------------------------------------------------------------------------------------------
    //! This RLE format compresses the data per row. At the beginning of the data there are the row
    //! offsets for direct access. Every offset is a uint32_t.
    //! Every row consists of a series of header + data. The headers is:
    //!     - a UINT16 with how many equal pixels / 4
    //!     - a UINT16 with how many different pixels / 4
    //!     - 4 UINT8 with the value of the "equal" pixels
    //! After the header there are as many pixel values as different pixels.
    //---------------------------------------------------------------------------------------------
	MUTABLERUNTIME_API extern void CompressRLE_RGBA( int width, int rows,
									const uint8_t* pBaseDataByte,
									TArray<uint8_t>& destData );

    //---------------------------------------------------------------------------------------------
	MUTABLERUNTIME_API extern void UncompressRLE_RGBA( int width, int rows,
                                    const uint8_t* pBaseData,
                                    uint8_t* pDestDataB );


    //---------------------------------------------------------------------------------------------
    //! This RLE format compresses the data per row. At the beginning of the data there are the row
    //! offsets for direct access. Every offset is a uint32_t.
    //! Every row consists of a series of header + data. The headers is:
    //!     - a UINT16 with how many equal pixels / 4
    //!     - a UINT16 with how many different pixels / 4
    //!     - 4 UINT8 with the value of the "equal" pixels
    //! After the header there are as many pixel values as different pixels.
    //---------------------------------------------------------------------------------------------
	MUTABLERUNTIME_API extern void CompressRLE_RGB( int width, int rows,
									const uint8_t* pBaseDataByte,
									TArray<uint8_t>& destData );

    //---------------------------------------------------------------------------------------------
	MUTABLERUNTIME_API extern void UncompressRLE_RGB( int width, int rows,
                                   const uint8_t* pBaseData,
                                   uint8_t* pDestDataB );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------
	inline void UncompressRLE_L(const Image* pBase, Image* pDest)
	{
		int sizeX = pBase->GetSizeX();
		int sizeY = pBase->GetSizeY();
		const uint8_t* pData = pBase->GetData();
		uint8_t* pDestData = pDest->GetData();
		for (int lod = 0; lod < pBase->GetLODCount(); ++lod)
		{
			uint32_t compressedSize = UncompressRLE_L(sizeX, sizeY, pData, pDestData);
			pData += compressedSize;

			pDestData += pDest->GetLODDataSize(lod);
			sizeX = FMath::Max(1, FMath::DivideAndRoundUp(sizeX, 2));
			sizeY = FMath::Max(1, FMath::DivideAndRoundUp(sizeY, 2));
		}
	}


    //---------------------------------------------------------------------------------------------
    //!
    //---------------------------------------------------------------------------------------------
    inline bool CompressRLE_L( const Image* pBase, Image* pDest )
    {
        int sizeX = pBase->GetSizeX();
        int sizeY = pBase->GetSizeY();
        const uint8_t* pData = pBase->GetData();
        uint32_t destDataSize = pDest->GetDataSize();
        uint8_t* pDestData = pDest->GetData();
        for (int lod=0;lod<pBase->GetLODCount();++lod)
        {
            uint32_t compressedSize = CompressRLE_L( sizeX, sizeY, pData,
                                                     pDestData,
                                                     destDataSize );
            if (!compressedSize)
            {
                return false;
            }

            destDataSize -= compressedSize;
            pDestData += compressedSize;

            pData += pBase->GetLODDataSize(lod);
            sizeX = FMath::Max(1, FMath::DivideAndRoundUp(sizeX,2));
            sizeY = FMath::Max(1, FMath::DivideAndRoundUp(sizeY,2));
        }

        pDest->m_data.SetNum( pDestData-pDest->GetData() );

#ifdef MUTABLE_DEBUG_RLE		
		{
			ImagePtr Temp = new Image(pBase->GetSizeX(),pBase->GetSizeY(),pBase->GetLODCount(),pBase->GetFormat());
			UncompressRLE_L(pDest,Temp.get());
			check( !FMemory::Memcmp(Temp->GetData(),pBase->GetData(),pBase->GetDataSize() ) );
		}
#endif

        return true;
    }


    //---------------------------------------------------------------------------------------------
    //! This assumes the maximum data size has been preallocateds in pDest. It will return false
    //! if the RLE-compressed data doesn't fit.
    //---------------------------------------------------------------------------------------------
    inline bool CompressRLE_L1( const Image* pBase, Image* pDest )
    {
        int sizeX = pBase->GetSizeX();
        int sizeY = pBase->GetSizeY();
        const uint8_t* pData = pBase->GetData();
        uint32_t destDataSize = pDest->GetDataSize();
        uint8_t* pDestData = pDest->GetData();
        for (int lod=0;lod<pBase->GetLODCount();++lod)
        {
            uint32_t compressedSize = CompressRLE_L1( sizeX, sizeY, pData,
                                                      pDestData,
                                                      destDataSize );
            if (!compressedSize)
            {
                return false;
            }

            destDataSize -= compressedSize;
            pDestData += compressedSize;

            pData += pBase->GetLODDataSize(lod);
			sizeX = FMath::Max(1, FMath::DivideAndRoundUp(sizeX, 2));
			sizeY = FMath::Max(1, FMath::DivideAndRoundUp(sizeY, 2));
		}

        pDest->m_data.SetNum( pDestData-pDest->GetData() );

        return true;
    }


    //---------------------------------------------------------------------------------------------
    inline void UncompressRLE_L1( const Image* pBase, Image* pDest )
    {
        int sizeX = pBase->GetSizeX();
        int sizeY = pBase->GetSizeY();
        const uint8_t* pData = pBase->GetData();
        uint8_t* pDestData = pDest->GetData();
        for (int lod=0;lod<pBase->GetLODCount();++lod)
        {
            uint32_t compressedSize = UncompressRLE_L1( sizeX, sizeY, pData, pDestData );
            pData += compressedSize;

            pDestData += pDest->GetLODDataSize(lod);
			sizeX = FMath::Max(1, FMath::DivideAndRoundUp(sizeX, 2));
			sizeY = FMath::Max(1, FMath::DivideAndRoundUp(sizeY, 2));
		}
    }


    //---------------------------------------------------------------------------------------------
    //!
    //---------------------------------------------------------------------------------------------
    inline void CompressRLE_RGB( const Image* pBase, Image* pDest )
    {
        CompressRLE_RGB( pBase->GetSizeX(),
                         pBase->GetSizeY(),
                         pBase->GetData(),
                         pDest->m_data );
    }


    //---------------------------------------------------------------------------------------------
    //!
    //---------------------------------------------------------------------------------------------
    inline void UncompressRLE_RGB( const Image* pBase, Image* pDest )
    {
        UncompressRLE_RGB( pBase->GetSizeX(),
                           pBase->GetSizeY(),
                           pBase->GetData(),
                           pDest->GetData() );
    }


    //---------------------------------------------------------------------------------------------
    //!
    //---------------------------------------------------------------------------------------------
    inline void CompressRLE_RGBA( const Image* pBase, Image* pDest )
    {
        CompressRLE_RGBA( pBase->GetSizeX(),
                          pBase->GetSizeY(),
                          pBase->GetData(),
                          pDest->m_data );
    }


    //---------------------------------------------------------------------------------------------
    //!
    //---------------------------------------------------------------------------------------------
    inline void UncompressRLE_RGBA( const Image* pBase, Image* pDest )
    {
        UncompressRLE_RGBA( pBase->GetSizeX(),
                            pBase->GetSizeY(),
                            pBase->GetData(),
                            pDest->GetData() );
    }

}
