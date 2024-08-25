// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGInstanceDataPackerBase.h"

#include "PCGInstanceDataPackerByAttribute.generated.h"

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class UPCGInstanceDataPackerByAttribute : public UPCGInstanceDataPackerBase 
{
	GENERATED_BODY()

public:
	virtual void PackInstances_Implementation(UPARAM(ref) FPCGContext& Context, const UPCGSpatialData* InSpatialData, UPARAM(ref) const FPCGMeshInstanceList& InstanceList, FPCGPackedCustomData& OutPackedCustomData) const override;

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = InstanceDataPacker)
	TArray<FName> AttributeNames;
};
