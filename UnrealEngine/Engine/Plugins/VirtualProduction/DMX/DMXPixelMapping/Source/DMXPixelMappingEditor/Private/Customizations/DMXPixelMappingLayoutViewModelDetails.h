// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"

class FDMXPixelMappingToolkit;
class UDMXPixelMappingLayoutViewModel;

class IPropertyUtilities;


/** Details customization for the Layout View Model */
class FDMXPixelMappingLayoutViewModelDetails
	: public IDetailCustomization
{
public:
	/** Creates an instance of this customization */
	static TSharedRef<IDetailCustomization> MakeInstance(const TWeakPtr<FDMXPixelMappingToolkit> InToolkitWeakPtr);

	/** Constructor */
	FDMXPixelMappingLayoutViewModelDetails(const TWeakPtr<FDMXPixelMappingToolkit> InToolkitWeakPtr)
		: WeakToolkit(InToolkitWeakPtr)
	{}

	//~ Begin IDetailCustomization interface 
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;
	//~ End IDetailCustomization interface 

protected:
	/** Retuns the object that is being customized */
	UDMXPixelMappingLayoutViewModel* GetLayoutViewModel() const;

	/** Weak reference to the DMX editor */
	TWeakPtr<FDMXPixelMappingToolkit> ToolkitWeakPtr;

	/** If true, shows the Layout Script Class property */
	bool bShowLayoutScriptClass = false;

	/** If true, shows an info why the Layout Script Class property is hidden */
	bool bShowLayoutScriptClassHiddenInfo = true;

	/** Property utilities for this customization */
	TSharedPtr<IPropertyUtilities> PropertyUtilities;

	/** Weak reference to the Pixel Mapping Toolkit */
	TWeakPtr<FDMXPixelMappingToolkit> WeakToolkit;
};
