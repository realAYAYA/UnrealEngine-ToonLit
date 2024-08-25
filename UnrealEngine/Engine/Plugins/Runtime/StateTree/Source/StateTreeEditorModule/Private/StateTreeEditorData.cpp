// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeEditorData.h"
#include "StateTree.h"
#include "StateTreeConditionBase.h"
#include "StateTreeDelegates.h"
#include "StateTreeEvaluatorBase.h"
#include "StateTreeTaskBase.h"
#include "Algo/LevenshteinDistance.h"
#include "StateTreeEditorModule.h"
#include "StateTreePropertyHelpers.h"

#if WITH_EDITOR
#include "Engine/UserDefinedStruct.h"
#include "StructUtilsDelegates.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(StateTreeEditorData)

UStateTreeEditorData::UStateTreeEditorData()
{
	FStateTreeEditorColor DefaultColor;
	DefaultColor.ColorRef = FStateTreeEditorColorRef();
	DefaultColor.Color = FLinearColor(FColor(31, 151, 167));
	DefaultColor.DisplayName = TEXT("Default Color");

	Colors.Add(MoveTemp(DefaultColor));
}

void UStateTreeEditorData::PostInitProperties()
{
	Super::PostInitProperties();

	RootParameters.ID = FGuid::NewGuid();

#if WITH_EDITOR
	OnObjectsReinstancedHandle = FCoreUObjectDelegates::OnObjectsReinstanced.AddUObject(this, &UStateTreeEditorData::OnObjectsReinstanced);
	OnUserDefinedStructReinstancedHandle = UE::StructUtils::Delegates::OnUserDefinedStructReinstanced.AddUObject(this, &UStateTreeEditorData::OnUserDefinedStructReinstanced);
	OnParametersChangedHandle = UE::StateTree::Delegates::OnParametersChanged.AddUObject(this, &UStateTreeEditorData::OnParametersChanged);
#endif
}

#if WITH_EDITOR

void UStateTreeEditorData::BeginDestroy()
{
	if (OnObjectsReinstancedHandle.IsValid())
	{
		FCoreUObjectDelegates::OnObjectsReinstanced.Remove(OnObjectsReinstancedHandle);
		OnObjectsReinstancedHandle.Reset();
	}
	if (OnUserDefinedStructReinstancedHandle.IsValid())
	{
		UE::StructUtils::Delegates::OnUserDefinedStructReinstanced.Remove(OnUserDefinedStructReinstancedHandle);
		OnUserDefinedStructReinstancedHandle.Reset();
	}
	if (OnParametersChangedHandle.IsValid())
	{
		UE::StateTree::Delegates::OnParametersChanged.Remove(OnParametersChangedHandle);
		OnParametersChangedHandle.Reset();
	}
	
	Super::BeginDestroy();
}

void UStateTreeEditorData::OnObjectsReinstanced(const FReplacementObjectMap& ObjectMap)
{
	if (ObjectMap.IsEmpty())
	{
		return;
	}
	
	TSet<const UStruct*> Structs;
	for (TMap<UObject*, UObject*>::TConstIterator It(ObjectMap); It; ++It)
	{
		if (const UObject* ObjectToBeReplaced = It->Value)
		{
			Structs.Add(ObjectToBeReplaced->GetClass());
		}
	}

	bool bShouldUpdate = false;
	VisitAllNodes([&Structs, &bShouldUpdate](const UStateTreeState* State, const FStateTreeBindableStructDesc& Desc, const FStateTreeDataView Value)
	{
		if (Structs.Contains(Value.GetStruct()))
		{
			bShouldUpdate = true;
			return EStateTreeVisitor::Break; 
		}
		return EStateTreeVisitor::Continue;
	});

	if (!bShouldUpdate)
	{
		bShouldUpdate = EditorBindings.ContainsAnyStruct(Structs);
	}
	
	if (bShouldUpdate)
	{
		UpdateBindingsInstanceStructs();
	}
}

void UStateTreeEditorData::OnUserDefinedStructReinstanced(const UUserDefinedStruct& UserDefinedStruct)
{
	TSet<const UStruct*> Structs;
	Structs.Add(&UserDefinedStruct);

	bool bShouldUpdate = false;
	VisitAllNodes([&Structs, &bShouldUpdate](const UStateTreeState* State, const FStateTreeBindableStructDesc& Desc, const FStateTreeDataView Value)
	{
		if (Structs.Contains(Value.GetStruct()))
		{
			bShouldUpdate = true;
			return EStateTreeVisitor::Break; 
		}
		return EStateTreeVisitor::Continue;
	});

	if (!bShouldUpdate)
	{
		bShouldUpdate = EditorBindings.ContainsAnyStruct(Structs);
	}
	
	if (bShouldUpdate)
	{
		UpdateBindingsInstanceStructs();
	}
}

void UStateTreeEditorData::OnParametersChanged(const UStateTree& StateTree)
{
	if (const UStateTree* OwnerStateTree = GetTypedOuter<UStateTree>())
	{
		if (OwnerStateTree == &StateTree)
		{
			UpdateBindingsInstanceStructs();
		}
	}
}


void UStateTreeEditorData::PostLoad()
{
	Super::PostLoad();

	// Ensure the schema and states have had their PostLoad() fixed applied as we may need them in the later calls (or StateTree compile which might be calling this).
	if (Schema)
	{
		Schema->ConditionalPostLoad();
	}
	VisitHierarchy([](UStateTreeState& State, UStateTreeState* ParentState) mutable 
	{
		State.ConditionalPostLoad();
		return EStateTreeVisitor::Continue;
	});

	ReparentStates();
	FixObjectNodes();
	FixDuplicateIDs();
	UpdateBindingsInstanceStructs();
}

void UStateTreeEditorData::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);

	FProperty* Property = PropertyChangedEvent.Property;
	FProperty* MemberProperty = nullptr;
	if (PropertyChangedEvent.PropertyChain.GetActiveMemberNode())
	{
		MemberProperty = PropertyChangedEvent.PropertyChain.GetActiveMemberNode()->GetValue();
	}

	if (MemberProperty && Property)
	{
		const UStateTree* StateTree = GetTypedOuter<UStateTree>();
		checkf(StateTree, TEXT("UStateTreeEditorData should only be allocated within a UStateTree"));
		
		const FName MemberName = MemberProperty->GetFName();
		if (MemberName == GET_MEMBER_NAME_CHECKED(UStateTreeEditorData, Schema))
		{			
			UE::StateTree::Delegates::OnSchemaChanged.Broadcast(*StateTree);
		}
		else if (MemberName == GET_MEMBER_NAME_CHECKED(UStateTreeEditorData, RootParameters))
		{
			UE::StateTree::Delegates::OnParametersChanged.Broadcast(*StateTree);
		}

		// Ensure unique ID on duplicated items.
		if (PropertyChangedEvent.ChangeType == EPropertyChangeType::Duplicate)
		{
			if (MemberName == GET_MEMBER_NAME_CHECKED(UStateTreeEditorData, Evaluators))
			{
				const int32 ArrayIndex = PropertyChangedEvent.GetArrayIndex(MemberProperty->GetFName().ToString());
				if (Evaluators.IsValidIndex(ArrayIndex))
				{
					if (FStateTreeEvaluatorBase* Eval = Evaluators[ArrayIndex].Node.GetMutablePtr<FStateTreeEvaluatorBase>())
					{
						Eval->Name = FName(Eval->Name.ToString() + TEXT(" Duplicate"));
					}
					
					const FGuid OldStructID = Evaluators[ArrayIndex].ID;
					Evaluators[ArrayIndex].ID = FGuid::NewGuid();
					EditorBindings.CopyBindings(OldStructID, Evaluators[ArrayIndex].ID);
				}
			}
			else if (MemberName == GET_MEMBER_NAME_CHECKED(UStateTreeEditorData, GlobalTasks))
			{
				const int32 ArrayIndex = PropertyChangedEvent.GetArrayIndex(MemberProperty->GetFName().ToString());
				if (GlobalTasks.IsValidIndex(ArrayIndex))
				{
					if (FStateTreeTaskBase* Task = GlobalTasks[ArrayIndex].Node.GetMutablePtr<FStateTreeTaskBase>())
					{
						Task->Name = FName(Task->Name.ToString() + TEXT(" Duplicate"));
					}
					
					const FGuid OldStructID = GlobalTasks[ArrayIndex].ID;
					GlobalTasks[ArrayIndex].ID = FGuid::NewGuid();
					EditorBindings.CopyBindings(OldStructID, GlobalTasks[ArrayIndex].ID);
				}
			}
		}
		else if (PropertyChangedEvent.ChangeType == EPropertyChangeType::ArrayRemove)
		{
			if (MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UStateTreeEditorData, Evaluators)
				|| MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UStateTreeEditorData, GlobalTasks))
			{
				TMap<FGuid, const FStateTreeDataView> AllStructValues;
				GetAllStructValues(AllStructValues);
				Modify();
				EditorBindings.RemoveUnusedBindings(AllStructValues);
			}
		}

		// Notify that the global data changed (will need to update binding widgets, etc)
		if (MemberName == GET_MEMBER_NAME_CHECKED(UStateTreeEditorData, Evaluators)
			|| MemberName == GET_MEMBER_NAME_CHECKED(UStateTreeEditorData, GlobalTasks))
		{
			UE::StateTree::Delegates::OnGlobalDataChanged.Broadcast(*StateTree);
		}

	}

	UE::StateTree::PropertyHelpers::DispatchPostEditToNodes(*this, PropertyChangedEvent);
}
#endif // WITH_EDITOR

void UStateTreeEditorData::GetAccessibleStructs(const FGuid TargetStructID, TArray<FStateTreeBindableStructDesc>& OutStructDescs) const
{
	// Find the states that are updated before the current state.
	TArray<const UStateTreeState*> Path;
	const UStateTreeState* State = GetStateByStructID(TargetStructID);
	while (State != nullptr)
	{
		Path.Insert(State, 0);

		// Stop at subtree root.
		if (State->Type == EStateTreeStateType::Subtree)
		{
			break;
		}

		State = State->Parent;
	}
	
	GetAccessibleStructs(Path, TargetStructID, OutStructDescs);
}

void UStateTreeEditorData::GetAccessibleStructs(const TConstArrayView<const UStateTreeState*> Path, const FGuid TargetStructID, TArray<FStateTreeBindableStructDesc>& OutStructDescs) const
{
	const UStateTree* StateTree = GetTypedOuter<UStateTree>();
	checkf(StateTree, TEXT("UStateTreeEditorData should only be allocated within a UStateTree"));


	EStateTreeVisitor BaseProgress = VisitGlobalNodes([&OutStructDescs, TargetStructID]
		(const UStateTreeState* State, const FStateTreeBindableStructDesc& Desc, const FStateTreeDataView Value)
	{
		if (Desc.ID == TargetStructID)
		{
			return EStateTreeVisitor::Break;
		}
		
		OutStructDescs.Add(Desc);
		
		return EStateTreeVisitor::Continue;
	});


	if (BaseProgress == EStateTreeVisitor::Continue)
	{
		TArray<FStateTreeBindableStructDesc> TaskDescs;

		for (const UStateTreeState* State : Path)
		{
			if (State == nullptr)
			{
				continue;
			}
			
			const EStateTreeVisitor StateProgress = VisitStateNodes(*State, [&OutStructDescs, &TaskDescs, TargetStructID]
				(const UStateTreeState* State, const FStateTreeBindableStructDesc& Desc, const FStateTreeDataView Value)
				{
					// Stop iterating as soon as we find the target node.
					if (Desc.ID == TargetStructID)
					{
						OutStructDescs.Append(TaskDescs);
						return EStateTreeVisitor::Break;
					}

					// Not at target yet, collect all bindable source accessible so far.
					if (Desc.DataSource == EStateTreeBindableStructSource::Task
						|| Desc.DataSource == EStateTreeBindableStructSource::State)
					{
						TaskDescs.Add(Desc);
					}
							
					return EStateTreeVisitor::Continue;
				});
			
			if (StateProgress == EStateTreeVisitor::Break)
			{
				break;
			}
		}
	}
	
	OutStructDescs.StableSort([](const FStateTreeBindableStructDesc& A, const FStateTreeBindableStructDesc& B)
	{
		return (uint8)A.DataSource < (uint8)B.DataSource;
	});
}

FStateTreeBindableStructDesc UStateTreeEditorData::FindContextData(const UStruct* ObjectType, const FString ObjectNameHint) const
{
	if (Schema == nullptr)
	{
		return FStateTreeBindableStructDesc();
	}

	// Find candidates based on type.
	TArray<FStateTreeBindableStructDesc> Candidates;
	for (const FStateTreeExternalDataDesc& Desc : Schema->GetContextDataDescs())
	{
		if (Desc.Struct->IsChildOf(ObjectType))
		{
			Candidates.Emplace(Desc.Name, Desc.Struct, FStateTreeDataHandle(), EStateTreeBindableStructSource::Context, Desc.ID);
		}
	}

	// Handle trivial cases.
	if (Candidates.IsEmpty())
	{
		return FStateTreeBindableStructDesc();
	}

	if (Candidates.Num() == 1)
	{
		return Candidates[0];
	}
	
	check(!Candidates.IsEmpty());
	
	// Multiple candidates, pick one that is closest match based on name.
	auto CalculateScore = [](const FString& Name, const FString& CandidateName)
	{
		if (CandidateName.IsEmpty())
		{
			return 1.0f;
		}
		const float WorstCase = static_cast<float>(Name.Len() + CandidateName.Len());
		return 1.0f - (Algo::LevenshteinDistance(Name, CandidateName) / WorstCase);
	};
	
	const FString ObjectNameLowerCase = ObjectNameHint.ToLower();
	
	int32 HighestScoreIndex = 0;
	float HighestScore = CalculateScore(ObjectNameLowerCase, Candidates[0].Name.ToString().ToLower());
	
	for (int32 Index = 1; Index < Candidates.Num(); Index++)
	{
		const float Score = CalculateScore(ObjectNameLowerCase, Candidates[Index].Name.ToString().ToLower());
		if (Score > HighestScore)
		{
			HighestScore = Score;
			HighestScoreIndex = Index;
		}
	}
	
	return Candidates[HighestScoreIndex];
}

bool UStateTreeEditorData::GetStructByID(const FGuid StructID, FStateTreeBindableStructDesc& OutStructDesc) const
{
	VisitAllNodes([&OutStructDesc, StructID](const UStateTreeState* State, const FStateTreeBindableStructDesc& Desc, const FStateTreeDataView Value)
	{
		if (Desc.ID == StructID)
		{
			OutStructDesc = Desc;
			return EStateTreeVisitor::Break;
		}
		return EStateTreeVisitor::Continue;
	});
	
	return OutStructDesc.IsValid();
}

bool UStateTreeEditorData::GetDataViewByID(const FGuid StructID, FStateTreeDataView& OutDataView) const
{
	bool bFound = false;
	VisitAllNodes([&OutDataView, &bFound, StructID](const UStateTreeState* State, const FStateTreeBindableStructDesc& Desc, const FStateTreeDataView Value)
	{
		if (Desc.ID == StructID)
		{
			bFound = true;
			OutDataView = Value;
			return EStateTreeVisitor::Break;
		}
		return EStateTreeVisitor::Continue;
	});

	return bFound;
}

const UStateTreeState* UStateTreeEditorData::GetStateByStructID(const FGuid TargetStructID) const
{
	const UStateTreeState* Result = nullptr;

	VisitHierarchyNodes([&Result, TargetStructID](const UStateTreeState* State, const FStateTreeBindableStructDesc& Desc, const FStateTreeDataView Value)
		{
			if (Desc.ID == TargetStructID)
			{
				Result = State;
				return EStateTreeVisitor::Break;
			}
			return EStateTreeVisitor::Continue;
			
		});

	return Result;
}

const UStateTreeState* UStateTreeEditorData::GetStateByID(const FGuid StateID) const
{
	return const_cast<UStateTreeEditorData*>(this)->GetMutableStateByID(StateID);
}

UStateTreeState* UStateTreeEditorData::GetMutableStateByID(const FGuid StateID)
{
	UStateTreeState* Result = nullptr;
	
	VisitHierarchy([&Result, &StateID](UStateTreeState& State, UStateTreeState* /*ParentState*/)
	{
		if (State.ID == StateID)
		{
			Result = &State;
			return EStateTreeVisitor::Break;
		}

		return EStateTreeVisitor::Continue;
	});

	return Result;
}

void UStateTreeEditorData::GetAllStructIDs(TMap<FGuid, const UStruct*>& AllStructs) const
{
	AllStructs.Reset();

	const UStateTree* StateTree = GetTypedOuter<UStateTree>();
	checkf(StateTree, TEXT("UStateTreeEditorData should only be allocated within a UStateTree"));

	VisitAllNodes([&AllStructs](const UStateTreeState* State, const FStateTreeBindableStructDesc& Desc, const FStateTreeDataView Value)
		{
			AllStructs.Emplace(Desc.ID, Desc.Struct);
			return EStateTreeVisitor::Continue;
		});
}

void UStateTreeEditorData::GetAllStructValues(TMap<FGuid, const FStateTreeDataView>& AllValues) const
{
	AllValues.Reset();

	const UStateTree* StateTree = GetTypedOuter<UStateTree>();
	checkf(StateTree, TEXT("UStateTreeEditorData should only be allocated within a UStateTree"));

	VisitAllNodes([&AllValues](const UStateTreeState* State, const FStateTreeBindableStructDesc& Desc, const FStateTreeDataView Value)
		{
			AllValues.Emplace(Desc.ID, Value);
			return EStateTreeVisitor::Continue;
		});
}

void UStateTreeEditorData::ReparentStates()
{
	VisitHierarchy([TreeData = this](UStateTreeState& State, UStateTreeState* ParentState) mutable 
	{
		UObject* ExpectedOuter = ParentState ? Cast<UObject>(ParentState) : Cast<UObject>(TreeData);
		if (State.GetOuter() != ExpectedOuter)
		{
			UE_LOG(LogStateTreeEditor, Log, TEXT("%s: Fixing outer on state %s."), *TreeData->GetFullName(), *GetNameSafe(&State));
			State.Rename(nullptr, ExpectedOuter, REN_DontCreateRedirectors | REN_DoNotDirty | REN_ForceNoResetLoaders);
		}
		
		State.Parent = ParentState;
		
		return EStateTreeVisitor::Continue;
	});
}

void UStateTreeEditorData::FixObjectInstance(TSet<UObject*>& SeenObjects, UObject& Outer, FStateTreeEditorNode& Node)
{
	if (Node.InstanceObject)
	{
		// Found a duplicate reference to an object, make unique copy.
		if (SeenObjects.Contains(Node.InstanceObject))
		{
			UE_LOG(LogStateTreeEditor, Log, TEXT("%s: Making duplicate node instance %s unique."), *GetFullName(), *GetNameSafe(Node.InstanceObject));
			Node.InstanceObject = DuplicateObject(Node.InstanceObject, &Outer);
		}
		else
		{
			// Make sure the instance object is property outered.
			if (Node.InstanceObject->GetOuter() != &Outer)
			{
				UE_LOG(LogStateTreeEditor, Log, TEXT("%s: Fixing outer on node instance %s."), *GetFullName(), *GetNameSafe(Node.InstanceObject));
				Node.InstanceObject->Rename(nullptr, &Outer, REN_DontCreateRedirectors | REN_DoNotDirty | REN_ForceNoResetLoaders);
			}
		}
		SeenObjects.Add(Node.InstanceObject);
	}
};

void UStateTreeEditorData::FixObjectNodes()
{
	// Older version of State Trees had all instances outered to the editor data. This causes issues with State copy/paste.
	// Instance data does not get duplicated but the copied state will reference the object on the source state instead.
	//
	// Ensure that all node objects are parented to their states, and make duplicated instances unique.

	TSet<UObject*> SeenObjects;
	
	VisitHierarchy([&SeenObjects, TreeData = this](UStateTreeState& State, UStateTreeState* ParentState) mutable 
	{

		// Enter conditions
		for (FStateTreeEditorNode& Node : State.EnterConditions)
		{
			TreeData->FixObjectInstance(SeenObjects, State, Node);
		}
		
		// Tasks
		for (FStateTreeEditorNode& Node : State.Tasks)
		{
			TreeData->FixObjectInstance(SeenObjects, State, Node);
		}

		TreeData->FixObjectInstance(SeenObjects, State, State.SingleTask);


		// Transitions
		for (FStateTreeTransition& Transition : State.Transitions)
		{
			for (FStateTreeEditorNode& Node : Transition.Conditions)
			{
				TreeData->FixObjectInstance(SeenObjects, State, Node);
			}
		}
		
		return EStateTreeVisitor::Continue;
	});

	for (FStateTreeEditorNode& Node : Evaluators)
	{
		FixObjectInstance(SeenObjects, *this, Node);
	}

	for (FStateTreeEditorNode& Node : GlobalTasks)
	{
		FixObjectInstance(SeenObjects, *this, Node);
	}
}

void UStateTreeEditorData::FixDuplicateIDs()
{
	// Around version 5.1-5.3 we had issue that copy/paste or some duplication methods could create nodes with duplicate IDs.
	// This code tries to fix that, it looks for duplicates, makes them unique, and duplicates the bindings when ID changes.
	TSet<FGuid> FoundNodeIDs;

	// Evaluators
	for (int32 Index = 0; Index < Evaluators.Num(); Index++)
	{
		FStateTreeEditorNode& Node = Evaluators[Index];
		if (const FStateTreeEvaluatorBase* Evaluator = Node.Node.GetPtr<FStateTreeEvaluatorBase>())
		{
			const FGuid OldID = Node.ID; 
			if (FoundNodeIDs.Contains(Node.ID))
			{
				Node.ID = UE::StateTree::PropertyHelpers::MakeDeterministicID(*this, TEXT("Evaluators"), Index);
				
				UE_LOG(LogStateTreeEditor, Log, TEXT("%s: Found Evaluator '%s' with duplicate ID, changing ID:%s to ID:%s."),
					*GetFullName(), *Node.GetName().ToString(), *OldID.ToString(), *Node.ID.ToString());
				EditorBindings.CopyBindings(OldID, Node.ID);
			}
			FoundNodeIDs.Add(Node.ID);
		}
	}
	
	// Global Tasks
	for (int32 Index = 0; Index < GlobalTasks.Num(); Index++)
	{
		FStateTreeEditorNode& Node = GlobalTasks[Index];
		if (const FStateTreeTaskBase* Task = Node.Node.GetPtr<FStateTreeTaskBase>())
		{
			const FGuid OldID = Node.ID; 
			if (FoundNodeIDs.Contains(Node.ID))
			{
				Node.ID = UE::StateTree::PropertyHelpers::MakeDeterministicID(*this, TEXT("GlobalTasks"), Index);
				
				UE_LOG(LogStateTreeEditor, Log, TEXT("%s: Found GlobalTask '%s' with duplicate ID, changing ID:%s to ID:%s."),
					*GetFullName(), *Node.GetName().ToString(), *OldID.ToString(), *Node.ID.ToString());
				EditorBindings.CopyBindings(OldID, Node.ID);
			}
			FoundNodeIDs.Add(Node.ID);
		}
	}
	
	VisitHierarchy([&FoundNodeIDs, &EditorBindings = EditorBindings, &Self = *this](UStateTreeState& State, UStateTreeState* ParentState)
	{
		// Enter conditions
		for (int32 Index = 0; Index < State.EnterConditions.Num(); Index++)
		{
			FStateTreeEditorNode& Node = State.EnterConditions[Index];
			if (const FStateTreeConditionBase* Cond = Node.Node.GetPtr<FStateTreeConditionBase>())
			{
				const FGuid OldID = Node.ID;
				
				bool bIsAlreadyInSet = false;
				FoundNodeIDs.Add(Node.ID, &bIsAlreadyInSet);
				if (bIsAlreadyInSet)
				{
					Node.ID = UE::StateTree::PropertyHelpers::MakeDeterministicID(State, TEXT("EnterConditions"), Index);
					
					UE_LOG(LogStateTreeEditor, Log, TEXT("%s: Found Enter Condition '%s' with duplicate ID on state '%s', changing ID:%s to ID:%s."),
						*Self.GetFullName(), *Node.GetName().ToString(), *GetNameSafe(&State), *OldID.ToString(), *Node.ID.ToString());
					EditorBindings.CopyBindings(OldID, Node.ID);
				}
			}
		}

		// Tasks
		for (int32 Index = 0; Index < State.Tasks.Num(); Index++)
		{
			FStateTreeEditorNode& Node = State.Tasks[Index];
			if (const FStateTreeTaskBase* Task = Node.Node.GetPtr<FStateTreeTaskBase>())
			{
				const FGuid OldID = Node.ID;
				
				bool bIsAlreadyInSet = false;
				FoundNodeIDs.Add(Node.ID, &bIsAlreadyInSet);
				if (bIsAlreadyInSet)
				{
					Node.ID = UE::StateTree::PropertyHelpers::MakeDeterministicID(State, TEXT("Tasks"), Index);

					UE_LOG(LogStateTreeEditor, Log, TEXT("%s: Found Task '%s' with duplicate ID on state '%s', changing ID:%s to ID:%s."),
						*Self.GetFullName(), *Node.GetName().ToString(), *GetNameSafe(&State), *OldID.ToString(), *Node.ID.ToString());
					EditorBindings.CopyBindings(OldID, Node.ID);
				}
			}
		}

		if (FStateTreeTaskBase* Task = State.SingleTask.Node.GetMutablePtr<FStateTreeTaskBase>())
		{
			const FGuid OldID = State.SingleTask.ID;

			bool bIsAlreadyInSet = false;
			FoundNodeIDs.Add(State.SingleTask.ID, &bIsAlreadyInSet);
			if (bIsAlreadyInSet)
			{
				State.SingleTask.ID = UE::StateTree::PropertyHelpers::MakeDeterministicID(State, TEXT("SingleTask"), 0);

				UE_LOG(LogStateTreeEditor, Log, TEXT("%s: Found enter condition '%s' with duplicate ID on state '%s', changing ID:%s to ID:%s."),
					*Self.GetFullName(), *State.SingleTask.GetName().ToString(), *GetNameSafe(&State), *OldID.ToString(), *State.SingleTask.ID.ToString());
				EditorBindings.CopyBindings(OldID, State.SingleTask.ID);
			}
		}

		// Transitions
		for (int32 TransitionIndex = 0; TransitionIndex < State.Transitions.Num(); TransitionIndex++)
		{
			FStateTreeTransition& Transition = State.Transitions[TransitionIndex];
			for (int32 Index = 0; Index < Transition.Conditions.Num(); Index++)
			{
				FStateTreeEditorNode& Node = Transition.Conditions[Index];
				if (const FStateTreeConditionBase* Cond = Node.Node.GetPtr<FStateTreeConditionBase>())
				{
					const FGuid OldID = Node.ID; 
					bool bIsAlreadyInSet = false;
					FoundNodeIDs.Add(Node.ID, &bIsAlreadyInSet);
					if (bIsAlreadyInSet)
					{
						Node.ID = UE::StateTree::PropertyHelpers::MakeDeterministicID(State, TEXT("TransitionConditions"), ((uint64)TransitionIndex << 32) | (uint64)Index);

						UE_LOG(LogStateTreeEditor, Log, TEXT("%s: Found transition condition '%s' with duplicate ID on state '%s', changing ID:%s to ID:%s."),
							*Self.GetFullName(), *Node.GetName().ToString(), *GetNameSafe(&State), *OldID.ToString(), *Node.ID.ToString());
						EditorBindings.CopyBindings(OldID, Node.ID);
					}
				}
			}
		}
		
		return EStateTreeVisitor::Continue;
	});

	// It is possible that the user has changed the node type so some of the bindings might not make sense anymore, clean them up.
	TMap<FGuid, const FStateTreeDataView> AllValues;
	GetAllStructValues(AllValues);
	EditorBindings.RemoveUnusedBindings(AllValues);
}

void UStateTreeEditorData::UpdateBindingsInstanceStructs()
{
	TMap<FGuid, const FStateTreeDataView> AllValues;
	GetAllStructValues(AllValues);
	for (FStateTreePropertyPathBinding& Binding : EditorBindings.GetMutableBindings())
	{
		if (AllValues.Contains(Binding.GetSourcePath().GetStructID()))
		{
			Binding.GetMutableSourcePath().UpdateSegmentsFromValue(AllValues[Binding.GetSourcePath().GetStructID()]);
		}

		if (AllValues.Contains(Binding.GetTargetPath().GetStructID()))
		{
			Binding.GetMutableTargetPath().UpdateSegmentsFromValue(AllValues[Binding.GetTargetPath().GetStructID()]);
		}
	}
}

EStateTreeVisitor UStateTreeEditorData::VisitStateNodes(const UStateTreeState& State, TFunctionRef<EStateTreeVisitor(const UStateTreeState* State, const FGuid& ID, const FName& Name, const EStateTreeNodeType NodeType, const UScriptStruct* NodeStruct, const UStruct* InstanceStruct)> InFunc) const
{
	bool bContinue = true;

	if (bContinue)
	{
		// Enter conditions
		for (const FStateTreeEditorNode& Node : State.EnterConditions)
		{
			if (const FStateTreeConditionBase* Cond = Node.Node.GetPtr<FStateTreeConditionBase>())
			{
				if (InFunc(&State, Node.ID, Node.Node.GetScriptStruct()->GetFName(), EStateTreeNodeType::EnterCondition, Node.Node.GetScriptStruct(), Cond->GetInstanceDataType()) == EStateTreeVisitor::Break)
				{
					bContinue = false;
					break;
				}
			}
		}
	}
	if (bContinue)
	{
		// Tasks
		for (const FStateTreeEditorNode& Node : State.Tasks)
		{
			if (const FStateTreeTaskBase* Task = Node.Node.GetPtr<FStateTreeTaskBase>())
			{
				if (InFunc(&State, Node.ID, Task->Name, EStateTreeNodeType::Task, Node.Node.GetScriptStruct(), Task->GetInstanceDataType()) == EStateTreeVisitor::Break)
				{
					bContinue = false;
					break;
				}
			}
		}
	}
	if (bContinue)
	{
		if (const FStateTreeTaskBase* Task = State.SingleTask.Node.GetPtr<FStateTreeTaskBase>())
		{
			if (InFunc(&State, State.SingleTask.ID, Task->Name, EStateTreeNodeType::Task, State.SingleTask.Node.GetScriptStruct(), Task->GetInstanceDataType()) == EStateTreeVisitor::Break)
			{
				bContinue = false;
			}
		}

	}
	if (bContinue)
	{
		// Transitions
		for (const FStateTreeTransition& Transition : State.Transitions)
		{
			for (const FStateTreeEditorNode& Node : Transition.Conditions)
			{
				if (const FStateTreeConditionBase* Cond = Node.Node.GetPtr<FStateTreeConditionBase>())
				{
					if (InFunc(&State, Node.ID, Node.Node.GetScriptStruct()->GetFName(), EStateTreeNodeType::TransitionCondition, Node.Node.GetScriptStruct(), Cond->GetInstanceDataType()) == EStateTreeVisitor::Break)
					{
						bContinue = false;
						break;
					}
				}
			}
		}
	}
	if (bContinue)
	{
		// Bindable state parameters
		if (State.Type != EStateTreeStateType::Subtree
			&& State.Parameters.Parameters.IsValid())
		{
			if (InFunc(&State, State.Parameters.ID, State.Name, EStateTreeNodeType::StateParameters, nullptr, State.Parameters.Parameters.GetPropertyBagStruct()) == EStateTreeVisitor::Break)
			{
				bContinue = false;
			}
		}
	}

	return bContinue ? EStateTreeVisitor::Continue : EStateTreeVisitor::Break;
}


EStateTreeVisitor UStateTreeEditorData::VisitStateNodes(const UStateTreeState& State, TFunctionRef<EStateTreeVisitor(const UStateTreeState* State, const FStateTreeBindableStructDesc& Desc, const FStateTreeDataView Value)> InFunc) const
{
	bool bContinue = true;

	if (bContinue)
	{
		// Bindable state parameters
		if (State.Parameters.Parameters.IsValid())
		{
			FStateTreeBindableStructDesc Desc;
			Desc.Struct = State.Parameters.Parameters.GetPropertyBagStruct();
			Desc.Name = State.Name;
			Desc.ID = State.Parameters.ID;
			Desc.DataSource = EStateTreeBindableStructSource::State;

			if (InFunc(&State, Desc, FStateTreeDataView(const_cast<FInstancedPropertyBag&>(State.Parameters.Parameters).GetMutableValue())) == EStateTreeVisitor::Break)
			{
				bContinue = false;
			}
		}
	}
	
	if (bContinue)
	{
		// Enter conditions
		for (const FStateTreeEditorNode& Node : State.EnterConditions)
		{
			if (const FStateTreeConditionBase* Cond = Node.Node.GetPtr<FStateTreeConditionBase>())
			{
				FStateTreeBindableStructDesc Desc;
				Desc.Struct = Cond->GetInstanceDataType();
				Desc.Name = Cond->Name;
				Desc.ID = Node.ID;
				Desc.DataSource = EStateTreeBindableStructSource::Condition;

				if (InFunc(&State, Desc, Node.GetInstance()) == EStateTreeVisitor::Break)
				{
					bContinue = false;
					break;
				}
			}
		}
	}
	if (bContinue)
	{
		// Tasks
		for (const FStateTreeEditorNode& Node : State.Tasks)
		{
			if (const FStateTreeTaskBase* Task = Node.Node.GetPtr<FStateTreeTaskBase>())
			{
				FStateTreeBindableStructDesc Desc;
				Desc.Struct = Task->GetInstanceDataType();
				Desc.Name = Task->Name;
				Desc.ID = Node.ID;
				Desc.DataSource = EStateTreeBindableStructSource::Task;

				if (InFunc(&State, Desc, Node.GetInstance()) == EStateTreeVisitor::Break)
				{
					bContinue = false;
					break;
				}
			}
		}
	}
	if (bContinue)
	{
		if (const FStateTreeTaskBase* Task = State.SingleTask.Node.GetPtr<FStateTreeTaskBase>())
		{
			FStateTreeBindableStructDesc Desc;
			Desc.Struct = Task->GetInstanceDataType();
			Desc.Name = Task->Name;
			Desc.ID = State.SingleTask.ID;
			Desc.DataSource = EStateTreeBindableStructSource::Task;

			if (InFunc(&State, Desc, State.SingleTask.GetInstance()) == EStateTreeVisitor::Break)
			{
				bContinue = false;
			}
		}

	}
	if (bContinue)
	{
		// Transitions
		for (const FStateTreeTransition& Transition : State.Transitions)
		{
			for (const FStateTreeEditorNode& Node : Transition.Conditions)
			{
				if (const FStateTreeConditionBase* Cond = Node.Node.GetPtr<FStateTreeConditionBase>())
				{
					FStateTreeBindableStructDesc Desc;
					Desc.Struct = Cond->GetInstanceDataType();
					Desc.Name = Cond->Name;
					Desc.ID = Node.ID;
					Desc.DataSource = EStateTreeBindableStructSource::Condition;

					if (InFunc(&State, Desc, Node.GetInstance()) == EStateTreeVisitor::Break)
					{
						bContinue = false;
						break;
					}
				}
			}
		}
	}

	return bContinue ? EStateTreeVisitor::Continue : EStateTreeVisitor::Break;
}


EStateTreeVisitor UStateTreeEditorData::VisitHierarchy(TFunctionRef<EStateTreeVisitor(UStateTreeState& State, UStateTreeState* ParentState)> InFunc) const
{
	using FStatePair = TTuple<UStateTreeState*, UStateTreeState*>; 
	TArray<FStatePair> Stack;
	bool bContinue = true;

	for (UStateTreeState* SubTree : SubTrees)
	{
		if (!SubTree)
		{
			continue;
		}

		Stack.Add( FStatePair(nullptr, SubTree));

		while (!Stack.IsEmpty() && bContinue)
		{
			FStatePair Current = Stack[0];
			UStateTreeState* ParentState = Current.Get<0>();
			UStateTreeState* State = Current.Get<1>();
			check(State);

			Stack.RemoveAt(0);

			bContinue = InFunc(*State, ParentState) == EStateTreeVisitor::Continue;
			
			if (bContinue)
			{
				// Children
				for (UStateTreeState* ChildState : State->Children)
				{
					Stack.Add(FStatePair(State, ChildState));
				}
			}
		}
		
		if (!bContinue)
		{
			break;
		}
	}

	return EStateTreeVisitor::Continue;
}

void UStateTreeEditorData::VisitHierarchyNodes(TFunctionRef<EStateTreeVisitor(const UStateTreeState* State, const FGuid& ID, const FName& Name, const EStateTreeNodeType NodeType, const UScriptStruct* NodeStruct, const UStruct* InstanceStruct)> InFunc) const
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	VisitHierarchy([this, &InFunc](const UStateTreeState& State, UStateTreeState* /*ParentState*/)
	{
		return VisitStateNodes(State, InFunc);
	});
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

EStateTreeVisitor UStateTreeEditorData::VisitGlobalNodes(TFunctionRef<EStateTreeVisitor(const UStateTreeState* State, const FStateTreeBindableStructDesc& Desc, const FStateTreeDataView Value)> InFunc) const
{
	// Root parameters
	{
		FStateTreeBindableStructDesc Desc;
		Desc.Struct = RootParameters.Parameters.GetPropertyBagStruct();
		Desc.Name = FName(TEXT("Parameters"));
		Desc.ID = RootParameters.ID;
		Desc.DataSource = EStateTreeBindableStructSource::Parameter;
		
		if (InFunc(nullptr, Desc, FStateTreeDataView(const_cast<FInstancedPropertyBag&>(RootParameters.Parameters).GetMutableValue())) == EStateTreeVisitor::Break)
		{
			return EStateTreeVisitor::Break;
		}
	}

	// All named external data items declared by the schema
	if (Schema != nullptr)
	{
		for (const FStateTreeExternalDataDesc& ContextDesc : Schema->GetContextDataDescs())
		{
			FStateTreeBindableStructDesc Desc;
			Desc.Struct = ContextDesc.Struct;
			Desc.Name = ContextDesc.Name;
			Desc.ID = ContextDesc.ID;
			Desc.DataSource = EStateTreeBindableStructSource::Context;

			// We don't have value for the external objects, but return the type and null value so that users of GetAllStructValues() can use the type. 
			if (InFunc(nullptr, Desc, FStateTreeDataView(Desc.Struct, nullptr)) == EStateTreeVisitor::Break)
			{
				return EStateTreeVisitor::Break;
			}
		}
	}

	// Evaluators
	for (const FStateTreeEditorNode& Node : Evaluators)
	{
		if (const FStateTreeEvaluatorBase* Evaluator = Node.Node.GetPtr<FStateTreeEvaluatorBase>())
		{
			FStateTreeBindableStructDesc Desc;
			Desc.Struct = Evaluator->GetInstanceDataType();
			Desc.Name = Evaluator->Name;
			Desc.ID = Node.ID;
			Desc.DataSource = EStateTreeBindableStructSource::Evaluator;

			if (InFunc(nullptr, Desc, Node.GetInstance()) == EStateTreeVisitor::Break)
			{
				return EStateTreeVisitor::Break;
			}
		}
	}

	// Global tasks
	for (const FStateTreeEditorNode& Node : GlobalTasks)
	{
		if (const FStateTreeTaskBase* Task = Node.Node.GetPtr<FStateTreeTaskBase>())
		{
			FStateTreeBindableStructDesc Desc;
			Desc.Struct = Task->GetInstanceDataType();
			Desc.Name = Task->Name;
			Desc.ID = Node.ID;
			Desc.DataSource = EStateTreeBindableStructSource::GlobalTask;

			if (InFunc(nullptr, Desc, Node.GetInstance()) == EStateTreeVisitor::Break)
			{
				return EStateTreeVisitor::Break;
			}
		}
	}

	return  EStateTreeVisitor::Continue;
}

EStateTreeVisitor UStateTreeEditorData::VisitHierarchyNodes(TFunctionRef<EStateTreeVisitor(const UStateTreeState* State, const FStateTreeBindableStructDesc& Desc, const FStateTreeDataView Value)> InFunc) const
{
	return VisitHierarchy([this, &InFunc](const UStateTreeState& State, UStateTreeState* /*ParentState*/)
	{
		return VisitStateNodes(State, InFunc);
	});
}

EStateTreeVisitor UStateTreeEditorData::VisitAllNodes(TFunctionRef<EStateTreeVisitor(const UStateTreeState* State, const FStateTreeBindableStructDesc& Desc, const FStateTreeDataView Value)> InFunc) const
{
	if (VisitGlobalNodes(InFunc) == EStateTreeVisitor::Break)
	{
		return EStateTreeVisitor::Break;
	}

	return VisitHierarchyNodes(InFunc);
}

#if WITH_STATETREE_DEBUGGER
bool UStateTreeEditorData::HasAnyBreakpoint(const FGuid ID) const
{
	return Breakpoints.ContainsByPredicate([ID](const FStateTreeEditorBreakpoint& Breakpoint) { return Breakpoint.ID == ID; });
}

bool UStateTreeEditorData::HasBreakpoint(const FGuid ID, const EStateTreeBreakpointType BreakpointType) const
{
	return GetBreakpoint(ID, BreakpointType) != nullptr;
}

const FStateTreeEditorBreakpoint* UStateTreeEditorData::GetBreakpoint(const FGuid ID, const EStateTreeBreakpointType BreakpointType) const
{
	return Breakpoints.FindByPredicate([ID, BreakpointType](const FStateTreeEditorBreakpoint& Breakpoint)
		{
			return Breakpoint.ID == ID && Breakpoint.BreakpointType == BreakpointType;
		});
}

void UStateTreeEditorData::AddBreakpoint(const FGuid ID, const EStateTreeBreakpointType BreakpointType)
{
	Breakpoints.Emplace(ID, BreakpointType);

	const UStateTree* StateTree = GetTypedOuter<UStateTree>();
	checkf(StateTree, TEXT("UStateTreeEditorData should only be allocated within a UStateTree"));
	UE::StateTree::Delegates::OnBreakpointsChanged.Broadcast(*StateTree);
}

bool UStateTreeEditorData::RemoveBreakpoint(const FGuid ID, const EStateTreeBreakpointType BreakpointType)
{
	const int32 Index = Breakpoints.IndexOfByPredicate([ID, BreakpointType](const FStateTreeEditorBreakpoint& Breakpoint)
		{
			return Breakpoint.ID == ID && Breakpoint.BreakpointType == BreakpointType;
		});
		
	if (Index != INDEX_NONE)
	{
		Breakpoints.RemoveAtSwap(Index);
		
		const UStateTree* StateTree = GetTypedOuter<UStateTree>();
		checkf(StateTree, TEXT("UStateTreeEditorData should only be allocated within a UStateTree"));
		UE::StateTree::Delegates::OnBreakpointsChanged.Broadcast(*StateTree);
	}

	return Index != INDEX_NONE;
}

#endif // WITH_STATETREE_DEBUGGER

PRAGMA_DISABLE_DEPRECATION_WARNINGS
void UStateTreeEditorData::AddPropertyBinding(const FStateTreeEditorPropertyPath& SourcePath, const FStateTreeEditorPropertyPath& TargetPath)
{
	EditorBindings.AddPropertyBinding(UE::StateTree::Private::ConvertEditorPath(SourcePath), UE::StateTree::Private::ConvertEditorPath(TargetPath));
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
