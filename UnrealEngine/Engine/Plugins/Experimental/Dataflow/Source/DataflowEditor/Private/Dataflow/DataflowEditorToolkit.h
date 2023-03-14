// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/SimpleAssetEditor.h"
#include "Misc/NotifyHook.h"
#include "GraphEditor.h"

class IDetailsView;
class FTabManager;
class IToolkitHost;
class UDataflow;

class FDataflowEditorToolkit : public FAssetEditorToolkit, public FNotifyHook, public FGCObject
{
public:
	void InitDataflowEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UObject* ObjectToEdit);

	// IToolkit Interface
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;

	// FGCObject Interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override; 

	// Tab spawners 
	virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& TabManager) override;
	TSharedRef<SDockTab> SpawnTab_GraphCanvas(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_AssetDetails(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_NodeDetails(const FSpawnTabArgs& Args);

	// Member Access
	UDataflow* GetDataflow() { return Dataflow; }
	const UDataflow* GetDataflow() const { return Dataflow; }

	TSharedPtr<IDetailsView> GetAssetDetailsEditor() {return AssetDetailsEditor;}
	const TSharedPtr<IDetailsView> GetAssetDetailsEditor() const { return AssetDetailsEditor; }

	TSharedPtr<IStructureDetailsView> GetNodeDetailsEditor() { return NodeDetailsEditor; }
	const TSharedPtr<IStructureDetailsView> GetNodeDetailsEditor() const { return NodeDetailsEditor; }

	TSharedPtr<SGraphEditor> GetGraphEditor() { return GraphEditor; }
	const TSharedPtr<SGraphEditor> GetGraphEditor() const { return GraphEditor; }

private:

	UDataflow* Dataflow = nullptr;

	static const FName GraphCanvasTabId;
	TSharedPtr<SGraphEditor> GraphEditor;
	TSharedPtr<FUICommandList> GraphEditorCommands;
	TSharedRef<SGraphEditor> CreateGraphEditorWidget(UDataflow* ObjectToEdit, TSharedPtr<IStructureDetailsView> PropertiesEditor);

	static const FName AssetDetailsTabId;
	TSharedPtr<IDetailsView> AssetDetailsEditor;
	TSharedPtr<IDetailsView> CreateAssetDetailsEditorWidget(UObject* ObjectToEdit);

	static const FName NodeDetailsTabId;
	TSharedPtr<IStructureDetailsView> NodeDetailsEditor;
	TSharedPtr<IStructureDetailsView> CreateNodeDetailsEditorWidget(UObject* ObjectToEdit);
};
