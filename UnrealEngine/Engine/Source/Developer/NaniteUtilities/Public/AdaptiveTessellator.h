// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "CoreMinimal.h"
#include "Math/UnrealMathSSE.h"
#include "Math/Vector.h"
#include "Math/Vector2D.h"
#include "Templates/Function.h"

#include <atomic>

struct FLerpVert;

namespace Nanite
{

class FAdaptiveTessellator
{
	using FDispFunc = TFunctionRef< FVector3f (
		const FVector3f&,	// Barycentrics,
		const FLerpVert&,	// Vert0,
		const FLerpVert&,	// Vert1,
		const FLerpVert&,	// Vert2,
		int32				// MaterialIndex
	) >;

	using FBoundsFunc = TFunctionRef< FVector2f ( 
		const FVector3f[3],	// Barycentrics[3],
		const FLerpVert&,	// Vert0,
		const FLerpVert&,	// Vert1,
		const FLerpVert&,	// Vert2,
		const FVector3f&,	// Displacement0,
		const FVector3f&,	// Displacement1,
		const FVector3f&,	// Displacement2,
		int32				// MaterialIndex
	) >;

public:
	NANITEUTILITIES_API FAdaptiveTessellator(
		TArray< FLerpVert >&	InVerts,
		TArray< uint32 >&		InIndexes,
		TArray< int32 >&		InMaterialIndexes,
		float		InTargetError,
		float		InSampleRate,
		bool		bCrackFree,
		FDispFunc	InGetDisplacement,
		FBoundsFunc	InGetErrorBounds
	);

protected:
	FDispFunc	GetDisplacement;
	FBoundsFunc	GetErrorBounds;

	float	TargetError;
	float	SampleRate;
	
	TArray< FLerpVert >&	Verts;
	TArray< uint32 >&		Indexes;
	TArray< int32 >&		MaterialIndexes;

	TArray< FVector3f >		Displacements;
	TArray< int32 >			AdjEdges;

	struct FTriangle
	{
		FVector3f	SplitBarycentrics;
		int32		RequestIndex = -1;
	};
	TArray< FTriangle >		Triangles;
	
	TArray< uint32 >		FindRequests;
	TArray< uint32 >		SplitRequests;
	std::atomic< uint32 >	NumSplits;

	void		AddFindRequest( uint32 TriIndex );
	void		RemoveSplitRequest( uint32 TriIndex );
	void		LinkEdge( int32 EdgeIndex0, int32 EdgeIndex1 );

	FVector3f	GetTriNormal( uint32 TriIndex ) const;
	bool		CouldFlipEdge( uint32 EdgeIndex ) const;
	void		TryDelaunayFlip( uint32 EdgeIndex );

	void		FindSplit( uint32 TriIndex );
	void		FindSplitBVH( uint32 TriIndex );
	void		SplitTriangle( uint32 TriIndex );
};

} // namespace Nanite