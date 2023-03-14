// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GeometryFlowNode.h"
#include "Templates/TypeHash.h"
#include "Util/ProgressCancel.h"

class FProgressCancel;

namespace UE
{
namespace GeometryFlow
{



// TODO: this needs more work
	// need option to opportunistically cache, ie on multi-use of output, or when we can't re-use anyway
	//   (this is currently what NeverCache does?)

enum class ENodeCachingStrategy
{
	Default = 0,
	AlwaysCache = 1,
	NeverCache = 2
};


//
// TODO: 
// - internal FNode pointers can be unique ptr?
// - parallel evaluation at graph level (pre-pass to collect references?)
//


class GEOMETRYFLOWCORE_API FGraph
{
public:

	~FGraph();

	struct FHandle
	{
		static const int32 InvalidHandle = -1;
		int32 Identifier = InvalidHandle;
		bool operator==(const FHandle& OtherHandle) const { return Identifier == OtherHandle.Identifier; }
	};

	friend uint32 GetTypeHash(FHandle Handle);

	struct FConnection
	{
		FHandle FromNode;
		FString FromOutput;
		FHandle ToNode;
		FString ToInput;
	};

	template<typename NodeType>
	FHandle AddNodeOfType(
		const FString& Identifier = FString(""),
		ENodeCachingStrategy CachingStrategy = ENodeCachingStrategy::Default )
	{
		FNodeInfo NewNodeInfo;
		NewNodeInfo.Node = MakeShared<NodeType, ESPMode::ThreadSafe>();
		NewNodeInfo.Node->SetIdentifier(Identifier);
		NewNodeInfo.CachingStrategy = CachingStrategy;
		FHandle Handle = { NodeCounter++ };
		AllNodes.Add(Handle, NewNodeInfo);

		AllNodeLocks.Add(Handle, MakeSafeShared<FRWLock>());
		return Handle;
	}

	EGeometryFlowResult AddConnection(FHandle FromNode, FString FromOutput, FHandle ToNode, FString ToInput);

	EGeometryFlowResult InferConnection(FHandle FromNode, FHandle ToNode);

	TSet<FHandle> GetSourceNodes() const;
	TSet<FHandle> GetNodesWithNoConnectedInputs() const;

	template<typename T>
	EGeometryFlowResult EvaluateResult(
		FHandle Node,
		FString OutputName,
		T& Storage,
		int32 StorageTypeIdentifier,
		TUniquePtr<FEvaluationInfo>& EvaluationInfo,
		bool bTryTakeResult)
	{
		EvaluateLock.Lock();
		EGeometryFlowResult Result;
		Result = EvaluateResultInternal(Node, OutputName, Storage, StorageTypeIdentifier, EvaluationInfo, bTryTakeResult);
		EvaluateLock.Unlock();
		return Result;
	}

	template<typename NodeType>
	EGeometryFlowResult ApplyToNodeOfType(
		FHandle NodeHandle, 
		TFunctionRef<void(NodeType&)> ApplyFunc)
	{
		TSafeSharedPtr<FNode> FoundNode = FindNode(NodeHandle);
		if (FoundNode)
		{
			NodeType* Value = static_cast<NodeType*>(FoundNode.Get());
			ApplyFunc(*Value);
			return EGeometryFlowResult::Ok;
		}
		return EGeometryFlowResult::NodeDoesNotExist;
	}



	void ConfigureCachingStrategy(ENodeCachingStrategy NewStrategy);
	EGeometryFlowResult SetNodeCachingStrategy(FHandle NodeHandle, ENodeCachingStrategy Strategy);

	FString DebugDumpGraph(TFunction<bool(TSafeSharedPtr<FNode>)> IncludeNodeFn) const;

protected:

	template<typename T>
	EGeometryFlowResult EvaluateResultInternal(
		FHandle Node,
		FString OutputName,
		T& Storage,
		int32 StorageTypeIdentifier,
		TUniquePtr<FEvaluationInfo>& EvaluationInfo,
		bool bTryTakeResult)
	{
		int32 OutputType;
		EGeometryFlowResult ValidOutput = GetOutputTypeForNode(Node, OutputName, OutputType);
		if (ValidOutput != EGeometryFlowResult::Ok)
		{
			return ValidOutput;
		}
		if (OutputType != StorageTypeIdentifier)
		{
			return EGeometryFlowResult::UnmatchedTypes;
		}
		if (bTryTakeResult)
		{
			TSafeSharedPtr<IData> Data = ComputeOutputData(Node, OutputName, EvaluationInfo, true);
			if (EvaluationInfo && EvaluationInfo->Progress && EvaluationInfo->Progress->Cancelled())
			{
				return EGeometryFlowResult::OperationCancelled;
			}
			Data->GiveTo(Storage, StorageTypeIdentifier);
		}
		else
		{
			TSafeSharedPtr<IData> Data = ComputeOutputData(Node, OutputName, EvaluationInfo, false);
			if (EvaluationInfo && EvaluationInfo->Progress && EvaluationInfo->Progress->Cancelled())
			{
				return EGeometryFlowResult::OperationCancelled;
			}
			Data->GetDataCopy(Storage, StorageTypeIdentifier);
		}
		return EGeometryFlowResult::Ok;
	}


	friend class FGeometryFlowExecutor;

	int32 NodeCounter = 0;

	ENodeCachingStrategy DefaultCachingStrategy = ENodeCachingStrategy::AlwaysCache;

	struct FNodeInfo
	{
		TSafeSharedPtr<FNode> Node;
		ENodeCachingStrategy CachingStrategy = ENodeCachingStrategy::Default;
	};

	TMap<FHandle, FNodeInfo> AllNodes;
	TMap<FHandle, TSafeSharedPtr<FRWLock>> AllNodeLocks;

	TSafeSharedPtr<FNode> FindNode(FHandle Handle) const;
	EGeometryFlowResult GetInputTypeForNode(FHandle NodeHandle, FString InputName, int32& Type) const;
	EGeometryFlowResult GetOutputTypeForNode(FHandle NodeHandle, FString OutputName, int32& Type) const;
	ENodeCachingStrategy GetCachingStrategyForNode(FHandle NodeHandle) const;
	TSafeSharedPtr<FRWLock> FindNodeLock(FHandle Handle) const;

	TArray<FConnection> Connections;

	EGeometryFlowResult FindConnectionForInput(FHandle ToNode, FString ToInput, FConnection& ConnectionOut) const;

	int32 CountOutputConnections(FHandle FromNode, const FString& FromOutput) const;

	TSafeSharedPtr<IData> ComputeOutputData(
		FHandle Node, 
		FString OutputName, 
		TUniquePtr<FEvaluationInfo>& EvaluationInfo,
		bool bStealOutputData = false);

	FCriticalSection EvaluateLock;
};


inline uint32 GetTypeHash(FGraph::FHandle Handle)
{
	return ::GetTypeHash(Handle.Identifier);
}









}	// end namespace GeometryFlow
}	//
