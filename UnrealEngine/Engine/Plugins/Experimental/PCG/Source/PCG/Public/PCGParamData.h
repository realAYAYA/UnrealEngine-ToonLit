// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGData.h"
#include "Metadata/PCGMetadata.h"

#include "PCGParamData.generated.h"

/**
* Class to hold execution parameters that will be consumed in nodes of the graph
*/
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGParamData : public UPCGData
{
	GENERATED_BODY()

public:
	UPCGParamData(const FObjectInitializer& ObjectInitializer);

	// ~Begin UPCGData interface
	virtual EPCGDataType GetDataType() const override { return EPCGDataType::Param | Super::GetDataType(); }
	// ~End UPCGData interface

	UFUNCTION(BlueprintCallable, Category = Metadata)
	const UPCGMetadata* ConstMetadata() const { return Metadata; }

	UFUNCTION(BlueprintCallable, Category = Metadata)
	UPCGMetadata* MutableMetadata() { return Metadata; }

	/** Returns the entry for the given name */
	UFUNCTION(BlueprintCallable, Category = Params)
	int64 FindMetadataKey(const FName& InName) const;

	/** Creates an entry for the given name, if not already added */
	UFUNCTION(BlueprintCallable, Category = Params)
	int64 FindOrAddMetadataKey(const FName& InName);

	/** Creates a new params that keeps only a given key/name */
	UFUNCTION(BlueprintCallable, Category = Params)
	UPCGParamData* FilterParamsByName(const FName& InName) const;

	UFUNCTION(BlueprintCallable, Category = Params)
	UPCGParamData* FilterParamsByKey(int64 InKey) const;

	// Not accessible through blueprint to make sure the constness is preserved
	UPROPERTY(VisibleAnywhere, Category = Metadata)
	TObjectPtr<UPCGMetadata> Metadata = nullptr;

protected:
	UPROPERTY()
	TMap<FName, int64> NameMap;
};