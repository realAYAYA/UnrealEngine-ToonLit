// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/GCObject.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class UDMXEntity;
class UDMXEntityFixturePatch;
class UDMXLibrary;

template<typename OptionType> class SComboBox;
class STextBlock;


/**
 * A Monitor for DMX activity in a range of DMX Universes
 */
class SDMXPatchTool
	: public SCompoundWidget
	, public FGCObject
{
public:
	SLATE_BEGIN_ARGS(SDMXPatchTool)
	{}

	SLATE_END_ARGS()

	/** Destructor */
	virtual ~SDMXPatchTool();

	/** Constructs the widget */
	void Construct(const FArguments& InArgs);

protected:
	// ~Begin FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("SDMXPatchTool");
	}
	// ~End FGCObject interface

	/** Updates the selected library. Useful on initialization or when assets changed */
	void UpdateLibrarySelection();

	/** Updates the selected fixture patch. Useful when the library changed */
	void UpdateFixturePatchSelection();

	/** Called when the Address Incremental Button was clicked */
	FReply OnAddressIncrementalClicked();

	/** Called when the Address Same Button was clicked */
	FReply OnAddressSameClicked();

	/** Called when the Address And Rename Button was clicked */
	FReply OnAddressAndRenameClicked();

private:
	/** Generates an entry in the library combo box */
	TSharedRef<SWidget> GenerateLibraryComboBoxEntry(UDMXLibrary* LibraryToAdd);

	/** Called when a dmx library was slected */
	void OnLibrarySelected(UDMXLibrary* SelectedLibrary, ESelectInfo::Type SelectInfo);

	/** Combobox to select a library */
	TSharedPtr<SComboBox<UDMXLibrary*>> LibraryComboBox;

	/** Text block showing the selected library */
	TSharedPtr<STextBlock> SelectedLibraryTextBlock;

	/** Source for the library combo box */
	TArray<UDMXLibrary*> LibrarySource;

private:
	/** Generates an entry in the library combo box */
	TSharedRef<SWidget> GenerateFixturePatchComboBoxEntry(UDMXEntityFixturePatch* FixturePatchToAdd);

	/** Called when a fixture patch was slected */
	void OnFixturePatchSelected(UDMXEntityFixturePatch* SelectedFixturePatch, ESelectInfo::Type SelectInfo);

	/** Combobox to select a patch within the library */
	TSharedPtr<SComboBox<UDMXEntityFixturePatch*>> FixturePatchComboBox;

	/** Text block showing the selected fixture patch */
	TSharedPtr<STextBlock> SelectedFixturePatchTextBlock;

	/** Source for the fixture patch combo box */
	TArray<UDMXEntityFixturePatch*> FixturePatchSource;

private:
	/** Called when the library was edited */
	void OnEntitiesAddedOrRemoved(UDMXLibrary* Library, TArray<UDMXEntity*> Entities);

	/** Called when the asset registry finished loading files */
	void OnAllDMXLibraryAssetsLoaded();

	/** Called when a dmx library asset was added */
	void OnDMXLibraryAssetAdded(UDMXLibrary* DMXLibrary);

	/** Called when a dmx library asset was removed */
	void OnDMXLibraryAssetRemoved(UDMXLibrary* DMXLibrary);

	/** The previously selected library, to unbind from library changes */
	TWeakObjectPtr<UDMXLibrary> PreviouslySelectedLibrary;
};
