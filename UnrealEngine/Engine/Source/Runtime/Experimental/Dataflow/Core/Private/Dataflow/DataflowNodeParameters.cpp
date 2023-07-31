// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowNodeParameters.h"
#include "Dataflow/DataflowInputOutput.h"

namespace Dataflow
{
	bool FContextSingle::Evaluate(const FDataflowOutput& Connection)
	{
		return Connection.EvaluateImpl(*this);
	}

	bool FContextThreaded::Evaluate(const FDataflowOutput& Connection)
	{
		Connection.OutputLock->Lock(); ON_SCOPE_EXIT { Connection.OutputLock->Unlock(); };
		return Connection.EvaluateImpl(*this);
	}
}

