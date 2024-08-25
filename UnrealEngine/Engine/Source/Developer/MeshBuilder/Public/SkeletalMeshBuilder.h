// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MeshBuilder.h"

DECLARE_LOG_CATEGORY_EXTERN(LogSkeletalMeshBuilder, Log, All);

class UStaticMesh;
class FStaticMeshRenderData;
class FStaticMeshLODGroup;
class USkeletalMesh;

class MESHBUILDER_API FSkeletalMeshBuilder : public FMeshBuilder
{
public:
	FSkeletalMeshBuilder();

	//No support for static mesh build in this class
	virtual bool Build(FStaticMeshRenderData& OutRenderData, UStaticMesh* StaticMesh, const FStaticMeshLODGroup& LODGroup, bool bAllowNanite) override
	{
		bool No_Support_For_StaticMesh_Build_In_FSkeletalMeshBuilder_Class = false;
		check(No_Support_For_StaticMesh_Build_In_FSkeletalMeshBuilder_Class);
		return false;
	}

	virtual bool BuildMeshVertexPositions(
		UStaticMesh* StaticMesh,
		TArray<uint32>& Indices,
		TArray<FVector3f>& Vertices,
		FStaticMeshSectionArray& Sections) override
	{
		bool No_Support_For_StaticMesh_Build_In_FSkeletalMeshBuilder_Class = false;
		check(No_Support_For_StaticMesh_Build_In_FSkeletalMeshBuilder_Class);
		return false;
	}
	
	virtual bool Build(const struct FSkeletalMeshBuildParameters& SkeletalMeshBuildParameters) override;

	virtual ~FSkeletalMeshBuilder() {}

private:

};

