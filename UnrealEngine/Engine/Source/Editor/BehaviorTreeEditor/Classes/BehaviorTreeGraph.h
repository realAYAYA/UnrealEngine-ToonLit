// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AIGraph.h"
#include "Containers/Set.h"
#include "CoreMinimal.h"
#include "HAL/Platform.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "BehaviorTreeGraph.generated.h"

class UBehaviorTreeGraphNode_Root;
class UEdGraphNode;
class UObject;

UCLASS()
class BEHAVIORTREEEDITOR_API UBehaviorTreeGraph : public UAIGraph
{
	GENERATED_UCLASS_BODY()

	enum EUpdateFlags
	{
		RebuildGraph = 0,
		ClearDebuggerFlags = 1,
		KeepRebuildCounter = 2,
	};

	/** increased with every graph rebuild, used to refresh data from subtrees */
	UPROPERTY()
	int32 ModCounter;

	UPROPERTY()
	bool bIsUsingModCounter;

	UPROPERTY()
	TSubclassOf<UBehaviorTreeGraphNode_Root> RootNodeClass;

	virtual void OnCreated() override;
	virtual void OnLoaded() override;
	virtual void Initialize() override;
	void OnSave();

	virtual void UpdateVersion() override;
	virtual void MarkVersion() override;
	virtual void UpdateAsset(int32 UpdateFlags = 0) override;
	virtual void OnSubNodeDropped() override;

	virtual bool DoesSupportServices() const { return true; }

	void UpdateBlackboardChange();
	void UpdateAbortHighlight(struct FAbortDrawHelper& Mode0, struct FAbortDrawHelper& Mode1);
	void CreateBTFromGraph(class UBehaviorTreeGraphNode* RootEdNode);
	void SpawnMissingNodes();
	void UpdatePinConnectionTypes();
	bool UpdateInjectedNodes();
	void UpdateBrokenComposites();
	class UEdGraphNode* FindInjectedNode(int32 Index);
	void ReplaceNodeConnections(UEdGraphNode* OldNode, UEdGraphNode* NewNode);
	void RebuildExecutionOrder();
	void RebuildChildOrder(UEdGraphNode* ParentNode);
	void SpawnMissingNodesForParallel();
	void RemoveUnknownSubNodes();

	void AutoArrange();

protected:

	void CollectAllNodeInstances(TSet<UObject*>& NodeInstances) override;

#if WITH_EDITOR
	virtual void PostEditUndo() override;
#endif

	void UpdateVersion_UnifiedSubNodes();
	void UpdateVersion_InnerGraphWhitespace();
	void UpdateVersion_RunBehaviorInSeparateGraph();
};
