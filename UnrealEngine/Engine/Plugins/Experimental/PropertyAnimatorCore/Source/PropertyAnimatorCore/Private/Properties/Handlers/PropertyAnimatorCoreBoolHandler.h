// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Properties/Handlers/PropertyAnimatorCoreHandlerBase.h"
#include "PropertyAnimatorCoreBoolHandler.generated.h"

/** Handles any boolean property values, get and set handler */
UCLASS(Transient)
class UPropertyAnimatorCoreBoolHandler : public UPropertyAnimatorCoreHandlerBase
{
	GENERATED_BODY()

public:
	//~ Begin UPropertyAnimatorCoreHandlerBase
	virtual bool IsPropertySupported(const FPropertyAnimatorCoreData& InPropertyData) const override;
	virtual bool GetValue(const FPropertyAnimatorCoreData& InPropertyData, FInstancedPropertyBag& OutValue) override;
	virtual bool SetValue(const FPropertyAnimatorCoreData& InPropertyData, const FInstancedPropertyBag& InValue) override;
	//~ End UPropertyAnimatorCoreHandlerBase
};