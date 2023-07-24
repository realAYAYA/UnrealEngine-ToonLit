// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Chaos/ChaosArchive.h"
#include "Dataflow/DataflowNode.h"
#include "Dataflow/DataflowTerminalNode.h"
#include "Serialization/Archive.h"

struct FDataflowConnection;

namespace Dataflow
{

	struct FLink {
		FGuid InputNode;
		FGuid Input;
		FGuid OutputNode;
		FGuid Output;

		FLink() {}

		FLink(FGuid InOutputNode, FGuid InOutput, FGuid InInputNode, FGuid InInput)
			: InputNode(InInputNode), Input(InInput)
			, OutputNode(InOutputNode), Output(InOutput) {}

		FLink(const FLink& Other)
			: InputNode(Other.InputNode), Input(Other.Input)
			, OutputNode(Other.OutputNode), Output(Other.Output) {}

		bool operator==(const FLink& Other) const
		{
			return Equals(Other);
		}

		bool Equals(const FLink& Other) const
		{
			return Input == Other.Input && InputNode == Other.InputNode
				&& Output == Other.Output && OutputNode == Other.OutputNode;
		}
	};


	//
	//
	//
	class DATAFLOWCORE_API FGraph
	{

		FGuid  Guid;
		TArray< TSharedPtr<FDataflowNode> > Nodes;
		TArray< TSharedPtr<FDataflowNode> > TerminalNodes;
		TArray< FLink > Connections;
		TSet< FName > DisabledNodes;
	public:
		FGraph(FGuid InGuid = FGuid::NewGuid());
		virtual ~FGraph() {}

		const TArray< TSharedPtr<FDataflowNode> >& GetTerminalNodes() const { return TerminalNodes; }
		const TArray< TSharedPtr<FDataflowNode> >& GetNodes() const { return Nodes; }
		TArray< TSharedPtr<FDataflowNode> >& GetNodes() { return Nodes; }
		int NumNodes() { return Nodes.Num(); }

		template<class T> TSharedPtr<T> AddNode(T* InNode)
		{
			TSharedPtr<T> NewNode(InNode);
			Nodes.AddUnique(NewNode);
			if (NewNode->IsA(FDataflowTerminalNode::StaticType()))
			{
				TerminalNodes.AddUnique(NewNode);
			}
			return NewNode;
		}

		template<class T> TSharedPtr<T> AddNode(TUniquePtr<T> &&InNode)
		{
			TSharedPtr<T> NewNode(InNode.Release());
			Nodes.AddUnique(NewNode);
			if (NewNode->IsA(FDataflowTerminalNode::StaticType()))
			{
				TerminalNodes.AddUnique(NewNode);
			}
			return NewNode;
		}

		TSharedPtr<FDataflowNode> FindBaseNode(FGuid InGuid)
		{
			for (TSharedPtr<FDataflowNode> Node : Nodes)
			{
				if (Node->GetGuid() == InGuid)
				{
					return Node;
				}
			}
			return TSharedPtr<FDataflowNode>(nullptr);
		}

		TSharedPtr<const FDataflowNode> FindBaseNode(FGuid InGuid) const
		{
			for (TSharedPtr<FDataflowNode> Node : Nodes)
			{
				if (Node->GetGuid() == InGuid)
				{
					return Node;
				}
			}
			return TSharedPtr<FDataflowNode>(nullptr);
		}

		TSharedPtr<FDataflowNode> FindBaseNode(FName InName)
		{
			for (TSharedPtr<FDataflowNode> Node : Nodes)
			{
				if (Node->GetName().IsEqual(InName))
				{
					return Node;
				}
			}
			return TSharedPtr<FDataflowNode>(nullptr);
		}

		TSharedPtr<const FDataflowNode> FindBaseNode(FName InName) const
		{
			for (TSharedPtr<FDataflowNode> Node : Nodes)
			{
				if (Node->GetName().IsEqual(InName))
				{
					return Node;
				}
			}
			return TSharedPtr<const FDataflowNode>(nullptr);
		}

		TSharedPtr<FDataflowNode> FindTerminalNode(FName InName)
		{
			for (TSharedPtr<FDataflowNode> Node : TerminalNodes)
			{
				if (Node->GetName().IsEqual(InName))
				{
					return Node;
				}
			}
			return TSharedPtr<FDataflowNode>(nullptr);
		}

		void RemoveNode(TSharedPtr<FDataflowNode> Node);

		void ClearConnections(FDataflowConnection* ConnectionBase);
		void ClearConnections(FDataflowInput* Input);
		void ClearConnections(FDataflowOutput* Output);

		void Connect(FDataflowOutput* OutputConnection, FDataflowInput* InputConnection);
		void Disconnect(FDataflowOutput* OutputConnection, FDataflowInput* InputConnection);

		virtual void Serialize(FArchive& Ar);
		const TSet<FName>& GetDisabledNodes() const { return DisabledNodes; }

	};
}

FORCEINLINE FArchive& operator<<(FArchive& Ar, Dataflow::FLink& Value)
{
	Ar << Value.InputNode << Value.OutputNode << Value.Input << Value.Output;
	return Ar;
}

FORCEINLINE FArchive& operator<<(Chaos::FChaosArchive& Ar, Dataflow::FLink& Value)
{
	Ar << Value.InputNode << Value.OutputNode << Value.Input << Value.Output;
	return Ar;
}

FORCEINLINE FArchive& operator<<(FArchive& Ar, Dataflow::FGraph& Value)
{
	Value.Serialize(Ar);
	return Ar;
}

FORCEINLINE FArchive& operator<<(Chaos::FChaosArchive& Ar, Dataflow::FGraph& Value)
{
	Value.Serialize(Ar);
	return Ar;
}




