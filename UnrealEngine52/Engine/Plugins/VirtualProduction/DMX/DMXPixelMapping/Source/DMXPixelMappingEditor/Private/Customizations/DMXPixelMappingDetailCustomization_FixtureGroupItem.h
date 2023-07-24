// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"
#include "Layout/Visibility.h"
#include "Misc/Attribute.h"
#include "UObject/WeakObjectPtr.h"


class IPropertyUtilities;

class FDMXPixelMappingToolkit;
class IDetailLayoutBuilder;
class IPropertyHandle;
class UDMXPixelMappingFixtureGroupItemComponent;
enum class EDMXColorMode : uint8;

class IPropertyUitilities;
template <typename ItemType> class SListView;


class FDMXPixelMappingDetailCustomization_FixtureGroupItem
	: public IDetailCustomization
{
private:
	struct FFunctionAttribute
	{
		TSharedPtr<IPropertyHandle> Handle;
		TSharedPtr<IPropertyHandle> ExposeHandle;
		TSharedPtr<IPropertyHandle> InvertHandle;
		TAttribute<EVisibility> Visibility;
	};


public:
	static TSharedRef<IDetailCustomization> MakeInstance(const TWeakPtr<FDMXPixelMappingToolkit> InToolkitWeakPtr)
	{
		return MakeShared<FDMXPixelMappingDetailCustomization_FixtureGroupItem>(InToolkitWeakPtr);
	}

	/** Constructor */
	FDMXPixelMappingDetailCustomization_FixtureGroupItem(const TWeakPtr<FDMXPixelMappingToolkit> InToolkitWeakPtr)
		: ToolkitWeakPtr(InToolkitWeakPtr)
	{}

	//~ IDetailCustomization interface begin
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;
	//~ IPropertyTypeCustomization interface end

private:
	/** Creates Details for the Output Modulators */
	void CreateModulatorDetails(IDetailLayoutBuilder& InDetailLayout);

	/** Forces the layout to redraw */
	void ForceRefresh();

	/** Fixture Group Item Components that are being edited */
	TArray<TWeakObjectPtr<UDMXPixelMappingFixtureGroupItemComponent>> FixtureGroupItemComponents;

	/** Property utilities for this customization */
	TSharedPtr<IPropertyUtilities> PropertyUtilities;

	/** Weak reference to the DMX editor */
	TWeakPtr<FDMXPixelMappingToolkit> ToolkitWeakPtr;
};
