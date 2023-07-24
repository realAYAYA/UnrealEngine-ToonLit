// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Metadata/PCGMetadataTypesConstantStruct.h"
#include "PCGSettings.h"
#include "Metadata/PCGAttributePropertySelector.h"

#include "PCGPointMatchAndSet.generated.h"

class UPCGMatchAndSetBase;

/** This settings class is used to create a PCG node that will apply a "Match and Set" operation
* on the point data it consumes as input.
* E.g. for a given point, if it matches with something in the Match & Set object, it will set a value on the point.
*/
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGPointMatchAndSetSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	UPCGPointMatchAndSetSettings(const FObjectInitializer& ObjectInitializer);

	// ~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("PointMatchAndSet")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGPointMatchAndSetSettings", "NodeTitle", "Point Match And Set"); }
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Metadata; }
#endif // WITH_EDITOR

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;

public:
	// ~Begin UObject interface
	void PostLoad();

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	// ~End UObject interface

	/** Recreates the match & set instance stored in this settings object if needed. */
	UFUNCTION(BlueprintCallable, Category = Settings)
	void SetMatchAndSetType(TSubclassOf<UPCGMatchAndSetBase> InMatchAndSetType);

protected:
	void RefreshMatchAndSet();

public:
	/** Defines the type of Match & Set object to use. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, NoClear, Category = Settings)
	TSubclassOf<UPCGMatchAndSetBase> MatchAndSetType;

	/** Instance of MatchAndSetType, stores the data that will be used in these settings. */
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Instanced, Category = Settings)
	TObjectPtr<UPCGMatchAndSetBase> MatchAndSetInstance;

	/** "Set" part of the Match & Set - defines what will be changed in the operation */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FPCGAttributePropertySelector SetTarget;

	/** If the "Set" part is an attribute, then the type must be provided */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "bSetTargetIsAttribute", HideEditConditionToggle, EditConditionHides, ValidEnumValues = "Float, Double, Integer32, Integer64, Vector2, Vector, Vector4, Quaternion, Transform, String, Boolean, Rotator, Name"))
	EPCGMetadataTypes SetTargetType = EPCGMetadataTypes::Double;

	/** For string types, the subtype is used to cleanup the UI. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "bSetTargetIsAttribute && SetTargetType == EPCGMetadataTypes::String", EditConditionHides))
	EPCGMetadataTypesConstantStructStringMode SetTargetStringMode = EPCGMetadataTypesConstantStructStringMode::String;

private:
#if WITH_EDITORONLY_DATA
	// Property used to sidestep edit condition issues - reflects SetTarget.Selection == Attribute
	UPROPERTY()
	bool bSetTargetIsAttribute = true;
#endif
};

class FPCGPointMatchAndSetElement : public FSimplePCGElement
{
public:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "MatchAndSet/PCGMatchAndSetBase.h"
#endif
