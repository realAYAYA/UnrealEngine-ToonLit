// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMModel/Nodes/RigVMFunctionEntryNode.h"
#include "RigVMModel/RigVMFunctionLibrary.h"
#include "RigVMModel/Nodes/RigVMCollapseNode.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigVMFunctionEntryNode)

uint32 URigVMFunctionEntryNode::GetStructureHash() const
{
	// Avoid hashing the template for library nodes
	return URigVMNode::GetStructureHash();
}

FLinearColor URigVMFunctionEntryNode::GetNodeColor() const
{
	if(URigVMGraph* RootGraph = GetRootGraph())
	{
		if(RootGraph->IsA<URigVMFunctionLibrary>())
		{
			return FLinearColor(FColor::FromHex("CB00FFFF"));
		}
	}
	return FLinearColor(FColor::FromHex("005DFFFF"));
}

bool URigVMFunctionEntryNode::IsDefinedAsVarying() const
{ 
	// todo
	return true; 
}

bool URigVMFunctionEntryNode::IsWithinLoop() const
{
	if(const URigVMCollapseNode* CollapseNode = Cast<URigVMCollapseNode>(GetGraph()->GetOuter()))
	{
		return CollapseNode->IsWithinLoop();
	}
	return Super::IsWithinLoop();
}

FString URigVMFunctionEntryNode::GetNodeTitle() const
{
	return TEXT("Entry");
}

FText URigVMFunctionEntryNode::GetToolTipText() const
{
	return FText::FromName(GetGraph()->GetOuter()->GetFName());
}

FText URigVMFunctionEntryNode::GetToolTipTextForPin(const URigVMPin* InPin) const
{
	return Super::GetToolTipTextForPin(InPin);
}
