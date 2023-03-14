// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WorldPartition/WorldPartitionBuilder.h"
#include "WorldPartitionLandscapeSplineMeshesBuilder.generated.h"

class UWorld;
class ALandscapeSplineMeshesActor;
class UStaticMeshComponent;
class UPackage;
class ILandscapeSplineInterface;

// Example Command Line: ProjectName MapName -run=WorldPartitionBuilderCommandlet -SCCProvider=Perforce -Builder=WorldPartitionLandscapeSplineMeshesBuilder (<Optional> -NewGridSize=Value)
UCLASS()
class UNREALED_API UWorldPartitionLandscapeSplineMeshesBuilder : public UWorldPartitionBuilder
{
	GENERATED_UCLASS_BODY()

public:
	// UWorldPartitionBuilder interface begin
	virtual bool RequiresCommandletRendering() const override { return false; }
	virtual ELoadingMode GetLoadingMode() const override { return ELoadingMode::Custom; }
	virtual bool RunInternal(UWorld* World, const FCellInfo& InCellInfo, FPackageSourceControlHelper& PackageHelper) override;
	// UWorldPartitionBuilder interface end

	// Helper function that creates a builder and runs it on an already loaded/initialized world
	static bool RunOnInitializedWorld(UWorld* World);

private:
	void FilterStaticMeshComponents(TArray<class UStaticMeshComponent*>& InOutComponents);
	void CloneStaticMeshComponentInActor(ALandscapeSplineMeshesActor* InActor, const UStaticMeshComponent* InMeshComponent);
	void CloneStaticMeshComponent(const UStaticMeshComponent* InSrcMeshComponent, UStaticMeshComponent* DstMeshComponent);
	int32 HashStaticMeshComponent(const UStaticMeshComponent* InComponent);
	FName GetSplineCollisionProfileName(const UStaticMeshComponent* InMeshComponent);
	ALandscapeSplineMeshesActor* GetOrCreatePartitionActorForComponent(class UWorld* InWorld, const UStaticMeshComponent* InMeshComponent, const FGuid& InLandscapeGuid);

	uint32 NewGridSize;
	
	static class UStaticMesh* SplineEditorMesh;
};