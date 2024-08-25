// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StateTreePropertyBindingCompiler.h"

struct FStructView;

enum class EStateTreeConditionOperand : uint8;
enum class EStateTreePropertyUsage : uint8;
struct FStateTreeDataView;
struct FStateTreeStateHandle;

class UStateTree;
class UStateTreeState;
class UStateTreeEditorData;
struct FStateTreeEditorNode;
struct FStateTreeStateLink;
struct FStateTreeNodeBase;

/**
 * Helper class to convert StateTree editor representation into a compact data.
 * Holds data needed during compiling.
 */
struct STATETREEEDITORMODULE_API FStateTreeCompiler
{
public:

	explicit FStateTreeCompiler(FStateTreeCompilerLog& InLog)
		: Log(InLog)
	{
	}
	
	bool Compile(UStateTree& InStateTree);
	
private:

	/** Resolves the state a transition points to. SourceState is nullptr for global tasks. */
	bool ResolveTransitionState(const UStateTreeState* SourceState, const FStateTreeStateLink& Link, FStateTreeStateHandle& OutTransitionHandle) const;
	FStateTreeStateHandle GetStateHandle(const FGuid& StateID) const;
	UStateTreeState* GetState(const FGuid& StateID) const;

	bool CreateStates();
	bool CreateStateRecursive(UStateTreeState& State, const FStateTreeStateHandle Parent);
	
	bool CreateEvaluators();
	bool CreateGlobalTasks();
	bool CreateStateTasksAndParameters();
	bool CreateStateTransitions();
	
	bool CreateConditions(UStateTreeState& State, TConstArrayView<FStateTreeEditorNode> Conditions);
	bool CreateCondition(UStateTreeState& State, const FStateTreeEditorNode& CondNode, const EStateTreeConditionOperand Operand, const int8 DeltaIndent);
	bool CreateTask(UStateTreeState* State, const FStateTreeEditorNode& TaskNode, const FStateTreeDataHandle TaskDataHandle);
	bool CreateEvaluator(const FStateTreeEditorNode& EvalNode, const FStateTreeDataHandle EvalDataHandle);
	bool GetAndValidateBindings(const FStateTreeBindableStructDesc& TargetStruct, FStateTreeDataView TargetValue, TArray<FStateTreePropertyPathBinding>& OutCopyBindings, TArray<FStateTreePropertyPathBinding>& OutReferenceBindings) const;
	bool IsPropertyOfType(UScriptStruct& Type, const FStateTreeBindableStructDesc& Struct, FStateTreePropertyPath Path) const;
	bool ValidateStructRef(const FStateTreeBindableStructDesc& SourceStruct, FStateTreePropertyPath SourcePath,
							const FStateTreeBindableStructDesc& TargetStruct, FStateTreePropertyPath TargetPath) const;
	bool CompileAndValidateNode(const UStateTreeState* SourceState, const FStateTreeBindableStructDesc& NodeDesc, FStructView NodeView, const FStateTreeDataView InstanceData);

	void InstantiateStructSubobjects(FStructView Struct);
	
	FStateTreeCompilerLog& Log;
	UStateTree* StateTree = nullptr;
	UStateTreeEditorData* EditorData = nullptr;
	TMap<FGuid, int32> IDToNode;
	TMap<FGuid, int32> IDToState;
	TMap<FGuid, int32> IDToTransition;
	TMap<FGuid, const FStateTreeDataView > IDToStructValue;
	TArray<UStateTreeState*> SourceStates;

	TArray<FInstancedStruct> Nodes;
	TArray<FInstancedStruct> InstanceStructs;
	TArray<FInstancedStruct> SharedInstanceStructs;
	
	FStateTreePropertyBindingCompiler BindingsCompiler;
};


namespace UE::StateTree::Compiler
{
	struct FValidationResult
	{
		FValidationResult() = default;
		FValidationResult(const bool bInResult, const int32 InValue, const int32 InMaxValue) : bResult(bInResult), Value(InValue), MaxValue(InMaxValue) {}

		/** Validation succeeded */
		bool DidSucceed() const { return bResult == true; }

		/** Validation failed */
		bool DidFail() const { return bResult == false; }

		/**
		 * Logs common validation for IsValidIndex16(), IsValidIndex8(), IsValidCount16(), IsValidCount8().
		 * @param Log reference to the compiler log.
		 * @param ContextText Text identifier for the context where the test is done.
		 * @param ContextStruct Struct identifier for the context where the test is done.
		 */
		void Log(FStateTreeCompilerLog& Log, const TCHAR* ContextText, const FStateTreeBindableStructDesc& ContextStruct = FStateTreeBindableStructDesc()) const;
		
		bool bResult = true;
		int32 Value = 0;
		int32 MaxValue = 0;
	};

	/**
	 * Checks if given index can be represented as uint16, including MAX_uint16 as INDEX_NONE.
	 * @param Index Index to test
	 * @return validation result.
	 */
	inline FValidationResult IsValidIndex16(const int32 Index)
	{
		return FValidationResult(Index == INDEX_NONE || (Index >= 0 && Index < MAX_uint16), Index, MAX_uint16 - 1);
	}

	/**
	 * Checks if given index can be represented as uint8, including MAX_uint8 as INDEX_NONE. 
	 * @param Index Index to test
	 * @return true if the index is valid.
	 */
	inline FValidationResult IsValidIndex8(const int32 Index)
	{
		return FValidationResult(Index == INDEX_NONE || (Index >= 0 && Index < MAX_uint8), Index, MAX_uint8 - 1);
	}

	/**
	 * Checks if given count can be represented as uint16. 
	 * @param Count Count to test
	 * @return true if the count is valid.
	 */
	inline FValidationResult IsValidCount16(const int32 Count)
	{
		return FValidationResult(Count >= 0 && Count <= MAX_uint16, Count, MAX_uint16);
	}

	/**
	 * Checks if given count can be represented as uint8. 
	 * @param Count Count to test
	 * @return true if the count is valid.
	 */
	inline FValidationResult IsValidCount8(const int32 Count)
	{
		return FValidationResult(Count >= 0 && Count <= MAX_uint8, Count, MAX_uint8);
	}

	/**
	 * Returns UScriptStruct defined in "BaseStruct" metadata of given property.
	 * @param Property Handle to property where value is got from.
	 * @param OutBaseStructName Handle to property where value is got from.
	 * @return Script struct defined by the BaseStruct or nullptr if not found.
	 */
	const UScriptStruct* GetBaseStructFromMetaData(const FProperty* Property, FString& OutBaseStructName);

}; // UE::StateTree::Compiler

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "StateTreeCompilerLog.h"
#endif
