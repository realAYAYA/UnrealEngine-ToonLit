// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGElement.h"
#include "MeshSelectors/PCGMeshSelectorBase.h"

#include "PCGInstancePackerBase.generated.h"

class UPCGSpatialData;
class FPCGMetadataAttributeBase;

USTRUCT(BlueprintType)
struct FPCGPackedCustomData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	int NumCustomDataFloats = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
    TArray<float> CustomData;
};

UCLASS(Abstract, BlueprintType, Blueprintable, ClassGroup = (Procedural))
class PCG_API UPCGInstancePackerBase : public UObject 
{
	GENERATED_BODY()

public:
	/** Defines the strategy for (H)ISM custom float data packing */
	UFUNCTION(BlueprintNativeEvent, Category = InstancePacking)
	void PackInstances(FPCGContext& Context, const UPCGSpatialData* InSpatialData, const FPCGMeshInstanceList& InstanceList, FPCGPackedCustomData& OutPackedCustomData) const;

	virtual void PackInstances_Implementation(FPCGContext& Context, const UPCGSpatialData* InSpatialData, const FPCGMeshInstanceList& InstanceList, FPCGPackedCustomData& OutPackedCustomData) const PURE_VIRTUAL(UPCGInstancePackerBase::PackInstances_Implementation);

	/** Interprets Metadata TypeId and increments OutPackedCustomData.NumCustomDataFloats appropriately. Returns false if the type could not be interpreted. */
	UFUNCTION(BlueprintCallable, Category = InstancePacking)
	bool AddTypeToPacking(int TypeId, FPCGPackedCustomData& OutPackedCustomData) const;

	/** Build a PackedCustomData by processing each attribute in order for each point in the InstanceList */
	UFUNCTION(BlueprintCallable, Category = InstancePacking) 
	void PackCustomDataFromAttributes(const FPCGMeshInstanceList& InstanceList, const UPCGMetadata* Metadata, const TArray<FName>& AttributeNames, FPCGPackedCustomData& OutPackedCustomData) const;

	/** Build a PackedCustomData by processing each attribute in order for each point in the InstanceList */
	void PackCustomDataFromAttributes(const FPCGMeshInstanceList& InstanceList, const TArray<const FPCGMetadataAttributeBase*>& Attributes, FPCGPackedCustomData& OutPackedCustomData) const;
};
