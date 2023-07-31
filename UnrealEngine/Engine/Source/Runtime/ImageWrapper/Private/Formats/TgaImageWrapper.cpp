// Copyright Epic Games, Inc. All Rights Reserved.

#include "Formats/TgaImageWrapper.h"
#include "ImageWrapperPrivate.h"

#include "TgaImageSupport.h"



// CanSetRawFormat returns true if SetRaw will accept this format
bool FTgaImageWrapper::CanSetRawFormat(const ERGBFormat InFormat, const int32 InBitDepth) const
{
	return ( InFormat == ERGBFormat::BGRA ) && ( InBitDepth == 8 );
}

// returns InFormat if supported, else maps to something supported
ERawImageFormat::Type FTgaImageWrapper::GetSupportedRawFormat(const ERawImageFormat::Type InFormat) const
{
	return ERawImageFormat::BGRA8;
}

void FTgaImageWrapper::Compress(int32 Quality)
{
	CompressedData.Reset();

	int64 SurfaceBytes = Width * Height * 4;
	check( RawData.Num() == SurfaceBytes );

	int64 CompresedSize = sizeof(FTGAFileHeader) + SurfaceBytes;
	CompressedData.SetNum(CompresedSize);

	uint8 * CompressedPtr = CompressedData.GetData();
	
	// super lazy TGA writer here :
	//	always writes 32 bit BGRA

	FTGAFileHeader header = { };

	header.Width  = INTEL_ORDER16( (uint16) Width );
	header.Height = INTEL_ORDER16( (uint16) Height );
	header.ImageTypeCode = 2;
	header.BitsPerPixel = 32;
	header.ImageDescriptor = 8;

	memcpy(CompressedPtr,&header,sizeof(header));
	CompressedPtr += sizeof(header);

	// write rows BGRA , bottom up :
	for(int y = Height-1; y>= 0;y--)
	{
		int64 RowBytes = Width * 4;
		const void * From = &RawData[y * RowBytes]; 
		memcpy(CompressedPtr,From,RowBytes);
		CompressedPtr += RowBytes;
	}

	check( CompressedPtr == CompressedData.GetData() + CompressedData.Num() );
}

bool FTgaImageWrapper::SetCompressed(const void* InCompressedData, int64 InCompressedSize)
{
	bool bResult = FImageWrapperBase::SetCompressed(InCompressedData, InCompressedSize);

	return bResult && LoadTGAHeader();
}

void FTgaImageWrapper::Uncompress(const ERGBFormat InFormat, const int32 InBitDepth)
{
	const int32 BytesPerPixel = ( InFormat == ERGBFormat::Gray ? 1 : 4 );
	int64 TextureDataSize = (int64)Width * Height * BytesPerPixel;
	RawData.Empty(TextureDataSize);
	RawData.AddUninitialized(TextureDataSize);

	uint32* TextureData = reinterpret_cast< uint32* >( RawData.GetData() );

	FTGAFileHeader* TgaHeader = reinterpret_cast< FTGAFileHeader* >( CompressedData.GetData() );
	if ( !DecompressTGA_helper( TgaHeader, CompressedData.Num(), TextureData, TextureDataSize ) )
	{
		SetError(TEXT("Error while decompressing a TGA"));
		RawData.Reset();
	}
}

bool FTgaImageWrapper::IsTGAHeader(const void * CompressedData,int64 CompressedDataLength)
{
	const FTGAFileHeader* TgaHeader = (FTGAFileHeader*)CompressedData;

	if ( CompressedDataLength >= sizeof( FTGAFileHeader ) &&
		( (TgaHeader->ColorMapType == 0 && TgaHeader->ImageTypeCode == 2) ||
		  ( TgaHeader->ColorMapType == 0 && TgaHeader->ImageTypeCode == 3 ) || // ImageTypeCode 3 is greyscale
		  ( TgaHeader->ColorMapType == 0 && TgaHeader->ImageTypeCode == 10 ) ||
		  ( TgaHeader->ColorMapType == 1 && TgaHeader->ImageTypeCode == 1 && TgaHeader->BitsPerPixel == 8 ) ) )
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool FTgaImageWrapper::LoadTGAHeader()
{
	check( CompressedData.Num() );

	if( ! IsTGAHeader(CompressedData.GetData(),CompressedData.Num()) )
	{
		return false;
	}

	const FTGAFileHeader* TgaHeader = (FTGAFileHeader*)CompressedData.GetData();

	Width = TgaHeader->Width;
	Height = TgaHeader->Height;
	BitDepth = 8;
	
	// must exactly match the logic in DecompressTGA_helper
	if(TgaHeader->ColorMapType == 1 && TgaHeader->ImageTypeCode == 1 && TgaHeader->BitsPerPixel == 8)
	{
		Format = ERGBFormat::Gray;
	}
	else if(TgaHeader->ColorMapType == 0 && TgaHeader->ImageTypeCode == 3 && TgaHeader->BitsPerPixel == 8)
	{
		Format = ERGBFormat::Gray;
	}
	else
	{
		Format = ERGBFormat::BGRA;
	}

	ColorMapType = TgaHeader->ColorMapType;
	ImageTypeCode = TgaHeader->ImageTypeCode;
	
	// TgaHeader->BitsPerPixel is bits per *color*

	return true;
}
