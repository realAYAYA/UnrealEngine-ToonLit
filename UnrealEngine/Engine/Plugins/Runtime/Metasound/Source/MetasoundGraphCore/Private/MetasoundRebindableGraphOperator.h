// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetasoundExecutableOperator.h"
#include "MetasoundDynamicGraphAlgo.h"
#include "MetasoundOperatorInterface.h"

namespace Metasound
{
	class FDataReferenceCollection;
	class FInputVertexInterfaceData;
	class FOutputVertexInterfaceData;


	class FRebindableGraphOperator : public TExecutableOperator<FRebindableGraphOperator>
	{
	public:

		FRebindableGraphOperator(DirectedGraphAlgo::FGraphOperatorData&& InOperatorState);

		virtual FDataReferenceCollection GetInputs() const override;

		virtual FDataReferenceCollection GetOutputs() const override;

		// Bind the graph's interface data references to FVertexInterfaceData.
		virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override;
		virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override;

		void Execute();
		void PostExecute();
		void Reset(const IOperator::FResetParams& InParams);

		virtual IOperator::FPostExecuteFunction GetPostExecuteFunction() override;

	private:
		static void StaticPostExecute(IOperator* InOperator);

		DynamicGraph::FDynamicGraphOperatorData GraphOperatorData;
	};
}

