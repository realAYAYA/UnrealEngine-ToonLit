// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "Containers/Array.h"

struct FStaticMeshBuildVertex;

struct FNaniteDisplacedMeshParams;

bool DisplaceNaniteMesh(
	const FNaniteDisplacedMeshParams& Parameters,
	const uint32 NumTextureCoord,
	TArray< FStaticMeshBuildVertex >& Verts,
	TArray< uint32 >& Indexes,
	TArray< int32 >& MaterialIndexes
);

#endif
