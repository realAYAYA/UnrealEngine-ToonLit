// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/Array.h"
#include "MetasoundBuilderInterface.h"
#include "MetasoundNode.h"
#include "MetasoundNodeInterface.h"
#include "MetasoundVertex.h"


namespace Metasound
{
	/** FGraph contains the edges between nodes as well as input and output 
	 * vertices.  FGraph does not maintain ownership over any node. Nodes used
	 * within the graph must be valid for the lifetime of the graph. 
	 */
	class METASOUNDGRAPHCORE_API FGraph : public IGraph
	{
		public:

			FGraph(const FString& InInstanceName, const FGuid& InInstanceID);

			virtual ~FGraph() = default;

			/** Return the name of this specific instance of the node class. */
			virtual const FVertexName& GetInstanceName() const override;

			/** Return the ID of this specific instance of the node class. */
			virtual const FGuid& GetInstanceID() const override;

			/** Return metadata about this graph. */
			virtual const FNodeClassMetadata& GetMetadata() const override;

			/** Retrieve all the edges associated with a graph. */
			virtual const TArray<FDataEdge>& GetDataEdges() const override;

			/** Return the current vertex interface. */
			virtual const FVertexInterface& GetVertexInterface() const override;

			/** Set the vertex interface. If the vertex was successfully changed, returns true. 
			 *
			 * @param InInterface - New interface for node. 
			 *
			 * @return True on success, false otherwise.
			 */
			virtual bool SetVertexInterface(const FVertexInterface& InInterface) override;

			/** Expresses whether a specific vertex interface is supported.
			 *
			 * @param InInterface - New interface. 
			 *
			 * @return True if the interface is supported, false otherwise. 
			 */
			virtual bool IsVertexInterfaceSupported(const FVertexInterface& InInterface) const override;

			/** Get vertices which contain input parameters. */
			virtual const FInputDataDestinationCollection& GetInputDataDestinations() const override;

			/** Get vertices which contain output parameters. */
			virtual const FOutputDataSourceCollection& GetOutputDataSources() const override;

			/** Add an edge to the graph. */
			void AddDataEdge(const FDataEdge& InEdge);

			/** Add an edge to the graph, connecting two vertices from two 
			 * nodes. 
			 *
			 * @param FromNode - Node which contains the output vertex.
			 * @param FromVertexKey - Key of the vertex in the FromNode.
			 * @param ToNode - Node which contains the input vertex.
			 * @param ToVertexKey - Key of the vertex in the ToNode.
			 *
			 * @return True if the edge was successfully added. False otherwise.
			 */
			bool AddDataEdge(const INode& FromNode, const FVertexName& FromVertexKey, const INode& ToNode, const FVertexName& ToVertexKey);

			/** Remove the given data edge. 
			 *
			 * @return True on success, false on failure.
			 */
			bool RemoveDataEdge(const INode& FromNode, const FVertexName& FromVertexKey, const INode& ToNode, const FVertexName& ToVertexKey);

			/** Removes all edges for which that predicate returns true.
			 *
			 * @param Predicate - A callable object which accepts an FDataEdge and returns true if
			 *                    the edge should be removed.
			 */
			template<typename PredicateType>
			UE_DEPRECATED(5.3, "Removing data edges by predicate is no longer supported")
			void RemoveDataEdgeByPredicate(const PredicateType& Predicate)
			{
				Edges.RemoveAllSwap(Predicate);
			}

			/** Removes all edges with connected to the node. */
			void RemoveDataEdgesWithNode(const INode& InNode);

			/** Store a node on this graph. 
			 *
			 * @param InNodeID - The NodeID related to the parent FMetasoundFrontendClass.
			 * @param InNode - A shared pointer to a node. 
			 */
			void AddNode(const FGuid& InNodeID, TSharedPtr<const INode> InNode);

			/** Retrieve node by node ID.
			 *
			 * @param InNodeID - The NodeID of the requested Node.
			 *
			 * @return Pointer to the Node if it is stored on this graph. nullptr otherwise. 
			 */
			const INode* FindNode(const FGuid& InNodeID) const;

			/** Populates an array with all nodes which exist in the graph but do not have
			 * any connections.
			 * 
			 * @param OutUnconnectedNodes - Array to populate.
			 * @return Number of unconnected nodes found.
			 */
			int32 FindUnconnectedNodes(TArray<TPair<FGuid, const INode*>>& OutUnconnectedNodes) const;

			/** Removes node from graph.
			 *
			 * @param InNodeID - ID of node to remove.
			 * @param bInRemoveDataEdgesWithNode - If true, will also call RemoveDataEdgesWithNode(...)
			 *
			 * @return true if node exists and is removed, false otherwise. 
			 */
			bool RemoveNode(const FGuid& InNodeID, bool bInRemoveDataEdgesWithNode = true);

			/** Add an input data destination to describe how data provided 
			 * outside this graph should be routed internally.
			 *
			 * @param InNode - Node which receives the data.
			 * @param InVertexName - Key for input vertex on InNode.
			 *
			 * @return True if the destination was successfully added. False 
			 * otherwise.
			 */
			bool AddInputDataDestination(const INode& InNode, const FVertexName& InVertexName);


			/** Add an input data destination to describe how data provided 
			 * outside this graph should be routed internally.
			 */
			void AddInputDataDestination(const FInputDataDestination& InDestination);

			/** Remove an input data destination by vertex name
			 * 
			 * @param InVertexName - Name of vertex to remove.
			 *
			 * @return True if destination existed and was successfully removed. False otherwise.
			 */
			bool RemoveInputDataDestination(const FVertexName& InVertexName);

			/** Add an output data source which describes routing of data which is 
			 * owned this graph and exposed externally.
			 *
			 * @param InNode - Node which produces the data.
			 * @param InVertexName - Key for output vertex on InNode.
			 *
			 * @return True if the source was successfully added. False 
			 * otherwise.
			 */
			bool AddOutputDataSource(const INode& InNode, const FVertexName& InVertexName);

			/** Add an output data source which describes routing of data which is 
			 * owned this graph and exposed externally.
			 */
			void AddOutputDataSource(const FOutputDataSource& InSource);

			/** Remove an output data source by vertex name
			 * 
			 * @param InVertexName - Name of vertex to remove.
			 *
			 * @return True if source existed and was successfully removed. False otherwise.
			 */
			bool RemoveOutputDataSource(const FVertexName& InVertexName);

			/** Return a reference to the default operator factory. */
			virtual FOperatorFactorySharedRef GetDefaultOperatorFactory() const override;

		private:

			class FFactory : public IOperatorFactory
			{
			public:
				virtual ~FFactory() = default;

				virtual TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults) override;
			};

			FVertexName InstanceName;
			FGuid InstanceID;

			FNodeClassMetadata Metadata;

			TArray<FDataEdge> Edges;
			TSortedMap<FGuid, TSharedPtr<const INode>> Nodes;

			FInputDataDestinationCollection InputDestinations;
			FOutputDataSourceCollection OutputSources;
	};
}
