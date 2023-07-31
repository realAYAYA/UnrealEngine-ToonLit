// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/HashTable.h"
#include "CoreMinimal.h"
#include "Math/IntVector.h"
#include "Math/UnrealMathSSE.h"
#include "RenderResource.h"

namespace Nanite
{

class FTessellationTable
{
public:
	static constexpr uint32 MaxTessFactor = 16;
	static constexpr uint32 MaxNumTris = MaxTessFactor * MaxTessFactor;
	static constexpr uint32 BarycentricMax = 1 << 15;

	TArray< FUintVector2 >	OffsetTable;
	TArray< uint32 >		Verts;
	TArray< uint32 >		Indexes;

public:
	RENDERER_API			FTessellationTable();
	RENDERER_API int32		GetPattern( FIntVector TessFactors ) const;

	uint32		GetNumVerts( int32 Pattern ) const;
	uint32		GetNumTris( int32 Pattern ) const;

private:
	FIntVector	GetBarycentrics( uint32 Vert ) const;
	float		LengthSquared( const FIntVector& Barycentrics, const FIntVector& TessFactors ) const;
	void		SnapAtEdges( FIntVector& Barycentrics, const FIntVector& TessFactors ) const;
	uint32		AddVert( uint32 Vert );
	void		SplitEdge( uint32 TriIndex, uint32 EdgeIndex, uint32 LeftFactor, uint32 RightFactor, const FIntVector& TessFactors );

	void		RecursiveSplit( const FIntVector& TessFactors );
	void		UniformTessellateAndSnap( const FIntVector& TessFactors );

	int32		FirstVert;
	int32		FirstTri;

	FHashTable	HashTable;
};

FORCEINLINE uint32 FTessellationTable::GetNumVerts( int32 Pattern ) const
{
	return	OffsetTable[ Pattern + 1 ].X -
			OffsetTable[ Pattern ].X;
}

FORCEINLINE uint32 FTessellationTable::GetNumTris( int32 Pattern ) const
{
	return	OffsetTable[ Pattern + 1 ].Y -
			OffsetTable[ Pattern ].Y;
}

RENDERER_API FTessellationTable& GetTessellationTable();


class FTessellationTableResources : public FRenderResource
{
public:
	FByteAddressBuffer	Offsets;
	FByteAddressBuffer	Verts;
	FByteAddressBuffer	Indexes;

	virtual void InitRHI() override;
	virtual void ReleaseRHI() override;
};

} // namespace Nanite