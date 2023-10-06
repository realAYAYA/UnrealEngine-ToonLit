// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MatchAndSet/PCGMatchAndSetBase.h"

#include "PCGMatchAndSetWeighted.generated.h"

enum class EPCGMetadataTypes : uint8;
struct FPropertyChangedEvent;

USTRUCT(BlueprintType)
struct PCG_API FPCGMatchAndSetWeightedEntry
{
	GENERATED_BODY()

	FPCGMatchAndSetWeightedEntry();

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (ShowOnlyInnerProperties))
	FPCGMetadataTypesConstantStruct Value;

	/** Relative weight of this entry */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (ClampMin = "0"))
	int Weight = 1;
};

/**
* This Match & Set object assigns randomly a value based on weighted ratios,
* provided in the entries.
*/
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGMatchAndSetWeighted : public UPCGMatchAndSetBase
{
	GENERATED_BODY()

public:
	virtual bool UsesRandomProcess() const { return true; }
	virtual bool ShouldMutateSeed() const { return bShouldMutateSeed; }
	virtual void SetType(EPCGMetadataTypes InType, EPCGMetadataTypesConstantStructStringMode InStringMode) override;

	virtual void MatchAndSet_Implementation(
		FPCGContext& Context,
		const UPCGPointMatchAndSetSettings* InSettings,
		const UPCGPointData* InPointData,
		UPCGPointData* OutPointData) const override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

public:
	/** Values and their respective weights */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	TArray<FPCGMatchAndSetWeightedEntry> Entries;

	/** Controls whether the output data should mutate its seed - prevents issues when doing multiple random processes in a row */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bShouldMutateSeed = true;
};
