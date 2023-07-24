// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundInputNode.h"

#include "MetasoundDataReference.h"
#include "MetasoundVertex.h"
#include "MetasoundVertexData.h"
#include "UObject/NameTypes.h"

namespace Metasound
{
	namespace MetasoundInputNodePrivate
	{
		FDataReferenceCollection FInputOperatorBase::GetInputs() const
		{
			// This is slated to be deprecated and removed.
			checkNoEntry();
			return {};
		}

		FDataReferenceCollection FInputOperatorBase::GetOutputs() const
		{
			// This is slated to be deprecated and removed.
			checkNoEntry();
			return {};
		}

		FNonExecutableInputOperatorBase::FNonExecutableInputOperatorBase(const FVertexName& InVertexName, FAnyDataReference&& InDataRef)
		: VertexName(InVertexName)
		, DataRef(MoveTemp(InDataRef))
		{
		}

		void FNonExecutableInputOperatorBase::Bind(FVertexInterfaceData& InVertexData) const
		{
			InVertexData.GetInputs().BindVertex(VertexName, DataRef);
			InVertexData.GetOutputs().BindVertex(VertexName, DataRef);
		}

		IOperator::FExecuteFunction FNonExecutableInputOperatorBase::GetExecuteFunction()
		{
			return nullptr;
		}
	}
}
