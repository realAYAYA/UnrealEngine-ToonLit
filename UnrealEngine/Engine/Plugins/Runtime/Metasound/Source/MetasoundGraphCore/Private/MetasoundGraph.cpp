// Copyright Epic Games, Inc. All Rights Reserved.
#include "MetasoundGraph.h"

#include "MetasoundOperatorBuilder.h"

namespace Metasound
{
	FGraph::FGraph(const FString& InInstanceName, const FGuid& InInstanceID)
	: InstanceName(InInstanceName)
	, InstanceID(InInstanceID)
	, Metadata(FNodeClassMetadata::GetEmpty())
	{
	}

	const FVertexName& FGraph::GetInstanceName() const
	{
		return InstanceName;
	}

	const FGuid& FGraph::GetInstanceID() const
	{
		return InstanceID;
	}

	const FNodeClassMetadata& FGraph::GetMetadata() const
	{
		return Metadata;
	}

	bool FGraph::AddInputDataDestination(const INode& InNode, const FVertexName& InVertexName)
	{
		if (!InNode.GetVertexInterface().ContainsInputVertex(InVertexName))
		{
			return false;
		}

		FInputDataDestination Destination(InNode, InNode.GetVertexInterface().GetInputVertex(InVertexName));

		AddInputDataDestination(Destination);
		
		return true;
	}

	void FGraph::AddInputDataDestination(const FInputDataDestination& InDestination)
	{
		Metadata.DefaultInterface.GetInputInterface().Add(InDestination.Vertex);
		InputDestinations.Add(MakeDestinationDataVertexKey(InDestination), InDestination);
	}

	const FInputDataDestinationCollection& FGraph::GetInputDataDestinations() const
	{
		return InputDestinations;
	}

	bool FGraph::AddOutputDataSource(const INode& InNode, const FVertexName& InVertexName)
	{
		if (!InNode.GetVertexInterface().ContainsOutputVertex(InVertexName))
		{
			return false;
		}

		FOutputDataSource Source(InNode, InNode.GetVertexInterface().GetOutputVertex(InVertexName));

		AddOutputDataSource(Source);

		return true;
	}

	void FGraph::AddOutputDataSource(const FOutputDataSource& InSource)
	{
		Metadata.DefaultInterface.GetOutputInterface().Add(InSource.Vertex);
		OutputSources.Add(MakeSourceDataVertexKey(InSource), InSource);
	}

	const FOutputDataSourceCollection& FGraph::GetOutputDataSources() const
	{
		return OutputSources;
	}

	void FGraph::AddDataEdge(const FDataEdge& InEdge)
	{
		Edges.Add(InEdge);
	}

	bool FGraph::AddDataEdge(const INode& FromNode, const FVertexName& FromKey, const INode& ToNode, const FVertexName& ToKey)
	{
		const FVertexInterface& FromVertexInterface = FromNode.GetVertexInterface();
		const FVertexInterface& ToVertexInterface = ToNode.GetVertexInterface();

		if (!FromVertexInterface.ContainsOutputVertex(FromKey))
		{
			return false;
		}

		if (!ToVertexInterface.ContainsInputVertex(ToKey))
		{
			return false;
		}

		const FOutputDataVertex& FromVertex = FromVertexInterface.GetOutputVertex(FromKey);
		const FInputDataVertex& ToVertex = ToVertexInterface.GetInputVertex(ToKey);


		if (FromVertex.DataTypeName != ToVertex.DataTypeName)
		{
			return false;
		}

		FDataEdge Edge(FOutputDataSource(FromNode, FromVertex), FInputDataDestination(ToNode, ToVertex));

		AddDataEdge(Edge);

		return true;
	}

	const TArray<FDataEdge>& FGraph::GetDataEdges() const
	{
		return Edges;
	}

	const FVertexInterface& FGraph::GetVertexInterface() const 
	{
		return Metadata.DefaultInterface;
	}


	bool FGraph::SetVertexInterface(const FVertexInterface& InInterface)
	{
		return InInterface == Metadata.DefaultInterface;
	}

	bool FGraph::IsVertexInterfaceSupported(const FVertexInterface& InInterface) const
	{
		return InInterface == Metadata.DefaultInterface;
	}

	FOperatorFactorySharedRef FGraph::GetDefaultOperatorFactory() const 
	{
		return MakeShared<FGraph::FFactory>();
	}

	TUniquePtr<IOperator> FGraph::FFactory::CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults)
	{
		const FGraph& Graph = static_cast<const FGraph&>(InParams.Node);

		FBuildGraphOperatorParams BuildParams { Graph, InParams.OperatorSettings, InParams.InputData, InParams.Environment};

		if (nullptr != InParams.Builder)
		{
			// Use the provided builder if it exists.
			return InParams.Builder->BuildGraphOperator(BuildParams, OutResults);
		}
		else
		{
			return FOperatorBuilder(FOperatorBuilderSettings::GetDefaultSettings()).BuildGraphOperator(BuildParams, OutResults);
		}
	}
}
