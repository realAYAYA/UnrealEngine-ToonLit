// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowInputOutput.h"

#include "ChaosLog.h"
#include "Dataflow/DataflowNodeParameters.h"
#include "Dataflow/DataflowNode.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DataflowInputOutput)


FDataflowInput FDataflowInput::NoOpInput = FDataflowInput();
FDataflowOutput FDataflowOutput::NoOpOutput = FDataflowOutput();


FDataflowInput::FDataflowInput(const Dataflow::FInputParameters& Param, FGuid InGuid)
	: FDataflowConnection(Dataflow::FPin::EDirection::INPUT, Param.Type, Param.Name, Param.Owner, Param.Property, InGuid)
	, Connection(nullptr)
{
}



bool FDataflowInput::AddConnection(FDataflowConnection* InOutput)
{
	if (ensure(InOutput->GetType() == this->GetType()))
	{
		Connection = (FDataflowOutput*)InOutput;
		GetOwningNode()->Invalidate();
		return true;
	}
	return false;
}

bool FDataflowInput::RemoveConnection(FDataflowConnection* InOutput)
{
	if (ensure(Connection == (FDataflowOutput*)InOutput))
	{
		Connection = nullptr;
		GetOwningNode()->Invalidate();
		return true;
	}
	return false;
}

TArray< FDataflowOutput* > FDataflowInput::GetConnectedOutputs()
{
	TArray<FDataflowOutput* > RetList;
	if (FDataflowOutput* Conn = GetConnection())
	{
		RetList.Add(Conn);
	}
	return RetList;
}

const TArray< const FDataflowOutput* > FDataflowInput::GetConnectedOutputs() const
{
	TArray<const FDataflowOutput* > RetList;
	if (const FDataflowOutput* Conn = GetConnection())
	{
		RetList.Add(Conn);
	}
	return RetList;
}

void FDataflowInput::Invalidate(const Dataflow::FTimestamp& ModifiedTimestamp)
{
	OwningNode->Invalidate(ModifiedTimestamp);
}

//
//
//  Output
//
//
//


FDataflowOutput::FDataflowOutput(const Dataflow::FOutputParameters& Param, FGuid InGuid)
	: FDataflowConnection(Dataflow::FPin::EDirection::OUTPUT, Param.Type, Param.Name, Param.Owner, Param.Property, InGuid)
{
	OutputLock = MakeShared<FCriticalSection>();
}

const TArray<FDataflowInput*>& FDataflowOutput::GetConnections() const { return Connections; }
TArray<FDataflowInput*>& FDataflowOutput::GetConnections() { return Connections; }

const TArray< const FDataflowInput*> FDataflowOutput::GetConnectedInputs() const
{
	TArray<const FDataflowInput*> RetList;
	RetList.Reserve(Connections.Num());
	for (FDataflowInput* Ptr : Connections) 
	{ 
		RetList.Add(Ptr); 
	}
	return RetList;
}

TArray< FDataflowInput*> FDataflowOutput::GetConnectedInputs()
{
	TArray<FDataflowInput*> RetList;
	RetList.Reserve(Connections.Num());
	for (FDataflowInput* Ptr : Connections) 
	{ 
		RetList.Add(Ptr); 
	}
	return RetList;
}

bool FDataflowOutput::AddConnection(FDataflowConnection* InOutput)
{
	if (ensure(InOutput->GetType() == this->GetType()))
	{
		Connections.Add((FDataflowInput*)InOutput);
		return true;
	}
	return false;
}

bool FDataflowOutput::RemoveConnection(FDataflowConnection* InInput)
{
	Connections.RemoveSwap((FDataflowInput*)InInput); return true;
}

void FDataflowOutput::Invalidate(const Dataflow::FTimestamp& ModifiedTimestamp)
{
	for (FDataflowConnection* Con : GetConnections())
	{
		Con->Invalidate(ModifiedTimestamp);
	}
}

bool FDataflowOutput::EvaluateImpl(Dataflow::FContext& Context) const
{
	Dataflow::FContextScopedCallstack Callstack(Context, this);
	if (Callstack.IsLoopDetected())
	{ 
		ensureMsgf(false, TEXT("Connection %s is already in the callstack, this is certainly because of a loop in the graph"), *GetName().ToString());
		return false;
	}

	// check if the cache has a valid version
	if(Context.HasData(CacheKey(), OwningNode->LastModifiedTimestamp))
	{
		return true;
	}
	// if not, evaluate
	OwningNode->Evaluate(Context, this);
	// Validation
	if (!Context.HasData(CacheKey()))
	{
		ensureMsgf(false, TEXT("Failed to evaluate output (%s:%s)"), *OwningNode->GetName().ToString(), *GetName().ToString());
		return false;
	}

	return true;
}
