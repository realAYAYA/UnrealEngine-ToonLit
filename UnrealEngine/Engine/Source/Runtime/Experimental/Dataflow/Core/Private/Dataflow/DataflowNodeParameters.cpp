// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowNodeParameters.h"

#include "Dataflow/DataflowInputOutput.h"
#include "Dataflow/DataflowNode.h"


namespace Dataflow
{
	uint64 FTimestamp::Invalid = 0;
	uint64 FTimestamp::Current() { return FPlatformTime::Cycles64();  }

	void FContext::PushToCallstack(const FDataflowConnection* Connection) 
	{
#if DATAFLOW_EDITOR_EVALUATION
		Callstack.Push(Connection); 
#endif
	}

	void FContext::PopFromCallstack(const FDataflowConnection* Connection)
	{ 
#if DATAFLOW_EDITOR_EVALUATION
		ensure(Connection == Callstack.Top());
		Callstack.Pop();
#endif
	}

	bool FContext::IsInCallstack(const FDataflowConnection* Connection) const
	{
#if DATAFLOW_EDITOR_EVALUATION
		return Callstack.Contains(Connection);
#else
		return false;
#endif
	}

	FContextScopedCallstack::FContextScopedCallstack(FContext& InContext, const FDataflowConnection* InConnection)
		: Context(InContext)
		, Connection(InConnection)
	{
		bLoopDetected = Context.IsInCallstack(Connection);
		Context.PushToCallstack(Connection);
	}

	FContextScopedCallstack::~FContextScopedCallstack()
	{
		Context.PopFromCallstack(Connection);
	}

	void BeginContextEvaluation(FContext& Context, const FDataflowNode* Node, const FDataflowOutput* Output)
	{
		if (Node != nullptr)
		{
			Context.Timestamp = FTimestamp(FPlatformTime::Cycles64());
			if (Node->NumOutputs())
			{
				if (Output)
				{
					Node->Evaluate(Context, Output);
				}
				else
				{
					for (FDataflowOutput* NodeOutput : Node->GetOutputs())
					{
						Node->Evaluate(Context, NodeOutput);
					}
				}
			}
			else
			{
				Node->Evaluate(Context, nullptr);
			}
		}
	}
	
	void FContextSingle::Evaluate(const FDataflowNode* Node, const FDataflowOutput* Output)
	{
		BeginContextEvaluation(*this, Node, Output);
	}
		
	bool FContextSingle::Evaluate(const FDataflowOutput& Connection)
	{
		return Connection.EvaluateImpl(*this);
	}



	void FContextThreaded::Evaluate(const FDataflowNode* Node, const FDataflowOutput* Output)
	{
		BeginContextEvaluation(*this, Node, Output);
	}

	bool FContextThreaded::Evaluate(const FDataflowOutput& Connection)
	{
		Connection.OutputLock->Lock(); ON_SCOPE_EXIT { Connection.OutputLock->Unlock(); };
		return Connection.EvaluateImpl(*this);
	}
}

