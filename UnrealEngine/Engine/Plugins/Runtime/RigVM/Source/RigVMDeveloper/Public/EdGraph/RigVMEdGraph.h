// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EdGraph/EdGraph.h"
#include "EdGraph/RigVMEdGraphNode.h"
#include "RigVMModel/RigVMGraph.h"
#include "RigVMCore/RigVM.h"
#include "RigVMModel/RigVMClient.h"
#include "RigVMEdGraph.generated.h"

class URigVMBlueprint;
class URigVMEdGraphSchema;
class URigVMController;

DECLARE_MULTICAST_DELEGATE_OneParam(FRigVMEdGraphNodeClicked, URigVMEdGraphNode*);

UCLASS()
class RIGVMDEVELOPER_API URigVMEdGraph : public UEdGraph, public IRigVMEditorSideObject
{
	GENERATED_BODY()

public:
	URigVMEdGraph();

	/** IRigVMEditorSideObject interface */
	virtual FRigVMClient* GetRigVMClient() const override;
	virtual FString GetRigVMNodePath() const override;
	virtual void HandleRigVMGraphRenamed(const FString& InOldNodePath, const FString& InNewNodePath) override;

	/** Set up this graph */
	virtual void InitializeFromBlueprint(URigVMBlueprint* InBlueprint);

	/** Get the ed graph schema */
	const URigVMEdGraphSchema* GetRigVMEdGraphSchema();

#if WITH_EDITORONLY_DATA
	/** Customize blueprint changes based on backwards compatibility */
	virtual void Serialize(FArchive& Ar) override;
#endif

#if WITH_EDITOR

	bool bSuspendModelNotifications;
	bool bIsTemporaryGraphForCopyPaste;

	UEdGraphNode* FindNodeForModelNodeName(const FName& InModelNodeName, const bool bCacheIfRequired = true);

	URigVMBlueprint* GetBlueprint() const;
	URigVMGraph* GetModel() const;
	URigVMController* GetController() const;
	bool IsRootGraph() const { return GetRootGraph() == this; }
	const URigVMEdGraph* GetRootGraph() const;

	void HandleModifiedEvent(ERigVMGraphNotifType InNotifType, URigVMGraph* InGraph, UObject* InSubject);
	virtual bool HandleModifiedEvent_Internal(ERigVMGraphNotifType InNotifType, URigVMGraph* InGraph, UObject* InSubject);

	int32 GetInstructionIndex(const URigVMEdGraphNode* InNode, bool bAsInput);

	void CacheEntryNameList();
	const TArray<TSharedPtr<FString>>* GetEntryNameList(URigVMPin* InPin = nullptr) const;

	virtual const TArray<TSharedPtr<FString>>* GetNameListForWidget(const FString& InWidgetName) const { return nullptr; }

	UPROPERTY()
	FString ModelNodePath;

	UPROPERTY()
	bool bIsFunctionDefinition;

protected:
	using Super::AddNode;
	virtual void AddNode(UEdGraphNode* NodeToAdd, bool bUserAction = false, bool bSelectNewNode = true) override;

private:

	bool bIsSelecting;

	FRigVMEdGraphNodeClicked OnGraphNodeClicked;

	TMap<URigVMNode*, TPair<int32, int32>> CachedInstructionIndices;

	void RemoveNode(UEdGraphNode* InNode);

#endif
#if WITH_EDITORONLY_DATA

	UPROPERTY(transient)
	TObjectPtr<URigVMController> TemplateController;

#endif
#if WITH_EDITOR

	URigVMController* GetTemplateController();

protected:
	void HandleVMCompiledEvent(UObject* InCompiledObject, URigVM* InVM);

private:
	TMap<FName, UEdGraphNode*> ModelNodePathToEdNode;
	mutable TWeakObjectPtr<URigVMGraph> CachedModelGraph;
	TArray<TSharedPtr<FString>> EntryNameList;

#endif
	friend class URigVMEdGraphNode;
	friend class URigVMBlueprint;
	friend class FRigVMEditor;
	friend class SRigVMGraphNode;
	friend class URigVMBlueprint;

	friend class URigVMEdGraphUnitNodeSpawner;
	friend class URigVMEdGraphVariableNodeSpawner;
	friend class URigVMEdGraphParameterNodeSpawner;
	friend class URigVMEdGraphBranchNodeSpawner;
	friend class URigVMEdGraphIfNodeSpawner;
	friend class URigVMEdGraphSelectNodeSpawner;
	friend class URigVMEdGraphTemplateNodeSpawner;
	friend class URigVMEdGraphEnumNodeSpawner;
	friend class URigVMEdGraphFunctionRefNodeSpawner;
	friend class URigVMEdGraphArrayNodeSpawner;
	friend class URigVMEdGraphInvokeEntryNodeSpawner;
};
