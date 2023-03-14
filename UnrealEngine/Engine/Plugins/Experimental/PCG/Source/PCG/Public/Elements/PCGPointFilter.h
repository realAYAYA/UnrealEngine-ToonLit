// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGSettings.h"
#include "PCGElement.h"
#include "PCGPoint.h"

#include "PCGPointFilter.generated.h"

UENUM()
enum class EPCGPointTargetFilterType : uint8
{
	Property,
	Metadata
};

UENUM()
enum class EPCGPointThresholdType : uint8
{
	Property,
	Metadata,
	Constant
};

UENUM()
enum class EPCGPointFilterConstantType : uint8
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
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("PointFilterNode")); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Filter; }
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGPointFilterOperator Operator = EPCGPointFilterOperator::Greater;

	/** Target property/attribute related properties */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGPointTargetFilterType TargetFilterType = EPCGPointTargetFilterType::Property;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "TargetFilterType==EPCGPointTargetFilterType::Property"))
	EPCGPointProperties TargetPointProperty = EPCGPointProperties::Density;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "TargetFilterType==EPCGPointTargetFilterType::Metadata"))
	FName TargetAttributeName = NAME_None;

	/** Threshold property/attribute/constant related properties */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGPointThresholdType ThresholdFilterType = EPCGPointThresholdType::Property;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "ThresholdFilterType==EPCGPointThresholdType::Property"))
	EPCGPointProperties ThresholdPointProperty = EPCGPointProperties::Density;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "ThresholdFilterType==EPCGPointThresholdType::Metadata"))
	FName ThresholdAttributeName = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "ThresholdFilterType==EPCGPointThresholdType::Constant"))
	EPCGPointFilterConstantType ThresholdConstantType = EPCGPointFilterConstantType::Float;

	/** Constants used as threshold comparator */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "ThresholdFilterType==EPCGPointThresholdType::Constant && ThresholdConstantType==EPCGPointFilterConstantType::Integer64"))
	int64 Integer64Constant = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="ThresholdFilterType==EPCGPointThresholdType::Constant && ThresholdConstantType==EPCGPointFilterConstantType::Float"))
	float FloatConstant = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "ThresholdFilterType==EPCGPointThresholdType::Constant && ThresholdConstantType==EPCGPointFilterConstantType::Vector"))
	FVector VectorConstant = FVector::Zero();

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "ThresholdFilterType==EPCGPointThresholdType::Constant && ThresholdConstantType==EPCGPointFilterConstantType::Vector4"))
	FVector4 Vector4Constant = FVector4::Zero();

	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "ThresholdFilterType==EPCGPointThresholdType::Constant && ThresholdConstantType==EPCGPointFilterConstantType::Rotation"))
	//FQuat RotationConstant = FQuat::Identity;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "ThresholdFilterType==EPCGPointThresholdType::Constant && ThresholdConstantType==EPCGPointFilterConstantType::String"))
	FString StringConstant;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "ThresholdFilterType!=EPCGPointThresholdType::Constant"))
	bool bUseSpatialQuery = true;
};

class FPCGPointFilterElement : public FSimplePCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};