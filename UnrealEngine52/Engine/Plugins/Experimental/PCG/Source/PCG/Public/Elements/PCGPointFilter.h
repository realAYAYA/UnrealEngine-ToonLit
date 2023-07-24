// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGSettings.h"
#include "Metadata/PCGAttributePropertySelector.h"
#include "Metadata/PCGMetadataTypesConstantStruct.h"

#include "PCGPointFilter.generated.h"

UENUM()
enum class UE_DEPRECATED(5.2, "Not used anymore") EPCGPointTargetFilterType : uint8
{
	Property,
	Metadata
};

UENUM()
enum class UE_DEPRECATED(5.2, "Not used anymore") EPCGPointThresholdType : uint8
{
	Property,
	Metadata,
	Constant
};

UENUM()
enum class UE_DEPRECATED(5.2, "Not used anymore") EPCGPointFilterConstantType : uint8
{
	Integer64,
	Float,
	Vector,
	Vector4,
	//Rotation,
	String,
	Unknown UMETA(Hidden)
};

UENUM()
enum class EPCGPointFilterOperator : uint8
{
	Greater UMETA(DisplayName=">"),
	GreaterOrEqual UMETA(DisplayName=">="),
	Lesser UMETA(DisplayName="<"),
	LesserOrEqual UMETA(DisplayName="<="),
	Equal UMETA(DisplayName="="),
	NotEqual UMETA(DisplayName="!=")
};

/**
* Point filter that allows to do "A op B" type filtering, where A is the input spatial data,
* and B is either a constant, another spatial data, a param data (in filter) or the input itself.
* The filtering can be done either on properties or attributes.
* Some examples:
* - Threshold on property by constant (A.Density > 0.5)
* - Threshold on attribute by constant (A.aaa != "bob")
* - Threshold on property by metadata attribute(A.density >= B.bbb)
* - Threshold on property by property(A.density <= B.steepness)
* - Threshold on attribute by metadata attribute(A.aaa < B.bbb)
* - Threshold on attribute by property(A.aaa == B.color)
*/
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGPointFilterSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	UPCGPointFilterSettings();

	//~Begin UObject interface
	virtual void PostLoad() override;
	//~End UObject interface

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("PointFilter")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGPointFilterSettings", "NodeTitle", "Point Filter"); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Filter; }
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGPointFilterOperator Operator = EPCGPointFilterOperator::Greater;

	/** Target property/attribute related properties */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FPCGAttributePropertySelector TargetAttribute;

	/** Threshold property/attribute/constant related properties */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bUseConstantThreshold = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "!bUseConstantThreshold", EditConditionHides))
	FPCGAttributePropertySelector ThresholdAttribute;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "!bUseConstantThreshold", EditConditionHides, PCG_Overridable))
	bool bUseSpatialQuery = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Constant, meta = (EditCondition = "bUseConstantThreshold", EditConditionHides, ShowOnlyInnerProperties, DisplayAfter = "bUseConstantThreshold"))
	FPCGMetadataTypesConstantStruct AttributeTypes;

#if WITH_EDITORONLY_DATA
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UPROPERTY()
	EPCGPointTargetFilterType TargetFilterType_DEPRECATED = EPCGPointTargetFilterType::Property;

	UPROPERTY()
	EPCGPointProperties TargetPointProperty_DEPRECATED = EPCGPointProperties::Density;

	UPROPERTY()
	FName TargetAttributeName_DEPRECATED = NAME_None;

	UPROPERTY()
	EPCGPointThresholdType ThresholdFilterType_DEPRECATED = EPCGPointThresholdType::Property;

	UPROPERTY()
	EPCGPointProperties ThresholdPointProperty_DEPRECATED = EPCGPointProperties::Density;

	UPROPERTY()
	FName ThresholdAttributeName_DEPRECATED = NAME_None;

	UPROPERTY()
	EPCGPointFilterConstantType ThresholdConstantType_DEPRECATED = EPCGPointFilterConstantType::Float;

	UPROPERTY()
	int64 Integer64Constant_DEPRECATED = 0;

	UPROPERTY()
	float FloatConstant_DEPRECATED = 0;

	UPROPERTY()
	FVector VectorConstant_DEPRECATED = FVector::Zero();

	UPROPERTY()
	FVector4 Vector4Constant_DEPRECATED = FVector4::Zero();

	UPROPERTY()
	FString StringConstant_DEPRECATED;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif 
};

class FPCGPointFilterElement : public FSimplePCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
