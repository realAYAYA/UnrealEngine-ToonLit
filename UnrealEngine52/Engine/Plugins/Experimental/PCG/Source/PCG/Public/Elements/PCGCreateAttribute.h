// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGSettings.h"
#include "Metadata/PCGMetadataTypesConstantStruct.h"
#include "Metadata/PCGMetadataCommon.h"

#include "PCGCreateAttribute.generated.h"

class FPCGMetadataAttributeBase;
class UPCGMetadata;

/**
* Adds an attribute to Spatial data or to an Attribute Set, or creates a new Attribute Set if no
* input is provided.
* 
* Note: This need to be updated if we ever add new types.
*/
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGCreateAttributeSettings : public UPCGSettings
{
	GENERATED_BODY()
public:
	//~Begin UObject interface
	virtual void PostLoad() override;
	//~End UObject interface

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override;
	virtual FText GetDefaultNodeTitle() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Metadata; }
	virtual bool HasDynamicPins() const override { return true; }
#endif // WITH_EDITOR

	virtual FName AdditionalTaskName() const override;
	//~End UPCGSettings interface

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FName OutputAttributeName = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = bDisplayFromSourceParamSetting, EditConditionHides, HideEditConditionToggle))
	bool bFromSourceParam = false;

	// This can be set false by inheriting nodes to hide the 'From Source Param' property.
	UPROPERTY(Transient, meta = (EditCondition = false, EditConditionHides))
	bool bDisplayFromSourceParamSetting = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "bFromSourceParam", EditConditionHides))
	FName SourceParamAttributeName = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "!bFromSourceParam", EditConditionHides, ShowOnlyInnerProperties, DisplayAfter = "bFromSourceParam"))
	FPCGMetadataTypesConstantStruct AttributeTypes;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	EPCGMetadataTypes Type_DEPRECATED = EPCGMetadataTypes::Double;

	UPROPERTY()
	float FloatValue_DEPRECATED = 0.0;

	UPROPERTY()
	int32 Int32Value_DEPRECATED = 0;

	UPROPERTY()
	double DoubleValue_DEPRECATED = 0.0;

	UPROPERTY()
	int64 IntValue_DEPRECATED = 0;

	UPROPERTY()
	FVector2D Vector2Value_DEPRECATED = FVector2D::ZeroVector;

	UPROPERTY()
	FVector VectorValue_DEPRECATED = FVector::ZeroVector;

	UPROPERTY()
	FVector4 Vector4Value_DEPRECATED = FVector4::Zero();

	UPROPERTY()
	FQuat QuatValue_DEPRECATED = FQuat::Identity;

	UPROPERTY()
	FTransform TransformValue_DEPRECATED = FTransform::Identity;

	UPROPERTY()
	FString StringValue_DEPRECATED = "";

	UPROPERTY()
	bool BoolValue_DEPRECATED = false;

	UPROPERTY()
	FRotator RotatorValue_DEPRECATED = FRotator::ZeroRotator;

	UPROPERTY()
	FName NameValue_DEPRECATED = NAME_None;

	UPROPERTY()
	bool bKeepExistingAttributes_DEPRECATED = false;
#endif // WITH_EDITORONLY_DATA

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

	FName AdditionalTaskNameInternal(FName NodeName) const;

	virtual FPCGElementPtr CreateElement() const override;
};

/* Creates a new Attribute Set. */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGCreateAttributeSetSettings : public UPCGCreateAttributeSettings
{
	GENERATED_BODY()

public:
	UPCGCreateAttributeSetSettings();

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override;
	virtual FText GetDefaultNodeTitle() const override;
#endif
	virtual FName AdditionalTaskName() const override;

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override { return TArray<FPCGPinProperties>(); }
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	//~End UPCGSettings interface
};

class FPCGCreateAttributeElement : public FSimplePCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;

private:
	/* Create (or clear) an attribute named by OutputAttributeName and depending on the selected type. Value can be overridden by params. Default value will be set to the specified value. */
	FPCGMetadataAttributeBase* ClearOrCreateAttribute(const UPCGCreateAttributeSettings* Settings, UPCGMetadata* Metadata, const FName* OutputAttributeNameOverride = nullptr) const;

	/* Set an entry defined by EntryKey (or create one if it is invalid) in the Attribute depending on the selected type. Value can be overridden by params. */
	PCGMetadataEntryKey SetAttribute(const UPCGCreateAttributeSettings* Settings, FPCGMetadataAttributeBase* Attribute, UPCGMetadata* Metadata, PCGMetadataEntryKey EntryKey) const;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "PCGPoint.h"
#endif
