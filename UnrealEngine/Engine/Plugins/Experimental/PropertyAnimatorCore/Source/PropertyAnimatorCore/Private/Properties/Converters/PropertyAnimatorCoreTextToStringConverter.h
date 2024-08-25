// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Properties/Converters/PropertyAnimatorCoreConverterBase.h"
#include "PropertyAnimatorCoreTextToStringConverter.generated.h"

UCLASS(Transient)
class UPropertyAnimatorCoreTextToStringConverter : public UPropertyAnimatorCoreConverterBase
{
	GENERATED_BODY()

public:
	//~ Begin UPropertyAnimatorCoreConverterBase
	virtual bool IsConversionSupported(const FPropertyBagPropertyDesc& InFromProperty, const FPropertyBagPropertyDesc& InToProperty) const override;
	virtual bool Convert(const FPropertyBagPropertyDesc& InFromProperty, const FInstancedPropertyBag& InFromBag, const FPropertyBagPropertyDesc& InToProperty, FInstancedPropertyBag& InToBag, const FInstancedStruct* InRule) override;
	virtual UScriptStruct* GetConversionRuleStruct() const override;
	//~ End UPropertyAnimatorCoreConverterBase
};