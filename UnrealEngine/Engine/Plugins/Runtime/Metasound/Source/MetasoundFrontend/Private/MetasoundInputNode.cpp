// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundInputNode.h"

#include "MetasoundDataReference.h"
#include "MetasoundVertex.h"
#include "MetasoundVertexData.h"
#include "UObject/NameTypes.h"

namespace Metasound
{
	FDataReferenceCollection FInputValueOperator::GetInputs() const
	{
		checkNoEntry();
		return {};
	}

	FDataReferenceCollection FInputValueOperator::GetOutputs() const
	{
		checkNoEntry();
		return {};
	}

	void FInputValueOperator::Bind(FVertexInterfaceData& InVertexData) const
	{
		if (const FAnyDataReference* Ref = InVertexData.GetInputs().FindDataReference(VertexName))
		{
			checkf(EDataReferenceAccessType::Value == Ref->GetAccessType(), TEXT("Expected bound reference to have %s access. Actual access was %s"), *LexToString(EDataReferenceAccessType::Value), *LexToString(Ref->GetAccessType()));

			// Pass through input to output
			InVertexData.GetOutputs().BindVertex(VertexName, *Ref);
		}
		else
		{
			// Use stored default value
			InVertexData.GetInputs().BindVertex(VertexName, Default);
			InVertexData.GetOutputs().BindVertex(VertexName, Default);
		}
	}

	IOperator::FExecuteFunction FInputValueOperator::GetExecuteFunction()
	{
		return nullptr;
	}
}
