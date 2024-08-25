// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGData.h"
#include "Metadata/PCGMetadata.h"

#include "PCGParamData.generated.h"

/**
* Class to hold execution parameters that will be consumed in nodes of the graph
*/
UCLASS(BlueprintType, ClassGroup = (Procedural), DisplayName = "PCGAttribute Set")
class PCG_API UPCGParamData : public UPCGData
{
	GENERATED_BODY()

public:
	UPCGParamData(const FObjectInitializer& ObjectInitializer);

	// ~Begin UPCGData interface
	virtual EPCGDataType GetDataType() const override { return EPCGDataType::Param; }
	virtual void AddToCrc(FArchiveCrc32& Ar, bool bFullDataCrc) const override;

	virtual bool HasCachedLastSelector() const override;
	virtual FPCGAttributePropertyInputSelector GetCachedLastSelector() const override;
	virtual void SetLastSelector(const FPCGAttributePropertySelector& InSelector) override;

	virtual UPCGParamData* DuplicateData(bool bInitializeMetadata = true) const override;
	// ~End UPCGData interface

	UFUNCTION(BlueprintCallable, Category = Metadata)
	virtual const UPCGMetadata* ConstMetadata() const override{ return Metadata; }

	UFUNCTION(BlueprintCallable, Category = Metadata)
	virtual UPCGMetadata* MutableMetadata() override { return Metadata; }

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

private:
	/** Cache to keep track of the latest attribute manipulated on this data. */
	UPROPERTY()
	bool bHasCachedLastSelector = false;

	UPROPERTY()
	FPCGAttributePropertyInputSelector CachedLastSelector;
};