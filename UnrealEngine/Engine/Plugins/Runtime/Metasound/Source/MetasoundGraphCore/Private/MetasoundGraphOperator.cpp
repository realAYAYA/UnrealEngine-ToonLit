// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundGraphOperator.h"

#include "CoreMinimal.h"
#include "MetasoundDataReference.h"
#include "MetasoundDataReferenceCollection.h"
#include "MetasoundOperatorInterface.h"
#include "MetasoundExecutableOperator.h"

namespace Metasound
{

	FGraphOperator::~FGraphOperator()
	{
	}

	void FGraphOperator::AppendOperator(FOperatorPtr InOperator)
	{
		if (InOperator.IsValid())
		{
			if (nullptr != InOperator->GetExecuteFunction())
			{
				OperatorStack.Emplace(MoveTemp(InOperator));
			}
		}
	}

	void FGraphOperator::SetInputs(const FDataReferenceCollection& InCollection)
	{
		VertexData.GetInputs().Bind(InCollection);
	}

	void FGraphOperator::SetOutputs(const FDataReferenceCollection& InCollection)
	{
		VertexData.GetOutputs().Bind(InCollection);
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

	void FGraphOperator::Bind(FVertexInterfaceData& InVertexData) const
	{
		InVertexData.Bind(VertexData);
	}

	IOperator::FExecuteFunction FGraphOperator::GetExecuteFunction()
	{
		return &FGraphOperator::ExecuteFunction;
	}

	void FGraphOperator::Execute()
	{
		FExecuter* StackPtr = OperatorStack.GetData();
		const int32 Num = OperatorStack.Num();
		for (int32 i = 0; i < Num; i++)
		{
			StackPtr[i].Execute();
		}
	}

	void FGraphOperator::ExecuteFunction(IOperator* InOperator)
	{
		FGraphOperator* Operator = static_cast<FGraphOperator*>(InOperator);

		check(nullptr != Operator);

		Operator->Execute();
	}
}
