// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMModel/Nodes/RigVMLibraryNode.h"
#include "RigVMModel/Nodes/RigVMFunctionEntryNode.h"
#include "RigVMModel/Nodes/RigVMFunctionReturnNode.h"
#include "RigVMModel/RigVMGraph.h"
#include "RigVMModel/Nodes/RigVMCollapseNode.h"
#include "RigVMModel/Nodes/RigVMFunctionReferenceNode.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigVMLibraryNode)

const TArray<URigVMNode*> URigVMLibraryNode::EmptyNodes;
const TArray<URigVMLink*> URigVMLibraryNode::EmptyLinks;

URigVMLibraryNode::URigVMLibraryNode()
{
	Template.Notation = *FString::Printf(TEXT("%s()"), *GetName());
}

bool URigVMLibraryNode::IsDefinedAsConstant() const
{
	return !IsDefinedAsVarying();
}

bool URigVMLibraryNode::IsDefinedAsVarying() const
{
	if (URigVMGraph* Graph = GetContainedGraph())
	{
		const TArray<URigVMNode*>& Nodes = Graph->GetNodes();
		for(URigVMNode* Node : Nodes)
		{
			if (Node->IsDefinedAsVarying())
			{
				return true;
			}
		}
	}
	return false;
}

const FRigVMTemplate* URigVMLibraryNode::GetTemplate() const
{
	return &Template;
}

FName URigVMLibraryNode::GetNotation() const
{
	return Template.GetNotation();
}

FText URigVMLibraryNode::GetToolTipText() const
{
	return FText::FromName(Template.GetNotation());
}

TArray<int32> URigVMLibraryNode::GetInstructionsForVMImpl(URigVM* InVM, const FRigVMASTProxy& InProxy) const
{
	TArray<int32> Instructions;

#if WITH_EDITOR

	if(InVM == nullptr)
	{
		return Instructions;
	}

	const FRigVMASTProxy Proxy = InProxy.GetChild((UObject*)this);
	for(URigVMNode* ContainedNode : GetContainedNodes())
	{
		Instructions.Append(ContainedNode->GetInstructionsForVM(InVM, Proxy));
	}

#endif
	
	return Instructions;
}

/*
int32 URigVMLibraryNode::GetInstructionVisitedCount(URigVM* InVM, const FRigVMASTProxy& InProxy, bool bConsolidatePerNode) const
{
	int32 Count = 0;

#if WITH_EDITOR

	if(InVM == nullptr)
	{
		return Count;
	}

	const FRigVMASTProxy Proxy = InProxy.GetChild((UObject*)this);
	for(URigVMNode* ContainedNode : GetContainedNodes())
	{
		const int32 CountPerNode = ContainedNode->GetInstructionVisitedCount(InVM, Proxy, bConsolidatePerNode);
		Count += CountPerNode;
	}

#endif

	return Count;
}
*/

const TArray<URigVMNode*>& URigVMLibraryNode::GetContainedNodes() const
{
	if(URigVMGraph* Graph = GetContainedGraph())
	{
		return Graph->GetNodes();
	}
	return EmptyNodes;
}

const TArray<URigVMLink*>& URigVMLibraryNode::GetContainedLinks() const
{
	if (URigVMGraph* Graph = GetContainedGraph())
	{
		return Graph->GetLinks();
	}
	return EmptyLinks;

}

URigVMFunctionEntryNode* URigVMLibraryNode::GetEntryNode() const
{
	if (URigVMGraph* Graph = GetContainedGraph())
	{
		return Graph->GetEntryNode();
	}
	return nullptr;
}

URigVMFunctionReturnNode* URigVMLibraryNode::GetReturnNode() const
{
	if (URigVMGraph* Graph = GetContainedGraph())
	{
		return Graph->GetReturnNode();
	}
	return nullptr;
}

bool URigVMLibraryNode::Contains(URigVMLibraryNode* InContainedNode, bool bRecursive) const
{
	if(InContainedNode == nullptr)
	{
		return false;
	}
	
	for(URigVMNode* ContainedNode : GetContainedNodes())
	{
		if(ContainedNode == InContainedNode)
		{
			return true;
		}

		if(URigVMFunctionReferenceNode* FunctionReferenceNode = Cast<URigVMFunctionReferenceNode>(ContainedNode))
		{
			if(URigVMLibraryNode* ReferencedNode = FunctionReferenceNode->GetReferencedNode())
			{
				if(ReferencedNode == InContainedNode)
				{
					return true;
				}
			}
		}
		if(URigVMLibraryNode* ContainedLibraryNode = Cast<URigVMLibraryNode>(ContainedNode))
		{
			if(ContainedLibraryNode->Contains(InContainedNode))
			{
				return true;
			}
		}
	}

	return false;
}

TArray<FRigVMExternalVariable> URigVMLibraryNode::GetExternalVariables() const
{
	TArray<FRigVMExternalVariable> Variables;

	if(URigVMGraph* ContainedGraph = GetContainedGraph())
	{
		TArray<FRigVMExternalVariable> ContainedVariables = ContainedGraph->GetExternalVariables();
		for(const FRigVMExternalVariable& ContainedVariable : ContainedVariables)
		{
			FRigVMExternalVariable::MergeExternalVariable(Variables, ContainedVariable);
		}
	}

	return Variables;
}


