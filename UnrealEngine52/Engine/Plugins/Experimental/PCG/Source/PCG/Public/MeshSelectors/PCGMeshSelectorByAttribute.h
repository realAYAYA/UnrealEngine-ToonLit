// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGMeshSelectorBase.h"

#include "PCGMeshSelectorByAttribute.generated.h"

UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGMeshSelectorByAttribute : public UPCGMeshSelectorBase 
{
	GENERATED_BODY()

public:
	// ~Begin UObject interface
	void PostLoad() override;
	// ~End UObject interface

	virtual bool SelectInstances(
		FPCGStaticMeshSpawnerContext& Context,
		const UPCGStaticMeshSpawnerSettings* Settings,
		const UPCGPointData* InPointData,
		TArray<FPCGMeshInstanceList>& OutMeshInstances,
		UPCGPointData* OutPointData) const override;

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FName AttributeName;

	UPROPERTY(EditAnywhere, Category = Settings)
	FSoftISMComponentDescriptor TemplateDescriptor;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (InlineEditConditionToggle))
	bool bUseAttributeMaterialOverrides = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName = "By Attribute Material Overrides", Category = Settings, meta = (EditCondition = "bUseAttributeMaterialOverrides"))
	TArray<FName> MaterialOverrideAttributes;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	bool bOverrideCollisionProfile_DEPRECATED = false;

	UPROPERTY()
	FCollisionProfileName CollisionProfile_DEPRECATED = UCollisionProfile::NoCollision_ProfileName;

	UPROPERTY()
	EPCGMeshSelectorMaterialOverrideMode MaterialOverrideMode_DEPRECATED = EPCGMeshSelectorMaterialOverrideMode::NoOverride;

	UPROPERTY()
	bool bOverrideMaterials_DEPRECATED = false;

	UPROPERTY()
	TArray<TSoftObjectPtr<UMaterialInterface>> MaterialOverrides_DEPRECATED;

	/** Distance at which instances begin to fade. */
	UPROPERTY()
	float CullStartDistance_DEPRECATED = 0;
	
	/** Distance at which instances are culled. Use 0 to disable. */
	UPROPERTY()
	float CullEndDistance_DEPRECATED = 0;

	UPROPERTY()
	int32 WorldPositionOffsetDisableDistance_DEPRECATED = 0;
#endif // WITH_EDITORONLY_DATA
};
