// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGSettings.h"
#include "Metadata/PCGAttributePropertySelector.h"
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
UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGCreateAttributeBaseSettings : public UPCGSettings
{
	GENERATED_BODY()
public:
	//~Begin UObject interface
	virtual void PostLoad() override;
	//~End UObject interface

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Metadata; }
	virtual bool IsPinUsedByNodeExecution(const UPCGPin* InPin) const override;
	virtual bool CanEditChange(const FProperty* InProperty) const override;
#endif // WITH_EDITOR
	virtual EPCGDataType GetCurrentPinTypes(const UPCGPin* InPin) const override;
	//~End UPCGSettings interface

	virtual FName GetOutputAttributeName(const FPCGAttributePropertyInputSelector* InSource, const UPCGData* InSourceData) const PURE_VIRTUAL(UPCGCreateAttributeBaseSettings::GetOutputAttributeName, return NAME_None;);

	// This can be set false by inheriting nodes to hide the 'From Source Param' property.
	UPROPERTY(Transient, meta = (EditCondition = false, EditConditionHides))
	bool bDisplayFromSourceParamSetting = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "bDisplayFromSourceParamSetting", EditConditionHides, HideEditConditionToggle, PCG_DiscardPropertySelection, PCG_DiscardExtraSelection))
	FPCGAttributePropertyInputSelector InputSource;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (ShowOnlyInnerProperties))
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

	UPROPERTY()
	FName SourceParamAttributeName_DEPRECATED = NAME_None;
#endif // WITH_EDITORONLY_DATA

protected:
	FName AdditionalTaskNameInternal(FName NodeName) const;

	virtual FPCGElementPtr CreateElement() const override;
	virtual bool ShouldAddAttributesPin() const { return true; }
};

/* Add a new attribute. */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGAddAttributeSettings : public UPCGCreateAttributeBaseSettings
{
	GENERATED_BODY()

	UPCGAddAttributeSettings();

public:
	//~Begin UObject interface
	virtual void PostLoad() override;
	//~End UObject interface

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override;
	virtual FText GetDefaultNodeTitle() const override;
	virtual bool HasDynamicPins() const override { return true; }
	virtual void ApplyDeprecation(UPCGNode* InOutNode) override;
#endif // WITH_EDITOR

	virtual FName AdditionalTaskName() const override;
	//~End UPCGSettings interface

	//~Begin UPCGCreateAttributeBaseSettings interface
	virtual FName GetOutputAttributeName(const FPCGAttributePropertyInputSelector* InSource, const UPCGData* InSourceData) const { return OutputTarget.CopyAndFixSource(InSource, InSourceData).GetName(); }
	//~End UPCGCreateAttributeBaseSettings interface

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_DiscardPropertySelection, PCG_DiscardExtraSelection))
	FPCGAttributePropertyOutputSelector OutputTarget;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FName OutputAttributeName_DEPRECATED = NAME_None;
#endif // WITH_EDITORONLY_DATA

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
};

/* Creates a new Attribute Set. */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGCreateAttributeSetSettings : public UPCGCreateAttributeBaseSettings
{
	GENERATED_BODY()

public:
	UPCGCreateAttributeSetSettings();

	//~Begin UObject interface
	virtual void PostLoad() override;
	//~End UObject interface

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override;
	virtual FText GetDefaultNodeTitle() const override;
	virtual bool HasDynamicPins() const override { return false; }
#endif
	virtual FName AdditionalTaskName() const override;
	//~End UPCGSettings interface

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_DiscardPropertySelection, PCG_DiscardExtraSelection))
	FPCGAttributePropertyOutputNoSourceSelector OutputTarget;

	//~Begin UPCGCreateAttributeBaseSettings interface
	virtual FName GetOutputAttributeName(const FPCGAttributePropertyInputSelector*, const UPCGData*) const { return OutputTarget.GetName(); }
	//~End UPCGCreateAttributeBaseSettings interface

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FName OutputAttributeName_DEPRECATED = NAME_None;
#endif // WITH_EDITORONLY_DATA

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

	virtual bool ShouldAddAttributesPin() const override { return false; }
};

class FPCGCreateAttributeElement : public FSimplePCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;

private:
	/* Create (or clear) an attribute named by OutputAttributeName and depending on the selected type. Value can be overridden by params. Default value will be set to the specified value. */
	FPCGMetadataAttributeBase* ClearOrCreateAttribute(const UPCGCreateAttributeBaseSettings* Settings, UPCGMetadata* Metadata, const FName OutputAttributeName) const;

	/* Set an entry defined by EntryKey (or create one if it is invalid) in the Attribute depending on the selected type. Value can be overridden by params. */
	PCGMetadataEntryKey SetAttribute(const UPCGCreateAttributeBaseSettings* Settings, FPCGMetadataAttributeBase* Attribute, UPCGMetadata* Metadata, PCGMetadataEntryKey EntryKey) const;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "PCGPoint.h"
#endif
