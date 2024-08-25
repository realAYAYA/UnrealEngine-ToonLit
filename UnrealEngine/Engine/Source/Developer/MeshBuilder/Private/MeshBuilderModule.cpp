// Copyright Epic Games, Inc. All Rights Reserved.

#include "IMeshBuilderModule.h"
#include "Modules/ModuleManager.h"

#include "StaticMeshBuilder.h"
#include "Engine/StaticMesh.h"
#include "SkeletalMeshBuilder.h"
#include "Engine/SkeletalMesh.h"

class FMeshBuilderModule : public IMeshBuilderModule
{
public:

	FMeshBuilderModule()
	{
	}

	virtual void StartupModule() override
	{
		// Register any modular features here
	}

	virtual void ShutdownModule() override
	{
		// Unregister any modular features here
	}

	virtual bool BuildMesh(FStaticMeshRenderData& OutRenderData, UObject* Mesh, const FStaticMeshLODGroup& LODGroup, bool bAllowNanite) override;

	virtual bool BuildMeshVertexPositions(
		UObject* StaticMesh,
		TArray<uint32>& Indices,
		TArray<FVector3f>& Vertices,
		FStaticMeshSectionArray& Sections) override;

	virtual bool BuildSkeletalMesh(const FSkeletalMeshBuildParameters& SkeletalMeshBuildParameters) override;

private:

};

IMPLEMENT_MODULE(FMeshBuilderModule, MeshBuilder );

bool FMeshBuilderModule::BuildMesh(FStaticMeshRenderData& OutRenderData, class UObject* Mesh, const FStaticMeshLODGroup& LODGroup, bool bAllowNanite)
{
	UStaticMesh* StaticMesh = Cast<UStaticMesh>(Mesh);
	if (StaticMesh != nullptr)
	{
		//Call the static mesh builder
		return FStaticMeshBuilder().Build(OutRenderData, StaticMesh, LODGroup, bAllowNanite);
	}
	return false;
}

bool FMeshBuilderModule::BuildMeshVertexPositions(
	UObject* Mesh,
	TArray<uint32>& Indices,
	TArray<FVector3f>& Vertices,
	FStaticMeshSectionArray& Sections)
{
	UStaticMesh* StaticMesh = Cast< UStaticMesh >(Mesh);
	if (StaticMesh)
	{
		//Call the static mesh builder
		return FStaticMeshBuilder().BuildMeshVertexPositions(StaticMesh, Indices, Vertices, Sections);
	}
	return false;
}

bool FMeshBuilderModule::BuildSkeletalMesh(const FSkeletalMeshBuildParameters& SkeletalMeshBuildParameters)
{
	//Call the skeletal mesh builder
	return FSkeletalMeshBuilder().Build(SkeletalMeshBuildParameters);
}