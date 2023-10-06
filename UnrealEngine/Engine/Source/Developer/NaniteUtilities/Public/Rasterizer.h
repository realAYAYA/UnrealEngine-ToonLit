// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

inline FIntPoint ToIntPoint( const FVector3f& V )
{
	return FIntPoint(
		FMath::RoundToInt( V.X ),
		FMath::RoundToInt( V.Y ) );
}

template< typename FWritePixel >
void RasterizeTri( const FVector3f Verts[3], const FIntRect& ScissorRect, uint32 SubpixelDilate, FWritePixel WritePixel )
{
	constexpr uint32 SubpixelBits		= 8;
	constexpr uint32 SubpixelSamples	= 1 << SubpixelBits;

	FVector3f v01 = Verts[1] - Verts[0];
	FVector3f v02 = Verts[2] - Verts[0];

	float DetXY = v01.X * v02.Y - v01.Y * v02.X;
	if( DetXY >= 0.0f )
	{
		// Backface cull
		// If not culling, need to swap verts to correct winding for rest of code
		return;
	}

	FVector2f GradZ;
	GradZ.X = ( v01.Z * v02.Y - v01.Y * v02.Z ) / DetXY;
	GradZ.Y = ( v01.X * v02.Z - v01.Z * v02.X ) / DetXY;

	// 24.8 fixed point
	FIntPoint Vert0 = ToIntPoint( Verts[0] * SubpixelSamples );
	FIntPoint Vert1 = ToIntPoint( Verts[1] * SubpixelSamples );
	FIntPoint Vert2 = ToIntPoint( Verts[2] * SubpixelSamples );

	// Bounding rect
	FIntRect RectSubpixel( Vert0, Vert0 );
	RectSubpixel.Include( Vert1 );
	RectSubpixel.Include( Vert2 );
	RectSubpixel.InflateRect( SubpixelDilate );

	// Round to nearest pixel
	FIntRect RectPixel = ( ( RectSubpixel + (SubpixelSamples / 2) - 1 ) ) / SubpixelSamples;

	// Clip to viewport
	RectPixel.Clip( ScissorRect );
	
	// Cull when no pixels covered
	if( RectPixel.IsEmpty() )
		return;

	// 12.8 fixed point
	FIntPoint Edge01 = Vert0 - Vert1;
	FIntPoint Edge12 = Vert1 - Vert2;
	FIntPoint Edge20 = Vert2 - Vert0;

	// Rebase off MinPixel with half pixel offset
	// 12.8 fixed point
	// Max triangle size = 2047x2047 pixels
	const FIntPoint BaseSubpixel = RectPixel.Min * SubpixelSamples + (SubpixelSamples / 2);
	Vert0 -= BaseSubpixel;
	Vert1 -= BaseSubpixel;
	Vert2 -= BaseSubpixel;

	auto EdgeC = [=]( const FIntPoint& Edge, const FIntPoint& Vert )
	{
		int64 ex = Edge.X;
		int64 ey = Edge.Y;
		int64 vx = Vert.X;
		int64 vy = Vert.Y;

		// Half-edge constants
		// 24.16 fixed point
		int64 C = ey * vx - ex * vy;

		// Correct for fill convention
		// Top left rule for CCW
		C -= ( Edge.Y < 0 || ( Edge.Y == 0 && Edge.X > 0 ) ) ? 0 : 1;

		// Dilate edges
		C += ( FMath::Abs( Edge.X ) + FMath::Abs( Edge.Y ) ) * SubpixelDilate;

		// Step in pixel increments
		// Low bits would always be the same and thus don't matter when testing sign.
		// 24.8 fixed point
		return int32( C >> SubpixelBits );
	};

	int32 C0 = EdgeC( Edge01, Vert0 );
	int32 C1 = EdgeC( Edge12, Vert1 );
	int32 C2 = EdgeC( Edge20, Vert2 );
	float Z0 = Verts[0].Z - ( GradZ.X * (float)Vert0.X + GradZ.Y * (float)Vert0.Y ) / (float)SubpixelSamples;
	
	int32 CY0 = C0;
	int32 CY1 = C1;
	int32 CY2 = C2;
	float ZY = Z0;

	for( int32 y = RectPixel.Min.Y; y < RectPixel.Max.Y; y++ )
	{
		int32 CX0 = CY0;
		int32 CX1 = CY1;
		int32 CX2 = CY2;
		float ZX = ZY;

		for( int32 x = RectPixel.Min.X; x < RectPixel.Max.X; x++ )
		{
			if( ( CX0 | CX1 | CX2 ) >= 0 )
			{
				WritePixel( x, y, ZX );
			}

			CX0 -= Edge01.Y;
			CX1 -= Edge12.Y;
			CX2 -= Edge20.Y;
			ZX += GradZ.X;
		}

		CY0 += Edge01.X;
		CY1 += Edge12.X;
		CY2 += Edge20.X;
		ZY += GradZ.Y;
	}
}