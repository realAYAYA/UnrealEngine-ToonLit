// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGSettings.h"

#include "PCGDeleteAttributesElement.generated.h"

UENUM()
enum class EPCGAttributeFilterOperation
{
	KeepSelectedAttributes UMETA(DisplayName = "Keep Only Selected Attributes"),
	DeleteSelectedAttributes
};

/**
* Removes attributes from a given input metadata.
* Either removes specifically named attributes or remove all attributes not in a given list.
* 
* The output will be the original data with the updated metadata.
*/
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class UPCGDeleteAttributesSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	UPCGDeleteAttributesSettings();

	//~Begin UObject interface
	virtual void PostLoad() override;
	//~End UObject interface

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override;
	virtual FText GetDefaultNodeTitle() const override;
	virtual TArray<FText> GetNodeTitleAliases() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Metadata; }
	virtual bool HasDynamicPins() const override { return true; }
#endif
	
	virtual FString GetAdditionalTitleInformation() const override;

protected:
#if WITH_EDITOR
	virtual EPCGChangeType GetChangeTypeForProperty(const FName& InPropertyName) const override { return Super::GetChangeTypeForProperty(InPropertyName) | EPCGChangeType::Cosmetic; }
#endif
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	// Implementation note: the default has been changed to DeleteSelected for new objects
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGAttributeFilterOperation Operation = EPCGAttributeFilterOperation::KeepSelectedAttributes;

	/** Comma-separated list of attributes to keep or remove from the input data. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FString SelectedAttributes;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TArray<FName> AttributesToKeep_DEPRECATED;
#endif // WITH_EDITORONLY_DATA
};


class FPCGDeleteAttributesElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
