// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Tools/UAssetEditor.h"

#include "UVEditor.generated.h"


class FUICommandList;
class UEdModeInteractiveToolsContext;
class UInteractiveToolBuilder;
class UPreviewMesh;
class UUVEditorToolTargetManager;
class UToolTarget;
class UUVEditorMode;
class IUVUnwrapDynamicMesh;
class IAssetEditorInstance;

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
class UVEDITOR_API UUVEditor : public UAssetEditor
{
	GENERATED_BODY()

public:
	void Initialize(const TArray<TObjectPtr<UObject>>& InObjects);

	/** Returns the asset editor instance interface, so that its window can be focused, for example. */
	IAssetEditorInstance* GetInstanceInterface();

	// UAssetEditor overrides
	void GetObjectsToEdit(TArray<UObject*>& OutObjects) override;
	virtual TSharedPtr<FBaseAssetToolkit> CreateToolkit() override;

	// Note that things like CloseWindow, FocusWindow, and IsPrimaryEditor seem to be called
	// on the toolkit, not the editor class, so they are overriden there.

protected:
	
	UPROPERTY()
	TArray<TObjectPtr<UObject>> OriginalObjectsToEdit;

	friend class UUVEditorSubsystem;
};
