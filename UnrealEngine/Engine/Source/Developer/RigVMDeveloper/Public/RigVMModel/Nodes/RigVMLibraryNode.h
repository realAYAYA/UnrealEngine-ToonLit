// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMModel/Nodes/RigVMTemplateNode.h"
#include "RigVMLibraryNode.generated.h"

class URigVMGraph;
class URigVMFunctionEntryNode;
class URigVMFunctionReturnNode;
class URigVMFunctionLibrary;

/**
 * The Library Node represents a function invocation of a
 * function specified somewhere else. The function can be 
 * expressed as a sub-graph (RigVMGroupNode) or as a 
 * referenced function (RigVMFunctionNode).
 */
UCLASS(BlueprintType)
class RIGVMDEVELOPER_API URigVMLibraryNode : public URigVMTemplateNode
{
	GENERATED_BODY()

public:
	URigVMLibraryNode();

	// Override node functions
	virtual bool IsDefinedAsConstant() const override;
	virtual bool IsDefinedAsVarying() const override;
	//virtual int32 GetInstructionVisitedCount(URigVM* InVM, const FRigVMASTProxy& InProxy = FRigVMASTProxy(), bool bConsolidatePerNode = false) const override;
	
	// Override template node functions
	virtual UScriptStruct* GetScriptStruct() const override { return nullptr; }
	virtual const FRigVMTemplate* GetTemplate() const override;
	virtual FName GetNotation() const override;

	// URigVMNode interface
	virtual  FText GetToolTipText() const override;
	
	// Library node interface
	virtual FString GetNodeCategory() const { return FString(); }
	virtual FString GetNodeKeywords() const { return FString(); }
	virtual FString GetNodeDescription() const { return FString(); }
	
	UFUNCTION(BlueprintCallable, Category = RigVMLibraryNode)
	virtual URigVMFunctionLibrary* GetLibrary() const { return nullptr; }

	UFUNCTION(BlueprintCallable, Category = RigVMLibraryNode)
	virtual URigVMGraph* GetContainedGraph() const { return nullptr; }
	
	virtual const TArray<URigVMNode*>& GetContainedNodes() const;
	virtual const TArray<URigVMLink*>& GetContainedLinks() const;
	virtual URigVMFunctionEntryNode* GetEntryNode() const;
	virtual URigVMFunctionReturnNode* GetReturnNode() const;
	virtual bool Contains(URigVMLibraryNode* InContainedNode, bool bRecursive = true) const;
	virtual TArray<FRigVMExternalVariable> GetExternalVariables() const;

protected:

	FRigVMTemplate Template;
	
	virtual TArray<int32> GetInstructionsForVMImpl(URigVM* InVM, const FRigVMASTProxy& InProxy = FRigVMASTProxy()) const override; 
	const static TArray<URigVMNode*> EmptyNodes;
	const static TArray<URigVMLink*> EmptyLinks;

private:

	friend class URigVMController;
	friend struct FRigVMSetLibraryTemplateAction;
};

