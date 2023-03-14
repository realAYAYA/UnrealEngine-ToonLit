// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDataInterface.h"
#include "RigVMCore/RigVM.h"
#include "DataInterfaceGraph.generated.h"

class UEdGraph;
class UDataInterfaceGraph;
namespace UE::DataInterfaceGraphUncookedOnly
{
	struct FUtils;
}

namespace UE::DataInterfaceGraphEditor
{
	class FGraphEditor;
}

namespace UE::DataInterfaceGraph
{
	extern DATAINTERFACEGRAPH_API const FName EntryPointName;
	extern DATAINTERFACEGRAPH_API const FName ResultName;
}

// A user-created graph of logic used to supply data
UCLASS()
class DATAINTERFACEGRAPH_API UDataInterfaceGraph : public UObject, public IDataInterface
{
	GENERATED_BODY()

	// IAnimDataInterface interface
	virtual FName GetReturnTypeNameImpl() const final override;
	virtual const UScriptStruct* GetReturnTypeStructImpl() const final override;
	virtual bool GetDataImpl(const UE::DataInterface::FContext& Context) const final override;

	// Support rig VM execution
	TArray<FRigVMExternalVariable> GetRigVMExternalVariables();
	
	friend class UDataInterfaceGraphFactory;
	friend class UDataInterfaceGraph_EditorData;
	friend struct UE::DataInterfaceGraphUncookedOnly::FUtils;
	friend class UE::DataInterfaceGraphEditor::FGraphEditor;
	friend class UDataInterface_Graph;
	
	UPROPERTY()
	TObjectPtr<URigVM> RigVM;

	UPROPERTY()
	FRigVMRuntimeSettings VMRuntimeSettings;
	
#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, Instanced, Category = "Graph", meta = (ShowInnerProperties))
	TObjectPtr<UObject> EditorData;
#endif
};