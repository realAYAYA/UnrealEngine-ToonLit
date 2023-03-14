// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "Types/SlateEnums.h"

struct FDMXEntityFixturePatchRef;
class FDMXPixelMappingToolkit;
class SDMXPixelMappingFixturePatchDetailRow;
class UDMXEntityFixturePatch;
class UDMXLibrary;
class UDMXPixelMapping;
class UDMXPixelMappingBaseComponent;
class UDMXPixelMappingFixtureGroupComponent;

class FReply;
struct FPointerEvent;
struct FGeometry;
class IDetailLayoutBuilder;
class IPropertyHandle;
class IPropertyUtilities;
class SBorder;
template <typename ItemType> class SListView;


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
	/** Creates a button that adds all patches to the group */
	void CreateAddAllPatchesButton(IDetailLayoutBuilder& InDetailLayout);

	/** Creates a list of fixture patches available to drag drop as detail rws */
	void CreateFixturePatchDetailRows(IDetailLayoutBuilder& InDetailLayout);

	/** Called when the 'Add all Patches' button was clicked */
	FReply OnAddAllPatchesClicked();

	/** Called when the library changed */
	void OnLibraryChanged();

	/** Called when a component was added */
	void OnComponentAdded(UDMXPixelMapping* PixelMapping, UDMXPixelMappingBaseComponent* Component);

	/** Called when a component was removed */
	void OnComponentRemoved(UDMXPixelMapping* PixelMapping, UDMXPixelMappingBaseComponent* Component);

	/** Called before the SizeX property changed */
	void OnSizePropertyPreChange();

	/** Called when the SizeX property changed */
	void OnSizePropertyChanged(const FPropertyChangedEvent& PropertyChangedEvent);

	/** Handles the size property changed, useful to call on tick */
	void HandleSizePropertyChanged();

	/** Forces the detail layout to refresh */
	void ForceRefresh();

	/** Called when a fixture patch widget got a mouse down event */
	void OnFixturePatchLMBDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, FDMXEntityFixturePatchRef FixturePatchRef);

	/** Called when a fixture patch widget got a mouse up event */
	void OnFixturePatchLMBUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, FDMXEntityFixturePatchRef FixturePatchRef);

	/** Called when a fixture patch was dragged */
	FReply OnFixturePatchesDragged(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);

	/** Returns true if the fixture patch is used in the pixel mapping */
	bool IsFixturePatchAssignedToPixelMapping(UDMXEntityFixturePatch* FixturePatch) const;

	/** Updates highlights for the fixture patches (selection) */
	void UpdateFixturePatchHighlights();

	/** Update fixture patches in use */
	void UpdateFixturePatchesInUse(UDMXLibrary* DMXLibrary);

	/** Updates the bCachedScaleChildrenWithParent member from UDMXPixelMappingLayoutSettings */
	void UpdateCachedScaleChildrenWithParent();

	/** Helper that returns the library selected in for the group */
	UDMXLibrary* GetSelectedDMXLibrary(UDMXPixelMappingFixtureGroupComponent* FixtureGroupComponent) const;

	/** Returns the currently selected fixture group */
	UDMXPixelMappingFixtureGroupComponent* GetSelectedFixtureGroupComponent(const IDetailLayoutBuilder& InDetailLayout) const;

	/** Weak reference to the DMX editor */
	TWeakPtr<FDMXPixelMappingToolkit> ToolkitWeakPtr;

	/** Fixture patches currently in the library */
	TArray<FDMXEntityFixturePatchRef> FixturePatches;

	/** Fixture patches currently in the library and selected */
	TArray<FDMXEntityFixturePatchRef> SelectedFixturePatches;

	/** The single fixture group component in use */
	TWeakObjectPtr<UDMXPixelMappingFixtureGroupComponent> WeakFixtureGroupComponent;

	/** Number of fixture patch rows displayed */
	int32 NumFixturePatchRows = 0;

	/** SizeX before it got changed */
	TMap<TWeakObjectPtr<UDMXPixelMappingFixtureGroupComponent>, FVector2D> PreEditChangeComponentToSizeMap;

	/** Cached UDMXPixelMappingLayoutSettings::bScaleChildrenWithParen, to avoid repetitive reads during interactive changes */
	bool bCachedScaleChildrenWithParent = false;

	/** Handle to the dmx library property */
	TSharedPtr<IPropertyHandle> DMXLibraryHandle;

	/** Handle to the dmx library's entity array */
	TSharedPtr<IPropertyHandle> EntitiesHandle;

	/** Property handle for the SizeX property */
	TSharedPtr<IPropertyHandle> SizeXHandle;

	/** Property handle for the SizeY property */
	TSharedPtr<IPropertyHandle> SizeYHandle;

	/** Patches with their details row. Helps to multiselect and toggle highlights */
	struct FDetailRowWidgetWithPatch
	{
		TWeakObjectPtr<UDMXEntityFixturePatch> WeakFixturePatch;
		TSharedPtr<SDMXPixelMappingFixturePatchDetailRow> DetailRowWidget;
	};
	TArray<FDetailRowWidgetWithPatch> DetailRowWidgetsWithPatch;

	/** Delegate handle while being bound to the OnEndFrame event */
	FDelegateHandle RequestForceRefreshHandle;

	TSharedPtr<IPropertyUtilities> PropertyUtilities;
};
