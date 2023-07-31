// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeCompiler.h"
#include "StateTree.h"
#include "StateTreeEditorData.h"
#include "StateTreeTypes.h"
#include "Conditions/StateTreeCommonConditions.h"
#include "StateTreeEvaluatorBase.h"
#include "StateTreeTaskBase.h"
#include "StateTreeConditionBase.h"
#include "StateTreeState.h"
#include "StateTreeExecutionContext.h"
#include "StateTreePropertyBindingCompiler.h"

namespace UE::StateTree::Compiler
{
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
			if (const auto Validation = UE::StateTree::Compiler::IsValidIndex8(ExternalStructIndex); Validation.DidFail())
			{
				Validation.Log(Log, TEXT("ExternalStructIndex"), ParametersDesc);
				return false;
			}
			Desc.Handle.DataViewIndex = FStateTreeIndex8(ExternalStructIndex); 
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
		return false;
	}
	
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

UStateTreeState* FStateTreeCompiler::GetState(const FGuid& StateID)
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
	// Create item for the runtime execution state
	InstanceStructs.Add(FInstancedStruct::Make<FStateTreeExecutionState>());

	// Create main tree (omit subtrees)
	for (UStateTreeState* SubTree : EditorData->SubTrees)
	{
		if (SubTree != nullptr)
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
	FStateTreeCompilerLogStateScope LogStateScope(&State, Log);

	const int32 StateIdx = StateTree->States.AddDefaulted();
	FCompactStateTreeState& CompactState = StateTree->States[StateIdx];
	CompactState.Name = State.Name;
	CompactState.Parent = Parent;

	CompactState.Type = State.Type; 
	
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
	StateTree->States[StateIdx].ChildrenEnd = uint16(ChildrenEnd);

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
		const int32 NextIndent = Conditions.IsValidIndex(Index + 1) ? FMath::Clamp((int32)Conditions[Index].ConditionIndent, 0, UE::StateTree::MaxConditionIndent) : 0;
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

bool FStateTreeCompiler::CreateStateTasksAndParameters()
{
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

					// Check that the bindings for this struct are still all valid.
					TArray<FStateTreeEditorPropertyBinding> Bindings;
					if (!GetAndValidateBindings(LinkedParamsDesc, Bindings))
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

		for (FStateTreeEditorNode& TaskNode : SourceState->Tasks)
		{
			if (!CreateTask(*SourceState, TaskNode))
			{
				return false;
			}
		}

		if (!CreateTask(*SourceState, SourceState->SingleTask))
		{
			return false;
		}
	
		const int32 TasksNum = Nodes.Num() - TasksBegin;
		if (const auto Validation = UE::StateTree::Compiler::IsValidCount8(TasksNum); Validation.DidFail())
		{
			Validation.Log(Log, TEXT("TasksNum"));
			return false;
		}
		CompactState.TasksNum = uint8(TasksNum);
	}
	
	return true;
}

bool FStateTreeCompiler::CreateStateTransitions()
{
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
			FCompactStateTransition& CompactTransition = StateTree->Transitions.AddDefaulted_GetRef();
			CompactTransition.Trigger = Transition.Trigger;
			CompactTransition.EventTag = Transition.EventTag;
			CompactTransition.Type = Transition.State.Type;
			CompactTransition.GateDelay = (uint8)FMath::Clamp(FMath::CeilToInt(Transition.GateDelay * 10.0f), 0, 255);
			CompactTransition.State = FStateTreeStateHandle::Invalid;
			if (!ResolveTransitionState(*SourceState, Transition.State, CompactTransition.State))
			{
				return false;
			}
			// Note: Unset transition is allowed here. It can be used to mask a transition at parent.
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

bool FStateTreeCompiler::ResolveTransitionState(const UStateTreeState& SourceState, const FStateTreeStateLink& Link, FStateTreeStateHandle& OutTransitionHandle) const 
{
	if (Link.Type == EStateTreeTransitionType::GotoState)
	{
		OutTransitionHandle = GetStateHandle(Link.ID);
		if (!OutTransitionHandle.IsValid())
		{
			Log.Reportf(EMessageSeverity::Error,
				TEXT("Failed to resolve transition to state '%s'."),
				*Link.Name.ToString());
			return false;
		}
	}
	else if (Link.Type == EStateTreeTransitionType::NextState)
	{
		// Find next state.
		const UStateTreeState* NextState = SourceState.GetNextSiblingState();
		if (NextState == nullptr)
		{
			Log.Reportf(EMessageSeverity::Error,
				TEXT("Failed to resolve transition, there's no next state."));
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
	const FInstancedStruct& Node = Nodes.Add_GetRef(CondNode.Node);
	FStateTreeConditionBase& Cond = Node.GetMutable<FStateTreeConditionBase>();

	Cond.Operand = Operand;
	Cond.DeltaIndent = DeltaIndent;
	
	if (CondNode.Instance.IsValid())
	{
		// Struct instance
		const int32 InstanceIndex = SharedInstanceStructs.Add(CondNode.Instance);
	
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
	}

	// Mark the struct as binding source.
	const int32 SourceStructIndex = BindingsCompiler.AddSourceStruct(StructDesc);

	// Check that the bindings for this struct are still all valid.
	TArray<FStateTreeEditorPropertyBinding> Bindings;
	if (!GetAndValidateBindings(StructDesc, Bindings))
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

bool FStateTreeCompiler::CreateTask(UStateTreeState& State, const FStateTreeEditorNode& TaskNode)
{
	// Silently ignore empty nodes.
	if (!TaskNode.Node.IsValid())
	{
		return true;
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
	const FInstancedStruct& Node = Nodes.Add_GetRef(TaskNode.Node);
	FStateTreeTaskBase& Task = Node.GetMutable<FStateTreeTaskBase>();

	if (TaskNode.Instance.IsValid())
	{
		// Struct Instance
		const int32 InstanceIndex = InstanceStructs.Add(TaskNode.Instance);

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
	}

	// Mark the instance as binding source.
	const int32 SourceStructIndex = BindingsCompiler.AddSourceStruct(StructDesc);

	// Check that the bindings for this struct are still all valid.
	TArray<FStateTreeEditorPropertyBinding> Bindings;
	if (!GetAndValidateBindings(StructDesc, Bindings))
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
	const FInstancedStruct& Node = Nodes.Add_GetRef(EvalNode.Node);
	FStateTreeEvaluatorBase& Eval = Node.GetMutable<FStateTreeEvaluatorBase>();

	if (EvalNode.Instance.IsValid())
	{
		// Struct Instance
		const int32 InstanceIndex = InstanceStructs.Add(EvalNode.Instance);

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
	}
		
	// Mark the instance as binding source.
	const int32 SourceStructIndex = BindingsCompiler.AddSourceStruct(StructDesc);

	// Check that the bindings for this struct are still all valid.
	TArray<FStateTreeEditorPropertyBinding> Bindings;
	if (!GetAndValidateBindings(StructDesc, Bindings))
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

bool FStateTreeCompiler::IsPropertyAnyEnum(const FStateTreeBindableStructDesc& Struct, FStateTreeEditorPropertyPath Path) const
{
	bool bIsAnyEnum = false;
	TArray<FStateTreePropertySegment> Segments;
	const FProperty* LeafProperty = nullptr;
	int32 LeafArrayIndex = INDEX_NONE;
	const bool bResolved = FStateTreePropertyBindingCompiler::ResolvePropertyPath(Struct, Path, Segments, LeafProperty, LeafArrayIndex);
	if (bResolved && LeafProperty)
	{
		if (const FProperty* OwnerProperty = LeafProperty->GetOwnerProperty())
		{
			if (const FStructProperty* OwnerStructProperty = CastField<FStructProperty>(OwnerProperty))
			{
				bIsAnyEnum = OwnerStructProperty->Struct == TBaseStructure<FStateTreeAnyEnum>::Get();
			}
		}
	}
	return bIsAnyEnum;
}

bool FStateTreeCompiler::ValidateStructRef(const FStateTreeBindableStructDesc& SourceStruct, FStateTreeEditorPropertyPath SourcePath,
											const FStateTreeBindableStructDesc& TargetStruct, FStateTreeEditorPropertyPath TargetPath) const
{
	TArray<FStateTreePropertySegment> Segments;

	const FProperty* TargetLeafProperty = nullptr;
	int32 TargetLeafArrayIndex = INDEX_NONE;
	if (FStateTreePropertyBindingCompiler::ResolvePropertyPath(TargetStruct, TargetPath, Segments, TargetLeafProperty, TargetLeafArrayIndex) == false)
	{
		// This will later be reported by the bindings compiler.
		return true;
	}

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
				TEXT("Could not find base struct '%s' for target '%s:%s'."),
				*TargetBaseStructName, *TargetStruct.Name.ToString(), *TargetPath.ToString());
		return false;
	}
	
	const FProperty* SourceLeafProperty = nullptr;
	int32 SourceLeafArrayIndex = INDEX_NONE;
	if (FStateTreePropertyBindingCompiler::ResolvePropertyPath(SourceStruct, SourcePath, Segments, SourceLeafProperty, SourceLeafArrayIndex) == false)
	{
		// This will later be reported by the bindings compiler.
		return true;
	}

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
					TEXT("Could not find base struct '%s' for bidning source '%s:%s'."),
					*SourceBaseStructName, *SourceStruct.Name.ToString(), *SourcePath.ToString());
			return false;
		}

		if (SourceBaseStruct->IsChildOf(TargetBaseStruct) == false)
		{
			Log.Reportf(EMessageSeverity::Error, TargetStruct,
						TEXT("Type mismatch between source '%s:%s' and target '%s:%s' types, '%s' is not child of '%s'."),
						*SourceStruct.Name.ToString(), *SourcePath.ToString(),
						*TargetStruct.Name.ToString(), *TargetPath.ToString(),
						*GetNameSafe(SourceBaseStruct), *GetNameSafe(TargetBaseStruct));
			return false;
		}
	}
	else
	{
		if (!SourceStructProperty->Struct || SourceStructProperty->Struct->IsChildOf(TargetBaseStruct) == false)
		{
			Log.Reportf(EMessageSeverity::Error, TargetStruct,
						TEXT("Type mismatch between source '%s:%s' and target '%s:%s' types, '%s' is not child of '%s'."),
						*SourceStruct.Name.ToString(), *SourcePath.ToString(),
						*TargetStruct.Name.ToString(), *TargetPath.ToString(),
						*GetNameSafe(SourceStructProperty->Struct), *GetNameSafe(TargetBaseStruct));
			return false;
		}
	}

	return true;
}

bool FStateTreeCompiler::GetAndValidateBindings(const FStateTreeBindableStructDesc& TargetStruct, TArray<FStateTreeEditorPropertyBinding>& OutBindings) const
{
	if (TargetStruct.Struct == nullptr)
	{
		Log.Reportf(EMessageSeverity::Error, TargetStruct,
				TEXT("The type of binding target '%s' is invalid."),
				*TargetStruct.Name.ToString());
		return false;
	}
	
	OutBindings.Reset();
	
	for (const FStateTreeEditorPropertyBinding& Binding : EditorData->EditorBindings.GetBindings())
	{
		if (Binding.TargetPath.StructID != TargetStruct.ID)
		{
			continue;
		}

		// Source must be one of the source structs we have discovered in the tree.
		const FGuid SourceStructID = Binding.SourcePath.StructID;
		const int32 SourceStructIdx = BindingsCompiler.GetSourceStructIndexByID(SourceStructID);
		if (SourceStructIdx == INDEX_NONE)
		{
			Log.Reportf(EMessageSeverity::Error, TargetStruct,
						TEXT("Failed to find binding source property '%s' for target '%s:%s'."),
						*Binding.SourcePath.ToString(), *TargetStruct.Name.ToString(), *Binding.TargetPath.ToString());
			return false;
		}
		const FStateTreeBindableStructDesc& SourceStruct = BindingsCompiler.GetSourceStructDesc(SourceStructIdx);

		// Source must be accessible by the target struct via all execution paths.
		TArray<FStateTreeBindableStructDesc> AccessibleStructs;
		EditorData->GetAccessibleStructs(Binding.TargetPath.StructID, AccessibleStructs);

		const bool bSourceAccessible = AccessibleStructs.ContainsByPredicate([SourceStructID](const FStateTreeBindableStructDesc& Structs)
			{
				return (Structs.ID == SourceStructID);
			});

		if (!bSourceAccessible)
		{
			Log.Reportf(EMessageSeverity::Error, TargetStruct,
						TEXT("Property '%s:%s' cannot be bound to '%s:%s', because the binding source '%s' is not updated before '%s' in the tree."),
						*SourceStruct.Name.ToString(), *Binding.SourcePath.ToString(),
						*TargetStruct.Name.ToString(), *Binding.TargetPath.ToString(),
						*SourceStruct.Name.ToString(), *TargetStruct.Name.ToString());
			return false;
		}
		
		// Special case fo AnyEnum. StateTreeBindingExtension allows AnyEnums to bind to other enum types.
		// The actual copy will be done via potential type promotion copy, into the value property inside the AnyEnum.
		// We amend the paths here to point to the 'Value' property.
		const bool bSourceIsAnyEnum = IsPropertyAnyEnum(SourceStruct, Binding.SourcePath);
		const bool bTargetIsAnyEnum = IsPropertyAnyEnum(TargetStruct, Binding.TargetPath);
		if (bSourceIsAnyEnum || bTargetIsAnyEnum)
		{
			FStateTreeEditorPropertyBinding ModifiedBinding(Binding);
			if (bSourceIsAnyEnum)
			{
				ModifiedBinding.SourcePath.Path.Add(GET_MEMBER_NAME_STRING_CHECKED(FStateTreeAnyEnum, Value));
			}
			if (bTargetIsAnyEnum)
			{
				ModifiedBinding.TargetPath.Path.Add(GET_MEMBER_NAME_STRING_CHECKED(FStateTreeAnyEnum, Value));
			}
			OutBindings.Add(ModifiedBinding);
		}
		else
		{
			OutBindings.Add(Binding);
		}

		// Check if the bindings is for struct ref and validate the types.
		if (!ValidateStructRef(SourceStruct, Binding.SourcePath, TargetStruct, Binding.TargetPath))
		{
			return false;
		}
	}


	auto IsPropertyBound = [&OutBindings](const FString& PropertyName)
	{
		return OutBindings.ContainsByPredicate([&PropertyName](const FStateTreeEditorPropertyBinding& Binding)
			{
				// We're looping over just the first level of properties on the struct, so we assume that the path is just one item
				// (or two in case of AnyEnum, because we expand the path to Property.Value, see code above).
				return Binding.TargetPath.Path.Num() >= 1 && Binding.TargetPath.Path[0] == PropertyName;
			});
	};

	bool bResult = true;
	
	// Validate that Input and Context bindings
	for (TFieldIterator<FProperty> It(TargetStruct.Struct, EFieldIterationFlags::None); It; ++It)
	{
		const FProperty* Property = *It;
		check(Property);
		const FString PropertyName = Property->GetName();
		const EStateTreePropertyUsage Usage = UE::StateTree::Compiler::GetUsageFromMetaData(Property);
		if (Usage == EStateTreePropertyUsage::Input)
		{
			const bool bIsOptional = Property->HasMetaData(TEXT("Optional"));
			
			// Make sure that an Input property is bound unless marked optional.
			if (bIsOptional == false && !IsPropertyBound(PropertyName))
			{
				Log.Reportf(EMessageSeverity::Error, TargetStruct,
					TEXT("Input property '%s:%s' is expected to have a binding."),
					*TargetStruct.Name.ToString(), *PropertyName);
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
					TEXT("Context property '%s:%s' type is expected to be Object Reference or Struct."),
					*TargetStruct.Name.ToString(), *PropertyName);
				bResult = false;
				continue;
			}

			const bool bIsBound = IsPropertyBound(PropertyName);

			if (!bIsBound)
			{
				const FStateTreeBindableStructDesc Desc = EditorData->FindContextData(ContextObjectType, PropertyName);

				if (Desc.IsValid())
				{
					// Add automatic binding to Context data.
					FStateTreeEditorPropertyBinding Binding;
					Binding.SourcePath.StructID = Desc.ID;

					Binding.TargetPath.StructID = TargetStruct.ID;
					Binding.TargetPath.Path.Add(PropertyName);

					OutBindings.Add(Binding);
				}
				else
				{
					Log.Reportf(EMessageSeverity::Error, TargetStruct,
						TEXT("Cound not find matching Context object for Context property '%s:%s'. Property must have munual binding."),
						*TargetStruct.Name.ToString(), *PropertyName);
					bResult = false;
				}
			}
		}
	}

	return bResult;
}
