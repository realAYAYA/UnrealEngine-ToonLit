// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IHasPropertyBindingExtensibility.h"

class UWidgetBlueprint;
class UWidget;

class FMVVMPropertyBindingExtension
	: public IPropertyBindingExtension
{
	virtual TOptional<FName> GetCurrentValue(const UWidgetBlueprint* WidgetBlueprint, const UWidget* Widget, const FProperty* Property) const override;
	virtual void ClearCurrentValue(const UWidgetBlueprint* WidgetBlueprint, const UWidget* Widget, const FProperty* Property) override;
	virtual TSharedPtr<FExtender> CreateMenuExtender(const UWidgetBlueprint* WidgetBlueprint, const UWidget* Widget, const FProperty* Property) override;
	virtual bool CanExtend(const UWidgetBlueprint* WidgetBlueprint, const UWidget* Widget, const FProperty* Property) const;
};
