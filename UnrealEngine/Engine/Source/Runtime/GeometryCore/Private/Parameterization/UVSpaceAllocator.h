// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FRect;
struct Rect;
struct FMD5Hash;

namespace UE
{
namespace Geometry
{

//
// GeometryProcessing port of MeshUtilitiesCommon FAllocator2D
//
class FUVSpaceAllocator
{
public:
	enum class EMode
	{
		// In this mode, segments represents free space
		// Used for the layout merging usedsegments
		FreeSegments,
		// In this mode, segments represents used space
		// Used for the rasterization of charts.
		UsedSegments
	};

	struct FRect
	{
		uint32 X;
		uint32 Y;
		uint32 W;
		uint32 H;
	};

	struct FSegment
	{
		uint32 StartPos;
		uint32 Length;

		bool operator<( const FSegment& Other ) const { return StartPos < Other.StartPos; }
	};

	struct FRun
	{
		uint32 LongestSegment;
		TArray< FSegment > Segments;

		// Contains mapping from pixel position to first segment index in search range.
		// Only computed when we're in FreeSegments mode to help TestOneRun find
		// the proper segment in O(c) at the expense of a likely cache miss.
		// We'll use a threshold to use this method when the number of iterations
		// saved is worth the cache miss.
		// Obviously, using a uint16 here will reduce cache misses but impose
		// an hopefully enough limitation of texture size 65536x65536 (4GB).
		TArray< uint16 > FreeSegmentsLookup;
	};

public:
	FUVSpaceAllocator( EMode Mode, uint32 Width, uint32 Height );

	// Must clear before using
	void       Clear();

	bool       Find( FRect& Rect );
	bool       Test( FRect Rect );
	void       Alloc( FRect Rect );

	bool       FindBitByBit( FRect& Rect, const FUVSpaceAllocator& Other );
	bool       FindWithSegments( FRect& Rect, const FUVSpaceAllocator& Other, TFunctionRef<bool (const FUVSpaceAllocator::FRect&)> IsBestRect ) const;
	bool       Test( FRect Rect, const FUVSpaceAllocator& Other );
	void       Alloc( FRect Rect, const FUVSpaceAllocator& Other );

	uint64     GetBit( uint32 x, uint32 y ) const;
	void       SetBit( uint32 x, uint32 y );
	void       ClearBit( uint32 x, uint32 y );

	void       CreateUsedSegments();
	void       MergeRun( FRun& Run, const FRun& OtherRun, uint32 RectOffset, uint32 RectLength, uint32 PrimaryResolution /* Resolution along the axis the run belongs to */, uint32 PerpendicularResolution );
	void       MergeSegments( const FRect& Rect, const FUVSpaceAllocator& Other );

	void       FlipX( const FRect& Rect );
	void       FlipY( const FRect& Rect );

	uint32     GetUsedTexels() const;

	void       CopyRuns( TArray<FRun>& Runs, const TArray<FRun>& OtherRuns, int32 MaxSize );

	// Take control of the copy to reduce the amount of data movement to the strict minimum
	FUVSpaceAllocator& operator = (const FUVSpaceAllocator& Other);

	// Allow to visualize the content in ascii for debugging purpose. (i.e Watch or Immediate window).
	FString    ToString() const;

	// Get the MD5 hash of the rasterized content
	FMD5Hash   GetRasterMD5() const;

	uint32     GetRasterWidth()  const { return RasterWidth; }
	uint32     GetRasterHeight() const { return RasterHeight; }

protected:
	bool       TestOneRun( const FRun& Run, const FRun& OtherRun, uint32 RectOffset, uint32 RectLength, uint32 PrimaryResolution, uint32& OutFailedLength ) const;
	bool       TestAllRows( const FRect& Rect, const FUVSpaceAllocator& Other, uint32& FailedLength ) const;
	bool       TestAllColumns( const FRect& Rect, const FUVSpaceAllocator& Other, uint32& FailedLength ) const;
	void       InitRuns( TArray<FRun>& Runs, uint32 PrimaryResolution, uint32 PerpendicularRasterSize);
	void       InitSegments();
	void       AddUsedSegment( FRun& Run, uint32 StartPos, uint32 Length );

	// Enforce that those cannot be changed in flight
	const EMode              Mode;
	const uint32             Width;
	const uint32             Height;
	const uint32             Pitch;

	uint32                   RasterWidth;
	uint32                   RasterHeight;
	TArray< FRun >           Rows;        // Represent rows in the grid
	TArray< FRun >           Columns;     // Represent columns in the grid (used when version >= Segments2D).
	TArray< uint64 >         Bits;

	// Index inside rows that will be sorted by rows with longest used segment first
	TArray< uint16 >         SortedRowsIndex;
	// Index inside columns that will be sorted by columns with longest used segment first
	TArray< uint16 >         SortedColumnsIndex;
};

// Returns non-zero if set
FORCEINLINE uint64 FUVSpaceAllocator::GetBit( uint32 x, uint32 y ) const
{
	return Bits[ (x >> 6) + y * Pitch ] & ( 1ull << ( x & 63 ) );
}

FORCEINLINE void FUVSpaceAllocator::SetBit( uint32 x, uint32 y )
{
	Bits[ (x >> 6) + y * Pitch ] |= ( 1ull << ( x & 63 ) );

	// Keep track of the rasterized dimension to optimize operations on that area only
	if (y >= RasterHeight)
	{
		RasterHeight = y + 1;
	}

	if (x >= RasterWidth)
	{
		RasterWidth = x + 1;
	}
}

FORCEINLINE void FUVSpaceAllocator::ClearBit( uint32 x, uint32 y )
{
	Bits[ (x >> 6) + y * Pitch ] &= ~( 1ull << ( x & 63 ) );
}

inline bool FUVSpaceAllocator::Test( FRect Rect )
{
	for( uint32 y = Rect.Y; y < Rect.Y + Rect.H; y++ )
	{
		for( uint32 x = Rect.X; x < Rect.X + Rect.W; x++ )
		{
			if( GetBit( x, y ) )
			{
				return false;
			}
		}
	}
	
	return true;
}

inline bool FUVSpaceAllocator::Test( FRect Rect, const FUVSpaceAllocator& Other )
{
	const uint32 LowShift = Rect.X & 63;
	const uint32 HighShift = 64 - LowShift;

	for( uint32 y = 0; y < Rect.H; y++ )
	{
#if 1
		uint32 ThisIndex = (Rect.X >> 6) + (y + Rect.Y) * Pitch;
		uint32 OtherIndex = y * Pitch;

		// Test a uint64 at a time
		for( uint32 x = 0; x < Rect.W; x += 64 )
		{
			// no need to zero out HighInt on wrap around because Other will always be zero outside Rect.
			uint64 LowInt  = Bits[ ThisIndex ];
			uint64 HighInt = Bits[ ThisIndex + 1 ];

			uint64 ThisInt = (HighInt << HighShift) | (LowInt >> LowShift);
			uint64 OtherInt = Other.Bits[ OtherIndex ];

			if( ThisInt & OtherInt )
			{
				return false;
			}

			ThisIndex++;
			OtherIndex++;
		}
#else
		for( uint32 x = 0; x < Rect.W; x++ )
		{
			if( Other.GetBit( x, y ) && GetBit( x + Rect.X, y + Rect.Y ) )
			{
				return false;
			}
		}
#endif
	}
	
	return true;
}


} // end namespace UE::Geometry
} // end namespace UE