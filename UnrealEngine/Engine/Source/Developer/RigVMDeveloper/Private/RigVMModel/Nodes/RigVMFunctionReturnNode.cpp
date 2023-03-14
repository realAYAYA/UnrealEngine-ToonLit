// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMModel/Nodes/RigVMFunctionReturnNode.h"
#include "RigVMModel/RigVMFunctionLibrary.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigVMFunctionReturnNode)

const FRigVMTemplate* URigVMFunctionReturnNode::GetTemplate() const
{
	if (URigVMLibraryNode* LibraryNode = GetTypedOuter<URigVMLibraryNode>())
	{
		return LibraryNode->GetTemplate();
	}
	return nullptr;
}

FName URigVMFunctionReturnNode::GetNotation() const
{
	if (URigVMLibraryNode* LibraryNode = GetTypedOuter<URigVMLibraryNode>())
	{
		return LibraryNode->GetNotation();
	}
	return NAME_None;
}

FLinearColor URigVMFunctionReturnNode::GetNodeColor() const
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

bool URigVMFunctionReturnNode::IsDefinedAsVarying() const
{ 
	// todo
	return true; 
}

FString URigVMFunctionReturnNode::GetNodeTitle() const
{
	return TEXT("Return");
}

FText URigVMFunctionReturnNode::GetToolTipText() const
{
	return FText::FromName(GetTemplate()->GetNotation());
}

FText URigVMFunctionReturnNode::GetToolTipTextForPin(const URigVMPin* InPin) const
{
	return Super::GetToolTipTextForPin(InPin);
}
