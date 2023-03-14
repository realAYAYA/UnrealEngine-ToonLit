// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "Misc/Attribute.h"
#include "UObject/WeakObjectPtr.h"
#include "IPropertyTypeCustomization.h"
#include "Layout/Visibility.h"

class FDMXPixelMappingToolkit;
class IDetailLayoutBuilder;
class ITableRow;
class STableViewBase;
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

	EVisibility GetRGBAttributeRowVisibilty(FFunctionAttribute* Attribute) const;

	EVisibility GetRGBAttributesVisibility() const;

	EVisibility GetMonochromeRowVisibilty(FFunctionAttribute* Attribute) const;

	EVisibility GetMonochromeAttributesVisibility() const;

	TSharedRef<ITableRow> GenerateExposeAndInvertRow(TSharedPtr<FFunctionAttribute> InAttribute, const TSharedRef<STableViewBase>& OwnerTable);

private:
	bool CheckComponentsDMXColorMode(const EDMXColorMode DMXColorMode) const;

	/** Creates Details for the Output Modulators */
	void CreateModulatorDetails(IDetailLayoutBuilder& InDetailLayout);

	/** Forces the layout to redraw */
	void ForceRefresh();

	IDetailLayoutBuilder* DetailLayout;

	TArray<TWeakObjectPtr<UDMXPixelMappingFixtureGroupItemComponent>> FixtureGroupItemComponents;

	TArray<TSharedPtr<FName>> ActiveModeFunctions;

	TArray<TSharedPtr<FFunctionAttribute>> RGBAttributes;

	TArray<TSharedPtr<FFunctionAttribute>> MonochromeAttributes;

	TSharedPtr<SListView<TSharedPtr<FFunctionAttribute>>> ExposeAndInvertListView;

	TSharedPtr<IPropertyUtilities> PropertyUtilities;

	/** Weak reference to the DMX editor */
	TWeakPtr<FDMXPixelMappingToolkit> ToolkitWeakPtr;
};
