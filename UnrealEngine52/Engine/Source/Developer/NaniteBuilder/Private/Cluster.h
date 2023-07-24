// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StaticMeshResources.h"
#include "Rendering/NaniteResources.h"
#include "Math/Bounds.h"
#include "MeshSimplify.h"
#include "TriangleUtil.h"

class FGraphPartitioner;

namespace Nanite
{

struct FMaterialTriangle
{
	uint32 Index0;
	uint32 Index1;
	uint32 Index2;
	uint32 MaterialIndex;
	uint32 RangeCount;
};

struct FMaterialRange
{
	uint32 RangeStart;
	uint32 RangeLength;
	uint32 MaterialIndex;
	TArray<uint8, TInlineAllocator<12>> BatchTriCounts;

	friend FArchive& operator<<(FArchive& Ar, FMaterialRange& Range);
};

struct FStripDesc
{
	uint32 Bitmasks[4][3];
	uint32 NumPrevRefVerticesBeforeDwords;
	uint32 NumPrevNewVerticesBeforeDwords;

	friend FArchive& operator<<(FArchive& Ar, FStripDesc& Desc);
};


class FCluster
{
public:
	FCluster() {}
	FCluster(
		const TArray< FStaticMeshBuildVertex >& InVerts,
		const TArrayView< const uint32 >& InIndexes,
		const TArrayView< const int32 >& InMaterialIndexes,
		uint32 InNumTexCoords, bool bInHasColors, bool bInPreserveArea,
		uint32 TriBegin, uint32 TriEnd, const FGraphPartitioner& Partitioner, const FAdjacency& Adjacency );

	FCluster( FCluster& SrcCluster, uint32 TriBegin, uint32 TriEnd, const FGraphPartitioner& Partitioner, const FAdjacency& Adjacency );
	FCluster( const TArray< const FCluster*, TInlineAllocator<32> >& MergeList );

	float		Simplify( uint32 TargetNumTris, float TargetError = 0.0f, uint32 LimitNumTris = 0, bool bForNaniteFallback = false );
	FAdjacency	BuildAdjacency() const;
	void		Split( FGraphPartitioner& Partitioner, const FAdjacency& Adjacency ) const;
	void		Bound();

private:
	uint32		AddVert( const float* Vert, FHashTable& HashTable );

public:
	uint32				GetVertSize() const;
	FVector3f&			GetPosition( uint32 VertIndex );
	float*				GetAttributes( uint32 VertIndex );
	FVector3f&			GetNormal( uint32 VertIndex );
	FLinearColor&		GetColor( uint32 VertIndex );
	FVector2f*			GetUVs( uint32 VertIndex );

	const FVector3f&	GetPosition( uint32 VertIndex ) const;
	const FVector3f&	GetNormal( uint32 VertIndex ) const;
	const FLinearColor&	GetColor( uint32 VertIndex ) const;
	const FVector2f*	GetUVs( uint32 VertIndex ) const;

	void				SanitizeVertexData();

	friend FArchive& operator<<(FArchive& Ar, FCluster& Cluster);

	static const uint32	ClusterSize = 128;

	uint32		NumVerts = 0;
	uint32		NumTris = 0;
	uint32		NumTexCoords = 0;
	bool		bHasColors = false;
	bool		bPreserveArea = false;

	TArray< float >		Verts;
	TArray< uint32 >	Indexes;
	TArray< int32 >		MaterialIndexes;
	TArray< int8 >		ExternalEdges;
	uint32				NumExternalEdges;

	TMap< uint32, uint32 >	AdjacentClusters;

	FBounds3f	Bounds;
	uint64		GUID = 0;
	int32		MipLevel = 0;

	FIntVector	QuantizedPosStart		= { 0u, 0u, 0u };
	int32		QuantizedPosPrecision	= 0u;
	FIntVector  QuantizedPosBits		= { 0u, 0u, 0u };

	float		EdgeLength = 0.0f;
	float		LODError = 0.0f;
	float		SurfaceArea = 0.0f;
	
	FSphere3f	SphereBounds;
	FSphere3f	LODBounds;

	uint32		GroupIndex			= MAX_uint32;
	uint32		GroupPartIndex		= MAX_uint32;
	uint32		GeneratingGroupIndex= MAX_uint32;

	TArray<FMaterialRange, TInlineAllocator<4>> MaterialRanges;
	TArray<FIntVector>	QuantizedPositions;

	FStripDesc		StripDesc;
	TArray<uint8>	StripIndexData;
};

FORCEINLINE uint32 FCluster::GetVertSize() const
{
	return 6 + ( bHasColors ? 4 : 0 ) + NumTexCoords * 2;
}

FORCEINLINE FVector3f& FCluster::GetPosition( uint32 VertIndex )
{
	return *reinterpret_cast< FVector3f* >( &Verts[ VertIndex * GetVertSize() ] );
}

FORCEINLINE const FVector3f& FCluster::GetPosition( uint32 VertIndex ) const
{
	return *reinterpret_cast< const FVector3f* >( &Verts[ VertIndex * GetVertSize() ] );
}

FORCEINLINE float* FCluster::GetAttributes( uint32 VertIndex )
{
	return &Verts[ VertIndex * GetVertSize() + 3 ];
}

FORCEINLINE FVector3f& FCluster::GetNormal( uint32 VertIndex )
{
	return *reinterpret_cast< FVector3f* >( &Verts[ VertIndex * GetVertSize() + 3 ] );
}

FORCEINLINE const FVector3f& FCluster::GetNormal( uint32 VertIndex ) const
{
	return *reinterpret_cast< const FVector3f* >( &Verts[ VertIndex * GetVertSize() + 3 ] );
}

FORCEINLINE FLinearColor& FCluster::GetColor( uint32 VertIndex )
{
	return *reinterpret_cast< FLinearColor* >( &Verts[ VertIndex * GetVertSize() + 6 ] );
}

FORCEINLINE const FLinearColor& FCluster::GetColor( uint32 VertIndex ) const
{
	return *reinterpret_cast< const FLinearColor* >( &Verts[ VertIndex * GetVertSize() + 6 ] );
}

FORCEINLINE FVector2f* FCluster::GetUVs( uint32 VertIndex )
{
	return reinterpret_cast< FVector2f* >( &Verts[ VertIndex * GetVertSize() + 6 + ( bHasColors ? 4 : 0 ) ] );
}

FORCEINLINE const FVector2f* FCluster::GetUVs( uint32 VertIndex ) const
{
	return reinterpret_cast< const FVector2f* >( &Verts[ VertIndex * GetVertSize() + 6 + ( bHasColors ? 4 : 0 ) ] );
}

} // namespace Nanite