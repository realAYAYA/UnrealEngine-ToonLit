// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeViewModel.h"
#include "StateTree.h"
#include "StateTreeEditorData.h"
#include "StateTreeDelegates.h"
#include "Debugger/StateTreeDebugger.h"
#include "Editor.h"
#include "ScopedTransaction.h"
#include "HAL/PlatformApplicationMisc.h"
#include "UnrealExporter.h"
#include "Exporters/Exporter.h"
#include "Factories.h"


#define LOCTEXT_NAMESPACE "StateTreeEditor"

namespace UE::StateTree::Editor
{
	class FStateTreeStateTextFactory : public FCustomizableTextObjectFactory
	{
	public:
		FStateTreeStateTextFactory()
			: FCustomizableTextObjectFactory(GWarn)
		{}

		virtual bool CanCreateClass(UClass* InObjectClass, bool& bOmitSubObjs) const override
		{
			UE_LOG(LogTemp, Error, TEXT("*** CanCreateClass: %s"), *GetNameSafe(InObjectClass));
			return InObjectClass->IsChildOf(UStateTreeState::StaticClass())
				|| InObjectClass->IsChildOf(UStateTreeClipboardBindings::StaticClass());
		}

		virtual void ProcessConstructedObject(UObject* NewObject) override
		{
			if (UStateTreeState* State = Cast<UStateTreeState>(NewObject))
			{
				States.Add(State);
			}
			else if (UStateTreeClipboardBindings* Bindings = Cast<UStateTreeClipboardBindings>(NewObject))
			{
				ClipboardBindings = Bindings;
			}
		}

	public:
		TArray<UStateTreeState*> States;
		UStateTreeClipboardBindings* ClipboardBindings = nullptr;
	};


	void CollectBindingsRecursive(UStateTreeEditorData* TreeData, UStateTreeState* State, TArray<FStateTreePropertyPathBinding>& AllBindings)
	{
		if (!State)
		{
			return;
		}
		
		TreeData->VisitStateNodes(*State, [TreeData, &AllBindings](const UStateTreeState* State, const FStateTreeBindableStructDesc& Desc, const FStateTreeDataView Value)
		{
			TArray<FStateTreePropertyPathBinding> NodeBindings;
			TreeData->GetPropertyEditorBindings()->GetPropertyBindingsFor(Desc.ID, NodeBindings);
			AllBindings.Append(NodeBindings);
			return EStateTreeVisitor::Continue;				
		});

		for (UStateTreeState* ChildState : State->Children)
		{
			CollectBindingsRecursive(TreeData, ChildState, AllBindings);
		}
	}

	FString ExportStatesToText(UStateTreeEditorData* TreeData, const TArrayView<UStateTreeState*> States)
	{
		if (States.IsEmpty())
		{
			return FString();
		}

		// Clear the mark state for saving.
		UnMarkAllObjects(EObjectMark(OBJECTMARK_TagExp | OBJECTMARK_TagImp));

		FStringOutputDevice Archive;
		const FExportObjectInnerContext Context;

		UStateTreeClipboardBindings* ClipboardBindings = NewObject<UStateTreeClipboardBindings>();
		check(ClipboardBindings);

		for (UStateTreeState* State : States)
		{
			UObject* ThisOuter = State->GetOuter();
			UExporter::ExportToOutputDevice(&Context, State, nullptr, Archive, TEXT("copy"), 0, PPF_ExportsNotFullyQualified | PPF_Copy | PPF_Delimited, false, ThisOuter);

			CollectBindingsRecursive(TreeData, State, ClipboardBindings->Bindings);
		}

		UExporter::ExportToOutputDevice(&Context, ClipboardBindings, nullptr, Archive, TEXT("copy"), 0, PPF_ExportsNotFullyQualified | PPF_Copy | PPF_Delimited, false);

		return *Archive;
	}

	void CollectStateLinks(const UStruct* Struct, void* Memory, TArray<FStateTreeStateLink*>& Links)
	{
		for (TPropertyValueIterator<FStructProperty> It(Struct, Memory); It; ++It)
		{
			if (It->Key->Struct == TBaseStructure<FStateTreeStateLink>::Get())
			{
				FStateTreeStateLink* StateLink = static_cast<FStateTreeStateLink*>(const_cast<void*>(It->Value));
				Links.Add(StateLink);
			}
		}
	}

	void FixNodesAfterDuplication(TArrayView<FStateTreeEditorNode> Nodes, TMap<FGuid, FGuid>& IDsMap, TArray<FStateTreeStateLink*>& Links)
	{
		for (FStateTreeEditorNode& Node : Nodes)
		{
			const FGuid NewNodeID = FGuid::NewGuid();
			IDsMap.Emplace(Node.ID, NewNodeID);
			Node.ID = NewNodeID;

			if (Node.Node.IsValid())
			{
				CollectStateLinks(Node.Node.GetScriptStruct(), Node.Node.GetMutableMemory(), Links);
			}
			if (Node.Instance.IsValid())
			{
				CollectStateLinks(Node.Instance.GetScriptStruct(), Node.Instance.GetMutableMemory(), Links);
			}
			if (Node.InstanceObject)
			{
				CollectStateLinks(Node.InstanceObject->GetClass(), Node.InstanceObject, Links);
			}
		}
	}

	void FixStateAfterDuplication(UStateTreeState* State, UStateTreeState* NewParentState, TMap<FGuid, FGuid>& IDsMap, TArray<FStateTreeStateLink*>& Links, TArray<UStateTreeState*>& NewStates)
	{
		State->Modify();

		const FGuid NewStateID = FGuid::NewGuid();
		IDsMap.Emplace(State->ID, NewStateID);
		State->ID = NewStateID;
		
		const FGuid NewParametersID = FGuid::NewGuid();
		IDsMap.Emplace(State->Parameters.ID, NewParametersID);
		State->Parameters.ID = NewParametersID;
		
		State->Parent = NewParentState;
		NewStates.Add(State);
		
		if (State->Type == EStateTreeStateType::Linked)
		{
			Links.Emplace(&State->LinkedSubtree);
		}

		FixNodesAfterDuplication(TArrayView<FStateTreeEditorNode>(&State->SingleTask, 1), IDsMap, Links);
		FixNodesAfterDuplication(State->Tasks, IDsMap, Links);
		FixNodesAfterDuplication(State->EnterConditions, IDsMap, Links);

		for (FStateTreeTransition& Transition : State->Transitions)
		{
			// Transition Ids are not used by nodes so no need to add to 'IDsMap'
			Transition.ID = FGuid::NewGuid();

			FixNodesAfterDuplication(Transition.Conditions, IDsMap, Links);
			Links.Emplace(&Transition.State);
		}

		for (UStateTreeState* Child : State->Children)
		{
			FixStateAfterDuplication(Child, State, IDsMap, Links, NewStates);
		}
	}

	// Removes states from the array which are children of any other state.
	void RemoveContainedChildren(TArray<UStateTreeState*>& States)
	{
		TSet<UStateTreeState*> UniqueStates;
		for (UStateTreeState* State : States)
		{
			UniqueStates.Add(State);
		}

		for (int32 i = 0; i < States.Num(); )
		{
			UStateTreeState* State = States[i];

			// Walk up the parent state sand if the current state
			// exists in any of them, remove it.
			UStateTreeState* StateParent = State->Parent;
			bool bShouldRemove = false;
			while (StateParent)
			{
				if (UniqueStates.Contains(StateParent))
				{
					bShouldRemove = true;
					break;
				}
				StateParent = StateParent->Parent;
			}

			if (bShouldRemove)
			{
				States.RemoveAt(i);
			}
			else
			{
				i++;
			}
		}
	}

	// Returns true if the state is child of parent state.
	bool IsChildOf(const UStateTreeState* ParentState, const UStateTreeState* State)
	{
		for (const UStateTreeState* Child : ParentState->Children)
		{
			if (Child == State)
			{
				return true;
			}
			if (IsChildOf(Child, State))
			{
				return true;
			}
		}
		return false;
	}

};


FStateTreeViewModel::FStateTreeViewModel()
	: TreeDataWeak(nullptr)
#if WITH_STATETREE_DEBUGGER
	, Debugger(MakeShareable(new FStateTreeDebugger))
#endif // WITH_STATETREE_DEBUGGER
{
}

FStateTreeViewModel::~FStateTreeViewModel()
{
	GEditor->UnregisterForUndo(this);

	UE::StateTree::Delegates::OnIdentifierChanged.RemoveAll(this);
}

void FStateTreeViewModel::Init(UStateTreeEditorData* InTreeData)
{
	TreeDataWeak = InTreeData;

	GEditor->RegisterForUndo(this);

	UE::StateTree::Delegates::OnIdentifierChanged.AddSP(this, &FStateTreeViewModel::HandleIdentifierChanged);
	
#if WITH_STATETREE_DEBUGGER
	UE::StateTree::Delegates::OnBreakpointsChanged.AddSP(this, &FStateTreeViewModel::HandleBreakpointsChanged);
	UE::StateTree::Delegates::OnPostCompile.AddSP(this, &FStateTreeViewModel::HandlePostCompile);

	Debugger->SetAsset(GetStateTree());
	BindToDebuggerDelegates();
	RefreshDebuggerBreakpoints();
#endif // WITH_STATETREE_DEBUGGER	
}

const UStateTree* FStateTreeViewModel::GetStateTree() const
{
	if (const UStateTreeEditorData* TreeData = TreeDataWeak.Get())
	{
		return TreeData->GetTypedOuter<UStateTree>();
	}

	return nullptr;
}

void FStateTreeViewModel::HandleIdentifierChanged(const UStateTree& StateTree) const
{
	if (GetStateTree() == &StateTree)
	{
		OnAssetChanged.Broadcast();
	}
}

#if WITH_STATETREE_DEBUGGER
void FStateTreeViewModel::HandleBreakpointsChanged(const UStateTree& StateTree)
{
	if (GetStateTree() == &StateTree)
	{
		RefreshDebuggerBreakpoints();
	}
}

void FStateTreeViewModel::HandlePostCompile(const UStateTree& StateTree)
{
	if (GetStateTree() == &StateTree)
	{
		RefreshDebuggerBreakpoints();
	}
}

void FStateTreeViewModel::RefreshDebuggerBreakpoints()
{
	const UStateTree* StateTree = GetStateTree();
	const UStateTreeEditorData* TreeData = TreeDataWeak.Get();
	if (StateTree != nullptr && TreeData != nullptr)
	{
		Debugger->ClearAllBreakpoints();

		for (const FStateTreeEditorBreakpoint& Breakpoint : TreeData->Breakpoints)
		{
			// Test if the ID is associated to a task
			const FStateTreeIndex16 Index = StateTree->GetNodeIndexFromId(Breakpoint.ID);
			if (Index.IsValid())
			{
				Debugger->SetTaskBreakpoint(Index, Breakpoint.BreakpointType);
			}
			else
			{
				// Then test if the ID is associated to a State
				FStateTreeStateHandle StateHandle = StateTree->GetStateHandleFromId(Breakpoint.ID);
				if (StateHandle.IsValid())
				{
					Debugger->SetStateBreakpoint(StateHandle, Breakpoint.BreakpointType);
				}
				else
				{
					// Then test if the ID is associated to a transition
					const FStateTreeIndex16 TransitionIndex = StateTree->GetTransitionIndexFromId(Breakpoint.ID);
					if (TransitionIndex.IsValid())
					{
						Debugger->SetTransitionBreakpoint(TransitionIndex, Breakpoint.BreakpointType);
					}
				}
			}
		}
	}
}

#endif // WITH_STATETREE_DEBUGGER

void FStateTreeViewModel::NotifyAssetChangedExternally() const
{
	OnAssetChanged.Broadcast();
}

void FStateTreeViewModel::NotifyStatesChangedExternally(const TSet<UStateTreeState*>& ChangedStates, const FPropertyChangedEvent& PropertyChangedEvent) const
{
	OnStatesChanged.Broadcast(ChangedStates, PropertyChangedEvent);
}

TArray<TObjectPtr<UStateTreeState>>* FStateTreeViewModel::GetSubTrees() const
{
	UStateTreeEditorData* TreeData = TreeDataWeak.Get();
	return TreeData != nullptr ? &TreeData->SubTrees : nullptr;
}

int32 FStateTreeViewModel::GetSubTreeCount() const
{
	UStateTreeEditorData* TreeData = TreeDataWeak.Get();
	return TreeData != nullptr ? TreeData->SubTrees.Num() : 0;
}

void FStateTreeViewModel::GetSubTrees(TArray<TWeakObjectPtr<UStateTreeState>>& OutSubtrees) const
{
	OutSubtrees.Reset();
	if (UStateTreeEditorData* TreeData = TreeDataWeak.Get())
	{
		for (UStateTreeState* Subtree : TreeData->SubTrees)
		{
			OutSubtrees.Add(Subtree);
		}
	}
}

void FStateTreeViewModel::PostUndo(bool bSuccess)
{
	// TODO: see if we can narrow this down.
	OnAssetChanged.Broadcast();
}

void FStateTreeViewModel::PostRedo(bool bSuccess)
{
	OnAssetChanged.Broadcast();
}

void FStateTreeViewModel::ClearSelection()
{
	SelectedStates.Reset();

	const TArray<TWeakObjectPtr<UStateTreeState>> SelectedStatesArr;
	OnSelectionChanged.Broadcast(SelectedStatesArr);
}

void FStateTreeViewModel::SetSelection(UStateTreeState* Selected)
{
	SelectedStates.Reset();

	SelectedStates.Add(Selected);

	TArray<TWeakObjectPtr<UStateTreeState>> SelectedStatesArr;
	SelectedStatesArr.Add(Selected);
	OnSelectionChanged.Broadcast(SelectedStatesArr);
}

void FStateTreeViewModel::SetSelection(const TArray<TWeakObjectPtr<UStateTreeState>>& InSelectedStates)
{
	SelectedStates.Reset();

	for (const TWeakObjectPtr<UStateTreeState>& State : InSelectedStates)
	{
		if (State.Get())
		{
			SelectedStates.Add(State);
		}
	}

	TArray<FGuid> SelectedTaskIDArr;
	OnSelectionChanged.Broadcast(InSelectedStates);
}

bool FStateTreeViewModel::IsSelected(const UStateTreeState* State) const
{
	const TWeakObjectPtr<UStateTreeState> WeakState = const_cast<UStateTreeState*>(State);
	return SelectedStates.Contains(WeakState);
}

bool FStateTreeViewModel::IsChildOfSelection(const UStateTreeState* State) const
{
	for (const TWeakObjectPtr<UStateTreeState>& WeakSelectedState : SelectedStates)
	{
		if (const UStateTreeState* SelectedState = Cast<UStateTreeState>(WeakSelectedState.Get()))
		{
			if (SelectedState == State)
			{
				return true;
			}
			
			if (UE::StateTree::Editor::IsChildOf(SelectedState, State))
			{
				return true;
			}
		}
	}
	return false;
}

void FStateTreeViewModel::GetSelectedStates(TArray<UStateTreeState*>& OutSelectedStates) const
{
	OutSelectedStates.Reset();
	for (const TWeakObjectPtr<UStateTreeState>& WeakState : SelectedStates)
	{
		if (UStateTreeState* State = WeakState.Get())
		{
			OutSelectedStates.Add(State);
		}
	}
}

void FStateTreeViewModel::GetSelectedStates(TArray<TWeakObjectPtr<UStateTreeState>>& OutSelectedStates) const
{
	OutSelectedStates.Reset();
	for (const TWeakObjectPtr<UStateTreeState>& WeakState : SelectedStates)
	{
		if (WeakState.Get())
		{
			OutSelectedStates.Add(WeakState);
		}
	}
}

bool FStateTreeViewModel::HasSelection() const
{
	return SelectedStates.Num() > 0;
}

void FStateTreeViewModel::GetPersistentExpandedStates(TSet<TWeakObjectPtr<UStateTreeState>>& OutExpandedStates)
{
	OutExpandedStates.Reset();
	if (UStateTreeEditorData* TreeData = TreeDataWeak.Get())
	{
		for (UStateTreeState* SubTree : TreeData->SubTrees)
		{
			GetExpandedStatesRecursive(SubTree, OutExpandedStates);
		}
	}
}

void FStateTreeViewModel::GetExpandedStatesRecursive(UStateTreeState* State, TSet<TWeakObjectPtr<UStateTreeState>>& OutExpandedStates)
{
	if (State->bExpanded)
	{
		OutExpandedStates.Add(State);
	}
	for (UStateTreeState* Child : State->Children)
	{
		GetExpandedStatesRecursive(Child, OutExpandedStates);
	}
}


void FStateTreeViewModel::SetPersistentExpandedStates(TSet<TWeakObjectPtr<UStateTreeState>>& InExpandedStates)
{
	UStateTreeEditorData* TreeData = TreeDataWeak.Get();
	if (TreeData == nullptr)
	{
		return;
	}

	TreeData->Modify();

	for (TWeakObjectPtr<UStateTreeState>& WeakState : InExpandedStates)
	{
		if (UStateTreeState* State = WeakState.Get())
		{
			State->bExpanded = true;
		}
	}
}


void FStateTreeViewModel::AddState(UStateTreeState* AfterState)
{
	UStateTreeEditorData* TreeData = TreeDataWeak.Get();
	if (TreeData == nullptr)
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("AddStateTransaction", "Add State"));

	UStateTreeState* NewState = NewObject<UStateTreeState>(TreeData, FName(), RF_Transactional);
	UStateTreeState* ParentState = nullptr;

	if (AfterState == nullptr)
	{
		// If no subtrees, add a subtree, or add to the root state.
		if (TreeData->SubTrees.IsEmpty())
		{
			TreeData->Modify();
			TreeData->SubTrees.Add(NewState);
		}
		else
		{
			UStateTreeState* RootState = TreeData->SubTrees[0];
			if (ensureMsgf(RootState, TEXT("%s: Root state is empty."), *GetNameSafe(TreeData->GetOuter())))
			{
				RootState->Modify();
				RootState->Children.Add(NewState);
				NewState->Parent = RootState;
				ParentState = RootState;
			}
		}
	}
	else
	{
		ParentState = AfterState->Parent;
		if (ParentState != nullptr)
		{
			ParentState->Modify();
		}
		else
		{
			TreeData->Modify();
		}

		TArray<TObjectPtr<UStateTreeState>>& ParentArray = ParentState ? ParentState->Children : TreeData->SubTrees;

		const int32 TargetIndex = ParentArray.Find(AfterState);
		if (TargetIndex != INDEX_NONE)
		{
			// Insert After
			ParentArray.Insert(NewState, TargetIndex + 1);
			NewState->Parent = ParentState;
		}
		else
		{
			// Fallback, should never happen.
			ensureMsgf(false, TEXT("%s: Failed to find specified target state %s on state %s while adding new state."), *GetNameSafe(TreeData->GetOuter()), *GetNameSafe(AfterState), *GetNameSafe(ParentState));
			ParentArray.Add(NewState);
			NewState->Parent = ParentState;
		}
	}

	OnStateAdded.Broadcast(ParentState, NewState);
}

void FStateTreeViewModel::AddChildState(UStateTreeState* ParentState)
{
	UStateTreeEditorData* TreeData = TreeDataWeak.Get();
	if (TreeData == nullptr || ParentState == nullptr)
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("AddChildStateTransaction", "Add Child State"));

	UStateTreeState* NewState = NewObject<UStateTreeState>(ParentState, FName(), RF_Transactional);

	ParentState->Modify();
	
	ParentState->Children.Add(NewState);
	NewState->Parent = ParentState;

	OnStateAdded.Broadcast(ParentState, NewState);
}

void FStateTreeViewModel::RenameState(UStateTreeState* State, FName NewName)
{
	if (State == nullptr)
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("RenameTransaction", "Rename"));
	State->Modify();
	State->Name = NewName;

	TSet<UStateTreeState*> AffectedStates;
	AffectedStates.Add(State);

	FProperty* NameProperty = FindFProperty<FProperty>(UStateTreeState::StaticClass(), GET_MEMBER_NAME_CHECKED(UStateTreeState, Name));
	FPropertyChangedEvent PropertyChangedEvent(NameProperty, EPropertyChangeType::ValueSet);
	OnStatesChanged.Broadcast(AffectedStates, PropertyChangedEvent);
}

void FStateTreeViewModel::RemoveSelectedStates()
{
	UStateTreeEditorData* TreeData = TreeDataWeak.Get();
	if (TreeData == nullptr)
	{
		return;
	}

	TArray<UStateTreeState*> States;
	GetSelectedStates(States);

	// Remove items whose parent also exists in the selection.
	UE::StateTree::Editor::RemoveContainedChildren(States);

	if (States.Num() > 0)
	{
		const FScopedTransaction Transaction(LOCTEXT("DeleteStateTransaction", "Delete State"));

		TSet<UStateTreeState*> AffectedParents;

		for (UStateTreeState* StateToRemove : States)
		{
			if (StateToRemove)
			{
				StateToRemove->Modify();

				UStateTreeState* ParentState = StateToRemove->Parent;
				if (ParentState != nullptr)
				{
					AffectedParents.Add(ParentState);
					ParentState->Modify();
				}
				else
				{
					AffectedParents.Add(nullptr);
					TreeData->Modify();
				}
				
				TArray<TObjectPtr<UStateTreeState>>& ArrayToRemoveFrom = ParentState ? ParentState->Children : TreeData->SubTrees;
				const int32 ItemIndex = ArrayToRemoveFrom.Find(StateToRemove);
				if (ItemIndex != INDEX_NONE)
				{
					ArrayToRemoveFrom.RemoveAt(ItemIndex);
					StateToRemove->Parent = nullptr;
				}

			}
		}

		OnStatesRemoved.Broadcast(AffectedParents);

		ClearSelection();
	}
}

void FStateTreeViewModel::CopySelectedStates()
{
	UStateTreeEditorData* TreeData = TreeDataWeak.Get();
	if (TreeData == nullptr)
	{
		return;
	}

	TArray<UStateTreeState*> States;
	GetSelectedStates(States);
	UE::StateTree::Editor::RemoveContainedChildren(States);
	
	FString ExportedText = UE::StateTree::Editor::ExportStatesToText(TreeData, States);
	
	FPlatformApplicationMisc::ClipboardCopy(*ExportedText);
}

bool FStateTreeViewModel::CanPasteStatesFromClipboard() const
{
	FString TextToImport;
	FPlatformApplicationMisc::ClipboardPaste(TextToImport);

	UE::StateTree::Editor::FStateTreeStateTextFactory Factory;
	return Factory.CanCreateObjectsFromText(TextToImport);
}

void FStateTreeViewModel::PasteStatesFromClipboard(UStateTreeState* AfterState)
{
	UStateTreeEditorData* TreeData = TreeDataWeak.Get();
	if (TreeData == nullptr)
	{
		return;
	}
	
	if (AfterState)
	{
		const int32 Index = AfterState->Parent ? AfterState->Parent->Children.Find(AfterState) : TreeData->SubTrees.Find(AfterState);
		if (Index != INDEX_NONE)
		{
			FString TextToImport;
			FPlatformApplicationMisc::ClipboardPaste(TextToImport);
			
			const FScopedTransaction Transaction(LOCTEXT("PasteStatesTransaction", "Paste State(s)"));
			PasteStatesAsChildrenFromText(TextToImport, AfterState->Parent, Index + 1);
		}
	}
}

void FStateTreeViewModel::PasteStatesAsChildrenFromClipboard(UStateTreeState* ParentState)
{
	UStateTreeEditorData* TreeData = TreeDataWeak.Get();
	if (TreeData == nullptr)
	{
		return;
	}
	
	FString TextToImport;
	FPlatformApplicationMisc::ClipboardPaste(TextToImport);

	const FScopedTransaction Transaction(LOCTEXT("PasteStatesTransaction", "Paste State(s)"));
	PasteStatesAsChildrenFromText(TextToImport, ParentState, INDEX_NONE);
}

void FStateTreeViewModel::PasteStatesAsChildrenFromText(const FString& TextToImport, UStateTreeState* ParentState, const int32 IndexToInsertAt)
{
	UStateTreeEditorData* TreeData = TreeDataWeak.Get();
	if (TreeData == nullptr)
	{
		return;
	}

	UObject* Outer = ParentState ? static_cast<UObject*>(ParentState) : static_cast<UObject*>(TreeData);
	Outer->Modify();

	UE::StateTree::Editor::FStateTreeStateTextFactory Factory;
	Factory.ProcessBuffer(Outer, RF_Transactional, TextToImport);

	TArray<TObjectPtr<UStateTreeState>>& ParentArray = ParentState ? ParentState->Children : TreeData->SubTrees;
	const int32 TargetIndex = (IndexToInsertAt == INDEX_NONE) ? ParentArray.Num() : IndexToInsertAt;
	ParentArray.Insert(Factory.States, TargetIndex);

	TArray<FStateTreeStateLink*> Links;
	TMap<FGuid, FGuid> IDsMap;
	TArray<UStateTreeState*> NewStates;

	for (UStateTreeState* State : Factory.States)
	{		
		UE::StateTree::Editor::FixStateAfterDuplication(State, ParentState, IDsMap, Links, NewStates);
	}

	// Copy property bindings for the duplicated states.
	if (Factory.ClipboardBindings)
	{
		for (const TPair<FGuid, FGuid>& Entry : IDsMap)
		{
			const FGuid OldTargetID = Entry.Key;
			const FGuid NewTargetID = Entry.Value;
			
			for (const FStateTreePropertyPathBinding& Binding : Factory.ClipboardBindings->Bindings)
			{
				if (Binding.GetTargetPath().GetStructID() == OldTargetID)
				{
					FStateTreePropertyPath TargetPath(Binding.GetTargetPath());
					TargetPath.SetStructID(NewTargetID);
					
					FStateTreePropertyPath SourcePath(Binding.GetSourcePath());
					if (const FGuid* NewSourceID = IDsMap.Find(Binding.GetSourcePath().GetStructID()))
					{
						SourcePath.SetStructID(*NewSourceID);
					}
					
					TreeData->GetPropertyEditorBindings()->AddPropertyBinding(SourcePath, TargetPath);
				}
			}
		}
	}

	// Patch IDs in state links.
	for (FStateTreeStateLink* Link : Links)
	{
		if (FGuid* NewID = IDsMap.Find(Link->ID))
		{
			Link->ID = *NewID;
		}
	}

	for (UStateTreeState* State : NewStates)
	{
		OnStateAdded.Broadcast(State->Parent, State);
	}
}

void FStateTreeViewModel::DuplicateSelectedStates()
{
	UStateTreeEditorData* TreeData = TreeDataWeak.Get();
	if (TreeData == nullptr)
	{
		return;
	}

	TArray<UStateTreeState*> States;
	GetSelectedStates(States);
	UE::StateTree::Editor::RemoveContainedChildren(States);

	if (States.IsEmpty())
	{
		return;
	}
	
	FString ExportedText = UE::StateTree::Editor::ExportStatesToText(TreeData, States);

	// Place duplicates after first selected state.
	UStateTreeState* AfterState = States[0];
	
	const int32 Index = AfterState->Parent ? AfterState->Parent->Children.Find(AfterState) : TreeData->SubTrees.Find(AfterState);
	if (Index != INDEX_NONE)
	{
		const FScopedTransaction Transaction(LOCTEXT("DuplicateStatesTransaction", "Duplicate State(s)"));
		PasteStatesAsChildrenFromText(ExportedText, AfterState->Parent, Index + 1);
	}
}


void FStateTreeViewModel::MoveSelectedStatesBefore(UStateTreeState* TargetState)
{
	MoveSelectedStates(TargetState, FStateTreeViewModelInsert::Before);
}

void FStateTreeViewModel::MoveSelectedStatesAfter(UStateTreeState* TargetState)
{
	MoveSelectedStates(TargetState, FStateTreeViewModelInsert::After);
}

void FStateTreeViewModel::MoveSelectedStatesInto(UStateTreeState* TargetState)
{
	MoveSelectedStates(TargetState, FStateTreeViewModelInsert::Into);
}

bool FStateTreeViewModel::CanEnableStates() const
{
	TArray<UStateTreeState*> States;
	GetSelectedStates(States);

	for (const UStateTreeState* State : States)
	{
		// Stop if at least one state can be enabled
		if (State->bEnabled == false)
		{
			return true;
		}
	}

	return false;
}

bool FStateTreeViewModel::CanDisableStates() const
{
	TArray<UStateTreeState*> States;
	GetSelectedStates(States);

	for (const UStateTreeState* State : States)
	{
		// Stop if at least one state can be disabled
		if (State->bEnabled)
		{
			return true;
		}
	}

	return false;
}

void FStateTreeViewModel::SetSelectedStatesEnabled(const bool bEnable)
{
	TArray<UStateTreeState*> States;
	GetSelectedStates(States);

	if (States.Num() > 0)
	{
		const FScopedTransaction Transaction(LOCTEXT("SetStatesEnabledTransaction", "Set State Enabled"));

		for (UStateTreeState* State : States)
		{
			State->Modify();
			State->bEnabled = bEnable;
		}

		OnAssetChanged.Broadcast();
	}
}

void FStateTreeViewModel::MoveSelectedStates(UStateTreeState* TargetState, const FStateTreeViewModelInsert RelativeLocation)
{
	UStateTreeEditorData* TreeData = TreeDataWeak.Get();
	if (TreeData == nullptr || TargetState == nullptr)
	{
		return;
	}

	TArray<UStateTreeState*> States;
	GetSelectedStates(States);

	// Remove child items whose parent also exists in the selection.
	UE::StateTree::Editor::RemoveContainedChildren(States);

	// Remove states which contain target state as child.
	States.RemoveAll([TargetState](const UStateTreeState* State)
	{
		return UE::StateTree::Editor::IsChildOf(State, TargetState);
	});

	if (States.Num() > 0 && TargetState != nullptr)
	{
		const FScopedTransaction Transaction(LOCTEXT("MoveTransaction", "Move"));

		TSet<UStateTreeState*> AffectedParents;
		TSet<UStateTreeState*> AffectedStates;

		UStateTreeState* TargetParent = TargetState->Parent;
		if (RelativeLocation == FStateTreeViewModelInsert::Into)
		{
			AffectedParents.Add(TargetState);
		}
		else
		{
			AffectedParents.Add(TargetParent);
		}
		
		for (int32 i = States.Num() - 1; i >= 0; i--)
		{
			if (UStateTreeState* State = States[i])
			{
				State->Modify();
				if (State->Parent)
				{
					AffectedParents.Add(State->Parent);
				}
			}
		}

		if (RelativeLocation == FStateTreeViewModelInsert::Into)
		{
			// Move into
			TargetState->Modify();
		}
		
		for (UStateTreeState* Parent : AffectedParents)
		{
			if (Parent)
			{
				Parent->Modify();
			}
			else
			{
				TreeData->Modify();
			}
		}

		// Add in reverse order to keep the original order.
		for (int32 i = States.Num() - 1; i >= 0; i--)
		{
			if (UStateTreeState* SelectedState = States[i])
			{
				AffectedStates.Add(SelectedState);

				UStateTreeState* SelectedParent = SelectedState->Parent;

				// Remove from current parent
				TArray<TObjectPtr<UStateTreeState>>& ArrayToRemoveFrom = SelectedParent ? SelectedParent->Children : TreeData->SubTrees;
				const int32 ItemIndex = ArrayToRemoveFrom.Find(SelectedState);
				if (ItemIndex != INDEX_NONE)
				{
					ArrayToRemoveFrom.RemoveAt(ItemIndex);
					SelectedState->Parent = nullptr;
				}

				// Insert to new parent
				if (RelativeLocation == FStateTreeViewModelInsert::Into)
				{
					// Into
					TargetState->Children.Insert(SelectedState, /*Index*/0);
					SelectedState->Parent = TargetState;
				}
				else
				{
					TArray<TObjectPtr<UStateTreeState>>& ArrayToMoveTo = TargetParent ? TargetParent->Children : TreeData->SubTrees;
					const int32 TargetIndex = ArrayToMoveTo.Find(TargetState);
					if (TargetIndex != INDEX_NONE)
					{
						if (RelativeLocation == FStateTreeViewModelInsert::Before)
						{
							// Before
							ArrayToMoveTo.Insert(SelectedState, TargetIndex);
							SelectedState->Parent = TargetParent;
						}
						else if (RelativeLocation == FStateTreeViewModelInsert::After)
						{
							// After
							ArrayToMoveTo.Insert(SelectedState, TargetIndex + 1);
							SelectedState->Parent = TargetParent;
						}
					}
					else
					{
						// Fallback, should never happen.
						ensureMsgf(false, TEXT("%s: Failed to find specified target state %s on state %s while moving a state."), *GetNameSafe(TreeData->GetOuter()), *GetNameSafe(TargetState), *GetNameSafe(SelectedParent));
						ArrayToMoveTo.Add(SelectedState);
						SelectedState->Parent = TargetParent;
					}
				}
			}
		}

		OnStatesMoved.Broadcast(AffectedParents, AffectedStates);

		TArray<TWeakObjectPtr<UStateTreeState>> WeakStates;
		for (UStateTreeState* State : States)
		{
			WeakStates.Add(State);
		}

		SetSelection(WeakStates);
	}
}


void FStateTreeViewModel::BindToDebuggerDelegates()
{
#if WITH_STATETREE_DEBUGGER
	Debugger->OnActiveStatesChanged.BindSPLambda(this, [this](const FStateTreeTraceActiveStates& NewActiveStates)
	{
		if (const UStateTree* OuterStateTree = GetStateTree())
		{
			for (const FStateTreeTraceActiveStates::FAssetActiveStates& AssetActiveStates : NewActiveStates.PerAssetStates)
			{
				// Only track states owned by the StateTree associated to the view model (skip linked assets)
				if (AssetActiveStates.WeakStateTree == OuterStateTree)
				{
					ActiveStates.Reset(AssetActiveStates.ActiveStates.Num());
					for (const FStateTreeStateHandle Handle : AssetActiveStates.ActiveStates)
					{
						ActiveStates.Add(OuterStateTree->GetStateIdFromHandle(Handle));
					}
				}
			}
		}
	});
#endif // WITH_STATETREE_DEBUGGER
}

bool FStateTreeViewModel::IsStateActiveInDebugger(const UStateTreeState& State) const
{
#if WITH_STATETREE_DEBUGGER
	return ActiveStates.Contains(State.ID);
#else
	return false;
#endif // WITH_STATETREE_DEBUGGER
}

#undef LOCTEXT_NAMESPACE
