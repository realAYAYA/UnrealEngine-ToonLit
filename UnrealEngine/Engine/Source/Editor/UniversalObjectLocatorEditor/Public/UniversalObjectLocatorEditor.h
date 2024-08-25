// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"

struct FSlateIcon;

class FText;
class UObject;
class SWidget;
class FDragDropOperation;
class IPropertyHandle;
class IDetailChildrenBuilder;
class IPropertyTypeCustomizationUtils;
struct FUniversalObjectLocator;

namespace UE::UniversalObjectLocator
{

class IUniversalObjectLocatorCustomization;

class UNIVERSALOBJECTLOCATOREDITOR_API ILocatorEditor : public TSharedFromThis<ILocatorEditor>
{
public:
	virtual ~ILocatorEditor() = default;

	virtual bool IsDragSupported(TSharedPtr<FDragDropOperation> DragOperation, UObject* Context) const = 0;

	virtual UObject* ResolveDragOperation(TSharedPtr<FDragDropOperation> DragOperation, UObject* Context) const = 0;

	virtual TSharedPtr<SWidget> MakeEditUI(TSharedPtr<IUniversalObjectLocatorCustomization> Customization) = 0;

	virtual	void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) {}

	virtual FText GetDisplayText() const = 0;
	
	virtual FText GetDisplayTooltip() const = 0;
	
	virtual FSlateIcon GetDisplayIcon() const = 0;

	// Make a default UOL for the given fragment type if applicable
	virtual FUniversalObjectLocator MakeDefaultLocator() const;
};


} // namespace UE::UniversalObjectLocator