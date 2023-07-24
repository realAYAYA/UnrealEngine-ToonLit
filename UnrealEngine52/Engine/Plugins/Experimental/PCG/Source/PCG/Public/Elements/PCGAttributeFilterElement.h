// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGSettings.h"

#include "PCGAttributeFilterElement.generated.h"

UENUM()
enum class EPCGAttributeFilterOperation
{
	KeepSelectedAttributes,
	DeleteSelectedAttributes
};

/**
* Filter the attributes from a given input metadata.
* Will remove all attributes that are not listed in AttributesToKeep.
* If an attribute to keep is not in the original metadata, it won't be added.
* 
* The output will be the original data with the updated metadata.
*/
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGAttributeFilterSettings : public UPCGSettings
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
#endif

	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FName AdditionalTaskName() const override;
	//~End UPCGSettings interface

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGAttributeFilterOperation Operation = EPCGAttributeFilterOperation::KeepSelectedAttributes;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FString SelectedAttributes;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TArray<FName> AttributesToKeep_DEPRECATED;
#endif // WITH_EDITORONLY_DATA

protected:
	virtual FPCGElementPtr CreateElement() const override;
};


class FPCGAttributeFilterElement : public FSimplePCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
