// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Properties/Handlers/PropertyAnimatorCoreHandlerBase.h"
#include "PropertyAnimatorCoreIntHandler.generated.h"

/** Handles any int property values, get and set handler */
UCLASS(Transient)
class UPropertyAnimatorCoreIntHandler : public UPropertyAnimatorCoreHandlerBase
{
	GENERATED_BODY()

public:
	//~ Begin UPropertyAnimatorCoreHandlerBase
	virtual bool IsPropertySupported(const FPropertyAnimatorCoreData& InPropertyData) const override;
	virtual bool GetValue(const FPropertyAnimatorCoreData& InPropertyData, FInstancedPropertyBag& OutValue) override;
	virtual bool SetValue(const FPropertyAnimatorCoreData& InPropertyData, const FInstancedPropertyBag& InValue) override;
	virtual bool IsAdditiveSupported() const override;
	virtual bool AddValue(const FPropertyAnimatorCoreData& InPropertyData, const FInstancedPropertyBag& InValue) override;
	virtual bool SubtractValue(const FPropertyAnimatorCoreData& InPropertyData, const FInstancedPropertyBag& InValue) override;
	//~ End UPropertyAnimatorCoreHandlerBase
};