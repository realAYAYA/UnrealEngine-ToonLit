// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/HashTable.h"

namespace Nanite
{

FORCEINLINE uint32 HashPosition( const FVector3f& Position )
{
	union { float f; uint32 i; } x;
	union { float f; uint32 i; } y;
	union { float f; uint32 i; } z;

	x.f = Position.X;
	y.f = Position.Y;
	z.f = Position.Z;

	return Murmur32( {
		Position.X == 0.0f ? 0u : x.i,
		Position.Y == 0.0f ? 0u : y.i,
		Position.Z == 0.0f ? 0u : z.i
	} );
}

FORCEINLINE uint32 Cycle3( uint32 Value )
{
	uint32 ValueMod3 = Value % 3;
	uint32 Value1Mod3 = ( 1 << ValueMod3 ) & 3;
	return Value - ValueMod3 + Value1Mod3;
}

FORCEINLINE uint32 Cycle3( uint32 Value, uint32 Offset )
{
	return Value - Value % 3 + ( Value + Offset ) % 3;
}

FORCEINLINE float EquilateralArea( float EdgeLength )
{
	const float sqrt3_4 = 0.4330127f;
	return sqrt3_4 * FMath::Square( EdgeLength * EdgeLength );
}

FORCEINLINE float EquilateralEdgeLength( float Area )
{
	const float sqrt3_4 = 0.4330127f;
	return FMath::Sqrt( Area / sqrt3_4 );
}

FORCEINLINE float TriangleArea( float a, float b, float c )
{
	float AreaSqrTimes16 = FMath::Max( 0.0f,
		( a + b + c ) *
		(-a + b + c ) *
		( a - b + c ) *
		( a + b - c ) );
	return FMath::Sqrt( AreaSqrTimes16 ) * 0.25f;
}

// a,b,c are tessellation factors for each edge
FORCEINLINE int32 ApproxNumTris( int32 a, int32 b, int32 c )
{
	// Heron's formula divided by area of unit triangle
	float s = 0.5f * float( a + b + c );
	float NumTris = 4.0f * FMath::Sqrt( FMath::Max( 0.0625f, s * (s - (float)a) * (s - (float)b) * (s - (float)c) / 3.0f ) );
	int32 MaxFactor = FMath::Max3( a, b, c );
	return FMath::Max( FMath::RoundToInt( NumTris ), MaxFactor );
}


namespace Barycentric
{
	// [ Schindler and Chen 2012, "Barycentric Coordinates in Olympiad Geometry" https://web.evanchen.cc/handouts/bary/bary-full.pdf ]
	FORCEINLINE float LengthSquared( const FVector3f& Barycentrics0, const FVector3f& Barycentrics1, const FVector3f& EdgeLengthsSqr )
	{
		// Barycentric displacement vector:
		// 0 = x + y + z
		FVector3f Disp = Barycentrics0 - Barycentrics1;

		/*	TODO change edge order to match ariel coords
				v0
				/\
			e2 /  \ e0
			  /____\
			v2  e1  v1
		*/

		// Length of displacement
		return	-Disp.X * Disp.Y * EdgeLengthsSqr[0]
				-Disp.Y * Disp.Z * EdgeLengthsSqr[1]
				-Disp.Z * Disp.X * EdgeLengthsSqr[2];
	}

	FORCEINLINE float SubtriangleArea( const FVector3f& Barycentrics0, const FVector3f& Barycentrics1, const FVector3f& Barycentrics2, float TriangleArea )
	{
		// Area * Determinant using triple product
		return TriangleArea * FMath::Abs( Barycentrics0 | ( Barycentrics1 ^ Barycentrics2 ) );
	}

	// https://math.stackexchange.com/questions/3748903/closest-point-to-triangle-edge-with-barycentric-coordinates
	FORCEINLINE float DistanceToEdge( float Barycentric, float EdgeLength, float TriangleArea )
	{
		return 2.0f * Barycentric * TriangleArea / EdgeLength;
	}

	// Angle at corner 0
	FORCEINLINE float Cotangent(
		const FVector3f& Barycentrics0,
		const FVector3f& Barycentrics1,
		const FVector3f& Barycentrics2,
		const FVector3f& EdgeLengthsSqr,
		float TriangleArea )
	{
		FVector3f LengthsSqr;
		LengthsSqr[0] = LengthSquared( Barycentrics1, Barycentrics2, EdgeLengthsSqr );
		LengthsSqr[1] = LengthSquared( Barycentrics2, Barycentrics0, EdgeLengthsSqr );
		LengthsSqr[2] = LengthSquared( Barycentrics0, Barycentrics1, EdgeLengthsSqr );
	
		float Area = SubtriangleArea( Barycentrics0, Barycentrics1, Barycentrics2, TriangleArea );
	
		return 0.25f * ( -LengthsSqr.X + LengthsSqr.Y + LengthsSqr.Z ) / Area;
	}
}

FORCEINLINE void SubtriangleBarycentrics( uint32 TriX, uint32 TriY, uint32 FlipTri, uint32 NumSubdivisions, FVector3f Barycentrics[3] )
{
	/*
		Vert order:
		1    0__1
		|\   \  |
		| \   \ |  <= flip triangle
		|__\   \|
		0   2   2
	*/

	uint32 VertXY[3][2] =
	{
		{ TriX,		TriY	},
		{ TriX,		TriY + 1},
		{ TriX + 1,	TriY	},
	};
	VertXY[0][1] += FlipTri;
	VertXY[1][0] += FlipTri;

	for( int Corner = 0; Corner < 3; Corner++ )
	{
		Barycentrics[ Corner ][0] = (float)VertXY[ Corner ][0];
		Barycentrics[ Corner ][1] = (float)VertXY[ Corner ][1];
		Barycentrics[ Corner ][2] = float(NumSubdivisions - VertXY[ Corner ][0] - VertXY[ Corner ][1]);
		Barycentrics[ Corner ]   /= (float)NumSubdivisions;
	}
}


// Find edge with opposite direction that shares these 2 verts.
/*
	  /\
	 /  \
	o-<<-o
	o->>-o
	 \  /
	  \/
*/
class FEdgeHash
{
public:
	FHashTable HashTable;

	FEdgeHash( int32 Num )
		: HashTable( 1 << FMath::FloorLog2( Num ), Num )
	{}

	template< typename FGetPosition >
	void Add_Concurrent( int32 EdgeIndex, FGetPosition&& GetPosition )
	{
		const FVector3f& Position0 = GetPosition( EdgeIndex );
		const FVector3f& Position1 = GetPosition( Cycle3( EdgeIndex ) );
				
		uint32 Hash0 = HashPosition( Position0 );
		uint32 Hash1 = HashPosition( Position1 );
		uint32 Hash = Murmur32( { Hash0, Hash1 } );

		HashTable.Add_Concurrent( Hash, EdgeIndex );
	}

	template< typename FGetPosition, typename FuncType >
	void ForAllMatching( int32 EdgeIndex, bool bAdd, FGetPosition&& GetPosition, FuncType&& Function )
	{
		const FVector3f& Position0 = GetPosition( EdgeIndex );
		const FVector3f& Position1 = GetPosition( Cycle3( EdgeIndex ) );
				
		uint32 Hash0 = HashPosition( Position0 );
		uint32 Hash1 = HashPosition( Position1 );
		uint32 Hash = Murmur32( { Hash1, Hash0 } );
		
		for( uint32 OtherEdgeIndex = HashTable.First( Hash ); HashTable.IsValid( OtherEdgeIndex ); OtherEdgeIndex = HashTable.Next( OtherEdgeIndex ) )
		{
			if( Position0 == GetPosition( Cycle3( OtherEdgeIndex ) ) &&
				Position1 == GetPosition( OtherEdgeIndex ) )
			{
				// Found matching edge.
				Function( EdgeIndex, OtherEdgeIndex );
			}
		}

		if( bAdd )
			HashTable.Add( Murmur32( { Hash0, Hash1 } ), EdgeIndex );
	}
};


struct FAdjacency
{
	TArray< int32 >				Direct;
	TMultiMap< int32, int32 >	Extended;

	FAdjacency( int32 Num )
	{
		Direct.AddUninitialized( Num );
	}

	void	Link( int32 EdgeIndex0, int32 EdgeIndex1 )
	{
		if( Direct[ EdgeIndex0 ] < 0 && 
			Direct[ EdgeIndex1 ] < 0 )
		{
			Direct[ EdgeIndex0 ] = EdgeIndex1;
			Direct[ EdgeIndex1 ] = EdgeIndex0;
		}
		else
		{
			Extended.AddUnique( EdgeIndex0, EdgeIndex1 );
			Extended.AddUnique( EdgeIndex1, EdgeIndex0 );
		}
	}

	template< typename FuncType >
	void	ForAll( int32 EdgeIndex, FuncType&& Function ) const
	{
		int32 AdjIndex = Direct[ EdgeIndex ];
		if( AdjIndex >= 0 )
		{
			Function( EdgeIndex, AdjIndex );
		}

		for( auto Iter = Extended.CreateConstKeyIterator( EdgeIndex ); Iter; ++Iter )
		{
			Function( EdgeIndex, Iter.Value() );
		}
	}
};

} // namespace Nanite