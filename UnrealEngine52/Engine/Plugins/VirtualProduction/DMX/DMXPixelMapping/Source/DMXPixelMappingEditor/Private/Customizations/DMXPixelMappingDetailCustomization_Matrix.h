// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"
#include "Misc/Attribute.h"
#include "Layout/Visibility.h"
#include "UObject/WeakObjectPtr.h"

class FDMXPixelMappingToolkit;
struct FDMXCellAttributeGroup;
class UDMXPixelMappingMatrixComponent;
class UDMXEntityFixturePatch;

enum class EDMXColorMode : uint8;
class ITableRow;
class IPropertyHandle;
class IPropertyUtilities;
class STableViewBase;
template <typename ItemType> class SListView;


class FDMXPixelMappingDetailCustomization_Matrix
	: public IDetailCustomization
{
private:
	struct FDMXCellAttributeGroup
	{
		TSharedPtr<IPropertyHandle> Handle;
		TSharedPtr<IPropertyHandle> ExposeHandle;
		TSharedPtr<IPropertyHandle> InvertHandle;
		TAttribute<EVisibility> Visibility;
	};

public:
	static TSharedRef<IDetailCustomization> MakeInstance(const TWeakPtr<FDMXPixelMappingToolkit> InToolkitWeakPtr)
	{
		return MakeShared<FDMXPixelMappingDetailCustomization_Matrix>(InToolkitWeakPtr);
	}

	/** Constructor */
	FDMXPixelMappingDetailCustomization_Matrix(const TWeakPtr<FDMXPixelMappingToolkit> InToolkitWeakPtr)
		: ToolkitWeakPtr(InToolkitWeakPtr)
	{}

	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;

private:	
	/** Creates Details for the Output Modulators */
	void CreateModulatorDetails(IDetailLayoutBuilder& InDetailLayout);

	/** Forces the layout to redraw */
	void ForceRefresh();

	/** Matrix Components that are being edited */
	TArray<TWeakObjectPtr<UDMXPixelMappingMatrixComponent>> MatrixComponents;

	/** Property utilities for this customization */
	TSharedPtr<IPropertyUtilities> PropertyUtilities;

	/** Weak reference to the DMX editor */
	TWeakPtr<FDMXPixelMappingToolkit> ToolkitWeakPtr;
};
