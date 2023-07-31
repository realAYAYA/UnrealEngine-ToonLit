// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMModel/Nodes/RigVMFunctionEntryNode.h"
#include "RigVMModel/RigVMFunctionLibrary.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigVMFunctionEntryNode)

const FRigVMTemplate* URigVMFunctionEntryNode::GetTemplate() const
{
	if (URigVMLibraryNode* LibraryNode = GetTypedOuter<URigVMLibraryNode>())
	{
		return LibraryNode->GetTemplate();
	}
	return nullptr;
}

FName URigVMFunctionEntryNode::GetNotation() const
{
	if (URigVMLibraryNode* LibraryNode = GetTypedOuter<URigVMLibraryNode>())
	{
		return LibraryNode->GetNotation();
	}
	return NAME_None;
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

FString URigVMFunctionEntryNode::GetNodeTitle() const
{
	return TEXT("Entry");
}

FText URigVMFunctionEntryNode::GetToolTipText() const
{
	return FText::FromName(GetTemplate()->GetNotation());
}

FText URigVMFunctionEntryNode::GetToolTipTextForPin(const URigVMPin* InPin) const
{
	return Super::GetToolTipTextForPin(InPin);
}
