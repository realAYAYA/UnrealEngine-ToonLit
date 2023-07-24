// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMCore/RigVMGraphFunctionDefinition.h"
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
	virtual TArray<FRigVMExternalVariable> GetExternalVariables() const override;
	virtual const FRigVMTemplate* GetTemplate() const override { return nullptr; }
	virtual FRigVMGraphFunctionIdentifier GetFunctionIdentifier() const override;
	// end URigVMLibraryNode interface

	bool IsReferencedFunctionHostLoaded() const;
	bool IsReferencedNodeLoaded() const;
	URigVMLibraryNode* LoadReferencedNode() const;

	// Variable remapping
	bool RequiresVariableRemapping() const;
	bool IsFullyRemapped() const;
	TArray<FRigVMExternalVariable> GetExternalVariables(bool bRemapped) const;
	const TMap<FName, FName>& GetVariableMap() const { return VariableMap; }
	FName GetOuterVariableName(const FName& InInnerVariableName) const;
	// end Variable remapping

	const FRigVMGraphFunctionHeader& GetReferencedFunctionHeader() const { return ReferencedFunctionHeader; }

	const FRigVMGraphFunctionData* GetReferencedFunctionData() const;

	
private:

	virtual FText GetToolTipTextForPin(const URigVMPin* InPin) const override;
	bool RequiresVariableRemappingInternal(TArray<FRigVMExternalVariable>& InnerVariables) const;

	//void SetReferencedFunctionData(FRigVMGraphFunctionData* Data);

	UPROPERTY(AssetRegistrySearchable)
	FRigVMGraphFunctionHeader ReferencedFunctionHeader;
	
	UPROPERTY(AssetRegistrySearchable, meta=(DeprecatedProperty=5.2))
	TSoftObjectPtr<URigVMLibraryNode> ReferencedNodePtr_DEPRECATED;

	UPROPERTY()
	TMap<FName, FName> VariableMap;

	friend class URigVMController;
	friend class FRigVMParserAST;
	friend class UControlRigBlueprint;
	friend struct FRigVMClient;
	friend struct EngineTestRigVMFramework;
};

