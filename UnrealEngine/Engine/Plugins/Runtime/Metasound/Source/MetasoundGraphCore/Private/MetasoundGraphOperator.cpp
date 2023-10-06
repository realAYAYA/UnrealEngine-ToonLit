// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundGraphOperator.h"

#include "Containers/Array.h"
#include "Containers/SortedMap.h"
#include "MetasoundDataReference.h"
#include "MetasoundDataReferenceCollection.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundGraphAlgoPrivate.h"
#include "MetasoundOperatorInterface.h"

namespace Metasound
{
	namespace MetasoundGraphPrivate
	{
		// FGraphOperator does not support rebinding with new inputs or outputs. This checks
		// that underlying data pointers were not updated when bind is called on the graph
		// operator.
		//
		// In order for FGraphOperator to support rebinding with new inputs, it would need
		// to maintain an internal map of all connections in the graph in order to update
		// internal operators appropriately. It does not hold onto this data for 
		// performance reasons. 
		template<typename InterfaceDataType>
		bool IsSupportedVertexData(const InterfaceDataType& InCurrentData, const InterfaceDataType& InNewData)
		{
#if DO_CHECK
			TArray<FVertexDataState> CurrentState;
			GetVertexInterfaceDataState(InCurrentData, CurrentState);

			TArray<FVertexDataState> NewState;
			GetVertexInterfaceDataState(InNewData, NewState);

			CurrentState.Sort();
			NewState.Sort();

			TArray<FVertexDataState>::TConstIterator CurrentIter = CurrentState.CreateConstIterator();
			TArray<FVertexDataState>::TConstIterator NewIter = NewState.CreateConstIterator();

			while (CurrentIter && NewIter)
			{
				if (NewIter->VertexName == CurrentIter->VertexName)
				{
					if ((NewIter->ID != nullptr) && (NewIter->ID != CurrentIter->ID))
					{
						UE_LOG(LogMetaSound, Warning, TEXT("Cannot bind to FGraphOperator because vertex %s has mismatched data"), *(NewIter->VertexName.ToString()));
						return false;
					}
					else
					{
						NewIter++;
						CurrentIter++;
					}
				}
				else if (*NewIter < *CurrentIter)
				{
					UE_LOG(LogMetaSound, Warning, TEXT("Cannot bind to FGraphOperator because vertex %s does not exist in current vertex data"), *(NewIter->VertexName.ToString()));
					return false;
				}
				else 
				{
					// It's ok if we have an entry in the current vertex data that does not exist in the new vertex data. 
					CurrentIter++;
				}
			}

			if (NewIter)
			{
				UE_LOG(LogMetaSound, Warning, TEXT("Cannot bind to FGraphOperator because vertex %s does not exist in current vertex data"), *(NewIter->VertexName.ToString()));
				return false;
			}

			return true;
#else
			return true;
#endif // DO_CHECK
		}
	}

	FGraphOperator::FGraphOperator(TUniquePtr<DirectedGraphAlgo::FGraphOperatorData>&& InOperatorState)
	{
		using namespace DirectedGraphAlgo;

		// Append operators in order.
		for (const FOperatorID OperatorID : InOperatorState->OperatorOrder)
		{
			AppendOperator(MoveTemp(InOperatorState->OperatorMap[OperatorID].Operator));
		}

		// Copy over vertex data
		SetVertexInterfaceData(MoveTemp(InOperatorState->VertexData));
	}

	void FGraphOperator::AppendOperator(FOperatorPtr InOperator)
	{
		if (InOperator.IsValid())
		{
			bool bIsOperatorInAnyStack = false;

			if (FExecuteFunction ExecuteFunc = InOperator->GetExecuteFunction())
			{
				ExecuteStack.Emplace(*InOperator, ExecuteFunc);
				bIsOperatorInAnyStack = true;
			}

			if (FPostExecuteFunction PostExecuteFunc = InOperator->GetPostExecuteFunction())
			{
				PostExecuteStack.Emplace(*InOperator, PostExecuteFunc);
				bIsOperatorInAnyStack = true;
			}

			if (FResetFunction ResetFunc = InOperator->GetResetFunction())
			{
				ResetStack.Emplace(*InOperator, ResetFunc);
				bIsOperatorInAnyStack = true;
			}

			if (bIsOperatorInAnyStack)
			{
				ActiveOperators.Add(MoveTemp(InOperator));
			}
		}
	}

	void FGraphOperator::SetVertexInterfaceData(FVertexInterfaceData&& InVertexData)
	{
		VertexData = InVertexData;
	}

	FDataReferenceCollection FGraphOperator::GetInputs() const
	{
		return VertexData.GetInputs().ToDataReferenceCollection();
	}

	FDataReferenceCollection FGraphOperator::GetOutputs() const
	{
		return VertexData.GetOutputs().ToDataReferenceCollection();
	}

	void FGraphOperator::BindInputs(FInputVertexInterfaceData& InInputVertexData)
	{
		if (!MetasoundGraphPrivate::IsSupportedVertexData(VertexData.GetInputs(), InInputVertexData))
		{
			UE_LOG(LogMetaSound, Error, TEXT("FGraphOperator does not support rebinding with new data"));
		}

		InInputVertexData = VertexData.GetInputs();
	}

	void FGraphOperator::BindOutputs(FOutputVertexInterfaceData& InOutputVertexData)
	{
		InOutputVertexData = VertexData.GetOutputs();
	}

	IOperator::FPostExecuteFunction FGraphOperator::GetPostExecuteFunction()
	{
		return &StaticPostExecute;
	}

	void FGraphOperator::StaticPostExecute(IOperator* InOperator)
	{
		FGraphOperator* GraphOperator = static_cast<FGraphOperator*>(InOperator);
		check(GraphOperator);

		GraphOperator->PostExecute();
	}

	void FGraphOperator::Execute()
	{
		FExecuteEntry* StackPtr = ExecuteStack.GetData();
		const int32 Num = ExecuteStack.Num();
		for (int32 i = 0; i < Num; i++)
		{
			StackPtr[i].Execute();
		}
	}

	void FGraphOperator::PostExecute()
	{
		FPostExecuteEntry* StackPtr = PostExecuteStack.GetData();
		const int32 Num = PostExecuteStack.Num();
		for (int32 i = 0; i < Num; i++)
		{
			StackPtr[i].PostExecute();
		}
	}

	void FGraphOperator::Reset(const FGraphOperator::FResetParams& InParams)
	{
		FResetEntry* StackPtr = ResetStack.GetData();
		const int32 Num = ResetStack.Num();
		for (int32 i = 0; i < Num; i++)
		{
			StackPtr[i].Reset(InParams);
		}
	}

	FGraphOperator::FExecuteEntry::FExecuteEntry(IOperator& InOperator, FExecuteFunction InFunc)
	: Operator(&InOperator)
	, Function(InFunc)
	{
		check(Function);
	}

	void FGraphOperator::FExecuteEntry::Execute()
	{
		Function(Operator);
	}

	FGraphOperator::FPostExecuteEntry::FPostExecuteEntry(IOperator& InOperator, FPostExecuteFunction InFunc)
	: Operator(&InOperator)
	, Function(InFunc)
	{
		check(Function);
	}

	void FGraphOperator::FPostExecuteEntry::PostExecute()
	{
		Function(Operator);
	}

	FGraphOperator::FResetEntry::FResetEntry(IOperator& InOperator, FResetFunction InFunc)
	: Operator(&InOperator)
	, Function(InFunc)
	{
		check(Function);
	}

	void FGraphOperator::FResetEntry::Reset(const FGraphOperator::FResetParams& InParams)
	{
		Function(Operator, InParams);
	}


}
