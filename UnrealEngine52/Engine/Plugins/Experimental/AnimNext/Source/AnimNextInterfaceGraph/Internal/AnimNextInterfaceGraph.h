// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IAnimNextInterface.h"
#include "RigVMCore/RigVM.h"
#include "AnimNextInterfaceGraph.generated.h"

class UEdGraph;
class UAnimNextInterfaceGraph;
namespace UE::AnimNext::InterfaceGraphUncookedOnly
{
	struct FUtils;
}

namespace UE::AnimNext::InterfaceGraphEditor
{
	class FGraphEditor;
}

namespace UE::AnimNext::InterfaceGraph
{
	extern ANIMNEXTINTERFACEGRAPH_API const FName EntryPointName;
	extern ANIMNEXTINTERFACEGRAPH_API const FName ResultName;
}

// A user-created graph of logic used to supply data
UCLASS()
class ANIMNEXTINTERFACEGRAPH_API UAnimNextInterfaceGraph : public UObject, public IAnimNextInterface
{
	GENERATED_BODY()

	// UObject interface
	virtual void PostRename(UObject* OldOuter, const FName OldName) override;
	virtual void GetPreloadDependencies(TArray<UObject*>& OutDeps) override;

	// IAnimAnimNextInterface interface
	virtual FName GetReturnTypeNameImpl() const final override;
	virtual const UScriptStruct* GetReturnTypeStructImpl() const final override;
	virtual bool GetDataImpl(const UE::AnimNext::Interface::FContext& Context) const final override;

	// Support rig VM execution
	TArray<FRigVMExternalVariable> GetRigVMExternalVariables();
	
	friend class UAnimNextInterfaceGraphFactory;
	friend class UAnimNextInterfaceGraph_EditorData;
	friend struct UE::AnimNext::InterfaceGraphUncookedOnly::FUtils;
	friend class UE::AnimNext::InterfaceGraphEditor::FGraphEditor;
	friend class UAnimNextInterface_Graph;
	friend class UAnimGraphNode_AnimNextInterfaceGraph;
	
	UPROPERTY()
	TObjectPtr<URigVM> RigVM;

	UPROPERTY()
	FRigVMRuntimeSettings VMRuntimeSettings;
	
	UPROPERTY()
	FName ReturnTypeName;

	UPROPERTY()
	TObjectPtr<UScriptStruct> ReturnTypeStruct;

#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, Instanced, Category = "Graph", meta = (ShowInnerProperties))
	TObjectPtr<UObject> EditorData;
#endif
};
