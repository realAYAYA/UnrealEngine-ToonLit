// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SDMXEntityEditor.h"

#include "CoreMinimal.h"

class FDMXEditor;
class FDMXFixturePatchSharedData;
class SDMXFixturePatcher;
class SDMXFixturePatchTree;
class SDMXMVRFixtureList;
class UDMXEntityFixturePatch;

struct FPropertyChangedEvent;
class IDetailsView;


/** Editor for Fixture Patches */
class SDMXFixturePatchEditor
	: public SDMXEntityEditor
{
public:
	SLATE_BEGIN_ARGS(SDMXFixturePatchEditor)
		: _DMXEditor(nullptr)
	{}
	
		SLATE_ARGUMENT(TWeakPtr<FDMXEditor>, DMXEditor)

	SLATE_END_ARGS()

public:	
	/** Constructs this widget */
	void Construct(const FArguments& InArgs);

public:
	//~ Begin SWidget interface
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	//~ End SWidget interface

	// Begin SDMXEntityEditorTab interface
	void RequestRenameOnNewEntity(const UDMXEntity* InEntity, ESelectInfo::Type SelectionType);
	void SelectEntity(UDMXEntity* InEntity, ESelectInfo::Type InSelectionType = ESelectInfo::Type::Direct);
	void SelectEntities(const TArray<UDMXEntity*>& InEntities, ESelectInfo::Type SelectionType = ESelectInfo::Type::Direct);
	TArray<UDMXEntity*> GetSelectedEntities() const;
	// ~End SDMXEntityEditorTab interface 

private:
	/** Selects the patch */
	void SelectUniverse(int32 UniverseID);

	/** Called whewn Fixture Patches were selected in Fixture Patch Shared Data */
	void OnFixturePatchesSelected();

	/** Generates a Detail View for the edited Fixture Patch */
	TSharedRef<IDetailsView> GenerateFixturePatchDetailsView() const;

	/** List of Fixture Patches as MVR Fixtures */
	TSharedPtr<SDMXMVRFixtureList> MVRFixtureList;

	/** Details View for the selected Fixture Patches */
	TSharedPtr<IDetailsView> FixturePatchDetailsView;

	/** Widget where the user can drag drop fixture patches */
	TSharedPtr<SDMXFixturePatcher> FixturePatcher;

	/** Shared data for Fixture Patches */
	TSharedPtr<FDMXFixturePatchSharedData> FixturePatchSharedData;

	/** Pointer back to the DMXEditor tool that owns us */
	TWeakPtr<FDMXEditor> DMXEditorPtr;
}; 
