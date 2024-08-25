// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGPoint.h"
#include "Metadata/PCGMetadata.h"

#include "Engine/CollisionProfile.h"
#include "ISMPartition/ISMComponentDescriptor.h"

#include "PCGMeshSelectorBase.generated.h"

class UPCGPointData;
class UPCGSpatialData;
class UStaticMesh;
struct FPCGContext;
struct FPCGStaticMeshSpawnerContext;

class UPCGStaticMeshSpawnerSettings;
class UMaterialInterface;

USTRUCT(BlueprintType)
struct FPCGMeshInstanceList
{
	GENERATED_BODY()

	FPCGMeshInstanceList() = default;

	explicit FPCGMeshInstanceList(const FSoftISMComponentDescriptor& InDescriptor)
		: Descriptor(InDescriptor)
		, AttributePartitionIndex(INDEX_NONE)
	{}

	UPROPERTY(EditAnywhere, Category = Settings)
	FSoftISMComponentDescriptor Descriptor;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	TArray<FTransform> Instances;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	TArray<int64> InstancesMetadataEntry;

	/** Tracks which partition the instance list belongs to. */
	int64 AttributePartitionIndex;
};

UENUM()
enum class EPCGMeshSelectorMaterialOverrideMode : uint8
{
	NoOverride UMETA(Tooltip = "Does not apply any material overrides to the spawned mesh(es)"),
	StaticOverride UMETA(Tooltip = "Applies the material overrides provided in the Static Material Overrides array"),
	ByAttributeOverride UMETA(Tooltip = "Applies the materials overrides using the point data attribute(s) specified in the By Attribute Material Overrides array")
};

/** Struct used to efficiently gather overrides and cache them during instance packing */
struct FPCGMeshMaterialOverrideHelper
{
	FPCGMeshMaterialOverrideHelper() = default;

	// Use this constructor when you have a 1:1 mapping between attributes or static overrides
	void Initialize(
		FPCGContext& InContext,
		bool bUseMaterialOverrideAttributes,
		const TArray<TSoftObjectPtr<UMaterialInterface>>& InStaticMaterialOverrides,
		const TArray<FName>& InMaterialOverrideAttributeNames,
		const UPCGMetadata* InMetadata);

	// Use this constructor when you have common attribute usage or separate static overrides
	void Initialize(
		FPCGContext& InContext,
		bool bInByAttributeOverride,
		const TArray<FName>& InMaterialOverrideAttributeNames,
		const UPCGMetadata* InMetadata);

	void Reset();

	bool IsInitialized() const { return bIsInitialized; }
	bool IsValid() const { return bIsValid; }
	bool OverridesMaterials() const { return bUseMaterialOverrideAttributes; }
	const TArray<TSoftObjectPtr<UMaterialInterface>>& GetMaterialOverrides(PCGMetadataEntryKey EntryKey);

private:
	// Cached data
	TArray<const FPCGMetadataAttributeBase*> MaterialAttributes;
	TArray<TMap<PCGMetadataValueKey, TSoftObjectPtr<UMaterialInterface>>> ValueKeyToOverrideMaterials;
	TArray<TSoftObjectPtr<UMaterialInterface>> WorkingMaterialOverrides;

	// Data needed to perform operations
	bool bIsInitialized = false;
	bool bIsValid = false;
	bool bUseMaterialOverrideAttributes = false;

	TArray<TSoftObjectPtr<UMaterialInterface>> StaticMaterialOverrides;
	TArray<FName> MaterialOverrideAttributeNames;
	const UPCGMetadata* Metadata = nullptr;

	void Initialize(FPCGContext& InContext);
};

UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGMeshSelectorBase : public UObject 
{
	GENERATED_BODY()

public:
	virtual bool SelectInstances(
		FPCGStaticMeshSpawnerContext& Context,
		const UPCGStaticMeshSpawnerSettings* Settings,
		const UPCGPointData* InPointData,
		TArray<FPCGMeshInstanceList>& OutMeshInstances,
		UPCGPointData* OutPointData) const PURE_VIRTUAL(UPCGMeshSelectorBase::SelectInstances, return true;);
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "Data/PCGPointData.h"
#include "PCGElement.h"
#endif
