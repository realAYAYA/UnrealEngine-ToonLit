// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/HashTable.h"

template< typename T > FORCEINLINE uint32 Min3Index( const T A, const T B, const T C ) { return ( A < B ) ? ( ( A < C ) ? 0 : 2 ) : ( ( B < C ) ? 1 : 2 ); }
template< typename T > FORCEINLINE uint32 Max3Index( const T A, const T B, const T C ) { return ( A > B ) ? ( ( A > C ) ? 0 : 2 ) : ( ( B > C ) ? 1 : 2 ); }

FORCEINLINE uint32 Hash3( const uint32 x, const uint32 y, const uint32 z )
{
	//return ( 73856093 * x ) ^ ( 15485867 * y ) ^ ( 83492791 * z );
	return Murmur32( { x, y, z } );
}

FORCEINLINE uint32 HashPoint( const FVector3f Point )
{
	int32 x = FMath::FloorToInt( Point.X / ( 2.0f * THRESH_POINTS_ARE_SAME ) );
	int32 y = FMath::FloorToInt( Point.Y / ( 2.0f * THRESH_POINTS_ARE_SAME ) );
	int32 z = FMath::FloorToInt( Point.Z / ( 2.0f * THRESH_POINTS_ARE_SAME ) );

	return Hash3( x, y, z );
}

FORCEINLINE uint32 HashPoint( const FVector3f Point, const uint32 Octant )
{
	int32 x = FMath::FloorToInt( Point.X / ( 2.0f * THRESH_POINTS_ARE_SAME ) - 0.5f );
	int32 y = FMath::FloorToInt( Point.Y / ( 2.0f * THRESH_POINTS_ARE_SAME ) - 0.5f );
	int32 z = FMath::FloorToInt( Point.Z / ( 2.0f * THRESH_POINTS_ARE_SAME ) - 0.5f );

	x += ( Octant >> 0 ) & 1;
	y += ( Octant >> 1 ) & 1;
	z += ( Octant >> 2 ) & 1;

	return Hash3( x, y, z );
}

template< typename T >
FORCEINLINE FVector3f GetPosition( const T& Vert )
{
	// Assumes XYZ floats at start of type T
	return *reinterpret_cast< const FVector3f* >( &Vert );
}

/** Split edges until less than MaxEdgeSize */
template< typename T, class LerpClass >
void MeshTessellate( TArray<T>& Verts, TArray< uint32 >& Indexes, float MaxEdgeSize, LerpClass Lerp )
{
	checkSlow( Indexes.Num() % 3 == 0 );

	TMultiMap< uint32, int32 > HashTable;
	HashTable.Reserve( Verts.Num()*2 );
	for( int i = 0; i < Verts.Num(); i++ )
	{
		const T& Vert = Verts[i];
		HashTable.Add( HashPoint( GetPosition( Vert ) ), i );
	}
	
	
	/*
	===========
		v0
		/\
	e2 /  \ e0
	  /____\
	v2  e1  v1
	===========
	*/

	for( int Tri = 0; Tri < Indexes.Num(); )
	{
		float EdgeLength2[3];
		for( int i = 0; i < 3; i++ )
		{
			const uint32 i0 = i;
			const uint32 i1 = (1 << i0) & 3;

			FVector3f p0 = GetPosition( Verts[ Indexes[ Tri + i0 ] ] );
			FVector3f p1 = GetPosition( Verts[ Indexes[ Tri + i1 ] ] );

			EdgeLength2[i] = ( p0 - p1 ).SizeSquared();
		}

		const uint32 e0 = Max3Index( EdgeLength2[0], EdgeLength2[1], EdgeLength2[2] );
		const uint32 e1 = (1 << e0) & 3;
		const uint32 e2 = (1 << e1) & 3;
		if( EdgeLength2[ e0 ] < MaxEdgeSize * MaxEdgeSize )
		{
			// Triangle is small enough
			Tri += 3;
			continue;
		}
		
		const uint32 i0 = Indexes[ Tri + e0 ];
		const uint32 i1 = Indexes[ Tri + e1 ];
		const uint32 i2 = Indexes[ Tri + e2 ];

		const T MidV = Lerp( Verts[i0], Verts[i1], 0.5f );
		uint32  MidI = ~0u;

		FVector3f MidPosition = GetPosition( MidV );

		// Find if there already exists one
		for( uint32 Octant = 0; Octant < 8; Octant++ )
		{
			uint32 Hash = HashPoint( MidPosition, Octant );

			for( auto Iter = HashTable.CreateKeyIterator( Hash ); Iter; ++Iter )
			{
				if( FMemory::Memcmp( &MidV, &Verts[ Iter.Value() ], sizeof(T) ) == 0 )
				{
					MidI = Iter.Value();
					break;
				}
			}
			if( MidI != ~0u )
			{
				break;
			}
		}
		if( MidI == ~0u )
		{
			MidI = Verts.Add( MidV );
			HashTable.Add( HashPoint( MidPosition ), MidI );
		}

		if( !ensureMsgf( MidI != i0 && MidI != i1 && MidI != i2, TEXT("Degenerate triangle generated") ) )
		{
			Tri += 3;
			continue;
		}
	
		// Replace e0
		Indexes.Add( MidI );
		Indexes.Add( i1 );
		Indexes.Add( i2 );

		// Replace e1
		Indexes[ Tri + e1 ] = MidI;
	}
}

template< typename T, class ShouldWeldClass >
void WeldVerts( TArray<T>& Verts, TArray< uint32 >& Indexes, ShouldWeldClass ShouldWeld )
{
	uint32 NumTris = Indexes.Num() / 3;

	TArray<T>			WeldedVerts;
	TArray< uint32 >	WeldedIndexes;

	WeldedVerts.Reserve( NumTris * 2 );
	WeldedIndexes.Reserve( Indexes.Num() );

	TMultiMap< uint32, int32 > HashTable;
	HashTable.Reserve( NumTris * 2 );

	for( int i = 0; i < Indexes.Num(); i++ )
	{
		const T& Vert = Verts[ Indexes[i] ];
		
		// Assumes XYZ floats at start of type T
		FVector3f Position = *reinterpret_cast< const FVector3f* >( &Vert );

		// Find if there already exists one within Equals threshold
		uint32 Index = ~0u;
		for( uint32 Octant = 0; Octant < 8; Octant++ )
		{
			uint32 Hash = HashPoint( Position, Octant );

			for( auto Iter = HashTable.CreateKeyIterator( Hash ); Iter; ++Iter )
			{
				if( ShouldWeld( Vert, WeldedVerts[ Iter.Value() ] ) )
				{
					Index = Iter.Value();
					break;
				}
			}
			if( Index != ~0u )
			{
				break;
			}
		}
		if( Index == ~0u )
		{
			Index = WeldedVerts.Add( Vert );
			WeldedIndexes.Add( Index );
			HashTable.Add( HashPoint( Position ), Index );
		}
	}

	Swap( Verts,	WeldedVerts );
	Swap( Indexes,	WeldedIndexes );
}