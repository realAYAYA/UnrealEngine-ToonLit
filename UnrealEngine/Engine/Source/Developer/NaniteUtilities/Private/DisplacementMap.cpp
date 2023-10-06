// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplacementMap.h"

#include "ImageCore.h"
#include "ImageCoreUtils.h"

namespace Nanite
{

FDisplacementMap::FDisplacementMap()
	: SourceFormat( TSF_G8 )
	, BytesPerPixel(1)
	, SizeX(1)
	, SizeY(1)
	, NumLevels(1)
	, Magnitude( 0.0f )
	, Center( 0.0f )
	, AddressX( TA_Wrap )
	, AddressY( TA_Wrap )
{
	SourceData.Add(0);
}

FDisplacementMap::FDisplacementMap( FImage&& TextureSourceImage, float InMagnitude, float InCenter, TextureAddress InAddressX, TextureAddress InAddressY )
	: NumLevels(1)
	, Magnitude( InMagnitude )
	, Center( InCenter )
	, AddressX( InAddressX )
	, AddressY( InAddressY )
{
	SourceData = MoveTemp(TextureSourceImage.RawData);
	check(!SourceData.IsEmpty());

	SourceFormat = FImageCoreUtils::ConvertToTextureSourceFormat(TextureSourceImage.Format);
	BytesPerPixel = ERawImageFormat::GetBytesPerPixel(TextureSourceImage.Format);
		
	SizeX = TextureSourceImage.GetWidth();
	SizeY = TextureSourceImage.GetHeight();

	uint32 PrevSizeX = SizeX;
	uint32 PrevSizeY = SizeY;
	for( uint32 Level = 1; ; Level++ )
	{
		uint32 MipSizeX = ( ( SizeX - 1 ) >> Level ) + 1;
		uint32 MipSizeY = ( ( SizeY - 1 ) >> Level ) + 1;
		if( MipSizeX == 1 && MipSizeY == 1 )
			break;

		MipData[ Level - 1 ].AddUninitialized( MipSizeX * MipSizeY );

		for( uint32 y = 0; y < MipSizeY; y++ )
		{
			for( uint32 x = 0; x < MipSizeX; x++ )
			{
				uint32 x0 = x*2;
				uint32 y0 = y*2;
				uint32 x1 = FMath::Min( x0 + 1, PrevSizeX - 1 );
				uint32 y1 = FMath::Min( y0 + 1, PrevSizeY - 1 );

				if( Level == 1 )
				{
					float d0 = Load( x0, y0 );
					float d1 = Load( x1, y0 );
					float d2 = Load( x0, y1 );
					float d3 = Load( x1, y1 );

					MipData[ Level - 1 ][ x + y * MipSizeX ] = FVector2f(
						FMath::Min( d0, FMath::Min3( d1, d2, d3 ) ),
						FMath::Max( d0, FMath::Max3( d1, d2, d3 ) ) );
				}
				else
				{
					FVector2f d0 = Load( x0, y0, Level - 1 );
					FVector2f d1 = Load( x1, y0, Level - 1 );
					FVector2f d2 = Load( x0, y1, Level - 1 );
					FVector2f d3 = Load( x1, y1, Level - 1 );

					MipData[ Level - 1 ][ x + y * MipSizeX ] = FVector2f(
						FMath::Min( d0.X, FMath::Min3( d1.X, d2.X, d3.X ) ),
						FMath::Max( d0.Y, FMath::Max3( d1.Y, d2.Y, d3.Y ) ) );
				}
			}
		}

		PrevSizeX = MipSizeX;
		PrevSizeY = MipSizeY;
		NumLevels++;
	}
}

// Bilinear filtered
float FDisplacementMap::Sample( FVector2f UV ) const
{
	// Half texel
	UV.X = UV.X * SizeX - 0.5f;
	UV.Y = UV.Y * SizeY - 0.5f;

	int32 x0 = FMath::FloorToInt32( UV.X );
	int32 y0 = FMath::FloorToInt32( UV.Y );
	int32 x1 = x0 + 1;
	int32 y1 = y0 + 1;

	float wx1 = UV.X - x0;
	float wy1 = UV.Y - y0;
	float wx0 = 1.0f - wx1;
	float wy0 = 1.0f - wy1;

	return
		Sample( x0, y0 ) * wx0 * wy0 +
		Sample( x1, y0 ) * wx1 * wy0 +
		Sample( x0, y1 ) * wx0 * wy1 +
		Sample( x1, y1 ) * wx1 * wy1;
}

// Returns min/max over bilinear footprint
FVector2f FDisplacementMap::Sample( FVector2f MinUV, FVector2f MaxUV ) const
{
	// Half texel
	MinUV = MinUV * FVector2f( SizeX, SizeY ) - FVector2f( 0.5f );
	MaxUV = MaxUV * FVector2f( SizeX, SizeY ) - FVector2f( 0.5f );

	int32 x0 = FMath::FloorToInt32( MinUV.X );
	int32 y0 = FMath::FloorToInt32( MinUV.Y );
	int32 x1 = FMath::FloorToInt32( MaxUV.X ) + 1;
	int32 y1 = FMath::FloorToInt32( MaxUV.Y ) + 1;

	uint32 Level = FMath::FloorLog2( FMath::Max( x1 - x0, y1 - y0 ) );

	if( (x1 >> Level) - (x0 >> Level) > 1 ||
		(y1 >> Level) - (y0 >> Level) > 1 )
		Level++;

	Level = FMath::Min( Level, NumLevels - 1 );

	if( Level == 0 )
	{
		float d0 = Sample( x0, y0 );
		float d1 = Sample( x1, y0 );
		float d2 = Sample( x0, y1 );
		float d3 = Sample( x1, y1 );

		return FVector2f(
			FMath::Min( d0, FMath::Min3( d1, d2, d3 ) ),
			FMath::Max( d0, FMath::Max3( d1, d2, d3 ) ) );
	}
	else
	{
		FVector2f d0 = Sample( x0, y0, Level );
		FVector2f d1 = Sample( x1, y0, Level );
		FVector2f d2 = Sample( x0, y1, Level );
		FVector2f d3 = Sample( x1, y1, Level );

		return FVector2f(
			FMath::Min( d0.X, FMath::Min3( d1.X, d2.X, d3.X ) ),
			FMath::Max( d0.Y, FMath::Max3( d1.Y, d2.Y, d3.Y ) ) );
	}
}

} // namespace Nanite