// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BaseCharacterFXEditor.h"

#include "Dataflow/DataflowEngine.h"
#include "Dataflow/AssetDefinition_DataflowAsset.h"
#include "Dataflow/DataflowAssetFactory.h"
#include "Dataflow/DataflowEditorCommands.h"
#include "Dataflow/DataflowContent.h"
#include "Dataflow/DataflowSchema.h"
#include "Dataflow/DataflowSNode.h"
#include "Dataflow/DataflowSNodeFactories.h"
#include "Templates/SharedPointer.h"

#include "DataflowEditor.generated.h"

class FDataflowEditorToolkit;
class UDataflowBaseContent;

/** 
 * The actual asset editor class doesn't have that much in it, intentionally. 
 * 
 * Our current asset editor guidelines ask us to place as little business logic as possible
 * into the class, instead putting as much of the non-UI code into the subsystem as possible,
 * and the UI code into the toolkit (which this class owns).
 *
 * However, since we're using a mode and the Interactive Tools Framework, a lot of our business logic
 * ends up inside the mode and the tools, not the subsystem. The front-facing code is mostly in
 * the asset editor toolkit, though the mode toolkit has most of the things that deal with the toolbar
 * on the left.
 */

UCLASS()
class DATAFLOWEDITOR_API UDataflowEditor : public UBaseCharacterFXEditor
{
	GENERATED_BODY()

public:
	UDataflowEditor();

	// UBaseCharacterFXEditor interface
	virtual TSharedPtr<FBaseAssetToolkit> CreateToolkit() override;
	virtual void Initialize(const TArray<TObjectPtr<UObject>>& InObjects) override;

	/** Initialize an editor with a given content and an owner */
	void InitializeContent(TObjectPtr<UDataflowBaseContent> BaseContent, const TObjectPtr<UObject>& ContentOwner);

private :

	friend class FDataflowEditorToolkit;
	
	// Dataflow editor is the owner of the object list to edit/process and the dataflow mode
	// is the one holding the dynamic mesh components to be rendered in the viewport
	// It is why the data flow asset/owner/skelmesh have been added here. Could be added
	// in the subsystem if necessary
	UPROPERTY()
	TObjectPtr<UDataflowBaseContent> DataflowContent;
};

DECLARE_LOG_CATEGORY_EXTERN(LogDataflowEditor, Log, All);