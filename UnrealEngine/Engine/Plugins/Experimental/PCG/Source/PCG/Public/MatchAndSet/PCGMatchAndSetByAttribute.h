// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MatchAndSet/PCGMatchAndSetBase.h"

#include "PCGMatchAndSetByAttribute.generated.h"

struct FPropertyChangedEvent;

USTRUCT(BlueprintType)
struct PCG_API FPCGMatchAndSetByAttributeEntry
{
	GENERATED_BODY()

	FPCGMatchAndSetByAttributeEntry();

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (ShowOnlyInnerProperties))
	FPCGMetadataTypesConstantStruct ValueToMatch;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (ShowOnlyInnerProperties))
	FPCGMetadataTypesConstantStruct Value;
};

/** 
* This Match & Set object looks up an attribute on a given point,
* then looks up its entries to find a match; if there is one, then it sets it value. 
*/
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGMatchAndSetByAttribute : public UPCGMatchAndSetBase
{
	GENERATED_BODY()

public:
	virtual void SetType(EPCGMetadataTypes InType, EPCGMetadataTypesConstantStructStringMode InStringMode) override;

	/** Propagates (does not set) the Match type to the entries */
	virtual void SetSourceType(EPCGMetadataTypes InType, EPCGMetadataTypesConstantStructStringMode InStringMode);

	virtual void MatchAndSet_Implementation(
		FPCGContext& Context,
		const UPCGPointMatchAndSetSettings* InSettings,
		const UPCGPointData* InPointData,
		UPCGPointData* OutPointData) const override;

	virtual bool ValidatePreconditions_Implementation(const UPCGPointData* InPointData) const override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

public:
	/** Attribute to match on the data */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FName MatchSourceAttribute;

	/** Type of the attribute to match against. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (ValidEnumValues = "Float, Double, Integer32, Integer64, Vector2, Vector, Vector4, Quaternion, Transform, String, Boolean, Rotator, Name"))
	EPCGMetadataTypes MatchSourceType = EPCGMetadataTypes::Double;

	/** String type of the attribute to match against (if required). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "MatchSourceType == EPCGMetadataTypes::String", EditConditionHides))
	EPCGMetadataTypesConstantStructStringMode MatchSourceStringMode = EPCGMetadataTypesConstantStructStringMode::String;

	/** Lookup entries (key-value pairs) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	TArray<FPCGMatchAndSetByAttributeEntry> Entries;
};
