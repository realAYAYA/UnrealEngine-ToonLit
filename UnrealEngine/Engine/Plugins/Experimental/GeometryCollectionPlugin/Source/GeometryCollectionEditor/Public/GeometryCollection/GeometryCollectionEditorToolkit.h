// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dataflow/DataflowNodeParameters.h"
#include "Dataflow/DataflowGraphEditor.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "GraphEditor.h"
#include "Toolkits/SimpleAssetEditor.h"
#include "Misc/NotifyHook.h"

class IDetailsView;
class FTabManager;
class IToolkitHost;
class UDataflow;


class FGeometryCollectionEditorToolkit : public FAssetEditorToolkit, public FNotifyHook, public FGCObject
{
public:
	void InitGeometryCollectionAssetEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UObject* ObjectToEdit);

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

	UDataflow* GetDataflow() { return Dataflow; }
	const UDataflow* GetDataflow() const { return Dataflow; }

	UGeometryCollection* GetGeometryCollection() { return GeometryCollection; }
	const UGeometryCollection* GetGeometryCollection() const { return GeometryCollection; }

	TSharedPtr<IDetailsView> GetAssetDetailsEditor() { return AssetDetailsEditor; }
	const TSharedPtr<IDetailsView> GetAssetDetailsEditor() const { return AssetDetailsEditor; }

	TSharedPtr<IStructureDetailsView> GetNodeDetailsEditor() { return NodeDetailsEditor; }
	const TSharedPtr<IStructureDetailsView> GetNodeDetailsEditor() const { return NodeDetailsEditor; }

	TSharedPtr<SGraphEditor> GetGraphEditor() { return GraphEditor; }
	const TSharedPtr<SGraphEditor> GetGraphEditor() const { return GraphEditor; }

private:
	static const FName GraphCanvasTabId;
	TSharedPtr<SGraphEditor> GraphEditor;
	TSharedRef<SGraphEditor> CreateGraphEditorWidget(UDataflow* ObjectToEdit, TSharedPtr<IStructureDetailsView> PropertiesEditor);

	static const FName AssetDetailsTabId;
	TSharedPtr<IDetailsView> AssetDetailsEditor;
	TSharedPtr<IDetailsView> CreateAssetDetailsEditorWidget(UObject* ObjectToEdit);

	static const FName NodeDetailsTabId;
	TSharedPtr<IStructureDetailsView> NodeDetailsEditor;
	TSharedPtr<IStructureDetailsView> CreateNodeDetailsEditorWidget(UObject* ObjectToEdit);

	UDataflow* Dataflow = nullptr;
	UGeometryCollection* GeometryCollection = nullptr;
};
