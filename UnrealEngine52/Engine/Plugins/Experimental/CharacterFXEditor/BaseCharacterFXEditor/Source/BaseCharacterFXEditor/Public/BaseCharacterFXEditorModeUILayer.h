// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Toolkits/AssetEditorModeUILayer.h"
#include "BaseCharacterFXEditorModeUILayer.generated.h"

/** Handles the hosting of additional toolkits, such as the mode toolkit, within the CharacterFXEditor's toolkit. **/

class BASECHARACTERFXEDITOR_API FBaseCharacterFXEditorModeUILayer : public FAssetEditorModeUILayer
{
public:
	FBaseCharacterFXEditorModeUILayer(const IToolkitHost* InToolkitHost);
	void OnToolkitHostingStarted(const TSharedRef<IToolkit>& Toolkit) override;
	void OnToolkitHostingFinished(const TSharedRef<IToolkit>& Toolkit) override;

	void SetModeMenuCategory(TSharedPtr<FWorkspaceItem> MenuCategoryIn);
	TSharedPtr<FWorkspaceItem> GetModeMenuCategory() const override;

protected:

	TSharedPtr<FWorkspaceItem> CharacterFXEditorMenuCategory;

};

/** 
 * Interchange layer to manage built in tab locations within the editor's layout. 
 */

UCLASS()
class BASECHARACTERFXEDITOR_API UBaseCharacterFXEditorUISubsystem : public UAssetEditorUISubsystem
{
	GENERATED_BODY()
public:

	// Adds RegisterLayoutExtensions as a callback to the module corresponding to GetModuleName()
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// Removes RegisterLayoutExtensions callback from the module corresponding to GetModuleName()
	virtual void Deinitialize() override;

	// Docks the editor Mode tab in the Editor side panel area of the layout
	virtual void RegisterLayoutExtensions(FLayoutExtender& Extender) override;

	// Identifier for the Layout extension in BaseCharacterFXEditorToolkit's StandaloneDefaultLayout
	static const FName EditorSidePanelAreaName;

protected:

	// OVERRIDE THIS
	virtual FName GetModuleName() const
	{
		return FName("");
	}

};
