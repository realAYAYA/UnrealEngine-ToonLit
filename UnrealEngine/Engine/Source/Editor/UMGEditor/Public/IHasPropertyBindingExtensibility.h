// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Misc/Optional.h"
#include "Templates/SharedPointer.h"

class UWidget;
class UWidgetBlueprint;

class UMGEDITOR_API IPropertyBindingExtension
{
public:
	/** Does this extension want to extend this property in the widget? */
	virtual bool CanExtend(const UWidgetBlueprint* WidgetBlueprint, const UWidget* Widget, const FProperty* Property) const = 0;
	virtual TSharedPtr<FExtender> CreateMenuExtender(const UWidgetBlueprint* WidgetBlueprint, const UWidget* Widget, const FProperty* Property) = 0;
	virtual TOptional<FName> GetCurrentValue(const UWidgetBlueprint* WidgetBlueprint, const UWidget* Widget, const FProperty* Property) const = 0;
	virtual void ClearCurrentValue(const UWidgetBlueprint* WidgetBlueprint, const UWidget* Widget, const FProperty* Property) = 0;
};

/**
 * Bindings menu extensibility manager holds a list of registered bindings menu extensions.
 */
class UMGEDITOR_API FPropertyBindingExtensibilityManager
{
public:
	void AddExtension(const TSharedRef<IPropertyBindingExtension>& Extension)
	{
		Extensions.AddUnique(Extension);
	}

	void RemoveExtension(const TSharedRef<IPropertyBindingExtension>& Extension)
	{
		Extensions.RemoveSingle(Extension);
	}

	const TArray<TSharedPtr<IPropertyBindingExtension>>& GetExtensions() const
	{
		return Extensions;
	}

private:
	TArray<TSharedPtr<IPropertyBindingExtension>> Extensions;
};

/** Indicates that a class has a bindings menu that is extensible */
class IHasPropertyBindingExtensibility
{
public:
	virtual TSharedPtr<FPropertyBindingExtensibilityManager> GetPropertyBindingExtensibilityManager() = 0;
};

