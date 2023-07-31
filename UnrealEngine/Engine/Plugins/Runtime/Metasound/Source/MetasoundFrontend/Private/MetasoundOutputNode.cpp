// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundOutputNode.h"

namespace Metasound
{
	namespace OutputNodePrivate
	{
		FOutputOperator::FOutputOperator(const FVertexName& InVertexName, const FAnyDataReference& InDataReference)
		: VertexName(InVertexName)
		, DataReference(InDataReference)
		{
		}

		FDataReferenceCollection FOutputOperator::GetInputs() const
		{
			// Slated for deprecation
			return {};
		}

		FDataReferenceCollection FOutputOperator::GetOutputs() const
		{
			// Slated for deprecation
			return {};
		}

		void FOutputOperator::Bind(FVertexInterfaceData& InVertexData) const
		{
			InVertexData.GetInputs().BindVertex(VertexName, DataReference);
			InVertexData.GetOutputs().BindVertex(VertexName, DataReference);
		}

		IOperator::FExecuteFunction FOutputOperator::GetExecuteFunction()
		{
			return nullptr;
		}
	}
}

