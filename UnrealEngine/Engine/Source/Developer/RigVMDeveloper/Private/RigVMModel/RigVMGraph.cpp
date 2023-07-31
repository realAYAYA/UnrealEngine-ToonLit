// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMModel/RigVMGraph.h"
#include "RigVMModel/RigVMFunctionLibrary.h"
#include "RigVMModel/RigVMController.h"
#include "RigVMModel/Nodes/RigVMLibraryNode.h"
#include "RigVMModel/Nodes/RigVMFunctionEntryNode.h"
#include "RigVMModel/Nodes/RigVMFunctionReturnNode.h"
#include "UObject/Package.h"
#include "RigVMTypeUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigVMGraph)

URigVMGraph::URigVMGraph()
: DiagnosticsAST(nullptr)
, RuntimeAST(nullptr)
, bEditable(true)
{
	SetExecuteContextStruct(FRigVMExecuteContext::StaticStruct());
}

const TArray<URigVMNode*>& URigVMGraph::GetNodes() const
{
	return Nodes;
}

const TArray<URigVMLink*>& URigVMGraph::GetLinks() const
{
	return Links;
}

TArray<URigVMGraph*> URigVMGraph::GetContainedGraphs(bool bRecursive) const
{
	TArray<URigVMGraph*> Graphs;
	for (URigVMNode* Node : GetNodes())
	{
		if (URigVMCollapseNode* CollapseNode = Cast<URigVMCollapseNode>(Node))
		{
			Graphs.Add(CollapseNode->GetContainedGraph());
			if (bRecursive)
			{
				Graphs.Append(CollapseNode->GetContainedGraph()->GetContainedGraphs(true));
			}
		}
	}
	return Graphs;
}

URigVMGraph* URigVMGraph::GetParentGraph() const
{
	if(URigVMCollapseNode* CollapseNode = Cast<URigVMCollapseNode>(GetOuter()))
	{
		return CollapseNode->GetGraph();
	}
	return nullptr;
}

URigVMGraph* URigVMGraph::GetRootGraph() const
{
	if(URigVMGraph* ParentGraph = GetParentGraph())
	{
		return ParentGraph->GetRootGraph();
	}
	return (URigVMGraph*)this;
}

bool URigVMGraph::IsRootGraph() const
{
	return GetRootGraph() == this;
}

URigVMFunctionEntryNode* URigVMGraph::GetEntryNode() const
{
	for (URigVMNode* Node : Nodes)
	{
		if (URigVMFunctionEntryNode* EntryNode = Cast<URigVMFunctionEntryNode>(Node))
		{
			return EntryNode;
		}
	}
	return nullptr;
}

URigVMFunctionReturnNode* URigVMGraph::GetReturnNode() const
{
	for (URigVMNode* Node : Nodes)
	{
		if (URigVMFunctionReturnNode* ReturnNode = Cast<URigVMFunctionReturnNode>(Node))
		{
			return ReturnNode;
		}
	}
	return nullptr;
}

TArray<FRigVMGraphVariableDescription> URigVMGraph::GetVariableDescriptions() const
{
	TArray<FRigVMGraphVariableDescription> Variables;
	for (URigVMNode* Node : Nodes)
	{
		if (URigVMVariableNode* VariableNode = Cast<URigVMVariableNode>(Node))
		{
			Variables.AddUnique(VariableNode->GetVariableDescription());
		}
	}
	return Variables;
}

FString URigVMGraph::GetNodePath() const
{
	if (URigVMCollapseNode* CollapseNode = Cast<URigVMCollapseNode>(GetOuter()))
	{
		return CollapseNode->GetNodePath(true /* recursive */);
	}

	static constexpr TCHAR NodePathFormat[] = TEXT("%s::");
	return FString::Printf(NodePathFormat, *GetName());
}

FString URigVMGraph::GetGraphName() const
{
	if (URigVMCollapseNode* CollapseNode = Cast<URigVMCollapseNode>(GetOuter()))
	{
		return CollapseNode->GetNodePath(false /* recursive */);
	}
	return GetName();
}

URigVMNode* URigVMGraph::FindNodeByName(const FName& InNodeName) const
{
	for (URigVMNode* Node : Nodes)
	{
		if (Node == nullptr)
		{
			continue;
		}

		if (Node->GetFName() == InNodeName)
		{
			return Node;
		}
	}
	return nullptr;
}

URigVMNode* URigVMGraph::FindNode(const FString& InNodePath) const
{
	if (InNodePath.IsEmpty())
	{
		return nullptr;
	}

	FString Path = InNodePath;

	if(IsRootGraph())
	{
		const FString MyNodePath = GetNodePath();
		if(Path.StartsWith(MyNodePath))
		{
			Path.RightChopInline(MyNodePath.Len() + 1);
		}
	}

	FString Left = Path, Right;
	URigVMNode::SplitNodePathAtStart(Path, Left, Right);

	if (Right.IsEmpty())
	{
		return FindNodeByName(*Left);
	}

	if (URigVMLibraryNode* LibraryNode = Cast< URigVMLibraryNode>(FindNodeByName(*Left)))
	{
		return LibraryNode->GetContainedGraph()->FindNode(Right);
	}

	return nullptr;
}

URigVMPin* URigVMGraph::FindPin(const FString& InPinPath) const
{
	FString Left, Right;
	if (!URigVMPin::SplitPinPathAtStart(InPinPath, Left, Right))
	{
		Left = InPinPath;
	}

	URigVMNode* Node = FindNode(Left);
	if (Node)
	{
		return Node->FindPin(Right);
	}

	return nullptr;
}

URigVMLink* URigVMGraph::FindLink(const FString& InLinkPinPathRepresentation) const
{
	for(URigVMLink* Link : Links)
	{
		if(Link->GetPinPathRepresentation() == InLinkPinPathRepresentation)
		{
			return Link;
		}
	}
	return nullptr;
}

bool URigVMGraph::IsNodeSelected(const FName& InNodeName) const
{
	return SelectedNodes.Contains(InNodeName);
}

const TArray<FName>& URigVMGraph::GetSelectNodes() const
{
	return SelectedNodes;
}

bool URigVMGraph::IsTopLevelGraph() const
{
	if (GetOuter()->IsA<URigVMLibraryNode>())
	{
		return false;
	}
	return true;
}

URigVMFunctionLibrary* URigVMGraph::GetDefaultFunctionLibrary() const
{
	if (DefaultFunctionLibraryPtr.IsValid())
	{
		return CastChecked<URigVMFunctionLibrary>(DefaultFunctionLibraryPtr.Get());
	}

	if (URigVMLibraryNode* OuterLibraryNode = Cast<URigVMLibraryNode>(GetOuter()))
	{
		if (URigVMGraph* OuterGraph = OuterLibraryNode->GetGraph())
		{
			return OuterGraph->GetDefaultFunctionLibrary();
		}
	}
	return nullptr;
}

void URigVMGraph::SetDefaultFunctionLibrary(URigVMFunctionLibrary* InFunctionLibrary)
{
	DefaultFunctionLibraryPtr = InFunctionLibrary;
}

TArray<FRigVMExternalVariable> URigVMGraph::GetExternalVariables() const
{
	TArray<FRigVMExternalVariable> Variables;
	
	for(URigVMNode* Node : GetNodes())
	{
		if(URigVMLibraryNode* LibraryNode = Cast<URigVMLibraryNode>(Node))
		{
			TArray<FRigVMExternalVariable> LibraryVariables = LibraryNode->GetExternalVariables();
			for(const FRigVMExternalVariable& LibraryVariable : LibraryVariables)
			{
				FRigVMExternalVariable::MergeExternalVariable(Variables, LibraryVariable);
			}
		}
		else if(URigVMVariableNode* VariableNode = Cast<URigVMVariableNode>(Node))
		{
			// Make sure it is not a local variable or input argument
			if (VariableNode->IsExternalVariable())
			{
				FRigVMExternalVariable::MergeExternalVariable(Variables, VariableNode->GetVariableDescription().ToExternalVariable());
			}
		}
	}
	
	return Variables;
}

TArray<FRigVMGraphVariableDescription> URigVMGraph::GetLocalVariables(bool bIncludeInputArguments) const
{
	if (bIncludeInputArguments)
	{
		TArray<FRigVMGraphVariableDescription> Variables;
		Variables.Append(LocalVariables);
		Variables.Append(GetInputArguments());
		return Variables;
	}
	
	return LocalVariables;
}

TArray<FRigVMGraphVariableDescription> URigVMGraph::GetInputArguments() const
{
	TArray<FRigVMGraphVariableDescription> Inputs;
	if (URigVMFunctionEntryNode* EntryNode = GetEntryNode())
	{
		for (URigVMPin* Pin : EntryNode->GetPins())
		{			
			FRigVMGraphVariableDescription Description;
			Description.Name = Pin->GetFName();
			Description.CPPType = Pin->GetCPPType();
			Description.CPPTypeObject = Pin->GetCPPTypeObject();
			Inputs.Add(Description);			
		}
	}
	return Inputs;
}

TArray<FRigVMGraphVariableDescription> URigVMGraph::GetOutputArguments() const
{
	TArray<FRigVMGraphVariableDescription> Outputs;
	if (URigVMFunctionReturnNode* ReturnNode = GetReturnNode())
	{
		for (URigVMPin* Pin : ReturnNode->GetPins())
		{			
			FRigVMGraphVariableDescription Description;
			Description.Name = Pin->GetFName();
			Description.CPPType = Pin->GetCPPType();
			Description.CPPTypeObject = Pin->GetCPPTypeObject();
			Outputs.Add(Description);			
		}
	}
	return Outputs;
}

FRigVMGraphModifiedEvent& URigVMGraph::OnModified()
{
	return ModifiedEvent;
}

void URigVMGraph::SetExecuteContextStruct(UScriptStruct* InExecuteContextStruct)
{
	check(InExecuteContextStruct);
	ensure(InExecuteContextStruct->IsChildOf(FRigVMExecuteContext::StaticStruct()));
	ExecuteContextStruct = InExecuteContextStruct;
}

UScriptStruct* URigVMGraph::GetExecuteContextStruct() const
{
	if (URigVMGraph* RootGraph = GetRootGraph())
	{
		return RootGraph->ExecuteContextStruct.Get();
	}
	return nullptr;
}

void URigVMGraph::Notify(ERigVMGraphNotifType InNotifType, UObject* InSubject)
{
	ModifiedEvent.Broadcast(InNotifType, this, InSubject);
}

TSharedPtr<FRigVMParserAST> URigVMGraph::GetDiagnosticsAST(bool bForceRefresh, TArray<URigVMLink*> InLinksToSkip)
{
	// only refresh the diagnostics AST if have a different set of links to skip
	if (!bForceRefresh && DiagnosticsAST.IsValid())
	{
		const TArray<URigVMLink*> PreviousLinksToSkip = DiagnosticsAST->GetSettings().LinksToSkip;
		if (PreviousLinksToSkip.Num() < InLinksToSkip.Num())
		{
			bForceRefresh = true;
		}
		else
		{
			for (int32 LinkIndex = 0; LinkIndex < InLinksToSkip.Num(); LinkIndex++)
			{
				if (PreviousLinksToSkip[LinkIndex] != InLinksToSkip[LinkIndex])
				{
					bForceRefresh = true;
					break;
				}
			}
		}
	}

	if (DiagnosticsAST == nullptr || bForceRefresh)
	{
		FRigVMParserASTSettings Settings = FRigVMParserASTSettings::Fast();
		Settings.LinksToSkip = InLinksToSkip;
		DiagnosticsAST = MakeShareable(new FRigVMParserAST({this}, nullptr, Settings));
	}
	return DiagnosticsAST;
}

TSharedPtr<FRigVMParserAST> URigVMGraph::GetRuntimeAST(const FRigVMParserASTSettings& InSettings, bool bForceRefresh)
{
	if (RuntimeAST == nullptr || bForceRefresh)
	{
		RuntimeAST = MakeShareable(new FRigVMParserAST({this}, nullptr, InSettings));
	}
	return RuntimeAST;
}

void URigVMGraph::ClearAST(bool bClearDiagnostics, bool bClearRuntime)
{
	if (bClearDiagnostics)
	{
		DiagnosticsAST.Reset();
	}
	if (bClearRuntime)
	{
		RuntimeAST.Reset();
	}
}

bool URigVMGraph::IsNameAvailable(const FString& InName)
{
	for (URigVMNode* Node : Nodes)
	{
		if (Node->GetName() == InName)
		{
			return false;
		}
	}
	return true;
}

void URigVMGraph::PrepareCycleChecking(URigVMPin* InPin, bool bAsInput)
{
	TArray<URigVMLink*> LinksToSkip;
	if (InPin)
	{
		LinksToSkip = InPin->GetLinks();
	}

	GetDiagnosticsAST(false, LinksToSkip)->PrepareCycleChecking(InPin);
}

bool URigVMGraph::CanLink(URigVMPin* InSourcePin, URigVMPin* InTargetPin, FString* OutFailureReason, const FRigVMByteCode* InByteCode, ERigVMPinDirection InUserLinkDirection)
{
	if (!URigVMPin::CanLink(InSourcePin, InTargetPin, OutFailureReason, InByteCode, InUserLinkDirection))
	{
		return false;
	}
	return GetDiagnosticsAST()->CanLink(InSourcePin, InTargetPin, OutFailureReason);
}


