// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGElement.h"
#include "PCGPoint.h"
#include "Data/PCGPointData.h"

#include "Engine/CollisionProfile.h"

#include "PCGMeshSelectorBase.generated.h"

class UPCGStaticMeshSpawnerSettings;
class UMaterialInterface;

USTRUCT(BlueprintType)
struct FPCGMeshInstanceList
{
	GENERATED_BODY()

	FPCGMeshInstanceList() = default;
	
	FPCGMeshInstanceList(const TSoftObjectPtr<UStaticMesh>& InMesh, bool bInOverrideCollisionProfile, const FCollisionProfileName& InCollisionProfile, bool bInOverrideMaterials, const TArray<UMaterialInterface*>& InMaterialOverrides, const float InCullStartDistance, const float InCullEndDistance)
		: Mesh(InMesh), bOverrideCollisionProfile(bInOverrideCollisionProfile), CollisionProfile(InCollisionProfile), bOverrideMaterials(bInOverrideMaterials), MaterialOverrides(InMaterialOverrides), CullStartDistance(InCullStartDistance), CullEndDistance(InCullEndDistance)
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

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
    TArray<FPCGPoint> Instances;

	/** Distance at which instances begin to fade. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	float CullStartDistance = 0;
	
	/** Distance at which instances are culled. Use 0 to disable. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	float CullEndDistance = 0;
};

UCLASS(Abstract, BlueprintType, Blueprintable, ClassGroup = (Procedural))
class PCG_API UPCGMeshSelectorBase : public UObject 
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, Category = MeshSelection)
	void SelectInstances(
		FPCGContext& Context,
		const UPCGStaticMeshSpawnerSettings* Settings,
		const UPCGSpatialData* InSpatialData,
		TArray<FPCGMeshInstanceList>& OutMeshInstances,
		UPCGPointData* OutPointData) const;

	virtual void SelectInstances_Implementation(
		FPCGContext& Context,
		const UPCGStaticMeshSpawnerSettings* Settings,
		const UPCGSpatialData* InSpatialData,
		TArray<FPCGMeshInstanceList>& OutMeshInstances,
		UPCGPointData* OutPointData) const PURE_VIRTUAL(UPCGMeshSelectorBase::SelectInstances_Implementation);

	/** Searches OutInstanceLists for an InstanceList matching the given parameters. If nothing is found, creates a new InstanceList and adds to OutInstanceLists. Returns true if added. */
	UFUNCTION(BlueprintCallable, Category = MeshSelection)
	bool FindOrAddInstanceList(
		TArray<FPCGMeshInstanceList>& OutInstanceLists,
		const TSoftObjectPtr<UStaticMesh>& Mesh,
		bool bOverrideCollisionProfile,
		const FCollisionProfileName& CollisionProfile,
		bool bOverrideMaterials,
		const TArray<UMaterialInterface*>& MaterialOverrides,
		const float InCullStartDistance,
		const float InCullEndDistance,
		int32& OutIndex) const;
};
