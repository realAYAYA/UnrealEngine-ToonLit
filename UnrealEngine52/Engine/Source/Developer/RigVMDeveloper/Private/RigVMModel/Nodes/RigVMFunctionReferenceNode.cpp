// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMModel/Nodes/RigVMFunctionReferenceNode.h"
#include "RigVMModel/RigVMFunctionLibrary.h"
#include "RigVMCore/RigVMGraphFunctionDefinition.h"
#include "RigVMCore/RigVMGraphFunctionHost.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigVMFunctionReferenceNode)

FString URigVMFunctionReferenceNode::GetNodeTitle() const
{
	return ReferencedFunctionHeader.NodeTitle;
}

FLinearColor URigVMFunctionReferenceNode::GetNodeColor() const
{
	return ReferencedFunctionHeader.NodeColor;
}

FText URigVMFunctionReferenceNode::GetToolTipText() const
{
	return ReferencedFunctionHeader.Tooltip;
}

FString URigVMFunctionReferenceNode::GetNodeCategory() const
{
	return ReferencedFunctionHeader.Category;
}

FString URigVMFunctionReferenceNode::GetNodeKeywords() const
{
	return ReferencedFunctionHeader.Keywords;
}

bool URigVMFunctionReferenceNode::RequiresVariableRemapping() const
{
	TArray<FRigVMExternalVariable> InnerVariables;
	return RequiresVariableRemappingInternal(InnerVariables);
}

bool URigVMFunctionReferenceNode::RequiresVariableRemappingInternal(TArray<FRigVMExternalVariable>& InnerVariables) const
{
	bool bHostedInDifferencePackage = false;
	
	FRigVMGraphFunctionIdentifier LibraryPointer = ReferencedFunctionHeader.LibraryPointer;
	const FString& LibraryPackagePath = LibraryPointer.LibraryNode.GetLongPackageName();
	const FString& ThisPacakgePath = GetPackage()->GetPathName();
	bHostedInDifferencePackage = LibraryPackagePath != ThisPacakgePath;
		
	if(bHostedInDifferencePackage)
	{
		InnerVariables = GetExternalVariables(false);
		if(InnerVariables.Num() == 0)
		{
			return false;
		}
	}

	return bHostedInDifferencePackage;
}

bool URigVMFunctionReferenceNode::IsFullyRemapped() const
{
	TArray<FRigVMExternalVariable> InnerVariables;
	if(!RequiresVariableRemappingInternal(InnerVariables))
	{
		return true;
	}

	for(const FRigVMExternalVariable& InnerVariable : InnerVariables)
	{
		const FName InnerVariableName = InnerVariable.Name;
		const FName* OuterVariableName = VariableMap.Find(InnerVariableName);
		if(OuterVariableName == nullptr)
		{
			return false;
		}
		check(!OuterVariableName->IsNone());
	}

	return true;
}

TArray<FRigVMExternalVariable> URigVMFunctionReferenceNode::GetExternalVariables() const
{
	return GetExternalVariables(true);
}

TArray<FRigVMExternalVariable> URigVMFunctionReferenceNode::GetExternalVariables(bool bRemapped) const
{
	TArray<FRigVMExternalVariable> Variables;
	
	if(!bRemapped)
	{
		if (const FRigVMGraphFunctionData* FunctionData = GetReferencedFunctionData())
		{
			Variables = FunctionData->Header.ExternalVariables;
		}
	}
	else
	{
		if(RequiresVariableRemappingInternal(Variables))
		{
			for(FRigVMExternalVariable& Variable : Variables)
			{
				const FName* OuterVariableName = VariableMap.Find(Variable.Name);
				if(OuterVariableName != nullptr)
				{
					check(!OuterVariableName->IsNone());
					Variable.Name = *OuterVariableName;
				}
			}
		}
	}
	
	return Variables; 
}

FName URigVMFunctionReferenceNode::GetOuterVariableName(const FName& InInnerVariableName) const
{
	if(const FName* OuterVariableName = VariableMap.Find(InInnerVariableName))
	{
		return *OuterVariableName;
	}
	return NAME_None;
}

const FRigVMGraphFunctionData* URigVMFunctionReferenceNode::GetReferencedFunctionData() const
{
	if (IRigVMGraphFunctionHost* Host = ReferencedFunctionHeader.GetFunctionHost())
	{
		return Host->GetRigVMGraphFunctionStore()->FindFunction(ReferencedFunctionHeader.LibraryPointer);
	}
	return nullptr;
}

FText URigVMFunctionReferenceNode::GetToolTipTextForPin(const URigVMPin* InPin) const
{
	check(InPin);

	URigVMPin* RootPin = InPin->GetRootPin();
	const FRigVMGraphFunctionArgument* Argument = ReferencedFunctionHeader.Arguments.FindByPredicate([RootPin](const FRigVMGraphFunctionArgument& Argument)
	{
		return Argument.Name == RootPin->GetFName();
	});

	if (Argument)
	{
		if (const FText* Tooltip = Argument->PathToTooltip.Find(InPin->GetSegmentPath(false)))
		{
			return *Tooltip;
		}
	}
	
	return Super::GetToolTipTextForPin(InPin);
}

FRigVMGraphFunctionIdentifier URigVMFunctionReferenceNode::GetFunctionIdentifier() const
{
	return GetReferencedFunctionHeader().LibraryPointer;
}

bool URigVMFunctionReferenceNode::IsReferencedFunctionHostLoaded() const
{
	return ReferencedFunctionHeader.LibraryPointer.HostObject.ResolveObject() != nullptr;
}

bool URigVMFunctionReferenceNode::IsReferencedNodeLoaded() const
{
	return ReferencedFunctionHeader.LibraryPointer.LibraryNode.ResolveObject() != nullptr;
}

URigVMLibraryNode* URigVMFunctionReferenceNode::LoadReferencedNode() const
{
	UObject* LibraryNode = ReferencedFunctionHeader.LibraryPointer.LibraryNode.ResolveObject();
	if (!LibraryNode)
	{
		LibraryNode = ReferencedFunctionHeader.LibraryPointer.LibraryNode.TryLoad();
	}
	return Cast<URigVMLibraryNode>(LibraryNode);
	
}
