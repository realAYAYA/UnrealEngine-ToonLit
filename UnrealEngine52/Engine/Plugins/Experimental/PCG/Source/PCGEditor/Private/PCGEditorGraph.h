// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EdGraph/EdGraph.h"

#include "PCGGraph.h"

#include "PCGEditorGraph.generated.h"

class FPCGEditor;
class UPCGNode;
class UPCGEditorGraphNodeBase;

UCLASS()
class UPCGEditorGraph : public UEdGraph
{
	GENERATED_BODY()

public:

	// ~Begin UObject interface
	virtual void BeginDestroy() override;
	// ~End UObject interface

	/** Initialize the editor graph from a PCGGraph */
	void InitFromNodeGraph(UPCGGraph* InPCGGraph);

	/** When the editor is closing */
	void OnClose();

	/** Creates the links for a given node */
	void CreateLinks(UPCGEditorGraphNodeBase* InGraphNode, bool bCreateInbound, bool bCreateOutbound);

	UPCGGraph* GetPCGGraph() { return PCGGraph; }

	void SetEditor(TWeakPtr<const FPCGEditor> InEditor) { PCGEditor = InEditor; }
	TWeakPtr<const FPCGEditor> GetEditor() const { return PCGEditor; }

protected:
	void CreateLinks(UPCGEditorGraphNodeBase* InGraphNode, bool bCreateInbound, bool bCreateOutbound, const TMap<UPCGNode*, UPCGEditorGraphNodeBase*>& InGraphNodeToPCGNodeMap);

	void OnGraphUserParametersChanged(UPCGGraphInterface* InGraph, EPCGGraphParameterEvent ChangeType, FName ChangedPropertyName);

private:
	UPROPERTY()
	TObjectPtr<UPCGGraph> PCGGraph = nullptr;

	TWeakPtr<const FPCGEditor> PCGEditor = nullptr;
};
