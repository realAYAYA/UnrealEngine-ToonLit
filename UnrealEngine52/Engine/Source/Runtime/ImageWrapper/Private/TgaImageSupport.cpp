// Copyright Epic Games, Inc. All Rights Reserved.

#include "TgaImageSupport.h"
#include "ImageWrapperPrivate.h"

namespace TgaImageSupportImpl
{
	bool DecompressTGA_RLE_32bpp( const FTGAFileHeader* TGA, const int64 TGABufferLenght, uint32* TextureData )
	{
		const uint8* const IdData = (uint8*)TGA + sizeof(FTGAFileHeader); 
		const uint8* const  ColorMap = IdData + TGA->IdFieldLength;
		const uint8* ImageData = (uint8*) (ColorMap + (TGA->ColorMapEntrySize + 4) / 8 * TGA->ColorMapLength);

		const uint8* TGAEnd = (uint8*)TGA + TGABufferLenght;

		uint32 Pixel = 0;
		int32 RLERun = 0;
		int32 RAWRun = 0;

		for(int32 Y = TGA->Height-1; Y >=0; Y--) // Y-flipped.
		{
			for(int32 X = 0;X < TGA->Width;X++)
			{
				if( RLERun > 0 )
				{
					RLERun--;  // reuse current Pixel data.
				}
				else if( RAWRun == 0 ) // new raw pixel or RLE-run.
				{
					if ( TGAEnd < ImageData )
					{
						return false;
					}
					uint8 RLEChunk = *(ImageData++);							
					if( RLEChunk & 0x80 )
					{
						RLERun = ( RLEChunk & 0x7F ) + 1;
						RAWRun = 1;
					}
					else
					{
						RAWRun = ( RLEChunk & 0x7F ) + 1;
					}

					if ( TGAEnd < ImageData + RAWRun * 4 )
					{
						return false;
					}
				}
				// Retrieve new pixel data - raw run or single pixel for RLE stretch.
				if( RAWRun > 0 )
				{
					Pixel = *(uint32*)ImageData; // RGBA 32-bit dword.
					ImageData += 4;
					RAWRun--;
					RLERun--;
				}
				// Store.
				*( (TextureData + Y*TGA->Width)+X ) = Pixel;
			}
		}

		return true;
	}

	bool DecompressTGA_RLE_24bpp( const FTGAFileHeader* TGA, const int64 TGABufferLenght, uint32* TextureData )
	{
		const uint8* const IdData = (uint8*)TGA + sizeof(FTGAFileHeader); 
		const uint8* const ColorMap = IdData + TGA->IdFieldLength;
		const uint8* ImageData = (uint8*) (ColorMap + (TGA->ColorMapEntrySize + 4) / 8 * TGA->ColorMapLength);

		const uint8* TGAEnd = (uint8*)TGA + TGABufferLenght;

		uint8 Pixel[4] = {};
		int32 RLERun = 0;
		int32 RAWRun = 0;

		for(int32 Y = TGA->Height-1; Y >=0; Y--) // Y-flipped.
		{
			for(int32 X = 0;X < TGA->Width;X++)
			{
				if( RLERun > 0 )
				{
					RLERun--; // reuse current Pixel data.
				}
				else if( RAWRun == 0 ) // new raw pixel or RLE-run.
				{
					if ( TGAEnd < ImageData )
					{
						return false;
					}
					uint8 RLEChunk = *(ImageData++);
					if( RLEChunk & 0x80 )
					{
						RLERun = ( RLEChunk & 0x7F ) + 1;
						RAWRun = 1;
					}
					else
					{
						RAWRun = ( RLEChunk & 0x7F ) + 1;
					}

					if ( TGAEnd < ImageData + RAWRun * 3 )
					{
						return false;
					}
				}
				// Retrieve new pixel data - raw run or single pixel for RLE stretch.
				if( RAWRun > 0 )
				{
					Pixel[0] = *(ImageData++);
					Pixel[1] = *(ImageData++);
					Pixel[2] = *(ImageData++);
					Pixel[3] = 255;
					RAWRun--;
					RLERun--;
				}
				// Store.
				*( (TextureData + Y*TGA->Width)+X ) = *(uint32*)&Pixel;
			}
		}
		return true;
	}

	bool DecompressTGA_RLE_16bpp( const FTGAFileHeader* TGA, const int64 TGABufferLenght, uint32* TextureData )
	{
		const uint8* const IdData = (uint8*)TGA + sizeof(FTGAFileHeader);
		const uint8* const ColorMap = IdData + TGA->IdFieldLength;				
		const uint16* ImageData = (uint16*) (ColorMap + (TGA->ColorMapEntrySize + 4) / 8 * TGA->ColorMapLength);

		const uint16* TGAEnd = (uint16*)TGA + TGABufferLenght / 2;

		uint32 TexturePixel = 0;
		int32 RLERun = 0;
		int32 RAWRun = 0;

		for(int32 Y = TGA->Height-1; Y >=0; Y--) // Y-flipped.
		{					
			for( int32 X=0;X<TGA->Width;X++ )
			{						
				if( RLERun > 0 )
				{
					RLERun--;  // reuse current Pixel data.
				}
				else if( RAWRun == 0 ) // new raw pixel or RLE-run.
				{
					if ( TGAEnd < ImageData )
					{
						return false;
					}
					uint8 RLEChunk =  *((uint8*)ImageData);
					ImageData = (uint16*)(((uint8*)ImageData)+1);
					if( RLEChunk & 0x80 )
					{
						RLERun = ( RLEChunk & 0x7F ) + 1;
						RAWRun = 1;
					}
					else
					{
						RAWRun = ( RLEChunk & 0x7F ) + 1;
					}

					if ( TGAEnd < ImageData + RAWRun )
					{
						return false;
					}
				}
				// Retrieve new pixel data - raw run or single pixel for RLE stretch.
				if( RAWRun > 0 )
				{ 
					const uint16 FilePixel = *(ImageData++);
					RAWRun--;
					RLERun--;

					// Convert file format A1R5G5B5 into pixel format B8G8R8B8
					TexturePixel = (FilePixel & 0x001F) << 3;
					TexturePixel |= (FilePixel & 0x03E0) << 6;
					TexturePixel |= (FilePixel & 0x7C00) << 9;
					TexturePixel |= (FilePixel & 0x8000) << 16;
				}
				// Store.
				*( (TextureData + Y*TGA->Width)+X ) = TexturePixel;
			}
		}

		return true;
	}

	bool DecompressTGA_32bpp( const FTGAFileHeader* TGA, const int64 TGABufferLenght, uint32* TextureData )
	{
		const uint8* const IdData = (uint8*)TGA + sizeof(FTGAFileHeader);
		const uint8* const ColorMap = IdData + TGA->IdFieldLength;
		const uint32* const ImageData = (uint32*) (ColorMap + (TGA->ColorMapEntrySize + 4) / 8 * TGA->ColorMapLength);

		if ( (uint8*)TGA + TGABufferLenght < (uint8*)(ImageData + TGA->Width * TGA->Height) )
		{
			return false;
		}

		for(int32 Y = 0;Y < TGA->Height;Y++)
		{
			FMemory::Memcpy(TextureData + Y * TGA->Width,ImageData + (TGA->Height - Y - 1) * TGA->Width,TGA->Width * 4);
		}

		return true;
	}

	bool DecompressTGA_24bpp( const FTGAFileHeader* TGA, const int64 TGABufferLenght, uint32* TextureData )
	{
		const uint8* const IdData = (uint8*)TGA + sizeof(FTGAFileHeader);
		const uint8* const ColorMap = IdData + TGA->IdFieldLength;
		const uint8* const ImageData = (uint8*) (ColorMap + (TGA->ColorMapEntrySize + 4) / 8 * TGA->ColorMapLength);
		uint8 Pixel[4];

		if ( (uint8*)TGA + TGABufferLenght < ImageData + TGA->Width * TGA->Height * 3 )
		{
			return false;
		}

		for(int32 Y = 0; Y < TGA->Height; Y++)
		{
			for(int32 X = 0; X < TGA->Width; X++)
			{
				Pixel[0] = *(( ImageData+( TGA->Height-Y-1 )*TGA->Width*3 )+X*3+0);
				Pixel[1] = *(( ImageData+( TGA->Height-Y-1 )*TGA->Width*3 )+X*3+1);
				Pixel[2] = *(( ImageData+( TGA->Height-Y-1 )*TGA->Width*3 )+X*3+2);
				Pixel[3] = 255;
				*((TextureData+Y*TGA->Width)+X) = *(uint32*)&Pixel;
			}
		}

		return true;
	}

	bool DecompressTGA_16bpp( const FTGAFileHeader* TGA, const int64 TGABufferLenght, uint32* TextureData )
	{
		const uint8* const IdData = (uint8*)TGA + sizeof(FTGAFileHeader);
		const uint8* const ColorMap = IdData + TGA->IdFieldLength;
		const uint16* ImageData = (uint16*) (ColorMap + (TGA->ColorMapEntrySize + 4) / 8 * TGA->ColorMapLength);
		uint16 FilePixel = 0;
		uint32 TexturePixel = 0;

		if ( (uint16*)((uint8*)TGA + TGABufferLenght) < ImageData + TGA->Height * TGA->Width )
		{
			return false;
		}

		for (int32 Y = TGA->Height - 1; Y >= 0; Y--)
		{					
			for (int32 X = 0; X < TGA->Width; X++)
			{
				FilePixel = *ImageData++;
				// Convert file format A1R5G5B5 into pixel format B8G8R8A8
				TexturePixel = (FilePixel & 0x001F) << 3;
				TexturePixel |= (FilePixel & 0x03E0) << 6;
				TexturePixel |= (FilePixel & 0x7C00) << 9;
				TexturePixel |= (FilePixel & 0x8000) << 16;
				// Store.
				*((TextureData + Y*TGA->Width) + X) = TexturePixel;						
			}
		}

		return true;
	}

	bool DecompressTGA_8bpp( const FTGAFileHeader* TGA, const int64 TGABufferLenght, uint8* TextureData )
	{
		const uint8* const IdData = (uint8*)TGA + sizeof(FTGAFileHeader);
		const uint8* const  ColorMap = IdData + TGA->IdFieldLength;
		const uint8* const  ImageData = (uint8*) (ColorMap + (TGA->ColorMapEntrySize + 4) / 8 * TGA->ColorMapLength);

		if ( (uint8*)TGA + TGABufferLenght < ImageData + TGA->Width * TGA->Height )
		{
			return false;
		}

		int32 RevY = 0;
		for (int32 Y = TGA->Height-1; Y >= 0; --Y)
		{
			const uint8* ImageCol = ImageData + (Y * TGA->Width); 
			uint8* TextureCol = TextureData + (RevY++ * TGA->Width);
			FMemory::Memcpy(TextureCol, ImageCol, TGA->Width);
		}

		return true;
	}
}

bool DecompressTGA_helper( const FTGAFileHeader* TgaHeader, const int64 TGABufferLenght, uint32*& TextureData, const int32 TextureDataSize )
{
	bool bSuccess = false;
	if ( TgaHeader->ImageTypeCode == 10 ) // 10 = RLE compressed 
	{
		check( TextureDataSize == TgaHeader->Width * TgaHeader->Height * 4 );

		// RLE compression: CHUNKS: 1 -byte header, high bit 0 = raw, 1 = compressed
		// bits 0-6 are a 7-bit count; count+1 = number of raw pixels following, or rle pixels to be expanded. 
		if(TgaHeader->BitsPerPixel == 32)
		{
			bSuccess = TgaImageSupportImpl::DecompressTGA_RLE_32bpp(TgaHeader, TGABufferLenght, TextureData);
		}
		else if( TgaHeader->BitsPerPixel == 24 )
		{
			bSuccess = TgaImageSupportImpl::DecompressTGA_RLE_24bpp(TgaHeader, TGABufferLenght, TextureData);
		}
		else if( TgaHeader->BitsPerPixel == 16 )
		{
			bSuccess = TgaImageSupportImpl::DecompressTGA_RLE_16bpp(TgaHeader, TGABufferLenght, TextureData);
		}
		else
		{
			UE_LOG( LogImageWrapper, Error, TEXT("TgaHeader uses an unsupported rle-compressed bit-depth: %u"), TgaHeader->BitsPerPixel );
			return false;
		}
	}
	else if(TgaHeader->ImageTypeCode == 2) // 2 = Uncompressed RGB
	{
		check( TextureDataSize == TgaHeader->Width * TgaHeader->Height * 4 );

		if(TgaHeader->BitsPerPixel == 32)
		{
			bSuccess = TgaImageSupportImpl::DecompressTGA_32bpp(TgaHeader, TGABufferLenght, TextureData);
		}
		else if(TgaHeader->BitsPerPixel == 16)
		{
			bSuccess = TgaImageSupportImpl::DecompressTGA_16bpp(TgaHeader, TGABufferLenght, TextureData);
		}
		else if(TgaHeader->BitsPerPixel == 24)
		{
			bSuccess = TgaImageSupportImpl::DecompressTGA_24bpp(TgaHeader, TGABufferLenght, TextureData);
		}
		else
		{
			UE_LOG( LogImageWrapper, Error, TEXT("TgaHeader uses an unsupported bit-depth: %u"), TgaHeader->BitsPerPixel );
			return false;
		}
	}
	// Support for alpha stored as pseudo-color 8-bit TgaHeader
	else if(TgaHeader->ColorMapType == 1 && TgaHeader->ImageTypeCode == 1 && TgaHeader->BitsPerPixel == 8)
	{
		check( TextureDataSize == TgaHeader->Width * TgaHeader->Height * 1 );

		bSuccess = TgaImageSupportImpl::DecompressTGA_8bpp(TgaHeader, TGABufferLenght, (uint8*)TextureData);
	}
	// standard grayscale
	else if(TgaHeader->ColorMapType == 0 && TgaHeader->ImageTypeCode == 3 && TgaHeader->BitsPerPixel == 8)
	{
		check( TextureDataSize == TgaHeader->Width * TgaHeader->Height * 1 );

		bSuccess = TgaImageSupportImpl::DecompressTGA_8bpp(TgaHeader, TGABufferLenght, (uint8*)TextureData);
	}
	else
	{
		UE_LOG( LogImageWrapper, Error, TEXT("TgaHeader is an unsupported type: %u"), TgaHeader->ImageTypeCode );
		return false;
	}

	if (!bSuccess)
	{
		UE_LOG(LogImageWrapper, Error, TEXT("The TGA file is invalid or corrupted"));
		return false;
	}

	// Flip the image data if the flip bits are set in the TgaHeader header.
	const bool bFlipX = (TgaHeader->ImageDescriptor & 0x10) ? 1 : 0;
	const bool bFlipY = (TgaHeader->ImageDescriptor & 0x20) ? 1 : 0;
	if ( bFlipX || bFlipY )
	{
		TArray<uint8> FlippedData;
		FlippedData.AddUninitialized(TextureDataSize);

		int32 NumBlocksX = TgaHeader->Width;
		int32 NumBlocksY = TgaHeader->Height;
		int32 BlockBytes = TgaHeader->BitsPerPixel == 8 ? 1 : 4;
		
		check( TextureDataSize == NumBlocksX * NumBlocksY * BlockBytes );

		uint8* MipData = (uint8*)TextureData;

		for( int32 Y = 0; Y < NumBlocksY;Y++ )
		{
			for( int32 X  = 0; X < NumBlocksX; X++ )
			{
				int32 DestX = bFlipX ? (NumBlocksX - X - 1) : X;
				int32 DestY = bFlipY ? (NumBlocksY - Y - 1) : Y;
				FMemory::Memcpy(
					&FlippedData[(DestX + DestY * NumBlocksX) * BlockBytes],
					&MipData[(X + Y * NumBlocksX) * BlockBytes],
					BlockBytes
				);
			}
		}

		FMemory::Memcpy( MipData, FlippedData.GetData(), FlippedData.Num() );
	}

	return true;
}