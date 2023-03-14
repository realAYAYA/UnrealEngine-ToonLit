// Copyright Epic Games, Inc. All Rights Reserved.

#include "TessellationTable.h"
#include "CoreMinimal.h"
#include "RHI.h"
#include "RenderUtils.h"

namespace Nanite
{

FTessellationTable::FTessellationTable()
{
	/*
		NumPatterns = (MaxTessFactor + 2) choose 3
		NumPatterns = 1/6 * N(N+1)(N+2)
		= 816
	*/

	HashTable.Clear( MaxNumTris, MaxNumTris );

	uint32 NumOffsets = MaxTessFactor * MaxTessFactor * MaxTessFactor;
	OffsetTable.AddUninitialized( NumOffsets + 1 );

	for( uint32 i = 0; i < NumOffsets; i++ )
	{
		FIntVector TessFactors(
			( (i >> 0) & 15 ) + 1,
			( (i >> 4) & 15 ) + 1,
			( (i >> 8) & 15 ) + 1 );

		FirstVert = Verts.Num();
		FirstTri = Indexes.Num();

		OffsetTable[i].X = FirstVert;
		OffsetTable[i].Y = FirstTri;

		// TessFactors in descending order to reduce size of table.
		if( TessFactors[0] >= TessFactors[1] &&
			TessFactors[1] >= TessFactors[2] )
		{
			//RecursiveSplit( TessFactors );
			UniformTessellateAndSnap( TessFactors );
		}
	}

	// One more on the end so we can do Num = Offset[i+1] - Offset[i].
	OffsetTable[ NumOffsets ].X = Verts.Num();
	OffsetTable[ NumOffsets ].Y = Indexes.Num();

	HashTable.Free();
}

int32 FTessellationTable::GetPattern( FIntVector TessFactors ) const
{
	checkSlow( 0 < TessFactors[0] && TessFactors[0] <= MaxTessFactor );
	checkSlow( 0 < TessFactors[1] && TessFactors[1] <= MaxTessFactor );
	checkSlow( 0 < TessFactors[2] && TessFactors[2] <= MaxTessFactor );

	if( TessFactors[0] < TessFactors[1] ) Swap( TessFactors[0], TessFactors[1] );
	if( TessFactors[0] < TessFactors[2] ) Swap( TessFactors[0], TessFactors[2] );
	if( TessFactors[1] < TessFactors[2] ) Swap( TessFactors[1], TessFactors[2] );

	return
		( TessFactors[0] - 1 ) +
		( TessFactors[1] - 1 ) * 16 +
		( TessFactors[2] - 1 ) * 256;
}

FIntVector FTessellationTable::GetBarycentrics( uint32 Vert ) const
{
	FIntVector Barycentrics;
	Barycentrics.X = Vert & 0xffff;
	Barycentrics.Y = Vert >> 16;
	Barycentrics.Z = BarycentricMax - Barycentrics.X - Barycentrics.Y;
	return Barycentrics;
}

// Average barycentric == average cartesian

float FTessellationTable::LengthSquared( const FIntVector& Barycentrics, const FIntVector& TessFactors ) const
{
	// Barycentric displacement vector:
	// 0 = x + y + z

	FVector3f Norm = FVector3f( Barycentrics ) / BarycentricMax;

	// Length of displacement
	// [ Schindler and Chen 2012, "Barycentric Coordinates in Olympiad Geometry" https://web.evanchen.cc/handouts/bary/bary-full.pdf ]
	return	-Norm.X * Norm.Y * FMath::Square( TessFactors[0] )
			-Norm.Y * Norm.Z * FMath::Square( TessFactors[1] )
			-Norm.Z * Norm.X * FMath::Square( TessFactors[2] );
}

// Snap to exact TessFactor at the edges
void FTessellationTable::SnapAtEdges( FIntVector& Barycentrics, const FIntVector& TessFactors ) const
{
	for( uint32 i = 0; i < 3; i++ )
	{
		const uint32 e0 = i;
		const uint32 e1 = (1 << e0) & 3;

		// Am I on this edge?
		if( Barycentrics[ e0 ] + Barycentrics[ e1 ] == BarycentricMax )
		{
			// Snap toward min barycentric means snapping mirrors. Adjacent patches will thus match.
			uint32 MinIndex = Barycentrics[ e0 ] <  Barycentrics[ e1 ] ? e0 : e1;
			uint32 MaxIndex = Barycentrics[ e0 ] >= Barycentrics[ e1 ] ? e0 : e1;

			// Fixed point round
			uint32 Snapped = ( Barycentrics[ MinIndex ] * TessFactors[i] + (BarycentricMax / 2) - 1 ) & ~( BarycentricMax - 1 );

			Barycentrics[ MinIndex ] = Snapped / TessFactors[i];
			Barycentrics[ MaxIndex ] = BarycentricMax - Barycentrics[ MinIndex ];
		}
	}
}

uint32 FTessellationTable::AddVert( uint32 Vert )
{
	uint32 Hash = MurmurFinalize32( Vert );

	// Find if there already exists one
	uint32 Index;
	for( Index = HashTable.First( Hash ); HashTable.IsValid( Index ); Index = HashTable.Next( Index ) )
	{
		if( Verts[ FirstVert + Index ] == Vert )
		{
			break;
		}
	}
	if( !HashTable.IsValid( Index ) )
	{
		Index = Verts.Add( Vert ) - FirstVert;
		HashTable.Add( Hash, Index );
	}

	return Index;
}

void FTessellationTable::SplitEdge( uint32 TriIndex, uint32 EdgeIndex, uint32 LeftFactor, uint32 RightFactor, const FIntVector& TessFactors )
{
	/*
	===========
		v0
		/\
	e2 /  \ e0
	  /____\
	v2  e1  v1
	===========
	*/

	const uint32 e0 = EdgeIndex;
	const uint32 e1 = (1 << e0) & 3;
	const uint32 e2 = (1 << e1) & 3;

	const uint32 Triangle = Indexes[ TriIndex ];
	const uint32 i0 = ( Triangle >> (e0 * 10) ) & 1023;
	const uint32 i1 = ( Triangle >> (e1 * 10) ) & 1023;
	const uint32 i2 = ( Triangle >> (e2 * 10) ) & 1023;

#if 0
	// Sort verts for deterministic split
	uint32 v[2];
	v[0] = FMath::Min( Verts[ FirstVert + i0 ], Verts[ FirstVert + i1 ] );
	v[1] = FMath::Max( Verts[ FirstVert + i0 ], Verts[ FirstVert + i1 ] );

	uint32 OriginallyZero = 0;
	FIntVector Barycentrics[2];
	for( int j = 0; j < 2; j++ )
	{
		Barycentrics[j] = GetBarycentrics( v[j] );

		// Count how many were zero originally.
		OriginallyZero += Barycentrics[j].X == 0 ?  1 : 0;
		OriginallyZero += Barycentrics[j].Y == 0 ?  4 : 0;
		OriginallyZero += Barycentrics[j].Z == 0 ? 16 : 0;
	}

	FIntVector SplitBarycentrics = Barycentrics[0] * LeftFactor + Barycentrics[1] * RightFactor;

	for( uint32 i = 0; i < 3; i++ )
		SplitBarycentrics[i] = FMath::DivideAndRoundNearest( (uint32)SplitBarycentrics[i], LeftFactor + RightFactor );

	for( uint32 i = 0; i < 3; i++ )
	{
		// If both verts were originally zero then force split to be zero as well.
		if( ( OriginallyZero & 3 ) == 2 )
			SplitBarycentrics[i] = 0;

		OriginallyZero >>= 2;
	}
#else
	// Sort verts for deterministic split
	FIntVector SplitBarycentrics =
		GetBarycentrics( FMath::Min( Verts[ FirstVert + i0 ], Verts[ FirstVert + i1 ] ) ) * LeftFactor +
		GetBarycentrics( FMath::Max( Verts[ FirstVert + i0 ], Verts[ FirstVert + i1 ] ) ) * RightFactor;

	bool bOriginallyZero[3] =
	{
		SplitBarycentrics.X == 0,
		SplitBarycentrics.Y == 0,
		SplitBarycentrics.Z == 0,
	};

	for( uint32 i = 0; i < 3; i++ )
		SplitBarycentrics[i] = FMath::DivideAndRoundNearest( (uint32)SplitBarycentrics[i], LeftFactor + RightFactor );
#endif
	
	uint32 Largest = FMath::Max3Index( SplitBarycentrics[0], SplitBarycentrics[1], SplitBarycentrics[2] );
	uint32 Sum = SplitBarycentrics[0] + SplitBarycentrics[1] + SplitBarycentrics[2];
	SplitBarycentrics[ Largest ] += BarycentricMax - Sum;

	SnapAtEdges( SplitBarycentrics, TessFactors );

	check( SplitBarycentrics[0] + SplitBarycentrics[1] + SplitBarycentrics[2] == BarycentricMax );
	check( !bOriginallyZero[0] || SplitBarycentrics[0] == 0 );
	check( !bOriginallyZero[1] || SplitBarycentrics[1] == 0 );
	check( !bOriginallyZero[2] || SplitBarycentrics[2] == 0 );

	uint32 SplitVert = SplitBarycentrics[0] | ( SplitBarycentrics[1] << 16 );
	uint32 SplitIndex = AddVert( SplitVert );

	checkf( SplitIndex != i0 && SplitIndex != i1 && SplitIndex != i2, TEXT("Degenerate triangle generated") );
	
	// Replace v0
	Indexes.Add( SplitIndex | (i1 << 10) | (i2 << 20) );

	// Replace v1
	Indexes[ TriIndex ] = i0 | ( SplitIndex << 10 ) | (i2 << 20);
}

// Longest edge bisection. Uses Diagsplit rules instead of exact bisection.
void FTessellationTable::RecursiveSplit( const FIntVector& TessFactors )
{
	// Start with patch triangle
	Verts.Add( BarycentricMax + 0 );	// Avoids TArray:Add grabbing reference to constexpr and forcing ODR-use.
	Verts.Add( BarycentricMax << 16 );
	Verts.Add( 0 );

	Indexes.Add( 0 | (1 << 10) | (2 << 20) );

	HashTable.Clear();
	HashTable.Add( Verts[0], 0 );
	HashTable.Add( Verts[1], 1 );
	HashTable.Add( Verts[2], 2 );

	for( int32 TriIndex = FirstTri; TriIndex < Indexes.Num(); )
	{
		float EdgeLength2[3];
		for( uint32 i = 0; i < 3; i++ )
		{
			const uint32 e0 = i;
			const uint32 e1 = (1 << e0) & 3;

			const uint32 Triangle = Indexes[ TriIndex ];
			const uint32 i0 = ( Triangle >> (e0 * 10) ) & 1023;
			const uint32 i1 = ( Triangle >> (e1 * 10) ) & 1023;

			FIntVector b0 = GetBarycentrics( Verts[ FirstVert + i0 ] );
			FIntVector b1 = GetBarycentrics( Verts[ FirstVert + i1 ] );

			EdgeLength2[i] = LengthSquared( b0 - b1, TessFactors );
		}

		uint32 EdgeIndex = FMath::Max3Index( EdgeLength2[0], EdgeLength2[1], EdgeLength2[2] );
		check( EdgeLength2[ EdgeIndex ] >= 0.0f );

		uint32 NumEdgeSplits = FMath::RoundToInt( FMath::Sqrt( EdgeLength2[ EdgeIndex ] ) );
		uint32 HalfSplit = NumEdgeSplits >> 1;

		if( NumEdgeSplits <= 1 )
		{
			// Triangle is small enough
			TriIndex++;
			continue;
		}

		SplitEdge( TriIndex, EdgeIndex, HalfSplit, NumEdgeSplits - HalfSplit, TessFactors );
	}
}

void FTessellationTable::UniformTessellateAndSnap( const FIntVector& TessFactors )
{
	/*
	===========
		v0
		/\
	e2 /  \ e0
	  /____\
	v2  e1  v1
	===========
	*/

	HashTable.Clear();

	const uint32 NumTris = TessFactors[0] * TessFactors[0];

	for( uint32 TriIndex = 0; TriIndex < NumTris; TriIndex++ )
	{
		/*
			Starts from top point. Adds rows of verts and corresponding rows of tri strips.

			|\
		row |\|\
			|\|\|\
			column
		*/

		// Find largest tessellation with NumTris <= TriIndex. These are the preceding tris before this row.
		uint32 TriRow = FMath::FloorToInt( FMath::Sqrt( (float)TriIndex ) );
		uint32 TriCol = TriIndex - TriRow * TriRow;
		/*
			Vert order:
			0    0__1
			|\   \  |
			| \   \ |  <= flip triangle
			|__\   \|
			2   1   2
		*/
		uint32 FlipTri = TriCol & 1;
		uint32 VertCol = TriCol >> 1;

		uint32 VertRowCol[3][2] =
		{
			{ TriRow,		VertCol		},
			{ TriRow + 1,	VertCol + 1	},
			{ TriRow + 1,	VertCol		},
		};
		VertRowCol[1][0] -= FlipTri;
		VertRowCol[2][1] += FlipTri;

		uint32 TriVerts[3];
		for( int Corner = 0; Corner < 3; Corner++ )
		{
			/*
				b0
				|\
			t2  | \  t0
				|__\
			   b2   b1
				 t1
			*/
			FIntVector Barycentrics;
			Barycentrics[0] = TessFactors[0] - VertRowCol[ Corner ][0];
			Barycentrics[1] = VertRowCol[ Corner ][1];
			Barycentrics[2] = VertRowCol[ Corner ][0] - VertRowCol[ Corner ][1];
			Barycentrics *= BarycentricMax;

			// Fixed point round
			Barycentrics[0] = ( Barycentrics[0] + (BarycentricMax / 2) - 1 ) & ~( BarycentricMax - 1 );
			Barycentrics[1] = ( Barycentrics[1] + (BarycentricMax / 2) - 1 ) & ~( BarycentricMax - 1 );
			Barycentrics[2] = ( Barycentrics[2] + (BarycentricMax / 2) - 1 ) & ~( BarycentricMax - 1 );
			Barycentrics /= TessFactors[0];

			{
				const uint32 e0 = FMath::Max3Index( Barycentrics[0], Barycentrics[1], Barycentrics[2] );
				const uint32 e1 = (1 << e0) & 3;
				const uint32 e2 = (1 << e1) & 3;

				Barycentrics[ e0 ] = BarycentricMax - Barycentrics[ e1 ] - Barycentrics[ e2 ];
			}

#if 1
			for( uint32 i = 0; i < 3; i++ )
			{
				const uint32 e0 = i;
				const uint32 e1 = (1 << e0) & 3;
				const uint32 e2 = (1 << e1) & 3;

				if( Barycentrics[ e0 ] == 0 ||
					Barycentrics[ e1 ] == 0 ||
					Barycentrics[ e2 ] == 0 )
					continue;

				uint32 Sum = Barycentrics[ e0 ] + Barycentrics[ e1 ];

#if 0
				// Snap toward min barycentric means snapping mirrors.
				uint32 MinIndex = Barycentrics[ e0 ] <  Barycentrics[ e1 ] ? e0 : e1;
				uint32 MaxIndex = Barycentrics[ e0 ] >= Barycentrics[ e1 ] ? e0 : e1;

				// Fixed point round
				uint32 Snapped = ( Barycentrics[ MinIndex ] * TessFactors[i] + (BarycentricMax / 2) - 1 ) & ~( BarycentricMax - 1 );

				Barycentrics[ MinIndex ] = FMath::Min( Sum, Snapped / TessFactors[i] );
				Barycentrics[ MaxIndex ] = Sum - Barycentrics[ MinIndex ];

				if( Barycentrics[ MinIndex ] > Barycentrics[ MaxIndex ] )
				{
					Barycentrics[ e0 ] = Sum / 2;
					Barycentrics[ e1 ] = Sum - Barycentrics[ e0 ];
				}
#else
				// Fixed point round
				uint32 Snapped = ( Barycentrics[ e0 ] * TessFactors[i] + (BarycentricMax / 2) - 1 ) & ~( BarycentricMax - 1 );

				Barycentrics[ e0 ] = FMath::Min( Sum, Snapped / TessFactors[i] );
				Barycentrics[ e1 ] = Sum - Barycentrics[ e0 ];
#endif
			}
#endif

#if 1
			// Snap verts to the edge if they are close.
			if( Barycentrics.X != 0 &&
				Barycentrics.Y != 0 &&
				Barycentrics.Z != 0 )
			{
				// Find closest point on edge
				uint32 b0 = FMath::Min3Index( Barycentrics[0], Barycentrics[1], Barycentrics[2] );
				uint32 b1 = (1 << b0) & 3;
				uint32 b2 = (1 << b1) & 3;

				//if( Barycentrics[ b1 ] < Barycentrics[ b2 ] )
				//	Swap( b1, b2 );

				uint32 Sum = Barycentrics[ b1 ] + Barycentrics[ b2 ];

				FIntVector ClosestEdgePoint;
				ClosestEdgePoint[ b0 ] = 0;
				ClosestEdgePoint[ b1 ] = ( Barycentrics[ b1 ] * BarycentricMax ) / Sum;
				ClosestEdgePoint[ b2 ] = BarycentricMax - ClosestEdgePoint[ b1 ];

				// Want edge point in its final position so we get the correct distance.
				SnapAtEdges( ClosestEdgePoint, TessFactors );

				float DistSqr = LengthSquared( Barycentrics - ClosestEdgePoint, TessFactors );
				if( DistSqr < 0.25f )
				{
					Barycentrics = ClosestEdgePoint;
				}
			}
#endif

			SnapAtEdges( Barycentrics, TessFactors );

			TriVerts[ Corner ] = Barycentrics[0] | ( Barycentrics[1] << 16 );
		}

		// Degenerate
		if( TriVerts[0] == TriVerts[1] ||
			TriVerts[1] == TriVerts[2] ||
			TriVerts[2] == TriVerts[0] )
			continue;

		uint32 VertIndexes[3];
		for( int Corner = 0; Corner < 3; Corner++ )
			VertIndexes[ Corner ] = AddVert( TriVerts[ Corner ] );

		Indexes.Add( VertIndexes[0] | ( VertIndexes[1] << 10 ) | ( VertIndexes[2] << 20 ) );
	}
}

FTessellationTable& GetTessellationTable()
{
	static FTessellationTable TessellationTable;
	return TessellationTable;
}


template< typename T >
static void CreateAndUpload( FByteAddressBuffer& Buffer, const TArray<T>& Array, const TCHAR* InDebugName )
{
	Buffer.Initialize( InDebugName, Array.Num() * Array.GetTypeSize() );

	uint8* DataPtr = (uint8*)RHILockBuffer( Buffer.Buffer, 0, Buffer.NumBytes, RLM_WriteOnly );

	FMemory::Memcpy( DataPtr, Array.GetData(), Buffer.NumBytes );

	RHIUnlockBuffer( Buffer.Buffer );
}

void FTessellationTableResources::InitRHI()
{
	if( DoesPlatformSupportNanite( GMaxRHIShaderPlatform ) )
	{
		FTessellationTable TessellationTable;

		CreateAndUpload( Offsets,	TessellationTable.OffsetTable,	TEXT("TessellationTable.Offsets") );
		CreateAndUpload( Verts,		TessellationTable.Verts,		TEXT("TessellationTable.Verts") );
		CreateAndUpload( Indexes,	TessellationTable.Indexes,		TEXT("TessellationTable.Indexes") );
	}
}

void FTessellationTableResources::ReleaseRHI()
{
	if( DoesPlatformSupportNanite( GMaxRHIShaderPlatform ) )
	{
		Offsets.Release();
		Verts.Release();
		Indexes.Release();
	}
}

} // namespace Nanite
