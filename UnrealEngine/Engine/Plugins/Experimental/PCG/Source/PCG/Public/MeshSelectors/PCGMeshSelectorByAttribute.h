// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGMeshSelectorBase.h"

#include "PCGMeshSelectorByAttribute.generated.h"

UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGMeshSelectorByAttribute : public UPCGMeshSelectorBase 
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
	FName AttributeName; 

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
};

