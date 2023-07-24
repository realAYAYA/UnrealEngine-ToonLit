// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MeshSelectors/PCGMeshSelectorBase.h"
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

#if WITH_EDITOR
	void ApplyDeprecation();
#endif

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
	virtual bool SelectInstances(
		FPCGStaticMeshSpawnerContext& Context,
		const UPCGStaticMeshSpawnerSettings* Settings,
		const UPCGPointData* InPointData,
		TArray<FPCGMeshInstanceList>& OutMeshInstances,
		UPCGPointData* OutPointData) const override;

	void PostLoad();

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FName CategoryAttribute;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	TArray<FPCGWeightedByCategoryEntryList> Entries;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (InlineEditConditionToggle))
	bool bUseAttributeMaterialOverrides = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName = "By Attribute Material Overrides", Category = Settings, meta = (EditCondition = "bUseAttributeMaterialOverrides"))
	TArray<FName> MaterialOverrideAttributes;
};
