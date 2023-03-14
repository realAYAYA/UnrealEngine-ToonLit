// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGMeshSelectorBase.h"

#include "PCGMeshSelectorWeighted.generated.h"

USTRUCT(BlueprintType)
struct PCG_API FPCGMeshSelectorWeightedEntry
{
	GENERATED_BODY()

	FPCGMeshSelectorWeightedEntry() = default;

	FPCGMeshSelectorWeightedEntry(TSoftObjectPtr<UStaticMesh> InMesh, int InWeight)
		: Mesh(InMesh), Weight(InWeight)
	{}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	TSoftObjectPtr<UStaticMesh> Mesh;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bOverrideCollisionProfile = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FCollisionProfileName CollisionProfile;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bOverrideMaterials = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	TArray<TObjectPtr<UMaterialInterface>> MaterialOverrides;

	/** Distance at which instances begin to fade. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	float CullStartDistance = 0;
	
	/** Distance at which instances are culled. Use 0 to disable. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	float CullEndDistance = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (ClampMin = "0"))
	int Weight = 1;
};

UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGMeshSelectorWeighted : public UPCGMeshSelectorBase 
{
	GENERATED_BODY()

public:
	virtual void SelectInstances_Implementation(
		UPARAM(ref) FPCGContext& Context, 
		const UPCGStaticMeshSpawnerSettings* Settings, 
		const UPCGSpatialData* InSpatialData,
		TArray<FPCGMeshInstanceList>& OutMeshInstances,
		UPCGPointData* OutPointData) const override;

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	TArray<FPCGMeshSelectorWeightedEntry> MeshEntries;
};

