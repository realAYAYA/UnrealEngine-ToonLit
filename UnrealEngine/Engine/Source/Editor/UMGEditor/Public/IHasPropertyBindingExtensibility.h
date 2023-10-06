// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Input/DragAndDrop.h"
#include "Misc/Optional.h"
#include "Templates/SharedPointer.h"

class IPropertyHandle;
class UWidget;
class UWidgetBlueprint;
struct FSlateBrush;

class UMGEDITOR_API IPropertyBindingExtension
{
public:
	enum class EDropResult
	{
		HandledContinue,	// The drop event was handled by the OnDrop call and it can still be re-used for OnDrop calls in other extensions.
		HandledBreak,		// The drop event was handled by the OnDrop call but it is not reusable for OnDrop calls in other extensions (e.g. it was modified).
		Unhandled,			// The drop event was not handled by this extension.
	};

	/** Does this extension want to extend this property in the widget? */
	virtual bool CanExtend(const UWidgetBlueprint* WidgetBlueprint, const UWidget* Widget, const FProperty* Property) const = 0;
	virtual TSharedPtr<FExtender> CreateMenuExtender(const UWidgetBlueprint* WidgetBlueprint, UWidget* Widget, TSharedPtr<IPropertyHandle> WidgetPropertyHandle) = 0;
	virtual void ClearCurrentValue(const UWidgetBlueprint* WidgetBlueprint, const UWidget* Widget, const FProperty* Property) = 0;
	virtual TOptional<FName> GetCurrentValue(const UWidgetBlueprint* WidgetBlueprint, const UWidget* Widget, const FProperty* Property) const = 0;
	virtual const FSlateBrush* GetCurrentIcon(const UWidgetBlueprint* WidgetBlueprint, const UWidget* Widget, const FProperty* Property) const = 0;
	virtual EDropResult OnDrop(const FGeometry& Geometry, const FDragDropEvent& DragDropEvent, UWidgetBlueprint* WidgetBlueprint, UWidget* Widget, TSharedPtr<IPropertyHandle> WidgetPropertyHandle) = 0;

	UE_DEPRECATED(5.3, "The function CreateMenuExtender with FProperty parameter is deprecated. Please pass the property handler as the parameter instead.")
	virtual TSharedPtr<FExtender> CreateMenuExtender(const UWidgetBlueprint* WidgetBlueprint, const UWidget* Widget, const FProperty* Property) = 0;
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

