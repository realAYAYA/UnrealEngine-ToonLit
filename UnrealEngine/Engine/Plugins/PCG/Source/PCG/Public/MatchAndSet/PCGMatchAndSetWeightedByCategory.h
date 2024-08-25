// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MatchAndSet/PCGMatchAndSetBase.h"
#include "MatchAndSet/PCGMatchAndSetWeighted.h"

#include "PCGMatchAndSetWeightedByCategory.generated.h"

struct FPCGMatchAndSetWeightedEntry;
struct FPropertyChangedEvent;

USTRUCT(BlueprintType)
struct PCG_API FPCGMatchAndSetWeightedByCategoryEntryList
{
	GENERATED_BODY()

	FPCGMatchAndSetWeightedByCategoryEntryList();

#if WITH_EDITOR
	void OnPostLoad();
#endif

	void SetType(EPCGMetadataTypes InType);
	int GetTotalWeight() const;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (ShowOnlyInnerProperties))
	FPCGMetadataTypesConstantStruct CategoryValue;

	/** If the category is the default, if the input does not match to anything, it will use this category. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bIsDefault = false;

	/** Values and their weights */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	TArray<FPCGMatchAndSetWeightedEntry> WeightedEntries;
};

UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGMatchAndSetWeightedByCategory : public UPCGMatchAndSetBase
{
	GENERATED_BODY()

public:
	virtual bool UsesRandomProcess() const { return true; }
	virtual bool ShouldMutateSeed() const { return bShouldMutateSeed; }
	virtual void SetType(EPCGMetadataTypes InType) override;
	/** Propagates (does not set) category (e.g. Match) type to entries. */
	virtual void SetCategoryType(EPCGMetadataTypes InType);

	virtual void MatchAndSet_Implementation(
		FPCGContext& Context,
		const UPCGPointMatchAndSetSettings* InSettings,
		const UPCGPointData* InPointData,
		UPCGPointData* OutPointData) const override;

	virtual bool ValidatePreconditions_Implementation(const UPCGPointData* InPointData) const override;

	//~Begin UObject interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostLoad() override;
#endif
	//~End UObject interface

public:
	/** Attribute to match against */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = MatchAndSet)
	FName CategoryAttribute;

	/** Type of the attribute to match against. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = MatchAndSet)
	EPCGMetadataTypes CategoryType = EPCGMetadataTypes::Double;

	UPROPERTY()
	EPCGMetadataTypesConstantStructStringMode CategoryStringMode_DEPRECATED;

	/** Lookup entries (key -> weighted list) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = MatchAndSet)
	TArray<FPCGMatchAndSetWeightedByCategoryEntryList> Categories;

	/** Controls whether the output data should mutate its seed - prevents issues when doing multiple random processes in a row */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = MatchAndSet)
	bool bShouldMutateSeed = true;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "MatchAndSet/PCGMatchAndSetWeighted.h"
#endif
