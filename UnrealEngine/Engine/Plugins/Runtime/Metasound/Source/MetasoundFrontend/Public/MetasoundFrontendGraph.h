// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "MetasoundFrontend.h"
#include "MetasoundGraph.h"
#include "MetasoundNodeInterface.h"

namespace Metasound
{
	/** FFrontendGraph is a utility graph for use in the frontend. It can own nodes
	 * that live within the graph and provides query interfaces for finding nodes
	 * by dependency ID or input/output index. 
	 */
	class METASOUNDFRONTEND_API FFrontendGraph : public FGraph
	{
		public:
			/** FFrontendGraph constructor.
			 *
			 * @parma InInstanceName - Name of this graph.
			 * @parma InInstanceID - ID of this graph.
			 */
			FFrontendGraph(const FString& InInstanceName, const FGuid& InInstanceID);

			virtual ~FFrontendGraph() = default;

			/** Add an input node to this graph.
			 *
			 * @param InNodeID - The NodeID related to the parent FMetasoundFrontendClass.
			 * @param InIndex - The positional index for the input.
			 * @param InVertexName - The key for the graph input vertex.
			 * @param InNode - A shared pointer to an input node. 
			 */
			void AddInputNode(FGuid InNodeID, int32 InIndex, const FVertexName& InVertexName, TSharedPtr<const INode> InNode);

			/** Add an output node to this graph.
			 *
			 * @param InNodeID - The NodeID related to the parent FMetasoundFrontendClass.
			 * @param InIndex - The positional index for the output.
			 * @param InVertexName - The key for the graph output vertex.
			 * @param InNode - A shared pointer to an output node. 
			 */
			void AddOutputNode(FGuid InNodeID, int32 InIndex, const FVertexName& InVertexName, TSharedPtr<const INode> InNode);

			/** Store a node on this graph. 
			 *
			 * @param InNodeID - The NodeID related to the parent FMetasoundFrontendClass.
			 * @param InNode - A shared pointer to a node. 
			 */
			void AddNode(FGuid InNodeID, TSharedPtr<const INode> InNode);

			/** Retrieve node by node ID.
			 *
			 * @param InNodeID - The NodeID of the requested Node.
			 *
			 * @return Pointer to the Node if it is stored on this graph. nullptr otherwise. 
			 */
			const INode* FindNode(FGuid InNodeID) const;

			/** Retrieve node by input index.
			 *
			 * @param InIndex - The index of the requested input.
			 *
			 * @return Pointer to the Node if it is stored on this graph. nullptr otherwise. 
			 */
			const INode* FindInputNode(int32 InIndex) const;

			/** Retrieve node by output index.
			 *
			 * @param InIndex - The index of the requested output.
			 *
			 * @return Pointer to the Node if it is stored on this graph. nullptr otherwise. 
			 */
			const INode* FindOutputNode(int32 InIndex) const;

			/** Returns true if all edges, destinations and sources refer to 
			 * nodes stored in this graph. */
			bool OwnsAllReferencedNodes() const;

		private:

			void StoreNode(TSharedPtr<const INode> InNode);

			TMap<int32, const INode*> InputNodes;
			TMap<int32, const INode*> OutputNodes;

			TMap<FGuid, const INode*> NodeMap;
			TSet<const INode*> StoredNodes;
			TArray<TSharedPtr<const INode>> NodeStorage;
	};

	/** FFrontendGraphBuilder builds a FFrontendGraph from a FMetasoundDoucment
	 * or FMetasoundFrontendClass.
	 */
	class METASOUNDFRONTEND_API FFrontendGraphBuilder
	{
	public:

		/** Check that all dependencies are C++ class dependencies. 
		 * 
		 * @param InDocument - Document containing dependencies.
		 *
		 * @return True if all dependencies are C++ classes. False otherwise.
		 */
		static bool IsFlat(const FMetasoundFrontendDocument& InDocument);

		static bool IsFlat(const FMetasoundFrontendGraphClass& InRoot, const TArray<FMetasoundFrontendClass>& InDependencies);

		/* Metasound document should be in order to create this graph. */
		static TUniquePtr<FFrontendGraph> CreateGraph(const FMetasoundFrontendDocument& InDocument, const TSet<FName>& TransmittableInputNames, const FString& InDebugAssetName);

		/* Metasound document should be in order to create this graph. */
		static TUniquePtr<FFrontendGraph> CreateGraph(const FMetasoundFrontendGraphClass& InGraphClass, const TArray<FMetasoundFrontendGraphClass>& InSubgraphs, const TArray<FMetasoundFrontendClass>& InDependencies, const TSet<FName>& TransmittableInputNames, const FString& InDebugAssetName);

	private:
		struct FDefaultLiteralData
		{
			FGuid DestinationNodeID;
			FGuid DestinationVertexID;
			FVertexName DestinationVertexKey;
			FName TypeName;
			FLiteralNodeConstructorParams InitParams;
		};

		// Map of Input VertexID to variable data required to construct and connect default variable
		using FNodeIDVertexID = TTuple<FGuid, FGuid>;
		using FDependencyByIDMap = TMap<FGuid, const FMetasoundFrontendClass*>;
		using FSharedNodeByIDMap = TMap<FGuid, TSharedPtr<const INode>>;
		using FDefaultInputByIDMap = TMap<FNodeIDVertexID, FDefaultLiteralData>;

		// Context used throughout entire graph build process
		// (for both a root and nested subgraphs)
		struct FBuildContext
		{
			FString DebugAssetName;
			FDependencyByIDMap FrontendClasses;
			FSharedNodeByIDMap Graphs;
		};

		// Transient context used for building a specific graph
		struct FBuildGraphContext
		{
			TUniquePtr<FFrontendGraph> Graph;
			const FMetasoundFrontendGraphClass& GraphClass;
			FBuildContext& BuildContext;
			FDefaultInputByIDMap DefaultInputs;
		};

		static TArray<FDefaultLiteralData> GetInputDefaultLiteralData(const FMetasoundFrontendNode& InNode, const FNodeInitData& InInitData, const TSet<FNodeIDVertexID>& InEdgeDestinations);

		static bool SortSubgraphDependencies(TArray<const FMetasoundFrontendGraphClass*>& Subgraphs);

		static TUniquePtr<FFrontendGraph> CreateGraph(FBuildContext& InContext, const FMetasoundFrontendGraphClass& InSubgraph, const TSet<FName>& TransmittableInputNames);

		static const FMetasoundFrontendClassInput* FindClassInputForInputNode(const FMetasoundFrontendGraphClass& InOwningGraph, const FMetasoundFrontendNode& InInputNode, int32& OutClassInputIndex);
		static const FMetasoundFrontendClassOutput* FindClassOutputForOutputNode(const FMetasoundFrontendGraphClass& InOwningGraph, const FMetasoundFrontendNode& InOutputNode, int32& OutClassOutputIndex);
		static const FMetasoundFrontendLiteral* FindInputLiteralForInputNode(const FMetasoundFrontendNode& InInputNode, const FMetasoundFrontendClass& InInputNodeClass, const FMetasoundFrontendClassInput& InOwningGraphClassInput);
		static const FMetasoundFrontendVariable* FindVariableForVariableNode(const FMetasoundFrontendNode& InVariableNode, const FMetasoundFrontendGraph& InGraph);

		static TUniquePtr<INode> CreateInputNode(const FMetasoundFrontendNode& InNode, const FMetasoundFrontendClass& InClass, const FMetasoundFrontendClassInput& InOwningGraphClassInput, bool bEnableTransmission);
		static TUniquePtr<INode> CreateOutputNode(const FMetasoundFrontendNode& InNode, const FMetasoundFrontendClass& InClass, FBuildGraphContext& InGraphContext, const TSet<FNodeIDVertexID>& InEdgeDestinations);
		static TUniquePtr<INode> CreateVariableNode(const FMetasoundFrontendNode& InNode, const FMetasoundFrontendGraph& InGraph);
		static TUniquePtr<INode> CreateExternalNode(const FMetasoundFrontendNode& InNode, const FMetasoundFrontendClass& InClass, FBuildGraphContext& InGraphContext, const TSet<FNodeIDVertexID>& InEdgeDestinations);

		// Returns false on error
		static bool AddNodesToGraph(FBuildGraphContext& InGraphContext, const TSet<FName>& TransmittableInputNames);

		// Returns false on error
		static bool AddEdgesToGraph(FBuildGraphContext& InGraphContext);

		// Returns false on error
		static bool AddDefaultInputLiterals(FBuildGraphContext& InGraphContext);
	};
}
