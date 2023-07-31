// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StateTreeTypes.h"
#include "InstancedStruct.h"
#include "StateTreePropertyBindingCompiler.h"
#include "StateTreeCompilerLog.h"

class UStateTree;
class UStateTreeState;
class UStateTreeEditorData;
struct FStateTreeEditorNode;
struct FStateTreeStateLink;

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

	bool ResolveTransitionState(const UStateTreeState& SourceState, const FStateTreeStateLink& Link, FStateTreeStateHandle& OutTransitionHandle) const;
	FStateTreeStateHandle GetStateHandle(const FGuid& StateID) const;
	UStateTreeState* GetState(const FGuid& StateID);

	bool CreateStates();
	bool CreateStateRecursive(UStateTreeState& State, const FStateTreeStateHandle Parent);
	
	bool CreateEvaluators();
	bool CreateStateTasksAndParameters();
	bool CreateStateTransitions();
	
	bool CreateConditions(UStateTreeState& State, TConstArrayView<FStateTreeEditorNode> Conditions);
	bool CreateCondition(UStateTreeState& State, const FStateTreeEditorNode& CondNode, const EStateTreeConditionOperand Operand, const int8 DeltaIndent);
	bool CreateTask(UStateTreeState& State, const FStateTreeEditorNode& TaskNode);
	bool CreateEvaluator(const FStateTreeEditorNode& EvalNode);
	bool GetAndValidateBindings(const FStateTreeBindableStructDesc& TargetStruct, TArray<FStateTreeEditorPropertyBinding>& OutBindings) const;
	bool IsPropertyAnyEnum(const FStateTreeBindableStructDesc& Struct, FStateTreeEditorPropertyPath Path) const;
	bool ValidateStructRef(const FStateTreeBindableStructDesc& SourceStruct, FStateTreeEditorPropertyPath SourcePath,
							const FStateTreeBindableStructDesc& TargetStruct, FStateTreeEditorPropertyPath TargetPath) const;

	FStateTreeCompilerLog& Log;
	UStateTree* StateTree = nullptr;
	UStateTreeEditorData* EditorData = nullptr;
	TMap<FGuid, int32> IDToState;
	TArray<UStateTreeState*> SourceStates;

	TArray<FInstancedStruct> Nodes;
	TArray<FInstancedStruct> InstanceStructs;
	TArray<FInstancedStruct> SharedInstanceStructs;
	TArray<TObjectPtr<UObject>> InstanceObjects;
	TArray<TObjectPtr<UObject>> SharedInstanceObjects;
	
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

	/**
	 * Returns property usage based on the Category metadata of given property.
	 * @param Property Handle to property where value is got from.
	 * @return found usage type, or EStateTreePropertyUsage::Invalid if not found.
	 */
	EStateTreePropertyUsage GetUsageFromMetaData(const FProperty* Property);


}; // UE::StateTree::Compiler