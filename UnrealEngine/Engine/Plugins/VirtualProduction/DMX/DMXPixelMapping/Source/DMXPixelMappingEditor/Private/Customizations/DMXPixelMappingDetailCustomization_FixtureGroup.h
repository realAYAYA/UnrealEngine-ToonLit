// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"
#include "Library/DMXEntityReference.h"

struct FPropertyChangedEvent;

class FDMXPixelMappingToolkit;
class UDMXPixelMapping;
class UDMXPixelMappingBaseComponent;
class UDMXPixelMappingFixtureGroupComponent;

class IDetailLayoutBuilder;
class IPropertyHandle;
class IPropertyUtilities;


class FDMXPixelMappingDetailCustomization_FixtureGroup
	: public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance(const TWeakPtr<FDMXPixelMappingToolkit> InToolkitWeakPtr)
	{
		return MakeShared<FDMXPixelMappingDetailCustomization_FixtureGroup>(InToolkitWeakPtr);
	}

	/** Constructor */
	FDMXPixelMappingDetailCustomization_FixtureGroup(const TWeakPtr<FDMXPixelMappingToolkit> InToolkitWeakPtr)
		: ToolkitWeakPtr(InToolkitWeakPtr)
	{}

	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;

private:
	/** Called when a component was added or removed */
	void OnComponentAddedOrRemoved(UDMXPixelMapping* PixelMapping, UDMXPixelMappingBaseComponent* Component);

	/** Called before the SizeX property changed */
	void OnSizePropertyPreChange();

	/** Called when the SizeX property changed */
	void OnSizePropertyChanged(const FPropertyChangedEvent& PropertyChangedEvent);

	/** Handles the size property changed, useful to call on tick */
	void HandleSizePropertyChanged();

	/** Weak reference to the DMX editor */
	TWeakPtr<FDMXPixelMappingToolkit> ToolkitWeakPtr;

	/** SizeX before it got changed */
	TMap<TWeakObjectPtr<UDMXPixelMappingFixtureGroupComponent>, FVector2D> PreEditChangeComponentToSizeMap;

	/** Property handle for the SizeX property */
	TSharedPtr<IPropertyHandle> SizeXHandle;

	/** Property handle for the SizeY property */
	TSharedPtr<IPropertyHandle> SizeYHandle;

	TSharedPtr<IPropertyUtilities> PropertyUtilities;
};
