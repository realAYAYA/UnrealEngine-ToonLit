// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BaseCharacterFXEditorModeUILayer.h"
#include "ClothEditorModeUILayer.generated.h"

/** Interchange layer to manage built in tab locations within the editor's layout. **/
UCLASS()
class CHAOSCLOTHASSETEDITOR_API UChaosClothAssetEditorUISubsystem : public UAssetEditorUISubsystem
{
	GENERATED_BODY()
public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void RegisterLayoutExtensions(FLayoutExtender& Extender) override;

	// Identifier for the Layout extension in ChaosClothAssetEditorToolkit's StandaloneDefaultLayout
	static const FName EditorSidePanelAreaName;

};

/** Handles the hosting of additional toolkits, such as the mode toolkit, within the ChaosClothAssetEditor's toolkit. **/
class FChaosClothAssetEditorModeUILayer : public FBaseCharacterFXEditorModeUILayer
{
public:
	FChaosClothAssetEditorModeUILayer(const IToolkitHost* InToolkitHost) : FBaseCharacterFXEditorModeUILayer(InToolkitHost) {}
};

