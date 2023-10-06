// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeCompiler.h"
#include "StateTree.h"
#include "StateTreeAnyEnum.h"
#include "StateTreeCompilerLog.h"
#include "StateTreeEditorData.h"
#include "StateTreeEvaluatorBase.h"
#include "StateTreeTaskBase.h"
#include "StateTreeConditionBase.h"
#include "Serialization/ArchiveUObject.h"
#include "GameFramework/Actor.h"


namespace UE::StateTree::Compiler
{
	// Helper archive that checks that the all instanced sub-objects have correct outer. 
	class FCheckOutersArchive : public FArchiveUObject
	{
		using Super = FArchiveUObject;
		const UStateTree& StateTree;
		const UStateTreeEditorData& EditorData;
		FStateTreeCompilerLog& Log;
	public:

		FCheckOutersArchive(const UStateTree& InStateTree, const UStateTreeEditorData& InEditorData, FStateTreeCompilerLog& InLog)
			: StateTree(InStateTree)
			, EditorData(InEditorData)
			, Log(InLog)
		{
			Super::SetIsSaving(true);
			Super::SetIsPersistent(true);
		}

		virtual bool ShouldSkipProperty(const FProperty* InProperty) const
		{
			// Skip editor data.
			if (const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(InProperty))
			{
				if (ObjectProperty->PropertyClass == UStateTreeEditorData::StaticClass())
				{
					return true;
				}
			}
			return false;
		}

		virtual FArchive& operator<<(UObject*& Object) override
		{
			if (Object)
			{
				if (const FProperty* Property = GetSerializedProperty())
				{
					if (Property->HasAnyPropertyFlags(CPF_InstancedReference))
					{
						if (!Object->IsInOuter(&StateTree))
						{
							Log.Reportf(EMessageSeverity::Error, TEXT("Compiled StateTree contains instanced object %s (%s), which does not belong to the StateTree. This is due to error in the State Tree node implementation."),
								*GetFullNameSafe(Object), *GetFullNameSafe(Object->GetClass()));
						}

						if (Object->IsInOuter(&EditorData))
						{
							Log.Reportf(EMessageSeverity::Error, TEXT("Compiled StateTree contains instanced object %s (%s), which still belongs to the Editor data. This is due to error in the State Tree node implementation."),
								*GetFullNameSafe(Object), *GetFullNameSafe(Object->GetClass()));
						}
					}
				}
			}
			return *this;
		}
	};

	/** Scans Data for actors that are tied to some level and returns them. */
	void ScanLevelActorReferences(FStateTreeDataView Data, TSet<const UObject*>& Visited, TArray<const AActor*>& OutActors)
	{
		if (!Data.IsValid())
		{
			return;
		}
		
		for (TPropertyValueIterator<FProperty> It(Data.GetStruct(), Data.GetMemory()); It; ++It)
		{
			const FProperty* Property = It->Key;
			const void* ValuePtr = It->Value;

			if (!ValuePtr)
			{
				continue;
			}
			
			if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
			{
				if (StructProperty->Struct == TBaseStructure<FInstancedStruct>::Get())
				{
					const FInstancedStruct& InstancedStruct = *static_cast<const FInstancedStruct*>(ValuePtr);
					if (InstancedStruct.IsValid())
					{
						ScanLevelActorReferences(FStateTreeDataView(const_cast<FInstancedStruct&>(InstancedStruct)), Visited, OutActors);
					}
				}
			}
			else if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
			{
				if (const UObject* Object = ObjectProperty->GetObjectPropertyValue(ValuePtr))
				{
					if (const AActor* Actor = Cast<AActor>(Object))
					{
						const ULevel* Level = Actor->GetLevel();
						if (Level != nullptr)
						{
							OutActors.Add(Actor);
						}
					}
					// Recurse into instanced object
					if (Property->HasAnyPropertyFlags(CPF_InstancedReference))
					{
						if (!Visited.Contains(Object))
						{
							Visited.Add(Object);
							ScanLevelActorReferences(FStateTreeDataView(const_cast<UObject*>(Object)), Visited, OutActors);
						}
					}
				}
			}
		}
	}

	bool ValidateNoLevelActorReferences(FStateTreeCompilerLog& Log, const FStateTreeBindableStructDesc& NodeDesc, FStateTreeDataView NodeView, FStateTreeDataView InstanceView)
	{
		TSet<const UObject*> Visited;
		TArray<const AActor*> LevelActors;
		UE::StateTree::Compiler::ScanLevelActorReferences(NodeView, Visited, LevelActors);
		UE::StateTree::Compiler::ScanLevelActorReferences(InstanceView, Visited, LevelActors);
		if (!LevelActors.IsEmpty())
		{
			FStringBuilderBase AllActorsString;
			for (const AActor* Actor : LevelActors)
			{
				if (AllActorsString.Len() > 0)
				{
					AllActorsString += TEXT(", ");
				}
				AllActorsString += *GetNameSafe(Actor);
			}
			Log.Reportf(EMessageSeverity::Error, NodeDesc,
				TEXT("Level Actor references were found: %s. Direct Actor references are not allowed."),
					*AllActorsString);
			return false;
		}
		
		return true;
	}


	void FValidationResult::Log(FStateTreeCompilerLog& Log, const TCHAR* ContextText, const FStateTreeBindableStructDesc& ContextStruct) const
	{
		Log.Reportf(EMessageSeverity::Error, ContextStruct, TEXT("The StateTree is too complex. Compact index %s out of range %d/%d."), ContextText, Value, MaxValue);
	}

	const UScriptStruct* GetBaseStructFromMetaData(const FProperty* Property, FString& OutBaseStructName)
	{
		static const FName NAME_BaseStruct = "BaseStruct";

		const UScriptStruct* Result = nullptr;
		OutBaseStructName = Property->GetMetaData(NAME_BaseStruct);
	
		if (!OutBaseStructName.IsEmpty())
		{
			Result = UClass::TryFindTypeSlow<UScriptStruct>(OutBaseStructName);
			if (!Result)
			{
				Result = LoadObject<UScriptStruct>(nullptr, *OutBaseStructName);
			}
		}

		return Result;
	}

	EStateTreePropertyUsage GetUsageFromMetaData(const FProperty* Property)
	{
		static const FName CategoryName(TEXT("Category"));

		if (Property == nullptr)
		{
			return EStateTreePropertyUsage::Invalid;
		}
		
		const FString Category = Property->GetMetaData(CategoryName);

		if (Category == TEXT("Input"))
		{
			return EStateTreePropertyUsage::Input;
		}
		if (Category == TEXT("Inputs"))
		{
			return EStateTreePropertyUsage::Input;
		}
		if (Category == TEXT("Output"))
		{
			return EStateTreePropertyUsage::Output;
		}
		if (Category == TEXT("Outputs"))
		{
			return EStateTreePropertyUsage::Output;
		}
		if (Category == TEXT("Context"))
		{
			return EStateTreePropertyUsage::Context;
		}

		return EStateTreePropertyUsage::Parameter;
	}

}; // UE::StateTree::Compiler

bool FStateTreeCompiler::Compile(UStateTree& InStateTree)
{
	StateTree = &InStateTree;
	EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
	if (!EditorData)
	{
		return false;
	}

	// Cleanup existing state
	StateTree->ResetCompiled();

	if (!BindingsCompiler.Init(StateTree->PropertyBindings, Log))
	{
		StateTree->ResetCompiled();
		return false;
	}

	EditorData->GetAllStructValues(IDToStructValue);

	// Copy schema the EditorData
	StateTree->Schema = DuplicateObject(EditorData->Schema, StateTree);

	// Copy parameters from EditorData	
	StateTree->Parameters = EditorData->RootParameters.Parameters;

	// Mark parameters as binding source
	const FStateTreeBindableStructDesc ParametersDesc = {
			TEXT("Parameters"),
			StateTree->Parameters.GetPropertyBagStruct(),
			EStateTreeBindableStructSource::Parameter,
			EditorData->RootParameters.ID
		};
	const int32 ParametersDataViewIndex = BindingsCompiler.AddSourceStruct(ParametersDesc);

	if (const auto Validation = UE::StateTree::Compiler::IsValidIndex8(ParametersDataViewIndex); Validation.DidFail())
	{
		Validation.Log(Log, TEXT("ParametersDataViewIndex"), ParametersDesc);
		return false;
	}
	StateTree->ParametersDataViewIndex = FStateTreeIndex8(ParametersDataViewIndex); 

	if (!UE::StateTree::Compiler::ValidateNoLevelActorReferences(Log, ParametersDesc, FStateTreeDataView(), FStateTreeDataView(EditorData->RootParameters.Parameters.GetMutableValue())))
	{
		StateTree->ResetCompiled();
		return false;
	}

	// Mark all named external values as binding source
	if (StateTree->Schema)
	{
		StateTree->ContextDataDescs = StateTree->Schema->GetContextDataDescs();
		for (FStateTreeExternalDataDesc& Desc : StateTree->ContextDataDescs)
		{
			const FStateTreeBindableStructDesc ExtDataDesc = {
					Desc.Name,
					Desc.Struct,
					EStateTreeBindableStructSource::Context,
					Desc.ID
				};
			const int32 ExternalStructIndex = BindingsCompiler.AddSourceStruct(ExtDataDesc);
			if (const auto Validation = UE::StateTree::Compiler::IsValidIndex16(ExternalStructIndex); Validation.DidFail())
			{
				Validation.Log(Log, TEXT("ExternalStructIndex"), ParametersDesc);
				return false;
			}
			Desc.Handle.DataViewIndex = FStateTreeIndex16(ExternalStructIndex); 
		} 
	}
	
	if (!CreateStates())
	{
		StateTree->ResetCompiled();
		return false;
	}

	if (!CreateEvaluators())
	{
		StateTree->ResetCompiled();
		return false;
	}

	
	if (!CreateGlobalTasks())
	{
		StateTree->ResetCompiled();
		return false;
	}

	if (!CreateStateTasksAndParameters())
	{
		StateTree->ResetCompiled();
		return false;
	}

	if (!CreateStateTransitions())
	{
		StateTree->ResetCompiled();
		return false;
	}

	StateTree->Nodes = Nodes;
	StateTree->DefaultInstanceData.Init(*StateTree, InstanceStructs, InstanceObjects);
	StateTree->SharedInstanceData.Init(*StateTree, SharedInstanceStructs, SharedInstanceObjects);
	
	BindingsCompiler.Finalize();

	if (!StateTree->Link())
	{
		StateTree->ResetCompiled();
		Log.Reportf(EMessageSeverity::Error, TEXT("Unexpected failure to link the StateTree asset. See log for more info."));
		return false;
	}

	// Store mapping between node unique ID and their compiled index. Used for debugging purposes.
	for (const TPair<FGuid, int32>& ToNode : IDToNode)
	{
		StateTree->IDToNodeMappings.Emplace(ToNode.Key, FStateTreeIndex16(ToNode.Value));
	}

	// Store mapping between state unique ID and state handle. Used for debugging purposes.
	for (const TPair<FGuid, int32>& ToState : IDToState)
	{
		StateTree->IDToStateMappings.Emplace(ToState.Key, FStateTreeStateHandle(ToState.Value));
	}

	// Store mapping between state transition identifier and compact transition index. Used for debugging purposes.
	for (const TPair<FGuid, int32>& ToTransition: IDToTransition)
	{
		StateTree->IDToTransitionMappings.Emplace(ToTransition.Key, FStateTreeIndex16(ToTransition.Value));
	}

	UE::StateTree::Compiler::FCheckOutersArchive CheckOuters(*StateTree, *EditorData, Log);
	StateTree->Serialize(CheckOuters);
	
	return true;
}

FStateTreeStateHandle FStateTreeCompiler::GetStateHandle(const FGuid& StateID) const
{
	const int32* Idx = IDToState.Find(StateID);
	if (Idx == nullptr)
	{
		return FStateTreeStateHandle::Invalid;
	}

	return FStateTreeStateHandle(uint16(*Idx));
}

UStateTreeState* FStateTreeCompiler::GetState(const FGuid& StateID) const
{
	const int32* Idx = IDToState.Find(StateID);
	if (Idx == nullptr)
	{
		return nullptr;
	}

	return SourceStates[*Idx];
}

bool FStateTreeCompiler::CreateStates()
{
	check(EditorData);
	
	// Create item for the runtime execution state
	InstanceStructs.Add(FInstancedStruct::Make<FStateTreeExecutionState>());

	// Create main tree (omit subtrees)
	for (UStateTreeState* SubTree : EditorData->SubTrees)
	{
		if (SubTree != nullptr
			&& SubTree->Type != EStateTreeStateType::Subtree)
		{
			if (!CreateStateRecursive(*SubTree, FStateTreeStateHandle::Invalid))
			{
				return false;
			}
		}
	}

	// Create Subtrees
	for (UStateTreeState* SubTree : EditorData->SubTrees)
	{
		TArray<UStateTreeState*> Stack;
		Stack.Push(SubTree);
		while (!Stack.IsEmpty())
		{
			if (UStateTreeState* State = Stack.Pop())
			{
				if (State->Type == EStateTreeStateType::Subtree)
				{
					if (!CreateStateRecursive(*State, FStateTreeStateHandle::Invalid))
					{
						return false;
					}
				}
				Stack.Append(State->Children);
			}
		}
	}

	return true;
}

bool FStateTreeCompiler::CreateStateRecursive(UStateTreeState& State, const FStateTreeStateHandle Parent)
{
	check(StateTree);

	FStateTreeCompilerLogStateScope LogStateScope(&State, Log);

	const int32 StateIdx = StateTree->States.AddDefaulted();
	FCompactStateTreeState& CompactState = StateTree->States[StateIdx];
	CompactState.Name = State.Name;
	CompactState.Parent = Parent;
	CompactState.bEnabled = State.bEnabled;

	CompactState.Type = State.Type;
	CompactState.SelectionBehavior = State.SelectionBehavior;
	
	SourceStates.Add(&State);
	IDToState.Add(State.ID, StateIdx);

	// Child states
	const int32 ChildrenBegin = StateTree->States.Num();
	if (const auto Validation = UE::StateTree::Compiler::IsValidCount16(ChildrenBegin); Validation.DidFail())
	{
		Validation.Log(Log, TEXT("ChildrenBegin"));
		return false;
	}
	CompactState.ChildrenBegin = uint16(ChildrenBegin);

	for (UStateTreeState* Child : State.Children)
	{
		if (Child != nullptr && Child->Type != EStateTreeStateType::Subtree)
		{
			if (!CreateStateRecursive(*Child, FStateTreeStateHandle((uint16)StateIdx)))
			{
				return false;
			}
		}
	}
	
	const int32 ChildrenEnd = StateTree->States.Num();
	if (const auto Validation = UE::StateTree::Compiler::IsValidCount16(ChildrenEnd); Validation.DidFail())
	{
		Validation.Log(Log, TEXT("ChildrenEnd"));
		return false;
	}
	StateTree->States[StateIdx].ChildrenEnd = uint16(ChildrenEnd); // Not using CompactState here because the array may have changed.
	
	return true;
}

bool FStateTreeCompiler::CreateConditions(UStateTreeState& State, TConstArrayView<FStateTreeEditorNode> Conditions)
{
	for (int32 Index = 0; Index < Conditions.Num(); Index++)
	{
		const bool bIsFirst = Index == 0;
		const FStateTreeEditorNode& CondNode = Conditions[Index];
		// First operand should be copy as we dont have a previous item to operate on.
		const EStateTreeConditionOperand Operand = bIsFirst ? EStateTreeConditionOperand::Copy : CondNode.ConditionOperand;
		// First indent must be 0 to make the parentheses calculation match.
		const int32 CurrIndent = bIsFirst ? 0 : FMath::Clamp((int32)CondNode.ConditionIndent, 0, UE::StateTree::MaxConditionIndent);
		// Next indent, or terminate at zero.
		const int32 NextIndent = Conditions.IsValidIndex(Index + 1) ? FMath::Clamp((int32)Conditions[Index + 1].ConditionIndent, 0, UE::StateTree::MaxConditionIndent) : 0;
		
		const int32 DeltaIndent = NextIndent - CurrIndent;

		if (!CreateCondition(State, CondNode, Operand, (int8)DeltaIndent))
		{
			return false;
		}
	}

	return true;
}

bool FStateTreeCompiler::CreateEvaluators()
{
	check(EditorData);
	check(StateTree);

	const int32 EvaluatorsBegin = Nodes.Num();
	if (const auto Validation = UE::StateTree::Compiler::IsValidCount16(EvaluatorsBegin); Validation.DidFail())
	{
		Validation.Log(Log, TEXT("EvaluatorsBegin"));
		return false;
	}
	StateTree->EvaluatorsBegin = uint16(EvaluatorsBegin);

	for (FStateTreeEditorNode& EvalNode : EditorData->Evaluators)
	{
		if (!CreateEvaluator(EvalNode))
		{
			return false;
		}
	}
	
	const int32 EvaluatorsNum = Nodes.Num() - EvaluatorsBegin;
	if (const auto Validation = UE::StateTree::Compiler::IsValidCount16(EvaluatorsNum); Validation.DidFail())
	{
		Validation.Log(Log, TEXT("EvaluatorsNum"));
		return false;
	}
	StateTree->EvaluatorsNum = uint16(EvaluatorsNum);

	return true;
}

bool FStateTreeCompiler::CreateGlobalTasks()
{
	check(EditorData);
	check(StateTree);

	const int32 GlobalTasksBegin = Nodes.Num();
	if (const auto Validation = UE::StateTree::Compiler::IsValidCount16(GlobalTasksBegin); Validation.DidFail())
	{
		Validation.Log(Log, TEXT("GlobalTasksBegin"));
		return false;
	}
	StateTree->GlobalTasksBegin = uint16(GlobalTasksBegin);

	StateTree->bHasGlobalTransitionTasks = false;
	for (FStateTreeEditorNode& TaskNode : EditorData->GlobalTasks)
	{
		// Silently ignore empty nodes.
		if (!TaskNode.Node.IsValid())
		{
			continue;
		}

		if (!CreateTask(nullptr, TaskNode))
		{
			return false;
		}
		
		const FStateTreeTaskBase& LastAddedTask = Nodes.Last().Get<FStateTreeTaskBase>();
		
		StateTree->bHasGlobalTransitionTasks |= LastAddedTask.bShouldAffectTransitions;
	}
	
	const int32 GlobalTasksNum = Nodes.Num() - GlobalTasksBegin;
	if (const auto Validation = UE::StateTree::Compiler::IsValidCount16(GlobalTasksNum); Validation.DidFail())
	{
		Validation.Log(Log, TEXT("GlobalTasksNum"));
		return false;
	}
	StateTree->GlobalTasksNum = uint16(GlobalTasksNum);

	return true;
}

bool FStateTreeCompiler::CreateStateTasksAndParameters()
{
	check(StateTree);

	for (int32 i = 0; i < StateTree->States.Num(); i++)
	{
		FCompactStateTreeState& CompactState = StateTree->States[i];
		UStateTreeState* SourceState = SourceStates[i];
		check(SourceState != nullptr);

		FStateTreeCompilerLogStateScope LogStateScope(SourceState, Log);

		// Create parameters
		if (SourceState->Type == EStateTreeStateType::Linked || SourceState->Type == EStateTreeStateType::Subtree)
		{
			// Both linked and subtree has instance data describing their parameters.
			// This allows to resolve the binding paths and lets us have bindable parameters when transitioned into a parameterized subtree directly.
			FInstancedStruct& Instance = InstanceStructs.AddDefaulted_GetRef();
			const int32 InstanceIndex = InstanceStructs.Num() - 1;
			if (const auto Validation = UE::StateTree::Compiler::IsValidIndex16(InstanceIndex); Validation.DidFail())
			{
				Validation.Log(Log, TEXT("InstanceIndex"));
				return false;
			}
			CompactState.ParameterInstanceIndex = FStateTreeIndex16(InstanceIndex);
		
			Instance.InitializeAs<FCompactStateTreeParameters>();
			FCompactStateTreeParameters& CompactParams = Instance.GetMutable<FCompactStateTreeParameters>();

			CompactParams.Parameters = SourceState->Parameters.Parameters;

			if (SourceState->Type == EStateTreeStateType::Subtree)
			{
				// Register a binding source if we have parameters
				int32 SourceStructIndex = INDEX_NONE;

				if (SourceState->Parameters.Parameters.IsValid())
				{
					const FStateTreeBindableStructDesc SubtreeParamsDesc = {
						SourceState->Name,
						SourceState->Parameters.Parameters.GetPropertyBagStruct(),
						EStateTreeBindableStructSource::State,
						SourceState->Parameters.ID
					};

					if (!UE::StateTree::Compiler::ValidateNoLevelActorReferences(Log, SubtreeParamsDesc, FStateTreeDataView(), FStateTreeDataView(CompactParams.Parameters.GetMutableValue())))
					{
						return false;
					}
					
					SourceStructIndex = BindingsCompiler.AddSourceStruct(SubtreeParamsDesc);
					
					if (const auto Validation = UE::StateTree::Compiler::IsValidIndex16(SourceStructIndex); Validation.DidFail())
					{
						Validation.Log(Log, TEXT("SourceStructIndex"), SubtreeParamsDesc);
						return false;
					}
				}
				
				CompactState.ParameterDataViewIndex = FStateTreeIndex16(SourceStructIndex);
			}
			else if (SourceState->Type == EStateTreeStateType::Linked)
			{
				int32 BatchIndex = INDEX_NONE;

				if (SourceState->Parameters.Parameters.IsValid())
				{
					// Binding target
					FStateTreeBindableStructDesc LinkedParamsDesc = {
						SourceState->Name,
						SourceState->Parameters.Parameters.GetPropertyBagStruct(),
						EStateTreeBindableStructSource::State,
						SourceState->Parameters.ID
					};

					if (!UE::StateTree::Compiler::ValidateNoLevelActorReferences(Log, LinkedParamsDesc, FStateTreeDataView(), FStateTreeDataView(CompactParams.Parameters.GetMutableValue())))
					{
						return false;
					}

					// Check that the bindings for this struct are still all valid.
					TArray<FStateTreePropertyPathBinding> Bindings;
					if (!GetAndValidateBindings(LinkedParamsDesc, FStateTreeDataView(SourceState->Parameters.Parameters.GetMutableValue()), Bindings))
					{
						return false;
					}

					if (!BindingsCompiler.CompileBatch(LinkedParamsDesc, Bindings, BatchIndex))
					{
						return false;
					}
					
					if (const auto Validation = UE::StateTree::Compiler::IsValidIndex16(BatchIndex); Validation.DidFail())
					{
						Validation.Log(Log, TEXT("BatchIndex"), LinkedParamsDesc);
						return false;
					}
				}

				CompactParams.BindingsBatch = FStateTreeIndex16(BatchIndex);
			}
		}
		
		// Create tasks
		const int32 TasksBegin = Nodes.Num();
		if (const auto Validation = UE::StateTree::Compiler::IsValidCount16(TasksBegin); Validation.DidFail())
		{
			Validation.Log(Log, TEXT("TasksBegin"));
			return false;
		}
		CompactState.TasksBegin = uint16(TasksBegin);
		
		int32 TaskInstanceStructNum = 0;
		int32 TaskInstanceObjectNum = 0;

		// Update instance data num for each state.
		if (CompactState.Type == EStateTreeStateType::Linked)
		{
			// Linked state parameters.
			TaskInstanceStructNum++;
		}

		TArrayView<FStateTreeEditorNode> Tasks;
		if (SourceState->Tasks.Num())
		{
			Tasks = SourceState->Tasks;
		}
		else if (SourceState->SingleTask.Node.IsValid())
		{
			Tasks = TArrayView<FStateTreeEditorNode>(&SourceState->SingleTask, 1);
		}
		
		bool bStateHasTransitionTasks = false;
		for (FStateTreeEditorNode& TaskNode : Tasks)
		{
			// Silently ignore empty nodes.
			if (!TaskNode.Node.IsValid())
			{
				continue;
			}

			if (!CreateTask(SourceState, TaskNode))
			{
				return false;
			}

			const FStateTreeTaskBase& LastAddedTask = Nodes.Last().Get<FStateTreeTaskBase>();
			if (LastAddedTask.bInstanceIsObject)
			{
				TaskInstanceObjectNum++;
			}
			else
			{
				TaskInstanceStructNum++;
			}
			
			bStateHasTransitionTasks |= LastAddedTask.bShouldAffectTransitions;
		}

		CompactState.bHasTransitionTasks = bStateHasTransitionTasks;
	
		const int32 TasksNum = Nodes.Num() - TasksBegin;
		if (const auto Validation = UE::StateTree::Compiler::IsValidCount8(TasksNum); Validation.DidFail())
		{
			Validation.Log(Log, TEXT("TasksNum"));
			return false;
		}
		if (const auto Validation = UE::StateTree::Compiler::IsValidCount8(TaskInstanceObjectNum); Validation.DidFail())
		{
			Validation.Log(Log, TEXT("TaskInstanceObjectNum"));
			return false;
		}
		if (const auto Validation = UE::StateTree::Compiler::IsValidCount8(TaskInstanceStructNum); Validation.DidFail())
		{
			Validation.Log(Log, TEXT("TaskInstanceStructNum"));
			return false;
		}

		CompactState.TasksNum = uint8(TasksNum);
		CompactState.TaskInstanceStructNum = uint8(TaskInstanceStructNum);
		CompactState.TaskInstanceObjectNum = uint8(TaskInstanceObjectNum);
	}
	
	return true;
}

bool FStateTreeCompiler::CreateStateTransitions()
{
	check(StateTree);

	for (int32 i = 0; i < StateTree->States.Num(); i++)
	{
		FCompactStateTreeState& CompactState = StateTree->States[i];
		UStateTreeState* SourceState = SourceStates[i];
		check(SourceState != nullptr);

		FStateTreeCompilerLogStateScope LogStateScope(SourceState, Log);
		
		// Enter conditions.
		const int32 EnterConditionsBegin = Nodes.Num();
		if (const auto Validation = UE::StateTree::Compiler::IsValidCount16(EnterConditionsBegin); Validation.DidFail())
		{
			Validation.Log(Log, TEXT("EnterConditionsBegin"));
			return false;
		}
		CompactState.EnterConditionsBegin = uint16(EnterConditionsBegin);
		
		if (!CreateConditions(*SourceState, SourceState->EnterConditions))
		{
			Log.Reportf(EMessageSeverity::Error,
				TEXT("Failed to create state enter condition."));
			return false;
		}
		
		const int32 EnterConditionsNum = Nodes.Num() - EnterConditionsBegin;
		if (const auto Validation = UE::StateTree::Compiler::IsValidCount8(EnterConditionsNum); Validation.DidFail())
		{
			Validation.Log(Log, TEXT("EnterConditionsNum"));
			return false;
		}
		CompactState.EnterConditionsNum = uint8(EnterConditionsNum);

		// Linked state
		if (SourceState->Type == EStateTreeStateType::Linked)
		{
			// Make sure the linked state is not self or parent to this state.
			const UStateTreeState* LinkedParentState = nullptr;
			for (const UStateTreeState* State = SourceState; State != nullptr; State = State->Parent)
			{
				if (State->ID == SourceState->LinkedSubtree.ID)
				{
					LinkedParentState = State;
					break;
				}
			}
			
			if (LinkedParentState != nullptr)
			{
				Log.Reportf(EMessageSeverity::Error,
					TEXT("State is linked to it's parent subtree '%s', which will create infinite loop."),
					*LinkedParentState->Name.ToString());
				return false;
			}

			// The linked state must be a subtree.
			const UStateTreeState* TargetState = GetState(SourceState->LinkedSubtree.ID);
			if (TargetState == nullptr)
			{
				Log.Reportf(EMessageSeverity::Error,
					TEXT("Failed to resolve linked subtree '%s'."),
					*SourceState->LinkedSubtree.Name.ToString());
				return false;
			}
			
			if (TargetState->Type != EStateTreeStateType::Subtree)
			{
				Log.Reportf(EMessageSeverity::Error,
					TEXT("State '%s' is linked to subtree '%s', which is not a subtree."),
					*SourceState->Name.ToString(), *TargetState->Name.ToString());
				return false;
			}
			
			CompactState.LinkedState = GetStateHandle(SourceState->LinkedSubtree.ID);
			
			if (!CompactState.LinkedState.IsValid())
			{
				Log.Reportf(EMessageSeverity::Error,
					TEXT("Failed to resolve linked subtree '%s'."),
					*SourceState->LinkedSubtree.Name.ToString());
				return false;
			}
		}
		
		// Transitions
		const int32 TransitionsBegin = StateTree->Transitions.Num();
		if (const auto Validation = UE::StateTree::Compiler::IsValidCount16(TransitionsBegin); Validation.DidFail())
		{
			Validation.Log(Log, TEXT("TransitionsBegin"));
			return false;
		}
		CompactState.TransitionsBegin = uint16(TransitionsBegin);
		
		for (FStateTreeTransition& Transition : SourceState->Transitions)
		{
			IDToTransition.Add(Transition.ID, StateTree->Transitions.Num());

			FCompactStateTransition& CompactTransition = StateTree->Transitions.AddDefaulted_GetRef();
			CompactTransition.Trigger = Transition.Trigger;
			CompactTransition.Priority = Transition.Priority;
			CompactTransition.EventTag = Transition.EventTag;
			CompactTransition.bTransitionEnabled = Transition.bTransitionEnabled;
			
			if (Transition.bDelayTransition)
			{
				CompactTransition.Delay.Set(Transition.DelayDuration, Transition.DelayRandomVariance);
			}
			
			if (CompactState.SelectionBehavior == EStateTreeStateSelectionBehavior::TryFollowTransitions
				&& Transition.bDelayTransition)
			{
				Log.Reportf(EMessageSeverity::Warning,
					TEXT("Transition to '%s' with delay will be ignored during state selection."),
					*Transition.State.Name.ToString());
			}

			if (EnumHasAnyFlags(Transition.Trigger, EStateTreeTransitionTrigger::OnStateCompleted))
			{
				// Completion transitions dont have priority.
				CompactTransition.Priority = EStateTreeTransitionPriority::None;
				
				// Completion transitions cannot have delay.
				CompactTransition.Delay.Reset();

				// Completion transitions must have valid target state.
				if (Transition.State.LinkType == EStateTreeTransitionType::None)
				{
					Log.Reportf(EMessageSeverity::Error,
						TEXT("State completion transition to '%s' must have transition to valid state, 'None' not accepted."),
						*Transition.State.Name.ToString());
				}
			}
			
			CompactTransition.State = FStateTreeStateHandle::Invalid;
			if (!ResolveTransitionState(SourceState, Transition.State, CompactTransition.State))
			{
				return false;
			}
			
			const int32 ConditionsBegin = Nodes.Num();
			if (const auto Validation = UE::StateTree::Compiler::IsValidCount16(ConditionsBegin); Validation.DidFail())
			{
				Validation.Log(Log, TEXT("ConditionsBegin"));
				return false;
			}
			CompactTransition.ConditionsBegin = uint16(ConditionsBegin);
			
			if (!CreateConditions(*SourceState, Transition.Conditions))
			{
				Log.Reportf(EMessageSeverity::Error,
					TEXT("Failed to create condition for transition to '%s'."),
					*Transition.State.Name.ToString());
				return false;
			}

			const int32 ConditionsNum = Nodes.Num() - ConditionsBegin;
			if (const auto Validation = UE::StateTree::Compiler::IsValidCount8(ConditionsNum); Validation.DidFail())
			{
				Validation.Log(Log, TEXT("ConditionsNum"));
				return false;
			}
			CompactTransition.ConditionsNum = uint8(ConditionsNum);
		}
		
		const int32 TransitionsNum = StateTree->Transitions.Num() - TransitionsBegin;
		if (const auto Validation = UE::StateTree::Compiler::IsValidCount8(TransitionsNum); Validation.DidFail())
		{
			Validation.Log(Log, TEXT("TransitionsNum"));
			return false;
		}
		CompactState.TransitionsNum = uint8(TransitionsNum);
	}

	// @todo: Add test to check that all success/failure transition is possible (see editor).
	
	return true;
}

bool FStateTreeCompiler::ResolveTransitionState(const UStateTreeState* SourceState, const FStateTreeStateLink& Link, FStateTreeStateHandle& OutTransitionHandle) const 
{
	if (Link.LinkType == EStateTreeTransitionType::GotoState)
	{
		// Warn if goto state points to another subtree.
		if (const UStateTreeState* TargetState = GetState(Link.ID))
		{
			if (TargetState->GetRootState() != SourceState->GetRootState())
			{
				Log.Reportf(EMessageSeverity::Warning,
					TEXT("Target state '%s' is in different subtree. Verify that this is intentional."),
					*Link.Name.ToString());
			}

			if (TargetState->SelectionBehavior == EStateTreeStateSelectionBehavior::None)
			{
				Log.Reportf(EMessageSeverity::Error,
					TEXT("The target State '%s' is not selectable, it's selection behavior is set to None."),
					*Link.Name.ToString());
				return false;
			}
		}
		
		OutTransitionHandle = GetStateHandle(Link.ID);
		if (!OutTransitionHandle.IsValid())
		{
			Log.Reportf(EMessageSeverity::Error,
				TEXT("Failed to resolve transition to state '%s'."),
				*Link.Name.ToString());
			return false;
		}
	}
	else if (Link.LinkType == EStateTreeTransitionType::NextState)
	{
		// Find next state.
		const UStateTreeState* NextState = SourceState ? SourceState->GetNextSelectableSiblingState() : nullptr;
		if (NextState == nullptr)
		{
			Log.Reportf(EMessageSeverity::Error,
				TEXT("Failed to resolve transition, there's no selectable next state."));
			return false;
		}
		OutTransitionHandle = GetStateHandle(NextState->ID);
		if (!OutTransitionHandle.IsValid())
		{
			Log.Reportf(EMessageSeverity::Error,
				TEXT("Failed to resolve transition next state, no handle found for '%s'."),
				*NextState->Name.ToString());
			return false;
		}
	}
	else if(Link.LinkType == EStateTreeTransitionType::Failed)
	{
		OutTransitionHandle = FStateTreeStateHandle::Failed;
		return true;
	}
	else if(Link.LinkType == EStateTreeTransitionType::Succeeded)
	{
		OutTransitionHandle = FStateTreeStateHandle::Succeeded;
		return true;
	}
	else if(Link.LinkType == EStateTreeTransitionType::None)
	{
		OutTransitionHandle = FStateTreeStateHandle::Invalid;
		return true;
	}
	
	return true;
}

bool FStateTreeCompiler::CreateCondition(UStateTreeState& State, const FStateTreeEditorNode& CondNode, const EStateTreeConditionOperand Operand, const int8 DeltaIndent)
{
	if (!CondNode.Node.IsValid())
	{
		// Empty line in conditions array, just silently ignore.
		return true;
	}

	FStateTreeBindableStructDesc StructDesc;
	StructDesc.ID = CondNode.ID;
	StructDesc.Name = CondNode.Node.GetScriptStruct()->GetFName();
	StructDesc.DataSource = EStateTreeBindableStructSource::Condition;

	// Check that item has valid instance initialized.
	if (!CondNode.Instance.IsValid() && CondNode.InstanceObject == nullptr)
	{
		Log.Reportf(EMessageSeverity::Error, StructDesc,
			TEXT("Malformed condition, missing instance value."));
		return false;
	}

	// Copy the condition
	IDToNode.Add(CondNode.ID, Nodes.Num());
	FInstancedStruct& Node = Nodes.Add_GetRef(CondNode.Node);
	InstantiateStructSubobjects(Node);

	FStateTreeConditionBase& Cond = Node.GetMutable<FStateTreeConditionBase>();

	Cond.Operand = Operand;
	Cond.DeltaIndent = DeltaIndent;

	FStateTreeDataView InstanceDataView;
	
	if (CondNode.Instance.IsValid())
	{
		// Struct instance
		const int32 InstanceIndex = SharedInstanceStructs.Add(CondNode.Instance);
		InstantiateStructSubobjects(SharedInstanceStructs[InstanceIndex]);

		// Create binding source struct descriptor.
		StructDesc.Struct = CondNode.Instance.GetScriptStruct();
		StructDesc.Name = Cond.Name;

		if (const auto Validation = UE::StateTree::Compiler::IsValidIndex16(InstanceIndex); Validation.DidFail())
		{
			Validation.Log(Log, TEXT("InstanceIndex"), StructDesc);
			return false;
		}
		Cond.InstanceIndex = FStateTreeIndex16(InstanceIndex);
		Cond.bInstanceIsObject = false;
		InstanceDataView = FStateTreeDataView(SharedInstanceStructs[InstanceIndex]);
	}
	else
	{
		// Object Instance
		check(CondNode.InstanceObject != nullptr);
		
		UObject* Instance = DuplicateObject(CondNode.InstanceObject, StateTree);
		const int32 InstanceIndex = SharedInstanceObjects.Add(Instance);
		
		// Create binding source struct descriptor.
		StructDesc.Struct = Instance->GetClass();
		StructDesc.Name = Cond.Name;

		if (const auto Validation = UE::StateTree::Compiler::IsValidIndex16(InstanceIndex); Validation.DidFail())
		{
			Validation.Log(Log, TEXT("InstanceIndex"), StructDesc);
			return false;
		}
		Cond.InstanceIndex = FStateTreeIndex16(InstanceIndex);
		Cond.bInstanceIsObject = true;
		InstanceDataView = FStateTreeDataView(Instance);
	}

	if (!CompileAndValidateNode(&State, StructDesc, Node, InstanceDataView))
	{
		return false;
	}

	// Mark the struct as binding source.
	const int32 SourceStructIndex = BindingsCompiler.AddSourceStruct(StructDesc);

	// Check that the bindings for this struct are still all valid.
	TArray<FStateTreePropertyPathBinding> Bindings;
	if (!GetAndValidateBindings(StructDesc, InstanceDataView, Bindings))
	{
		return false;
	}

	// Compile batch copy for this struct, we pass in all the bindings, the compiler will pick up the ones for the target structs.
	int32 BatchIndex = INDEX_NONE;
	if (!BindingsCompiler.CompileBatch(StructDesc, Bindings, BatchIndex))
	{
		return false;
	}

	if (const auto Validation = UE::StateTree::Compiler::IsValidIndex16(BatchIndex); Validation.DidFail())
	{
		Validation.Log(Log, TEXT("BatchIndex"), StructDesc);
		return false;
	}
	Cond.BindingsBatch = FStateTreeIndex16(BatchIndex);

	if (const auto Validation = UE::StateTree::Compiler::IsValidIndex16(SourceStructIndex); Validation.DidFail())
	{
		Validation.Log(Log, TEXT("SourceStructIndex"), StructDesc);
		return false;
	}
	Cond.DataViewIndex = FStateTreeIndex16(SourceStructIndex);
	
	return true;
}

bool FStateTreeCompiler::CompileAndValidateNode(const UStateTreeState* SourceState, const FStateTreeBindableStructDesc& NodeDesc, FStructView NodeView, const FStateTreeDataView InstanceData)
{
	if (!NodeView.IsValid())
	{
		return false;
	}
	
	FStateTreeNodeBase& Node = NodeView.Get<FStateTreeNodeBase>();
	check(InstanceData.IsValid());

	auto ValidateStateLinks = [this, SourceState](TPropertyValueIterator<FStructProperty> It)
	{
		for ( ; It; ++It)
		{
			if (It->Key->Struct == TBaseStructure<FStateTreeStateLink>::Get())
			{
				FStateTreeStateLink& StateLink = *static_cast<FStateTreeStateLink*>(const_cast<void*>(It->Value));

				if (!ResolveTransitionState(SourceState, StateLink, StateLink.StateHandle))
				{
					return false;
				}
			}
		}

		return true;
	};
	
	// Validate any state links.
	if (!ValidateStateLinks(TPropertyValueIterator<FStructProperty>(InstanceData.GetStruct(), InstanceData.GetMutableMemory())))
	{
		return false;
	}
	if (!ValidateStateLinks(TPropertyValueIterator<FStructProperty>(NodeView.GetScriptStruct(), NodeView.GetMemory())))
	{
		return false;
	}

	TArray<FText> ValidationErrors;
	const EDataValidationResult Result = Node.Compile(InstanceData, ValidationErrors);

	if (Result == EDataValidationResult::Invalid && ValidationErrors.IsEmpty())
	{
		Log.Report(EMessageSeverity::Error, NodeDesc, TEXT("Node validation failed."));
	}
	else
	{
		const EMessageSeverity::Type Severity = Result == EDataValidationResult::Invalid ? EMessageSeverity::Error : EMessageSeverity::Warning;
		for (const FText& Error : ValidationErrors)
		{
			Log.Report(Severity, NodeDesc, Error.ToString());
		}
	}

	// Make sure there's no level actor references in the data.
	if (!UE::StateTree::Compiler::ValidateNoLevelActorReferences(Log, NodeDesc, NodeView, InstanceData))
	{
		return false;
	}
	
	return Result != EDataValidationResult::Invalid;
}

bool FStateTreeCompiler::CreateTask(UStateTreeState* State, const FStateTreeEditorNode& TaskNode)
{
	if (!TaskNode.Node.IsValid())
	{
		return false;
	}
	
	// Create binding source struct descriptor.
	FStateTreeBindableStructDesc StructDesc;
	StructDesc.ID = TaskNode.ID;
	StructDesc.Name = TaskNode.Node.GetScriptStruct()->GetFName();
	StructDesc.DataSource = EStateTreeBindableStructSource::Task;

	// Check that node has valid instance initialized.
	if (!TaskNode.Instance.IsValid() && TaskNode.InstanceObject == nullptr)
	{
		Log.Reportf(EMessageSeverity::Error, StructDesc,
			TEXT("Malformed task, missing instance value."));
		return false;
	}

	// Copy the task
	IDToNode.Add(TaskNode.ID, Nodes.Num());
	FInstancedStruct& Node = Nodes.Add_GetRef(TaskNode.Node);
	InstantiateStructSubobjects(Node);
	
	FStateTreeTaskBase& Task = Node.GetMutable<FStateTreeTaskBase>();
	FStateTreeDataView InstanceDataView;

	if (TaskNode.Instance.IsValid())
	{
		// Struct Instance
		const int32 InstanceIndex = InstanceStructs.Add(TaskNode.Instance);
		InstantiateStructSubobjects(InstanceStructs[InstanceIndex]);

		// Create binding source struct descriptor.
		StructDesc.Struct = TaskNode.Instance.GetScriptStruct();
		StructDesc.Name = Task.Name;

		if (const auto Validation = UE::StateTree::Compiler::IsValidIndex16(InstanceIndex); Validation.DidFail())
		{
			Validation.Log(Log, TEXT("InstanceIndex"), StructDesc);
			return false;
		}
		Task.InstanceIndex = FStateTreeIndex16(InstanceIndex);
		Task.bInstanceIsObject = false;
		InstanceDataView = FStateTreeDataView(InstanceStructs[InstanceIndex]);
	}
	else
	{
		// Object Instance
		check(TaskNode.InstanceObject != nullptr);

		UObject* Instance = DuplicateObject(TaskNode.InstanceObject, StateTree);
		const int32 InstanceIndex = InstanceObjects.Add(Instance);

		// Create binding source struct descriptor.
		StructDesc.Struct = Instance->GetClass();
		StructDesc.Name = Task.Name;

		if (const auto Validation = UE::StateTree::Compiler::IsValidIndex16(InstanceIndex); Validation.DidFail())
		{
			Validation.Log(Log, TEXT("InstanceIndex"), StructDesc);
			return false;
		}
		Task.InstanceIndex = FStateTreeIndex16(InstanceIndex);
		Task.bInstanceIsObject = true;
		InstanceDataView = FStateTreeDataView(Instance);
	}

	if (!CompileAndValidateNode(State, StructDesc, Node,  InstanceDataView))
	{
		return false;
	}

	// Mark the instance as binding source.
	const int32 SourceStructIndex = BindingsCompiler.AddSourceStruct(StructDesc);
	
	// Check that the bindings for this struct are still all valid.
	TArray<FStateTreePropertyPathBinding> Bindings;
	if (!GetAndValidateBindings(StructDesc, InstanceDataView, Bindings))
	{
		return false;
	}

	// Compile batch copy for this struct, we pass in all the bindings, the compiler will pick up the ones for the target structs.
	int32 BatchIndex = INDEX_NONE;
	if (!BindingsCompiler.CompileBatch(StructDesc, Bindings, BatchIndex))
	{
		return false;
	}

	if (const auto Validation = UE::StateTree::Compiler::IsValidIndex16(BatchIndex); Validation.DidFail())
	{
		Validation.Log(Log, TEXT("BatchIndex"), StructDesc);
		return false;
	}
	Task.BindingsBatch = FStateTreeIndex16(BatchIndex);

	if (const auto Validation = UE::StateTree::Compiler::IsValidIndex16(SourceStructIndex); Validation.DidFail())
	{
		Validation.Log(Log, TEXT("SourceStructIndex"), StructDesc);
		return false;
	}
	Task.DataViewIndex = FStateTreeIndex16(SourceStructIndex);
	
	return true;
}

bool FStateTreeCompiler::CreateEvaluator(const FStateTreeEditorNode& EvalNode)
{
	// Silently ignore empty nodes.
	if (!EvalNode.Node.IsValid())
	{
		return true;
	}
	
	// Create binding source struct descriptor.
	FStateTreeBindableStructDesc StructDesc;
    StructDesc.ID = EvalNode.ID;
    StructDesc.Name = EvalNode.Node.GetScriptStruct()->GetFName();
	StructDesc.DataSource = EStateTreeBindableStructSource::Evaluator;

    // Check that node has valid instance initialized.
    if (!EvalNode.Instance.IsValid() && EvalNode.InstanceObject == nullptr)
    {
        Log.Reportf(EMessageSeverity::Error, StructDesc,
        	TEXT("Malformed evaluator, missing instance value."));
        return false;
    }

	// Copy the evaluator
	IDToNode.Add(EvalNode.ID, Nodes.Num());
	FInstancedStruct& Node = Nodes.Add_GetRef(EvalNode.Node);
	InstantiateStructSubobjects(Node);
	
	FStateTreeEvaluatorBase& Eval = Node.GetMutable<FStateTreeEvaluatorBase>();
	FStateTreeDataView InstanceDataView;
	
	if (EvalNode.Instance.IsValid())
	{
		// Struct Instance
		const int32 InstanceIndex = InstanceStructs.Add(EvalNode.Instance);
		InstantiateStructSubobjects(InstanceStructs[InstanceIndex]);

		// Create binding source struct descriptor.
		StructDesc.Struct = EvalNode.Instance.GetScriptStruct();
		StructDesc.Name = Eval.Name;

		if (const auto Validation = UE::StateTree::Compiler::IsValidIndex16(InstanceIndex); Validation.DidFail())
		{
			Validation.Log(Log, TEXT("InstanceIndex"), StructDesc);
			return false;
		}
		Eval.InstanceIndex = FStateTreeIndex16(InstanceIndex);
		Eval.bInstanceIsObject = false;
		InstanceDataView = FStateTreeDataView(InstanceStructs[InstanceIndex]);
	}
	else
	{
		// Object Instance
		check(EvalNode.InstanceObject != nullptr);

		UObject* Instance = DuplicateObject(EvalNode.InstanceObject, StateTree);
		const int32 InstanceIndex = InstanceObjects.Add(Instance);

		// Create binding source struct descriptor.
		StructDesc.Struct = Instance->GetClass();
		StructDesc.Name = Eval.Name;

		if (const auto Validation = UE::StateTree::Compiler::IsValidIndex16(InstanceIndex); Validation.DidFail())
		{
			Validation.Log(Log, TEXT("InstanceIndex"), StructDesc);
			return false;
		}
		Eval.InstanceIndex = FStateTreeIndex16(InstanceIndex);
		Eval.bInstanceIsObject = true;
		InstanceDataView = FStateTreeDataView(Instance);
	}

	if (!CompileAndValidateNode(nullptr, StructDesc, Node,  InstanceDataView))
	{
		return false;
	}

	// Mark the instance as binding source.
	const int32 SourceStructIndex = BindingsCompiler.AddSourceStruct(StructDesc);

	// Check that the bindings for this struct are still all valid.
	TArray<FStateTreePropertyPathBinding> Bindings;
	if (!GetAndValidateBindings(StructDesc, InstanceDataView, Bindings))
	{
		return false;
	}

	// Compile batch copy for this struct, we pass in all the bindings, the compiler will pick up the ones for the target structs.
	int32 BatchIndex = INDEX_NONE;
	if (!BindingsCompiler.CompileBatch(StructDesc, Bindings, BatchIndex))
	{
		return false;
	}

	if (const auto Validation = UE::StateTree::Compiler::IsValidIndex16(BatchIndex); Validation.DidFail())
	{
		Validation.Log(Log, TEXT("BatchIndex"), StructDesc);
		return false;
	}
	Eval.BindingsBatch = FStateTreeIndex16(BatchIndex);

	if (const auto Validation = UE::StateTree::Compiler::IsValidIndex16(SourceStructIndex); Validation.DidFail())
	{
		Validation.Log(Log, TEXT("SourceStructIndex"), StructDesc);
		return false;
	}
	Eval.DataViewIndex = FStateTreeIndex16(SourceStructIndex);

	return true;
}

bool FStateTreeCompiler::IsPropertyAnyEnum(const FStateTreeBindableStructDesc& Struct, FStateTreePropertyPath Path) const
{
	bool bIsAnyEnum = false;

	TArray<FStateTreePropertyPathIndirection> Indirection;
	const bool bResolved = Path.ResolveIndirections(Struct.Struct, Indirection);
	
	if (bResolved && Indirection.Num() > 0)
	{
		check(Indirection.Last().GetProperty());
		if (const FProperty* OwnerProperty = Indirection.Last().GetProperty()->GetOwnerProperty())
		{
			if (const FStructProperty* OwnerStructProperty = CastField<FStructProperty>(OwnerProperty))
			{
				bIsAnyEnum = OwnerStructProperty->Struct == TBaseStructure<FStateTreeAnyEnum>::Get();
			}
		}
	}
	return bIsAnyEnum;
}

bool FStateTreeCompiler::ValidateStructRef(const FStateTreeBindableStructDesc& SourceStruct, FStateTreePropertyPath SourcePath,
											const FStateTreeBindableStructDesc& TargetStruct, FStateTreePropertyPath TargetPath) const
{
	FString ResolveError;
	TArray<FStateTreePropertyPathIndirection> TargetIndirection;
	if (!TargetPath.ResolveIndirections(TargetStruct.Struct, TargetIndirection, &ResolveError))
	{
		// This will later be reported by the bindings compiler.
		Log.Reportf(EMessageSeverity::Error, TargetStruct, TEXT("Failed to resolve binding path in %s: %s"), *TargetStruct.ToString(), *ResolveError);
		return false;
	}
	const FProperty* TargetLeafProperty = TargetIndirection.Num() > 0 ? TargetIndirection.Last().GetProperty() : nullptr;

	// Early out if the target is not FStateTreeStructRef.
	const FStructProperty* TargetStructProperty = CastField<FStructProperty>(TargetLeafProperty);
	if (TargetStructProperty == nullptr || TargetStructProperty->Struct != TBaseStructure<FStateTreeStructRef>::Get())
	{
		return true;
	}

	FString TargetBaseStructName;
	const UScriptStruct* TargetBaseStruct = UE::StateTree::Compiler::GetBaseStructFromMetaData(TargetStructProperty, TargetBaseStructName);
	if (TargetBaseStruct == nullptr)
	{
		Log.Reportf(EMessageSeverity::Error, TargetStruct,
				TEXT("Could not find base struct type '%s' for target %s'."),
				*TargetBaseStructName, *UE::StateTree::GetDescAndPathAsString(TargetStruct, TargetPath));
		return false;
	}

	TArray<FStateTreePropertyPathIndirection> SourceIndirection;
	if (!SourcePath.ResolveIndirections(SourceStruct.Struct, SourceIndirection, &ResolveError))
	{
		// This will later be reported by the bindings compiler.
		Log.Reportf(EMessageSeverity::Error, SourceStruct, TEXT("Failed to resolve binding path in %s: %s"), *SourceStruct.ToString(), *ResolveError);
		return false;
	}
	const FProperty* SourceLeafProperty = SourceIndirection.Num() > 0 ? SourceIndirection.Last().GetProperty() : nullptr;

	// Exit if the source is not a struct property.
	const FStructProperty* SourceStructProperty = CastField<FStructProperty>(SourceLeafProperty);
	if (SourceStructProperty == nullptr)
	{
		return true;
	}
	
	if (SourceStructProperty->Struct == TBaseStructure<FStateTreeStructRef>::Get())
	{
		// Source is struct ref too, check the types match.
		FString SourceBaseStructName;
		const UScriptStruct* SourceBaseStruct = UE::StateTree::Compiler::GetBaseStructFromMetaData(SourceStructProperty, SourceBaseStructName);
		if (SourceBaseStruct == nullptr)
		{
			Log.Reportf(EMessageSeverity::Error, TargetStruct,
					TEXT("Could not find base struct '%s' for binding source %s."),
					*SourceBaseStructName, *UE::StateTree::GetDescAndPathAsString(SourceStruct, SourcePath));
			return false;
		}

		if (SourceBaseStruct->IsChildOf(TargetBaseStruct) == false)
		{
			Log.Reportf(EMessageSeverity::Error, TargetStruct,
						TEXT("Type mismatch between source %s and target %s types, '%s' is not child of '%s'."),
						*UE::StateTree::GetDescAndPathAsString(SourceStruct, SourcePath),
						*UE::StateTree::GetDescAndPathAsString(TargetStruct, TargetPath),
						*GetNameSafe(SourceBaseStruct), *GetNameSafe(TargetBaseStruct));
			return false;
		}
	}
	else
	{
		if (!SourceStructProperty->Struct || SourceStructProperty->Struct->IsChildOf(TargetBaseStruct) == false)
		{
			Log.Reportf(EMessageSeverity::Error, TargetStruct,
						TEXT("Type mismatch between source %s and target %s types, '%s' is not child of '%s'."),
						*UE::StateTree::GetDescAndPathAsString(SourceStruct, SourcePath),
						*UE::StateTree::GetDescAndPathAsString(TargetStruct, TargetPath),
						*GetNameSafe(SourceStructProperty->Struct), *GetNameSafe(TargetBaseStruct));
			return false;
		}
	}

	return true;
}

FStateTreeDataView FStateTreeCompiler::GetBindingSourceValue(const int32 SourceIndex)
{
	for (const FInstancedStruct& Node : Nodes)
	{
		if (const FStateTreeNodeBase* NodeBase = Node.GetPtr<FStateTreeNodeBase>())
		{
			if (NodeBase->DataViewIndex.Get() == SourceIndex)
			{
				if (NodeBase->bInstanceIsObject)
				{
					return FStateTreeDataView(InstanceObjects[NodeBase->InstanceIndex.Get()]);
				}
				else
				{
					return FStateTreeDataView(InstanceStructs[NodeBase->InstanceIndex.Get()]);
				}
			}
		}
	}

	return {};
}

bool FStateTreeCompiler::GetAndValidateBindings(const FStateTreeBindableStructDesc& TargetStruct, FStateTreeDataView TargetValue, TArray<FStateTreePropertyPathBinding>& OutBindings) const
{
	check(EditorData);
	
	if (TargetStruct.Struct == nullptr)
	{
		Log.Reportf(EMessageSeverity::Error, TargetStruct,
				TEXT("The type of binding target %s is invalid."),
				*TargetStruct.ToString());
		return false;
	}
	
	OutBindings.Reset();
	
	for (FStateTreePropertyPathBinding& Binding : EditorData->EditorBindings.GetMutableBindings())
	{
		if (Binding.GetTargetPath().GetStructID() != TargetStruct.ID)
		{
			continue;
		}

		// Source must be one of the source structs we have discovered in the tree.
		const FGuid SourceStructID = Binding.GetSourcePath().GetStructID();
		const int32 SourceStructIdx = BindingsCompiler.GetSourceStructIndexByID(SourceStructID);
		if (SourceStructIdx == INDEX_NONE)
		{
			Log.Reportf(EMessageSeverity::Error, TargetStruct,
						TEXT("Failed to find binding source property '%s' for target %s."),
						*Binding.GetSourcePath().ToString(), *UE::StateTree::GetDescAndPathAsString(TargetStruct, Binding.GetTargetPath()));
			return false;
		}
		const FStateTreeBindableStructDesc& SourceStruct = BindingsCompiler.GetSourceStructDesc(SourceStructIdx);

		// Update path instance types from latest data. E.g. binding may have been created for instanced object of type FooB, and changed to FooA.
 		// @todo: not liking how this mutates the Binding.TargetPath, but currently we dont track well the instanced object changes.

		if (!Binding.GetMutableTargetPath().UpdateSegmentsFromValue(TargetValue))
		{
			Log.Reportf(EMessageSeverity::Error, TargetStruct,
						TEXT("Malformed target property path for binding source property '%s' for target %s."),
						*Binding.GetSourcePath().ToString(), *UE::StateTree::GetDescAndPathAsString(TargetStruct, Binding.GetTargetPath()));
			return false;
		}
		
		// Source must be accessible by the target struct via all execution paths.
		TArray<FStateTreeBindableStructDesc> AccessibleStructs;
		EditorData->GetAccessibleStructs(Binding.GetTargetPath().GetStructID(), AccessibleStructs);

		const bool bSourceAccessible = AccessibleStructs.ContainsByPredicate([SourceStructID](const FStateTreeBindableStructDesc& Structs)
			{
				return (Structs.ID == SourceStructID);
			});

		if (!bSourceAccessible)
		{
			Log.Reportf(EMessageSeverity::Error, TargetStruct,
						TEXT("Property at %s cannot be bound to %s, because the binding source %s is not updated before %s in the tree."),
						*UE::StateTree::GetDescAndPathAsString(SourceStruct, Binding.GetSourcePath()),
						*UE::StateTree::GetDescAndPathAsString(TargetStruct, Binding.GetTargetPath()),
						*SourceStruct.ToString(), *TargetStruct.ToString());
			return false;
		}

		if (!IDToStructValue.Contains(SourceStructID))
		{
			Log.Reportf(EMessageSeverity::Error, TargetStruct,
				TEXT("Failed to find value for binding source property '%s' for target %s."),
				*Binding.GetSourcePath().ToString(), *UE::StateTree::GetDescAndPathAsString(TargetStruct, Binding.GetTargetPath()));
			return false;
		}

		// Update the source structs only if we have value for it. For some sources (e.g. context structs) we know only type, and in that case there are no instance structs.
		const FStateTreeDataView SourceValue = IDToStructValue[SourceStructID];
		if (SourceValue.IsValid())
		{
			if (!Binding.GetMutableSourcePath().UpdateSegmentsFromValue(SourceValue))
			{
				Log.Reportf(EMessageSeverity::Error, TargetStruct,
					TEXT("Malformed target property path for binding source property '%s' for source %s."),
					*Binding.GetSourcePath().ToString(), *UE::StateTree::GetDescAndPathAsString(TargetStruct, Binding.GetTargetPath()));
				return false;
			}
		}

		// Special case fo AnyEnum. StateTreeBindingExtension allows AnyEnums to bind to other enum types.
		// The actual copy will be done via potential type promotion copy, into the value property inside the AnyEnum.
		// We amend the paths here to point to the 'Value' property.
		const bool bSourceIsAnyEnum = IsPropertyAnyEnum(SourceStruct, Binding.GetSourcePath());
		const bool bTargetIsAnyEnum = IsPropertyAnyEnum(TargetStruct, Binding.GetTargetPath());
		if (bSourceIsAnyEnum || bTargetIsAnyEnum)
		{
			FStateTreePropertyPathBinding ModifiedBinding(Binding);
			if (bSourceIsAnyEnum)
			{
				ModifiedBinding.GetMutableSourcePath().AddPathSegment(GET_MEMBER_NAME_STRING_CHECKED(FStateTreeAnyEnum, Value));
			}
			if (bTargetIsAnyEnum)
			{
				ModifiedBinding.GetMutableTargetPath().AddPathSegment(GET_MEMBER_NAME_STRING_CHECKED(FStateTreeAnyEnum, Value));
			}
			OutBindings.Add(ModifiedBinding);
		}
		else
		{
			OutBindings.Add(Binding);
		}

		// Check if the bindings is for struct ref and validate the types.
		if (!ValidateStructRef(SourceStruct, Binding.GetSourcePath(), TargetStruct, Binding.GetTargetPath()))
		{
			return false;
		}
	}


	auto IsPropertyBound = [&OutBindings](const FName& PropertyName)
	{
		return OutBindings.ContainsByPredicate([&PropertyName](const FStateTreePropertyPathBinding& Binding)
			{
				// We're looping over just the first level of properties on the struct, so we assume that the path is just one item
				// (or two in case of AnyEnum, because we expand the path to Property.Value, see code above).
				return Binding.GetTargetPath().GetSegments().Num() >= 1 && Binding.GetTargetPath().GetSegments()[0].GetName() == PropertyName;
			});
	};

	bool bResult = true;
	
	// Validate that Input and Context bindings
	for (TFieldIterator<FProperty> It(TargetStruct.Struct, EFieldIterationFlags::None); It; ++It)
	{
		const FProperty* Property = *It;
		check(Property);
		const FName PropertyName = Property->GetFName();
		const EStateTreePropertyUsage Usage = UE::StateTree::Compiler::GetUsageFromMetaData(Property);
		if (Usage == EStateTreePropertyUsage::Input)
		{
			const bool bIsOptional = Property->HasMetaData(TEXT("Optional"));
			
			// Make sure that an Input property is bound unless marked optional.
			if (bIsOptional == false && !IsPropertyBound(PropertyName))
			{
				Log.Reportf(EMessageSeverity::Error, TargetStruct,
					TEXT("Input property '%s' on %s is expected to have a binding."),
					*PropertyName.ToString(), *TargetStruct.ToString());
				bResult = false;
			}
		}
		else if (Usage == EStateTreePropertyUsage::Context)
		{
			// Make sure that an Context property is manually or automatically bound. 
			const UStruct* ContextObjectType = nullptr; 
			if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
			{
				ContextObjectType = StructProperty->Struct;
			}		
			else if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
			{
				ContextObjectType = ObjectProperty->PropertyClass;
			}

			if (ContextObjectType == nullptr)
			{
				Log.Reportf(EMessageSeverity::Error, TargetStruct,
					TEXT("The type of Context property '%s' on %s is expected to be Object Reference or Struct."),
					*PropertyName.ToString(), *TargetStruct.ToString());
				bResult = false;
				continue;
			}

			const bool bIsBound = IsPropertyBound(PropertyName);

			if (!bIsBound)
			{
				const FStateTreeBindableStructDesc Desc = EditorData->FindContextData(ContextObjectType, PropertyName.ToString());

				if (Desc.IsValid())
				{
					// Add automatic binding to Context data.
					OutBindings.Emplace(FStateTreePropertyPath(Desc.ID), FStateTreePropertyPath(TargetStruct.ID, PropertyName));
				}
				else
				{
					Log.Reportf(EMessageSeverity::Error, TargetStruct,
						TEXT("Cound not find matching Context object for Context property '%s' on %s. Property must have manual binding."),
						*PropertyName.ToString(), *TargetStruct.ToString());
					bResult = false;
				}
			}
		}
	}

	return bResult;
}

void FStateTreeCompiler::InstantiateStructSubobjects(FStructView Struct)
{
	check(StateTree);
	check(EditorData);
	
	// Empty struct, nothing to do.
	if (!Struct.IsValid())
	{
		return;
	}

	for (TPropertyValueIterator<FProperty> It(Struct.GetScriptStruct(), Struct.GetMemory()); It; ++It)
	{
		if (const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(It->Key))
		{
			// Duplicate instanced objects.
			if (ObjectProperty->HasAnyPropertyFlags(CPF_InstancedReference))
			{
				if (UObject* Object = ObjectProperty->GetObjectPropertyValue(It->Value))
				{
					UObject* OuterObject = Object->GetOuter();
					// If the instanced object was created as Editor Data as outer,
					// change the outer to State Tree to prevent references to editor only data.
					if (Object->IsInOuter(EditorData))
					{
						OuterObject = StateTree;
					}
					UObject* DuplicatedObject = DuplicateObject(Object, OuterObject);
					ObjectProperty->SetObjectPropertyValue(const_cast<void*>(It->Value), DuplicatedObject);
				}
			}
		}
		if (const FStructProperty* StructProperty = CastField<FStructProperty>(It->Key))
		{
			// If we encounter instanced struct, recursively handle it too.
			if (StructProperty->Struct == TBaseStructure<FInstancedStruct>::Get())
			{
				FInstancedStruct& InstancedStruct = *static_cast<FInstancedStruct*>(const_cast<void*>(It->Value));
				InstantiateStructSubobjects(InstancedStruct);
			}
		}
	}
}
