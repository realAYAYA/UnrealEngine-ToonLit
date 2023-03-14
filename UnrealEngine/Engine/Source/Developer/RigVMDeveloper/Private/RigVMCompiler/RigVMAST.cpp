// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMCompiler/RigVMAST.h"
#include "RigVMCompiler/RigVMCompiler.h"
#include "RigVMModel/Nodes/RigVMUnitNode.h"
#include "RigVMModel/Nodes/RigVMParameterNode.h"
#include "RigVMModel/Nodes/RigVMVariableNode.h"
#include "RigVMModel/Nodes/RigVMCommentNode.h"
#include "RigVMModel/Nodes/RigVMRerouteNode.h"
#include "RigVMModel/Nodes/RigVMBranchNode.h"
#include "RigVMModel/Nodes/RigVMIfNode.h"
#include "RigVMModel/Nodes/RigVMSelectNode.h"
#include "RigVMModel/Nodes/RigVMEnumNode.h"
#include "RigVMModel/Nodes/RigVMFunctionReturnNode.h"
#include "RigVMModel/Nodes/RigVMFunctionEntryNode.h"
#include "RigVMModel/Nodes/RigVMArrayNode.h"
#include "RigVMModel/Nodes/RigVMInvokeEntryNode.h"
#include "RigVMModel/RigVMGraph.h"
#include "RigVMModel/RigVMController.h"
#include "RigVMCore/RigVMExecuteContext.h"
#include "Stats/StatsHierarchical.h"
#include "RigVMDeveloperModule.h"
#include "VisualGraphUtils.h"
#include "UObject/FieldIterator.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigVMAST)

FRigVMExprAST::FRigVMExprAST(EType InType, const FRigVMASTProxy& InProxy)
	: Name(NAME_None)
	, Type(InType)
	, Index(INDEX_NONE)
{
}

FName FRigVMExprAST::GetTypeName() const
{
	switch (GetType())
	{
		case EType::Block:
		{
			return TEXT("[.Block.]");
		}
		case EType::Entry:
		{
			return TEXT("[.Entry.]");
		}
		case EType::CallExtern:
		{
			return TEXT("[.Call..]");
		}
		case EType::NoOp:
		{
			return TEXT("[.NoOp..]");
		}
		case EType::Var:
		{
			return TEXT("[.Var...]");
		}
		case EType::Literal:
		{
			return TEXT("[Literal]");
		}
		case EType::ExternalVar:
		{
			return TEXT("[ExtVar.]");
		}
		case EType::Assign:
		{
			return TEXT("[.Assign]");
		}
		case EType::Copy:
		{
			return TEXT("[.Copy..]");
		}
		case EType::CachedValue:
		{
			return TEXT("[.Cache.]");
		}
		case EType::Exit:
		{
			return TEXT("[.Exit..]");
		}
		case EType::Branch:
		{
			return TEXT("[Branch.]");
		}
		case EType::If:
		{
			return TEXT("[..If...]");
		}
		case EType::Select:
		{
			return TEXT("[Select.]");
		}
		case EType::Array:
		{
			return TEXT("[Array..]");
		}
		case EType::Invalid:
		{
			return TEXT("[Invalid]");
		}
		default:
		{
			ensure(false);
		}
	}
	return NAME_None;
}

const FRigVMExprAST* FRigVMExprAST::GetParent() const
{
	if (Parents.Num() > 0)
	{
		return ParentAt(0);
	}
	return nullptr;
}

const FRigVMExprAST* FRigVMExprAST::GetFirstParentOfType(EType InExprType) const
{
	for(const FRigVMExprAST* Parent : Parents)
	{
		if (Parent->IsA(InExprType))
		{
			return Parent;
		}
	}
	for (const FRigVMExprAST* Parent : Parents)
	{
		if (const FRigVMExprAST* GrandParent = Parent->GetFirstParentOfType(InExprType))
		{
			return GrandParent;
		}
	}
	return nullptr;
}

const FRigVMExprAST* FRigVMExprAST::GetFirstChildOfType(EType InExprType) const
{
	for (const FRigVMExprAST* Child : Children)
	{
		if (Child->IsA(InExprType))
		{
			return Child;
		}
	}
	for (const FRigVMExprAST* Child : Children)
	{
		if (const FRigVMExprAST* GrandChild = Child->GetFirstChildOfType(InExprType))
		{
			return GrandChild;
		}
	}
	return nullptr;
}

const FRigVMBlockExprAST* FRigVMExprAST::GetBlock() const
{
	if (Parents.Num() == 0)
	{
		if (IsA(EType::Block))
		{
			return To<FRigVMBlockExprAST>();
		}
		return ParserPtr->GetObsoleteBlock();
	}

	const FRigVMExprAST* Parent = GetParent();
	if (Parent->IsA(EType::Block))
	{
		return Parent->To<FRigVMBlockExprAST>();
	}

	return Parent->GetBlock();
}

const FRigVMBlockExprAST* FRigVMExprAST::GetRootBlock() const
{
	const FRigVMBlockExprAST* Block = GetBlock();

	if (IsA(EType::Block))
	{
		if (Block && NumParents() > 0)
		{
			return Block->GetRootBlock();
		}
		return To<FRigVMBlockExprAST>();
	}

	if (Block)
	{
		return Block->GetRootBlock();
	}

	return nullptr;
}

int32 FRigVMExprAST::GetMinChildIndexWithinParent(const FRigVMExprAST* InParentExpr) const
{
	int32 MinIndex = INDEX_NONE;

	const TTuple<const FRigVMExprAST*, const FRigVMExprAST*> MapKey(InParentExpr, this);

	const FRigVMParserAST* Parser = GetParser();
	if(Parser)
	{
		if(const int32* IndexPtr = Parser->MinIndexOfChildWithinParent.Find(MapKey))
		{
			return *IndexPtr;
		}
	}

	for (const FRigVMExprAST* Parent : Parents)
	{
		int32 ChildIndex = INDEX_NONE;

		if (Parent == InParentExpr)
		{
			Parent->Children.Find((FRigVMExprAST*)this, ChildIndex);
		}
		else
		{
			ChildIndex = Parent->GetMinChildIndexWithinParent(InParentExpr);
		}

		if (ChildIndex != INDEX_NONE)
		{
			if (ChildIndex < MinIndex || MinIndex == INDEX_NONE)
			{
				MinIndex = ChildIndex;
			}
		}
	}

	if(Parser)
	{
		Parser->MinIndexOfChildWithinParent.Add(MapKey, MinIndex);
	}
	return MinIndex;
}

void FRigVMExprAST::AddParent(FRigVMExprAST* InParent)
{
	ensure(IsValid());
	ensure(InParent->IsValid());
	ensure(InParent != this);
	
	if (Parents.Contains(InParent))
	{
		return;
	}

	InParent->Children.Add(this);
	Parents.Add(InParent);
}

void FRigVMExprAST::RemoveParent(FRigVMExprAST* InParent)
{
	ensure(IsValid());
	ensure(InParent->IsValid());
	
	if (Parents.Remove(InParent) > 0)
	{
		const int32 ExistingIndex = InParent->Children.Find(this);
		if(ExistingIndex != INDEX_NONE)
		{
			FName NameToRemove(NAME_None);
			for(TPair<FName, int32>& Pair: InParent->PinNameToChildIndex)
			{
				if(Pair.Value > ExistingIndex)
				{
					Pair.Value--;
				}
				else if(Pair.Value == ExistingIndex)
				{
					NameToRemove = Pair.Key;
				}
			}
			InParent->PinNameToChildIndex.Remove(NameToRemove);
		}
		InParent->Children.Remove(this);
	}
}

void FRigVMExprAST::RemoveChild(FRigVMExprAST* InChild)
{
	ensure(IsValid());
	ensure(InChild->IsValid());
	
	InChild->RemoveParent(this);
}

void FRigVMExprAST::ReplaceParent(FRigVMExprAST* InCurrentParent, FRigVMExprAST* InNewParent)
{
	ensure(IsValid());
	ensure(InCurrentParent->IsValid());
	ensure(InNewParent->IsValid());
	
	for (int32 ParentIndex = 0; ParentIndex < Parents.Num(); ParentIndex++)
	{
		if (Parents[ParentIndex] == InCurrentParent)
		{
			Parents[ParentIndex] = InNewParent;
			InCurrentParent->Children.Remove(this);
			InNewParent->Children.Add(this);
		}
	}
}

void FRigVMExprAST::ReplaceChild(FRigVMExprAST* InCurrentChild, FRigVMExprAST* InNewChild)
{
	ensure(IsValid());
	ensure(InCurrentChild->IsValid());
	ensure(InNewChild->IsValid());

	for (int32 ChildIndex = 0; ChildIndex < Children.Num(); ChildIndex++)
	{
		if (Children[ChildIndex] == InCurrentChild)
		{
			Children[ChildIndex] = InNewChild;
			InCurrentChild->Parents.Remove(this);
			InNewChild->Parents.Add(this);
		}
	}
}

void FRigVMExprAST::ReplaceBy(FRigVMExprAST* InReplacement)
{
	TArray<FRigVMExprAST*> PreviousParents;
	PreviousParents.Append(Parents);

	for (FRigVMExprAST* PreviousParent : PreviousParents)
	{
		PreviousParent->ReplaceChild(this, InReplacement);
	}
}

bool FRigVMExprAST::IsConstant() const
{
	for (FRigVMExprAST* ChildExpr : Children)
	{
		if (!ChildExpr->IsConstant())
		{
			return false;
		}
	}
	return true;
}

FString FRigVMExprAST::DumpText(const FString& InPrefix) const
{
	FString Result;
	if (Name.IsNone())
	{
		Result = FString::Printf(TEXT("%s%s"), *InPrefix, *GetTypeName().ToString());
	}
	else
	{
		Result = FString::Printf(TEXT("%s%s %s"), *InPrefix, *GetTypeName().ToString(), *Name.ToString());
	}

	if (Children.Num() > 0)
	{
		FString Prefix = InPrefix;
		if (Prefix.IsEmpty())
		{
			Prefix = TEXT("-- ");
		}
		else
		{
			Prefix = TEXT("---") + Prefix;
		}
		for (FRigVMExprAST* Child : Children)
		{
			Result += TEXT("\n") + Child->DumpText(Prefix);
		}
	}
	return Result;
}

bool FRigVMBlockExprAST::ShouldExecute() const
{
	return ContainsEntry();
}

bool FRigVMBlockExprAST::ContainsEntry() const
{
	if (IsA(FRigVMExprAST::EType::Entry))
	{
		return true;
	}
	for (FRigVMExprAST* Expression : *this)
	{
		if (Expression->IsA(EType::Entry))
		{
			return true;
		}
	}
	return false;
}

bool FRigVMBlockExprAST::Contains(const FRigVMExprAST* InExpression, TMap<const FRigVMExprAST*, bool>* ContainedExpressionsCache) const
{
	if (InExpression == this)
	{
		return true;
	}

	if (ContainedExpressionsCache)
	{
		if (bool* Result = ContainedExpressionsCache->Find(InExpression))
		{
			return *Result;
		}
	}

	for (int32 ParentIndex = 0; ParentIndex < InExpression->NumParents(); ParentIndex++)
	{
		const FRigVMExprAST* ParentExpr = InExpression->ParentAt(ParentIndex);
		if (Contains(ParentExpr, ContainedExpressionsCache))
		{
			if (ContainedExpressionsCache)
			{
				ContainedExpressionsCache->Add(InExpression, true);
			}
			return true;
		}
	}

	if (ContainedExpressionsCache)
	{
		ContainedExpressionsCache->Add(InExpression, false);
	}
	return false;
}

bool FRigVMNodeExprAST::IsConstant() const
{
	if (URigVMNode* CurrentNode = GetNode())
	{
		if (CurrentNode->IsDefinedAsConstant())
		{
			return true;
		}
		else if (CurrentNode->IsDefinedAsVarying())
		{
			return false;
		}

		TArray<URigVMPin*> AllPins = CurrentNode->GetAllPinsRecursively();
		for (URigVMPin* Pin : AllPins)
		{
			// don't flatten pins which have a watch
			if(Pin->RequiresWatch(false))
			{
				return false;
			}
		}
	}
	return FRigVMExprAST::IsConstant();
}

const FRigVMExprAST* FRigVMNodeExprAST::FindExprWithPinName(const FName& InPinName) const
{
	if(const int32* PinIndex = PinNameToChildIndex.Find(InPinName))
	{
		return ChildAt(*PinIndex);
	}
	
	if(URigVMNode* CurrentNode = GetNode())
	{
		if(const URigVMPin* Pin = CurrentNode->FindPin(InPinName.ToString()))
		{
			const int32 PinIndex = Pin->GetPinIndex();
			if(PinIndex <= NumChildren())
			{
				return ChildAt(PinIndex);
			}
		}
	}
	return nullptr;
}

const FRigVMVarExprAST* FRigVMNodeExprAST::FindVarWithPinName(const FName& InPinName) const
{
	const FRigVMExprAST* Child = FindExprWithPinName(InPinName);
	if (Child->IsA(FRigVMExprAST::Var))
	{
		return Child->To<FRigVMVarExprAST>();
	}
	return nullptr;
}

FRigVMNodeExprAST::FRigVMNodeExprAST(EType InType, const FRigVMASTProxy& InNodeProxy)
	: FRigVMBlockExprAST(InType)
	, Proxy(InNodeProxy)
{
}

FName FRigVMEntryExprAST::GetEventName() const
{
	if (URigVMNode* EventNode = GetNode())
	{
		return EventNode->GetEventName();
	}
	return NAME_None;
}

bool FRigVMVarExprAST::IsConstant() const
{
	if (GetPin()->IsExecuteContext())
	{
		return false;
	}

	if (GetPin()->IsDefinedAsConstant())
	{
		return true;
	}

	if (SupportsSoftLinks())
	{
		return false;
	}

	ERigVMPinDirection Direction = GetPin()->GetDirection();
	if (Direction == ERigVMPinDirection::Hidden)
	{
		if (Cast<URigVMVariableNode>(GetPin()->GetNode()))
		{
			if (GetPin()->GetName() == URigVMVariableNode::VariableName)
			{
				return true;
			}
		}
		return false;
	}

	if (GetPin()->GetDirection() == ERigVMPinDirection::IO ||
		GetPin()->GetDirection() == ERigVMPinDirection::Output)
	{
		if (GetPin()->GetNode()->IsDefinedAsVarying())
		{
			return false;
		}
	}

	return FRigVMExprAST::IsConstant();
}

FString FRigVMVarExprAST::GetCPPType() const
{
	return GetPin()->GetCPPType();
}

UObject* FRigVMVarExprAST::GetCPPTypeObject() const
{
	return GetPin()->GetCPPTypeObject();
}

ERigVMPinDirection FRigVMVarExprAST::GetPinDirection() const
{
	return GetPin()->GetDirection();
}

FString FRigVMVarExprAST::GetDefaultValue() const
{
	return GetPin()->GetDefaultValue(URigVMPin::FPinOverride(GetProxy(), GetParser()->GetPinOverrides()));
}

bool FRigVMVarExprAST::IsExecuteContext() const
{
	return GetPin()->IsExecuteContext();
}

bool FRigVMVarExprAST::IsGraphVariable() const
{
	if (Cast<URigVMVariableNode>(GetPin()->GetNode()))
	{
		return GetPin()->GetName() == URigVMVariableNode::ValueName;
	}
	return false;
}

bool FRigVMVarExprAST::IsEnumValue() const
{
	if (Cast<URigVMEnumNode>(GetPin()->GetNode()))
	{
		return GetPin()->GetName() == TEXT("EnumIndex");
	}
	return false;
}

bool FRigVMVarExprAST::SupportsSoftLinks() const
{
	if (URigVMUnitNode* UnitNode = Cast<URigVMUnitNode>(GetPin()->GetNode()))
	{
		if (UnitNode->IsLoopNode())
		{
			if (GetPin()->GetFName() != FRigVMStruct::ExecuteContextName &&
				GetPin()->GetFName() != FRigVMStruct::ForLoopCompletedPinName)
			{
				return true;
			}
		}
	}
	else if(URigVMArrayNode* ArrayNode = Cast<URigVMArrayNode>(GetPin()->GetNode()))
	{
		if(ArrayNode->IsLoopNode())
		{
			if (GetPin()->GetFName() != FRigVMStruct::ExecuteContextName &&
				GetPin()->GetName() != URigVMArrayNode::CompletedName)
			{
				return true;
			}
		}
	}
	return false;
}


bool FRigVMBranchExprAST::IsConstant() const
{
	if (IsAlwaysTrue())
	{
		return GetTrueExpr()->IsConstant();
	}
	else if(IsAlwaysFalse())
	{
		return GetFalseExpr()->IsConstant();
	}
	return FRigVMNodeExprAST::IsConstant();
}

bool FRigVMBranchExprAST::IsAlwaysTrue() const
{
	const FRigVMVarExprAST* ConditionExpr = GetConditionExpr();
	if (ConditionExpr->IsA(EType::Literal))
	{
		const FString& PinDefaultValue = ConditionExpr->GetDefaultValue();
		return PinDefaultValue == TEXT("True");
	}
	return false;
}

bool FRigVMBranchExprAST::IsAlwaysFalse() const
{
	const FRigVMVarExprAST* ConditionExpr = GetConditionExpr();
	if (ConditionExpr->IsA(EType::Literal))
	{
		const FString& PinDefaultValue = ConditionExpr->GetDefaultValue();
		return PinDefaultValue == TEXT("False") || PinDefaultValue.IsEmpty();
	}
	return false;
}

bool FRigVMIfExprAST::IsConstant() const
{
	if (IsAlwaysTrue())
	{
		return GetTrueExpr()->IsConstant();
	}
	else if (IsAlwaysFalse())
	{
		return GetFalseExpr()->IsConstant();
	}
	return FRigVMNodeExprAST::IsConstant();
}

bool FRigVMIfExprAST::IsAlwaysTrue() const
{
	const FRigVMVarExprAST* ConditionExpr = GetConditionExpr();
	if (ConditionExpr->IsA(EType::Literal))
	{
		const FString& PinDefaultValue = ConditionExpr->GetDefaultValue();
		return PinDefaultValue == TEXT("True");
	}
	return false;
}

bool FRigVMIfExprAST::IsAlwaysFalse() const
{
	const FRigVMVarExprAST* ConditionExpr = GetConditionExpr();
	if (ConditionExpr->IsA(EType::Literal))
	{
		const FString& PinDefaultValue = ConditionExpr->GetDefaultValue();
		return PinDefaultValue == TEXT("False") || PinDefaultValue.IsEmpty();
	}
	return false;
}

bool FRigVMSelectExprAST::IsConstant() const
{
	int32 ConstantCaseIndex = GetConstantValueIndex();
	if (ConstantCaseIndex != INDEX_NONE)
	{
		return GetValueExpr(ConstantCaseIndex)->IsConstant();
	}
	return FRigVMNodeExprAST::IsConstant();
}

int32 FRigVMSelectExprAST::GetConstantValueIndex() const
{
	const FRigVMVarExprAST* IndexExpr = GetIndexExpr();
	if (IndexExpr->IsA(EType::Literal))
	{
		int32 NumCases = NumValues();
		if (NumCases == 0)
		{
			return INDEX_NONE;
		}

		const FString& PinDefaultValue = IndexExpr->GetDefaultValue();
		int32 CaseIndex = 0;
		if (!PinDefaultValue.IsEmpty())
		{
			CaseIndex = FCString::Atoi(*PinDefaultValue);
		}

		return FMath::Clamp<int32>(CaseIndex, 0, NumCases - 1);
	}
	return INDEX_NONE;
}

int32 FRigVMSelectExprAST::NumValues() const
{
	return GetNode()->FindPin(URigVMSelectNode::ValueName)->GetArraySize();
}

void FRigVMParserASTSettings::Report(EMessageSeverity::Type InSeverity, UObject* InSubject, const FString& InMessage) const
{
	if (ReportDelegate.IsBound())
	{
		ReportDelegate.Execute(InSeverity, InSubject, InMessage);
	}
	else
	{
		if (InSeverity == EMessageSeverity::Error)
		{
			FScriptExceptionHandler::Get().HandleException(ELogVerbosity::Error, *InMessage, *FString());
		}
		else if (InSeverity == EMessageSeverity::Warning)
		{
			FScriptExceptionHandler::Get().HandleException(ELogVerbosity::Warning, *InMessage, *FString());
		}
		else
		{
			UE_LOG(LogRigVMDeveloper, Display, TEXT("%s"), *InMessage);
		}
	}
}

const TArray<FRigVMASTProxy> FRigVMParserAST::EmptyProxyArray;

FRigVMParserAST::FRigVMParserAST(TArray<URigVMGraph*> InGraphs, URigVMController* InController, const FRigVMParserASTSettings& InSettings, const TArray<FRigVMExternalVariable>& InExternalVariables, const TArray<FRigVMUserDataArray>& InRigVMUserData)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	Settings = InSettings;
	ObsoleteBlock = nullptr;
	LastCycleCheckExpr = nullptr;
	LinksToSkip = InSettings.LinksToSkip;

	check(!InGraphs.IsEmpty());

	// construct the inlined nodes and links information
	Inline(InGraphs);

	for (const FRigVMASTProxy& NodeProxy : NodeProxies)
	{
		URigVMNode* Node = NodeProxy.GetSubjectChecked<URigVMNode>();
		if(Node->IsEvent())
		{
			TraverseMutableNode(NodeProxy, nullptr);
		}
	}

	// traverse all remaining mutable nodes,
	// followed by a pass for all remaining non-mutable nodes
	for (int32 PassIndex = 0; PassIndex < 2; PassIndex++)
	{
		const bool bTraverseMutable = PassIndex == 0;
		for (int32 NodeIndex = 0; NodeIndex < NodeProxies.Num(); NodeIndex++)
		{
			if (const int32* ExprIndex = NodeExpressionIndex.Find(NodeProxies[NodeIndex]))
			{
				if (*ExprIndex != INDEX_NONE)
				{
					continue;
				}
			}

			URigVMNode* Node = NodeProxies[NodeIndex].GetSubjectChecked<URigVMNode>();
			if (Node->IsMutable() == bTraverseMutable)
			{
				if (bTraverseMutable)
				{
					TraverseMutableNode(NodeProxies[NodeIndex], GetObsoleteBlock());
				}
				else
				{
					TraverseNode(NodeProxies[NodeIndex], GetObsoleteBlock());
				}
			}
		}
	}

	FoldEntries();
	InjectExitsToEntries();
	FoldNoOps();

	// keep folding constant branches and values while we can
	bool bContinueToFoldConstantBranches = InSettings.bFoldConstantBranches;
	while (bContinueToFoldConstantBranches)
	{
		bContinueToFoldConstantBranches = false;
		if (FoldConstantValuesToLiterals(InGraphs, InController, InExternalVariables, InRigVMUserData))
		{
			bContinueToFoldConstantBranches = true;
		}
		if (FoldUnreachableBranches(InGraphs))
		{
			bContinueToFoldConstantBranches = true;
		}
	}

	BubbleUpExpressions();

	if (InSettings.bFoldAssignments)
	{
		FoldAssignments();
	}

	if (InSettings.bFoldLiterals)
	{
		FoldLiterals();
	}
}

FRigVMParserAST::FRigVMParserAST(TArray<URigVMGraph*> InGraphs, const TArray<FRigVMASTProxy>& InNodesToCompute)
{
	check(!InGraphs.IsEmpty());

	LastCycleCheckExpr = nullptr;

	FRigVMBlockExprAST* Block = MakeExpr<FRigVMBlockExprAST>(FRigVMExprAST::EType::Block, FRigVMASTProxy());
	Block->Name = TEXT("NodesToCompute");
	RootExpressions.Add(Block);

	NodeProxies = InNodesToCompute;
	
	Inline(InGraphs, InNodesToCompute);

	for (const FRigVMASTProxy& NodeProxy : NodeProxies)
	{
		URigVMNode* Node = NodeProxy.GetSubjectChecked<URigVMNode>();
		if (Node->IsEvent())
		{
			continue;
		}
		if (Node->IsMutable())
		{
			continue;
		}
		TraverseNode(NodeProxy, Block);
	}

	FRigVMExprAST* ExitExpr = MakeExpr<FRigVMExitExprAST>(FRigVMASTProxy());
	ExitExpr->AddParent(Block);
}

FRigVMParserAST::~FRigVMParserAST()
{
	for (FRigVMExprAST* Expression : Expressions)
	{
		delete(Expression);
	}
	Expressions.Empty();

	for (FRigVMExprAST* Expression : DeletedExpressions)
	{
		delete(Expression);
	}
	DeletedExpressions.Empty();

	// root expressions are a subset of the
	// expressions array, so no cleanup necessary
	RootExpressions.Empty();
}

FRigVMExprAST* FRigVMParserAST::TraverseMutableNode(const FRigVMASTProxy& InNodeProxy, FRigVMExprAST* InParentExpr)
{
	if (SubjectToExpression.Contains(InNodeProxy))
	{
		return SubjectToExpression.FindChecked(InNodeProxy);
	}

	URigVMNode* Node = InNodeProxy.GetSubjectChecked<URigVMNode>();
	if(Node->HasOrphanedPins())
	{
		return nullptr;
	}

	FRigVMExprAST* NodeExpr = CreateExpressionForNode(InNodeProxy, InParentExpr);
	if (NodeExpr)
	{
		if (InParentExpr == nullptr)
		{
			InParentExpr = NodeExpr;
		}

		TraversePins(InNodeProxy, NodeExpr);

		for (URigVMPin* SourcePin : Node->GetPins())
		{
			if (SourcePin->GetDirection() == ERigVMPinDirection::Output || SourcePin->GetDirection() == ERigVMPinDirection::IO)
			{
				if (SourcePin->IsExecuteContext())
				{
					FRigVMASTProxy SourcePinProxy = InNodeProxy.GetSibling(SourcePin);

					FRigVMExprAST* ParentExpr = InParentExpr;
					if (NodeExpr->IsA(FRigVMExprAST::Branch) || Node->IsLoopNode())
					{
						if (FRigVMExprAST** PinExpr = SubjectToExpression.Find(SourcePinProxy))
						{
							FRigVMBlockExprAST* BlockExpr = MakeExpr<FRigVMBlockExprAST>(FRigVMExprAST::EType::Block, FRigVMASTProxy());
							BlockExpr->AddParent(*PinExpr);
							BlockExpr->Name = SourcePin->GetFName();
							ParentExpr = BlockExpr;
						}
					}

					const TArray<int32>& LinkIndices = GetTargetLinkIndices(SourcePinProxy);
					for(const int32 LinkIndex : LinkIndices)
					{
						const FRigVMASTLinkDescription& Link = Links[LinkIndex];
						if(ShouldLinkBeSkipped(Link))
						{
							continue;
						}

						URigVMNode* TargetNode = Link.TargetProxy.GetSubjectChecked<URigVMPin>()->GetNode();
						FRigVMASTProxy TargetNodeProxy = Link.TargetProxy.GetSibling(TargetNode);
						TraverseMutableNode(TargetNodeProxy, ParentExpr);
					}
				}
			}
		}
	}

	return NodeExpr;
}

FRigVMExprAST* FRigVMParserAST::TraverseNode(const FRigVMASTProxy& InNodeProxy, FRigVMExprAST* InParentExpr)
{
	URigVMNode* Node = InNodeProxy.GetSubjectChecked<URigVMNode>();
	if (Cast<URigVMCommentNode>(Node))
	{
		return nullptr;
	
	}

	if(Node->HasOrphanedPins())
	{
		return nullptr;
	}
	
	if (SubjectToExpression.Contains(InNodeProxy))
	{
		FRigVMExprAST* NodeExpr = SubjectToExpression.FindChecked(InNodeProxy);
		NodeExpr->AddParent(InParentExpr);
		return NodeExpr;
	}

	FRigVMExprAST* NodeExpr = CreateExpressionForNode(InNodeProxy, InParentExpr);
	if (NodeExpr)
	{
		TraversePins(InNodeProxy, NodeExpr);
	}

	return NodeExpr;
}

FRigVMExprAST* FRigVMParserAST::CreateExpressionForNode(const FRigVMASTProxy& InNodeProxy, FRigVMExprAST* InParentExpr)
{
	URigVMNode* Node = InNodeProxy.GetSubjectChecked<URigVMNode>();

	FRigVMExprAST* NodeExpr = nullptr;
	if (Node->IsEvent())
	{
		NodeExpr = MakeExpr<FRigVMEntryExprAST>(InNodeProxy);
		NodeExpr->Name = Node->GetEventName();
	}
	else
	{
		if (InNodeProxy.IsA<URigVMRerouteNode>() ||
			InNodeProxy.IsA<URigVMVariableNode>() ||
			InNodeProxy.IsA<URigVMEnumNode>() ||
			InNodeProxy.IsA<URigVMLibraryNode>() ||
			InNodeProxy.IsA<URigVMFunctionEntryNode>() ||
			InNodeProxy.IsA<URigVMFunctionReturnNode>())
		{
			NodeExpr = MakeExpr<FRigVMNoOpExprAST>(InNodeProxy);
		}
		else if (InNodeProxy.IsA<URigVMBranchNode>())
		{
			NodeExpr = MakeExpr<FRigVMBranchExprAST>(InNodeProxy);
		}
		else if (InNodeProxy.IsA<URigVMIfNode>())
		{
			NodeExpr = MakeExpr<FRigVMIfExprAST>(InNodeProxy);
		}
		else if (InNodeProxy.IsA<URigVMSelectNode>())
		{
			NodeExpr = MakeExpr<FRigVMSelectExprAST>(InNodeProxy);
		}
		else if (InNodeProxy.IsA<URigVMArrayNode>())
		{
			NodeExpr = MakeExpr<FRigVMArrayExprAST>(InNodeProxy);
		}
		else if (InNodeProxy.IsA<URigVMInvokeEntryNode>())
		{
			NodeExpr = MakeExpr<FRigVMInvokeEntryExprAST>(InNodeProxy);
		}
		else
		{
			NodeExpr = MakeExpr<FRigVMCallExternExprAST>(InNodeProxy);
		}
		NodeExpr->Name = Node->GetFName();
	}

	if (InParentExpr != nullptr)
	{
		NodeExpr->AddParent(InParentExpr);
	}
	else
	{
		RootExpressions.Add(NodeExpr);
	}
	SubjectToExpression.Add(InNodeProxy, NodeExpr);
	NodeExpressionIndex.Add(InNodeProxy, NodeExpr->GetIndex());

	return NodeExpr;
}

TArray<FRigVMExprAST*> FRigVMParserAST::TraversePins(const FRigVMASTProxy& InNodeProxy, FRigVMExprAST* InParentExpr)
{
	URigVMNode* Node = InNodeProxy.GetSubjectChecked<URigVMNode>();
	TArray<FRigVMExprAST*> PinExpressions;

	TArray<URigVMPin*> Pins;

	// traverse the pins on a unit node in the order of the property definitions
	if(const URigVMUnitNode* UnitNode = Cast<URigVMUnitNode>(Node))
	{
		if(UScriptStruct* ScriptStruct = UnitNode->GetScriptStruct())
		{
			TArray<UStruct*> Structs = FRigVMTemplate::GetSuperStructs(ScriptStruct, true);
			for(const UStruct* Struct : Structs)
			{
				for (TFieldIterator<FProperty> PropertyIt(Struct, EFieldIterationFlags::None); PropertyIt; ++PropertyIt)
				{
					if(URigVMPin* Pin = UnitNode->FindPin(PropertyIt->GetName()))
					{
						Pins.Add(Pin);
					}
				}
			}
		}
	}

	if(Pins.IsEmpty())
	{
		Pins = Node->GetPins();
	}

	for (URigVMPin* Pin : Pins)
	{
		FRigVMASTProxy PinProxy = InNodeProxy.GetSibling(Pin);

		if (InParentExpr &&
		    Pin->GetDirection() == ERigVMPinDirection::Input &&
		    InParentExpr->IsA(FRigVMExprAST::EType::Select))
		{
			if (Pin->GetName() == URigVMSelectNode::ValueName)
			{
				const TArray<URigVMPin*>& CasePins = Pin->GetSubPins();
				for (URigVMPin* CasePin : CasePins)
				{
					FRigVMASTProxy CasePinProxy = PinProxy.GetSibling(CasePin);

					PinExpressions.Add(TraversePin(CasePinProxy, InParentExpr));

					const FString PinName = FString::Printf(TEXT("%s_%s"), *Pin->GetName(), *CasePin->GetName());
					const int32 ChildIndex = InParentExpr->Children.Find(PinExpressions.Last());
					InParentExpr->PinNameToChildIndex.FindOrAdd(*PinName) = ChildIndex;
				}
				continue;
			}
		}

		PinExpressions.Add(TraversePin(PinProxy, InParentExpr));
		if(InParentExpr)
		{
			const int32 ChildIndex = InParentExpr->Children.Find(PinExpressions.Last());
			InParentExpr->PinNameToChildIndex.FindOrAdd(Pin->GetFName()) = ChildIndex;
		}
	}

	return PinExpressions;
}

FRigVMExprAST* FRigVMParserAST::TraversePin(const FRigVMASTProxy& InPinProxy, FRigVMExprAST* InParentExpr)
{
	ensure(!SubjectToExpression.Contains(InPinProxy));

	URigVMPin* Pin = InPinProxy.GetSubjectChecked<URigVMPin>();
	URigVMPin::FPinOverride PinOverride(InPinProxy, PinOverrides);

	TArray<int32> LinkIndices = GetSourceLinkIndices(InPinProxy, true);
	
	if (LinksToSkip.Num() > 0)
	{
		LinkIndices.RemoveAll([this](int32 LinkIndex)
			{
				return this->ShouldLinkBeSkipped(Links[LinkIndex]);
			}
		);
	}

	FRigVMExprAST* PinExpr = nullptr;

	if (Cast<URigVMVariableNode>(Pin->GetNode()))
	{
		if (Pin->GetName() == URigVMVariableNode::VariableName)
		{
			return nullptr;
		}
	}
	else if (Cast<URigVMEnumNode>(Pin->GetNode()))
	{
		if (Pin->GetDirection() == ERigVMPinDirection::Visible)
		{
			return nullptr;
		}
	}

	if ((Pin->GetDirection() == ERigVMPinDirection::Input ||
		Pin->GetDirection() == ERigVMPinDirection::Visible) &&
		LinkIndices.Num() == 0)
	{
		if (Cast<URigVMVariableNode>(Pin->GetNode()))
		{
			PinExpr = MakeExpr<FRigVMVarExprAST>(FRigVMExprAST::EType::Var, InPinProxy);
			FRigVMExprAST* PinLiteralExpr = MakeExpr<FRigVMLiteralExprAST>(InPinProxy);
			PinLiteralExpr->Name = PinExpr->Name;
			const FRigVMASTLinkDescription LiteralLink(InPinProxy, InPinProxy, FString());
			FRigVMExprAST* PinCopyExpr = MakeExpr<FRigVMCopyExprAST>(LiteralLink);
			PinCopyExpr->AddParent(PinExpr);
			PinLiteralExpr->AddParent(PinCopyExpr);
		}
		else
		{
			PinExpr = MakeExpr<FRigVMLiteralExprAST>(InPinProxy);
		}
	}
	else if (Cast<URigVMEnumNode>(Pin->GetNode()))
	{
		PinExpr = MakeExpr<FRigVMLiteralExprAST>(InPinProxy);
	}
	else
	{
		PinExpr = MakeExpr<FRigVMVarExprAST>(FRigVMExprAST::EType::Var, InPinProxy);
	}

	PinExpr->AddParent(InParentExpr);
	PinExpr->Name = *Pin->GetPinPath();
	SubjectToExpression.Add(InPinProxy, PinExpr);

	if (Pin->IsExecuteContext())
	{
		return PinExpr;
	}

	if (PinExpr->IsA(FRigVMExprAST::ExternalVar))
	{
		return PinExpr;
	}

	if ((Pin->GetDirection() == ERigVMPinDirection::IO ||
		Pin->GetDirection() == ERigVMPinDirection::Input)
		&& !Pin->IsExecuteContext())
	{
		bool bHasSourceLinkToRoot = false;
		URigVMPin* RootPin = Pin->GetRootPin();
		for (const int32 LinkIndex : LinkIndices)
		{
			if (Links[LinkIndex].TargetProxy.GetSubject() == RootPin)
			{
				bHasSourceLinkToRoot = true;
				break;
			}
		}

		if (!bHasSourceLinkToRoot && 
			GetSourceLinkIndices(InPinProxy, false).Num() == 0 &&
			(Pin->GetDirection() == ERigVMPinDirection::IO || LinkIndices.Num() > 0))
		{
			FRigVMLiteralExprAST* LiteralExpr = MakeExpr<FRigVMLiteralExprAST>(InPinProxy);
			const FRigVMASTLinkDescription LiteralLink(InPinProxy, InPinProxy, FString());
			FRigVMCopyExprAST* LiteralCopyExpr = MakeExpr<FRigVMCopyExprAST>(LiteralLink);
			LiteralCopyExpr->Name = *GetLinkAsString(LiteralCopyExpr->GetLink());
			LiteralCopyExpr->AddParent(PinExpr);
			LiteralExpr->AddParent(LiteralCopyExpr);
			LiteralExpr->Name = *Pin->GetPinPath();

			SubjectToExpression[InPinProxy] = LiteralExpr;
		}
	}

	FRigVMExprAST* ParentExprForLinks = PinExpr;

	if ((Pin->GetDirection() == ERigVMPinDirection::IO || Pin->GetDirection() == ERigVMPinDirection::Input) &&
		(InParentExpr->IsA(FRigVMExprAST::If) || InParentExpr->IsA(FRigVMExprAST::Select)) &&
		LinkIndices.Num() > 0)
	{
		FRigVMBlockExprAST* BlockExpr = MakeExpr<FRigVMBlockExprAST>(FRigVMExprAST::EType::Block, FRigVMASTProxy());
		BlockExpr->AddParent(PinExpr);
		BlockExpr->Name = Pin->GetFName();
		ParentExprForLinks = BlockExpr;
	}

	for (const int32 LinkIndex : LinkIndices)
	{
		TraverseLink(LinkIndex, ParentExprForLinks);
	}

	return PinExpr;
}

FRigVMExprAST* FRigVMParserAST::TraverseLink(int32 InLinkIndex, FRigVMExprAST* InParentExpr)
{
	const FRigVMASTLinkDescription& Link = Links[InLinkIndex];
	const FRigVMASTProxy& SourceProxy = Link.SourceProxy;
	const FRigVMASTProxy& TargetProxy = Link.TargetProxy;
	URigVMPin* SourcePin = SourceProxy.GetSubjectChecked<URigVMPin>();
	URigVMPin* TargetPin = TargetProxy.GetSubjectChecked<URigVMPin>();
	URigVMPin* SourceRootPin = SourcePin->GetRootPin();
	URigVMPin* TargetRootPin = TargetPin->GetRootPin();
	FRigVMASTProxy SourceNodeProxy = SourceProxy.GetSibling(SourcePin->GetNode());

	bool bRequiresCopy = SourceRootPin != SourcePin || TargetRootPin != TargetPin || !Link.SegmentPath.IsEmpty();
	if (!bRequiresCopy)
	{
		if(Cast<URigVMVariableNode>(TargetRootPin->GetNode()))
		{
			bRequiresCopy = true;
		}
	}

	FRigVMAssignExprAST* AssignExpr = nullptr;
	if (bRequiresCopy)
	{
		AssignExpr = MakeExpr<FRigVMCopyExprAST>(Link);
	}
	else
	{
		AssignExpr = MakeExpr<FRigVMAssignExprAST>(FRigVMExprAST::EType::Assign, Link);
	}

	AssignExpr->Name = *GetLinkAsString(Link);
	AssignExpr->AddParent(InParentExpr);

	FRigVMExprAST* NodeExpr = TraverseNode(SourceNodeProxy, AssignExpr);
	if (NodeExpr)
	{
		// if this is a copy expression - we should require the copy to use a ref instead
		if (NodeExpr->IsA(FRigVMExprAST::EType::CallExtern) ||
			NodeExpr->IsA(FRigVMExprAST::EType::If) ||
			NodeExpr->IsA(FRigVMExprAST::EType::Select) ||
			NodeExpr->IsA(FRigVMExprAST::EType::Array))
		{
			for (FRigVMExprAST* ChildExpr : *NodeExpr)
			{
				if (ChildExpr->IsA(FRigVMExprAST::EType::Var))
				{
					FRigVMVarExprAST* VarExpr = ChildExpr->To<FRigVMVarExprAST>();
					if (VarExpr->GetPin() == SourceRootPin)
					{
						if (VarExpr->SupportsSoftLinks())
						{
							AssignExpr->ReplaceChild(NodeExpr, VarExpr);
							return AssignExpr;
						}

						FRigVMCachedValueExprAST* CacheExpr = nullptr;
						for (FRigVMExprAST* VarExprParent : VarExpr->Parents)
						{
							if (VarExprParent->IsA(FRigVMExprAST::EType::CachedValue))
							{
								CacheExpr = VarExprParent->To<FRigVMCachedValueExprAST>();
								break;
							}
						}

						if (CacheExpr == nullptr)
						{
							CacheExpr = MakeExpr<FRigVMCachedValueExprAST>(FRigVMASTProxy());
							CacheExpr->Name = AssignExpr->GetName();
							VarExpr->AddParent(CacheExpr);
							NodeExpr->AddParent(CacheExpr);
						}

						AssignExpr->ReplaceChild(NodeExpr, CacheExpr);
						return AssignExpr;
					}
				}
			}
			checkNoEntry();
		}
	}

	return AssignExpr;
}

void FRigVMParserAST::FoldEntries()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	TArray<FRigVMExprAST*> FoldRootExpressions;
	TArray<FRigVMExprAST*> ExpressionsToRemove;
	TMap<FName, FRigVMEntryExprAST*> EntryByName;

	for (FRigVMExprAST* RootExpr : RootExpressions)
	{
		if (RootExpr->IsA(FRigVMExprAST::EType::Entry))
		{
			FRigVMEntryExprAST* Entry = RootExpr->To<FRigVMEntryExprAST>();
			if (EntryByName.Contains(Entry->GetEventName()))
			{
				FRigVMEntryExprAST* FoldEntry = EntryByName.FindChecked(Entry->GetEventName());

				// replace the original entry with a noop
				FRigVMNoOpExprAST* NoOpExpr = MakeExpr<FRigVMNoOpExprAST>(Entry->GetProxy());
				NoOpExpr->AddParent(FoldEntry);
				NoOpExpr->Name = Entry->Name;
				SubjectToExpression.FindChecked(Entry->GetProxy()) = NoOpExpr;

				TArray<FRigVMExprAST*> Children = Entry->Children; // copy since the loop changes the array
				for (FRigVMExprAST* ChildExpr : Children)
				{
					ChildExpr->RemoveParent(Entry);
					if (ChildExpr->IsA(FRigVMExprAST::Var))
					{
						if (ChildExpr->To<FRigVMVarExprAST>()->IsExecuteContext())
						{
							ExpressionsToRemove.AddUnique(ChildExpr);
							continue;
						}
					}
					ChildExpr->AddParent(FoldEntry);
				}
				ExpressionsToRemove.AddUnique(Entry);
			}
			else
			{
				FoldRootExpressions.Add(Entry);
				EntryByName.Add(Entry->GetEventName(), Entry);
			}
		}
		else
		{
			FoldRootExpressions.Add(RootExpr);
		}
	}

	RootExpressions = FoldRootExpressions;

	RemoveExpressions(ExpressionsToRemove);
}

void FRigVMParserAST::InjectExitsToEntries()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	for (FRigVMExprAST* RootExpr : RootExpressions)
	{
		if (RootExpr->IsA(FRigVMExprAST::EType::Entry))
		{
			bool bHasExit = false;
			if (RootExpr->Children.Num() > 0)
			{
				if (RootExpr->Children.Last()->IsA(FRigVMExprAST::EType::Exit))
				{
					bHasExit = true;
					break;
				}
			}

			if (!bHasExit)
			{
				FRigVMExprAST* ExitExpr = MakeExpr<FRigVMExitExprAST>(FRigVMASTProxy());
				ExitExpr->AddParent(RootExpr);
			}
		}
	}
}

void FRigVMParserAST::BubbleUpExpressions()
{
	for (int32 ExpressionIndex = 0; ExpressionIndex < Expressions.Num(); ExpressionIndex++)
	{
		FRigVMExprAST* Expression = Expressions[ExpressionIndex];
		if (!Expression->IsA(FRigVMExprAST::CachedValue))
		{
			continue;
		}

		if (Expression->NumParents() < 2)
		{
			continue;
		}

		// collect all of the blocks this is in and make sure it's bubbled up before that
		TArray<FRigVMBlockExprAST*> Blocks;
		for (int32 ParentIndex = 0; ParentIndex < Expression->NumParents(); ParentIndex++)
		{
			const FRigVMExprAST* ParentExpression = Expression->ParentAt(ParentIndex);
			if (ParentExpression->IsA(FRigVMExprAST::Block))
			{
				Blocks.AddUnique((FRigVMBlockExprAST*)ParentExpression->To<FRigVMBlockExprAST>());
			}
			else
			{
				Blocks.AddUnique((FRigVMBlockExprAST*)ParentExpression->GetBlock());
			}
		}

		if (Blocks.Num() > 1)
		{
			TArray<FRigVMBlockExprAST*> BlockCandidates;
			BlockCandidates.Append(Blocks);
			FRigVMBlockExprAST* OuterBlock = nullptr;

			// deal with a case where an expression is linked within both the true and false case of an "if" node
			if(Blocks.Num() == 2)
			{
				const FRigVMExprAST* Parent0 = Blocks[0]->GetParent();
				const FRigVMExprAST* Parent1 = Blocks[1]->GetParent();
				if(Parent0 && Parent1)
				{
					const FRigVMExprAST* GrandParent0 = Parent0->GetParent();
					const FRigVMExprAST* GrandParent1 = Parent1->GetParent();
					if(GrandParent0 && GrandParent1 && GrandParent0 == GrandParent1)
					{
						if(GrandParent0->IsA(FRigVMExprAST::EType::If))
						{
							const FRigVMIfExprAST* IfExpression = GrandParent0->To<FRigVMIfExprAST>();
							const FRigVMExprAST* ConditionBlockExpression = IfExpression->GetConditionExpr()->GetFirstChildOfType(FRigVMExprAST::Block);
							if(ConditionBlockExpression)
							{
								OuterBlock = (FRigVMBlockExprAST*)ConditionBlockExpression->To<FRigVMBlockExprAST>();
								OuterBlock->Children.Add(Expression);
								Expression->Parents.Insert(OuterBlock, 0);
								continue;
							}
						}
					}
				}
			}

			// this expression is part of multiple blocks, and it needs to be bubbled up.
			// for this we'll walk up the block tree and find the first block which contains all of them
			for (int32 BlockCandidateIndex = 0; BlockCandidateIndex < BlockCandidates.Num(); BlockCandidateIndex++)
			{
				FRigVMBlockExprAST* BlockCandidate = BlockCandidates[BlockCandidateIndex];

				bool bFoundCandidate = true;
				TMap<const FRigVMExprAST*, bool> ContainedExpressionsCache;
				for (int32 BlockIndex = 0; BlockIndex < Blocks.Num(); BlockIndex++)
				{
					FRigVMBlockExprAST* Block = Blocks[BlockIndex];
					if (!BlockCandidate->Contains(Block, &ContainedExpressionsCache))
					{
						bFoundCandidate = false;
						break;
					}
				}

				if (bFoundCandidate)
				{
					OuterBlock = BlockCandidate;
					break;
				}

				BlockCandidates.AddUnique((FRigVMBlockExprAST*)BlockCandidate->GetBlock());
			}

			// we found a block which contains all of our blocks.
			// we are now going to inject this block as the first parent
			// of the cached value, so that the traverser sees it earlier
			if (OuterBlock)
			{
				MinIndexOfChildWithinParent.Reset();
				int32 ChildIndex = Expression->GetMinChildIndexWithinParent(OuterBlock);
				if (ChildIndex != INDEX_NONE)
				{
					OuterBlock->Children.Insert(Expression, ChildIndex);
					Expression->Parents.Insert(OuterBlock, 0);
				}
			}
		}
	}
}

void FRigVMParserAST::RefreshExprIndices()
{
	for (int32 Index = 0; Index < Expressions.Num(); Index++)
	{
		Expressions[Index]->Index = Index;
	}
}

void FRigVMParserAST::FoldNoOps()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	for (FRigVMExprAST* Expression : Expressions)
	{
		if (Expression->IsA(FRigVMExprAST::EType::NoOp))
		{
			if (URigVMNode* Node = Expression->To<FRigVMNoOpExprAST>()->GetNode())
			{
				if (URigVMVariableNode* VariableNode = Cast<URigVMVariableNode>(Node))
				{
					if (!VariableNode->IsGetter())
					{
						continue;
					}
				}
			}
			// copy since we are changing the content during iteration below
			TArray<FRigVMExprAST*> Children = Expression->Children;
			TArray<FRigVMExprAST*> Parents = Expression->Parents;

			for (FRigVMExprAST* Parent : Parents)
			{
				Expression->RemoveParent(Parent);
			}

			for (FRigVMExprAST* Child : Children)
			{
				Child->RemoveParent(Expression);
				for (FRigVMExprAST* Parent : Parents)
				{
					Child->AddParent(Parent);
				}
			}
		}
	}
}

void FRigVMParserAST::FoldAssignments()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	TArray<FRigVMExprAST*> ExpressionsToRemove;

	// first - Fold all assignment chains
	for (FRigVMExprAST* Expression : Expressions)
	{
		if (Expression->Parents.Num() == 0)
		{
			continue;
		}

		if (Expression->GetType() != FRigVMExprAST::EType::Assign)
		{
			continue;
		}

		FRigVMAssignExprAST* AssignExpr = Expression->To<FRigVMAssignExprAST>();
		if (AssignExpr->Parents.Num() != 1 || AssignExpr->Children.Num() != 1)
		{
			continue;
		}

		URigVMPin* SourcePin = AssignExpr->GetSourcePin();
		URigVMPin* TargetPin = AssignExpr->GetTargetPin();

		// it's possible that we'll see copies from an element onto an array
		// here. we'll need to ignore these links and leave them since they
		// represent copies.
		if(SourcePin->IsArray() != TargetPin->IsArray())
		{
			continue;
		}

		// in case the assign has different types for left and right - we need to avoid folding
		// since this assign represents a cast operation
		if(SourcePin->GetCPPTypeObject() != TargetPin->GetCPPTypeObject())
		{
			continue;
		}
		else if(SourcePin->GetCPPTypeObject() == nullptr)
		{
			if(SourcePin->GetCPPType() != TargetPin->GetCPPType())
			{
				continue;
			}
		}
		
		// non-input pins on anything but a reroute / array node should be skipped
		if (TargetPin->GetDirection() != ERigVMPinDirection::Input &&
			(Cast<URigVMRerouteNode>(TargetPin->GetNode()) == nullptr))
		{
			continue;
		}

		// if this node is a loop node - let's skip the folding
		if (URigVMUnitNode* UnitNode = Cast<URigVMUnitNode>(TargetPin->GetNode()))
		{
			if (UnitNode->IsLoopNode())
			{
				continue;
			}
		}

		// if this node is a variable node and the pin requires a watch... skip this
		if (Cast<URigVMVariableNode>(SourcePin->GetNode()))
		{
			if(SourcePin->RequiresWatch(true))
			{
				continue;
			}
		}
		
		// if this node is an array iterator node - let's skip the folding
		if (URigVMArrayNode* ArrayNode = Cast<URigVMArrayNode>(TargetPin->GetNode()))
		{
			if (ArrayNode->IsLoopNode())
			{
				continue;
			}
		}

		FRigVMExprAST* Parent = AssignExpr->Parents[0];
		if (!Parent->IsA(FRigVMExprAST::EType::Var))
		{
			continue;
		}

		// To prevent bad assignments in LWC for VMs compiled in non LWC, we do not allow folding of assignments
		// to/from external variables of type float
		{
			if (SourcePin->GetCPPType() == TEXT("float") || SourcePin->GetCPPType() == TEXT("TArray<float>"))
			{
				if (URigVMVariableNode* SourceVariableNode = Cast<URigVMVariableNode>(SourcePin->GetNode()))
				{
					if (!SourceVariableNode->IsInputArgument() && !SourceVariableNode->IsLocalVariable())
					{
						continue;
					}
				}
			}
			if (TargetPin->GetCPPType() == TEXT("float") || TargetPin->GetCPPType() == TEXT("TArray<float>"))
			{
				if (URigVMVariableNode* TargetVariableNode = Cast<URigVMVariableNode>(TargetPin->GetNode()))
				{
					if (!TargetVariableNode->IsInputArgument() && !TargetVariableNode->IsLocalVariable())
					{
						continue;
					}
				}
			}
		}

		FRigVMExprAST* Child = AssignExpr->Children[0];
		AssignExpr->RemoveParent(Parent);
		Child->RemoveParent(AssignExpr);

		TArray<FRigVMExprAST*> GrandParents = Parent->Parents;
		for (FRigVMExprAST* GrandParent : GrandParents)
		{
			GrandParent->ReplaceChild(Parent, Child);
			if (GrandParent->IsA(FRigVMExprAST::EType::Assign))
			{
				FRigVMAssignExprAST* GrandParentAssign = GrandParent->To<FRigVMAssignExprAST>();
				GrandParentAssign->Link = FRigVMASTLinkDescription(AssignExpr->GetSourceProxy(),
					GrandParentAssign->GetTargetProxy(), FString());
				GrandParentAssign->Name = *GetLinkAsString(GrandParentAssign->GetLink());
			}
		}

		ExpressionsToRemove.AddUnique(AssignExpr);
		if (Parent->Parents.Num() == 0)
		{
			ExpressionsToRemove.AddUnique(Parent);
		}
	}

	RemoveExpressions(ExpressionsToRemove);
}
bool FRigVMParserAST::FoldConstantValuesToLiterals(TArray<URigVMGraph*> InGraphs, URigVMController* InController, const TArray<FRigVMExternalVariable>& InExternalVariables, const TArray<FRigVMUserDataArray>& InRigVMUserData)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	if (InController == nullptr)
	{
		return false;
	}

	if (InRigVMUserData.Num() == 0)
	{
		return false;
	}

	// loop over all call externs and figure out if they are a non-const node
	// with one or more const pins. we then build a temporary VM to run the part of the 
	// graph, and pull out the required values - we then bake the value into a literal 
	// and remove the tree that created the value.

	TMap<FString, FString> ComputedDefaultValues;
	TArray<FRigVMASTProxy> PinsToUpdate;
	TArray<FRigVMASTProxy> RootPinsToUpdate;
	TArray<FRigVMASTProxy> PinsToCompute;
	TArray<FRigVMASTProxy> NodesToCompute;

	for (const FRigVMASTProxy& NodeProxy : NodeProxies)
	{
		if (NodeProxy.IsA<URigVMVariableNode>() ||
			NodeProxy.IsA<URigVMEnumNode>())
		{
			continue;
		}

		FRigVMExprAST** NodeExprPtr = SubjectToExpression.Find(NodeProxy);
		if(NodeExprPtr == nullptr)
		{
			continue;
		}

		FRigVMExprAST* NodeExpr = *NodeExprPtr;
		if (NodeExpr->IsConstant())
		{
			continue;
		}

		URigVMNode* Node = NodeProxy.GetSubjectChecked<URigVMNode>();
		const TArray<URigVMPin*> Pins = Node->GetPins();
		for (URigVMPin* Pin : Pins)
		{
			if (Pin->GetDirection() != ERigVMPinDirection::Input &&
				Pin->GetDirection() != ERigVMPinDirection::IO)
			{
				continue;
			}

			FRigVMASTProxy PinProxy = NodeProxy.GetSibling(Pin);
			FRigVMExprAST** PinExprPtr = SubjectToExpression.Find(PinProxy);
			if (PinExprPtr == nullptr)
			{
				continue;
			}
			FRigVMExprAST* PinExpr = *PinExprPtr;
			if (PinExpr->IsA(FRigVMExprAST::EType::Literal))
			{
				if (const FRigVMExprAST* VarPinExpr = PinExpr->GetFirstParentOfType(FRigVMExprAST::EType::Var))
				{
					if (VarPinExpr->GetName() == PinExpr->GetName())
					{
						PinExpr = (FRigVMExprAST*)VarPinExpr;
					}
				}

				// if we are still a literal, carry on
				if (PinExpr->IsA(FRigVMExprAST::EType::Literal))
				{
					continue;
				}
			}

			TArray<int32> LinkIndices = GetSourceLinkIndices(PinProxy, true);
			if (LinkIndices.Num() == 0)
			{
				continue;
			}

			if (!PinExpr->IsConstant())
			{
				continue;
			}

			bool bFoundValidSourcePin = false;
			for (const int32 LinkIndex : LinkIndices)
			{
				const FRigVMASTProxy& SourcePinProxy = Links[LinkIndex].SourceProxy;
				URigVMNode* SourceNode = SourcePinProxy.GetSubjectChecked<URigVMPin>()->GetNode();
				FRigVMASTProxy SourceNodeProxy = SourcePinProxy.GetSibling(SourceNode);

				check(SourceNodeProxy.IsValid());

				if (SourceNodeProxy.IsA<URigVMVariableNode>() ||
					SourceNodeProxy.IsA<URigVMRerouteNode>() ||
					SourceNodeProxy.IsA<URigVMEnumNode>())
				{
					continue;
				}

				PinsToCompute.AddUnique(SourcePinProxy);
				NodesToCompute.AddUnique(SourceNodeProxy);
				bFoundValidSourcePin = true;
			}

			if (bFoundValidSourcePin)
			{
				PinsToUpdate.Add(PinProxy);
				RootPinsToUpdate.AddUnique(PinProxy.GetSibling(Pin->GetRootPin()));
			}
		}
	}

	if (NodesToCompute.Num() == 0)
	{
		return false;
	}

	// add all of the additional nodes driving these
	for(int32 NodeToComputeIndex = 0; NodeToComputeIndex < NodesToCompute.Num(); NodeToComputeIndex++)
	{
		FRigVMASTProxy ProxyToCompute = NodesToCompute[NodeToComputeIndex];
		if(URigVMNode* NodeToCompute = ProxyToCompute.GetSubject<URigVMNode>())
		{
			// first find all of the source node of this node directly
			TArray<URigVMNode*> SourceNodes = NodeToCompute->GetLinkedSourceNodes();
			for(URigVMNode* SourceNode : SourceNodes)
			{
				FRigVMASTProxy SourceProxy = ProxyToCompute.GetSibling(SourceNode);
				NodesToCompute.AddUnique(SourceProxy);
			}

			// if the node is a library node - also add the return node
			if(URigVMLibraryNode* LibraryToCompute = Cast<URigVMLibraryNode>(NodeToCompute))
			{
				if(URigVMNode* ReturnNodeToCompute = LibraryToCompute->GetReturnNode())
				{
					const FRigVMASTProxy LibraryProxy = ProxyToCompute.GetSibling(LibraryToCompute);
					const FRigVMASTProxy ReturnProxy = LibraryProxy.GetChild(ReturnNodeToCompute);
					NodesToCompute.AddUnique(ReturnProxy);
				}
			}
      
			// if the node is an entry node - also add host library node
			if(NodeToCompute->IsA<URigVMFunctionEntryNode>())
			{
				check(ProxyToCompute.GetCallstack().Num() > 1);
				const FRigVMASTProxy LibraryProxy = ProxyToCompute.GetParent();
				NodesToCompute.AddUnique(LibraryProxy);
			}
		}
	}

	// we now know the node we need to run.
	// let's build an temporary AST which has only those nodes
	TSharedPtr<FRigVMParserAST> TempAST = MakeShareable(new FRigVMParserAST(InGraphs, NodesToCompute));

	// share the pin overrides with the constant folding AST to ensure
	// the complete view of default values across function references is available
	// in the subset AST.
	TempAST->PinOverrides = PinOverrides;

	// build the VM to run this AST
	TMap<FString, FRigVMOperand> Operands;
	check(!InGraphs.IsEmpty());
	URigVM* TempVM = NewObject<URigVM>(InGraphs[0]);
	
	URigVMCompiler* TempCompiler = NewObject<URigVMCompiler>(GetTransientPackage());
	TempCompiler->Settings.SetupNodeInstructionIndex = false;
	TempCompiler->Settings.IsPreprocessorPhase = true;
	TempCompiler->Settings.EnablePinWatches = false;
	TempCompiler->Settings.ASTSettings = FRigVMParserASTSettings::Fast();

	TempCompiler->Compile(InGraphs, InController, TempVM, InExternalVariables, InRigVMUserData, &Operands, TempAST);

	TArray<URigVMMemoryStorage*> Memory;
	Memory.Add(TempVM->GetWorkMemory());
	Memory.Add(TempVM->GetLiteralMemory());
	Memory.Add(TempVM->GetDebugMemory());
	for (const FRigVMUserDataArray& RigVMUserData : InRigVMUserData)
	{
		TempVM->Execute(Memory, RigVMUserData);
	}

	// copy the values out of the temp VM and set them on the cached value
	for (const FRigVMASTProxy& PinToComputeProxy : PinsToCompute)
	{
		TGuardValue<bool> GuardControllerNotifs(InController->bSuspendNotifications, true);

		URigVMPin* PinToCompute = PinToComputeProxy.GetSubjectChecked<URigVMPin>();
		URigVMPin* RootPin = PinToCompute->GetRootPin();
		FRigVMASTProxy RootPinProxy = PinToComputeProxy.GetSibling(RootPin);

		FRigVMVarExprAST* RootVarExpr = nullptr;
		FRigVMExprAST** RootPinExprPtr = SubjectToExpression.Find(RootPinProxy);
		if (RootPinExprPtr != nullptr)
		{
			FRigVMExprAST* RootPinExpr = *RootPinExprPtr;
			if (RootPinExpr->IsA(FRigVMExprAST::EType::Var))
			{
				RootVarExpr = RootPinExpr->To<FRigVMVarExprAST>();
			}
		}

		FString PinHash = URigVMCompiler::GetPinHash(RootPin, RootVarExpr, false);
		const FRigVMOperand* OperandPtr = Operands.Find(PinHash);
		if(OperandPtr == nullptr)
		{
			continue;
		}
		const FRigVMOperand& Operand = *OperandPtr;

		FString DefaultValue;
		if(Operand.GetMemoryType() == ERigVMMemoryType::Work)
		{
			DefaultValue = TempVM->GetWorkMemory()->GetDataAsString(Operand.GetRegisterIndex());
		}
		else if(Operand.GetMemoryType() == ERigVMMemoryType::Literal)
		{
			DefaultValue = TempVM->GetLiteralMemory()->GetDataAsString(Operand.GetRegisterIndex());
		}
		if(DefaultValue.IsEmpty())
		{
			continue;
		}

		// FString TempDefaultValue = FString::Printf(TEXT("(%s)"), *FString::Join(DefaultValues, TEXT(",")));
		// UE_LOG(LogRigVMDeveloper, Display, TEXT("Computed constant value '%s' = '%s'"), *PinHash, *TempDefaultValue);

		TArray<FString> SegmentNames;
		if (!URigVMPin::SplitPinPath(PinToCompute->GetSegmentPath(), SegmentNames))
		{
			SegmentNames.Add(PinToCompute->GetName());
		}

		URigVMPin* PinForDefaultValue = RootPin;
		while (PinForDefaultValue != PinToCompute && SegmentNames.Num() > 0)
		{
			TArray<FString> SplitDefaultValues = URigVMPin::SplitDefaultValue(DefaultValue);

			if (PinForDefaultValue->IsArray())
			{
				int32 ElementIndex = FCString::Atoi(*SegmentNames[0]);
				DefaultValue = SplitDefaultValues[ElementIndex];
				PinForDefaultValue = PinForDefaultValue->GetSubPins()[ElementIndex];
				URigVMController::PostProcessDefaultValue(PinForDefaultValue, DefaultValue);
				SegmentNames.RemoveAt(0);
			}
			else if (PinForDefaultValue->IsStruct())
			{
				if(SplitDefaultValues.IsEmpty())
				{
					break;
				}
				
				for (const FString& MemberNameValuePair : SplitDefaultValues)
				{
					FString MemberName, MemberValue;
					if (MemberNameValuePair.Split(TEXT("="), &MemberName, &MemberValue))
					{
						if (MemberName == SegmentNames[0])
						{
							URigVMPin* SubPin = PinForDefaultValue->FindSubPin(MemberName);
							if (SubPin == nullptr)
							{
								SegmentNames.Reset();
								break;
							}

							DefaultValue = MemberValue;
							PinForDefaultValue = SubPin;
							URigVMController::PostProcessDefaultValue(PinForDefaultValue, DefaultValue);
							SegmentNames.RemoveAt(0);
							break;
						}
					}
				}
			}
			else
			{
				checkNoEntry();
			}
		}

		const TArray<int32> LinkIndices = GetTargetLinkIndices(PinToComputeProxy);
		for (const int32 LinkIndex : LinkIndices)
		{
			URigVMPin::FPinOverrideValue OverrideValue;
			OverrideValue.DefaultValue = DefaultValue;
			PinOverrides.FindOrAdd(Links[LinkIndex].TargetProxy) = OverrideValue;
		}
	}

	// now remove all of the expressions no longer needed
	TArray<FRigVMExprAST*> ExpressionsToRemove;
	for (const FRigVMASTProxy& RootPinToUpdateProxy : RootPinsToUpdate)
	{
		FRigVMExprAST** PreviousExprPtr = SubjectToExpression.Find(RootPinToUpdateProxy);
		if (PreviousExprPtr)
		{
			FRigVMVarExprAST* PreviousVarExpr = (*PreviousExprPtr)->To<FRigVMVarExprAST>();

			// if the previous var expression is a literal used to initialize a var
			// (for example on an IO pin, or when we are driving sub pins)
			if (PreviousVarExpr->IsA(FRigVMExprAST::EType::Literal))
			{
				bool bRedirectedVar = false;
				for (int32 ParentIndex = 0; ParentIndex < PreviousVarExpr->NumParents(); ParentIndex++)
				{
					const FRigVMExprAST* ParentExpr = PreviousVarExpr->ParentAt(ParentIndex);
					if (ParentExpr->IsA(FRigVMExprAST::EType::Assign))
					{
						for (int32 GrandParentIndex = 0; GrandParentIndex < ParentExpr->NumParents(); GrandParentIndex++)
						{
							const FRigVMExprAST* GrandParentExpr = ParentExpr->ParentAt(GrandParentIndex);
							if (GrandParentExpr->IsA(FRigVMExprAST::EType::Block))
							{
								GrandParentExpr = GrandParentExpr->GetParent();
							}
							if (GrandParentExpr->IsA(FRigVMExprAST::EType::Var) && (GrandParentExpr->GetName() == PreviousVarExpr->GetName()))
							{
								PreviousVarExpr = (FRigVMVarExprAST*)GrandParentExpr->To<FRigVMVarExprAST>();
								bRedirectedVar = true;
								break;
							}
						}
					}

					if (bRedirectedVar)
					{
						break;
					}
				}
			}

			FRigVMLiteralExprAST* LiteralExpr = MakeExpr<FRigVMLiteralExprAST>(RootPinToUpdateProxy);
			LiteralExpr->Name = PreviousVarExpr->Name;
			SubjectToExpression[RootPinToUpdateProxy] = LiteralExpr;
			PreviousVarExpr->ReplaceBy(LiteralExpr);
			ExpressionsToRemove.Add(PreviousVarExpr);
		}
	}

	TempVM->Rename(nullptr, GetTransientPackage(), REN_ForceNoResetLoaders | REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional);
	TempVM->MarkAsGarbage();

	RemoveExpressions(ExpressionsToRemove);

	return ExpressionsToRemove.Num() > 0;
}

bool FRigVMParserAST::FoldUnreachableBranches(TArray<URigVMGraph*> InGraphs)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	TArray<FRigVMExprAST*> ExpressionsToRemove;

	for (const FRigVMASTProxy& NodeProxy : NodeProxies)
	{
		if (NodeProxy.IsA<URigVMVariableNode>())
		{
			continue;
		}

		//URigVMNode* Node = NodeProxy.GetSubjectChecked<URigVMNode>();

		FRigVMExprAST** NodeExprPtr = SubjectToExpression.Find(NodeProxy);
		if (NodeExprPtr == nullptr)
		{
			continue;
		}

		FRigVMExprAST* NodeExpr = *NodeExprPtr;
		if (NodeExpr->NumParents() == 0)
		{
			continue;
		}

		if (NodeExpr->IsA(FRigVMExprAST::EType::Branch))
		{
			const FRigVMBranchExprAST* BranchExpr = NodeExpr->To<FRigVMBranchExprAST>();
			FRigVMExprAST* ExprReplacement = nullptr;

			if (BranchExpr->IsAlwaysTrue())
			{
				ExprReplacement = (FRigVMExprAST*)BranchExpr->GetTrueExpr();
			}
			else if (BranchExpr->IsAlwaysFalse())
			{
				ExprReplacement = (FRigVMExprAST*)BranchExpr->GetFalseExpr();
			}

			if (ExprReplacement)
			{
				if (ExprReplacement->NumChildren() == 1)
				{
					ExprReplacement = (FRigVMExprAST*)ExprReplacement->ChildAt(0);
					if (ExprReplacement->IsA(FRigVMExprAST::EType::Block))
					{
						ExprReplacement->RemoveParent((FRigVMExprAST*)ExprReplacement->GetParent());
						NodeExpr->ReplaceBy(ExprReplacement);
						ExpressionsToRemove.Add(NodeExpr);
					}
				}
			}
		}
		else
		{
			FRigVMExprAST* CachedValueExpr = (FRigVMExprAST*)NodeExpr->GetParent();
			if (!CachedValueExpr->IsA(FRigVMExprAST::EType::CachedValue))
			{
				continue;
			}

			FRigVMExprAST* ExprReplacement = nullptr;
			if (NodeExpr->IsA(FRigVMExprAST::EType::If))
			{
				const FRigVMIfExprAST* IfExpr = NodeExpr->To<FRigVMIfExprAST>();
				if (IfExpr->IsAlwaysTrue())
				{
					ExprReplacement = (FRigVMExprAST*)IfExpr->GetTrueExpr();
				}
				else if (IfExpr->IsAlwaysFalse())
				{
					ExprReplacement = (FRigVMExprAST*)IfExpr->GetFalseExpr();
				}
			}
			else if (NodeExpr->IsA(FRigVMExprAST::EType::Select))
			{
				const FRigVMSelectExprAST* SelectExpr = NodeExpr->To<FRigVMSelectExprAST>();
				int32 ConstantCaseIndex = SelectExpr->GetConstantValueIndex();
				if (ConstantCaseIndex != INDEX_NONE)
				{
					ExprReplacement = (FRigVMExprAST*)SelectExpr->GetValueExpr(ConstantCaseIndex);
				}
			}

			if (ExprReplacement)
			{
				ExprReplacement->RemoveParent((FRigVMExprAST*)ExprReplacement->GetParent());
				CachedValueExpr->ReplaceBy(ExprReplacement);
				ExpressionsToRemove.Add(CachedValueExpr);
			}
		}
	}

	RemoveExpressions(ExpressionsToRemove);
	return ExpressionsToRemove.Num() > 0;
}

void FRigVMParserAST::FoldLiterals()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	TMap<FString, FRigVMLiteralExprAST*> ValueToLiteral;
	TArray<FRigVMExprAST*> ExpressionsToRemove;

	for (int32 ExpressionIndex = 0; ExpressionIndex < Expressions.Num(); ExpressionIndex++)
	{
		FRigVMExprAST* Expression = Expressions[ExpressionIndex];
		if (Expression->Parents.Num() == 0)
		{
			continue;
		}

		if (Expression->GetType() == FRigVMExprAST::EType::Literal)
		{
			ensure(Expression->Children.Num() == 0);

			FRigVMLiteralExprAST* LiteralExpr = Expression->To<FRigVMLiteralExprAST>();
			FString DefaultValue = LiteralExpr->GetDefaultValue();
			if (DefaultValue.IsEmpty())
			{
				if (LiteralExpr->GetCPPType() == TEXT("bool"))
				{
					DefaultValue = TEXT("False");
				}
				else if (LiteralExpr->GetCPPType() == TEXT("float"))
				{
					DefaultValue = TEXT("0.000000");
				}
				else if (LiteralExpr->GetCPPType() == TEXT("double"))
				{
					DefaultValue = TEXT("0.000000");
				}
				else if (LiteralExpr->GetCPPType() == TEXT("int32"))
				{
					DefaultValue = TEXT("0");
				}
				else
				{
					continue;
				}
			}

			FString Hash = FString::Printf(TEXT("[%s] %s"), *LiteralExpr->GetCPPType(), *DefaultValue);

			FRigVMLiteralExprAST* const* MappedExpr = ValueToLiteral.Find(Hash);
			if (MappedExpr)
			{
				TArray<FRigVMExprAST*> Parents = Expression->Parents;
				for (FRigVMExprAST* Parent : Parents)
				{
					Parent->ReplaceChild(Expression, *MappedExpr);
				}
				ExpressionsToRemove.AddUnique(Expression);
			}
			else
			{
				ValueToLiteral.Add(Hash, LiteralExpr);
			}
		}
	}

	RemoveExpressions(ExpressionsToRemove);
}

const FRigVMExprAST* FRigVMParserAST::GetExprForSubject(const FRigVMASTProxy& InProxy) const
{
	if (FRigVMExprAST* const* ExpressionPtr = SubjectToExpression.Find(InProxy))
	{
		return *ExpressionPtr;
	}
	return nullptr;
}

TArray<const FRigVMExprAST*> FRigVMParserAST::GetExpressionsForSubject(UObject* InSubject) const
{
	TArray<const FRigVMExprAST*> ExpressionsForSubject;
	
	for (TPair<FRigVMASTProxy, FRigVMExprAST*> Pair : SubjectToExpression)
	{
		if(Pair.Key.GetCallstack().Last() == InSubject)
		{
			ExpressionsForSubject.Add(Pair.Value);
		}
	}

	return ExpressionsForSubject;
}

void FRigVMParserAST::PrepareCycleChecking(URigVMPin* InPin)
{
	if (InPin == nullptr)
	{
		LastCycleCheckExpr = nullptr;
		CycleCheckFlags.Reset();
		return;
	}
	FRigVMASTProxy NodeProxy = FRigVMASTProxy::MakeFromUObject(InPin->GetNode());

	const FRigVMExprAST* Expression = nullptr;
	if (FRigVMExprAST* const* ExpressionPtr = SubjectToExpression.Find(NodeProxy))
	{
		Expression = *ExpressionPtr;
	}
	else
	{
		return;
	}

	if (LastCycleCheckExpr != Expression)
	{
		LastCycleCheckExpr = Expression;
		CycleCheckFlags.SetNumZeroed(Expressions.Num());
		CycleCheckFlags[LastCycleCheckExpr->GetIndex()] = ETraverseRelationShip_Self;
	}
}

bool FRigVMParserAST::CanLink(URigVMPin* InSourcePin, URigVMPin* InTargetPin, FString* OutFailureReason)
{
	if (InSourcePin == nullptr || InTargetPin == nullptr || InSourcePin == InTargetPin)
	{
		if (OutFailureReason)
		{
			*OutFailureReason = FString(TEXT("Provided objects contain nullptr."));
		}
		return false;
	}

	URigVMNode* SourceNode = InSourcePin->GetNode();
	URigVMNode* TargetNode = InTargetPin->GetNode();
	if (SourceNode == TargetNode)
	{
		if (OutFailureReason)
		{
			*OutFailureReason = FString(TEXT("Source and Target Nodes are identical."));
		}
		return false;
	}

	if(SourceNode->IsA<URigVMRerouteNode>())
	{
		TArray<URigVMPin*> SourcePins;
		for (URigVMPin* Pin : SourceNode->GetPins())
		{
			SourcePins.Append(Pin->GetLinkedSourcePins(true));
		}
		
		for (URigVMPin* SourcePin : SourcePins)
		{
			if (!CanLink(SourcePin, InTargetPin, OutFailureReason))
			{
				return false;
			}
		}
		return true;
	}

	if(TargetNode->IsA<URigVMRerouteNode>())
	{
		TArray<URigVMPin*> TargetPins;
		for (URigVMPin* Pin : TargetNode->GetPins())
		{
			TargetPins.Append(Pin->GetLinkedTargetPins(true));
		}
		
		for (URigVMPin* TargetPin : TargetPins)
		{
			if (!CanLink(InSourcePin, TargetPin, OutFailureReason))
			{
				return false;
			}
		}
		return true;
	}

	FRigVMASTProxy SourceNodeProxy = FRigVMASTProxy::MakeFromUObject(SourceNode);
	FRigVMASTProxy TargetNodeProxy = FRigVMASTProxy::MakeFromUObject(TargetNode);

	const FRigVMExprAST* SourceExpression = nullptr;
	if (FRigVMExprAST* const* SourceExpressionPtr = SubjectToExpression.Find(SourceNodeProxy))
	{
		SourceExpression = *SourceExpressionPtr;
	}
	else
	{
		if (OutFailureReason)
		{
			*OutFailureReason = FString(TEXT("Source node is not part of AST."));
		}
		return false;
	}

	const FRigVMVarExprAST* SourceVarExpression = nullptr;
	if (FRigVMExprAST* const* SourceVarExpressionPtr = SubjectToExpression.Find(SourceNodeProxy.GetSibling(InSourcePin->GetRootPin())))
	{
		if ((*SourceVarExpressionPtr)->IsA(FRigVMExprAST::EType::Var))
		{
			SourceVarExpression = (*SourceVarExpressionPtr)->To<FRigVMVarExprAST>();
		}
	}

	const FRigVMExprAST* TargetExpression = nullptr;
	if (FRigVMExprAST* const* TargetExpressionPtr = SubjectToExpression.Find(TargetNodeProxy))
	{
		TargetExpression = *TargetExpressionPtr;
	}
	else
	{
		if (OutFailureReason)
		{
			*OutFailureReason = FString(TEXT("Target node is not part of AST."));
		}
		return false;
	}

	const FRigVMBlockExprAST* SourceBlock = SourceExpression->GetBlock();
	const FRigVMBlockExprAST* TargetBlock = TargetExpression->GetBlock();
	if (SourceBlock == nullptr || TargetBlock == nullptr)
	{
		return false;
	}

	if (SourceBlock == TargetBlock ||
		SourceBlock->Contains(TargetBlock) ||
		TargetBlock->Contains(SourceBlock) ||
		TargetBlock->GetRootBlock()->Contains(SourceBlock) ||
		SourceBlock->GetRootBlock()->Contains(TargetBlock))
	{
		if (SourceVarExpression)
		{
			if (SourceVarExpression->SupportsSoftLinks())
			{
				return true;
			}
		}

		if (LastCycleCheckExpr != SourceExpression && LastCycleCheckExpr != TargetExpression)
		{
			PrepareCycleChecking(InSourcePin);
		}

		TArray<ETraverseRelationShip>& Flags = CycleCheckFlags;
		TraverseParents(LastCycleCheckExpr, [&Flags](const FRigVMExprAST* InExpr) -> bool {
			if (Flags[InExpr->GetIndex()] == ETraverseRelationShip_Self)
			{
				return true;
			}
			if (Flags[InExpr->GetIndex()] != ETraverseRelationShip_Unknown)
			{
				return false;
			}
			if (InExpr->IsA(FRigVMExprAST::EType::Var))
			{
				if (InExpr->To<FRigVMVarExprAST>()->SupportsSoftLinks())
				{
					return false;
				}
			}
			Flags[InExpr->GetIndex()] = ETraverseRelationShip_Parent;
			return true;
		});

		TraverseChildren(LastCycleCheckExpr, [&Flags](const FRigVMExprAST* InExpr) -> bool {
			if (Flags[InExpr->GetIndex()] == ETraverseRelationShip_Self)
			{
				return true;
			}
			if (Flags[InExpr->GetIndex()] != ETraverseRelationShip_Unknown)
			{
				return false;
			}
			if (InExpr->IsA(FRigVMExprAST::EType::Var))
			{
				if (InExpr->To<FRigVMVarExprAST>()->SupportsSoftLinks())
				{
					return false;
				}
			}
			Flags[InExpr->GetIndex()] = ETraverseRelationShip_Child;
			return true;
		});

		bool bFoundCycle = false;
		if (LastCycleCheckExpr == SourceExpression)
		{
			bFoundCycle = Flags[TargetExpression->GetIndex()] == ETraverseRelationShip_Child;
		}
		else
		{
			bFoundCycle = Flags[SourceExpression->GetIndex()] == ETraverseRelationShip_Parent;
		}

		if (bFoundCycle)
		{
			if (OutFailureReason)
			{
				*OutFailureReason = FString(TEXT("Cycles are not allowed."));
			}
			return false;
		}
	}
	else
	{
		// if one of the blocks is not part of the current 
		// execution - that's fine.
		if (SourceBlock->GetRootBlock()->ContainsEntry() != 
			TargetBlock->GetRootBlock()->ContainsEntry())
		{
			return true;
		}

		if (OutFailureReason)
		{
			*OutFailureReason = FString::Printf(TEXT("You cannot combine nodes from \"%s\" and \"%s\"."), *SourceBlock->GetName().ToString(), *TargetBlock->GetName().ToString());
		}
		return false;
	}

	return true;
}

FString FRigVMParserAST::DumpText() const
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	FString Result;
	for (FRigVMExprAST* RootExpr : RootExpressions)
	{
		if(RootExpr == GetObsoleteBlock(false /* create */))
		{
			continue;
		}
		Result += TEXT("\n") + RootExpr->DumpText();
	}
	return Result;
}

FString FRigVMParserAST::DumpDot() const
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	TArray<bool> OutExpressionDefined;
	OutExpressionDefined.AddZeroed(Expressions.Num());

	FVisualGraph VisualGraph(TEXT("AST"));

	VisualGraph.AddSubGraph(TEXT("AST"), FName(TEXT("AST")));
	VisualGraph.AddSubGraph(TEXT("unused"), FName(TEXT("Unused")));

	struct Local
	{
		static TArray<int32> VisitChildren(const FRigVMExprAST* InExpr, int32 InSubGraphIndex, FVisualGraph& OutGraph)
		{
			TArray<int32> ChildNodeIndices;
			for (FRigVMExprAST* Child : InExpr->Children)
			{
				ChildNodeIndices.Add(VisitExpr(Child, InSubGraphIndex, OutGraph));
			}
			return ChildNodeIndices;
		}
		
		static int32 VisitExpr(const FRigVMExprAST* InExpr, int32 InSubGraphIndex, FVisualGraph& OutGraph)
		{
			const FName NodeName = *FString::Printf(TEXT("node_%d"), InExpr->GetIndex());

			int32 NodeIndex = OutGraph.FindNode(NodeName); 
			if(NodeIndex != INDEX_NONE)
			{
				return NodeIndex;
			}

			FString Label = InExpr->GetName().ToString();
			TOptional<EVisualGraphShape> Shape = EVisualGraphShape::Ellipse;
			int32 SubGraphIndex = InSubGraphIndex;
			
			switch (InExpr->GetType())
			{
				case FRigVMExprAST::EType::Literal:
				{
					Label = FString::Printf(TEXT("%s(Literal)"), *InExpr->To<FRigVMLiteralExprAST>()->GetPin()->GetName());
					break;
				}
				case FRigVMExprAST::EType::ExternalVar:
				{
					Label = FString::Printf(TEXT("%s(ExternalVar)"), *InExpr->To<FRigVMExternalVarExprAST>()->GetPin()->GetBoundVariableName());
					break;
				}
				case FRigVMExprAST::EType::Var:
				{
					if (InExpr->To<FRigVMVarExprAST>()->IsGraphVariable())
					{
						URigVMVariableNode* VariableNode = Cast<URigVMVariableNode>(InExpr->To<FRigVMVarExprAST>()->GetPin()->GetNode());
						check(VariableNode);
						Label = FString::Printf(TEXT("Variable %s"), *VariableNode->GetVariableName().ToString());
					}
					else if (InExpr->To<FRigVMVarExprAST>()->IsEnumValue())
					{
						URigVMEnumNode* EnumNode = Cast<URigVMEnumNode>(InExpr->To<FRigVMVarExprAST>()->GetPin()->GetNode());
						check(EnumNode);
						Label = FString::Printf(TEXT("Enum %s"), *EnumNode->GetCPPType());
					}
					else
					{
						Label = InExpr->To<FRigVMVarExprAST>()->GetPin()->GetPinPath(true);
					}
						
					if (InExpr->To<FRigVMVarExprAST>()->IsExecuteContext())
					{
						Shape = EVisualGraphShape::House;
					}
						
					break;
				}
				case FRigVMExprAST::EType::Block:
				{
					if (InExpr->GetParent() == nullptr)
					{
						Label = TEXT("Unused");
						SubGraphIndex = OutGraph.FindSubGraph(TEXT("unused"));;
					}
					else
					{
						Label = TEXT("Block");
					}
					break;
				}
				case FRigVMExprAST::EType::Assign:
				{
					Label = TEXT("=");
					break;
				}
				case FRigVMExprAST::EType::Copy:
				{
					Label = TEXT("Copy");
					break;
				}
				case FRigVMExprAST::EType::CachedValue:
				{
					Label = TEXT("Cache");
					break;
				}
				case FRigVMExprAST::EType::CallExtern:
				{
					if (URigVMUnitNode* Node = Cast<URigVMUnitNode>(InExpr->To<FRigVMCallExternExprAST>()->GetNode()))
					{
						Label = Node->GetScriptStruct()->GetName();
					}
					break;
				}
				case FRigVMExprAST::EType::NoOp:
				{
					Label = TEXT("NoOp");
					break;
				}
				case FRigVMExprAST::EType::Array:
				{
					ERigVMOpCode OpCode = CastChecked<URigVMArrayNode>(InExpr->To<FRigVMArrayExprAST>()->GetNode())->GetOpCode();
					Label = StaticEnum<ERigVMOpCode>()->GetDisplayNameTextByValue((int64)OpCode).ToString();
					break;
				}
				case FRigVMExprAST::EType::Exit:
				{
					Label = TEXT("Exit");
					break;
				}
				case FRigVMExprAST::EType::Entry:
				{
					SubGraphIndex = OutGraph.FindSubGraph(InExpr->GetName());
					if(SubGraphIndex == INDEX_NONE)
					{
						const int32 ASTGraphIndex = OutGraph.FindSubGraph(TEXT("AST"));
						SubGraphIndex = OutGraph.AddSubGraph(InExpr->GetName(), InExpr->GetName(), ASTGraphIndex);
					}
					break;
				}
				default:
				{
					break;
				}
			}

			switch (InExpr->GetType())
			{
				case FRigVMExprAST::EType::Entry:
				case FRigVMExprAST::EType::Exit:
				case FRigVMExprAST::EType::Branch:
				case FRigVMExprAST::EType::Block:
				{
					Shape = EVisualGraphShape::Diamond;
					break;
				}
				case FRigVMExprAST::EType::Assign:
				case FRigVMExprAST::EType::Copy:
				case FRigVMExprAST::EType::CallExtern:
				case FRigVMExprAST::EType::If:
				case FRigVMExprAST::EType::Select:
				case FRigVMExprAST::EType::NoOp:
				{
					Shape = EVisualGraphShape::Box;
					break;
				}
				default:
				{
					break;
				}
			}

			if (!Label.IsEmpty())
			{
				const TOptional<FName> DisplayName = FName(*Label);
				NodeIndex = OutGraph.AddNode(NodeName, DisplayName, TOptional<FLinearColor>(), Shape);
				OutGraph.AddNodeToSubGraph(NodeIndex, SubGraphIndex);
			}

			TArray<int32> ChildNodeIndices = VisitChildren(InExpr, SubGraphIndex, OutGraph);

			if(NodeIndex != INDEX_NONE)
			{
				for(const int32 ChildNodeIndex : ChildNodeIndices)
				{
					if(ChildNodeIndex != INDEX_NONE)
					{
						OutGraph.AddEdge(ChildNodeIndex, NodeIndex, EVisualGraphEdgeDirection::SourceToTarget);
					}
				}
			}

			return NodeIndex;
		}
	};

	for (FRigVMExprAST* Expr : RootExpressions)
	{
		if (Expr == GetObsoleteBlock(false))
		{
			continue;
		}

		Local::VisitExpr(Expr, INDEX_NONE, VisualGraph);
	}

	return VisualGraph.DumpDot();
}

FRigVMBlockExprAST* FRigVMParserAST::GetObsoleteBlock(bool bCreateIfMissing)
{
	if (ObsoleteBlock == nullptr && bCreateIfMissing)
	{
		ObsoleteBlock = MakeExpr<FRigVMBlockExprAST>(FRigVMExprAST::EType::Block, FRigVMASTProxy());
		ObsoleteBlock->bIsObsolete = true;
		RootExpressions.Add(ObsoleteBlock);
	}
	return ObsoleteBlock;
}

const FRigVMBlockExprAST* FRigVMParserAST::GetObsoleteBlock(bool bCreateIfMissing) const
{
	if (ObsoleteBlock == nullptr && bCreateIfMissing)
	{
		FRigVMParserAST* MutableThis = (FRigVMParserAST*)this;
		MutableThis->ObsoleteBlock = MutableThis->MakeExpr<FRigVMBlockExprAST>(FRigVMExprAST::EType::Block, FRigVMASTProxy());
		MutableThis->ObsoleteBlock->bIsObsolete = true;
		MutableThis->RootExpressions.Add(MutableThis->ObsoleteBlock);
	}
	return ObsoleteBlock;
}

void FRigVMParserAST::RemoveExpressions(TArray<FRigVMExprAST*> InExprs)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	if(InExprs.IsEmpty())
	{
		return;
	}
	
	RefreshExprIndices();

	TArray<FRigVMExprAST*> ExpressionsToRemove;
	ExpressionsToRemove.Append(InExprs);

	TArray<int32> NumRemainingParents;
	NumRemainingParents.AddUninitialized(Expressions.Num());
	for(int32 ExpressionIndex = 0; ExpressionIndex < Expressions.Num(); ExpressionIndex++)
	{
		NumRemainingParents[ExpressionIndex] = Expressions[ExpressionIndex]->Parents.Num();
	}

	TArray<bool> bRemoveExpression;
	bRemoveExpression.AddZeroed(Expressions.Num());
	for(int32 ExpressionIndex = 0; ExpressionIndex < ExpressionsToRemove.Num(); ExpressionIndex++)
	{
		FRigVMExprAST* Expr = ExpressionsToRemove[ExpressionIndex];
		bRemoveExpression[Expr->GetIndex()] = true;

		for (FRigVMExprAST* Child : Expr->Children)
		{
			if(--NumRemainingParents[Child->GetIndex()] == 0)
			{
				ExpressionsToRemove.Add(Child);
			}
		}
	}

	TArray<FRigVMExprAST*> RemainingExpressions;
	RemainingExpressions.Reserve(Expressions.Num() - ExpressionsToRemove.Num());
	
	for(int32 ExpressionIndex = 0; ExpressionIndex < Expressions.Num(); ExpressionIndex++)
	{
		if(!bRemoveExpression[ExpressionIndex])
		{
			FRigVMExprAST* Expr = Expressions[ExpressionIndex];
			RemainingExpressions.Add(Expr);

			for(int32 ChildIndex = Expr->Children.Num() - 1; ChildIndex >= 0; ChildIndex--)
			{
				FRigVMExprAST* ChildExpr = Expr->Children[ChildIndex];
				if(bRemoveExpression[ChildExpr->GetIndex()])
				{
					Expr->Children.RemoveAt(ChildIndex);
				}
			}

			for(int32 ParentIndex = Expr->Parents.Num() - 1; ParentIndex >= 0; ParentIndex--)
			{
				FRigVMExprAST* ParentExpr = Expr->Parents[ParentIndex];
				if(bRemoveExpression[ParentExpr->GetIndex()])
				{
					Expr->Parents.RemoveAt(ParentIndex);
				}
			}
		}
	}

	Expressions = RemainingExpressions;

	TArray<FRigVMASTProxy> KeysToRemove;
	for (TPair<FRigVMASTProxy, FRigVMExprAST*> Pair : SubjectToExpression)
	{
		if (bRemoveExpression[Pair.Value->GetIndex()])
		{
			KeysToRemove.Add(Pair.Key);
		}
	}
	
	for (const FRigVMASTProxy& KeyToRemove : KeysToRemove)
	{
		SubjectToExpression.Remove(KeyToRemove);
	}

	for(int32 ExpressionIndex = ExpressionsToRemove.Num() - 1; ExpressionIndex >= 0; ExpressionIndex--)
	{
		ExpressionsToRemove[ExpressionIndex]->Index = INDEX_NONE;
		DeletedExpressions.Add(ExpressionsToRemove[ExpressionIndex]);
	}

	RefreshExprIndices();
}

void FRigVMParserAST::TraverseParents(const FRigVMExprAST* InExpr, TFunctionRef<bool(const FRigVMExprAST*)> InContinuePredicate)
{
	if (!InContinuePredicate(InExpr))
	{
		return;
	}
	for (const FRigVMExprAST* ParentExpr : InExpr->Parents)
	{
		TraverseParents(ParentExpr, InContinuePredicate);
	}
}

void FRigVMParserAST::TraverseChildren(const FRigVMExprAST* InExpr, TFunctionRef<bool(const FRigVMExprAST*)> InContinuePredicate)
{
	if (!InContinuePredicate(InExpr))
	{
		return;
	}
	for (const FRigVMExprAST* ChildExpr : InExpr->Children)
	{
		TraverseChildren(ChildExpr, InContinuePredicate);
	}
}

TArray<int32> FRigVMParserAST::GetSourceLinkIndices(const FRigVMASTProxy& InPinProxy, bool bRecursive) const
{
	return GetLinkIndices(InPinProxy, true, bRecursive);
}

TArray<int32> FRigVMParserAST::GetTargetLinkIndices(const FRigVMASTProxy& InPinProxy, bool bRecursive) const
{
	return GetLinkIndices(InPinProxy, false, bRecursive);
}

TArray<int32> FRigVMParserAST::GetLinkIndices(const FRigVMASTProxy& InPinProxy, bool bGetSource, bool bRecursive) const
{
	const static TArray<int32> EmptyIntArray;
	TArray<int32> LinkIndices;

	if(const TArray<int32>* LinkIndicesPtr = (bGetSource ? SourceLinkIndices : TargetLinkIndices).Find(InPinProxy))
	{
		LinkIndices = *LinkIndicesPtr;
	}

	if (bRecursive)
	{
		URigVMPin* Pin = InPinProxy.GetSubjectChecked<URigVMPin>();
		for (URigVMPin* SubPin : Pin->GetSubPins())
		{
			FRigVMASTProxy SubPinProxy = InPinProxy.GetSibling(SubPin);
			LinkIndices.Append(GetLinkIndices(SubPinProxy, bGetSource, true));
		}
	}

	return LinkIndices;
}


const FRigVMASTLinkDescription& FRigVMParserAST::GetLink(int32 InLinkIndex) const
{
	return Links[InLinkIndex];
}

void FRigVMParserAST::Inline(TArray<URigVMGraph*> InGraphs)
{
	TArray<FRigVMASTProxy> LocalNodeProxies;
	for(URigVMGraph* Graph : InGraphs)
	{
		for (URigVMNode* LocalNode : Graph->GetNodes())
		{
			LocalNodeProxies.Add(FRigVMASTProxy::MakeFromUObject(LocalNode));
		}
	}
	Inline(InGraphs, LocalNodeProxies);
}

void FRigVMParserAST::Inline(TArray<URigVMGraph*> InGraphs, const TArray<FRigVMASTProxy>& InNodeProxies)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	struct LocalPinTraversalInfo
	{
		URigVMPin::FPinOverrideMap* PinOverrides;
		TMap<FRigVMASTProxy, FRigVMASTProxy>* SourcePins;
		TMap<FRigVMASTProxy, TArray<int32>>* TargetLinkIndices;
		TMap<FRigVMASTProxy, TArray<int32>>* SourceLinkIndices;
		TArray<FRigVMASTLinkDescription>* Links;
		TArray<FRigVMASTProxy> LibraryNodeCallstack;
		FRigVMParserASTSettings* Settings;

		static bool ShouldRecursePin(const FRigVMASTProxy& InPinProxy)
		{
			URigVMPin* Pin = InPinProxy.GetSubjectChecked<URigVMPin>();
			URigVMNode* Node = Pin->GetNode();
			if (URigVMVariableNode* VarNode = Cast<URigVMVariableNode>(Node))
			{
				return VarNode->IsInputArgument();
			}
			
			return Node->IsA<URigVMRerouteNode>() ||
				Node->IsA<URigVMLibraryNode>() ||
				Node->IsA<URigVMFunctionEntryNode>() ||
				Node->IsA<URigVMFunctionReturnNode>();
		}

		static bool IsValidPinForAST(const FRigVMASTProxy& InPinProxy)
		{
			return !ShouldRecursePin(InPinProxy);
		}

		static bool IsValidLinkForAST(const FRigVMASTProxy& InSourcePinProxy, const FRigVMASTProxy& InTargetPinProxy)
		{
			return IsValidPinForAST(InSourcePinProxy) && IsValidPinForAST(InTargetPinProxy);
		}

		static FRigVMASTProxy FindSourcePin(const FRigVMASTProxy& InPinProxy, LocalPinTraversalInfo& OutTraversalInfo)
		{
			return FindSourcePin(InPinProxy, InPinProxy, OutTraversalInfo);
		}

		static FRigVMASTProxy FindSourcePin(const FRigVMASTProxy& InPinProxy, const FRigVMASTProxy& InPinProxyForMap, LocalPinTraversalInfo& OutTraversalInfo)
		{
			URigVMPin* Pin = InPinProxy.GetSubjectChecked<URigVMPin>();
			const bool bStoreSourcePinOnMap = InPinProxy == InPinProxyForMap;

			// if this pin is a root on a library
			if (Pin->GetParentPin() == nullptr)
			{
				if (Pin->GetDirection() == ERigVMPinDirection::Output ||
					Pin->GetDirection() == ERigVMPinDirection::IO)
				{
					URigVMNode* Node = Pin->GetNode();
					if (URigVMLibraryNode* LibraryNode = Cast<URigVMLibraryNode>(Node))
					{
						FRigVMASTProxy LibraryNodeProxy = InPinProxy.GetSibling(LibraryNode);
						if (!OutTraversalInfo.LibraryNodeCallstack.Contains(LibraryNodeProxy))
						{
							if (URigVMFunctionReturnNode* ReturnNode = LibraryNode->GetReturnNode())
							{
								if (URigVMPin* ReturnPin = ReturnNode->FindPin(Pin->GetName()))
								{
									OutTraversalInfo.LibraryNodeCallstack.Push(LibraryNodeProxy);
									FRigVMASTProxy ReturnPinProxy = LibraryNodeProxy.GetChild(ReturnPin);
									FRigVMASTProxy SourcePinProxy = FindSourcePin(ReturnPinProxy, OutTraversalInfo);
									SourcePinProxy = SourcePinProxy.IsValid() ? SourcePinProxy : ReturnPinProxy;
									if(bStoreSourcePinOnMap)
									{
										OutTraversalInfo.SourcePins->FindOrAdd(InPinProxyForMap) = SourcePinProxy;
									}
									OutTraversalInfo.LibraryNodeCallstack.Pop();
									return SourcePinProxy;

								}
							}
						}
					}
					else if(URigVMVariableNode* VariableNode = Cast<URigVMVariableNode>(Node))
					{
						if (VariableNode->IsInputArgument())
						{
							if (URigVMFunctionEntryNode* EntryNode = VariableNode->GetGraph()->GetEntryNode())
							{
								if (URigVMPin* EntryPin = EntryNode->FindPin(VariableNode->GetVariableName().ToString()))
								{
									FRigVMASTProxy EntryPinProxy = InPinProxy.GetSibling(EntryPin);
									FRigVMASTProxy SourcePinProxy = FindSourcePin(EntryPinProxy, OutTraversalInfo);
									SourcePinProxy = SourcePinProxy.IsValid() ? SourcePinProxy : EntryPinProxy;
									if(bStoreSourcePinOnMap)
									{
										OutTraversalInfo.SourcePins->FindOrAdd(InPinProxyForMap) = SourcePinProxy;
									}
									return SourcePinProxy;
								} 
							}
						}
					}
					else if (URigVMFunctionEntryNode* EntryNode = Cast<URigVMFunctionEntryNode>(Node))
					{
						for (int32 LibraryNodeIndex = OutTraversalInfo.LibraryNodeCallstack.Num() - 1; LibraryNodeIndex >= 0; LibraryNodeIndex--)
						{
							FRigVMASTProxy LibraryNodeProxy = OutTraversalInfo.LibraryNodeCallstack[LibraryNodeIndex];
							URigVMLibraryNode* LastLibraryNode = LibraryNodeProxy.GetSubject<URigVMLibraryNode>();
							if (LastLibraryNode == nullptr)
							{
								continue;
							}

							if(LastLibraryNode->GetEntryNode() == EntryNode)
							{
								if (URigVMPin* LibraryPin = LastLibraryNode->FindPin(Pin->GetName()))
								{
									FRigVMASTProxy LibraryPinProxy = LibraryNodeProxy.GetSibling(LibraryPin);
									FRigVMASTProxy SourcePinProxy = FindSourcePin(LibraryPinProxy, OutTraversalInfo);
									SourcePinProxy = SourcePinProxy.IsValid() ? SourcePinProxy : LibraryPinProxy;
									if(bStoreSourcePinOnMap)
									{
										OutTraversalInfo.SourcePins->FindOrAdd(InPinProxyForMap) = SourcePinProxy;
									}
									return SourcePinProxy;
								}
							}
						}
					}
				}
			}

			if (Pin->GetDirection() != ERigVMPinDirection::Input &&
				Pin->GetDirection() != ERigVMPinDirection::IO &&
				Pin->GetDirection() != ERigVMPinDirection::Output)
			{
				return FRigVMASTProxy();
			}

			bool bIOPinOnLeftOfLibraryNode = false;
			if (Pin->GetDirection() == ERigVMPinDirection::IO)
			{
				if (URigVMLibraryNode* LibraryNode = Cast<URigVMLibraryNode>(Pin->GetNode()))
				{
					bIOPinOnLeftOfLibraryNode = OutTraversalInfo.LibraryNodeCallstack.Contains(InPinProxy.GetSibling(LibraryNode));
				}
			}

			if (!bIOPinOnLeftOfLibraryNode && bStoreSourcePinOnMap)
			{
				// note: this map isn't going to work for functions which are referenced.
				// (since the pin objects are shared between multiple invocation nodes)
				if (const FRigVMASTProxy* SourcePinProxy = OutTraversalInfo.SourcePins->Find(InPinProxyForMap))
				{
					return *SourcePinProxy;
				}
			}

			TArray<FString> SegmentPath;
			FRigVMASTProxy SourcePinProxy;

			URigVMPin* ChildPin = Pin;
			while (ChildPin != nullptr)
			{
				if (ChildPin->GetDirection() == ERigVMPinDirection::Output && ChildPin->GetParentPin() == nullptr)
				{
					if (URigVMFunctionEntryNode* EntryNode = Cast<URigVMFunctionEntryNode>(ChildPin->GetNode()))
					{
						// rather than relying on the other we are going to query what's in the call stack.
						// for collapse nodes that's not a different, but for function ref nodes the outer
						// node is the definition and not the reference node - which sits in the callstack.
						if (URigVMLibraryNode* OuterNode = (InPinProxy.GetParent().GetSubject<URigVMLibraryNode>()))
						{
							if (URigVMPin* OuterPin = OuterNode->FindPin(ChildPin->GetName()))
							{
								FRigVMASTProxy OuterPinProxy = InPinProxy.GetParent().GetSibling(OuterPin);
								SourcePinProxy = FindSourcePin(OuterPinProxy, OutTraversalInfo);
								SourcePinProxy = SourcePinProxy.IsValid() ? SourcePinProxy : OuterPinProxy;
								break;
							}
						}
					}
					else if(URigVMLibraryNode* LibraryNode = Cast<URigVMLibraryNode>(ChildPin->GetNode()))
					{
						if(ChildPin != InPinProxy.GetSubject<URigVMPin>())
						{
							const FRigVMASTProxy ChildPinProxy = InPinProxy.GetSibling(ChildPin); 
							if (ShouldRecursePin(ChildPinProxy))
							{
								FRigVMASTProxy SourceSourcePinProxy = FindSourcePin(ChildPinProxy, OutTraversalInfo);
								if(SourceSourcePinProxy.IsValid())
								{
									SourcePinProxy = SourceSourcePinProxy;
								}
							}
						}
					}
				}

				TArray<URigVMLink*> SourceLinks = ChildPin->GetSourceLinks(false /* recursive */);
				
				URigVMPin* SourcePin = nullptr;
				if (SourceLinks.Num() > 0)
				{
					SourcePin = SourceLinks[0]->GetSourcePin();
				}
				else if(ChildPin->IsBoundToInputArgument())
				{
					if (URigVMGraph* Graph = ChildPin->GetGraph())
					{
						if (URigVMFunctionEntryNode* EntryNode = Graph->GetEntryNode())
						{
							SourcePin = EntryNode->FindPin(ChildPin->GetBoundVariableName());
						}
					}
				}
				else if(URigVMVariableNode* VariableNode = Cast<URigVMVariableNode>(ChildPin->GetNode()))
				{
					if (VariableNode->IsInputArgument())
					{
						if (URigVMGraph* Graph = ChildPin->GetGraph())
						{
							if (URigVMFunctionEntryNode* EntryNode = Graph->GetEntryNode())
							{
								SourcePin = EntryNode->FindPin(VariableNode->GetVariableName().ToString());
								if (ChildPin->GetParentPin())
								{									
									SourcePin = SourcePin->FindSubPin(ChildPin->GetSegmentPath());
								}
							}
						}
					}
				}
				
				if(SourcePin)
				{
					SourcePinProxy = InPinProxy.GetSibling(SourcePin);

					if (ShouldRecursePin(SourcePinProxy))
					{
						FRigVMASTProxy SourceSourcePinProxy = FindSourcePin(SourcePinProxy, OutTraversalInfo);
						if(SourceSourcePinProxy.IsValid())
						{
							SourcePinProxy = SourceSourcePinProxy;
						}
					}

					break;
				}

				URigVMPin* ParentPin = ChildPin->GetParentPin();
				if (ParentPin)
				{
					FRigVMASTProxy ParentPinProxy = InPinProxy.GetSibling(ParentPin);

					// if we found a parent pin which has a source that is not a reroute
					if (FRigVMASTProxy* ParentSourcePinProxyPtr = OutTraversalInfo.SourcePins->Find(ParentPinProxy))
					{
						FRigVMASTProxy& ParentSourcePinProxy = *ParentSourcePinProxyPtr;
						if (ParentSourcePinProxy.IsValid())
						{
							if (!ShouldRecursePin(ParentSourcePinProxy))
							{
								// only discard results here if we haven't crossed a collapse node boundary
								if(ParentSourcePinProxy.GetSubjectChecked<URigVMPin>()->GetGraph() == ChildPin->GetGraph())
								{
									SourcePinProxy = FRigVMASTProxy();
									break;
								}
							}
						}
					}

					SegmentPath.Push(ChildPin->GetName());
				}
				ChildPin = ParentPin;
			}
			
			if (SourcePinProxy.IsValid())
			{
				while (!SegmentPath.IsEmpty())
				{
					FString Segment = SegmentPath.Pop();

					URigVMPin* SourcePin = SourcePinProxy.GetSubjectChecked<URigVMPin>();
					if (URigVMPin* SourceSubPin = SourcePin->FindSubPin(Segment))
					{
						SourcePinProxy = SourcePinProxy.GetSibling(SourceSubPin);

						if (ShouldRecursePin(SourcePinProxy))
						{
							FRigVMASTProxy SourceSourceSubPinProxy = FindSourcePin(SourcePinProxy, OutTraversalInfo);
							if (SourceSourceSubPinProxy.IsValid())
							{
								SourcePinProxy = SourceSourceSubPinProxy;
							}
						}
					}
					else
					{
						SourcePinProxy = FRigVMASTProxy();
						break;
					}
				}
			}

			if (!bIOPinOnLeftOfLibraryNode && bStoreSourcePinOnMap)
			{
				OutTraversalInfo.SourcePins->FindOrAdd(InPinProxyForMap) = SourcePinProxy;
			}
			return SourcePinProxy;
		}

		static void VisitPin(const FRigVMASTProxy& InPinProxy, LocalPinTraversalInfo& OutTraversalInfo)
		{
			return VisitPin(InPinProxy, InPinProxy, OutTraversalInfo, FString());
		}

		static void VisitPin(const FRigVMASTProxy& InPinProxy, const FRigVMASTProxy& InPinProxyForMap,
			LocalPinTraversalInfo& OutTraversalInfo, const FString& InSegmentPath)
		{
			const FRigVMASTProxy SourcePinProxy = FindSourcePin(InPinProxy, InPinProxyForMap, OutTraversalInfo);
			if (SourcePinProxy.IsValid())
			{
				// The source pin is the final determined source pin, since
				// FindSourcePin is recursive.
				// If the source pin is on a reroute node, this means that
				// we only care about the default value - since it is a 
				// "hanging" reroute without any live input.
				// same goes for library nodes or return nodes - we'll 
				// just use the default pin value in that case.

				URigVMPin* SourcePin = SourcePinProxy.GetSubjectChecked<URigVMPin>();
				URigVMNode* SourceNode = SourcePin->GetNode();
				if (SourceNode->IsA<URigVMRerouteNode>() ||
					SourceNode->IsA<URigVMLibraryNode>() ||
					SourceNode->IsA<URigVMFunctionReturnNode>())
				{
					// for arrays - if there are sub-pins on the determined source, we need to walk those as well
					if(SourcePin->IsArray())
					{
						TArray<URigVMPin*> SourceSubPins = SourcePin->GetSubPins();
						for (int32 SourceSubPinIndex = 0; SourceSubPinIndex < SourceSubPins.Num(); SourceSubPinIndex++)
						{
							URigVMPin* SourceSubPin = SourceSubPins[SourceSubPinIndex];
							const FRigVMASTProxy SubPinProxy = SourcePinProxy.GetSibling(SourceSubPin);
							const FString SegmentPath = SourceSubPin->GetSubPinPath(SourcePin, false);
							VisitPin(SubPinProxy, InPinProxy, OutTraversalInfo, SegmentPath);
							SourceSubPins.Append(SourceSubPin->GetSubPins());
						}
					}

					// when asking for the default value of the array - we may need to get the previously stored override
					const URigVMPin::FPinOverride SourceOverride(SourcePinProxy, *OutTraversalInfo.PinOverrides);
						
					// we query the default value now since the potential array elements have been visited and their
					// potential default value override has been stored to the map.
					OutTraversalInfo.PinOverrides->FindOrAdd(InPinProxy) = URigVMPin::FPinOverrideValue(SourcePin, SourceOverride);
				}
				else
				{
					if (IsValidLinkForAST(SourcePinProxy, InPinProxyForMap))
					{
						FRigVMASTLinkDescription Link(SourcePinProxy, InPinProxyForMap, InSegmentPath);
						Link.LinkIndex = OutTraversalInfo.Links->Num();
						OutTraversalInfo.Links->Add(Link);
						OutTraversalInfo.SourceLinkIndices->FindOrAdd(InPinProxyForMap).Add(Link.LinkIndex);
						OutTraversalInfo.TargetLinkIndices->FindOrAdd(SourcePinProxy).Add(Link.LinkIndex);
					}
				}
			}

			// If the pin is an array, and it has a source pin, the subpins can be safely ignored
			URigVMPin* Pin = InPinProxy.GetSubjectChecked<URigVMPin>();
			if (!Pin->IsArray() || !SourcePinProxy.IsValid())
			{
				for (URigVMPin* SubPin : Pin->GetSubPins())
				{
					FRigVMASTProxy SubPinProxy = InPinProxy.GetSibling(SubPin);
					VisitPin(SubPinProxy, OutTraversalInfo);
				}
			}
		}

		static void VisitNode(const FRigVMASTProxy& InNodeProxy, LocalPinTraversalInfo& OutTraversalInfo)
		{
			if (InNodeProxy.IsA<URigVMRerouteNode>() ||
				InNodeProxy.IsA<URigVMFunctionEntryNode>() ||
				InNodeProxy.IsA<URigVMFunctionReturnNode>())
			{
				return;
			}

			if (URigVMLibraryNode* LibraryNode = InNodeProxy.GetSubject<URigVMLibraryNode>())
			{
				if (LibraryNode->GetContainedGraph() == nullptr)
				{
					if (URigVMFunctionReferenceNode* FunctionRef = Cast<URigVMFunctionReferenceNode>(LibraryNode))
					{
						FString FunctionPath = FunctionRef->ReferencedNodePtr.ToString();

						OutTraversalInfo.Settings->Reportf(
							EMessageSeverity::Error, 
							LibraryNode, 
							TEXT("Function Reference '%s' references a missing function (%s)."), 
							*LibraryNode->GetName(),
							*FunctionPath);
					}
					else
					{
						OutTraversalInfo.Settings->Reportf(
							EMessageSeverity::Error,
							LibraryNode,
							TEXT("Library Node '%s' doesn't contain a subgraph."),
							*LibraryNode->GetName());
					}
					return;
				}

				OutTraversalInfo.LibraryNodeCallstack.Push(InNodeProxy);
				TArray<URigVMNode*> ContainedNodes = LibraryNode->GetContainedNodes();
				for (URigVMNode* ContainedNode : ContainedNodes)
				{
					// create a proxy which uses the previous node as a callstack
					FRigVMASTProxy ContainedNodeProxy = InNodeProxy.GetChild(ContainedNode);
					VisitNode(ContainedNodeProxy, OutTraversalInfo);
				}
				OutTraversalInfo.LibraryNodeCallstack.Pop();
			}
			else
			{
				URigVMNode* Node = InNodeProxy.GetSubjectChecked<URigVMNode>();
				for (URigVMPin* Pin : Node->GetPins())
				{
					FRigVMASTProxy PinProxy = InNodeProxy.GetSibling(Pin);
					LocalPinTraversalInfo::VisitPin(PinProxy, OutTraversalInfo);
				}
			}
		}
	};

	NodeProxies.Reset();
	SourceLinkIndices.Reset();
	TargetLinkIndices.Reset();

	// a) find all of the relevant nodes,
	//    inline and traverse into library nodes
	NodeProxies = InNodeProxies;

	// c) flatten links from an entry node / to a return node
	//    also traverse links along reroutes and flatten them
	LocalPinTraversalInfo TraversalInfo;
	TraversalInfo.PinOverrides = &PinOverrides;
	TraversalInfo.SourcePins = &SharedOperandPins;
	TraversalInfo.TargetLinkIndices = &TargetLinkIndices;
	TraversalInfo.SourceLinkIndices = &SourceLinkIndices;
	TraversalInfo.Links = &Links;
	TraversalInfo.Settings = &Settings;

	for (const FRigVMASTProxy& NodeProxy : NodeProxies)
	{
		LocalPinTraversalInfo::VisitNode(NodeProxy, TraversalInfo);
	}

	// once we are done with the inlining we may need to clean up pin value overrides for pins
	// that also have overrides on sub pins
	TArray<FRigVMASTProxy> PinOverridesToRemove;
	for(const TPair<FRigVMASTProxy, URigVMPin::FPinOverrideValue>& Override : PinOverrides)
	{
		if(URigVMPin* Pin = Override.Key.GetSubject<URigVMPin>())
		{
			for(URigVMPin* SubPin : Pin->GetSubPins())
			{
				const FRigVMASTProxy SubPinProxy = Override.Key.GetSibling(SubPin);
				if(PinOverrides.Contains(SubPinProxy))
				{
					PinOverridesToRemove.Add(Override.Key);
				}
			}
		}
	}
	for(const FRigVMASTProxy& ProxyToRemove : PinOverridesToRemove)
	{
		PinOverrides.Remove(ProxyToRemove);
	}
}

bool FRigVMParserAST::ShouldLinkBeSkipped(const FRigVMASTLinkDescription& InLink) const
{
	const URigVMPin* SourcePin = InLink.SourceProxy.GetSubjectChecked<URigVMPin>();
	const URigVMPin* TargetPin = InLink.TargetProxy.GetSubjectChecked<URigVMPin>();

	for (URigVMLink* LinkToSkip : LinksToSkip)
	{
		if (LinkToSkip->GetSourcePin() == SourcePin &&
			LinkToSkip->GetTargetPin() == TargetPin)
		{
			return true;
		}
	}
	return false;
}

FString FRigVMParserAST::GetLinkAsString(const FRigVMASTLinkDescription& InLink)
{
	const URigVMPin* SourcePin = InLink.SourceProxy.GetSubjectChecked<URigVMPin>();
	const URigVMPin* TargetPin = InLink.TargetProxy.GetSubjectChecked<URigVMPin>();
	static const FString EmptyString;
	static const FString PeriodString = TEXT(".");

	return FString::Printf(TEXT("%s -> %s%s%s"), *SourcePin->GetPinPath(), *TargetPin->GetPinPath(),
		*(InLink.SegmentPath.IsEmpty() ? EmptyString : PeriodString), *InLink.SegmentPath);
}
