// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMModel/RigVMGraph.h"
#include "RigVMModel/Nodes/RigVMLibraryNode.h"
#include "RigVMFunctionReferenceNode.generated.h"

class URigVMFunctionLibrary;

/**
 * The Function Reference Node is a library node which references
 * a library node from a separate function library graph.
 */
UCLASS(BlueprintType)
class RIGVMDEVELOPER_API URigVMFunctionReferenceNode : public URigVMLibraryNode
{
	GENERATED_BODY()

public:

	// URigVMNode interface
	virtual FString GetNodeTitle() const override;
	virtual FLinearColor GetNodeColor() const override;
	virtual FText GetToolTipText() const override;
	// end URigVMNode interface

	// URigVMLibraryNode interface
	virtual FString GetNodeCategory() const override;
	virtual FString GetNodeKeywords() const override;
	virtual URigVMFunctionLibrary* GetLibrary() const override;
	virtual URigVMGraph* GetContainedGraph() const override;
	virtual TArray<FRigVMExternalVariable> GetExternalVariables() const override;
	virtual const FRigVMTemplate* GetTemplate() const override;
	// end URigVMLibraryNode interface

	URigVMLibraryNode* GetReferencedNode() const;

	// Variable remapping
	bool RequiresVariableRemapping() const;
	bool IsFullyRemapped() const;
	TArray<FRigVMExternalVariable> GetExternalVariables(bool bRemapped) const;
	const TMap<FName, FName>& GetVariableMap() const { return VariableMap; }
	FName GetOuterVariableName(const FName& InInnerVariableName) const;
	// end Variable remapping

private:

	virtual FText GetToolTipTextForPin(const URigVMPin* InPin) const override;
	bool RequiresVariableRemappingInternal(TArray<FRigVMExternalVariable>& InnerVariables) const;

	void SetReferencedNode(URigVMLibraryNode* InReferenceNode);

	UPROPERTY(AssetRegistrySearchable)
	mutable TSoftObjectPtr<URigVMLibraryNode> ReferencedNodePtr;

	UPROPERTY()
	TMap<FName, FName> VariableMap;

	friend class URigVMController;
	friend class FRigVMParserAST;
	friend class UControlRigBlueprint;
};

