// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundDynamicOperatorTransactor.h"

#include "Containers/SpscQueue.h"
#include "Containers/UnrealString.h"
#include "MetasoundAudioBuffer.h"
#include "MetasoundBuilderInterface.h"
#include "MetasoundDataReference.h"
#include "MetasoundDynamicOperator.h"
#include "MetasoundGraph.h"
#include "MetasoundGraphAlgo.h"
#include "MetasoundGraphAlgoPrivate.h"
#include "MetasoundNodeInterface.h"
#include "MetasoundOperatorBuilder.h"
#include "MetasoundOperatorBuilderSettings.h"
#include "MetasoundOperatorInterface.h"
#include "MetasoundTrace.h"
#include "Templates/SharedPointer.h"
#include "Templates/UniquePtr.h"

namespace Metasound
{
	namespace DynamicGraph
	{
		namespace DynamicOperatorTransactorPrivate
		{
			// Return operator builder settings appropriate for building subgraphs
			// of a dynamic operaotr.
			FOperatorBuilderSettings GetOperatorBuilderSettings()
			{
				FOperatorBuilderSettings Settings = FOperatorBuilderSettings::GetDefaultSettings();

				// Subgraphs must be rebindable to support connecting and disconnecting
				// data references to subgraphs. 
				Settings.bEnableOperatorRebind = true;

				return Settings;
			}

			// Literal nodes always have output vertex with this name. 
			static const FLazyName LiteralNodeOutputVertexName("Value");
			
			// Sorts the graph and determines order of operator execution.
			TArray<FOperatorID> DetermineOperatorOrder(const IGraph& InGraph)
			{
				/* determine new operator order. */
				TArray<const INode*> NodeOrder;

				bool bSuccess = DirectedGraphAlgo::DepthFirstTopologicalSort(InGraph, NodeOrder);
				if (!bSuccess)
				{
					UE_LOG(LogMetaSound, Error, TEXT("Cycles found in dynamic graph"));
				}

				TArray<FOperatorID> OperatorOrder;
				Algo::Transform(NodeOrder, OperatorOrder, static_cast<FOperatorID(*)(const INode*)>(DirectedGraphAlgo::GetOperatorID)); //< Static cast to help deduce which overloaded version of GetOperatorID to call in Algo::Transform

				return OperatorOrder;
			}

			FString GetDebugNodeNameString(const INode& InNode)
			{
				return FString::Printf(TEXT("%s_v%d.%d"), *InNode.GetMetadata().ClassName.GetFullName().ToString(), InNode.GetMetadata().MajorVersion, InNode.GetMetadata().MinorVersion);
			}

			FString GetDebugNodeNameString(const FGuid& InNodeID, const INode& InNode)
			{
				return FString::Printf(TEXT("%s:%s"), *InNodeID.ToString(), *GetDebugNodeNameString(InNode));
			}
		}

		bool operator<(const FDynamicOperatorTransactor::FLiteralNodeID& InLHS, const FDynamicOperatorTransactor::FLiteralNodeID& InRHS)
		{
			if (InLHS.ToNode < InRHS.ToNode)
			{
				return true;
			}
			else if (InRHS.ToNode < InLHS.ToNode)
			{
				return false;
			}
			else
			{
				return InLHS.ToVertex.FastLess(InRHS.ToVertex);
			}
		}

		FDynamicOperatorTransactor::FDynamicOperatorTransactor(const FGraph& InGraph)
		: OperatorBuilder(DynamicOperatorTransactorPrivate::GetOperatorBuilderSettings())
		, Graph(InGraph)
		{
			CurrentOperatorOrder = DynamicOperatorTransactorPrivate::DetermineOperatorOrder(Graph);
		}

		FDynamicOperatorTransactor::FDynamicOperatorTransactor()
		: OperatorBuilder(DynamicOperatorTransactorPrivate::GetOperatorBuilderSettings())
		, Graph(TEXT(""), FGuid())
		{
		}

		TSharedRef<TSpscQueue<TUniquePtr<IDynamicOperatorTransform>>> FDynamicOperatorTransactor::CreateTransformQueue(const FOperatorSettings& InOperatorSettings, const FMetasoundEnvironment& InEnvironment)
		{
			TSharedRef<TSpscQueue<TUniquePtr<IDynamicOperatorTransform>>> Queue = MakeShared<TSpscQueue<TUniquePtr<IDynamicOperatorTransform>>>();
			OperatorInfos.Add(FDynamicOperatorInfo{InOperatorSettings, InEnvironment, Queue});

			// Unconnected nodes are intentionally skipped by the operator builder
			// in order to reduce perf. In a dynamic operator, these nodes may be
			// connected in the future. We queue them up to be added here so that
			// they exist on the dynamic operator in the case they are connected at
			// a later time.
			TArray<TPair<FGuid, const INode*>> UnconnectedNodes;
			if (Graph.FindUnconnectedNodes(UnconnectedNodes) > 0)
			{
				for (const TPair<FGuid, const INode*>& GuidAndNode : UnconnectedNodes)
				{
					// Only add to this queue because we do not know at what point
					// the other queues and dynamic operators were created.
					Queue->Enqueue(CreateAddOperatorTransform(*GuidAndNode.Value, EExecutionOrderInsertLocation::Last, InOperatorSettings, InEnvironment));
				}
			}

			// Force order to be synchronized to internal execution order.
			Queue->Enqueue(MakeUnique<FSetOperatorOrder>(CurrentOperatorOrder));

			return Queue;
		}

		void FDynamicOperatorTransactor::AddNode(const FGuid& InNodeID, TUniquePtr<INode> InNode)
		{
			using namespace DirectedGraphAlgo;

			METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::FDynamicOperatorTransactor::AddNode);

			if (!InNode)
			{
				return;
			}

			TSharedRef<const INode> NodePtr(InNode.Release());

			Graph.AddNode(InNodeID, NodePtr);

			EnqueueAddOperatorTransform(*NodePtr, EExecutionOrderInsertLocation::Last);
		}

		void FDynamicOperatorTransactor::RemoveNode(const FGuid& InNodeID)
		{
			using namespace DirectedGraphAlgo;

			METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::FDynamicOperatorTransactor::RemoveNode);

			if (const INode* Node = Graph.FindNode(InNodeID))
			{
				TArray<FVertexName> OutputsToFade;
				const FOutputVertexInterface& OutputInterface = Node->GetVertexInterface().GetOutputInterface();
				for (const FOutputDataVertex& OutputVertex : OutputInterface)
				{
					if (OutputVertex.DataTypeName == GetMetasoundDataTypeName<FAudioBuffer>())
					{
						OutputsToFade.Add(OutputVertex.VertexName);
					}
				}

				if (OutputsToFade.Num())
				{
					EnqueueFadeAndRemoveOperatorTransform(*Node, OutputsToFade);
				}
				else
				{
					EnqueueRemoveOperatorTransform(*Node);
				}

				constexpr bool bRemoveDataEdgesWithNode = true;
				bool bRemovedNodeFromGraph = Graph.RemoveNode(InNodeID, bRemoveDataEdgesWithNode);
				check(bRemovedNodeFromGraph); //< Should always be true because we know the node exists in the Graph from `Graph.FindNode(...)`
			}
			else
			{
				UE_LOG(LogMetaSound, Error, TEXT("No node found in dynamic transactor graph with ID %s"), *InNodeID.ToString());
			}
		}

		/** Add an edge to the graph. */
		void FDynamicOperatorTransactor::AddDataEdge(const FGuid& InFromNodeID, const FVertexName& InFromVertex, const FGuid& InToNodeID, const FVertexName& InToVertex)
		{
			using namespace DirectedGraphAlgo; 

			METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::FDynamicOperatorTransactor::AddDataEdge);

			const INode* FromNode = Graph.FindNode(InFromNodeID);
			const INode* ToNode = Graph.FindNode(InToNodeID);

			if ((nullptr == ToNode) || (nullptr == FromNode))
			{
				UE_LOG(LogMetaSound, Error, TEXT("Cannot add edge from %s:%s to %s:%s because of missing node"), *InFromNodeID.ToString(), *InFromVertex.ToString(), *InToNodeID.ToString(), *InToVertex.ToString());
				return;
			}

			AddDataEdgeInternal(*FromNode, InFromVertex, InToNodeID, *ToNode, InToVertex);
		}

		void FDynamicOperatorTransactor::RemoveDataEdge(const FGuid& InFromNodeID, const FVertexName& InFromVertex, const FGuid& InToNodeID, const FVertexName& InToVertex, TUniquePtr<INode> InReplacementLiteralNode)
		{
			using namespace DirectedGraphAlgo; 

			METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::FDynamicOperatorTransactor::RemoveDataEdge);

			const INode* FromNode = Graph.FindNode(InFromNodeID);
			const INode* ToNode = Graph.FindNode(InToNodeID);
			const INode* ReplacementLiteralNode = InReplacementLiteralNode.Get(); // Cache pointer because TUniquePtr<INode> will get moved

			if ((nullptr == ToNode) || (nullptr == FromNode))
			{
				UE_LOG(LogMetaSound, Error, TEXT("Cannot remove edge from %s:%s to %s:%s because of missing node"), *InFromNodeID.ToString(), *InFromVertex.ToString(), *InToNodeID.ToString(), *InToVertex.ToString());
				return;
			}

			if (!ToNode->GetVertexInterface().ContainsInputVertex(InToVertex))
			{
				UE_LOG(LogMetaSound, Error, TEXT("Cannot remove edge from %s:%s to %s:%s because of destination node does not contain vertex %s."), *InFromNodeID.ToString(), *InFromVertex.ToString(), *InToNodeID.ToString(), *InToVertex.ToString(), *InToVertex.ToString());
				return;
			}

			if (!InReplacementLiteralNode.IsValid())
			{
				UE_LOG(LogMetaSound, Error, TEXT("Cannot remove edge from %s:%s to %s:%s because of invalid pointer to replacement literal node."), *InFromNodeID.ToString(), *InFromVertex.ToString(), *InToNodeID.ToString(), *InToVertex.ToString());
				return;
			}

			/* remove edge from internal graph. */
			bool bSuccess = Graph.RemoveDataEdge(*FromNode, InFromVertex, *ToNode, InToVertex);
			if (!bSuccess)
			{
				UE_LOG(LogMetaSound, Error, TEXT("Failed to remove edge from %s:%s to %s:%s on internal graph."), *InFromNodeID.ToString(), *InFromVertex.ToString(), *InToNodeID.ToString(), *InToVertex.ToString());
				return;
			}

			// Store literal node associated with the target of the literal value.
			LiteralNodeMap.Add(FLiteralNodeID{ InToNodeID, InToVertex }, MoveTemp(InReplacementLiteralNode));
			bSuccess = Graph.AddDataEdge(*ReplacementLiteralNode, DynamicOperatorTransactorPrivate::LiteralNodeOutputVertexName, *ToNode, InToVertex);
			if (!bSuccess)
			{
				UE_LOG(LogMetaSound, Error, TEXT("Failed to add literal for %s:%s on internal graph."), *InToNodeID.ToString(), *InToVertex.ToString());
				return;
			}

			if (ToNode->GetVertexInterface().GetInputVertex(InToVertex).DataTypeName == GetMetasoundDataTypeName<FAudioBuffer>())
			{
				// Handle audio edge removal with a fade out.
				EnqueueFadeAndRemoveEdgeOperatorTransform(*FromNode, InFromVertex, *ToNode, InToVertex, *ReplacementLiteralNode);
			}
			else
			{
				// Immediately disconnect non-audio edges. 
				EnqueueRemoveEdgeOperatorTransform(*FromNode, InFromVertex, *ToNode, InToVertex, *ReplacementLiteralNode);
			}
		}

		void FDynamicOperatorTransactor::SetValue(const FGuid& InNodeID, const FVertexName& InVertex, TUniquePtr<INode> InLiteralNode)
		{
			METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::FDynamicOperatorTransactor::SetValue);

			const INode* Node = Graph.FindNode(InNodeID);
			const INode* LiteralNode = InLiteralNode.Get();

			if (nullptr == Node)
			{
				UE_LOG(LogMetaSound, Error, TEXT("Cannot set node value of %s:%s because of missing node"), *InNodeID.ToString(), *InVertex.ToString());
				return;
			}

			if (!InLiteralNode.IsValid())
			{
				UE_LOG(LogMetaSound, Error, TEXT("Cannot set value on %s:%s because of invalid pointer to literal node."), *InNodeID.ToString(), *InVertex.ToString());
				return;
			}

			// Always insert new literal nodes first in execution order.
			CurrentOperatorOrder.Insert(DirectedGraphAlgo::GetOperatorID(*LiteralNode), 0);

			auto CreateAddNodeTransform = [&](const FOperatorSettings& InOperatorSettings, const FMetasoundEnvironment& InEnvironment) -> TUniquePtr<IDynamicOperatorTransform>
			{
				return CreateAddOperatorTransform(*LiteralNode, EExecutionOrderInsertLocation::First, InOperatorSettings, InEnvironment);
			};

			EnqueueTransformOnOperatorQueues(CreateAddNodeTransform);

			AddDataEdgeInternal(*LiteralNode, DynamicOperatorTransactorPrivate::LiteralNodeOutputVertexName, InNodeID, *Node, InVertex);

			// Add literal node after calling "AddDataEdgeInternal" so that AddDataEdgeInternal can check if there is a prior existing literal node.
			LiteralNodeMap.Add(FLiteralNodeID{InNodeID, InVertex}, MoveTemp(InLiteralNode));
		}

		/** Add an input data destination to describe how data provided 
		 * outside this graph should be routed internally.
		 *
		 * @param InNode - Node which receives the data.
		 * @param InVertexName - Key for input vertex on InNode.
		 *
		 */
		void FDynamicOperatorTransactor::AddInputDataDestination(const FGuid& InNodeID, const FVertexName& InVertexName, const FLiteral& InDefaultLiteral, FReferenceCreationFunction InFunc)
		{
			METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::FDynamicOperatorTransactor::AddInputDataDestination);

			const INode* Node = Graph.FindNode(InNodeID);
			if (nullptr == Node)
			{
				UE_LOG(LogMetaSound, Error, TEXT("Cannot add Input Data Destination %s:%s because of missing node"), *InNodeID.ToString(), *InVertexName.ToString());
				return;
			}

			if (!Node->GetVertexInterface().ContainsInputVertex(InVertexName))
			{
				UE_LOG(LogMetaSound, Error, TEXT("Cannot add Input Data Destination %s:%s because of node does not contain input vertex with name %s."), *InNodeID.ToString(), *InVertexName.ToString(), *InVertexName.ToString());
				return;
			}

			const FInputDataVertex& InputVertex = Node->GetVertexInterface().GetInputVertex(InVertexName);
			EDataReferenceAccessType ReferenceAccessType = InputVertex.AccessType == EVertexAccessType::Value ? EDataReferenceAccessType::Value : EDataReferenceAccessType::Write;
			FOperatorID OperatorID = DirectedGraphAlgo::GetOperatorID(Node);

			Graph.AddInputDataDestination(*Node, InVertexName);

			auto CreateAddInputTransform = [&](const FOperatorSettings& InOperatorSettings, const FMetasoundEnvironment& InEnvironment) -> TUniquePtr<IDynamicOperatorTransform>
			{
				TOptional<FAnyDataReference> NewDataReference = InFunc(InOperatorSettings, InputVertex.DataTypeName, InDefaultLiteral, ReferenceAccessType);
				if (NewDataReference.IsSet())
				{
					return MakeUnique<FAddInput>(OperatorID, InVertexName, *NewDataReference);
				}
				else
				{
					UE_LOG(LogMetaSound, Error, TEXT("Cannot add Input Data Destination %s:%s because of failure to create data reference."), *InNodeID.ToString(), *InVertexName.ToString());
					return TUniquePtr<IDynamicOperatorTransform>(nullptr);
				}
			};

			EnqueueTransformOnOperatorQueues(CreateAddInputTransform);
		}

		void FDynamicOperatorTransactor::RemoveInputDataDestination(const FVertexName& InVertexName)
		{
			METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::FDynamicOperatorTransactor::RemoveInputDataDestination);

			Graph.RemoveInputDataDestination(InVertexName);

			auto CreateRemoveInputTransform = [&](const FOperatorSettings& InOperatorSettings, const FMetasoundEnvironment& InEnvironment) -> TUniquePtr<IDynamicOperatorTransform>
			{
				return MakeUnique<FRemoveInput>(InVertexName);
			};

			EnqueueTransformOnOperatorQueues(CreateRemoveInputTransform);
		}

		/** Add an output data source which describes routing of data which is 
		 * owned this graph and exposed externally.
		 *
		 * @param InNode - Node which produces the data.
		 * @param InVertexName - Key for output vertex on InNode.
		 */
		void FDynamicOperatorTransactor::AddOutputDataSource(const FGuid& InNodeID, const FVertexName& InVertexName)
		{
			METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::FDynamicOperatorTransactor::AddOutputDataSource);

			const INode* Node = Graph.FindNode(InNodeID);
			if (nullptr == Node)
			{
				UE_LOG(LogMetaSound, Error, TEXT("Cannot add Output Data Source %s:%s because of missing node"), *InNodeID.ToString(), *InVertexName.ToString());
				return;
			}

			Graph.AddOutputDataSource(*Node, InVertexName);
			const FOperatorID OperatorID = DirectedGraphAlgo::GetOperatorID(Node);

			auto CreateAddOutputTransform = [&](const FOperatorSettings& InOperatorSettings, const FMetasoundEnvironment& InEnvironment) -> TUniquePtr<IDynamicOperatorTransform>
			{
				return MakeUnique<FAddOutput>(OperatorID, InVertexName);
			};

			EnqueueTransformOnOperatorQueues(CreateAddOutputTransform);
		}

		void FDynamicOperatorTransactor::RemoveOutputDataSource(const FVertexName& InVertexName)
		{
			METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::FDynamicOperatorTransactor::RemoveOutputDataSource);

			Graph.RemoveOutputDataSource(InVertexName);

			auto CreateRemoveOutputTransform = [&](const FOperatorSettings& InOperatorSettings, const FMetasoundEnvironment& InEnvironment) -> TUniquePtr<IDynamicOperatorTransform>
			{
				return MakeUnique<FRemoveOutput>(InVertexName);
			};

			EnqueueTransformOnOperatorQueues(CreateRemoveOutputTransform);
		}

		const FGraph& FDynamicOperatorTransactor::GetGraph() const
		{
			return Graph;
		}

		void FDynamicOperatorTransactor::AddDataEdgeInternal(const INode& InFromNode, const FVertexName& InFromVertex, const FGuid& InToNodeID, const INode& InToNode, const FVertexName& InToVertex)
		{
			using namespace DirectedGraphAlgo;
			using namespace DynamicOperatorTransactorPrivate;

			const FInputDataVertex* InputVertex = InToNode.GetVertexInterface().GetInputInterface().Find(InToVertex);
			if (nullptr == InputVertex)
			{
				UE_LOG(LogMetaSound, Error, TEXT("Cannot connect nodes because destination node %s does not contain input vertex %s"), *GetDebugNodeNameString(InToNodeID, InToNode), *InToVertex.ToString());
				return;
			}

			/* Determine if there is an existing literal node connected to the node. 
			 * Literal nodes are stored on the FDynamicOperatorTransactor and need to be 
			 * disconnected and removed if they are no longer being used. 
			 */
			const FLiteralNodeID LiteralNodeKey{InToNodeID, InToVertex};
			TUniquePtr<INode> PriorLiteralNode;
			if (LiteralNodeMap.RemoveAndCopyValue(LiteralNodeKey, PriorLiteralNode))
			{
				Graph.RemoveDataEdge(*PriorLiteralNode, DynamicOperatorTransactorPrivate::LiteralNodeOutputVertexName, InToNode, InToVertex);
			}

			/* add edge to internal graph. */
			Graph.AddDataEdge(InFromNode, InFromVertex, InToNode, InToVertex);
			
			if (InputVertex->DataTypeName == GetMetasoundDataTypeName<FAudioBuffer>())
			{
				// If edge is audio, then the connection needs to be faded
				EnqueueFadeAndAddEdgeOperatorTransform(InFromNode, InFromVertex, InToNode, InToVertex, PriorLiteralNode.Get());
			}
			else
			{
				// If the edge is not audio, then no fading is performed. 
				EnqueueAddEdgeOperatorTransform(InFromNode, InFromVertex, InToNode, InToVertex, PriorLiteralNode.Get());
			}
		}
		
		void FDynamicOperatorTransactor::EnqueueAddOperatorTransform(const INode& InNode, EExecutionOrderInsertLocation InLocation)
		{
			// Update the CurrentOperatorOrder based on the insert location of new node. 
			// This avoids resorting of entire graph on rendering dynamic metasounds by
			// simply appending to a specific location.
			switch (InLocation)
			{
				case EExecutionOrderInsertLocation::First:
					CurrentOperatorOrder.Insert(DirectedGraphAlgo::GetOperatorID(InNode), 0);
					break;

				case EExecutionOrderInsertLocation::Last:
					CurrentOperatorOrder.Add(DirectedGraphAlgo::GetOperatorID(InNode));
					break;
			}

			auto CreateAddNodeTransform = [&](const FOperatorSettings& InOperatorSettings, const FMetasoundEnvironment& InEnvironment) -> TUniquePtr<IDynamicOperatorTransform>
			{
				return CreateAddOperatorTransform(InNode, InLocation, InOperatorSettings, InEnvironment);
			};

			EnqueueTransformOnOperatorQueues(CreateAddNodeTransform);
		}

		void FDynamicOperatorTransactor::EnqueueFadeAndRemoveOperatorTransform(const INode& InNode, TArrayView<const FVertexName> InOutputsToFade)
		{
			TArrayView<const FVertexName> InputsToFade; // We do not need to fade any inputs when removing a node.

			EnqueueBeginFadeOperatorTransform(InNode, EAudioFadeType::FadeOut, InputsToFade, InOutputsToFade);

			const FOperatorID OperatorID = DirectedGraphAlgo::GetOperatorID(InNode);
			CurrentOperatorOrder.RemoveSingle(OperatorID);

			auto CreateEndFadeOutTransform = [&](const FOperatorSettings& InOperatorSettings, const FMetasoundEnvironment& InEnvironment) -> TUniquePtr<IDynamicOperatorTransform>
			{
				// We can skip the FEndAudioFadeTransform as an optimization here.
				// FEndAudioFadeTransform removes the fading wrapper around the node,
				// but since the node is being removed, we can remove the wrapper
				// and the node with a single FRemoveOperator transform.
				return MakeUnique<FRemoveOperator>(OperatorID);
			};

			EnqueueTransformOnOperatorQueues(CreateEndFadeOutTransform);
		}

		void FDynamicOperatorTransactor::EnqueueAddEdgeOperatorTransform(const INode& InFromNode, const FVertexName& InFromVertex, const INode& InToNode, const FVertexName& InToVertex, const INode* InPriorLiteralNode)
		{
			/* enqueue an update. */
			FOperatorID FromOperatorID = DirectedGraphAlgo::GetOperatorID(InFromNode);
			FOperatorID ToOperatorID = DirectedGraphAlgo::GetOperatorID(InToNode);

			if (InPriorLiteralNode)
			{
				CurrentOperatorOrder.RemoveSingle(DirectedGraphAlgo::GetOperatorID(InPriorLiteralNode));
			}

			// Find order of operators after adding edge. 
			TArray<FOperatorID> NewOperatorOrder = DynamicOperatorTransactorPrivate::DetermineOperatorOrder(Graph);

			// Only set new order if it's different than existing.
			bool bSetNewOperatorOrder = NewOperatorOrder != CurrentOperatorOrder;
			if (bSetNewOperatorOrder)
			{
				CurrentOperatorOrder = NewOperatorOrder;
			}

			auto CreateAddEdgeTransform = [&](const FOperatorSettings& InOperatorSettings, const FMetasoundEnvironment& InEnvironment) -> TUniquePtr<IDynamicOperatorTransform>
			{
				TArray<TUniquePtr<IDynamicOperatorTransform>> AtomicTransforms;

				if (InPriorLiteralNode)
				{
					AtomicTransforms.Add(MakeUnique<FRemoveOperator>(DirectedGraphAlgo::GetOperatorID(InPriorLiteralNode)));
				}
				if (bSetNewOperatorOrder)
				{
					AtomicTransforms.Add(MakeUnique<FSetOperatorOrder>(NewOperatorOrder));
				}
				AtomicTransforms.Add(MakeUnique<FConnectOperators>(FromOperatorID, InFromVertex, ToOperatorID, InToVertex));

				return MakeUnique<FAtomicTransform>(MoveTemp(AtomicTransforms));
			};

			EnqueueTransformOnOperatorQueues(CreateAddEdgeTransform);
		}

		void FDynamicOperatorTransactor::EnqueueFadeAndAddEdgeOperatorTransform(const INode& InFromNode, const FVertexName& InFromVertex, const INode& InToNode, const FVertexName& InToVertex, const INode* InPriorLiteralNode)
		{
			FOperatorID FromOperatorID = DirectedGraphAlgo::GetOperatorID(InFromNode);
			FOperatorID ToOperatorID = DirectedGraphAlgo::GetOperatorID(InToNode);
			
			// Fade inputs on the receiving node when adding an edge. We don't fade the source node's outputs
			// because those outputs could also be connected to other nodes which we do not want to fade. 
			TArrayView<const FVertexName> InputsToFade(&InToVertex, 1);
			TArrayView<const FVertexName> OutputsToFade;

			if (InPriorLiteralNode)
			{
				CurrentOperatorOrder.RemoveSingle(DirectedGraphAlgo::GetOperatorID(InPriorLiteralNode));
			}

			// Find order of operators after removing literal and adding edge. 
			TArray<FOperatorID> NewOperatorOrder = DynamicOperatorTransactorPrivate::DetermineOperatorOrder(Graph);

			// Only set new order if it's different than existing.
			bool bSetNewOperatorOrder = NewOperatorOrder != CurrentOperatorOrder;
			if (bSetNewOperatorOrder)
			{
				CurrentOperatorOrder = NewOperatorOrder;
			}

			auto CreateBeginFadeAndAddEdgeTransform = [&](const FOperatorSettings& InOperatorSettings, const FMetasoundEnvironment& InEnvironment) -> TUniquePtr<IDynamicOperatorTransform>
			{
				TArray<TUniquePtr<IDynamicOperatorTransform>> AtomicTransforms;

				if (InPriorLiteralNode)
				{
					// We assume that any FAudioBuffer created from a literal node is just silent audio. That means we can
					// Remove that literal node without fading out the audio from the literal node connected to the destination node. 
					AtomicTransforms.Add(MakeUnique<FRemoveOperator>(DirectedGraphAlgo::GetOperatorID(InPriorLiteralNode)));
				}
				
				if (bSetNewOperatorOrder)
				{
					AtomicTransforms.Add(MakeUnique<FSetOperatorOrder>(NewOperatorOrder));
				}
				AtomicTransforms.Add(MakeUnique<FConnectOperators>(FromOperatorID, InFromVertex, ToOperatorID, InToVertex));
				AtomicTransforms.Add(MakeUnique<FBeginAudioFadeTransform>(ToOperatorID, EAudioFadeType::FadeIn, InputsToFade, OutputsToFade));
				
				// Fence must be the last transform since the fade must be performed
				// before anything else happens in the graph. To apply a fade, the 
				// graph must execute. This FExecuteFence transform ensures that the graph
				// is executed before any additional transforms are applied. 
				AtomicTransforms.Add(MakeUnique<FExecuteFence>());

				return MakeUnique<FAtomicTransform>(MoveTemp(AtomicTransforms));
			};

			EnqueueTransformOnOperatorQueues(CreateBeginFadeAndAddEdgeTransform);

			EnqueueEndFadeOperatorTransform(InToNode);
		}

		void FDynamicOperatorTransactor::EnqueueBeginFadeOperatorTransform(const INode& InNode, EAudioFadeType InFadeType, TArrayView<const FVertexName> InInputsToFade, TArrayView<const FVertexName> InOutputsToFade)
		{
			const FOperatorID OperatorID = DirectedGraphAlgo::GetOperatorID(InNode);

			auto CreateBeginAudioFadeTransform = [&](const FOperatorSettings& InOperatorSettings, const FMetasoundEnvironment& InEnvironment) -> TUniquePtr<IDynamicOperatorTransform>
			{
				TArray<TUniquePtr<IDynamicOperatorTransform>> BeginAudioFadeAtomicTransforms;

				BeginAudioFadeAtomicTransforms.Add(MakeUnique<FBeginAudioFadeTransform>(OperatorID, InFadeType, InInputsToFade, InOutputsToFade));
				// Fence must be the last transform since the fade must be performed
				// before anything else happens in the graph. To apply a fade, the 
				// graph must execute. This FExecuteFence transform ensures that the graph
				// is executed before any additional transforms are applied. 
				BeginAudioFadeAtomicTransforms.Add(MakeUnique<FExecuteFence>());

				return MakeUnique<FAtomicTransform>(MoveTemp(BeginAudioFadeAtomicTransforms));
			};

			EnqueueTransformOnOperatorQueues(CreateBeginAudioFadeTransform);
		}

		void FDynamicOperatorTransactor::EnqueueEndFadeOperatorTransform(const INode& InNode)
		{
			const FOperatorID OperatorID = DirectedGraphAlgo::GetOperatorID(InNode);

			auto CreateEndAudioFadeTransform = [&](const FOperatorSettings& InOperatorSettings, const FMetasoundEnvironment& InEnvironment) -> TUniquePtr<IDynamicOperatorTransform>
			{
				return MakeUnique<FEndAudioFadeTransform>(OperatorID);
			};

			EnqueueTransformOnOperatorQueues(CreateEndAudioFadeTransform);
		}

		void FDynamicOperatorTransactor::EnqueueRemoveOperatorTransform(const INode& InNode)
		{
			FOperatorID OperatorID = DirectedGraphAlgo::GetOperatorID(InNode);

			// Removing operator does not require a re-sort because existing dependencies
			// are still met due to DAG structure. 
			CurrentOperatorOrder.RemoveSingle(OperatorID);

			auto CreateRemoveNodeTransform = [&](const FOperatorSettings& InOperatorSettings, const FMetasoundEnvironment& InEnvironment) -> TUniquePtr<IDynamicOperatorTransform>
			{
				return MakeUnique<FRemoveOperator>(OperatorID);
			};
			EnqueueTransformOnOperatorQueues(CreateRemoveNodeTransform);
		}

		void FDynamicOperatorTransactor::EnqueueRemoveEdgeOperatorTransform(const INode& InFromNode, const FVertexName& InFromVertex, const INode& InToNode, const FVertexName& InToVertex, const INode& InReplacementLiteralNode)
		{
			using namespace DynamicOperatorTransactorPrivate;

			const FOperatorID FromOperatorID = DirectedGraphAlgo::GetOperatorID(InFromNode);
			const FOperatorID ToOperatorID = DirectedGraphAlgo::GetOperatorID(InToNode);
			const FOperatorID LiteralOperatorID = DirectedGraphAlgo::GetOperatorID(InReplacementLiteralNode);

			// Put literals in the front of the execution stack to simplify updating 
			// runtime instances. No need to sort the entire graph if we are just
			// inserting something at the beginning of the execution stack. 
			CurrentOperatorOrder.Insert(LiteralOperatorID, 0);

			auto CreateRemoveEdgeTransform = [&](const FOperatorSettings& InOperatorSettings, const FMetasoundEnvironment& InEnvironment) -> TUniquePtr<IDynamicOperatorTransform>
			{
				// Add the literal node.
				TUniquePtr<IDynamicOperatorTransform> AddNodeTransform = CreateAddOperatorTransform(InReplacementLiteralNode, EExecutionOrderInsertLocation::First, InOperatorSettings, InEnvironment);

				// Swap prior connection with new connections.
				TUniquePtr<IDynamicOperatorTransform> ConnectOperatorsTransform = MakeUnique<FSwapOperatorConnection>(FromOperatorID, InFromVertex, LiteralOperatorID, DynamicOperatorTransactorPrivate::LiteralNodeOutputVertexName, ToOperatorID, InToVertex);

				if (AddNodeTransform.IsValid() && ConnectOperatorsTransform.IsValid())
				{
					// Create an atomic transform so all sub-transforms happen before next execution.
					TArray<TUniquePtr<IDynamicOperatorTransform>> AtomicTransforms;

					AtomicTransforms.Emplace(MoveTemp(AddNodeTransform));
					AtomicTransforms.Emplace(MoveTemp(ConnectOperatorsTransform));

					return MakeUnique<FAtomicTransform>(MoveTemp(AtomicTransforms));
				}
				else
				{

					UE_LOG(LogMetaSound, Error, TEXT("Cannot remove edge from %s:%s to %s:%s because of failure to create all transforms needed to perform operatorn."), *GetDebugNodeNameString(InFromNode), *InFromVertex.ToString(), *GetDebugNodeNameString(InToNode), *InToVertex.ToString());
					return TUniquePtr<IDynamicOperatorTransform>(nullptr);
				}
			};

			EnqueueTransformOnOperatorQueues(CreateRemoveEdgeTransform);
		}

		void FDynamicOperatorTransactor::EnqueueFadeAndRemoveEdgeOperatorTransform(const INode& InFromNode, const FVertexName& InFromVertex, const INode& InToNode, const FVertexName& InToVertex, const INode& InReplacementLiteralNode)
		{
			// Fade the input to the node getting disconnected rather than the output of the source node. The source node
			// may be connected to other nodes and fading it's output would fade all the other connected nodes' inputs. 
			TArrayView<const FVertexName> InputVerticesToFade(&InToVertex, 1);
			TArrayView<const FVertexName> OutputVerticesToFade;

			EnqueueBeginFadeOperatorTransform(InToNode, EAudioFadeType::FadeOut, InputVerticesToFade, OutputVerticesToFade);

			// Replace input with literal. This assumes that the replacement audio
			// buffer contains silent audio. The fade transform will get the input
			// audio to silent which will then seamlessly be swapped with a silent
			// audio buffer as a permanent connection. 
			//
			// If we ever find ourselves creating audio buffers with literals which 
			// are anything other than silent buffers, we should rework this operation
			// to do either a cross-fade, or an additional "fade in" to the new value. 
			EnqueueRemoveEdgeOperatorTransform(InFromNode, InFromVertex, InToNode, InToVertex, InReplacementLiteralNode);

			// Remove fade operation. 
			EnqueueEndFadeOperatorTransform(InToNode);
		}



		TUniquePtr<IDynamicOperatorTransform> FDynamicOperatorTransactor::CreateAddOperatorTransform(const INode& InNode, EExecutionOrderInsertLocation InLocation, const FOperatorSettings& InOperatorSettings, const FMetasoundEnvironment& InEnvironment) const
		{
			using namespace DynamicOperatorTransactorPrivate;

			const FOperatorID OperatorID = DirectedGraphAlgo::GetOperatorID(InNode);
			FVertexInterfaceData InterfaceData(InNode.GetVertexInterface());
			FBuildOperatorParams OperatorParams
			{
				InNode,
				InOperatorSettings,
				InterfaceData.GetInputs(),
				InEnvironment,
				&OperatorBuilder // Supply an operator builder set to build rebindable inputs to ensure that subgraphs have their data references updated. 
			};

			FBuildResults Results;
			TUniquePtr<IOperator> Operator = InNode.GetDefaultOperatorFactory()->CreateOperator(OperatorParams, Results);

			for (const TUniquePtr<IOperatorBuildError>& Error : Results.Errors)
			{
				if (Error.IsValid())
				{
					UE_LOG(LogMetaSound, Warning, TEXT("Encountered error while building operator for node %s. %s:%s"), *GetDebugNodeNameString(InNode), *Error->GetErrorType().ToString(), *Error->GetErrorDescription().ToString());
				}
			}

			if (Operator.IsValid())
			{
				Operator->BindInputs(InterfaceData.GetInputs());
				Operator->BindOutputs(InterfaceData.GetOutputs());

				FOperatorInfo OpInfo
				{
					MoveTemp(Operator),
					MoveTemp(InterfaceData)
				};

				return MakeUnique<FAddOperator>(OperatorID, InLocation, MoveTemp(OpInfo));
			}
			else
			{
				return TUniquePtr<IDynamicOperatorTransform>(nullptr);
			}
		}


		void FDynamicOperatorTransactor::EnqueueTransformOnOperatorQueues(FCreateTransformFunctionRef InFunc)
		{
			TArray<FDynamicOperatorInfo>::TIterator OperatorInfoIterator = OperatorInfos.CreateIterator();
			while (OperatorInfoIterator)
			{
				TSharedPtr<TSpscQueue<TUniquePtr<IDynamicOperatorTransform>>> OperatorQueue = OperatorInfoIterator->Queue.Pin();
				if (OperatorQueue.IsValid())
				{
					TUniquePtr<IDynamicOperatorTransform> Transform = InFunc(OperatorInfoIterator->OperatorSettings, OperatorInfoIterator->Environment);
					if (Transform.IsValid())
					{
						OperatorQueue->Enqueue(MoveTemp(Transform));
					}
				}
				else
				{
					OperatorInfoIterator.RemoveCurrent();
				}
				OperatorInfoIterator++;
			}
		}
	}
}
