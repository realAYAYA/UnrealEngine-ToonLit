// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "CoreMinimal.h"
#include "Math/UnrealMathSSE.h"

class FStaticMeshLODGroup;
class FStaticMeshRenderData;
class FStaticMeshSectionArray;
class USkeletalMesh;
class UStaticMesh;
struct FSkeletalMeshBuildParameters;
struct FStaticMeshBuildVertex;
struct FStaticMeshSection;
struct FMeshBuildVertexData;

/**
 * Abstract class which is the base class of all builder.
 * All share code to build some render data should be found inside this class
 */
class MESHBUILDER_API FMeshBuilder
{
public:
	FMeshBuilder();
	
	/**
	 * Build function should be override and is the starting point for static mesh builders
	 */
	virtual bool Build(
		FStaticMeshRenderData& OutRenderData,
		UStaticMesh* StaticMesh,
		const FStaticMeshLODGroup& LODGroup,
		bool bAllowNanite) = 0;

	virtual bool BuildMeshVertexPositions(
		UStaticMesh* StaticMesh,
		TArray<uint32>& Indices,
		TArray<FVector3f>& Vertices,
		FStaticMeshSectionArray& Sections) = 0;

	/**
	 * Build function should be override and is the starting point for skeletal mesh builders
	 */
	virtual bool Build(const FSkeletalMeshBuildParameters& SkeletalMeshBuildParameters) = 0;

private:

};
