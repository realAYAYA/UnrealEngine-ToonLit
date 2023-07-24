// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundOperatorBuilder.h"

#include "Algo/Count.h"
#include "Algo/Transform.h"
#include "MetasoundBuildError.h"
#include "MetasoundBuilderInterface.h"
#include "MetasoundBuildError.h"
#include "MetasoundGraphAlgo.h"
#include "MetasoundGraphLinter.h"
#include "MetasoundGraphOperator.h"
#include "MetasoundOperatorInterface.h"
#include "MetasoundThreadLocalDebug.h"
#include "Templates/PimplPtr.h"


namespace Metasound
{
	namespace OperatorBuilderPrivate
	{
		using FBuildErrorPtr = TUniquePtr<IOperatorBuildError>;

		// Convenience function for adding graph cycle build errors
		void AddBuildErrorsForCycles(const FDirectedGraphAlgoAdapter& InAdapter, TArray<FBuildErrorPtr>& OutErrors)
		{
			if (FGraphLinter::ValidateNoCyclesInGraph(InAdapter, OutErrors))
			{
				AddBuildError<FInternalError>(OutErrors, __FILE__, __LINE__);
			}
		}

	}

	FOperatorBuilder::FOperatorBuilder(const FOperatorBuilderSettings& InBuilderSettings)
	: BuilderSettings(InBuilderSettings)
	{
	}

	FOperatorBuilder::~FOperatorBuilder()
	{
	}

	TUniquePtr<IOperator> FOperatorBuilder::BuildGraphOperator(const FBuildGraphOperatorParams& InParams, FBuildResults& OutResults) const
	{
		using namespace OperatorBuilderPrivate;

		FBuildStatus BuildStatus = FBuildStatus::NoError;

		// Validate that the sources and destinations declared in an edge actually
		// exist in the node.
		if (BuilderSettings.bValidateVerticesExist)
		{
			if (!FGraphLinter::ValidateVerticesExist(InParams.Graph, OutResults.Errors))
			{
				BuildStatus |= FBuildStatus::FatalError;
			}
		}

		// Validate that the data types for a source and destination match.
		if (BuilderSettings.bValidateEdgeDataTypesMatch)
		{
			if (!FGraphLinter::ValidateEdgeDataTypesMatch(InParams.Graph, OutResults.Errors))
			{
				BuildStatus |= FBuildStatus::FatalError;
			}
		}
		
		// Validate that node inputs only have one source
		if (BuilderSettings.bValidateNoDuplicateInputs)
		{
			if (!FGraphLinter::ValidateNoDuplicateInputs(InParams.Graph, OutResults.Errors))
			{
				BuildStatus |= FBuildStatus::FatalError;
			}
		}

		// Possible early exit if edge validation fails.
		if (BuildStatus > GetMaxErrorLevel())
		{
			return TUniquePtr<IOperator>(nullptr);
		}

		// Create algo adapter view of graph to cache graph operations.
		TPimplPtr<FDirectedGraphAlgoAdapter> AlgoAdapter = FDirectedGraphAlgo::CreateDirectedGraphAlgoAdapter(InParams.Graph);
		
		if (!AlgoAdapter.IsValid())
		{
			AddBuildError<FInternalError>(OutResults.Errors, __FILE__, __LINE__);
			return TUniquePtr<IOperator>(nullptr);
		}

		FBuildContext BuildContext(InParams.Graph, *AlgoAdapter, InParams.OperatorSettings, InParams.Environment, BuilderSettings, OutResults);

		TArray<const INode*> SortedNodes;

		// Sort the nodes in a valid execution order
		BuildStatus |= DepthFirstTopologicalSort(BuildContext, SortedNodes);

		// TODO: Add FindReachableNodesFromVariables in Prune.
		// Otherwise, subgraphs incorrectly get pruned.
		// BuildStatus |= PruneNodges(BuildContext, SortedNodes);

		// Check build status in case build routine should be exited early.
		if (BuildStatus > GetMaxErrorLevel())
		{
			return TUniquePtr<IOperator>(nullptr);
		}

		// Create node operators from factories.
		BuildStatus |= CreateOperators(BuildContext, SortedNodes, InParams.InputData);

		if (BuildStatus > GetMaxErrorLevel())
		{
			return TUniquePtr<IOperator>(nullptr);
		}

		// Create graph operator from collection of node operators.
		return CreateGraphOperator(BuildContext);
	}

	FOperatorBuilder::FBuildStatus FOperatorBuilder::DepthFirstTopologicalSort(FBuildContext& InOutContext, TArray<const INode*>& OutNodes) const
	{
		using namespace OperatorBuilderPrivate;

		bool bSuccess = FDirectedGraphAlgo::DepthFirstTopologicalSort(InOutContext.AlgoAdapter, OutNodes);

		if (!bSuccess)
		{
			// If there was an error, there is likely a cycle in the graph.
			AddBuildErrorsForCycles(InOutContext.AlgoAdapter, InOutContext.Results.Errors);

			return FBuildStatus::FatalError;
		}

		return FBuildStatus::NoError;
	}

	FOperatorBuilder::FBuildStatus FOperatorBuilder::KahnsTopologicalSort(FBuildContext& InOutContext, TArray<const INode*>& OutNodes) const
	{
		using namespace OperatorBuilderPrivate;

		bool bSuccess = FDirectedGraphAlgo::KahnTopologicalSort(InOutContext.AlgoAdapter, OutNodes);

		if (!bSuccess)
		{
			// If there was an error, there is likely a cycle in the graph.
			AddBuildErrorsForCycles(InOutContext.AlgoAdapter, InOutContext.Results.Errors);

			return FBuildStatus::FatalError;
		}

		return FBuildStatus::NoError;
	}

	FOperatorBuilder::FBuildStatus FOperatorBuilder::PruneNodes(FBuildContext& InOutContext, TArray<const INode*>& InOutNodes) const
	{
		using namespace OperatorBuilderPrivate;

		FBuildStatus BuildStatus = FBuildStatus::NoError;

		TSet<const INode*> ReachableNodes;

		switch (BuilderSettings.PruningMode)
		{
			case EOperatorBuilderNodePruning::PruneNodesWithoutExternalDependency:
				FDirectedGraphAlgo::FindReachableNodes(InOutContext.AlgoAdapter, ReachableNodes);
				break;

			case EOperatorBuilderNodePruning::PruneNodesWithoutOutputDependency:
				FDirectedGraphAlgo::FindReachableNodesFromOutput(InOutContext.AlgoAdapter, ReachableNodes);
				break;

			case EOperatorBuilderNodePruning::PruneNodesWithoutInputDependency:
				FDirectedGraphAlgo::FindReachableNodesFromInput(InOutContext.AlgoAdapter, ReachableNodes);
				break;

			case EOperatorBuilderNodePruning::None:
			default:
				return FBuildStatus::NoError;
		}

		if (InOutNodes.Num() == ReachableNodes.Num())
		{
			// Nothing to remove since all nodes are reachable. It's assumed that
			// InOutNodes has a unique set of nodes. 
			return FBuildStatus::NoError;
		}

		if (0 == ReachableNodes.Num())
		{
			// Pruning all nodes. 
			for (const INode* Node : InOutNodes)
			{
				AddBuildError<FNodePrunedError>(InOutContext.Results.Errors, Node);
			}

			InOutNodes.Reset();

			// This is non fatal, but results in an IOperator which is a No-op.
			return FBuildStatus::NonFatalError;
		}

		// Split the nodes-to-keep and the nodes-to-prune into two arrays. Need 
		// to ensure that kept nodes are still in same relative order. 
		TArray<const INode*> SortedNodesToKeep;

		SortedNodesToKeep.Reserve(ReachableNodes.Num());

		for (const INode* Node : InOutNodes)
		{
			if (ReachableNodes.Contains(Node))
			{
				SortedNodesToKeep.Add(Node);
			}
			else
			{
				AddBuildError<FNodePrunedError>(InOutContext.Results.Errors, Node);

				// Denote a pruned node as a non-fatal error. In the future this
				// may be simply a warning as some nodes are required to conform
				// to metasound interfaces even if they are unused.
				BuildStatus |= FBuildStatus::NonFatalError;
			}
		}

		InOutNodes = SortedNodesToKeep;

		return BuildStatus;
	}

	FOperatorBuilder::FBuildStatus FOperatorBuilder::GatherInputDataReferences(FBuildContext& InOutContext, const INode* InNode, const FNodeEdgeMultiMap& InEdgeMap, FInputVertexInterfaceData& OutVertexData) const
	{
		using namespace OperatorBuilderPrivate;

		FBuildStatus BuildStatus;

		// Find all input edges associated with this node. 
		TArray<const FDataEdge*> Edges;
		InEdgeMap.MultiFind(InNode, Edges);

		// Add parameters to collection based on edge description
		for (const FDataEdge* Edge : Edges)
		{
			if (!InOutContext.DataReferences.Contains(Edge->From.Node))
			{
				// This is likely due to a failed topological sort and is more of an internal error
				// than a user error.
				AddBuildError<FInternalError>(InOutContext.Results.Errors, TEXT(__FILE__), __LINE__);

				return FBuildStatus::NonFatalError;
			}

			const FOutputVertexInterfaceData& FromData = InOutContext.DataReferences[Edge->From.Node].GetOutputs();

			if (const FAnyDataReference* DataReference = FromData.FindDataReference(Edge->From.Vertex.VertexName))
			{
				check(DataReference->GetDataTypeName() == Edge->From.Vertex.DataTypeName);
				check(DataReference->GetDataTypeName() == Edge->To.Vertex.DataTypeName);
				OutVertexData.BindVertex(Edge->To.Vertex.VertexName, FAnyDataReference{*DataReference});
			}
			else 
			{
				// Does not contain any reference
				// This is likely a node programming error where the edges reported by the INode interface
				// did not match the readable parameter refs created by the operators outputs. Or, the edge description is invalid.

				AddBuildError<FMissingOutputDataReferenceError>(InOutContext.Results.Errors, Edge->From);

				BuildStatus |= FBuildStatus::NonFatalError;
				continue;
			}
		}

		return BuildStatus;
	}

	FOperatorBuilder::FBuildStatus FOperatorBuilder::GatherExternalInputDataReferences(FBuildContext& InOutContext, const INode* InNode, const FNodeDestinationMap& InNodeDestinationMap, const FInputVertexInterfaceData& InExternalInputData, FInputVertexInterfaceData& OutVertexData) const
	{
		using namespace OperatorBuilderPrivate;

		FBuildStatus BuildStatus;

		// Check if there are any graph inputs connected to the node's inputs.
		if (const FInputDataDestination* const* DestinationPtr = InNodeDestinationMap.Find(InNode))
		{
			if (const FInputDataDestination* Destination = *DestinationPtr)
			{
				if (const FAnyDataReference* DataReference = InExternalInputData.FindDataReference(Destination->Vertex.VertexName))
				{
					if (DataReference->GetDataTypeName() == Destination->Vertex.DataTypeName)
					{
						OutVertexData.BindVertex(Destination->Vertex.VertexName, FAnyDataReference{*DataReference});
					}
					else
					{
						// Mismatch in datatypes. This likely corresponds to a corrupt 
						// Metasound Graph. The graph's inputs should route directly
						// to TInputNode<>s with matching data types. 
						
						// Create source for reporting connection since external inputs do not have nodes. 
						const FInputDataVertex& GraphVertex = InExternalInputData.GetVertex(Destination->Vertex.VertexName);

						FOutputDataSource Source;
						Source.Vertex.VertexName = GraphVertex.VertexName; 
						Source.Vertex.DataTypeName = GraphVertex.DataTypeName;
#if WITH_EDITORONLY_DATA
						Source.Vertex.Metadata = GraphVertex.Metadata;
#endif

						AddBuildError<FInvalidConnectionDataTypeError>(InOutContext.Results.Errors, FDataEdge{Source, *Destination});
					}
				}
			}
		}

		return BuildStatus;
	}

	void FOperatorBuilder::GatherInternalGraphDataReferences(FBuildContext& InOutContext, TMap<FGuid, FDataReferenceCollection>& OutNodeVertexData) const
	{
		for (TPair<const INode*, FVertexInterfaceData>& ReferencePair : InOutContext.DataReferences)
		{
			const INode* Node = ReferencePair.Key;
			check(Node);
			OutNodeVertexData.Emplace(Node->GetInstanceID(), ReferencePair.Value.GetOutputs().ToDataReferenceCollection());
		}
	}

	FOperatorBuilder::FBuildStatus FOperatorBuilder::ValidateOperatorOutputsAreBound(const INode& InNode, const FOutputVertexInterfaceData& InVertexData) const
	{
		using FOutputVertexBinding = MetasoundVertexDataPrivate::TBinding<FOutputDataVertex>;

		FBuildStatus BuildStatus = FBuildStatus::NoError;
		bool bFoundUnboundVertex = false;
		for (const FOutputVertexBinding& OutputBinding : InVertexData)
		{
			if (!OutputBinding.IsBound())
			{
				bFoundUnboundVertex = true;
				const FNodeClassMetadata& Metadata = InNode.GetMetadata();
				UE_LOG(LogMetaSound, Warning, TEXT("Operator for node %s v%d.%d contains unbound output vertex %s"), *Metadata.ClassName.GetFullName().ToString(), Metadata.MajorVersion, Metadata.MinorVersion, *OutputBinding.GetVertex().VertexName.ToString());
			}
		}

		if (bFoundUnboundVertex)
		{
			BuildStatus |= FBuildStatus::NonFatalError;
		}

		return BuildStatus;
	}

	FOperatorBuilder::FBuildStatus FOperatorBuilder::CreateOperators(FBuildContext& InOutContext, const TArray<const INode*>& InSortedNodes, const FInputVertexInterfaceData& InExternalInputData) const
	{
		FBuildStatus BuildStatus;

		FNodeEdgeMultiMap NodeInputEdges;

		// Gather input edges for each node.
		for (const FDataEdge& Edge : InOutContext.Graph.GetDataEdges())
		{
			NodeInputEdges.Add(Edge.To.Node, &Edge);
		}

		// Map input nodes to graph destinations
		FNodeDestinationMap NodeDestinations;
		for (const auto& InputDestinationKV : InOutContext.Graph.GetInputDataDestinations())
		{
			const FInputDataDestination& Destination = InputDestinationKV.Value;
			NodeDestinations.Add(Destination.Node, &Destination);
		}

		// Call operator factory for each node.
		for (const INode* Node : InSortedNodes)
		{
			ThreadLocalDebug::SetActiveNodeClass(Node->GetMetadata());

			const FVertexInterface& NodeInterface = Node->GetVertexInterface();
			FInputVertexInterfaceData InputData(NodeInterface.GetInputInterface());

			// Gather the input parameters for this IOperator from the output parameters of already created IOperators. 
			BuildStatus |= GatherInputDataReferences(InOutContext, Node, NodeInputEdges, InputData);
			// Gather the input parameters for this IOperator from the graph inputs.
			BuildStatus |= GatherExternalInputDataReferences(InOutContext, Node, NodeDestinations, InExternalInputData, InputData);

			if (BuildStatus >= FBuildStatus::FatalError)
			{
				return BuildStatus;
			}

			FOperatorFactorySharedRef Factory = Node->GetDefaultOperatorFactory();

			// This is here to handle the deprecated field FCreateOperator::InputDataReferences.
			// It can be removed once the deprecated member is removed.
			FDataReferenceCollection InputCollection = InputData.ToDataReferenceCollection();

			FBuildOperatorParams CreateParams{*Node, InOutContext.Settings, InputData, InOutContext.Environment, this};

			FOperatorPtr Operator = Factory->CreateOperator(CreateParams, InOutContext.Results);

			if (!Operator.IsValid())
			{
				return FBuildStatus::FatalError;
			}

			// Bind vertex to operator data
			FVertexInterfaceData BoundOperatorData(NodeInterface);
			Operator->Bind(BoundOperatorData);
			
			// Check if outputs are bound correctly.
			if (BuilderSettings.bValidateOperatorOutputsAreBound)
			{
				BuildStatus |= ValidateOperatorOutputsAreBound(*Node, BoundOperatorData.GetOutputs());
			}

			// Store bound interface for later use.
			InOutContext.DataReferences.Emplace(Node, MoveTemp(BoundOperatorData));

			// Add operator to operator array
			InOutContext.Operators.Add(MoveTemp(Operator));

			ThreadLocalDebug::ResetActiveNodeClass();
		}

		return BuildStatus;
	}

	FOperatorBuilder::FBuildStatus FOperatorBuilder::GatherGraphDataReferences(FBuildContext& InOutContext, FVertexInterfaceData& OutVertexData) const
	{
		using namespace OperatorBuilderPrivate;
		using FDestinationElement = FInputDataDestinationCollection::ElementType;
		using FSourceElement = FOutputDataSourceCollection::ElementType;

		FBuildStatus BuildStatus;

		// Gather graph inputs
		for (const FDestinationElement& Element : InOutContext.Graph.GetInputDataDestinations())
		{
			bool bFoundDataReference = false;
			const FInputDataDestination& InputDestination = Element.Value;

			if (const FVertexInterfaceData* VertexData = InOutContext.DataReferences.Find(InputDestination.Node))
			{
				const FInputVertexInterfaceData& NodeInputData = VertexData->GetInputs();
				if (const FAnyDataReference* DataReference = NodeInputData.FindDataReference(InputDestination.Vertex.VertexName))
				{
					if (DataReference->GetDataTypeName() == InputDestination.Vertex.DataTypeName)
					{
						bFoundDataReference = true;
						OutVertexData.GetInputs().BindVertex(InputDestination.Vertex.VertexName, *DataReference);
					}
				}
			}

			if (!bFoundDataReference)
			{
				AddBuildError<FMissingInputDataReferenceError>(InOutContext.Results.Errors, InputDestination);
				BuildStatus |= FBuildStatus::NonFatalError;
			}
		}

		// Gather graph outputs.
		for (const FSourceElement& Element : InOutContext.Graph.GetOutputDataSources())
		{
			bool bFoundDataReference = false;
			const FOutputDataSource& OutputSource = Element.Value;

			if (const FVertexInterfaceData* VertexData = InOutContext.DataReferences.Find(OutputSource.Node))
			{
				const FOutputVertexInterfaceData& NodeOutputData = VertexData->GetOutputs();
				if (const FAnyDataReference* DataReference = NodeOutputData.FindDataReference(OutputSource.Vertex.VertexName))
				{
					if (DataReference->GetDataTypeName() == OutputSource.Vertex.DataTypeName)
					{
						bFoundDataReference = true;
						OutVertexData.GetOutputs().BindVertex(OutputSource.Vertex.VertexName, FAnyDataReference{*DataReference});
					}
				}
			}

			if (!bFoundDataReference)
			{
				AddBuildError<FMissingOutputDataReferenceError>(InOutContext.Results.Errors, OutputSource);
				BuildStatus |= FBuildStatus::NonFatalError;
			}
		}
	
		return BuildStatus;
	}

	TUniquePtr<IOperator> FOperatorBuilder::CreateGraphOperator(FBuildContext& InOutContext) const
	{
		FVertexInterfaceData BoundGraphData(InOutContext.Graph.GetVertexInterface());

		FBuildStatus BuildStatus = GatherGraphDataReferences(InOutContext, BoundGraphData);

		if (BuildStatus > GetMaxErrorLevel())
		{
			return TUniquePtr<IOperator>(nullptr);
		}

		TUniquePtr<FGraphOperator> GraphOperator = MakeUnique<FGraphOperator>();

		GraphOperator->SetVertexInterfaceData(MoveTemp(BoundGraphData));
		
		if (BuilderSettings.bPopulateInternalDataReferences)
		{
			GatherInternalGraphDataReferences(InOutContext, InOutContext.Results.InternalDataReferences);
		}

		for (FOperatorPtr& Ptr : InOutContext.Operators)
		{
			GraphOperator->AppendOperator(MoveTemp(Ptr));
		}

		return MoveTemp(GraphOperator);
	}

	FOperatorBuilder::FBuildStatus::EStatus FOperatorBuilder::GetMaxErrorLevel() const
	{
		return BuilderSettings.bFailOnAnyError ? FBuildStatus::NoError : FBuildStatus::NonFatalError;
	}
}
