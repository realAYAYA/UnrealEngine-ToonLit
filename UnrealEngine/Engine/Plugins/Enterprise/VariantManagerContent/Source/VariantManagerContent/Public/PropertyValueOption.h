// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "PropertyValue.h"

#include "PropertyValueOption.generated.h"

// PropertyValue that can only be captured from ASwitchActors
UCLASS(BlueprintType)
class VARIANTMANAGERCONTENT_API UPropertyValueOption : public UPropertyValue
{
	GENERATED_UCLASS_BODY()

public:
	// UPropertyValue interface
	virtual bool Resolve(UObject* OnObject = nullptr) override;
	virtual TArray<uint8> GetDataFromResolvedObject() const override;
	virtual void ApplyDataToResolvedObject() override;
	virtual const TArray<uint8>& GetDefaultValue() override;
	virtual int32 GetValueSizeInBytes() const override;
	//~ UPropertyValue interface
};