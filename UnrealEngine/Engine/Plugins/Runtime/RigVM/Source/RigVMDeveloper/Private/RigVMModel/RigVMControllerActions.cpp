// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMModel/RigVMControllerActions.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Algo/Transform.h"
#include "UObject/Package.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigVMControllerActions)

#if WITH_EDITOR
#include "Misc/TransactionObjectEvent.h"
#endif

UScriptStruct* FRigVMActionKey::GetScriptStruct() const
{
	return FindObjectChecked<UScriptStruct>(nullptr, *ScriptStructPath);		
}

TSharedPtr<FStructOnScope> FRigVMActionKey::GetAction() const
{
	UScriptStruct* ScriptStruct = GetScriptStruct();
	TSharedPtr<FStructOnScope> StructOnScope = MakeShareable(new FStructOnScope(ScriptStruct));
	ScriptStruct->ImportText(*ExportedText, StructOnScope->GetStructMemory(), nullptr, PPF_None, nullptr, ScriptStruct->GetName());
	return StructOnScope;
}

FRigVMActionWrapper::FRigVMActionWrapper(const FRigVMActionKey& Key)
{
	StructOnScope = Key.GetAction();
}
FRigVMActionWrapper::~FRigVMActionWrapper()
{
}

const UScriptStruct* FRigVMActionWrapper::GetScriptStruct() const
{
	return CastChecked<UScriptStruct>(StructOnScope->GetStruct());
}

FRigVMBaseAction* FRigVMActionWrapper::GetAction(URigVMController* InLastController) const
{
	FRigVMBaseAction* Action = (FRigVMBaseAction*)StructOnScope->GetStructMemory();
	Action->EnsureControllerValidity();

	// reuse the last controller so that we don't have to rely on resolving it
	// from the soft object path each time
	if(Action->WeakController.IsValid() && IsValid(InLastController) && InLastController->IsValidGraph())
	{
		if(Action->ControllerPath.ToString() == InLastController->GetPathName())
		{
			Action->WeakController = InLastController;
		}
	}
	return Action;
}

FString FRigVMActionWrapper::ExportText() const
{
	FString ExportedText;
	if (StructOnScope.IsValid() && StructOnScope->IsValid())
	{
		const UScriptStruct* ScriptStruct = GetScriptStruct();
		FStructOnScope DefaultScope(ScriptStruct);
		ScriptStruct->ExportText(ExportedText, GetAction(), DefaultScope.GetStructMemory(), nullptr, PPF_None, nullptr);
	}
	return ExportedText;
}

bool URigVMActionStack::OpenUndoBracket(URigVMController* InController, const FString& InTitle)
{
	FRigVMBaseAction* Action = new FRigVMBaseAction(InController);
	Action->Title = InTitle;
	BracketActions.Add(Action);
	BeginAction(*Action);
	return true;
}

bool URigVMActionStack::CloseUndoBracket(URigVMController* InController)
{
	ensure(BracketActions.Num() > 0);
	if(BracketActions.Last()->IsEmpty())
	{
		return CancelUndoBracket(InController);
	}
	FRigVMBaseAction* Action = BracketActions.Pop();
	EndAction(*Action);
	delete(Action);
	return true;
}

bool URigVMActionStack::CancelUndoBracket(URigVMController* InController)
{
	ensure(BracketActions.Num() > 0);
	FRigVMBaseAction* Action = BracketActions.Pop();
	CancelAction(*Action);
	delete(Action);
	return true;
}

bool URigVMActionStack::Undo(URigVMController* InController)
{
	if (UndoActions.Num() == 0 && InController)
	{
		InController->ReportWarning(TEXT("Nothing to undo."));
		return false;
	}

	FRigVMActionKey KeyToUndo = UndoActions.Pop();
	ActionIndex = UndoActions.Num();
	
	FRigVMActionWrapper Wrapper(KeyToUndo);

#if RIGVM_ACTIONSTACK_VERBOSE_LOG		
	TGuardValue<int32> TabDepthGuard(LogActionDepth, 0);
	LogAction(Wrapper.GetScriptStruct(), *Wrapper.GetAction(), FRigVMBaseAction::UndoPrefix);
#endif
	
	if (Wrapper.GetAction(InController)->Undo())
	{
		RedoActions.Add(KeyToUndo);
		return true;
	}

	if(InController)
	{
		InController->ReportAndNotifyErrorf(TEXT("Error while undoing action %s."), *Wrapper.GetAction()->Title);
	}
	return false;
}

bool URigVMActionStack::Redo(URigVMController* InController)
{
	if (RedoActions.Num() == 0 && InController)
	{
		InController->ReportWarning(TEXT("Nothing to redo."));
		return false;
	}

	FRigVMActionKey KeyToRedo = RedoActions.Pop();

	FRigVMActionWrapper Wrapper(KeyToRedo);

#if RIGVM_ACTIONSTACK_VERBOSE_LOG		
	TGuardValue<int32> TabDepthGuard(LogActionDepth, 0);
	LogAction(Wrapper.GetScriptStruct(), *Wrapper.GetAction(), FRigVMBaseAction::RedoPrefix);
#endif
	
	if (Wrapper.GetAction(InController)->Redo())
	{
		UndoActions.Add(KeyToRedo);
		ActionIndex = UndoActions.Num();
		return true;
	}

	if(InController)
	{
		InController->ReportAndNotifyErrorf(TEXT("Error while undoing action %s."), *Wrapper.GetAction()->Title);
	}
	return false;
}

#if WITH_EDITOR

void URigVMActionStack::PostTransacted(const FTransactionObjectEvent& TransactionEvent)
{
	if (TransactionEvent.GetEventType() == ETransactionObjectEventType::UndoRedo)
	{
		int32 DesiredActionIndex = ActionIndex;
		ActionIndex = UndoActions.Num();

		if (DesiredActionIndex == ActionIndex)
		{
			return;
		}

		ModifiedEvent.Broadcast(ERigVMGraphNotifType::InteractionBracketOpened, nullptr, nullptr);

		while (DesiredActionIndex < ActionIndex)
		{
			if (UndoActions.Num() == 0)
			{
				break;
			}
			if (!Undo(nullptr))
			{
				ModifiedEvent.Broadcast(ERigVMGraphNotifType::InteractionBracketCanceled, nullptr, nullptr);
				return;
			}
		}
		while (DesiredActionIndex > ActionIndex)
		{
			if (RedoActions.Num() == 0)
			{
				break;
			}
			if (!Redo(nullptr))
			{
				ModifiedEvent.Broadcast(ERigVMGraphNotifType::InteractionBracketCanceled, nullptr, nullptr);
				return;
			}
		}

		ModifiedEvent.Broadcast(ERigVMGraphNotifType::InteractionBracketClosed, nullptr, nullptr);
	}
}

#if RIGVM_ACTIONSTACK_VERBOSE_LOG		

void URigVMActionStack::LogAction(const UScriptStruct* InActionStruct, const FRigVMBaseAction& InAction, const FString& InPrefix)
{
	if(bSuspendLogActions)
	{
		return;
	}
	
	if(URigVMController* Controller = InAction.GetController())
	{
		static TArray<FString> TabPrefix = {TEXT(""), TEXT("  ")};

		while(TabPrefix.Num() <= LogActionDepth)
		{
			TabPrefix.Add(TabPrefix.Last() + TabPrefix[1]);
		}
		
		const FString ActionContent = FRigVMStruct::ExportToFullyQualifiedText(InActionStruct, (const uint8*)&InAction);
		Controller->ReportInfof(TEXT("%s%s: %s (%s) '%s"), *TabPrefix[LogActionDepth], *InPrefix, *InAction.GetTitle(), *InActionStruct->GetStructCPPName(), *ActionContent);
	}

	if(InPrefix == FRigVMBaseAction::AddActionPrefix)
	{
		URigVMController* Controller = InAction.GetController();
		TGuardValue<int32> ActionDepthGuard(LogActionDepth, LogActionDepth + 1);
		for(const FRigVMActionKey& SubAction : InAction.SubActions)
		{
			const FRigVMActionWrapper Wrapper(SubAction);
			LogAction(Wrapper.GetScriptStruct(), *Wrapper.GetAction(Controller), InPrefix);
		}
	}
}

#endif

#endif


URigVMController* FRigVMBaseAction::GetController() const
{
	EnsureControllerValidity();
	if(!WeakController.IsValid())
	{
		if(URigVMController* Controller = Cast<URigVMController>(ControllerPath.ResolveObject()))
		{
			WeakController = Controller;
		}
	}
	return WeakController.Get();
}

void FRigVMBaseAction::EnsureControllerValidity() const
{
	if(WeakController.IsValid())
	{
		if(const URigVMController* Controller = WeakController.Get())
		{
			if(!IsValid(Controller) || !Controller->IsValidGraph())
			{
				WeakController.Reset();
			}
		}
	}
}

bool FRigVMBaseAction::Merge(const FRigVMBaseAction* Other)
{
	return SubActions.Num() == 0 && Other->SubActions.Num() == 0;
}

bool FRigVMBaseAction::MakesObsolete(const FRigVMBaseAction* Other) const
{
	return false;
}

bool FRigVMBaseAction::Undo()
{
	URigVMController* Controller = GetController();
	if(Controller == nullptr)
	{
		return false;
	}

#if RIGVM_ACTIONSTACK_VERBOSE_LOG		
	URigVMActionStack* Stack = Controller->GetActionStack();
	check(Stack);
	TGuardValue<int32> TabDepthGuard(Stack->LogActionDepth, Stack->LogActionDepth + 1);
#endif
	TGuardValue<bool> TransactionGuard(Controller->bIsTransacting, true);

	bool Result = true;
	for (int32 KeyIndex = SubActions.Num() - 1; KeyIndex >= 0; KeyIndex--)
	{
		FRigVMActionWrapper Wrapper(SubActions[KeyIndex]);
		FRigVMBaseAction* SubAction = Wrapper.GetAction(Controller);
#if RIGVM_ACTIONSTACK_VERBOSE_LOG		
		SubAction->LogAction(UndoPrefix);
#endif
		if(!SubAction->Undo())
		{
			Controller->ReportAndNotifyErrorf(TEXT("Error while undoing action '%s'."), *SubAction->Title);
			Result = false;
		}
	}
	return Result;
}

bool FRigVMBaseAction::Redo()
{
	URigVMController* Controller = GetController();
	if(Controller == nullptr)
	{
		return false;
	}

#if RIGVM_ACTIONSTACK_VERBOSE_LOG		
	URigVMActionStack* Stack = Controller->GetActionStack();
	check(Stack);
	TGuardValue<int32> TabDepthGuard(Stack->LogActionDepth, Stack->LogActionDepth + 1);
#endif
	TGuardValue<bool> TransactionGuard(Controller->bIsTransacting, true);

	bool Result = true;
	for (int32 KeyIndex = 0; KeyIndex < SubActions.Num(); KeyIndex++)
	{
		FRigVMActionWrapper Wrapper(SubActions[KeyIndex]);
#if RIGVM_ACTIONSTACK_VERBOSE_LOG		
		Wrapper.GetAction()->LogAction(RedoPrefix);
#endif
		if (!Wrapper.GetAction()->Redo())
		{
			Controller->ReportAndNotifyErrorf(TEXT("Error while redoing action '%s'."), *Wrapper.GetAction()->Title);
			Result = false;
		}
	}
	return Result;
}

bool FRigVMBaseAction::StoreNode(const URigVMNode* InNode, bool bIsPriorChange)
{
	URigVMController* Controller = GetController();
	if(Controller == nullptr)
	{
		return false;
	}

	check(InNode);

	const FString& Content = Controller->ExportNodesToText({InNode->GetFName()}, true);
	if(!Content.IsEmpty())
	{
		FRigVMActionNodeContent& StoredContent = ExportedNodes.FindOrAdd(InNode->GetFName());
		if(bIsPriorChange)
		{
			StoredContent.Old = Content;
		}
		else
		{
			StoredContent.New = Content;
		}
	}
	return !Content.IsEmpty();
}

bool FRigVMBaseAction::RestoreNode(const FName& InNodeName, bool bIsUndoing)
{
	URigVMController* Controller = GetController();
	if(Controller == nullptr)
	{
		return false;
	}

	URigVMGraph* Graph = Controller->GetGraph();
	check(Graph);

	FRigVMActionNodeContent& Content = ExportedNodes.FindChecked(InNodeName);

	if(URigVMNode* ExistingNode = Graph->FindNodeByName(InNodeName))
	{
		if(!Controller->RemoveNode(ExistingNode, false, false))
		{
			return false;
		}
	}

	const FString& ContentToImport = bIsUndoing ? Content.Old : Content.New;
	const TArray<FName> ImportedNodeNames = Controller->ImportNodesFromText(ContentToImport, false, false);
	if(ImportedNodeNames.Num() == 1)
	{
		if(!ImportedNodeNames[0].IsEqual(InNodeName, ENameCase::CaseSensitive))
		{
			return false;
		}
	}

	return true;
}

#if RIGVM_ACTIONSTACK_VERBOSE_LOG

void FRigVMBaseAction::LogAction(const FString& InPrefix) const
{
	URigVMController* Controller = GetController();
	if(Controller == nullptr)
	{
		return;
	}
	
	URigVMActionStack* Stack = Controller->GetActionStack();
	check(Stack);
	TGuardValue<int32> TabDepthGuard(Stack->LogActionDepth, Stack->LogActionDepth + 1);

	Stack->LogAction(GetScriptStruct(), *this, InPrefix);
}

#endif

FRigVMInjectNodeIntoPinAction::FRigVMInjectNodeIntoPinAction()
: FRigVMBaseAction(nullptr)
, PinPath()
, bAsInput(false)
, InputPinName(NAME_None)
, OutputPinName(NAME_None)
, NodePath()
{
}

FRigVMInjectNodeIntoPinAction::FRigVMInjectNodeIntoPinAction(URigVMController* InController, URigVMInjectionInfo* InInjectionInfo)
: FRigVMBaseAction(InController)
, PinPath(InInjectionInfo->GetPin()->GetPinPath())
, bAsInput(InInjectionInfo->bInjectedAsInput)
, NodePath(InInjectionInfo->Node->GetName())
{
	if (InInjectionInfo->InputPin)
	{
		InputPinName = InInjectionInfo->InputPin->GetFName();
	}
	if (InInjectionInfo->OutputPin)
	{
		OutputPinName = InInjectionInfo->OutputPin->GetFName();
	}
}

bool FRigVMInjectNodeIntoPinAction::Undo()
{
	if (!FRigVMBaseAction::Undo())
	{
		return false;
	}
	return GetController()->EjectNodeFromPin(*PinPath, false) != nullptr;
}

bool FRigVMInjectNodeIntoPinAction::Redo()
{
	if(!CanUndoRedo())
	{
		return false;
	}
#if WITH_EDITOR
	if (URigVMInjectionInfo* InjectionInfo = GetController()->InjectNodeIntoPin(PinPath, bAsInput, InputPinName, OutputPinName, false))
	{
		return FRigVMBaseAction::Redo();
	}
#endif
	return false;
}

FRigVMEjectNodeFromPinAction::FRigVMEjectNodeFromPinAction()
: FRigVMInjectNodeIntoPinAction()
{
}

FRigVMEjectNodeFromPinAction::FRigVMEjectNodeFromPinAction(URigVMController* InController, URigVMInjectionInfo* InInjectionInfo)
: FRigVMInjectNodeIntoPinAction(InController, InInjectionInfo)
{
}

bool FRigVMEjectNodeFromPinAction::Undo()
{
	if (!FRigVMBaseAction::Undo())
	{
		return false;
	}
#if WITH_EDITOR
	const URigVMInjectionInfo* InjectionInfo = GetController()->InjectNodeIntoPin(PinPath, bAsInput, InputPinName, OutputPinName, false);
	return InjectionInfo != nullptr;
#else
	return false;
#endif
}

bool FRigVMEjectNodeFromPinAction::Redo()
{
	if(!CanUndoRedo())
	{
		return false;
	}
	if (GetController()->EjectNodeFromPin(*PinPath, false) != nullptr)
	{
		return FRigVMBaseAction::Redo();
	}
	return false;
}

FRigVMRemoveNodesAction::FRigVMRemoveNodesAction()
: FRigVMBaseAction(nullptr)
{
}

FRigVMRemoveNodesAction::FRigVMRemoveNodesAction(URigVMController* InController, TArray<URigVMNode*> InNodes)
: FRigVMBaseAction(InController)
{
	Algo::Transform(InNodes, NodeNames, [](const URigVMNode* Node) -> FName
	{
		return Node->GetFName();
	});
	ExportedContent = InController->ExportNodesToText(NodeNames, true);
}

bool FRigVMRemoveNodesAction::Undo()
{
	if (!FRigVMBaseAction::Undo())
	{
		return false;
	}
	const TArray<FName> NewNodeNames = GetController()->ImportNodesFromText(ExportedContent, false, false);
	return NewNodeNames.Num() == NodeNames.Num();
}

bool FRigVMRemoveNodesAction::Redo()
{
	if(!CanUndoRedo())
	{
		return false;
	}
	if(GetController()->RemoveNodesByName(NodeNames, false, false))
	{
		return FRigVMBaseAction::Redo();
	}
	return false;
}

FRigVMSetNodeSelectionAction::FRigVMSetNodeSelectionAction()
: FRigVMBaseAction(nullptr)
{
}

FRigVMSetNodeSelectionAction::FRigVMSetNodeSelectionAction(URigVMController* InController, URigVMGraph* InGraph, TArray<FName> InNewSelection)
: FRigVMBaseAction(InController)
{
	OldSelection = InGraph->GetSelectNodes();
	NewSelection = InNewSelection;
}

bool FRigVMSetNodeSelectionAction::Undo()
{
	if(!FRigVMBaseAction::Undo())
	{
		return false;
	}
	return GetController()->SetNodeSelection(OldSelection, false);
}

bool FRigVMSetNodeSelectionAction::Redo()
{
	if(!CanUndoRedo())
	{
		return false;
	}
	if(!GetController()->SetNodeSelection(NewSelection, false))
	{
		return false;
	}
	return FRigVMBaseAction::Redo();
}

FRigVMSetNodePositionAction::FRigVMSetNodePositionAction()
: FRigVMBaseAction(nullptr)
, OldPosition(FVector2D::ZeroVector)
, NewPosition(FVector2D::ZeroVector)
{
}

FRigVMSetNodePositionAction::FRigVMSetNodePositionAction(URigVMController* InController, URigVMNode* InNode, const FVector2D& InNewPosition)
: FRigVMBaseAction(InController)
, NodePath(InNode->GetNodePath())
, OldPosition(InNode->GetPosition())
, NewPosition(InNewPosition)
{
}

bool FRigVMSetNodePositionAction::Merge(const FRigVMBaseAction* Other)
{
	if (!FRigVMBaseAction::Merge(Other))
	{
		return false;
	}

	const FRigVMSetNodePositionAction* Action = (const FRigVMSetNodePositionAction*)Other;
	if (NodePath != Action->NodePath)
	{
		return false;
	}

	NewPosition = Action->NewPosition;
	return true;
}

bool FRigVMSetNodePositionAction::Undo()
{
	if(!FRigVMBaseAction::Undo())
	{
		return false;
	}
	return GetController()->SetNodePositionByName(*NodePath, OldPosition, false);
}

bool FRigVMSetNodePositionAction::Redo()
{
	if(!CanUndoRedo())
	{
		return false;
	}
	if(!GetController()->SetNodePositionByName(*NodePath, NewPosition, false))
	{
		return false;
	}
	return FRigVMBaseAction::Redo();
}

FRigVMSetNodeSizeAction::FRigVMSetNodeSizeAction()
: FRigVMBaseAction(nullptr)
, OldSize(FVector2D::ZeroVector)
, NewSize(FVector2D::ZeroVector)
{
}

FRigVMSetNodeSizeAction::FRigVMSetNodeSizeAction(URigVMController* InController, URigVMNode* InNode, const FVector2D& InNewSize)
: FRigVMBaseAction(InController)
, NodePath(InNode->GetNodePath())
, OldSize(InNode->GetSize())
, NewSize(InNewSize)
{
}

bool FRigVMSetNodeSizeAction::Merge(const FRigVMBaseAction* Other)
{
	if (!FRigVMBaseAction::Merge(Other))
	{
		return false;
	}

	const FRigVMSetNodeSizeAction* Action = (const FRigVMSetNodeSizeAction*)Other;
	if (NodePath != Action->NodePath)
	{
		return false;
	}

	NewSize = Action->NewSize;
	return true;
}

bool FRigVMSetNodeSizeAction::Undo()
{
	if(!FRigVMBaseAction::Undo())
	{
		return false;
	}
	return GetController()->SetNodeSizeByName(*NodePath, OldSize, false);
}

bool FRigVMSetNodeSizeAction::Redo()
{
	if(!CanUndoRedo())
	{
		return false;
	}
	if(!GetController()->SetNodeSizeByName(*NodePath, NewSize, false))
	{
		return false;
	}
	return FRigVMBaseAction::Redo();
}

FRigVMSetNodeColorAction::FRigVMSetNodeColorAction()
: FRigVMBaseAction(nullptr)
, OldColor(FLinearColor::Black)
, NewColor(FLinearColor::Black)
{
}

FRigVMSetNodeColorAction::FRigVMSetNodeColorAction(URigVMController* InController, URigVMNode* InNode, const FLinearColor& InNewColor)
: FRigVMBaseAction(InController)
, NodePath(InNode->GetNodePath())
, OldColor(InNode->GetNodeColor())
, NewColor(InNewColor)
{
}

bool FRigVMSetNodeColorAction::Merge(const FRigVMBaseAction* Other)
{
	if (!FRigVMBaseAction::Merge(Other))
	{
		return false;
	}

	const FRigVMSetNodeColorAction* Action = (const FRigVMSetNodeColorAction*)Other;
	if (NodePath != Action->NodePath)
	{
		return false;
	}

	NewColor = Action->NewColor;
	return true;
}

bool FRigVMSetNodeColorAction::Undo()
{
	if(!FRigVMBaseAction::Undo())
	{
		return false;
	}
	return GetController()->SetNodeColorByName(*NodePath, OldColor, false);
}

bool FRigVMSetNodeColorAction::Redo()
{
	if(!CanUndoRedo())
	{
		return false;
	}
	if(!GetController()->SetNodeColorByName(*NodePath, NewColor, false))
	{
		return false;
	}
	return FRigVMBaseAction::Redo();
}

FRigVMSetNodeCategoryAction::FRigVMSetNodeCategoryAction()
: FRigVMBaseAction(nullptr)
, OldCategory()
, NewCategory()
{
}

FRigVMSetNodeCategoryAction::FRigVMSetNodeCategoryAction(URigVMController* InController, URigVMCollapseNode* InNode, const FString& InNewCategory)
: FRigVMBaseAction(InController)
, NodePath(InNode->GetNodePath())
, OldCategory(InNode->GetNodeCategory())
, NewCategory(InNewCategory)
{
}

bool FRigVMSetNodeCategoryAction::Merge(const FRigVMBaseAction* Other)
{
	if (!FRigVMBaseAction::Merge(Other))
	{
		return false;
	}

	const FRigVMSetNodeCategoryAction* Action = (const FRigVMSetNodeCategoryAction*)Other;
	if (NodePath != Action->NodePath)
	{
		return false;
	}

	NewCategory = Action->NewCategory;
	return true;
}

bool FRigVMSetNodeCategoryAction::Undo()
{
	if (!FRigVMBaseAction::Undo())
	{
		return false;
	}
	return GetController()->SetNodeCategoryByName(*NodePath, OldCategory, false);
}

bool FRigVMSetNodeCategoryAction::Redo()
{
	if(!CanUndoRedo())
	{
		return false;
	}
	if (!GetController()->SetNodeCategoryByName(*NodePath, NewCategory, false))
	{
		return false;
	}
	return FRigVMBaseAction::Redo();
}

FRigVMSetNodeKeywordsAction::FRigVMSetNodeKeywordsAction()
: FRigVMBaseAction(nullptr)
, OldKeywords()
, NewKeywords()
{
}

FRigVMSetNodeKeywordsAction::FRigVMSetNodeKeywordsAction(URigVMController* InController, URigVMCollapseNode* InNode, const FString& InNewKeywords)
: FRigVMBaseAction(InController)
, NodePath(InNode->GetNodePath())
, OldKeywords(InNode->GetNodeKeywords())
, NewKeywords(InNewKeywords)
{
}

bool FRigVMSetNodeKeywordsAction::Merge(const FRigVMBaseAction* Other)
{
	if (!FRigVMBaseAction::Merge(Other))
	{
		return false;
	}

	const FRigVMSetNodeKeywordsAction* Action = (const FRigVMSetNodeKeywordsAction*)Other;
	if (NodePath != Action->NodePath)
	{
		return false;
	}

	NewKeywords = Action->NewKeywords;
	return true;
}

bool FRigVMSetNodeKeywordsAction::Undo()
{
	if (!FRigVMBaseAction::Undo())
	{
		return false;
	}
	return GetController()->SetNodeKeywordsByName(*NodePath, OldKeywords, false);
}

bool FRigVMSetNodeKeywordsAction::Redo()
{
	if(!CanUndoRedo())
	{
		return false;
	}
	if (!GetController()->SetNodeKeywordsByName(*NodePath, NewKeywords, false))
	{
		return false;
	}
	return FRigVMBaseAction::Redo();
}

FRigVMSetNodeDescriptionAction::FRigVMSetNodeDescriptionAction()
: FRigVMBaseAction(nullptr)
, OldDescription()
, NewDescription()
{
}

FRigVMSetNodeDescriptionAction::FRigVMSetNodeDescriptionAction(URigVMController* InController, URigVMCollapseNode* InNode, const FString& InNewDescription)
: FRigVMBaseAction(InController)
, NodePath(InNode->GetNodePath())
, OldDescription(InNode->GetNodeDescription())
, NewDescription(InNewDescription)
{
}

bool FRigVMSetNodeDescriptionAction::Merge(const FRigVMBaseAction* Other)
{
	if (!FRigVMBaseAction::Merge(Other))
	{
		return false;
	}

	const FRigVMSetNodeDescriptionAction* Action = (const FRigVMSetNodeDescriptionAction*)Other;
	if (NodePath != Action->NodePath)
	{
		return false;
	}

	NewDescription = Action->NewDescription;
	return true;
}

bool FRigVMSetNodeDescriptionAction::Undo()
{
	if (!FRigVMBaseAction::Undo())
	{
		return false;
	}
	return GetController()->SetNodeDescriptionByName(*NodePath, OldDescription, false);
}

bool FRigVMSetNodeDescriptionAction::Redo()
{
	if(!CanUndoRedo())
	{
		return false;
	}
	if (!GetController()->SetNodeDescriptionByName(*NodePath, NewDescription, false))
	{
		return false;
	}
	return FRigVMBaseAction::Redo();
}

FRigVMSetCommentTextAction::FRigVMSetCommentTextAction()
: FRigVMBaseAction(nullptr)
, NodePath()
, OldText()
, NewText()
, OldFontSize(18)
, NewFontSize(18)
, bOldBubbleVisible(false)
, bNewBubbleVisible(false)
, bOldColorBubble(false)
, bNewColorBubble(false)
{
	
}


FRigVMSetCommentTextAction::FRigVMSetCommentTextAction(URigVMController* InController, URigVMCommentNode* InNode, const FString& InNewText, const int32& InNewFontSize, const bool& bInNewBubbleVisible, const bool& bInNewColorBubble)
: FRigVMBaseAction(InController)
, NodePath(InNode->GetNodePath())
, OldText(InNode->GetCommentText())
, NewText(InNewText)
, OldFontSize(InNode->GetCommentFontSize())
, NewFontSize(InNewFontSize)
, bOldBubbleVisible(InNode->GetCommentBubbleVisible())
, bNewBubbleVisible(bInNewBubbleVisible)
, bOldColorBubble(InNode->GetCommentColorBubble())
, bNewColorBubble(bInNewColorBubble)
{
}

bool FRigVMSetCommentTextAction::Undo()
{
	if(!FRigVMBaseAction::Undo())
	{
		return false;
	}
	return GetController()->SetCommentTextByName(*NodePath, OldText, OldFontSize, bOldBubbleVisible, bOldColorBubble, false);
}

bool FRigVMSetCommentTextAction::Redo()
{
	if(!CanUndoRedo())
	{
		return false;
	}
	if(!GetController()->SetCommentTextByName(*NodePath, NewText, NewFontSize, bNewBubbleVisible, bNewColorBubble, false))
	{
		return false;
	}
	return FRigVMBaseAction::Redo();
}

FRigVMRenameVariableAction::FRigVMRenameVariableAction()
: FRigVMBaseAction(nullptr)
{
}

FRigVMRenameVariableAction::FRigVMRenameVariableAction(URigVMController* InController, const FName& InOldVariableName, const FName& InNewVariableName)
: FRigVMBaseAction(InController)
, OldVariableName(InOldVariableName.ToString())
, NewVariableName(InNewVariableName.ToString())
{
}

bool FRigVMRenameVariableAction::Undo()
{
	if(!FRigVMBaseAction::Undo())
	{
		return false;
	}
	return GetController()->OnExternalVariableRenamed(*NewVariableName, *OldVariableName, false);
}

bool FRigVMRenameVariableAction::Redo()
{
	if(!CanUndoRedo())
	{
		return false;
	}
	if(!GetController()->OnExternalVariableRenamed(*OldVariableName, *NewVariableName, false))
	{
		return false;
	}
	return FRigVMBaseAction::Redo();
}

FRigVMSetPinExpansionAction::FRigVMSetPinExpansionAction()
: FRigVMBaseAction(nullptr)
, PinPath()
, OldIsExpanded(false)
, NewIsExpanded(false)
{
}

FRigVMSetPinExpansionAction::FRigVMSetPinExpansionAction(URigVMController* InController, URigVMPin* InPin, bool bNewIsExpanded)
: FRigVMBaseAction(InController)
, PinPath(InPin->GetPinPath())
, OldIsExpanded(InPin->IsExpanded())
, NewIsExpanded(bNewIsExpanded)
{
}

bool FRigVMSetPinExpansionAction::Undo()
{
	if(!FRigVMBaseAction::Undo())
	{
		return false;
	}
	return GetController()->SetPinExpansion(PinPath, OldIsExpanded, false);
}

bool FRigVMSetPinExpansionAction::Redo()
{
	if(!CanUndoRedo())
	{
		return false;
	}
	if(!GetController()->SetPinExpansion(PinPath, NewIsExpanded, false))
	{
		return false;
	}
	return FRigVMBaseAction::Redo();
}

FRigVMSetPinWatchAction::FRigVMSetPinWatchAction()
: FRigVMBaseAction(nullptr)
, OldIsWatched(false)
, NewIsWatched(false)
{
}

FRigVMSetPinWatchAction::FRigVMSetPinWatchAction(URigVMController* InController, URigVMPin* InPin, bool bNewIsWatched)
: FRigVMBaseAction(InController)
, PinPath(InPin->GetPinPath())
, OldIsWatched(InPin->RequiresWatch())
, NewIsWatched(bNewIsWatched)
{
}

bool FRigVMSetPinWatchAction::Undo()
{
	if(!FRigVMBaseAction::Undo())
	{
		return false;
	}
	return GetController()->SetPinIsWatched(PinPath, OldIsWatched, false);
}

bool FRigVMSetPinWatchAction::Redo()
{
	if(!CanUndoRedo())
	{
		return false;
	}
	if(!GetController()->SetPinIsWatched(PinPath, NewIsWatched, false))
	{
		return false;
	}
	return FRigVMBaseAction::Redo();
}

FRigVMSetPinDefaultValueAction::FRigVMSetPinDefaultValueAction()
: FRigVMBaseAction(nullptr)
{
}

FRigVMSetPinDefaultValueAction::FRigVMSetPinDefaultValueAction(URigVMController* InController, URigVMPin* InPin, const FString& InNewDefaultValue)
: FRigVMBaseAction(InController)
, PinPath(InPin->GetPinPath())
, OldDefaultValue(InPin->GetDefaultValueStoredByUserInterface())
, NewDefaultValue(InNewDefaultValue)
{
	/* Since for template we are chaning types - it is possible that the
	 * pin is no longer compliant with the old value
	if(!OldDefaultValue.IsEmpty())
	{
		check(InPin->IsValidDefaultValue(OldDefaultValue));
	}
	*/
	if(!NewDefaultValue.IsEmpty())
	{
		check(InPin->IsValidDefaultValue(NewDefaultValue));
	}
}

bool FRigVMSetPinDefaultValueAction::Merge(const FRigVMBaseAction* Other)
{
	if (!FRigVMBaseAction::Merge(Other))
	{
		return false;
	}

	const FRigVMSetPinDefaultValueAction* Action = (const FRigVMSetPinDefaultValueAction*)Other;
	if (PinPath != Action->PinPath)
	{
		return false;
	}

	NewDefaultValue = Action->NewDefaultValue;
	return true;
}

bool FRigVMSetPinDefaultValueAction::Undo()
{
	if(!FRigVMBaseAction::Undo())
	{
		return false;
	}
	if (OldDefaultValue.IsEmpty())
	{
		return true;
	}
	return GetController()->SetPinDefaultValue(PinPath, OldDefaultValue, true, false);
}

bool FRigVMSetPinDefaultValueAction::Redo()
{
	if(!CanUndoRedo())
	{
		return false;
	}
	if (!NewDefaultValue.IsEmpty())
	{
		if (!GetController()->SetPinDefaultValue(PinPath, NewDefaultValue, true, false))
		{
			return false;
		}
	}
	return FRigVMBaseAction::Redo();
}

FRigVMInsertArrayPinAction::FRigVMInsertArrayPinAction()
: FRigVMBaseAction(nullptr)
, Index(0)
{
}

FRigVMInsertArrayPinAction::FRigVMInsertArrayPinAction(URigVMController* InController, URigVMPin* InArrayPin, int32 InIndex, const FString& InNewDefaultValue)
: FRigVMBaseAction(InController)
, ArrayPinPath(InArrayPin->GetPinPath())
, Index(InIndex)
, NewDefaultValue(InNewDefaultValue)
{
}

bool FRigVMInsertArrayPinAction::Undo()
{
	if(!FRigVMBaseAction::Undo())
	{
		return false;
	}
	return GetController()->RemoveArrayPin(FString::Printf(TEXT("%s.%d"), *ArrayPinPath, Index), false);
}

bool FRigVMInsertArrayPinAction::Redo()
{
	if(!CanUndoRedo())
	{
		return false;
	}
	if(GetController()->InsertArrayPin(ArrayPinPath, Index, NewDefaultValue, false).IsEmpty())
	{
		return false;
	}
	return FRigVMBaseAction::Redo();
}

FRigVMRemoveArrayPinAction::FRigVMRemoveArrayPinAction()
: FRigVMBaseAction(nullptr)
, Index(0)
{
}

FRigVMRemoveArrayPinAction::FRigVMRemoveArrayPinAction(URigVMController* InController, URigVMPin* InArrayElementPin)
: FRigVMBaseAction(InController)
, ArrayPinPath(InArrayElementPin->GetParentPin()->GetPinPath())
, Index(InArrayElementPin->GetPinIndex())
, DefaultValue(InArrayElementPin->GetDefaultValue())
{
}

bool FRigVMRemoveArrayPinAction::Undo()
{
	if(!CanUndoRedo())
	{
		return false;
	}
	if (GetController()->InsertArrayPin(*ArrayPinPath, Index, DefaultValue, false).IsEmpty())
	{
		return false;
	}
	return FRigVMBaseAction::Undo();
}

bool FRigVMRemoveArrayPinAction::Redo()
{
	if (!FRigVMBaseAction::Redo())
	{
		return false;
	}
	return GetController()->RemoveArrayPin(FString::Printf(TEXT("%s.%d"), *ArrayPinPath, Index), false);
}

FRigVMAddLinkAction::FRigVMAddLinkAction()
: FRigVMBaseAction(nullptr)
{
}

FRigVMAddLinkAction::FRigVMAddLinkAction(URigVMController* InController, URigVMPin* InOutputPin, URigVMPin* InInputPin)
: FRigVMBaseAction(InController)
, OutputPinPath(InOutputPin->GetPinPath())
, InputPinPath(InInputPin->GetPinPath())
{
}

bool FRigVMAddLinkAction::Undo()
{
	if(!FRigVMBaseAction::Undo())
	{
		return false;
	}
	return GetController()->BreakLink(OutputPinPath, InputPinPath, false);
}

bool FRigVMAddLinkAction::Redo()
{
	if(!CanUndoRedo())
	{
		return false;
	}
	if(!GetController()->AddLink(OutputPinPath, InputPinPath, false))
	{
		return false;
	}
	return FRigVMBaseAction::Redo();
}

FRigVMBreakLinkAction::FRigVMBreakLinkAction()
: FRigVMBaseAction(nullptr)
{
}

FRigVMBreakLinkAction::FRigVMBreakLinkAction(URigVMController* InController, URigVMPin* InOutputPin, URigVMPin* InInputPin)
: FRigVMBaseAction(InController)
, OutputPinPath(InOutputPin->GetPinPath())
, InputPinPath(InInputPin->GetPinPath())
{
	GraphPath = TSoftObjectPtr<URigVMGraph>(Cast<URigVMGraph>(InOutputPin->GetGraph())).GetUniqueID();
}

bool FRigVMBreakLinkAction::Undo()
{
	if(!FRigVMBaseAction::Undo())
	{
		return false;
	}
	return GetController()->AddLink(OutputPinPath, InputPinPath, false);
}

bool FRigVMBreakLinkAction::Redo()
{
	if(!CanUndoRedo())
	{
		return false;
	}
	if(!GetController()->BreakLink(OutputPinPath, InputPinPath, false))
	{
		return false;
	}
	return FRigVMBaseAction::Redo();
}

FRigVMChangePinTypeAction::FRigVMChangePinTypeAction()
: FRigVMBaseAction(nullptr)
, PinPath()
, OldTypeIndex(INDEX_NONE)
, NewTypeIndex(INDEX_NONE)
, bSetupOrphanPins(true)
, bBreakLinks(true)
, bRemoveSubPins(true)
{}

FRigVMChangePinTypeAction::FRigVMChangePinTypeAction(URigVMController* InController, URigVMPin* InPin, int32 InTypeIndex, bool InSetupOrphanPins, bool InBreakLinks, bool InRemoveSubPins)
: FRigVMBaseAction(InController)
, PinPath(InPin->GetPinPath())
, OldTypeIndex(InPin->GetTypeIndex())
, NewTypeIndex(InTypeIndex)
, bSetupOrphanPins(InSetupOrphanPins)
, bBreakLinks(InBreakLinks)
, bRemoveSubPins(InRemoveSubPins)
{
}

bool FRigVMChangePinTypeAction::Undo()
{
	if(!FRigVMBaseAction::Undo())
	{
		return false;
	}

	if(const URigVMGraph* Graph = GetController()->GetGraph())
	{
		if(URigVMPin* Pin = Graph->FindPin(PinPath))
		{
			return GetController()->ChangePinType(Pin, OldTypeIndex, false, bSetupOrphanPins, bBreakLinks, bRemoveSubPins);
		}
	}
	return false;
}

bool FRigVMChangePinTypeAction::Redo()
{
	if(!CanUndoRedo())
	{
		return false;
	}
	if(const URigVMGraph* Graph = GetController()->GetGraph())
	{
		if(URigVMPin* Pin = Graph->FindPin(PinPath))
		{
			if(!GetController()->ChangePinType(Pin, NewTypeIndex, false, bSetupOrphanPins, bBreakLinks, bRemoveSubPins))
			{
				return false;
			}
		}
		else
		{
			return false;
		}
	}
	else
	{
		return false;
	}
	return FRigVMBaseAction::Redo();
}

FRigVMCollapseNodesAction::FRigVMCollapseNodesAction()
: FRigVMBaseAction(nullptr)
, LibraryNodePath()
, bIsAggregate(false)
{
}

FRigVMCollapseNodesAction::FRigVMCollapseNodesAction(URigVMController* InController, const TArray<URigVMNode*>& InNodes, const FString& InNodePath, const bool bIsAggregate)
: FRigVMBaseAction(InController)
, LibraryNodePath(InNodePath)
, bIsAggregate(bIsAggregate)
{
	TArray<FName> NodesToExport;
	for (const URigVMNode* InNode : InNodes)
	{
		CollapsedNodesPaths.Add(InNode->GetName());
		NodesToExport.Add(InNode->GetFName());
	}

	CollapsedNodesContent = InController->ExportNodesToText(NodesToExport, true);
}

bool FRigVMCollapseNodesAction::Undo()
{
	if (!FRigVMBaseAction::Undo())
	{
		return false;
	}

	// remove the library node
	{
		if(!GetController()->RemoveNodeByName(*LibraryNodePath, false, false))
		{
			return false;
		}
	}

	const TArray<FName> RecoveredNodes = GetController()->ImportNodesFromText(CollapsedNodesContent, false, false);
	if(RecoveredNodes.Num() != CollapsedNodesPaths.Num())
	{
		return false;
	}

	return true;
}

bool FRigVMCollapseNodesAction::Redo()
{
	if(!CanUndoRedo())
	{
		return false;
	}
#if WITH_EDITOR
	TArray<FName> NodeNames;
	for (const FString& NodePath : CollapsedNodesPaths)
	{
		NodeNames.Add(*NodePath);
	}

	URigVMLibraryNode* LibraryNode = GetController()->CollapseNodes(NodeNames, LibraryNodePath, false, false, bIsAggregate);
	if (LibraryNode)
	{
		return FRigVMBaseAction::Redo();
	}
#endif
	return false;
}

FRigVMExpandNodeAction::FRigVMExpandNodeAction()
: FRigVMBaseAction(nullptr)
, LibraryNodePath()
{
}

FRigVMExpandNodeAction::FRigVMExpandNodeAction(URigVMController* InController, URigVMLibraryNode* InLibraryNode)
: FRigVMBaseAction(InController)
, LibraryNodePath(InLibraryNode->GetName())
{
	TArray<FName> NodesToExport;
	NodesToExport.Add(InLibraryNode->GetFName());
	LibraryNodeContent = InController->ExportNodesToText(NodesToExport, true);
}

bool FRigVMExpandNodeAction::Undo()
{
	if (!FRigVMBaseAction::Undo())
	{
		return false;
	}

	// remove the expanded nodes
	TArray<FName> NodeNames;
	Algo::Transform(ExpandedNodePaths, NodeNames, [](const FString& NodePath)
	{
		return FName(*NodePath); 
	});
	if(!GetController()->RemoveNodesByName(NodeNames, false, false))
	{
		return false;
	}

	const TArray<FName> RecoveredNodes = GetController()->ImportNodesFromText(LibraryNodeContent, false, false);
	if(RecoveredNodes.Num() != 1)
	{
		return false;
	}

	return true;
}

bool FRigVMExpandNodeAction::Redo()
{
	if(!CanUndoRedo())
	{
		return false;
	}
#if WITH_EDITOR
	const TArray<URigVMNode*> ExpandedNodes = GetController()->ExpandLibraryNode(*LibraryNodePath, false);
	if (ExpandedNodes.Num() == ExpandedNodePaths.Num())
	{
		return FRigVMBaseAction::Redo();
	}
#endif
	return false;
}

FRigVMRenameNodeAction::FRigVMRenameNodeAction()
: FRigVMBaseAction(nullptr)
{
}

FRigVMRenameNodeAction::FRigVMRenameNodeAction(URigVMController* InController, const FName& InOldNodeName, const FName& InNewNodeName)
: FRigVMBaseAction(InController)
, OldNodeName(InOldNodeName.ToString())
, NewNodeName(InNewNodeName.ToString())
{
}

bool FRigVMRenameNodeAction::Undo()
{
	if (!FRigVMBaseAction::Undo())
	{
		return false;
	}
	if (URigVMNode* Node = GetController()->GetGraph()->FindNode(NewNodeName))
	{
		return GetController()->RenameNode(Node, *OldNodeName, false);
	}
	return false;
}

bool FRigVMRenameNodeAction::Redo()
{
	if(!CanUndoRedo())
	{
		return false;
	}
	if (URigVMNode* Node = GetController()->GetGraph()->FindNode(OldNodeName))
	{
		return GetController()->RenameNode(Node, *NewNodeName, false);
	}
	return false;
}

FRigVMAddExposedPinAction::FRigVMAddExposedPinAction()
: FRigVMBaseAction(nullptr)
, Direction(ERigVMPinDirection::Input)
{
}

FRigVMAddExposedPinAction::FRigVMAddExposedPinAction(URigVMController* InController, URigVMPin* InPin)
: FRigVMBaseAction(InController)
, PinName(InPin->GetName())
, Direction(InPin->GetDirection())
, CPPType(InPin->GetCPPType())
, CPPTypeObjectPath()
, DefaultValue(InPin->GetDefaultValue())
{
	if (UObject* CPPTypeObject = InPin->GetCPPTypeObject())
	{
		CPPTypeObjectPath = CPPTypeObject->GetPathName();
	}
}

bool FRigVMAddExposedPinAction::Undo()
{
	if (!FRigVMBaseAction::Undo())
	{
		return false;
	}
	return GetController()->RemoveExposedPin(*PinName, false);
}

bool FRigVMAddExposedPinAction::Redo()
{
	if(!CanUndoRedo())
	{
		return false;
	}
	if (!GetController()->AddExposedPin(*PinName, Direction, CPPType, *CPPTypeObjectPath, DefaultValue, false).IsNone())
	{
		return FRigVMBaseAction::Redo();
	}
	return false;
}

FRigVMRemoveExposedPinAction::FRigVMRemoveExposedPinAction()
: FRigVMBaseAction(nullptr)
, Direction(ERigVMPinDirection::Input)
, PinIndex(INDEX_NONE)
{
}

FRigVMRemoveExposedPinAction::FRigVMRemoveExposedPinAction(URigVMController* InController, URigVMPin* InPin)
: FRigVMBaseAction(InController)
, PinName(InPin->GetName())
, Direction(InPin->GetDirection())
, CPPType(InPin->GetCPPType())
, CPPTypeObjectPath()
, DefaultValue(InPin->GetDefaultValue())
, PinIndex(InPin->GetPinIndex())
{
	if (UObject* CPPTypeObject = InPin->GetCPPTypeObject())
	{
		CPPTypeObjectPath = CPPTypeObject->GetPathName();
	}
}

bool FRigVMRemoveExposedPinAction::Undo()
{
	if(!CanUndoRedo())
	{
		return false;
	}
	if(!GetController()->AddExposedPin(*PinName, Direction, CPPType, *CPPTypeObjectPath, DefaultValue, false).IsNone())
	{
		if (GetController()->SetExposedPinIndex(*PinName, PinIndex, false))
		{
			return FRigVMBaseAction::Undo();
		}
	}
	return false;
}

bool FRigVMRemoveExposedPinAction::Redo()
{
	if(!CanUndoRedo())
	{
		return false;
	}
	if(FRigVMBaseAction::Redo())
	{
		return GetController()->RemoveExposedPin(*PinName, false);
	}
	return false;
}

FRigVMRenameExposedPinAction::FRigVMRenameExposedPinAction()
: FRigVMBaseAction(nullptr)
{
}

FRigVMRenameExposedPinAction::FRigVMRenameExposedPinAction(URigVMController* InController, const FName& InOldPinName, const FName& InNewPinName)
: FRigVMBaseAction(InController)
, OldPinName(InOldPinName.ToString())
, NewPinName(InNewPinName.ToString())
{
}

bool FRigVMRenameExposedPinAction::Undo()
{
	if (!FRigVMBaseAction::Undo())
	{
		return false;
	}
	return GetController()->RenameExposedPin(*NewPinName, *OldPinName, false);
}

bool FRigVMRenameExposedPinAction::Redo()
{
	if(!CanUndoRedo())
	{
		return false;
	}
	if(!GetController()->RenameExposedPin(*OldPinName, *NewPinName, false))
	{
		return false;
	}
	return FRigVMBaseAction::Redo();
}

FRigVMSetPinIndexAction::FRigVMSetPinIndexAction()
: FRigVMBaseAction(nullptr)
, PinPath()
, OldIndex(INDEX_NONE)
, NewIndex(INDEX_NONE)
{
}

FRigVMSetPinIndexAction::FRigVMSetPinIndexAction(URigVMController* InController, URigVMPin* InPin, int32 InNewIndex)
: FRigVMBaseAction(InController)
, PinPath(InPin->GetName())
, OldIndex(InPin->GetPinIndex())
, NewIndex(InNewIndex)
{
}

bool FRigVMSetPinIndexAction::Undo()
{
	if (!FRigVMBaseAction::Undo())
	{
		return false;
	}
	return GetController()->SetExposedPinIndex(*PinPath, OldIndex, false);
}

bool FRigVMSetPinIndexAction::Redo()
{
	if(!CanUndoRedo())
	{
		return false;
	}
	if (!GetController()->SetExposedPinIndex(*PinPath, NewIndex, false))
	{
		return false;
	}
	return FRigVMBaseAction::Redo();
}

FRigVMSetRemappedVariableAction::FRigVMSetRemappedVariableAction()
: FRigVMBaseAction(nullptr)
, InnerVariableName(NAME_None)
, OldOuterVariableName(NAME_None)
, NewOuterVariableName(NAME_None)
{
}

FRigVMSetRemappedVariableAction::FRigVMSetRemappedVariableAction(
	URigVMController* InController,
	URigVMFunctionReferenceNode* InFunctionRefNode,
	const FName& InInnerVariableName,
	const FName& InOldOuterVariableName,
	const FName& InNewOuterVariableName)
: FRigVMBaseAction(InController)
, NodePath()
, InnerVariableName(InInnerVariableName)
, OldOuterVariableName(InOldOuterVariableName)
, NewOuterVariableName(InNewOuterVariableName)
{
	if(InFunctionRefNode)
	{
		NodePath = InFunctionRefNode->GetName();
	}
}

bool FRigVMSetRemappedVariableAction::Undo()
{
	if (!FRigVMBaseAction::Undo())
	{
		return false;
	}
	if (URigVMFunctionReferenceNode* Node = Cast<URigVMFunctionReferenceNode>(GetController()->GetGraph()->FindNode(NodePath)))
	{
		return GetController()->SetRemappedVariable(Node, InnerVariableName, OldOuterVariableName, false);
	}
	return false;
}

bool FRigVMSetRemappedVariableAction::Redo()
{
	if(!CanUndoRedo())
	{
		return false;
	}
	if (URigVMFunctionReferenceNode* Node = Cast<URigVMFunctionReferenceNode>(GetController()->GetGraph()->FindNode(NodePath)))
	{
		return GetController()->SetRemappedVariable(Node, InnerVariableName, NewOuterVariableName, false);
	}
	return false;
}

FRigVMAddLocalVariableAction::FRigVMAddLocalVariableAction()
: FRigVMBaseAction(nullptr)
{
}

FRigVMAddLocalVariableAction::FRigVMAddLocalVariableAction(URigVMController* InController, const FRigVMGraphVariableDescription& InLocalVariable)
: FRigVMBaseAction(InController)
, LocalVariable(InLocalVariable)
{
	
}

bool FRigVMAddLocalVariableAction::Undo()
{
	if (!FRigVMBaseAction::Undo())
	{
		return false;
	}
	return GetController()->RemoveLocalVariable(LocalVariable.Name, false);
}

bool FRigVMAddLocalVariableAction::Redo()
{
	if(!CanUndoRedo())
	{
		return false;
	}
	if (!LocalVariable.Name.IsNone())
	{
		return GetController()->AddLocalVariable(LocalVariable.Name, LocalVariable.CPPType, LocalVariable.CPPTypeObject, LocalVariable.DefaultValue, false).Name.IsNone() == false;
	}
	return FRigVMBaseAction::Redo();
}

FRigVMRemoveLocalVariableAction::FRigVMRemoveLocalVariableAction()
: FRigVMBaseAction(nullptr)
{
}

FRigVMRemoveLocalVariableAction::FRigVMRemoveLocalVariableAction(URigVMController* InController, const FRigVMGraphVariableDescription& InLocalVariable)
: FRigVMBaseAction(InController)
, LocalVariable(InLocalVariable)
{
}

bool FRigVMRemoveLocalVariableAction::Undo()
{
	if (!FRigVMBaseAction::Undo())
	{
		return false;
	}
	return !GetController()->AddLocalVariable(LocalVariable.Name, LocalVariable.CPPType, LocalVariable.CPPTypeObject, LocalVariable.DefaultValue, false).Name.IsNone();
}

bool FRigVMRemoveLocalVariableAction::Redo()
{
	if(!CanUndoRedo())
	{
		return false;
	}
	if (!LocalVariable.Name.IsNone())
	{
		return GetController()->RemoveLocalVariable(LocalVariable.Name, false);
	}
	return FRigVMBaseAction::Redo();
}

FRigVMRenameLocalVariableAction::FRigVMRenameLocalVariableAction()
: FRigVMBaseAction(nullptr)
, OldVariableName(NAME_None)
, NewVariableName(NAME_None)
{
}

FRigVMRenameLocalVariableAction::FRigVMRenameLocalVariableAction(URigVMController* InController, const FName& InOldName, const FName& InNewName)
: FRigVMBaseAction(InController)
, OldVariableName(InOldName)
, NewVariableName(InNewName)
{
	
}

bool FRigVMRenameLocalVariableAction::Undo()
{
	if (!FRigVMBaseAction::Undo())
	{
		return false;
	}
	return GetController()->RenameLocalVariable(NewVariableName, OldVariableName, false);
}

bool FRigVMRenameLocalVariableAction::Redo()
{
	if(!CanUndoRedo())
	{
		return false;
	}
	if (!GetController()->RenameLocalVariable(OldVariableName, NewVariableName, false))
	{
		return false;
	}
	return FRigVMBaseAction::Redo();
}

FRigVMChangeLocalVariableTypeAction::FRigVMChangeLocalVariableTypeAction()
: FRigVMBaseAction(nullptr)
, LocalVariable()
, CPPType()
, CPPTypeObject(nullptr)
{
}

FRigVMChangeLocalVariableTypeAction::FRigVMChangeLocalVariableTypeAction(
	URigVMController* InController, 
	const FRigVMGraphVariableDescription& InLocalVariable,
	const FString& InCPPType,
	UObject* InCPPTypeObject)
: FRigVMBaseAction(InController)
, LocalVariable(InLocalVariable)
, CPPType(InCPPType)
, CPPTypeObject(InCPPTypeObject)
{
}

bool FRigVMChangeLocalVariableTypeAction::Undo()
{
	if (!FRigVMBaseAction::Undo())
	{
		return false;
	}
	return GetController()->SetLocalVariableType(LocalVariable.Name, LocalVariable.CPPType, LocalVariable.CPPTypeObject, false);
}

bool FRigVMChangeLocalVariableTypeAction::Redo()
{
	if(!CanUndoRedo())
	{
		return false;
	}
	if (!GetController()->SetLocalVariableType(LocalVariable.Name, CPPType, CPPTypeObject, false))
	{
		return false;
	}
	return FRigVMBaseAction::Redo();
}

FRigVMChangeLocalVariableDefaultValueAction::FRigVMChangeLocalVariableDefaultValueAction()
: FRigVMBaseAction(nullptr)
, LocalVariable()
, DefaultValue()
{
}

FRigVMChangeLocalVariableDefaultValueAction::FRigVMChangeLocalVariableDefaultValueAction(
	URigVMController* InController, 
	const FRigVMGraphVariableDescription& InLocalVariable,
	const FString& InDefaultValue)
: FRigVMBaseAction(InController)
, LocalVariable(InLocalVariable)
, DefaultValue(InDefaultValue)
{
}

bool FRigVMChangeLocalVariableDefaultValueAction::Undo()
{
	if (!FRigVMBaseAction::Undo())
	{
		return false;
	}
	return GetController()->SetLocalVariableDefaultValue(LocalVariable.Name, LocalVariable.DefaultValue, false);
}

bool FRigVMChangeLocalVariableDefaultValueAction::Redo()
{
	if(!CanUndoRedo())
	{
		return false;
	}
	if (!GetController()->SetLocalVariableDefaultValue(LocalVariable.Name, DefaultValue, false))
	{
		return false;		
	}
	return FRigVMBaseAction::Redo();
}

FRigVMPromoteNodeAction::FRigVMPromoteNodeAction()
: FRigVMBaseAction(nullptr)
, LibraryNodePath()
, FunctionDefinitionPath()
, bFromFunctionToCollapseNode(false)
{
}

FRigVMPromoteNodeAction::FRigVMPromoteNodeAction(URigVMController* InController, const URigVMNode* InNodeToPromote, const FString& InNodePath, const FString& InFunctionDefinitionPath)
: FRigVMBaseAction(InController)
, LibraryNodePath(InNodePath)
, FunctionDefinitionPath(InFunctionDefinitionPath)
, bFromFunctionToCollapseNode(InNodeToPromote->IsA<URigVMFunctionReferenceNode>())
{
}

bool FRigVMPromoteNodeAction::Undo()
{
	if(bFromFunctionToCollapseNode)
	{
		const FName FunctionRefNodeName = GetController()->PromoteCollapseNodeToFunctionReferenceNode(*LibraryNodePath, false, false, FunctionDefinitionPath);
		return FunctionRefNodeName.ToString() == LibraryNodePath;
	}

	const FName CollapseNodeName = GetController()->PromoteFunctionReferenceNodeToCollapseNode(*LibraryNodePath, false, false, true);
	return CollapseNodeName.ToString() == LibraryNodePath;
}

bool FRigVMPromoteNodeAction::Redo()
{
	if(!CanUndoRedo())
	{
		return false;
	}
	if(bFromFunctionToCollapseNode)
	{
		const FName CollapseNodeName = GetController()->PromoteFunctionReferenceNodeToCollapseNode(*LibraryNodePath, false, false, false);
		return CollapseNodeName.ToString() == LibraryNodePath;
	}
	const FName FunctionRefNodeName = GetController()->PromoteCollapseNodeToFunctionReferenceNode(*LibraryNodePath, false, false);
	return FunctionRefNodeName.ToString() == LibraryNodePath;
}

FRigVMMarkFunctionPublicAction::FRigVMMarkFunctionPublicAction()
: FRigVMBaseAction(nullptr)
, FunctionName(NAME_None)
, bIsPublic(false)
{
}

FRigVMMarkFunctionPublicAction::FRigVMMarkFunctionPublicAction(URigVMController* InController, const FName& InFunctionName, bool bInIsPublic)
: FRigVMBaseAction(InController)
, FunctionName(InFunctionName)
, bIsPublic(bInIsPublic)
{
}

bool FRigVMMarkFunctionPublicAction::Undo()
{
	if (!FRigVMBaseAction::Undo())
	{
		return false;
	}
	return GetController()->MarkFunctionAsPublic(FunctionName, !bIsPublic, false);
}

bool FRigVMMarkFunctionPublicAction::Redo()
{
	if(!CanUndoRedo())
	{
		return false;
	}
#if WITH_EDITOR
	if (GetController()->MarkFunctionAsPublic(FunctionName, bIsPublic, false))
	{
		return FRigVMBaseAction::Redo();
	}
#endif
	return false;
}

FRigVMImportFromTextAction::FRigVMImportFromTextAction()
: FRigVMBaseAction(nullptr)
{
}

FRigVMImportFromTextAction::FRigVMImportFromTextAction(URigVMController* InController, const FString& InContent, const TArray<FName>& InTopLevelNodeNames)
: FRigVMBaseAction(InController)
, Content(InContent)
, TopLevelNodeNames(InTopLevelNodeNames)
{
}

FRigVMImportFromTextAction::FRigVMImportFromTextAction(URigVMController* InController, URigVMNode* InNode, bool bIncludeExteriorLinks)
: FRigVMBaseAction(InController)
{
	SetContent({InNode}, bIncludeExteriorLinks);
}

FRigVMImportFromTextAction::FRigVMImportFromTextAction(URigVMController* InController, const TArray<URigVMNode*>& InNodes, bool bIncludeExteriorLinks)
: FRigVMBaseAction(InController)
{
	SetContent(InNodes, bIncludeExteriorLinks);
}

void FRigVMImportFromTextAction::SetContent(const TArray<URigVMNode*>& InNodes, bool bIncludeExteriorLinks)
{
	TopLevelNodeNames.Reset();
	Algo::Transform(InNodes, TopLevelNodeNames, [](const URigVMNode* Node)
	{
		return Node->GetFName();
	});
	Content = GetController()->ExportNodesToText(TopLevelNodeNames, bIncludeExteriorLinks);
}

bool FRigVMImportFromTextAction::Undo()
{
	if(!FRigVMBaseAction::Undo())
	{
		return false;
	}
	return GetController()->RemoveNodesByName(TopLevelNodeNames, false, false);
}

bool FRigVMImportFromTextAction::Redo()
{
	if(!CanUndoRedo())
	{
		return false;
	}
	const TArray<FName> ImportedNames = GetController()->ImportNodesFromText(Content, false, false);
	if(ImportedNames.Num() != TopLevelNodeNames.Num())
	{
		return false;
	}
	return FRigVMBaseAction::Redo();
}

FRigVMReplaceNodesAction::FRigVMReplaceNodesAction()
: FRigVMBaseAction(nullptr)
{
}

FRigVMReplaceNodesAction::FRigVMReplaceNodesAction(URigVMController* InController, const TArray<URigVMNode*>& InNodes)
: FRigVMBaseAction(InController)
{
	for(const URigVMNode* Node : InNodes)
	{
		StoreNode(Node, true);
	}
}

bool FRigVMReplaceNodesAction::Undo()
{
	if(!FRigVMBaseAction::Undo())
	{
		return false;
	}

	for(const TPair<FName, FRigVMActionNodeContent>& Pair : ExportedNodes)
	{
		if(!RestoreNode(Pair.Key, true))
		{
			return false;
		}
	}

	return true;
}

bool FRigVMReplaceNodesAction::Redo()
{
	if(!CanUndoRedo())
	{
		return false;
	}
	for(const TPair<FName, FRigVMActionNodeContent>& Pair : ExportedNodes)
	{
		if(!RestoreNode(Pair.Key, false))
		{
			return false;
		}
	}
	
	return FRigVMBaseAction::Redo();
}

FRigVMAddDecoratorAction::FRigVMAddDecoratorAction()
: FRigVMBaseAction()
, NodeName(NAME_None)
, DecoratorName(NAME_None)
, ScriptStructPath()
, DecoratorDefault()
, PinIndex(INDEX_NONE)
{
}

FRigVMAddDecoratorAction::FRigVMAddDecoratorAction(URigVMController* InController, const URigVMNode* InNode, const FName& InDecoratorName,
	const UScriptStruct* InDecoratorScriptStruct, const FString& InDecoratorDefault, int32 InPinIndex)
: FRigVMBaseAction(InController)
, NodeName(InNode->GetFName())
, DecoratorName(InDecoratorName)
, ScriptStructPath(InDecoratorScriptStruct->GetPathName())
, DecoratorDefault(InDecoratorDefault)
, PinIndex(InPinIndex)
{
}

bool FRigVMAddDecoratorAction::Undo()
{
	if (!FRigVMBaseAction::Undo())
	{
		return false;
	}
	return GetController()->RemoveDecorator(NodeName, DecoratorName, false);
}

bool FRigVMAddDecoratorAction::Redo()
{
	if(!CanUndoRedo())
	{
		return false;
	}
	if (GetController()->AddDecorator(NodeName, *ScriptStructPath, DecoratorName, DecoratorDefault, PinIndex, false, false) == DecoratorName)
	{
		return FRigVMBaseAction::Redo();
	}
	return false;
}

FRigVMRemoveDecoratorAction::FRigVMRemoveDecoratorAction()
: FRigVMAddDecoratorAction()
{
}

FRigVMRemoveDecoratorAction::FRigVMRemoveDecoratorAction(URigVMController* InController, const URigVMNode* InNode,
	const FName& InDecoratorName, const UScriptStruct* InDecoratorScriptStruct, const FString& InDecoratorDefault, int32 InPinIndex)
: FRigVMAddDecoratorAction(InController, InNode, InDecoratorName, InDecoratorScriptStruct, InDecoratorDefault, InPinIndex)
{
}

bool FRigVMRemoveDecoratorAction::Undo()
{
	if (!FRigVMBaseAction::Undo())
	{
		return false;
	}
	return GetController()->AddDecorator(NodeName, *ScriptStructPath, DecoratorName, DecoratorDefault, PinIndex, false, false) == DecoratorName;
}

bool FRigVMRemoveDecoratorAction::Redo()
{
	if(!CanUndoRedo())
	{
		return false;
	}
	if (GetController()->RemoveDecorator(NodeName, DecoratorName, false))
	{
		return FRigVMBaseAction::Redo();
	}
	return false;
}
