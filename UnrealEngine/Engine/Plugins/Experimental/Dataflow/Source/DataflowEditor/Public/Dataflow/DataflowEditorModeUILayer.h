// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BaseCharacterFXEditorModeUILayer.h"
#include "DataflowEditorModeUILayer.generated.h"

/** Interchange layer to manage built in tab locations within the editor's layout. **/
UCLASS()
class DATAFLOWEDITOR_API UDataflowEditorUISubsystem : public UAssetEditorUISubsystem
{
	GENERATED_BODY()
public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void RegisterLayoutExtensions(FLayoutExtender& Extender) override;

	// Identifier for the Layout extension in DataflowEditorToolkit's StandaloneDefaultLayout
	static const FName EditorSidePanelAreaName;

};

/** Handles the hosting of additional toolkits, such as the mode toolkit, within the DataflowEditor's toolkit. **/
class FDataflowEditorModeUILayer : public FBaseCharacterFXEditorModeUILayer
{
public:
	FDataflowEditorModeUILayer(const IToolkitHost* InToolkitHost) : FBaseCharacterFXEditorModeUILayer(InToolkitHost) {}
};

