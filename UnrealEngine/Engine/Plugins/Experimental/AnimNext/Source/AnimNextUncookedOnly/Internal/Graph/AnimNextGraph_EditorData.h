// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimNextGraph_Controller.h"
#include "RigVMModel/RigVMGraph.h"
#include "AnimNextGraph_EdGraph.h"
#include "AnimNextRigVMAssetEditorData.h"
#include "Graph/AnimNextExecuteContext.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AnimNextGraph_EditorData.generated.h"

class UAnimNextGraph;
enum class ERigVMGraphNotifType : uint8;
class FAnimationAnimNextRuntimeTest_GraphAddDecorator;
class FAnimationAnimNextRuntimeTest_GraphExecute;
class FAnimationAnimNextRuntimeTest_GraphExecuteLatent;

namespace UE::AnimNext::UncookedOnly
{
	struct FUtils;
}

namespace UE::AnimNext::Editor
{
	class FGraphEditor;
	class SAnimNextGraphView;
	struct FUtils;
}

/**
 * The Schema is used to determine which actions are allowed
 * on a graph. This includes any topological change.
 */
UCLASS()
class UAnimNextGraph_Schema : public URigVMSchema
{
	GENERATED_BODY()
};

// Script-callable editor API hoisted onto UAnimNextGraph
UCLASS()
class ANIMNEXTUNCOOKEDONLY_API UAnimNextGraphLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	/** Adds a graph to an AnimNext Graph asset */
	UFUNCTION(BlueprintCallable, Category = "AnimNext|Graph", meta=(ScriptMethod))
	static UAnimNextGraphEntry* AddGraph(UAnimNextGraph* InGraph, FName InName, bool bSetupUndoRedo = true, bool bPrintPythonCommand = true);
};

/** Editor data for AnimNext graphs */
UCLASS(MinimalAPI)
class UAnimNextGraph_EditorData : public UAnimNextRigVMAssetEditorData
{
	GENERATED_BODY()

	friend class UAnimNextGraphFactory;
	friend class UAnimNextGraph_EdGraph;
	friend class UAnimNextGraphEntry;
	friend struct UE::AnimNext::UncookedOnly::FUtils;
	friend struct UE::AnimNext::Editor::FUtils;
	friend class UE::AnimNext::Editor::FGraphEditor;
	friend class UE::AnimNext::Editor::SAnimNextGraphView;
	friend struct FAnimNextGraphSchemaAction_RigUnit;
	friend struct FAnimNextGraphSchemaAction_DispatchFactory;
	friend class FAnimationAnimNextRuntimeTest_GraphAddDecorator;
	friend class FAnimationAnimNextRuntimeTest_GraphExecute;
	friend class FAnimationAnimNextRuntimeTest_GraphExecuteLatent;
	
public:
	/** Adds a graph to this asset */
	ANIMNEXTUNCOOKEDONLY_API UAnimNextGraphEntry* AddGraph(FName InName, bool bSetupUndoRedo = true, bool bPrintPythonCommand = true);

private:
	// UObject interface
	virtual void PostLoad() override;

	// IRigVMClientHost interface
	virtual void RecompileVM() override;

	// UAnimNextRigVMAssetEditorData interface
	virtual TSubclassOf<URigVMController> GetControllerClass() const override { return UAnimNextGraph_Controller::StaticClass(); }
	virtual TSubclassOf<URigVMSchema> GetRigVMSchemaClass() const override { return UAnimNextGraph_Schema::StaticClass(); }
	virtual UScriptStruct* GetExecuteContextStruct() const override { return FAnimNextExecuteContext::StaticStruct(); }
	virtual UEdGraph* CreateEdGraph(URigVMGraph* InRigVMGraph, bool bForce) override;
	virtual bool RemoveEdGraph(URigVMGraph* InModel) override;
	virtual void CreateEdGraphForCollapseNode(URigVMCollapseNode* InNode) override;
	virtual void HandleModifiedEvent(ERigVMGraphNotifType InNotifType, URigVMGraph* InGraph, UObject* InSubject) override;
	virtual TConstArrayView<TSubclassOf<UAnimNextRigVMAssetEntry>> GetEntryClasses() const override;

private:
	UPROPERTY()
	TArray<TObjectPtr<UAnimNextGraph_EdGraph>> Graphs_DEPRECATED;
};
