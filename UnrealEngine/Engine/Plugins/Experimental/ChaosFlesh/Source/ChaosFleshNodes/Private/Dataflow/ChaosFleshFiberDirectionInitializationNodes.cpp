// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/ChaosFleshFiberDirectionInitializationNodes.h"

#include "Dataflow/DataflowInputOutput.h"
#include "Dataflow/DataflowNodeFactory.h"

namespace Dataflow
{
	void ChaosFleshFiberDirectionInitializationNodes()
	{
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FGenerateFiberDirectionsDataflowNode);
	}
}

void FGenerateFiberDirectionsDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<DataType>(&Collection))
	{
		DataType InCollection = GetValue<DataType>(Context, &Collection);

		//
		// @todo(dataflow) : Implemention fiber direction
		//

		SetValue(Context, MoveTemp(InCollection), &Collection);
	}
}

