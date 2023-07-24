// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGInstancePackerBase.h"

#include "PCGInstancePackerByAttribute.generated.h"

UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGInstancePackerByAttribute : public UPCGInstancePackerBase 
{
	GENERATED_BODY()

public:
	virtual void PackInstances_Implementation(UPARAM(ref) FPCGContext& Context, const UPCGSpatialData* InSpatialData, UPARAM(ref) const FPCGMeshInstanceList& InstanceList, FPCGPackedCustomData& OutPackedCustomData) const override;

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	TArray<FName> AttributeNames;
};
