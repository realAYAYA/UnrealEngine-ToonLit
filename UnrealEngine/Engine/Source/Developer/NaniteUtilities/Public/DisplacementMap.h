// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/Texture.h"


namespace Nanite
{

class FDisplacementMap
{
public:
	ETextureSourceFormat	SourceFormat;

	int32		BytesPerPixel;
	int32		SizeX;
	int32		SizeY;
	uint32		NumLevels;

	float		Magnitude;
	float		Center;

	TextureAddress	AddressX;
	TextureAddress	AddressY;

public:
	NANITEUTILITIES_API				FDisplacementMap();
	NANITEUTILITIES_API				FDisplacementMap( struct FImage&& TextureSourceImage, float InMagnitude, float InCenter, TextureAddress InAddressX, TextureAddress InAddressY );

									// Bilinear filtered
	NANITEUTILITIES_API float		Sample( FVector2f UV ) const;
	NANITEUTILITIES_API FVector2f	Sample( FVector2f MinUV, FVector2f MaxUV ) const;

	float		Sample( int32 x, int32 y ) const;
	FVector2f	Sample( int32 x, int32 y, uint32 Level ) const;

	float		Load( int32 x, int32 y ) const;
	FVector2f	Load( int32 x, int32 y, uint32 Level ) const;

private:
	TArray64< uint8 >	SourceData;
	TArray< FVector2f >	MipData[12];
	
	void		Address( int32& x, int32& y ) const;
};


FORCEINLINE float FDisplacementMap::Sample( int32 x, int32 y ) const
{
	Address(x,y);

	float Displacement = Load(x,y);
	Displacement -= Center;
	Displacement *= Magnitude;

	return Displacement;
}

FORCEINLINE FVector2f FDisplacementMap::Sample( int32 x, int32 y, uint32 Level ) const
{
	Address(x,y);

	x >>= Level;
	y >>= Level;

	FVector2f Displacement = Load( x, y, Level );
	Displacement -= FVector2f( Center );
	Displacement *= Magnitude;

	return Displacement;
}

FORCEINLINE float FDisplacementMap::Load( int32 x, int32 y ) const
{
	const uint8* PixelPtr = &SourceData[ int64( x + (int64)y * SizeX ) * BytesPerPixel ];

	if( SourceFormat == TSF_BGRA8 )
	{
		return float( PixelPtr[2] ) / 255.0f;
	}
	else if( SourceFormat == TSF_RGBA16 )
	{
		checkSlow(BytesPerPixel == sizeof(uint16) * 4);
		return float( *(uint16*)PixelPtr ) / 65535.0f;
	}
	else if( SourceFormat == TSF_RGBA16F || SourceFormat == TSF_R16F )
	{
		FFloat16 HalfValue = *(FFloat16*)PixelPtr;
		return HalfValue;
	}
	else if( SourceFormat == TSF_G8 )
	{
		return float( PixelPtr[0] ) / 255.0f;
	}
	else if (SourceFormat == TSF_G16)
	{
		return float( *(uint16*)PixelPtr ) / 65535.0f;
	}
	else if( SourceFormat == TSF_RGBA32F || SourceFormat == TSF_R32F )
	{
		return *(float*)PixelPtr;
	}
	else
	{
		checkf( 0, TEXT("Displacement map format not supported") );
		return 0.0f;
	}
}

FORCEINLINE FVector2f FDisplacementMap::Load( int32 x, int32 y, uint32 Level ) const
{
	checkSlow( Level > 0 );

	uint32 MipSizeX = ( ( SizeX - 1 ) >> Level ) + 1;
	uint32 MipSizeY = ( ( SizeY - 1 ) >> Level ) + 1;

	return MipData[ Level - 1 ][ x + y * MipSizeX ];
}

FORCEINLINE void FDisplacementMap::Address( int32& x, int32& y ) const
{
	if( AddressX == TA_Clamp )
		x = FMath::Clamp( x, 0, SizeX );
	else
	{
		x  = x % SizeX;
		x += x < 0 ? SizeX : 0;
	}

	if( AddressY == TA_Clamp )
		y = FMath::Clamp( y, 0, SizeY );
	else
	{
		y  = y % SizeY;
		y += y < 0 ? SizeY : 0;
	}
}

} // namespace Nanite