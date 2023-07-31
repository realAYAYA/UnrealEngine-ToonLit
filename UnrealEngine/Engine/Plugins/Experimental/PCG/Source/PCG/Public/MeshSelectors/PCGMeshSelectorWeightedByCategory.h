// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGMeshSelectorBase.h"
#include "PCGMeshSelectorWeighted.h"

#include "PCGMeshSelectorWeightedByCategory.generated.h"

USTRUCT(BlueprintType)
struct PCG_API FPCGWeightedByCategoryEntryList
{
	GENERATED_BODY()

	FPCGWeightedByCategoryEntryList() = default;

	FPCGWeightedByCategoryEntryList(const FString& InCategoryEntry, const TArray<FPCGMeshSelectorWeightedEntry>& InWeightedMeshEntries)
		: CategoryEntry(InCategoryEntry), WeightedMeshEntries(InWeightedMeshEntries)
	{}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FString CategoryEntry;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool IsDefault = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	TArray<FPCGMeshSelectorWeightedEntry> WeightedMeshEntries;
};

UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGMeshSelectorWeightedByCategory : public UPCGMeshSelectorBase 
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
	FName CategoryAttribute;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	TArray<FPCGWeightedByCategoryEntryList> Entries;
};