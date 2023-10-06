// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuR/Image.h"

#include "Containers/Array.h"
#include "HAL/UnrealMemory.h"
#include "Math/UnrealMathSSE.h"
#include "Misc/AssertionMacros.h"
#include "MuR/BlockCompression/Miro/Miro.h"
#include "MuR/ImageRLE.h"
#include "MuR/MutableMath.h"
#include "MuR/MutableTrace.h"
#include "MuR/Ptr.h"


namespace mu
{


Ptr<Image> FImageOperator::ImagePixelFormat( int32 CompressionQuality, const Image* Base, EImageFormat TargetFormat, int32 OnlyLOD )
{
	MUTABLE_CPUPROFILER_SCOPE(ImagePixelFormat);
	TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*FString::Printf(TEXT("From %d to %d"), int32(Base->GetFormat()), int32(TargetFormat)));

	FIntVector2 resultSize;
    int resultLODCount = 0;

    if (OnlyLOD==-1)
    {
        resultSize[0] = Base->GetSizeX();
        resultSize[1] = Base->GetSizeY();
        resultLODCount = Base->GetLODCount();
    }
    else
    {
        resultSize = Base->CalculateMipSize( OnlyLOD );
        resultLODCount = 1;
    }

    Ptr<Image> Result = CreateImage( resultSize[0], resultSize[1], resultLODCount, TargetFormat, EInitializationType::NotInitialized );
	Result->m_flags = Base->m_flags;

	if (Base->GetSizeX()<=0 ||Base->GetSizeY()<=0)
	{
		return Result;
	}

	bool bSuccess = false;
	ImagePixelFormat( bSuccess, CompressionQuality, Result.get(), Base, OnlyLOD );
	int32 OriginalDataSize = FMath::Max(Result->m_data.Num(), Base->m_data.Num());
	int32 ExcessDataSize = OriginalDataSize * 4 + 4;
	while (!bSuccess)
    {
		MUTABLE_CPUPROFILER_SCOPE(Recompression_OutOfSpace);

        // Bad case, where the RLE compressed data requires more memory than the uncompressed data.
		// We need to support it anyway for small mips or scaled images.
		Result->m_data.SetNum(ExcessDataSize);
		ImagePixelFormat(bSuccess, CompressionQuality, Result.get(), Base, OnlyLOD);
		ExcessDataSize *= 4;
	}
    return Result;
}


void FImageOperator::ImagePixelFormat( bool& bOutSuccess, int32 CompressionQuality, Image* pResult, const Image* Base, int32 OnlyLOD)
{
	MUTABLE_CPUPROFILER_SCOPE(ImagePixelFormatInPlace);

	bOutSuccess = true;

	FIntVector2 resultSize;
    int resultLODCount = 0;
    const uint8_t* pBaseBuf = 0;
    int pixelCount = 0;
    int baseLOD = 0;

    if (OnlyLOD==-1)
    {
        pixelCount = (int)Base->CalculatePixelCount();
        pBaseBuf = Base->GetData();
        resultSize[0] = Base->GetSizeX();
        resultSize[1] = Base->GetSizeY();
        resultLODCount = Base->GetLODCount();
        baseLOD = 0;
    }
    else
    {
        pixelCount = (int)Base->CalculatePixelCount( OnlyLOD );
        pBaseBuf = Base->GetMipData( OnlyLOD );
        resultSize = Base->CalculateMipSize( OnlyLOD );
        resultLODCount = 1;
        baseLOD = OnlyLOD;
    }

    check( pResult->GetSizeX()==resultSize[0] && pResult->GetSizeY()==resultSize[1] );
    check( pResult->GetLODCount()==resultLODCount );

    if ( pResult->GetFormat() == Base->GetFormat()
         && OnlyLOD==-1 )
    {
        // Shouldn't really happen
        pResult->m_data = Base->m_data;
    }
    else
    {
        switch ( pResult->GetFormat() )
        {

        case EImageFormat::IF_L_UBYTE:
        {
            uint8_t* pDestBuf = pResult->GetData();

            switch ( Base->GetFormat() )
            {

            case EImageFormat::IF_L_UBYTE:
            {
				FMemory::Memcpy( pDestBuf, pBaseBuf, pixelCount );
                break;
            }

            case EImageFormat::IF_L_UBYTE_RLE:
            {
                if( OnlyLOD == -1 )
                {
                    UncompressRLE_L( Base, pResult );
                }
                else
                {
                    UncompressRLE_L( resultSize[0], resultSize[1], pBaseBuf, pDestBuf );
                }
                break;
            }

            case EImageFormat::IF_L_UBIT_RLE:
            {
                if( OnlyLOD == -1 )
                {
                    UncompressRLE_L1( Base, pResult );
                }
                else
                {
                    UncompressRLE_L1( resultSize[0], resultSize[1], pBaseBuf, pDestBuf );
                }
                break;
            }

            case EImageFormat::IF_RGB_UBYTE:
            {
                for ( int i=0; i<pixelCount; ++i )
                {
                    unsigned Result = 76 * pBaseBuf[3*i+0]
                            + 150 * pBaseBuf[3*i+1]
                            + 29 * pBaseBuf[3*i+2];
                    pDestBuf[i] = (uint8_t)FMath::Min( 255u, Result >> 8 );
                }
                break;
            }

            case EImageFormat::IF_RGB_UBYTE_RLE:
            {
                ImagePtr pTempBase =
                    ImagePixelFormat( CompressionQuality, Base, EImageFormat::IF_RGB_UBYTE );
                if (OnlyLOD==-1)
                {
                    pBaseBuf = pTempBase->GetData();
                }
                else
                {
                    pBaseBuf = pTempBase->GetMipData( OnlyLOD );
                }
                for ( int i=0; i<pixelCount; ++i )
                {
                    unsigned Result = 76 * pBaseBuf[3*i+0]
                            + 150 * pBaseBuf[3*i+1]
                            + 29 * pBaseBuf[3*i+2];
                    pDestBuf[i] = (uint8_t)FMath::Min( 255u, Result >> 8 );
                }
                break;
            }

            case EImageFormat::IF_RGBA_UBYTE:
            {
                for ( int i=0; i<pixelCount; ++i )
                {
                    unsigned Result = 76 * pBaseBuf[4*i+0]
                            + 150 * pBaseBuf[4*i+1]
                            + 29 * pBaseBuf[4*i+2];
                    pDestBuf[i] = (uint8_t)FMath::Min( 255u, Result >> 8 );
                }
                break;
            }

            case EImageFormat::IF_RGBA_UBYTE_RLE:
            {
                ImagePtr pTempBase =
                    ImagePixelFormat( CompressionQuality, Base, EImageFormat::IF_RGBA_UBYTE );
                if (OnlyLOD==-1)
                {
                    pBaseBuf = pTempBase->GetData();
                }
                else
                {
                    pBaseBuf = pTempBase->GetMipData( OnlyLOD );
                }
                for ( int i=0; i<pixelCount; ++i )
                {
                    unsigned Result = 76 * pBaseBuf[4*i+0]
                            + 150 * pBaseBuf[4*i+1]
                            + 29 * pBaseBuf[4*i+2];
                    pDestBuf[i] = (uint8_t)FMath::Min( 255u, Result >> 8 );
                }
                break;
            }

            case EImageFormat::IF_BGRA_UBYTE:
            {
                for ( int i=0; i<pixelCount; ++i )
                {
                    unsigned Result = 76 * pBaseBuf[4*i+2]
                            + 150 * pBaseBuf[4*i+1]
                            + 29 * pBaseBuf[4*i+0];
                    pDestBuf[i] = (uint8_t)FMath::Min( 255u, Result >> 8 );
                }
                break;
            }

            case EImageFormat::IF_BC1:
            case EImageFormat::IF_BC2:
            case EImageFormat::IF_BC3:
            {
                for (int m = 0; m < resultLODCount; ++m)
                {
                    ImagePtr pTempBase =
                        ImagePixelFormat( CompressionQuality, Base, EImageFormat::IF_RGB_UBYTE );
                    if (OnlyLOD == -1)
                    {
                        pBaseBuf = pTempBase->GetData();
                    }
                    else
                    {
                        pBaseBuf = pTempBase->GetMipData(OnlyLOD);
                    }
                    for (int i = 0; i < pixelCount; ++i)
                    {
                        unsigned Result = 76 * pBaseBuf[3 * i + 0] + 150 * pBaseBuf[3 * i + 1] +
                                          29 * pBaseBuf[3 * i + 2];
                        pDestBuf[i] = (uint8_t)FMath::Min(255u, Result >> 8);
                    }
                }
                break;
            }
			case EImageFormat::IF_BC4:
			{
                for ( int m=0; m<resultLODCount; ++m )
                {
					FIntVector2 mipSize = pResult->CalculateMipSize( m );
                    miro::BC4_to_L( mipSize[0], mipSize[1], Base->GetMipData(baseLOD+m), pResult->GetMipData(m) );
                }
                break;
			}

            default:
                // Case not implemented
                check( false );

            }

            break;
        }

        case EImageFormat::IF_L_UBYTE_RLE:
        {
            switch ( Base->GetFormat() )
            {

            case EImageFormat::IF_L_UBYTE:
            {
                check( OnlyLOD == -1 );

                // Try to compress
				if (!pResult->m_data.Num())
				{
					// Allocate memory for the compressed data. TODO: Smaller?
					uint32 TotalMemory = Base->m_data.Num();
					pResult->m_data.SetNum(TotalMemory);
				}

                CompressRLE_L(bOutSuccess, Base, pResult);

#ifdef MUTABLE_DEBUG_RLE		
				if (bOutSuccess)
                {
                    // verify
                    ImagePtr pTest = ImagePixelFormat( CompressionQuality, pResult, EImageFormat::IF_L_UBYTE );
					check(!FMemory::Memcmp(pTest->GetData(), Base->GetData(), Base->GetDataSize()));
                }
#endif

                break;
            }

            case EImageFormat::IF_RGB_UBYTE:
            case EImageFormat::IF_RGBA_UBYTE:
            case EImageFormat::IF_BGRA_UBYTE:
            {
                check( OnlyLOD == -1 );
                ImagePtr Temp = ImagePixelFormat( CompressionQuality, Base, EImageFormat::IF_L_UBYTE, -1 );
 
				// Try to compress
				if (!pResult->m_data.Num())
				{
					// Allocate memory for the compressed data. TODO: Smaller?
					pResult->m_data.SetNum(Temp->m_data.Num());
				}

				CompressRLE_L(bOutSuccess, Temp.get(), pResult );

#ifdef MUTABLE_DEBUG_RLE		
				if (bOutSuccess)
                {
                    // verify
                    ImagePtr pTest = ImagePixelFormat( CompressionQuality, pResult, EImageFormat::IF_L_UBYTE );
                    check( !FMemory::Memcmp(pTest->GetData(), pTemp->GetData(), pTemp->GetDataSize() ) );
                }
#endif
				ReleaseImage(Temp);

                break;
            }

            default:
                // Case not implemented
                check( false );

            }

            break;
        }

        case EImageFormat::IF_L_UBIT_RLE:
        {
            switch ( Base->GetFormat() )
            {

            case EImageFormat::IF_L_UBYTE:
            {
                check( OnlyLOD == -1 );

				// Allocate memory for the compressed data. TODO: Smaller?
				if (!pResult->m_data.Num())
				{
					// Allocate memory for the compressed data. TODO: Smaller?
					pResult->m_data.SetNum(Base->m_data.Num());
				}
				
				CompressRLE_L1(bOutSuccess, Base, pResult );
                
#ifdef MUTABLE_DEBUG_RLE		
				if (bOutSuccess)
                {
					// Verify
                    ImagePtr pTest = ImagePixelFormat( CompressionQuality, pResult, EImageFormat::IF_L_UBYTE );
                    check( !FMemory::Memcmp(pTest->GetData(), Base->GetData(), Base->GetDataSize() ) );
                }
#endif

                break;
            }

            case EImageFormat::IF_RGB_UBYTE:
            case EImageFormat::IF_RGBA_UBYTE:
            case EImageFormat::IF_BGRA_UBYTE:
            {
                check( OnlyLOD == -1 );
                ImagePtr Temp = ImagePixelFormat( CompressionQuality, Base, EImageFormat::IF_L_UBYTE, -1);
				CompressRLE_L1(bOutSuccess, Temp.get(), pResult );

#ifdef bOutSuccess		
				if (bOutSuccess)
				{
					// Verify
					ImagePtr pTest = ImagePixelFormat(CompressionQuality, pResult, EImageFormat::IF_L_UBYTE);
					check(!FMemory::Memcmp(pTest->GetData(), pTemp->GetData(), pTemp->GetDataSize()));
				}
#endif

				ReleaseImage(Temp);
                break;
            }

            default:
                // Case not implemented
                check( false );

            }

            break;
        }

        case EImageFormat::IF_RGB_UBYTE:
        {
            uint8_t* pDestBuf = pResult->GetData();

            switch ( Base->GetFormat() )
            {

            case EImageFormat::IF_L_UBYTE:
            {
                for ( int i=0; i<pixelCount; ++i )
                {
                    pDestBuf[i*3+0] = pBaseBuf[i];
                    pDestBuf[i*3+1] = pBaseBuf[i];
                    pDestBuf[i*3+2] = pBaseBuf[i];
                }
                break;
            }

            case EImageFormat::IF_RGB_UBYTE:
            {
				FMemory::Memcpy( pDestBuf, pBaseBuf, 3*pixelCount );
                break;
            }

            case EImageFormat::IF_RGBA_UBYTE:
            {
                for ( int i=0; i<pixelCount; ++i )
                {
                    pDestBuf[i*3+0] = pBaseBuf[i*4+0];
                    pDestBuf[i*3+1] = pBaseBuf[i*4+1];
                    pDestBuf[i*3+2] = pBaseBuf[i*4+2];
                }
                break;
            }

            case EImageFormat::IF_BGRA_UBYTE:
            {
                for ( int i=0; i<pixelCount; ++i )
                {
                    pDestBuf[i*3+0] = pBaseBuf[i*4+2];
                    pDestBuf[i*3+1] = pBaseBuf[i*4+1];
                    pDestBuf[i*3+2] = pBaseBuf[i*4+0];
                }
                break;
            }

            case EImageFormat::IF_RGB_UBYTE_RLE:
            {
                UncompressRLE_RGB( Base, pResult );
                break;
            }

            case EImageFormat::IF_RGBA_UBYTE_RLE:
            {
                check( OnlyLOD == -1 );
                ImagePtr Temp = ImagePixelFormat( CompressionQuality, Base, EImageFormat::IF_RGBA_UBYTE, -1);
                const uint8* TempBuf = Temp->GetData();
                for ( int i=0; i<pixelCount; ++i )
                {
                    pDestBuf[i*3+0] = TempBuf[i*4+0];
                    pDestBuf[i*3+1] = TempBuf[i*4+1];
                    pDestBuf[i*3+2] = TempBuf[i*4+2];
                }
				ReleaseImage(Temp);
                break;
            }

            case EImageFormat::IF_BC1:
            {
                for ( int m=0; m<resultLODCount; ++m )
                {
					FIntVector2 mipSize = pResult->CalculateMipSize( m );
                    miro::BC1_to_RGB( mipSize[0], mipSize[1],
                                        Base->GetMipData(baseLOD+m),
                                        pResult->GetMipData(m) );
                }
                break;
            }

            case EImageFormat::IF_BC2:
            {
                for ( int m=0; m<resultLODCount; ++m )
                {
					FIntVector2 mipSize = pResult->CalculateMipSize( m );
                    miro::BC2_to_RGB( mipSize[0], mipSize[1],
                                        Base->GetMipData(baseLOD+m),
                                        pResult->GetMipData(m) );
                }
                break;
            }

            case EImageFormat::IF_BC3:
            {
                for ( int m=0; m<resultLODCount; ++m )
                {
					FIntVector2 mipSize = pResult->CalculateMipSize( m );
                    miro::BC3_to_RGB( mipSize[0], mipSize[1],
                                        Base->GetMipData(baseLOD+m),
                                        pResult->GetMipData(m) );
                }
                break;
            }

            case EImageFormat::IF_BC4:
            {
                for ( int m=0; m<resultLODCount; ++m )
                {
					FIntVector2 mipSize = pResult->CalculateMipSize( m );
                    miro::BC4_to_RGB( mipSize[0], mipSize[1],
                                      Base->GetMipData(baseLOD+m),
                                      pResult->GetMipData(m) );
                }
                break;
            }

            case EImageFormat::IF_BC5:
            {
                for ( int m=0; m<resultLODCount; ++m )
                {
					FIntVector2 mipSize = pResult->CalculateMipSize( m );
                    miro::BC5_to_RGB( mipSize[0], mipSize[1],
                                      Base->GetMipData(baseLOD+m),
                                      pResult->GetMipData(m) );
                }
                break;
            }

            case EImageFormat::IF_ASTC_4x4_RGB_LDR:
            {
                for ( int m=0; m<resultLODCount; ++m )
                {
					FIntVector2 mipSize = pResult->CalculateMipSize( m );
                    miro::ASTC4x4RGBL_to_RGB( mipSize[0], mipSize[1],
                                      Base->GetMipData(baseLOD+m),
                                      pResult->GetMipData(m) );
                }
                break;
            }

            case EImageFormat::IF_ASTC_4x4_RGBA_LDR:
            {
                for ( int m=0; m<resultLODCount; ++m )
                {
					FIntVector2 mipSize = pResult->CalculateMipSize( m );
                    miro::ASTC4x4RGBAL_to_RGB( mipSize[0], mipSize[1],
                                      Base->GetMipData(baseLOD+m),
                                      pResult->GetMipData(m) );
                }
                break;
            }

            case EImageFormat::IF_ASTC_4x4_RG_LDR:
            {
                for ( int m=0; m<resultLODCount; ++m )
                {
					FIntVector2 mipSize = pResult->CalculateMipSize( m );
                    miro::ASTC4x4RGL_to_RGB( mipSize[0], mipSize[1],
                                      Base->GetMipData(baseLOD+m),
                                      pResult->GetMipData(m) );
                }
                break;
            }

			case EImageFormat::IF_ASTC_8x8_RGB_LDR:
			{
				for (int m = 0; m < resultLODCount; ++m)
				{
					FIntVector2 mipSize = pResult->CalculateMipSize(m);
					miro::ASTC8x8RGBL_to_RGB(mipSize[0], mipSize[1],
						Base->GetMipData(baseLOD + m),
						pResult->GetMipData(m));
				}
				break;
			}

			case EImageFormat::IF_ASTC_8x8_RGBA_LDR:
			{
				for (int m = 0; m < resultLODCount; ++m)
				{
					FIntVector2 mipSize = pResult->CalculateMipSize(m);
					miro::ASTC8x8RGBAL_to_RGB(mipSize[0], mipSize[1],
						Base->GetMipData(baseLOD + m),
						pResult->GetMipData(m));
				}
				break;
			}

			case EImageFormat::IF_ASTC_8x8_RG_LDR:
			{
				for (int m = 0; m < resultLODCount; ++m)
				{
					FIntVector2 mipSize = pResult->CalculateMipSize(m);
					miro::ASTC8x8RGL_to_RGB(mipSize[0], mipSize[1],
						Base->GetMipData(baseLOD + m),
						pResult->GetMipData(m));
				}
				break;
			}

			case EImageFormat::IF_ASTC_12x12_RGB_LDR:
			{
				for (int m = 0; m < resultLODCount; ++m)
				{
					FIntVector2 mipSize = pResult->CalculateMipSize(m);
					miro::ASTC12x12RGBL_to_RGB(mipSize[0], mipSize[1],
						Base->GetMipData(baseLOD + m),
						pResult->GetMipData(m));
				}
				break;
			}

			case EImageFormat::IF_ASTC_12x12_RGBA_LDR:
			{
				for (int m = 0; m < resultLODCount; ++m)
				{
					FIntVector2 mipSize = pResult->CalculateMipSize(m);
					miro::ASTC12x12RGBAL_to_RGB(mipSize[0], mipSize[1],
						Base->GetMipData(baseLOD + m),
						pResult->GetMipData(m));
				}
				break;
			}

			case EImageFormat::IF_ASTC_12x12_RG_LDR:
			{
				for (int m = 0; m < resultLODCount; ++m)
				{
					FIntVector2 mipSize = pResult->CalculateMipSize(m);
					miro::ASTC12x12RGL_to_RGB(mipSize[0], mipSize[1],
						Base->GetMipData(baseLOD + m),
						pResult->GetMipData(m));
				}
				break;
			}

            default:
                // Case not implemented
                check( false );

            }

            break;
        }

        case EImageFormat::IF_BGRA_UBYTE:
        {
            // TODO: Optimise
            pResult->m_format = EImageFormat::IF_RGBA_UBYTE;
            ImagePixelFormat(bOutSuccess, CompressionQuality, pResult, Base, OnlyLOD);
			check(bOutSuccess);
            pResult->m_format = EImageFormat::IF_BGRA_UBYTE;

            uint8* pDestBuf = pResult->GetData();
            for ( int i=0; i<pixelCount; ++i )
            {
                uint8 temp = pDestBuf[i*4+0];
                pDestBuf[i*4+0] = pDestBuf[i*4+2];
                pDestBuf[i*4+2] = temp;
            }
            break;
        }

        case EImageFormat::IF_RGBA_UBYTE:
        {
            uint8_t* pDestBuf = pResult->GetData();

            switch ( Base->GetFormat() )
            {

            case EImageFormat::IF_RGBA_UBYTE_RLE:
            {
                check( OnlyLOD == -1 );
                UncompressRLE_RGBA( Base, pResult );
                break;
            }

            case EImageFormat::IF_L_UBYTE:
            {
                for ( int i=0; i<pixelCount; ++i )
                {
                    pDestBuf[i*4+0] = pBaseBuf[i];
                    pDestBuf[i*4+1] = pBaseBuf[i];
                    pDestBuf[i*4+2] = pBaseBuf[i];
                    pDestBuf[i*4+3] = 255;
                }
                break;
            }

            case EImageFormat::IF_RGB_UBYTE:
            {
                for ( int i=0; i<pixelCount; ++i )
                {
                    pDestBuf[i*4+0] = pBaseBuf[i*3+0];
                    pDestBuf[i*4+1] = pBaseBuf[i*3+1];
                    pDestBuf[i*4+2] = pBaseBuf[i*3+2];
                    pDestBuf[i*4+3] = 255;
                }
                break;
            }

            case EImageFormat::IF_RGBA_UBYTE:
            {
				FMemory::Memcpy( pDestBuf, pBaseBuf, 4*pixelCount );
                break;
            }

            case EImageFormat::IF_BGRA_UBYTE:
            {
                for ( int i=0; i<pixelCount; ++i )
                {
                    pDestBuf[i*4+0] = pBaseBuf[i*4+2];
                    pDestBuf[i*4+1] = pBaseBuf[i*4+1];
                    pDestBuf[i*4+2] = pBaseBuf[i*4+0];
                    pDestBuf[i*4+3] = pBaseBuf[i*4+3];
                }
                break;
            }

            case EImageFormat::IF_BC1:
            {
                for ( int m=0; m<resultLODCount; ++m )
                {
					FIntVector2 mipSize = pResult->CalculateMipSize( m );
                    miro::BC1_to_RGBA( mipSize[0], mipSize[1],
                                        Base->GetMipData(baseLOD+m),
                                        pResult->GetMipData(m) );
                }
                break;
            }

            case EImageFormat::IF_BC2:
            {
                for ( int m=0; m<resultLODCount; ++m )
                {
					FIntVector2 mipSize = pResult->CalculateMipSize( m );
                    miro::BC2_to_RGBA( mipSize[0], mipSize[1],
                                        Base->GetMipData(baseLOD+m),
                                        pResult->GetMipData(m) );
                }
                break;
            }

            case EImageFormat::IF_BC3:
            {
                for ( int m=0; m<resultLODCount; ++m )
                {
					FIntVector2 mipSize = pResult->CalculateMipSize( m );
                    miro::BC3_to_RGBA( mipSize[0], mipSize[1],
                                        Base->GetMipData(baseLOD+m),
                                        pResult->GetMipData(m) );
                }
                break;
            }

            case EImageFormat::IF_BC4:
            {
                for ( int m=0; m<resultLODCount; ++m )
                {
					FIntVector2 mipSize = pResult->CalculateMipSize( m );
                    miro::BC4_to_RGBA( mipSize[0], mipSize[1],
                                       Base->GetMipData(baseLOD+m),
                                       pResult->GetMipData(m) );
                }
                break;
            }

            case EImageFormat::IF_BC5:
            {
                for ( int m=0; m<resultLODCount; ++m )
                {
                    FIntVector2 mipSize = pResult->CalculateMipSize( m );
                    miro::BC5_to_RGBA( mipSize[0], mipSize[1],
                                        Base->GetMipData(baseLOD+m),
                                        pResult->GetMipData(m) );
                }
                break;
            }

            case EImageFormat::IF_ASTC_4x4_RGB_LDR:
            {
                for ( int m=0; m<resultLODCount; ++m )
                {
                    FIntVector2 mipSize = pResult->CalculateMipSize( m );
                    miro::ASTC4x4RGBL_to_RGBA( mipSize[0], mipSize[1],
                                        Base->GetMipData(baseLOD+m),
                                        pResult->GetMipData(m) );
                }
                break;
            }

            case EImageFormat::IF_ASTC_4x4_RGBA_LDR:
            {
                for ( int m=0; m<resultLODCount; ++m )
                {
                    FIntVector2 mipSize = pResult->CalculateMipSize( m );
                    miro::ASTC4x4RGBAL_to_RGBA( mipSize[0], mipSize[1],
                                        Base->GetMipData(baseLOD+m),
                                        pResult->GetMipData(m) );
                }
                break;
            }

            case EImageFormat::IF_ASTC_4x4_RG_LDR:
            {
                for ( int m=0; m<resultLODCount; ++m )
                {
                    FIntVector2 mipSize = pResult->CalculateMipSize( m );
                    miro::ASTC4x4RGL_to_RGBA( mipSize[0], mipSize[1],
                                        Base->GetMipData(baseLOD+m),
                                        pResult->GetMipData(m) );
                }
                break;
            }

			case EImageFormat::IF_ASTC_8x8_RGB_LDR:
			{
				for (int m = 0; m < resultLODCount; ++m)
				{
					FIntVector2 mipSize = pResult->CalculateMipSize(m);
					miro::ASTC8x8RGBL_to_RGBA(mipSize[0], mipSize[1],
						Base->GetMipData(baseLOD + m),
						pResult->GetMipData(m));
				}
				break;
			}

			case EImageFormat::IF_ASTC_8x8_RGBA_LDR:
			{
				for (int m = 0; m < resultLODCount; ++m)
				{
					FIntVector2 mipSize = pResult->CalculateMipSize(m);
					miro::ASTC8x8RGBAL_to_RGBA(mipSize[0], mipSize[1],
						Base->GetMipData(baseLOD + m),
						pResult->GetMipData(m));
				}
				break;
			}

			case EImageFormat::IF_ASTC_8x8_RG_LDR:
			{
				for (int m = 0; m < resultLODCount; ++m)
				{
					FIntVector2 mipSize = pResult->CalculateMipSize(m);
					miro::ASTC8x8RGL_to_RGBA(mipSize[0], mipSize[1],
						Base->GetMipData(baseLOD + m),
						pResult->GetMipData(m));
				}
				break;
			}

			case EImageFormat::IF_ASTC_12x12_RGB_LDR:
			{
				for (int m = 0; m < resultLODCount; ++m)
				{
					FIntVector2 mipSize = pResult->CalculateMipSize(m);
					miro::ASTC12x12RGBL_to_RGBA(mipSize[0], mipSize[1],
						Base->GetMipData(baseLOD + m),
						pResult->GetMipData(m));
				}
				break;
			}

			case EImageFormat::IF_ASTC_12x12_RGBA_LDR:
			{
				for (int m = 0; m < resultLODCount; ++m)
				{
					FIntVector2 mipSize = pResult->CalculateMipSize(m);
					miro::ASTC12x12RGBAL_to_RGBA(mipSize[0], mipSize[1],
						Base->GetMipData(baseLOD + m),
						pResult->GetMipData(m));
				}
				break;
			}

			case EImageFormat::IF_ASTC_12x12_RG_LDR:
			{
				for (int m = 0; m < resultLODCount; ++m)
				{
					FIntVector2 mipSize = pResult->CalculateMipSize(m);
					miro::ASTC12x12RGL_to_RGBA(mipSize[0], mipSize[1],
						Base->GetMipData(baseLOD + m),
						pResult->GetMipData(m));
				}
				break;
			}

			case EImageFormat::IF_RGB_UBYTE_RLE:
			{
				check(OnlyLOD == -1);
				ImagePtr Temp = ImagePixelFormat(CompressionQuality, Base, EImageFormat::IF_RGB_UBYTE);
				const uint8* TempData = Temp->GetData();
				for (int i = 0; i < pixelCount; ++i)
				{
					pDestBuf[i * 4 + 0] = TempData[i * 3 + 0];
					pDestBuf[i * 4 + 1] = TempData[i * 3 + 1];
					pDestBuf[i * 4 + 2] = TempData[i * 3 + 2];
					pDestBuf[i * 4 + 3] = 255;
				}
				ReleaseImage(Temp);
				break;
			}

			case EImageFormat::IF_L_UBYTE_RLE:
			{
				check(OnlyLOD == -1);
				ImagePtr Temp = ImagePixelFormat(CompressionQuality, Base, EImageFormat::IF_L_UBYTE);
				const uint8* TempData = Temp->GetData();
				for (int i = 0; i < pixelCount; ++i)
				{
					pDestBuf[i * 4 + 0] = TempData[i];
					pDestBuf[i * 4 + 1] = TempData[i];
					pDestBuf[i * 4 + 2] = TempData[i];
					pDestBuf[i * 4 + 3] = 255;
				}
				ReleaseImage(Temp);
				break;
			}

            default:
                // Case not implemented
                check( false );

            }

            break;
        }


        case EImageFormat::IF_RGBA_UBYTE_RLE:
        {
            switch ( Base->GetFormat() )
            {

            case EImageFormat::IF_RGBA_UBYTE:
            {
                check( OnlyLOD == -1 );
                CompressRLE_RGBA( Base, pResult );
                break;
            }

            case EImageFormat::IF_RGB_UBYTE:
            {
                check( OnlyLOD == -1 );

                // \todo: optimise
                ImagePtr Temp = ImagePixelFormat( CompressionQuality, Base, EImageFormat::IF_RGBA_UBYTE);
                CompressRLE_RGBA( Temp.get(), pResult );

                // Test
                //                ImagePtr pTest = ImagePixelFormat( CompressionQuality,
                //                pResult, EImageFormat::IF_RGBA_UBYTE ); check(
                //                !memcmp(pTest->GetData(), Base->GetData(),
                //                Base->GetDataSize() ) );

				ReleaseImage(Temp);
                break;
            }

            case EImageFormat::IF_RGB_UBYTE_RLE:
            {
                check( OnlyLOD == -1 );

                // \todo: optimise
                ImagePtr Temp1 = ImagePixelFormat( CompressionQuality, Base, EImageFormat::IF_RGB_UBYTE);
                ImagePtr Temp2 = ImagePixelFormat( CompressionQuality, Temp1.get(), EImageFormat::IF_RGBA_UBYTE);
				ReleaseImage(Temp1);
				CompressRLE_RGBA( Temp2.get(), pResult );
				ReleaseImage(Temp2);
				break;
            }

            default:
                // Case not implemented
                check( false );

            }

            break;
        }


        case EImageFormat::IF_RGB_UBYTE_RLE:
        {
            switch ( Base->GetFormat() )
            {

            case EImageFormat::IF_RGB_UBYTE:
            {
                check( OnlyLOD == -1 );
                CompressRLE_RGB( Base, pResult );

                // Test
                // ImagePtr pTest = ImagePixelFormat( CompressionQuality, pDest.get(),
                // EImageFormat::IF_RGB_UBYTE ); check( !memcmp(pTest->GetData(), Base->GetData(),
                // Base->GetDataSize() ) );
                break;
            }

            case EImageFormat::IF_RGBA_UBYTE_RLE:
            {
                check( OnlyLOD == -1 );

                // \todo: optimise
                ImagePtr Temp1 = ImagePixelFormat( CompressionQuality, Base, EImageFormat::IF_RGBA_UBYTE);
                ImagePtr Temp2 = ImagePixelFormat( CompressionQuality, Temp1.get(), EImageFormat::IF_RGB_UBYTE);
				ReleaseImage(Temp1);
				CompressRLE_RGB( Temp2.get(), pResult );
				ReleaseImage(Temp2);
				break;
            }

            default:
                // Case not implemented
                check( false );

            }

            break;
        }

        case EImageFormat::IF_BC1:
        {
            switch ( Base->GetFormat() )
            {
            case EImageFormat::IF_RGB_UBYTE:
                for ( int m=0; m<resultLODCount; ++m )
                {
                    FIntVector2 mipSize = pResult->CalculateMipSize( m );
                    miro::RGB_to_BC1(
                        mipSize[0], mipSize[1], Base->GetMipData( baseLOD + m ),
                        pResult->GetMipData( m ), CompressionQuality );
                }
                break;
            case EImageFormat::IF_RGBA_UBYTE:
                for ( int m=0; m<resultLODCount; ++m )
                {
                    FIntVector2 mipSize = pResult->CalculateMipSize( m );
                    miro::RGBA_to_BC1(
                        mipSize[0], mipSize[1], Base->GetMipData( baseLOD + m ),
                        pResult->GetMipData( m ), CompressionQuality );
                }
                break;

            case EImageFormat::IF_L_UBYTE:
            {
                ImagePtr Temp = ImagePixelFormat( CompressionQuality, Base, EImageFormat::IF_RGB_UBYTE);
                for ( int m=0; m<resultLODCount; ++m )
                {
                    FIntVector2 mipSize = pResult->CalculateMipSize( m );
                    miro::RGB_to_BC1(
                        mipSize[0], mipSize[1], Temp->GetMipData( baseLOD + m ),
                        pResult->GetMipData( m ), CompressionQuality );
                }
				ReleaseImage(Temp);
                break;
            }

            case EImageFormat::IF_BC3:
            {
                ImagePtr Temp = ImagePixelFormat( CompressionQuality, Base, EImageFormat::IF_RGBA_UBYTE);
                for (int m = 0; m < resultLODCount; ++m)
                {
                    FIntVector2 mipSize = pResult->CalculateMipSize(m);
                    miro::RGBA_to_BC1(
                        mipSize[0], mipSize[1], Temp->GetMipData( baseLOD + m ),
                        pResult->GetMipData( m ), CompressionQuality );
                }
				ReleaseImage(Temp);
				break;
            }

            default:
                // Case not implemented
                check( false );

            }

            break;
        }

        case EImageFormat::IF_BC2:
        {
            switch ( Base->GetFormat() )
            {
            case EImageFormat::IF_RGBA_UBYTE:
                for ( int m=0; m<resultLODCount; ++m )
                {
                    FIntVector2 mipSize = pResult->CalculateMipSize( m );
                    miro::RGBA_to_BC2(
                        mipSize[0], mipSize[1], Base->GetMipData( baseLOD + m ),
                        pResult->GetMipData( m ), CompressionQuality );
                }
                break;

            default:
                // Case not implemented
                check( false );

            }

            break;
        }

        case EImageFormat::IF_BC3:
        {
            switch ( Base->GetFormat() )
            {
            case EImageFormat::IF_RGBA_UBYTE:
                for ( int m=0; m<resultLODCount; ++m )
                {
                    FIntVector2 mipSize = pResult->CalculateMipSize( m );
                    miro::RGBA_to_BC3(
                        mipSize[0], mipSize[1], Base->GetMipData( baseLOD + m ),
                        pResult->GetMipData( m ), CompressionQuality );
                }
                break;

            case EImageFormat::IF_RGB_UBYTE:          
                for ( int m=0; m<resultLODCount; ++m )
                {
                    FIntVector2 mipSize = pResult->CalculateMipSize( m );
                    miro::RGB_to_BC3(
                        mipSize[0], mipSize[1], Base->GetMipData( baseLOD + m ),
                        pResult->GetMipData( m ), CompressionQuality );
                }
                break;

            case EImageFormat::IF_BC1:
            {
                MUTABLE_CPUPROFILER_SCOPE(BC1toBC3);

                for ( int m=0; m<resultLODCount; ++m )
                {
                    FIntVector2 mipSize = pResult->CalculateMipSize( m );
                    miro::BC1_to_BC3(
                        mipSize[0], mipSize[1], Base->GetMipData( baseLOD + m ),
                        pResult->GetMipData( m ), CompressionQuality );
                }
                break;
            }

            default:
                // Case not implemented
                check( false );

            }

            break;
        }

        case EImageFormat::IF_BC4:
        {
            switch ( Base->GetFormat() )
            {
            case EImageFormat::IF_L_UBYTE:
			{
				for (int m = 0; m < resultLODCount; ++m)
				{
					FIntVector2 mipSize = pResult->CalculateMipSize(m);
					miro::L_to_BC4(
						mipSize[0], mipSize[1], Base->GetMipData(baseLOD + m),
						pResult->GetMipData(m), CompressionQuality);
				}
				break;
			}
			case EImageFormat::IF_RGB_UBYTE:
			case EImageFormat::IF_RGBA_UBYTE:
			case EImageFormat::IF_BGRA_UBYTE:
			{	
                ImagePtr TempBase = ImagePixelFormat( CompressionQuality, Base, EImageFormat::IF_L_UBYTE, -1);
                for (int m = 0; m < resultLODCount; ++m)
                {
                    FIntVector2 MipSize = TempBase->CalculateMipSize( m );
                    miro::L_to_BC4( MipSize[0], MipSize[1], Base->GetMipData( baseLOD + m ), pResult->GetMipData( m ), CompressionQuality );
                }
				ReleaseImage(TempBase);
				break;
			}

            default:
                // Case not implemented
                check( false );

            }

            break;
        }

        case EImageFormat::IF_BC5:
        {
            switch ( Base->GetFormat() )
            {
            case EImageFormat::IF_RGBA_UBYTE:
                for ( int m=0; m<resultLODCount; ++m )
                {
                    FIntVector2 mipSize = pResult->CalculateMipSize( m );
                    miro::RGBA_to_BC5(
                        mipSize[0], mipSize[1], Base->GetMipData( baseLOD + m ),
                        pResult->GetMipData( m ), CompressionQuality );
                }
                break;

            case EImageFormat::IF_RGB_UBYTE:
                for ( int m=0; m<resultLODCount; ++m )
                {
                    FIntVector2 mipSize = pResult->CalculateMipSize( m );
                    miro::RGB_to_BC5(
                        mipSize[0], mipSize[1], Base->GetMipData( baseLOD + m ),
                        pResult->GetMipData( m ), CompressionQuality );
                }
                break;

            default:
                // Case not implemented
                check( false );

            }

            break;
        }

        case EImageFormat::IF_ASTC_4x4_RGB_LDR:
        {
            switch ( Base->GetFormat() )
            {
            case EImageFormat::IF_RGBA_UBYTE:
                for ( int m=0; m<resultLODCount; ++m )
                {
                    FIntVector2 mipSize = pResult->CalculateMipSize( m );
                    miro::RGBA_to_ASTC4x4RGBL(
                        mipSize[0], mipSize[1], Base->GetMipData( baseLOD + m ),
                        pResult->GetMipData( m ), CompressionQuality );
                }
                break;

            case EImageFormat::IF_RGB_UBYTE:
                for ( int m=0; m<resultLODCount; ++m )
                {
                    FIntVector2 mipSize = pResult->CalculateMipSize( m );
                    miro::RGB_to_ASTC4x4RGBL(
                        mipSize[0], mipSize[1], Base->GetMipData( baseLOD + m ),
                        pResult->GetMipData( m ), CompressionQuality );
                }
                break;

            case EImageFormat::IF_ASTC_4x4_RGBA_LDR:
                for ( int m=0; m<resultLODCount; ++m )
                {
                    FIntVector2 mipSize = pResult->CalculateMipSize( m );
                    miro::ASTC4x4RGBAL_to_ASTC4x4RGBL( mipSize[0], mipSize[1],
                                       Base->GetMipData(baseLOD+m),
                                       pResult->GetMipData(m) );
                }
                break;

			case EImageFormat::IF_L_UBYTE:
				for (int m = 0; m < resultLODCount; ++m)
				{
					FIntVector2 mipSize = pResult->CalculateMipSize(m);
					miro::L_to_ASTC4x4RGBL(mipSize[0], mipSize[1],
						Base->GetMipData(baseLOD + m),
						pResult->GetMipData(m), CompressionQuality);
				}
				break;

            default:
                // Case not implemented
                check( false );

            }

            break;
        }

        case EImageFormat::IF_ASTC_4x4_RGBA_LDR:
        {
            switch ( Base->GetFormat() )
            {
            case EImageFormat::IF_RGBA_UBYTE:
                for ( int m=0; m<resultLODCount; ++m )
                {
                    FIntVector2 mipSize = pResult->CalculateMipSize( m );
                    miro::RGBA_to_ASTC4x4RGBAL(
                        mipSize[0], mipSize[1], Base->GetMipData( baseLOD + m ),
                        pResult->GetMipData( m ), CompressionQuality );
                }
                break;

            case EImageFormat::IF_RGB_UBYTE:
                for ( int m=0; m<resultLODCount; ++m )
                {
                    FIntVector2 mipSize = pResult->CalculateMipSize( m );
                    miro::RGB_to_ASTC4x4RGBAL(
                        mipSize[0], mipSize[1], Base->GetMipData( baseLOD + m ),
                        pResult->GetMipData( m ), CompressionQuality );
                }
                break;

            case EImageFormat::IF_ASTC_4x4_RGB_LDR:
                for ( int m=0; m<resultLODCount; ++m )
                {
                    FIntVector2 mipSize = pResult->CalculateMipSize( m );
                    miro::ASTC4x4RGBL_to_ASTC4x4RGBAL( mipSize[0], mipSize[1],
                                       Base->GetMipData(baseLOD+m),
                                       pResult->GetMipData(m) );
                }
                break;

            default:
                // Case not implemented
                check( false );

            }

            break;
        }

        case EImageFormat::IF_ASTC_4x4_RG_LDR:
        {
            switch ( Base->GetFormat() )
            {
            case EImageFormat::IF_RGBA_UBYTE:
                for ( int m=0; m<resultLODCount; ++m )
                {
                    FIntVector2 mipSize = pResult->CalculateMipSize( m );
                    miro::RGBA_to_ASTC4x4RGL(
                        mipSize[0], mipSize[1], Base->GetMipData( baseLOD + m ),
                        pResult->GetMipData( m ), CompressionQuality );
                }
                break;

            case EImageFormat::IF_RGB_UBYTE:
                for ( int m=0; m<resultLODCount; ++m )
                {
                    FIntVector2 mipSize = pResult->CalculateMipSize( m );
                    miro::RGB_to_ASTC4x4RGL(
                        mipSize[0], mipSize[1], Base->GetMipData( baseLOD + m ),
                        pResult->GetMipData( m ), CompressionQuality );
                }
                break;

            case EImageFormat::IF_ASTC_4x4_RG_LDR:
                for ( int m=0; m<resultLODCount; ++m )
                {
                    FIntVector2 mipSize = pResult->CalculateMipSize( m );
                    // Hack that actually works because of block size.
                    miro::ASTC4x4RGBAL_to_ASTC4x4RGBL( mipSize[0], mipSize[1],
                                       Base->GetMipData(baseLOD+m),
                                       pResult->GetMipData(m) );
                }
                break;

            default:
                // Case not implemented
                check( false );

            }

            break;
        }

        default:
            // Case not implemented
            check( false );
        }
    }

}


}
