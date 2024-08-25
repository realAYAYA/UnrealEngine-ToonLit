// Copyright Epic Games, Inc. All Rights Reserved.

#include "TessellationTable.h"
#include "NaniteDefinitions.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

#define NANITE_BUILD_TESSELLATION_TABLE 0

#if NANITE_BUILD_TESSELLATION_TABLE
#include "DynamicMesh/DynamicMesh3.h"
#include "TriangleUtil.h"
#include "Misc/OutputDeviceRedirector.h"

#ifdef _MSC_VER
#pragma warning(disable : 6385)
#endif

using namespace UE::Geometry;
#endif

namespace Nanite
{

FTessellationTable::FTessellationTable()
{
	/*
		NumPatterns = (MaxTessFactor + 2) choose 3
		NumPatterns = 1/6 * N(N+1)(N+2)
		= 816
	*/

	FString FilePath = FPaths::EngineContentDir() / TEXT("Renderer/TessellationTable.bin");

#if NANITE_BUILD_TESSELLATION_TABLE
	uint32 Time0 = FPlatformTime::Cycles();

	OffsetTable.AddZeroed( 2 * NANITE_TESSELLATION_TABLE_PO2_SIZE * NANITE_TESSELLATION_TABLE_PO2_SIZE * NANITE_TESSELLATION_TABLE_PO2_SIZE );

	// TessFactors in descending order to reduce size of table.
	
	// Regular tessellation table
	for( uint32 TessFactorZ = 1; TessFactorZ <= NANITE_TESSELLATION_TABLE_SIZE; TessFactorZ++ )
	{
		for( uint32 TessFactorY = TessFactorZ; TessFactorY <= NANITE_TESSELLATION_TABLE_SIZE; TessFactorY++ )
		{
			for( uint32 TessFactorX = TessFactorY; TessFactorX <= NANITE_TESSELLATION_TABLE_SIZE; TessFactorX++ )
			{
				FIntVector TessFactors( TessFactorX, TessFactorY, TessFactorZ );

				AddPatch( TessFactors );
				//ConstrainToCacheWindow();
				WriteSVG( TessFactors );
				
				AddToVertsAndIndices( false, TessFactors );
			}
		}
	}

	// Immediate-mode tessellation table
	for( uint32 TessFactorZ = 1; TessFactorZ <= NANITE_TESSELLATION_TABLE_IMMEDIATE_SIZE; TessFactorZ++ )
	{
		for( uint32 TessFactorY = TessFactorZ; TessFactorY <= NANITE_TESSELLATION_TABLE_IMMEDIATE_SIZE; TessFactorY++ )
		{
			for( uint32 TessFactorX = TessFactorY; TessFactorX <= NANITE_TESSELLATION_TABLE_IMMEDIATE_SIZE; TessFactorX++ )
			{
				FIntVector TessFactors( TessFactorX, TessFactorY, TessFactorZ );

				// TODO: Reuse already generated data instead of generating it again?
				AddPatch( TessFactors );
				WriteSVG( TessFactors );

				const uint32 NumTrisBefore	= Indexes.Num();
				
				ConstrainToCacheWindow();
				ConstrainForImmediateTessellation();

				const uint32 NumTrisAfter = Indexes.Num();
				check( NumTrisAfter <= NumTrisBefore + 2 );	// Two degenerate triangles needed for first triangle, so this is optimal
				
				AddToVertsAndIndices( true, TessFactors );
			}
		}
	}

	uint32 Time1 = FPlatformTime::Cycles();

	GLog->Logf( TEXT("TessellationTable [%.2fms]"), FPlatformTime::ToMilliseconds( Time1 - Time0 ) );

	TUniquePtr< FArchive > Ar( IFileManager::Get().CreateFileWriter( *FilePath ) );
#else
	TUniquePtr< FArchive > Ar( IFileManager::Get().CreateFileReader( *FilePath ) );
#endif

	*Ar << OffsetTable;
	*Ar << VertsAndIndexes;
}

int32 FTessellationTable::GetPattern( FIntVector TessFactors ) const
{
	checkSlow( 0 < TessFactors[0] && TessFactors[0] <= int32(NANITE_TESSELLATION_TABLE_SIZE) );
	checkSlow( 0 < TessFactors[1] && TessFactors[1] <= int32(NANITE_TESSELLATION_TABLE_SIZE) );
	checkSlow( 0 < TessFactors[2] && TessFactors[2] <= int32(NANITE_TESSELLATION_TABLE_SIZE) );

	if( TessFactors[0] < TessFactors[1] ) Swap( TessFactors[0], TessFactors[1] );
	if( TessFactors[0] < TessFactors[2] ) Swap( TessFactors[0], TessFactors[2] );
	if( TessFactors[1] < TessFactors[2] ) Swap( TessFactors[1], TessFactors[2] );

	return
		( TessFactors[0] - 1 ) +
		( TessFactors[1] - 1 ) * NANITE_TESSELLATION_TABLE_PO2_SIZE +
		( TessFactors[2] - 1 ) * NANITE_TESSELLATION_TABLE_PO2_SIZE * NANITE_TESSELLATION_TABLE_PO2_SIZE;
}

FIntVector FTessellationTable::GetBarycentrics( uint32 Vert ) const
{
	FIntVector Barycentrics;
	Barycentrics.X = Vert & 0xffff;
	Barycentrics.Y = Vert >> 16;
	Barycentrics.Z = BarycentricMax - Barycentrics.X - Barycentrics.Y;
	return Barycentrics;
}

#if NANITE_BUILD_TESSELLATION_TABLE

void Cache()
{
#if 0
	using namespace UE::DerivedData;

	FString DerivedDataKey( TEXT("FTessellationTable-0") );

	FCacheKey CacheKey;
	CacheKey.Bucket = FCacheBucket( TEXT("FTessellationTable") );
	CacheKey.Hash = FIoHash::HashBuffer( MakeMemoryView( FTCHARToUTF8( DerivedDataKey ) ) );

	// Check if the data already exists in DDC
	FSharedBuffer Data;
	{
		FCacheGetValueRequest Request;
		Request.Name = FString("Get-FTessellationTable");
		Request.Key = CacheKey;
		Request.Policy = ECachePolicy::Local;
	
		FRequestOwner RequestOwner( EPriority::Blocking );
		GetCache().GetValue( MakeArrayView( &Request, 1 ), RequestOwner,
			[&]( FCacheGetValueResponse&& Response )
			{
				if( Response.Status == EStatus::Ok )
				{
					Data = Response.Value.GetData().Decompress();
				}
			} );
		RequestOwner.Wait();
	}

	if( !Data.IsNull() )
	{
		FMemoryReaderView Ar( Data.GetView(), /*bIsPersistent=*/ true );
		Serialize( Ar, Owner, /*bCooked=*/ false );
	}
	else
	{
		// DDC lookup failed! Build the data again.
		const bool bBuiltSuccessfully = Build(Owner, SourceData);

		FMemoryWriter Ar( 0, /*bIsPersistent=*/ true );
		Serialize( Ar, Owner, /*bCooked=*/ false );

		FCachePutValueRequest Request;
		Request.Name = FString("Put-FTessellationTable");
		Request.Key = CacheKey;
		Request.Value = FValue::Compress( FSharedBuffer::MakeView( Ar.GetData(), Ar.TotalSize() ) );
		Request.Policy = ECachePolicy::Local;
	
		FRequestOwner RequestOwner( EPriority::Blocking );
		GetCache().PutValue( MakeArrayView( &Request, 1 ), RequestOwner );
		RequestOwner.Wait();
	}
#endif
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

class FTessellatedPatch
{
public:
	FIntVector		TessFactors;
	FDynamicMesh3	Mesh;

public:
				FTessellatedPatch( const FIntVector& InTessFactors );

private:
	void		Uniform();
	void		Remesh();
	void		ProcessEdge( int EdgeIndex );

	TArray< FVector3d >	Relaxed;
};

FTessellatedPatch::FTessellatedPatch( const FIntVector& InTessFactors )
	: TessFactors( InTessFactors )
{
	if( TessFactors[0] == TessFactors[2] )
		Uniform();
	else
		Remesh();
}

void FTessellatedPatch::Uniform()
{
	const uint32 NumVerts = ( ( TessFactors[0] + 1 ) * ( TessFactors[0] + 2 ) ) / 2;
	const uint32 NumTris = TessFactors[0] * TessFactors[0];

	/*
		Starts from top point. Adds rows of verts and corresponding rows of tri strips.

		|\
	row |\|\
		|\|\|\
		column
	*/
	for( uint32 VertIndex = 0; VertIndex < NumVerts; VertIndex++ )
	{
		// Find largest tessellation with NumVerts <= VertIndex. These are the preceding verts before this row.
		uint32 VertRow = FMath::FloorToInt( FMath::Sqrt( VertIndex * 8.0f + 1.0f ) * 0.5f - 0.5f );
		uint32 VertCol = VertIndex - ( VertRow * ( VertRow + 1 ) ) / 2;

		FVector3d Barycentrics;
		Barycentrics[0] = TessFactors[0] - VertRow;
		Barycentrics[1] = VertCol;
		Barycentrics[2] = VertRow - VertCol;
		Barycentrics /= TessFactors[0];

		Mesh.AppendVertex( Barycentrics );
	}

	for( uint32 TriIndex = 0; TriIndex < NumTris; TriIndex++ )
	{
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

		FIndex3i Triangle;
		for( int Corner = 0; Corner < 3; Corner++ )
		{
			uint32 Row = VertRowCol[ Corner ][0];
			uint32 Col = VertRowCol[ Corner ][1];

			Triangle[ Corner ] = Col + ( Row * ( Row + 1 ) ) / 2;
		}
		Mesh.AppendTriangle( Triangle );
	}
}

void FTessellatedPatch::Remesh()
{
	FIndex3i Triangle;
	Triangle[0] = Mesh.AppendVertex( FVector3d( 1, 0, 0 ) );
	Triangle[1] = Mesh.AppendVertex( FVector3d( 0, 1, 0 ) );
	Triangle[2] = Mesh.AppendVertex( FVector3d( 0, 0, 1 ) );

	Mesh.AppendTriangle( Triangle );

	const int RemeshIterations = 64;
	for (int k = 0; k < RemeshIterations; ++k)
	{
		{
			int MaxEdgeID = Mesh.MaxEdgeID();
			int StartIndex = k % MaxEdgeID;
			int EdgeIndex = StartIndex;
			do
			{
				if( Mesh.IsEdge( EdgeIndex ) )
					ProcessEdge( EdgeIndex );
	
				// Iterate in random order
				const int ModuloPrime = 31337;
				EdgeIndex = ( EdgeIndex + ModuloPrime ) % MaxEdgeID;
			} while( EdgeIndex != StartIndex );
		}

		Relaxed.SetNum( Mesh.MaxVertexID(), EAllowShrinking::No );

		for( int VertexIndex : Mesh.VertexIndicesItr() )
		{
			FVector3d v = Mesh.GetVertex( VertexIndex );
	
			if( !Mesh.IsBoundaryVertex( VertexIndex ) )
			{
				Mesh.GetVtxOneRingCentroid( VertexIndex, v );
			}

			Relaxed[ VertexIndex ] = v;
		}

		for( int VertexIndex : Mesh.VertexIndicesItr() )
		{
			Mesh.SetVertex( VertexIndex, Relaxed[ VertexIndex ] );
		}
	}
}

void FTessellatedPatch::ProcessEdge( int EdgeIndex )
{
	// MinEdgeLength < MaxEdgeLength/2
	const float MinEdgeLength = 0.66f;
	const float MaxEdgeLength = 1.33f;

	FDynamicMesh3::FEdge Edge = Mesh.GetEdge( EdgeIndex );

	FVector3d v0 = Mesh.GetVertex( Edge.Vert[0] );
	FVector3d v1 = Mesh.GetVertex( Edge.Vert[1] );

	float EdgeLengthSqr = Barycentric::LengthSquared( FVector3f(v0), FVector3f(v1), FVector3f( TessFactors * TessFactors ) );

	bool bBoundary0 = Mesh.IsBoundaryVertex( Edge.Vert[0] );
	bool bBoundary1 = Mesh.IsBoundaryVertex( Edge.Vert[1] );
	bool bBoundaryEdge = Mesh.IsBoundaryEdge( EdgeIndex );

	if( ( bBoundaryEdge || !bBoundary0 || !bBoundary1 ) && EdgeLengthSqr < MinEdgeLength * MinEdgeLength )
	{
		int iKeep	= Edge.Vert[0];
		int iRemove	= Edge.Vert[1];
		double collapse_t = 0.5;
	
		if( bBoundary0 )
		{
			iKeep	= Edge.Vert[0];
			iRemove	= Edge.Vert[1];
			collapse_t = 0;
		}
		if( bBoundary1 )
		{
			iKeep	= Edge.Vert[1];
			iRemove	= Edge.Vert[0];
			collapse_t = 0;
		}
	
		FDynamicMesh3::FEdgeCollapseInfo CollapseInfo;
		EMeshResult Result = Mesh.CollapseEdge( iKeep, iRemove, collapse_t, CollapseInfo );
		if( Result == EMeshResult::Ok )
			return;
	}

	if( !bBoundaryEdge ) 
	{
		FIndex2i Opposing = Mesh.GetEdgeOpposingV( EdgeIndex );
		FVector3d o0 = Mesh.GetVertex( Opposing[0] );
		FVector3d o1 = Mesh.GetVertex( Opposing[1] );

		float FlipDistSqr = Barycentric::LengthSquared( FVector3f(o0), FVector3f(o1), FVector3f( TessFactors * TessFactors ) );
		bool bFlipEdge = FlipDistSqr < 0.9f * EdgeLengthSqr;

		if( bFlipEdge )
		{
			FDynamicMesh3::FEdgeFlipInfo FlipInfo;
			EMeshResult Result = Mesh.FlipEdge( EdgeIndex, FlipInfo );
			if( Result == EMeshResult::Ok )
				return;
		}
	}

	if( EdgeLengthSqr > MaxEdgeLength * MaxEdgeLength )
	{
		float SplitT = 0.5f;
		if( bBoundaryEdge )
		{
			float EdgeLength = FMath::Sqrt( EdgeLengthSqr );
			float NumEdgeSplits = FMath::Floor( EdgeLength + 0.5f );
			float HalfSplit = FMath::Floor( NumEdgeSplits / 2.0f );
			SplitT = HalfSplit / NumEdgeSplits;
		}

		FDynamicMesh3::FEdgeSplitInfo SplitInfo;
		EMeshResult Result = Mesh.SplitEdge( EdgeIndex, SplitInfo, SplitT );
		if( Result == EMeshResult::Ok )
			return;
	}
}

void FTessellationTable::AddPatch( const FIntVector& TessFactors )
{
	FTessellatedPatch Patch( TessFactors );

	Patch.Mesh.CompactInPlace();

	for( int VertexIndex : Patch.Mesh.VertexIndicesItr() )
	{
		FVector3d v = Patch.Mesh.GetVertex( VertexIndex );

		FIntVector Barycentrics;
		Barycentrics.X = FMath::RoundToInt( FMath::Clamp( v.X, 0.0f, 1.0f ) * BarycentricMax );
		Barycentrics.Y = FMath::RoundToInt( FMath::Clamp( v.Y, 0.0f, 1.0f ) * BarycentricMax );
		Barycentrics.Z = FMath::RoundToInt( FMath::Clamp( v.Z, 0.0f, 1.0f ) * BarycentricMax );

		{
			const uint32 e0 = FMath::Max3Index( Barycentrics[0], Barycentrics[1], Barycentrics[2] );
			const uint32 e1 = (1 << e0) & 3;
			const uint32 e2 = (1 << e1) & 3;

			Barycentrics[ e0 ] = BarycentricMax - Barycentrics[ e1 ] - Barycentrics[ e2 ];
		}

		SnapAtEdges( Barycentrics, TessFactors );

		Verts.Add( Barycentrics[0] | ( Barycentrics[1] << 16 ) );
	}

	for( int TriangleID : Patch.Mesh.TriangleIndicesItr() )
	{
		FIndex3i Triangle = Patch.Mesh.GetTriangle( TriangleID );

		Indexes.Add( Triangle[0] | ( Triangle[1] << 10 ) | ( Triangle[2] << 20 ) );
	}
}


#define CACHE_WINDOW_SIZE	32

// Weights for individual cache entries based on simulated annealing optimization on DemoLevel.
static int16 CacheWeightTable[ CACHE_WINDOW_SIZE ] = {
	 577,	 616,	 641,  512,		 614,  635,  478,  651,
	  65,	 213,	 719,  490,		 213,  726,  863,  745,
	 172,	 939,	 805,  885,		 958, 1208, 1319, 1318,
	1475,	1779,	2342,  159,		2307, 1998, 1211,  932
};

// Constrain index buffer to only use vertex references that are within a fixed sized trailing window from the current highest encountered vertex index.
// Triangles are reordered based on a FIFO-style cache optimization to minimize the number of vertices that need to be duplicated.
void FTessellationTable::ConstrainToCacheWindow()
{
	uint32 NumOldVertices = Verts.Num();
	uint32 NumOldTriangles = Indexes.Num();

	check( NANITE_TESSELLATION_TABLE_SIZE <= 16 );
	constexpr uint32 MaxNumTris = 16 * 16;
	constexpr uint32 MaxTrianglesInDwords = ( MaxNumTris + 31 ) / 32;

	uint32 VertexToTriangleMasks[ MaxNumTris * 3 ][ MaxTrianglesInDwords ] = {};

	// Generate vertex to triangle masks
	for( uint32 i = 0; i < NumOldTriangles; i++ )
	{
		const uint32 i0 = ( Indexes[i] >>  0 ) & 1023;
		const uint32 i1 = ( Indexes[i] >> 10 ) & 1023;
		const uint32 i2 = ( Indexes[i] >> 20 ) & 1023;
		check( i0 != i1 && i1 != i2 && i2 != i0 ); // Degenerate input triangle!
		check( i0 < NumOldVertices && i1 < NumOldVertices && i2 < NumOldVertices );

		VertexToTriangleMasks[ i0 ][ i >> 5 ] |= 1 << ( i & 31 );
		VertexToTriangleMasks[ i1 ][ i >> 5 ] |= 1 << ( i & 31 );
		VertexToTriangleMasks[ i2 ][ i >> 5 ] |= 1 << ( i & 31 );
	}

	uint32 TrianglesEnabled[ MaxTrianglesInDwords ] = {};	// Enabled triangles are in the current material range and have not yet been visited.
	uint32 TrianglesTouched[ MaxTrianglesInDwords ] = {};	// Touched triangles have had at least one of their vertices visited.

	uint32 NumNewVertices = 0;
	uint32 NumNewTriangles = 0;
	uint16 OldToNewVertex[ MaxNumTris * 3 ];

	uint32 NewVerts[ MaxNumTris * 3 ] = {};	// Initialize to make static analysis happy
	uint32 NewIndexes[ MaxNumTris ];
	
	FMemory::Memset( OldToNewVertex, -1, sizeof( OldToNewVertex ) );

	uint32 DwordEnd	= NumOldTriangles / 32;
	uint32 BitEnd	= NumOldTriangles & 31;

	FMemory::Memset( TrianglesEnabled, -1, DwordEnd * sizeof( uint32 ) );

	if( BitEnd != 0 )
		TrianglesEnabled[ DwordEnd ] = ( 1u << BitEnd ) - 1u;

	auto ScoreVertex = [ &OldToNewVertex, &NumNewVertices ] ( uint32 OldVertex )
	{
		uint16 NewIndex = OldToNewVertex[ OldVertex ];

		int32 CacheScore = 0;
		if( NewIndex != 0xFFFF )
		{
			uint32 CachePosition = ( NumNewVertices - 1 ) - NewIndex;
			if( CachePosition < CACHE_WINDOW_SIZE )
				CacheScore = CacheWeightTable[ CachePosition ];
		}

		return CacheScore;
	};

	while( true )
	{
		uint32 NextTriangleIndex = 0xFFFF;
		int32  NextTriangleScore = 0;

		// Pick highest scoring available triangle
		for( uint32 TriangleDwordIndex = 0; TriangleDwordIndex < MaxTrianglesInDwords; TriangleDwordIndex++ )
		{
			uint32 CandidateMask = TrianglesTouched[ TriangleDwordIndex ] & TrianglesEnabled[ TriangleDwordIndex ];
			while( CandidateMask )
			{
				uint32 TriangleDwordOffset = FMath::CountTrailingZeros( CandidateMask );
				CandidateMask &= CandidateMask - 1;

				int32 TriangleIndex = ( TriangleDwordIndex << 5 ) + TriangleDwordOffset;

				int32 TriangleScore = 0;
				TriangleScore += ScoreVertex( ( Indexes[ TriangleIndex ] >>  0 ) & 1023 );
				TriangleScore += ScoreVertex( ( Indexes[ TriangleIndex ] >> 10 ) & 1023 );
				TriangleScore += ScoreVertex( ( Indexes[ TriangleIndex ] >> 20 ) & 1023 );

				if( TriangleScore > NextTriangleScore )
				{
					NextTriangleIndex = TriangleIndex;
					NextTriangleScore = TriangleScore;
				}
			}
		}

		if( NextTriangleIndex == 0xFFFF )
		{
			// If we didn't find a triangle. It might be because it is part of a separate component. Look for an unvisited triangle to restart from.
			for( uint32 TriangleDwordIndex = 0; TriangleDwordIndex < MaxTrianglesInDwords; TriangleDwordIndex++ )
			{
				uint32 EnableMask = TrianglesEnabled[ TriangleDwordIndex ];
				if( EnableMask )
				{
					NextTriangleIndex = ( TriangleDwordIndex << 5 ) + FMath::CountTrailingZeros( EnableMask );
					break;
				}
			}

			if( NextTriangleIndex == 0xFFFF )
				break;
		}

		uint32 OldIndex[3];
		OldIndex[0] = ( Indexes[ NextTriangleIndex ] >>  0 ) & 1023;
		OldIndex[1] = ( Indexes[ NextTriangleIndex ] >> 10 ) & 1023;
		OldIndex[2] = ( Indexes[ NextTriangleIndex ] >> 20 ) & 1023;

		// Mark incident triangles
		for( uint32 i = 0; i < MaxTrianglesInDwords; i++ )
		{
			TrianglesTouched[i] |= VertexToTriangleMasks[ OldIndex[0] ][i];
			TrianglesTouched[i] |= VertexToTriangleMasks[ OldIndex[1] ][i];
			TrianglesTouched[i] |= VertexToTriangleMasks[ OldIndex[2] ][i];
		}

		uint32 NewIndex[3];
		NewIndex[0] = OldToNewVertex[ OldIndex[0] ];
		NewIndex[1] = OldToNewVertex[ OldIndex[1] ];
		NewIndex[2] = OldToNewVertex[ OldIndex[2] ];

		uint32 NumNew = (NewIndex[0] == 0xFFFF) + (NewIndex[1] == 0xFFFF) + (NewIndex[2] == 0xFFFF);

		// Generate new indices such that they are all within a trailing window of CACHE_WINDOW_SIZE of NumNewVertices.
		// This can require multiple iterations as new/duplicate vertices can push other vertices outside the window.			
		uint32 TestNumNewVertices = NumNewVertices;
		TestNumNewVertices += NumNew;

		while(true)
		{
			if (NewIndex[0] != 0xFFFF && TestNumNewVertices - NewIndex[0] >= CACHE_WINDOW_SIZE)
			{
				NewIndex[0] = 0xFFFF;
				TestNumNewVertices++;
				continue;
			}

			if (NewIndex[1] != 0xFFFF && TestNumNewVertices - NewIndex[1] >= CACHE_WINDOW_SIZE)
			{
				NewIndex[1] = 0xFFFF;
				TestNumNewVertices++;
				continue;
			}

			if (NewIndex[2] != 0xFFFF && TestNumNewVertices - NewIndex[2] >= CACHE_WINDOW_SIZE)
			{
				NewIndex[2] = 0xFFFF;
				TestNumNewVertices++;
				continue;
			}
			break;
		}

		for( int k = 0; k < 3; k++ )
		{
			if( NewIndex[k] == 0xFFFF)
				NewIndex[k] = NumNewVertices++;

			OldToNewVertex[ OldIndex[k] ] = (uint16)NewIndex[k];

			NewVerts[ NewIndex[k] ] = Verts[ OldIndex[k] ];
		}

		// Rotate triangle such that 1st index is smallest
		const uint32 i0 = FMath::Min3Index( NewIndex[0], NewIndex[1], NewIndex[2] );
		const uint32 i1 = (1 << i0) & 3;
		const uint32 i2 = (1 << i1) & 3;

		// Output triangle
		NewIndexes[ NumNewTriangles++ ] = NewIndex[ i0 ] | ( NewIndex[ i1 ] << 10 ) | ( NewIndex[ i2 ] << 20 );

		// Disable selected triangle
		TrianglesEnabled[ NextTriangleIndex >> 5 ] &= ~( 1 << ( NextTriangleIndex & 31 ) );
	}


	check( NumNewTriangles == NumOldTriangles );

	if( NumNewVertices > NumOldVertices )
		Verts.AddUninitialized( NumNewVertices - NumOldVertices );
	check(NumNewVertices == NumOldVertices);

	// Write back new triangle order
	FMemory::Memcpy( Verts.GetData(),	NewVerts,	NumNewVertices * sizeof( uint32 ) );
	FMemory::Memcpy( Indexes.GetData(),	NewIndexes,	NumNewTriangles * sizeof( uint32 ) );
}

void FTessellationTable::ConstrainForImmediateTessellation()
{
	// Constrain such that the tessellation pattern has the same number of triangles and vertices.
	// Triangles can only references vertices with an index lower or equal to the current triangle index.
	// Vertex references can reference at most 32 back from the triangle index.
	// Each triangle adds one new vertex and has two vertex references.
	// The new vertex is always the first of the 3 indices.

	const uint32 NumOldVerts = Verts.Num();
	const uint32 NumOldTris = Indexes.Num();
	
	const uint32 InvalidVert = 0xFFFFu;

	TArray<uint16> OldToNewVertex;
	OldToNewVertex.Init( InvalidVert, NumOldVerts );
	
	TArray<uint32> NewVerts;
	TArray<uint32> NewTris;
	for (uint32 OldTriIndex = 0; OldTriIndex < NumOldTris; OldTriIndex++)
	{
		const uint32 IndexData = Indexes[OldTriIndex];
		const uint32 Index0 = IndexData & 0x3FFu;
		const uint32 Index1 = (IndexData >> 10) & 0x3FFu;
		const uint32 Index2 = IndexData >> 20;

		uint32 NumAddedVerts = 0;
		while (true)
		{
			if (OldToNewVertex[Index0] == InvalidVert || NewVerts.Num() - OldToNewVertex[Index0] > 32)
			{
				OldToNewVertex[Index0] = NewVerts.Num();
				NewVerts.Add(Verts[Index0]);
				NumAddedVerts++;
				continue;
			}

			if (OldToNewVertex[Index1] == InvalidVert || NewVerts.Num() - OldToNewVertex[Index1] > 32)
			{
				OldToNewVertex[Index1] = NewVerts.Num();
				NewVerts.Add(Verts[Index1]);
				NumAddedVerts++;
				continue;
			}

			if (OldToNewVertex[Index2] == InvalidVert || NewVerts.Num() - OldToNewVertex[Index2] > 32)
			{
				OldToNewVertex[Index2] = NewVerts.Num();
				NewVerts.Add(Verts[Index2]);
				NumAddedVerts++;
				continue;
			}

			if (NumAddedVerts == 0)
			{
				// No new vertices needed.
				// Arbitrarily duplicate the Index0 vertex.
				const uint32 Data = NewVerts[OldToNewVertex[Index0]];
				OldToNewVertex[Index0] = NewVerts.Num();
				NewVerts.Add(Data);
				NumAddedVerts++;
				continue;
			}

			break;
		}

		// Add any degenerate triangles
		for (uint32 i = 0; i + 1 < NumAddedVerts; i++)
		{
			const uint32 Index = NewVerts.Num() - NumAddedVerts;
			NewTris.Add( (Index << 20) | (Index << 10) | Index );
		}
		check(NumAddedVerts != 0);

		// Add triangle
		{
			uint32 I0 = OldToNewVertex[Index0];
			uint32 I1 = OldToNewVertex[Index1];
			uint32 I2 = OldToNewVertex[Index2];

			// Rotate such that first index is the highest one
			while (I1 > I0 || I2 > I0)
			{
				uint32 Tmp = I0;
				I0 = I1; I1 = I2; I2 = Tmp;
			}
			check(I0 == NewTris.Num());
			NewTris.Add( (I2 << 20) | (I1 << 10) | I0 );
		}
		check(NewVerts.Num() == NewTris.Num());
	}

	check((uint32)NewVerts.Num() >= NumOldVerts);
	check((uint32)NewTris.Num() >= NumOldTris);

	if ((uint32)NewVerts.Num() > NumOldVerts)
		Verts.AddUninitialized(NewVerts.Num() - NumOldVerts);

	if ((uint32)NewTris.Num() > NumOldTris)
		Indexes.AddUninitialized(NewTris.Num() - NumOldTris);

	// Write back new triangle order
	FMemory::Memcpy( Verts.GetData(),	NewVerts.GetData(), NewVerts.Num() * sizeof(uint32));
	FMemory::Memcpy( Indexes.GetData(),	NewTris.GetData(),	NewTris.Num() * sizeof(uint32));
}

void FTessellationTable::AddToVertsAndIndices( bool bImmediate, const FIntVector& TessFactors )
{
	uint32 Index =	(TessFactors.Z - 1u) * NANITE_TESSELLATION_TABLE_PO2_SIZE * NANITE_TESSELLATION_TABLE_PO2_SIZE +
					(TessFactors.Y - 1u) * NANITE_TESSELLATION_TABLE_PO2_SIZE +
					(TessFactors.X - 1u);
	
	if (bImmediate) Index += NANITE_TESSELLATION_TABLE_PO2_SIZE * NANITE_TESSELLATION_TABLE_PO2_SIZE * NANITE_TESSELLATION_TABLE_PO2_SIZE;

	const uint32 NumVerts = Verts.Num();
	const uint32 NumTris = Indexes.Num();

	check(NumVerts < 512);
	check(NumTris < 1024);

	OffsetTable[Index].X = VertsAndIndexes.Num();
	OffsetTable[Index].Y = /* Pattern | */ (NumVerts << 13) | (NumTris << 22);

	VertsAndIndexes.Append( Verts );
	VertsAndIndexes.Append( Indexes );

	Verts.Reset();
	Indexes.Reset();
}

void FTessellationTable::WriteSVG( const FIntVector& TessFactors )
{
#if 0
	const uint32 NumVerts = Verts.Num();
	const uint32 NumTris = Indexes.Num();

	char Filename[128];
	sprintf(Filename, "d:\\tessellation_pattern\\%d_%d_%d.svg", TessFactors.X, TessFactors.Y, TessFactors.Z);
	FILE* File = fopen(Filename, "wb");
	fputs(R"xyz(<?xml version="1.0" encoding="UTF-8" standalone="no"?>
<!DOCTYPE svg PUBLIC "-//W3C//DTD SVG 1.1//EN" "http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd">
<svg viewBox="0 0 1024 1024" xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink">
<rect fill="#fff" stroke="#000" x="0" y="0" width="1024" height="1024"/>
<g opacity="0.8">
)xyz", File);

	/*
		Derive cartesian coordinates for patch corners using TessFactors as edge lengths.

		v0 = (0,0)
		v1 = (0,e0)
		v2 = (x,y)

		e2^2 = x^2 + y^2
		e1^2 = x^2 + (e0-y)^2

		e1^2 = e2^2 - y^2 + (e0-y)^2
		e1^2 = e0^2 - 2*e0 * y + e2^2
		y = ( e0^2 + e2^2 - e1^2 ) / ( 2*e0 )
		x = sqrt( e2^2 - y^2 )
	*/
	FVector2f PatchCorner0( 0, 0 );
	FVector2f PatchCorner1( 0, TessFactors[0] );
	FVector2f PatchCorner2;
	PatchCorner2.Y = float( TessFactors[0] * TessFactors[0] + TessFactors[2] * TessFactors[2] - TessFactors[1] * TessFactors[1] ) / ( 2 * TessFactors[0] );
	// Limit impossible triangles which result in NaNs
	if( PatchCorner2.Y < (float)TessFactors[2] )
		PatchCorner2.X = FMath::Sqrt( TessFactors[2] * TessFactors[2] - PatchCorner2.Y * PatchCorner2.Y );
	else
		PatchCorner2.X = 1.0f;

	PatchCorner0 *= 1023.0f / ( TessFactors[0] * BarycentricMax );
	PatchCorner1 *= 1023.0f / ( TessFactors[0] * BarycentricMax );
	PatchCorner2 *= 1023.0f / ( TessFactors[0] * BarycentricMax );

	for( uint32 TriIndex = 0; TriIndex < NumTris; TriIndex++ )
	{
		const uint32 VertIndex0 = ( Indexes[ TriIndex ] >>  0 ) & 1023;
		const uint32 VertIndex1 = ( Indexes[ TriIndex ] >> 10 ) & 1023;
		const uint32 VertIndex2 = ( Indexes[ TriIndex ] >> 20 ) & 1023;

		const FIntVector Barycentrics0 = GetBarycentrics( Verts[ VertIndex0 ] );
		const FIntVector Barycentrics1 = GetBarycentrics( Verts[ VertIndex1 ] );
		const FIntVector Barycentrics2 = GetBarycentrics( Verts[ VertIndex2 ] );

		const FVector2f TriCorner0 = PatchCorner0 * Barycentrics0.X + PatchCorner1 * Barycentrics0.Y + PatchCorner2 * Barycentrics0.Z;
		const FVector2f TriCorner1 = PatchCorner0 * Barycentrics1.X + PatchCorner1 * Barycentrics1.Y + PatchCorner2 * Barycentrics1.Z;
		const FVector2f TriCorner2 = PatchCorner0 * Barycentrics2.X + PatchCorner1 * Barycentrics2.Y + PatchCorner2 * Barycentrics2.Z;

		fprintf(File, "\t<polyline points = \"%d,%d %d,%d %d,%d %d,%d\" stroke = \"black\" stroke-width = \"4\" fill = \"none\" />\n",
			int(TriCorner0.X), int(TriCorner0.Y),
			int(TriCorner1.X), int(TriCorner1.Y),
			int(TriCorner2.X), int(TriCorner2.Y),
			int(TriCorner0.X), int(TriCorner0.Y));
	}

	fputs("(</g></svg>", File);

	fclose(File);
#endif
}

#endif // NANITE_BUILD_TESSELLATION_TABLE

} // namespace Nanite
