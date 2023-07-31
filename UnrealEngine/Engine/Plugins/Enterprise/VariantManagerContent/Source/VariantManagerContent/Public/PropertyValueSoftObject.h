// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "PropertyValue.h"

#include "PropertyValueSoftObject.generated.h"

/**
 * Stores data from a USoftObjectProperty.
 * It will store it's recorded data as a raw UObject*, and use the usual UPropertyValue
 * facilities for serializing it as a Soft object ptr. This derived class handles converting
 * to and from the property's underlying FSoftObjectPtr to our UObject*.
 * We can't keep a FSoftObjectPtr ourselves, neither as a temp member nor as raw bytes, as it has
 * internal heap-allocated data members like FName and FString.
 */
UCLASS(BlueprintType)
class VARIANTMANAGERCONTENT_API UPropertyValueSoftObject : public UPropertyValue
{
	GENERATED_UCLASS_BODY()

public:
	// UPropertyValue interface
	virtual int32 GetValueSizeInBytes() const override;
	virtual FFieldClass* GetPropertyClass() const override;

	virtual void ApplyDataToResolvedObject() override;
	virtual TArray<uint8> GetDataFromResolvedObject() const;

	virtual void ApplyViaFunctionSetter(UObject* TargetObject) override;

	virtual bool IsRecordedDataCurrent() override;

	virtual void SetRecordedData(const uint8* NewDataBytes, int32 NumBytes, int32 Offset = 0) override;
	//~ UPropertyValue interface
};