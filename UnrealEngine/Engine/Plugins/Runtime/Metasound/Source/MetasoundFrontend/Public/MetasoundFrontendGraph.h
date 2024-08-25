// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Containers/UnrealString.h"
#include "MetasoundFrontendDocument.h"
#include "MetasoundGraph.h"
#include "MetasoundNodeConstructorParams.h"
#include "MetasoundNodeInterface.h"
#include "MetasoundVertex.h"
#include "Misc/Guid.h"
#include "Templates/SharedPointer.h"
#include "Templates/UniquePtr.h"

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

			UE_DEPRECATED(5.3, "This function is no longer analyzes node ownership and will always return true.")
			bool OwnsAllReferencedNodes() const;

		private:

			TMap<int32, const INode*> InputNodes;
			TMap<int32, const INode*> OutputNodes;
	};

	/** FFrontendGraphBuilder builds a FFrontendGraph from a FMetasoundDocument
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

		/* Create a FFrontendGraph from a FMetasoundFrontendDocument.*/
		UE_DEPRECATED(5.4, "Use the version of CreateGraph(...) which does not include \"TransmittableInputNames\".")
		static TUniquePtr<FFrontendGraph> CreateGraph(const FMetasoundFrontendDocument& InDocument, const TSet<FName>& TransmittableInputNames, const FString& InDebugAssetName);

		/* Create a FFrontendGraph from a FMetasoundFrontendDocument subobjects.*/
		UE_DEPRECATED(5.4, "Use the version of CreateGraph(...) which does not include \"TransmittableInputNames\".")
		static TUniquePtr<FFrontendGraph> CreateGraph(const FMetasoundFrontendGraphClass& InGraphClass, const TArray<FMetasoundFrontendGraphClass>& InSubgraphs, const TArray<FMetasoundFrontendClass>& InDependencies, const TSet<FName>& TransmittableInputNames, const FString& InDebugAssetName);

		/* Create a FFrontendGraph from a FMetasoundFrontendDocument.*/
		static TUniquePtr<FFrontendGraph> CreateGraph(const FMetasoundFrontendDocument& InDocument, const FString& InDebugAssetName);

		/* Create a FFrontendGraph from a FMetasoundFrontendDocument retrieving proxies from a FProxyDataCache.*/
		static TUniquePtr<FFrontendGraph> CreateGraph(const FMetasoundFrontendDocument& InDocument, const Frontend::FProxyDataCache& InProxies, const FString& InDebugAssetName);

		/* Create a FFrontendGraph from a FMetasoundFrontendDocument subobjects retrieving proxies from a FProxyDataCache.*/
		static TUniquePtr<FFrontendGraph> CreateGraph(const FMetasoundFrontendGraphClass& InGraph, const TArray<FMetasoundFrontendGraphClass>& InSubgraphs, const TArray<FMetasoundFrontendClass>& InDependencies, const FString& InDebugAssetName);

		/* Create a FFrontendGraph from a FMetasoundFrontendDocument subobjects retrieving proxies from a FProxyDataCache.*/
		static TUniquePtr<FFrontendGraph> CreateGraph(const FMetasoundFrontendGraphClass& InGraph, const TArray<FMetasoundFrontendGraphClass>& InSubgraphs, const TArray<FMetasoundFrontendClass>& InDependencies, const Frontend::FProxyDataCache& InProxyDataCache, const FString& InDebugAssetName);

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
			const Frontend::IDataTypeRegistry& DataTypeRegistry;
			const Frontend::FProxyDataCache& ProxyDataCache;
		};

		// Transient context used for building a specific graph
		struct FBuildGraphContext
		{
			TUniquePtr<FFrontendGraph> Graph;
			const FMetasoundFrontendGraphClass& GraphClass;
			FBuildContext& BuildContext;
			FDefaultInputByIDMap DefaultInputs;
		};


		static TArray<FDefaultLiteralData> GetInputDefaultLiteralData(const FBuildContext& InContext, const FMetasoundFrontendNode& InNode, const FNodeInitData& InInitData, const TSet<FNodeIDVertexID>& InEdgeDestinations);

		static bool SortSubgraphDependencies(TArray<const FMetasoundFrontendGraphClass*>& Subgraphs);

		static TUniquePtr<FFrontendGraph> CreateGraph(FBuildContext& InContext, const FMetasoundFrontendGraphClass& InSubgraph);

		static const FMetasoundFrontendClassInput* FindClassInputForInputNode(const FMetasoundFrontendGraphClass& InOwningGraph, const FMetasoundFrontendNode& InInputNode, int32& OutClassInputIndex);
		static const FMetasoundFrontendClassOutput* FindClassOutputForOutputNode(const FMetasoundFrontendGraphClass& InOwningGraph, const FMetasoundFrontendNode& InOutputNode, int32& OutClassOutputIndex);
		static const FMetasoundFrontendLiteral* FindInputLiteralForInputNode(const FMetasoundFrontendNode& InInputNode, const FMetasoundFrontendClass& InInputNodeClass, const FMetasoundFrontendClassInput& InOwningGraphClassInput);
		static const FMetasoundFrontendVariable* FindVariableForVariableNode(const FMetasoundFrontendNode& InVariableNode, const FMetasoundFrontendGraph& InGraph);

		static TUniquePtr<INode> CreateInputNode(const FBuildContext& InContext, const FMetasoundFrontendNode& InNode, const FMetasoundFrontendClass& InClass, const FMetasoundFrontendClassInput& InOwningGraphClassInput);
		static TUniquePtr<INode> CreateOutputNode(const FBuildContext& InContext, const FMetasoundFrontendNode& InNode, const FMetasoundFrontendClass& InClass, FBuildGraphContext& InGraphContext, const TSet<FNodeIDVertexID>& InEdgeDestinations);
		static TUniquePtr<INode> CreateVariableNode(const FBuildContext& InContext, const FMetasoundFrontendNode& InNode, const FMetasoundFrontendGraph& InGraph);
		static TUniquePtr<INode> CreateExternalNode(const FBuildContext& InContext, const FMetasoundFrontendNode& InNode, const FMetasoundFrontendClass& InClass, FBuildGraphContext& InGraphContext, const TSet<FNodeIDVertexID>& InEdgeDestinations);

		// Returns false on error
		static bool AddNodesToGraph(FBuildGraphContext& InGraphContext);

		// Returns false on error
		static bool AddEdgesToGraph(FBuildGraphContext& InGraphContext);

		// Returns false on error
		static bool AddDefaultInputLiterals(FBuildGraphContext& InGraphContext);
	};
}
