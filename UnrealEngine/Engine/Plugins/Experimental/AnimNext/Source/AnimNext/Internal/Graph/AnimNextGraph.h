// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RigVMCore/RigVM.h"

#include "AnimNextGraph.generated.h"

class UEdGraph;
class UAnimNextGraph;
class UAnimGraphNode_AnimNextGraph;
struct FAnimNode_AnimNextGraph;

namespace UE::AnimNext
{
	struct FContext;
}

namespace UE::AnimNext::UncookedOnly
{
	struct FUtils;
}

namespace UE::AnimNext::Editor
{
	class FGraphEditor;
}

namespace UE::AnimNext::Graph
{
	extern ANIMNEXT_API const FName EntryPointName;
	extern ANIMNEXT_API const FName ResultName;
}

// A user-created graph of logic used to supply data
UCLASS(BlueprintType)
class ANIMNEXT_API UAnimNextGraph : public UObject
{
	GENERATED_BODY()

	// UObject interface
	virtual void PostRename(UObject* OldOuter, const FName OldName) override;
	virtual void GetPreloadDependencies(TArray<UObject*>& OutDeps) override;

	// Run this graph with the given context
	void Run(const UE::AnimNext::FContext& Context) const;

	// Support rig VM execution
	TArray<FRigVMExternalVariable> GetRigVMExternalVariables();
	
	friend class UAnimNextGraphFactory;
	friend class UAnimNextGraph_EditorData;
	friend struct UE::AnimNext::UncookedOnly::FUtils;
	friend class UE::AnimNext::Editor::FGraphEditor;
	friend class UAnimNextGraph;
	friend class UAnimGraphNode_AnimNextGraph;
	friend struct FAnimNode_AnimNextGraph;

	UPROPERTY()
	TObjectPtr<URigVM> RigVM;

	UPROPERTY(transient)
	mutable FRigVMExtendedExecuteContext ExtendedExecuteContext;

	UPROPERTY()
	FRigVMRuntimeSettings VMRuntimeSettings;

#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, Instanced, Category = "Graph", meta = (ShowInnerProperties))
	TObjectPtr<UObject> EditorData;
#endif
};
