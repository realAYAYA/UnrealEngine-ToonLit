// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryFlowGraph.h"
#include "Util/ProgressCancel.h"
#include "Async/Async.h"
#include "Algo/Find.h"

using namespace UE::GeometryFlow;


namespace
{
	// probably should be something defined for the whole tool framework. UETOOL-2989.
	static EAsyncExecution GeometryFlowGraphAsyncExecTarget = EAsyncExecution::Thread;

	// Can't seem to use threadpool in GeometryProcessingUnitTests. GThreadPool is null.
}

FGraph::~FGraph()
{
	EvaluateLock.Lock();    // Wait for any EvaluateResult to finish and also prevent any new evals from happening while destructing
}

TSafeSharedPtr<FNode> FGraph::FindNode(FHandle NodeHandle) const
{
	const FNodeInfo* Found = AllNodes.Find(NodeHandle);
	if (!ensure(Found != nullptr))
	{
		return nullptr;
	}
	return Found->Node;
}

TSafeSharedPtr<FRWLock> FGraph::FindNodeLock(FHandle NodeHandle) const
{
	const TSafeSharedPtr<FRWLock>* Found = AllNodeLocks.Find(NodeHandle);
	if (!ensure(Found != nullptr))
	{
		return nullptr;
	}
	return *Found;
}

EGeometryFlowResult FGraph::GetInputTypeForNode(FHandle NodeHandle, FString InputName, int32& Type) const
{
	const FNodeInfo* Found = AllNodes.Find(NodeHandle);
	if (!ensure(Found != nullptr))
	{
		return EGeometryFlowResult::NodeDoesNotExist;
	}
	EGeometryFlowResult Result = Found->Node->GetInputType(InputName, Type);
	return Result;
}

EGeometryFlowResult FGraph::GetOutputTypeForNode(FHandle NodeHandle, FString OutputName, int32& Type) const
{
	const FNodeInfo* Found = AllNodes.Find(NodeHandle);
	if (!ensure(Found != nullptr))
	{
		return EGeometryFlowResult::NodeDoesNotExist;
	}
	EGeometryFlowResult Result = Found->Node->GetOutputType(OutputName, Type);
	return Result;
}


ENodeCachingStrategy FGraph::GetCachingStrategyForNode(FHandle NodeHandle) const
{
	const FNodeInfo* Found = AllNodes.Find(NodeHandle);
	if (!ensure(Found != nullptr))
	{
		return ENodeCachingStrategy::AlwaysCache;
	}
	return (Found->CachingStrategy == ENodeCachingStrategy::Default) ?
		DefaultCachingStrategy : Found->CachingStrategy;
}


EGeometryFlowResult FGraph::AddConnection(FHandle FromNode, FString FromOutput, FHandle ToNode, FString ToInput)
{
	int32 FromType = -1, ToType = -2;
	EGeometryFlowResult FromTypeResult = GetOutputTypeForNode(FromNode, FromOutput, FromType);
	if (!ensure(FromTypeResult == EGeometryFlowResult::Ok))
	{
		return FromTypeResult;
	}
	EGeometryFlowResult ToTypeResult = GetInputTypeForNode(ToNode, ToInput, ToType);
	if (!ensure(ToTypeResult == EGeometryFlowResult::Ok))
	{
		return ToTypeResult;
	}

	if (!ensure(FromType == ToType))
	{
		return EGeometryFlowResult::UnmatchedTypes;
	}

	FConnection* Found = Algo::FindByPredicate(Connections, [&](const FConnection& Conn)
	{
		return ((Conn.ToNode == ToNode) && (Conn.ToInput == ToInput));
	});
	if (Found != nullptr)
	{
		return EGeometryFlowResult::InputAlreadyConnected;
	}

	Connections.Add({ FromNode, FromOutput, ToNode, ToInput });

	return EGeometryFlowResult::Ok;
}



EGeometryFlowResult FGraph::InferConnection(FHandle FromNodeHandle, FHandle ToNodeHandle)
{
	TSafeSharedPtr<FNode> FromNode = FindNode(FromNodeHandle);
	TSafeSharedPtr<FNode> ToNode = FindNode(ToNodeHandle);
	if (FromNode == nullptr || ToNode == nullptr)
	{
		ensure(false);
		return EGeometryFlowResult::NodeDoesNotExist;
	}

	FString FromOutputName;
	FString ToInputName;
	int32 TotalMatchesFound = 0;
	FromNode->EnumerateOutputs([&](const FString& OutputName, const TUniquePtr<INodeOutput>& Output)
	{
		int32 OutputType = Output->GetDataType();
		ToNode->EnumerateInputs([&](const FString& InputName, const TUniquePtr<INodeInput>& Input)
		{
			// if input already has a connection, we cannot add another one and can skip it
			FConnection ExistingConnection;
			if (FindConnectionForInput(ToNodeHandle, InputName, ExistingConnection) != EGeometryFlowResult::Ok)
			{
				int32 InputType = Input->GetDataType();
				if (OutputType == InputType)
				{
					TotalMatchesFound++;
					FromOutputName = OutputName;
					ToInputName = InputName;
				}
			}
		});
	});
	ensure(TotalMatchesFound == 1);
	switch (TotalMatchesFound)
	{
	case 1:
		return AddConnection(FromNodeHandle, FromOutputName, ToNodeHandle, ToInputName);
	case 0:
		return EGeometryFlowResult::NoMatchesFound;
	default:
		return EGeometryFlowResult::MultipleMatchingAmbiguityFound;
	}
}

TSet<FGraph::FHandle> FGraph::GetSourceNodes() const
{
	TSet<FHandle> SourceNodes;
	for (const TMap<FHandle, FNodeInfo>::ElementType& IdNodePair : AllNodes)
	{
		if (IdNodePair.Value.Node->NodeInputs.Num() == 0)
		{
			SourceNodes.Add(FHandle{ IdNodePair.Key });
		}
	}
	return SourceNodes;
}

TSet<FGraph::FHandle> FGraph::GetNodesWithNoConnectedInputs() const
{
	TSet<FHandle> NoInputNodes;

	for (const TMap<FHandle, FNodeInfo>::ElementType& IdNodePair : AllNodes)
	{
		FHandle NodeHandle = IdNodePair.Key;
		TSafeSharedPtr<FNode> Node = IdNodePair.Value.Node;

		bool bAnyInputConnected = false;
		Node->EnumerateInputs([this, NodeHandle, &bAnyInputConnected](const FString& InputName, const TUniquePtr<INodeInput>& Input)
		{
			FConnection ConnectionOut;
			EGeometryFlowResult FindResult = FindConnectionForInput(NodeHandle, InputName, ConnectionOut);
			if (FindResult == EGeometryFlowResult::Ok)
			{
				bAnyInputConnected = true;
			}
		});

		if (!bAnyInputConnected)
		{
			NoInputNodes.Add(NodeHandle);
		}
	}

	return NoInputNodes;
}




EGeometryFlowResult FGraph::FindConnectionForInput(FHandle ToNode, FString ToInput, FConnection& ConnectionOut) const
{
	for (const FConnection& Connection : Connections)
	{
		if (Connection.ToNode == ToNode && Connection.ToInput == ToInput)
		{
			ConnectionOut = Connection;
			return EGeometryFlowResult::Ok;
		}
	}
	return EGeometryFlowResult::ConnectionDoesNotExist;
}


int32 FGraph::CountOutputConnections(FHandle FromNode, const FString& OutputName) const
{
	int32 Count = 0;
	for (const FConnection& Connection : Connections)
	{
		if (Connection.FromNode == FromNode && Connection.FromOutput == OutputName)
		{
			Count++;
		}
	}
	return Count;
}



TSafeSharedPtr<IData> FGraph::ComputeOutputData(
	FHandle NodeHandle, 
	FString OutputName, 
	TUniquePtr<FEvaluationInfo>& EvaluationInfo,
	bool bStealOutputData)
{
	TSafeSharedPtr<FNode> Node = FindNode(NodeHandle);
	check(Node);

	// figure out which upstream Connections/Inputs we need to compute this Output
	TArray<FString> Outputs;
	Outputs.Add(OutputName);
	TArray<FEvalRequirement> InputRequirements;
	Node->CollectRequirements({ OutputName }, InputRequirements);

	// this is the map of (InputName, Data) we will build up by pulling from the Connections
	FNamedDataMap DataIn;

	TArray<TFuture<void>> AsyncFutures;
	FRWLock DataInLock;

	// Collect data from those Inputs.
	// This will recursively call ComputeOutputData() on those (Node/Output) pairs
	for (int32 k = 0; k < InputRequirements.Num(); ++k)
	{
		FDataFlags DataFlags;
		const FString& InputName = InputRequirements[k].InputName;

		// find the connection for this input
		FConnection Connection;
		EGeometryFlowResult FoundResult = FindConnectionForInput(NodeHandle, InputName, Connection);

		if (FoundResult == EGeometryFlowResult::ConnectionDoesNotExist) 
		{
			TSafeSharedPtr<IData> DefaultData = Node->GetDefaultInputData(InputName);
			// TODO: Bubble this error up rather than crash
			checkf(DefaultData != nullptr, TEXT("Node \"%s\" input \"%s\" is not connected and has no default value"), *Node->GetIdentifier(), *InputName);
			DataInLock.WriteLock();
			DataIn.Add(InputName, DefaultData, DataFlags);
			DataInLock.WriteUnlock();
			continue;
		}

		ENodeCachingStrategy FromCachingStrategy = GetCachingStrategyForNode(Connection.FromNode);

		// If there is only one Connection from this upstream Output (ie to our Input), and the Node/Input 
		// can steal and transform that data, then we will do it
		int32 OutputUsageCount = CountOutputConnections(Connection.FromNode, Connection.FromOutput);
		bool bStealDataForInput = false;
		if (OutputUsageCount == 1 
			&& InputRequirements[k].InputFlags.bCanTransformInput
			&& FromCachingStrategy != ENodeCachingStrategy::AlwaysCache)
		{
			bStealDataForInput = true;
			DataFlags.bIsMutableData = true;
		}

		TFuture<void> Future = Async(GeometryFlowGraphAsyncExecTarget, 
									[this, &DataIn, &DataInLock, &InputName, DataFlags, Connection, &EvaluationInfo, bStealDataForInput] ()
		{
			// recursively fetch Data coming in to this Input via Connection
			TSafeSharedPtr<IData> UpstreamData = ComputeOutputData(Connection.FromNode, Connection.FromOutput, EvaluationInfo, bStealDataForInput);

			DataInLock.WriteLock();
			DataIn.Add(InputName, UpstreamData, DataFlags);
			DataInLock.WriteUnlock();
		});

		AsyncFutures.Emplace(MoveTemp(Future));
	}

	for (TFuture<void>& Future : AsyncFutures)
	{
		Future.Wait();
	}

	check(DataIn.GetNames().Num() == InputRequirements.Num());

	if (EvaluationInfo && EvaluationInfo->Progress && EvaluationInfo->Progress->Cancelled())
	{
		return nullptr;
	}

	// evalute node
	FNamedDataMap DataOut;
	DataOut.Add(OutputName);
	
	TSafeSharedPtr<FRWLock> NodeLock = FindNodeLock(NodeHandle);
	NodeLock->WriteLock();
	Node->Evaluate(DataIn, DataOut, EvaluationInfo);
	NodeLock->WriteUnlock();

	if (EvaluationInfo)
	{
		EvaluationInfo->CountEvaluation(Node.Get());

		if (EvaluationInfo->Progress && EvaluationInfo->Progress->Cancelled())
		{
			return nullptr;
		}
	}

	// collect (and optionally take/steal) desired Output data
	TSafeSharedPtr<IData> Result = (bStealOutputData) ? 
		Node->StealOutput(OutputName) : DataOut.FindData(OutputName);

	check(Result);
	return Result;
}





void FGraph::ConfigureCachingStrategy(ENodeCachingStrategy NewStrategy)
{
	if (NewStrategy != DefaultCachingStrategy && ensure(NewStrategy != ENodeCachingStrategy::Default) )
	{
		DefaultCachingStrategy = NewStrategy;

		// todo: clear caches if necessary?
	}
}


EGeometryFlowResult FGraph::SetNodeCachingStrategy(FHandle NodeHandle, ENodeCachingStrategy Strategy)
{
	FNodeInfo* Found = AllNodes.Find(NodeHandle);
	if (!ensure(Found != nullptr))
	{
		return EGeometryFlowResult::NodeDoesNotExist;
	}
	Found->CachingStrategy = Strategy;
	return EGeometryFlowResult::Ok;
}


FString FGraph::DebugDumpGraph(TFunction<bool(TSafeSharedPtr<FNode>)> IncludeNodeFn) const
{
	// Can be used by, e.g., https://csacademy.com/app/graph_editor/

	FString Out;

	// First, all node names
	for (const TPair<FHandle, FNodeInfo>& NodeHandleAndInfo : AllNodes)
	{
		if (!NodeHandleAndInfo.Value.Node)
		{ 
			return "Error"; 
		}
			
		if (!IncludeNodeFn(NodeHandleAndInfo.Value.Node))
		{ 
			continue; 
		}		

		FString NodeName = NodeHandleAndInfo.Value.Node->GetIdentifier();
		Out += NodeName + "\n";
	}

	// Second, connections by node name
	for (const FConnection& Connection : Connections)
	{
		FHandle FromHandle = Connection.FromNode;
		if (!AllNodes.Find(FromHandle) || !AllNodes.Find(FromHandle)->Node) 
		{ 
			return "Error"; 
		}

		TSafeSharedPtr<FNode> FromNode = AllNodes.Find(FromHandle)->Node;
		if (!IncludeNodeFn(FromNode))
		{
			continue;
		}

		FHandle ToHandle = Connection.ToNode;
		if (!AllNodes.Find(ToHandle) || !AllNodes.Find(ToHandle)->Node) 
		{ 
			return "Error"; 
		}

		TSafeSharedPtr<FNode> ToNode = AllNodes.Find(ToHandle)->Node;
		if (!IncludeNodeFn(ToNode))
		{ 
			continue; 
		}

		Out += FromNode->GetIdentifier() + " " + ToNode->GetIdentifier() + "\n";
	}

	return Out;
}
