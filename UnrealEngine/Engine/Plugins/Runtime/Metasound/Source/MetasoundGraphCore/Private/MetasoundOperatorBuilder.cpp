// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundOperatorBuilder.h"

#include "Algo/Count.h"
#include "Algo/Transform.h"
#include "MetasoundBuildError.h"
#include "MetasoundBuilderInterface.h"
#include "MetasoundBuildError.h"
#include "MetasoundDynamicOperator.h"
#include "MetasoundDynamicOperatorTransactor.h"
#include "MetasoundGraphAlgo.h"
#include "MetasoundGraphAlgoPrivate.h"
#include "MetasoundGraphLinter.h"
#include "MetasoundGraphOperator.h"
#include "MetasoundOperatorInterface.h"
#include "MetasoundProfilingOperator.h"
#include "MetasoundRebindableGraphOperator.h"
#include "MetasoundThreadLocalDebug.h"
#include "MetasoundTrace.h"
#include "Templates/PimplPtr.h"


namespace Metasound
{
	namespace OperatorBuilder
	{
		// Shared context used in the builder to maintain state of current build.
		struct FBuildContext
		{
			const IGraph& Graph;
			const FDirectedGraphAlgoAdapter& AlgoAdapter;
			const FOperatorSettings& Settings;
			const FMetasoundEnvironment& Environment;
			
			FBuildResults& Results;
			TUniquePtr<DirectedGraphAlgo::FGraphOperatorData> GraphOperatorData;

			FBuildContext(
				const IGraph& InGraph,
				const FDirectedGraphAlgoAdapter& InAlgoAdapter,
				const FOperatorSettings& InSettings,
				const FMetasoundEnvironment& InEnvironment,
				FBuildResults& OutResults)
			: Graph(InGraph)
			, AlgoAdapter(InAlgoAdapter)
			, Settings(InSettings)
			, Environment(InEnvironment)
			, Results(OutResults)
			, GraphOperatorData(MakeUnique<DirectedGraphAlgo::FGraphOperatorData>(InSettings))
			{
				GraphOperatorData->VertexData = FVertexInterfaceData(InGraph.GetVertexInterface());
			}
		};
	}

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
		TUniquePtr<DirectedGraphAlgo::FGraphOperatorData> GraphData = BuildGraphOperatorData(InParams, OutResults);

		if (GraphData.IsValid())
		{
			// Create graph operator from collection of node operators.
			return CreateGraphOperator(MoveTemp(GraphData));
		}

		return TUniquePtr<IOperator>(nullptr);
	}

	TUniquePtr<IOperator> FOperatorBuilder::BuildDynamicGraphOperator(const FBuildDynamicGraphOperatorParams& InParams, FBuildResults& OutResults)
	{
		using namespace DynamicGraph;

		TUniquePtr<DirectedGraphAlgo::FGraphOperatorData> GraphData = BuildGraphOperatorData(InParams, OutResults);

		if (GraphData.IsValid())
		{
			return MakeUnique<FDynamicOperator>(MoveTemp(*GraphData), InParams.TransformQueue, InParams.OperatorUpdateCallbacks);
		}

		return TUniquePtr<IOperator>(nullptr);
	}

	TUniquePtr<DirectedGraphAlgo::FGraphOperatorData> FOperatorBuilder::BuildGraphOperatorData(const FBuildGraphOperatorParams& InParams, FBuildResults& OutResults) const
	{
		METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::FOperatorBuilder::BuildGraphOperator);

		using namespace OperatorBuilderPrivate;
		using namespace DirectedGraphAlgo;

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
			return TUniquePtr<FGraphOperatorData>(nullptr);
		}

		// Create algo adapter view of graph to cache graph operations.
		TPimplPtr<FDirectedGraphAlgoAdapter> AlgoAdapter = DirectedGraphAlgo::CreateDirectedGraphAlgoAdapter(InParams.Graph);
		
		if (!AlgoAdapter.IsValid())
		{
			AddBuildError<FInternalError>(OutResults.Errors, __FILE__, __LINE__);
			return TUniquePtr<FGraphOperatorData>(nullptr);
		}

		OperatorBuilder::FBuildContext BuildContext(InParams.Graph, *AlgoAdapter, InParams.OperatorSettings, InParams.Environment, OutResults);

		TArray<const INode*> SortedNodes;

		// Sort the nodes in a valid execution order
		BuildStatus |= DepthFirstTopologicalSort(BuildContext, SortedNodes);

		// TODO: Add FindReachableNodesFromVariables in Prune.
		// TODO: will need to prune edges as well.
		// Otherwise, subgraphs incorrectly get pruned.
		// BuildStatus |= PruneNodges(BuildContext, SortedNodes);

		// Check build status in case build routine should be exited early.
		if (BuildStatus > GetMaxErrorLevel())
		{
			return TUniquePtr<FGraphOperatorData>(nullptr);
		}

		InitializeOperatorInfo(InParams.Graph, SortedNodes, *BuildContext.GraphOperatorData);

		// Assign external inputs to various vertex interfaces.
		BuildStatus |= FOperatorBuilder::GatherExternalInputDataReferences(BuildContext, InParams.InputData);

		// Check build status in case build routine should be exited early.
		if (BuildStatus > GetMaxErrorLevel())
		{
			return TUniquePtr<FGraphOperatorData>(nullptr);
		}

		// Create node operators from factories.
		BuildStatus |= CreateOperators(BuildContext, SortedNodes, InParams.InputData);

		if (BuildStatus > GetMaxErrorLevel())
		{
			return TUniquePtr<FGraphOperatorData>(nullptr);
		}

		if (BuilderSettings.bPopulateInternalDataReferences)
		{
			GatherInternalGraphDataReferences(BuildContext, SortedNodes, BuildContext.Results.InternalDataReferences);
		}

		// Gather the inputs for the graph data. 
		BuildStatus |= GatherGraphDataReferences(BuildContext, BuildContext.GraphOperatorData->VertexData);

		if (BuildStatus > GetMaxErrorLevel())
		{
			return TUniquePtr<FGraphOperatorData>(nullptr);
		}

		return MoveTemp(BuildContext.GraphOperatorData);
	}

	FOperatorBuilder::FBuildStatus FOperatorBuilder::DepthFirstTopologicalSort(OperatorBuilder::FBuildContext& InOutContext, TArray<const INode*>& OutNodes) const
	{
		METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::FOperatorBuilder::DepthFirstTopologicalSort);
		using namespace OperatorBuilderPrivate;

		bool bSuccess = DirectedGraphAlgo::DepthFirstTopologicalSort(InOutContext.AlgoAdapter, OutNodes);

		if (!bSuccess)
		{
			// If there was an error, there is likely a cycle in the graph.
			AddBuildErrorsForCycles(InOutContext.AlgoAdapter, InOutContext.Results.Errors);

			return FBuildStatus::FatalError;
		}

		return FBuildStatus::NoError;
	}

	FOperatorBuilder::FBuildStatus FOperatorBuilder::KahnsTopologicalSort(OperatorBuilder::FBuildContext& InOutContext, TArray<const INode*>& OutNodes) const
	{
		METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::FOperatorBuilder::KahnsTopologicalSort);
		using namespace OperatorBuilderPrivate;

		bool bSuccess = DirectedGraphAlgo::KahnTopologicalSort(InOutContext.AlgoAdapter, OutNodes);

		if (!bSuccess)
		{
			// If there was an error, there is likely a cycle in the graph.
			AddBuildErrorsForCycles(InOutContext.AlgoAdapter, InOutContext.Results.Errors);

			return FBuildStatus::FatalError;
		}

		return FBuildStatus::NoError;
	}

	FOperatorBuilder::FBuildStatus FOperatorBuilder::PruneNodes(OperatorBuilder::FBuildContext& InOutContext, TArray<const INode*>& InOutNodes) const
	{
		METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::FOperatorBuilder::PruneNodes);
		using namespace OperatorBuilderPrivate;

		FBuildStatus BuildStatus = FBuildStatus::NoError;

		TSet<const INode*> ReachableNodes;

		switch (BuilderSettings.PruningMode)
		{
			case EOperatorBuilderNodePruning::PruneNodesWithoutExternalDependency:
				DirectedGraphAlgo::FindReachableNodes(InOutContext.AlgoAdapter, ReachableNodes);
				break;

			case EOperatorBuilderNodePruning::PruneNodesWithoutOutputDependency:
				DirectedGraphAlgo::FindReachableNodesFromOutput(InOutContext.AlgoAdapter, ReachableNodes);
				break;

			case EOperatorBuilderNodePruning::PruneNodesWithoutInputDependency:
				DirectedGraphAlgo::FindReachableNodesFromInput(InOutContext.AlgoAdapter, ReachableNodes);
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

	void FOperatorBuilder::InitializeOperatorInfo(const IGraph& InGraph, TArray<const INode*>& InSortedNodes, DirectedGraphAlgo::FGraphOperatorData& InOutGraphOperatorData) const
	{
		METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::FOperatorBuilder::InitializeOperatorInfo);
		using namespace DirectedGraphAlgo;

		// Create FOperatorInfos from Nodes
		TSortedMap<FOperatorID, FGraphOperatorData::FOperatorInfo>& OperatorMap = InOutGraphOperatorData.OperatorMap;
		TArray<FOperatorID>& OperatorOrder = InOutGraphOperatorData.OperatorOrder;

		const int32 NumNodes = InSortedNodes.Num();
		OperatorMap.Reserve(OperatorMap.Num() + NumNodes);
		OperatorOrder.Reserve(OperatorOrder.Num() + NumNodes);

		{
			METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::FOperatorBuilder::InitializeOperatorInfo::Nodes);
			for (const INode* Node : InSortedNodes)
			{
				FOperatorID OperatorID = GetOperatorID(Node);
				OperatorOrder.Add(OperatorID);
				OperatorMap.Add(OperatorID, FGraphOperatorData::FOperatorInfo{nullptr, Node->GetVertexInterface()});
			}
		}

		{
			METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::FOperatorBuilder::InitializeOperatorInfo::OutputDestinations);
			// Set the output destinations on operator infos
			for (const FDataEdge& Edge : InGraph.GetDataEdges())
			{
				const FOperatorID FromOperatorID = GetOperatorID(Edge.From.Node);
				FGraphOperatorData::FOperatorInfo& OperatorInfo = OperatorMap.FindChecked(FromOperatorID);

				OperatorInfo.OutputConnections.FindOrAdd(Edge.From.Vertex.VertexName).Add(FGraphOperatorData::FVertexDestination{GetOperatorID(Edge.To.Node), Edge.To.Vertex.VertexName});
			}
		}
	}

	FOperatorBuilder::FBuildStatus FOperatorBuilder::GatherExternalInputDataReferences(OperatorBuilder::FBuildContext& InOutContext, const FInputVertexInterfaceData& InExternalInputData) const
	{
		METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::FOperatorBuilder::GatherExternalInputDataReferences);
		using namespace DirectedGraphAlgo;

		FBuildStatus BuildStatus;
		// Gather external input data to graph destinations
		for (const TPair<FNodeDataVertexKey, FInputDataDestination>& InputDestinationKV : InOutContext.Graph.GetInputDataDestinations())
		{
			const FInputDataDestination& Destination = InputDestinationKV.Value;

			FOperatorID OperatorID = GetOperatorID(Destination.Node);
			FGraphOperatorData::FOperatorInfo& OperatorInfo = InOutContext.GraphOperatorData->OperatorMap.FindChecked(OperatorID);

			if (const FAnyDataReference* DataReference = InExternalInputData.FindDataReference(Destination.Vertex.VertexName))
			{
				if (DataReference->GetDataTypeName() == Destination.Vertex.DataTypeName)
				{
					OperatorInfo.VertexData.GetInputs().SetVertex(Destination.Vertex.VertexName, *DataReference);
				}
				else
				{
					// Mismatch in datatypes. This likely corresponds to a corrupt 
					// Metasound Graph. The graph's inputs should route directly
					// to TInputNode<>s with matching data types. 
					
					// Create source for reporting connection since external inputs do not have nodes. 
					const FInputDataVertex& GraphVertex = InExternalInputData.GetVertex(Destination.Vertex.VertexName);

					FOutputDataSource Source;
					Source.Vertex.VertexName = GraphVertex.VertexName; 
					Source.Vertex.DataTypeName = GraphVertex.DataTypeName;
#if WITH_EDITORONLY_DATA
					Source.Vertex.Metadata = GraphVertex.Metadata;
#endif

					AddBuildError<FInvalidConnectionDataTypeError>(InOutContext.Results.Errors, FDataEdge{Source, Destination});
				}
			}
		}

		return BuildStatus;
	}

	void FOperatorBuilder::GatherInternalGraphDataReferences(OperatorBuilder::FBuildContext& InOutContext, const TArray<const INode*>& InNodes, TMap<FGuid, FDataReferenceCollection>& OutNodeVertexData) const
	{
		METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::FOperatorBuilder::GatherInternalGraphDataReferences);
		using namespace DirectedGraphAlgo;

		for (const INode* NodePtr : InNodes)
		{
			check(NodePtr);
			if (const FGraphOperatorData::FOperatorInfo* OpInfo = InOutContext.GraphOperatorData->OperatorMap.Find(GetOperatorID(NodePtr)))
			{
				OutNodeVertexData.Emplace(NodePtr->GetInstanceID(), OpInfo->VertexData.GetOutputs().ToDataReferenceCollection());
			}
		}
	}

	FOperatorBuilder::FBuildStatus FOperatorBuilder::ValidateOperatorOutputsAreBound(const INode& InNode, const FOutputVertexInterfaceData& InVertexData) const
	{
		METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::FOperatorBuilder::ValidateOperatorOutputsAreBound);
		using FOutputVertexBinding = MetasoundVertexDataPrivate::FOutputBinding;

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

	FOperatorBuilder::FBuildStatus FOperatorBuilder::CreateOperators(OperatorBuilder::FBuildContext& InOutContext, const TArray<const INode*>& InSortedNodes, const FInputVertexInterfaceData& InExternalInputData) const
	{
		METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::FOperatorBuilder::CreateOperators);
		METASOUND_DEBUG_DECLARE_SCOPE;

		using namespace DirectedGraphAlgo;

		bool ProfileOperators = BuilderSettings.bProfileOperators || Profiling::ProfileAllGraphs();

		FBuildStatus BuildStatus;

		// Create FOperatorInfos from Nodes
		TSortedMap<FOperatorID, FGraphOperatorData::FOperatorInfo>& OperatorMap = InOutContext.GraphOperatorData->OperatorMap;

		// Call operator factory for each node.
		for (const INode* Node : InSortedNodes)
		{
			METASOUND_DEBUG_SET_ACTIVE_NODE_SCOPE(Node);

			FOperatorID OperatorID = GetOperatorID(Node);
			FGraphOperatorData::FOperatorInfo& OperatorInfo = OperatorMap.FindChecked(OperatorID);

			{
#if METASOUND_CPUPROFILERTRACE_ENABLED
				// Use node class name if valid, otherwise (for example graph nodes) use instance name 
				TStringBuilder<256> TraceNamePtr;
				const FNodeClassName& NodeClassName = Node->GetMetadata().ClassName;
				const FName& NodeTraceName = NodeClassName.IsValid() ? NodeClassName.GetFullName() : Node->GetInstanceName();
				TraceNamePtr << "Metasound::FOperatorBuilder::CreateOperators::CreateAndBind " << NodeTraceName;
				METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*TraceNamePtr);
#endif // METASOUND_CPUPROFILERTRACE_ENABLED

				FBuildOperatorParams CreateParams{*Node, InOutContext.Settings, OperatorInfo.VertexData.GetInputs(), InOutContext.Environment, this};
				FOperatorFactorySharedRef Factory = Node->GetDefaultOperatorFactory();
				if (ProfileOperators && Profiling::OperatorShouldBeProfiled(Node->GetMetadata()))
				{
					METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::FOperatorBuilder::CreateOperators::CreateOperator);

					OperatorInfo.Operator = MakeUnique<FProfilingOperator>(Factory->CreateOperator(CreateParams, InOutContext.Results), Node);
				}
				else
				{
					METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::FOperatorBuilder::CreateOperators::CreateOperator);
					OperatorInfo.Operator = Factory->CreateOperator(CreateParams, InOutContext.Results);
				}

				if (!OperatorInfo.Operator.IsValid())
				{
					return FBuildStatus::FatalError;
				}

				// Bind vertex to operator data
				{
					METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::FOperatorBuilder::CreateOperators::BindInputsAndOutputs);
					// Inputs don't need to be bound for all nodes unless they are dynamic 
					// Inputs for input nodes will be bound separately in GatherGraphDataReferences
					if (BuilderSettings.bEnableOperatorRebind)
					{
						OperatorInfo.Operator->BindInputs(OperatorInfo.VertexData.GetInputs());
					}
					OperatorInfo.Operator->BindOutputs(OperatorInfo.VertexData.GetOutputs());
				}
				
				// Check if outputs are bound correctly.
				if (BuilderSettings.bValidateOperatorOutputsAreBound)
				{
					METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::FOperatorBuilder::CreateOperators::ValidateOperatorOutputsAreBound);

					BuildStatus |= ValidateOperatorOutputsAreBound(*Node, OperatorInfo.VertexData.GetOutputs());
				}
			}


			// Route outputs of operator to downstream operators' VertexData
			{
				METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::FOperatorBuilder::CreateOperators::RouteOutputs);
				for (const TPair<FVertexName, TArray<FGraphOperatorData::FVertexDestination>>& OutputRouting : OperatorInfo.OutputConnections)
				{
					const FVertexName& VertexName = OutputRouting.Key;

					if (const FAnyDataReference* Ref = OperatorInfo.VertexData.GetOutputs().FindDataReference(VertexName))
					{
						for (const FGraphOperatorData::FVertexDestination& Destination : OutputRouting.Value)
						{
							OperatorMap.FindChecked(Destination.OperatorID).VertexData.GetInputs().SetVertex(Destination.VertexName, *Ref);
						}
					}
					else
					{
						// Does not contain any reference
						// This is likely a node programming error where the edges reported by the INode interface
						// did not match the readable parameter refs created by the operators outputs. Or, the edge description is invalid.

						AddBuildError<FMissingOutputDataReferenceError>(InOutContext.Results.Errors, FOutputDataSource{*Node, Node->GetVertexInterface().GetOutputVertex(VertexName)});

						BuildStatus |= FBuildStatus::NonFatalError;
					}
				}
			}
		}

		return BuildStatus;
	}

	FOperatorBuilder::FBuildStatus FOperatorBuilder::GatherGraphDataReferences(OperatorBuilder::FBuildContext& InOutContext, FVertexInterfaceData& OutVertexData) const
	{
		METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::FOperatorBuilder::GatherGraphDataReferences);
		using namespace OperatorBuilderPrivate;
		using namespace DirectedGraphAlgo;
		using FDestinationElement = FInputDataDestinationCollection::ElementType;
		using FSourceElement = FOutputDataSourceCollection::ElementType;

		FBuildStatus BuildStatus;
		// Gather graph inputs
		for (const FDestinationElement& Element : InOutContext.Graph.GetInputDataDestinations())
		{
			bool bFoundDataReference = false;
			const FInputDataDestination& InputDestination = Element.Value;

			const FOperatorID OperatorID = GetOperatorID(InputDestination.Node);
			if (FGraphOperatorData::FOperatorInfo* OperatorInfo = InOutContext.GraphOperatorData->OperatorMap.Find(OperatorID))
			{
				FInputVertexInterfaceData& NodeInputData = OperatorInfo->VertexData.GetInputs();
				OperatorInfo->Operator->BindInputs(NodeInputData);

				if (const FAnyDataReference* DataReference = NodeInputData.FindDataReference(InputDestination.Vertex.VertexName))
				{
					if (DataReference->GetDataTypeName() == InputDestination.Vertex.DataTypeName)
					{
						bFoundDataReference = true;
						OutVertexData.GetInputs().SetVertex(InputDestination.Vertex.VertexName, *DataReference);
					}
				}
				InOutContext.GraphOperatorData->InputVertexMap.Emplace(InputDestination.Vertex.VertexName, OperatorID);
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

			const FOperatorID OperatorID = GetOperatorID(OutputSource.Node);
			if (const FGraphOperatorData::FOperatorInfo* OperatorInfo = InOutContext.GraphOperatorData->OperatorMap.Find(OperatorID))
			{
				const FOutputVertexInterfaceData& NodeOutputData = OperatorInfo->VertexData.GetOutputs();
				if (const FAnyDataReference* DataReference = NodeOutputData.FindDataReference(OutputSource.Vertex.VertexName))
				{
					if (DataReference->GetDataTypeName() == OutputSource.Vertex.DataTypeName)
					{
						bFoundDataReference = true;
						OutVertexData.GetOutputs().SetVertex(OutputSource.Vertex.VertexName, FAnyDataReference{*DataReference});
					}
				}
				InOutContext.GraphOperatorData->OutputVertexMap.Emplace(OutputSource.Vertex.VertexName, OperatorID);
			}

			if (!bFoundDataReference)
			{
				AddBuildError<FMissingOutputDataReferenceError>(InOutContext.Results.Errors, OutputSource);
				BuildStatus |= FBuildStatus::NonFatalError;
			}
		}
	
		return BuildStatus;
	}

	TUniquePtr<IOperator> FOperatorBuilder::CreateGraphOperator(TUniquePtr<DirectedGraphAlgo::FGraphOperatorData>&& InGraphOperatorData) const
	{
		METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::FOperatorBuilder::CreateGraphOperator);

		TUniquePtr<IOperator> GraphOperator;

		if (BuilderSettings.bEnableOperatorRebind)
		{
			 GraphOperator = MakeUnique<FRebindableGraphOperator>(MoveTemp(*InGraphOperatorData));
		}
		else
		{
			 GraphOperator = MakeUnique<FGraphOperator>(MoveTemp(InGraphOperatorData));
		}

		return GraphOperator;
	}

	FOperatorBuilder::FBuildStatus::EStatus FOperatorBuilder::GetMaxErrorLevel() const
	{
		return BuilderSettings.bFailOnAnyError ? FBuildStatus::NoError : FBuildStatus::NonFatalError;
	}
}
