// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMModel/Nodes/RigVMFunctionReferenceNode.h"
#include "RigVMModel/RigVMFunctionLibrary.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigVMFunctionReferenceNode)

FString URigVMFunctionReferenceNode::GetNodeTitle() const
{
	if (URigVMLibraryNode* ReferencedNode = GetReferencedNode())
	{
		return ReferencedNode->GetNodeTitle();
	}
	return Super::GetNodeTitle();
}

FLinearColor URigVMFunctionReferenceNode::GetNodeColor() const
{
	if (URigVMLibraryNode* ReferencedNode = GetReferencedNode())
	{
		return ReferencedNode->GetNodeColor();
	}
	return Super::GetNodeColor();
}

FText URigVMFunctionReferenceNode::GetToolTipText() const
{
	if (URigVMLibraryNode* ReferencedNode = GetReferencedNode())
	{
		return ReferencedNode->GetToolTipText();
	}
	return Super::GetToolTipText();
}

FString URigVMFunctionReferenceNode::GetNodeCategory() const
{
	if (URigVMLibraryNode* ReferencedNode = GetReferencedNode())
	{
		return ReferencedNode->GetNodeCategory();
	}
	return Super::GetNodeCategory();
}

FString URigVMFunctionReferenceNode::GetNodeKeywords() const
{
	if (URigVMLibraryNode* ReferencedNode = GetReferencedNode())
	{
		return ReferencedNode->GetNodeKeywords();
	}
	return Super::GetNodeKeywords();
}

URigVMFunctionLibrary* URigVMFunctionReferenceNode::GetLibrary() const
{
	if(URigVMLibraryNode* ReferencedNode = GetReferencedNode())
	{
		return ReferencedNode->GetLibrary();
	}
	return nullptr;
}

URigVMGraph* URigVMFunctionReferenceNode::GetContainedGraph() const
{
	if (URigVMLibraryNode* ReferencedNode = GetReferencedNode())
	{
		return ReferencedNode->GetContainedGraph();
	}
	return nullptr;
}

bool URigVMFunctionReferenceNode::RequiresVariableRemapping() const
{
	TArray<FRigVMExternalVariable> InnerVariables;
	return RequiresVariableRemappingInternal(InnerVariables);
}

bool URigVMFunctionReferenceNode::RequiresVariableRemappingInternal(TArray<FRigVMExternalVariable>& InnerVariables) const
{
	bool bHostedInDifferencePackage = false;
	if(URigVMLibraryNode* ReferencedNode = GetReferencedNode())
	{
		// we only need to remap variables if we are referencing
		// a function which is hosted in another asset.
		const UObject* ReferencedPackage = ReferencedNode->GetOutermost(); 
		const UObject* Package = GetOutermost();
		if(ReferencedPackage != Package)
		{
			bHostedInDifferencePackage = true;
		}
		else
		{
			// this case might happen within unit tests
			// where the graphs are not parented to a package
			const UObject* TransientPackage = GetTransientPackage();
			if((ReferencedPackage == TransientPackage) && (Package == TransientPackage))
			{
				const UObject* ReferencedOuter = ReferencedNode;
				while(ReferencedOuter)
				{
					const UObject* Outer = ReferencedOuter->GetOuter();
					if(Outer != TransientPackage)
					{
						ReferencedOuter = Outer;
					}
					else
					{
						break;
					}
				}

				const UObject* CurrentOuter = this;
				while(CurrentOuter)
				{
					const UObject* Outer = CurrentOuter->GetOuter();
					if(Outer != TransientPackage)
					{
						CurrentOuter = Outer;
					}
					else
					{
						break;
					}
				}

				if(CurrentOuter != ReferencedOuter)
				{
					bHostedInDifferencePackage = true;
				}
			}
		}
	}

	if(bHostedInDifferencePackage)
	{
		InnerVariables = Super::GetExternalVariables();
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

const FRigVMTemplate* URigVMFunctionReferenceNode::GetTemplate() const
{
	if (URigVMLibraryNode* ReferencedNode = GetReferencedNode())
	{
		return ReferencedNode->GetTemplate();
	}
	return nullptr;
}

TArray<FRigVMExternalVariable> URigVMFunctionReferenceNode::GetExternalVariables(bool bRemapped) const
{
	TArray<FRigVMExternalVariable> Variables;
	
	if(!bRemapped)
	{
		Variables = Super::GetExternalVariables();
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

FText URigVMFunctionReferenceNode::GetToolTipTextForPin(const URigVMPin* InPin) const
{
	check(InPin);
	
	if(URigVMLibraryNode* ReferencedNode = GetReferencedNode())
	{
		if(URigVMPin* ReferencedPin = ReferencedNode->FindPin(InPin->GetSegmentPath(true)))
		{
			const FString DefaultValue = ReferencedPin->GetDefaultValue();
			if(!DefaultValue.IsEmpty())
			{
				return FText::FromString(FString::Printf(TEXT("%s\nDefault %s"),
					*ReferencedPin->GetToolTipText().ToString(),
					*DefaultValue));
			}
		}
	}

	return Super::GetToolTipTextForPin(InPin);
}

URigVMLibraryNode* URigVMFunctionReferenceNode::GetReferencedNode() const
{
	if (!ReferencedNodePtr.IsValid())
	{
		ReferencedNodePtr.LoadSynchronous();
	}
	return ReferencedNodePtr.Get();
}

void URigVMFunctionReferenceNode::SetReferencedNode(URigVMLibraryNode* InReferenceNode)
{
	ReferencedNodePtr = InReferenceNode;
}
