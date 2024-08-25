// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/SpscQueue.h"
#include "Containers/UnrealString.h"
#include "MetasoundBuilderInterface.h"
#include "MetasoundGraph.h"
#include "MetasoundOperatorBuilder.h"
#include "MetasoundOperatorInterface.h"
#include "MetasoundNodeInterface.h"
#include "Templates/Function.h"
#include "Templates/SharedPointer.h"
#include "Templates/UniquePtr.h"

namespace Metasound
{
	class FOperatorSettings;
	struct FLiteral;
	class FAnyDataReference;

	namespace DynamicGraph
	{
		// Forward declare
		class IDynamicOperatorTransform;
		class FDynamicOperator;
		enum class EAudioFadeType : uint8;
		enum class EExecutionOrderInsertLocation : uint8;

		using FLiteralAssignmentFunction = void(*)(const FOperatorSettings& InOperatorSettings, const FLiteral& InLiteral, const FAnyDataReference& OutDataRef);
		using FReferenceCreationFunction = TOptional<FAnyDataReference>(*)(const FOperatorSettings& InSettings, FName DataType, const FLiteral& InLiteral, EDataReferenceAccessType InAccessType);
		using FOnInputVertexUpdated = TFunction<void(const FVertexName&, const FInputVertexInterfaceData&)>;
		using FOnOutputVertexUpdated = TFunction<void(const FVertexName&, const FOutputVertexInterfaceData&)>;

		/** A collection of callbacks for handling updates to MetaSound dynamic operators.
		 * 
		 * Callbacks are invoked on the same thread which executes the dynamic operator. 
		 */
		struct FDynamicOperatorUpdateCallbacks
		{
			FOnInputVertexUpdated OnInputAdded;
			FOnInputVertexUpdated OnInputRemoved;
			FOnOutputVertexUpdated OnOutputAdded;
			FOnOutputVertexUpdated OnOutputUpdated;
			FOnOutputVertexUpdated OnOutputRemoved;
		};

		/** The FDynamicOperatorTransactor is used for communicating with a dynamic
		 * MetaSound operator.
		 *
		 * Graph manipulations performed on the transactor are forwarded to dynamic
		 * operators using the transform queue. Each modification is converted into
		 * IDynamicOperatorTransforms which are consumed by dynamic operators during
		 * their execution.
		 */
		class METASOUNDGRAPHCORE_API FDynamicOperatorTransactor
		{
		public:
			FDynamicOperatorTransactor& operator=(const FDynamicOperatorTransactor&) = delete;
			FDynamicOperatorTransactor(const FDynamicOperatorTransactor&) = delete;

			FDynamicOperatorTransactor();
			FDynamicOperatorTransactor(const FGraph& InGraph);

			/** Create a queue for communication with a dynamic operator. */
			TSharedRef<TSpscQueue<TUniquePtr<IDynamicOperatorTransform>>> CreateTransformQueue(const FOperatorSettings& InOperatorSettings, const FMetasoundEnvironment& InEnvironment);

			/** Add a node to the graph. */
			void AddNode(const FGuid& InNodeID, TUniquePtr<INode> InNode);

			/** Remove a node from the graph. */
			void RemoveNode(const FGuid& InNodeID);

			/** Add an edge to the graph, connecting two vertices from two 
			 * nodes. 
			 *
			 * @param FromNode - Node which contains the output vertex.
			 * @param FromVertex - Key of the vertex in the FromNode.
			 * @param ToNode - Node which contains the input vertex.
			 * @param ToVertex - Key of the vertex in the ToNode.
			 */
			void AddDataEdge(const FGuid& InFromNodeID, const FVertexName& InFromVertex, const FGuid& InToNodeID, const FVertexName& InToVertex);

			/** Remove the given data edge. */
			void RemoveDataEdge(const FGuid& InFromNode, const FVertexName& InFromVertex, const FGuid& InToNode, const FVertexName& InToVertex, TUniquePtr<INode> InReplacementLiteralNode);

			/** Set the value on a unconnected node input vertex. */
			void SetValue(const FGuid& InNodeID, const FVertexName& InVertex, TUniquePtr<INode> InLiteralNode);

			/** Add an input data destination to describe how data provided 
			 * outside this graph should be routed internally.
			 *
			 * @param InNode - Node which receives the data.
			 * @param InVertexName - Key for input vertex on InNode.
			 */
			void AddInputDataDestination(const FGuid& InNode, const FVertexName& InVertexName, const FLiteral& InDefaultLiteral, FReferenceCreationFunction InFunc);

			/** Remove an exposed input from the graph. */
			void RemoveInputDataDestination(const FVertexName& InVertexName);

			/** Add an output data source which describes routing of data which is 
			 * owned this graph and exposed externally.
			 *
			 * @param InNode - Node which produces the data.
			 * @param InVertexName - Key for output vertex on InNode.
			 */
			void AddOutputDataSource(const FGuid& InNode, const FVertexName& InVertexName);

			/** Remove an exposed output from the graph. */
			void RemoveOutputDataSource(const FVertexName& InVertexName);

			/** Return internal version of graph. */
			const FGraph& GetGraph() const;

		private:
			using FOperatorID = uintptr_t;

			void EnqueueAddOperatorTransform(const INode& InNode, EExecutionOrderInsertLocation InLocation);
			void EnqueueFadeAndRemoveOperatorTransform(const INode& InNode, TArrayView<const FVertexName> InOutputsToFade);
			void EnqueueRemoveOperatorTransform(const INode& InNode);
			void EnqueueBeginFadeOperatorTransform(const INode& InNode, EAudioFadeType InFadeType, TArrayView<const FVertexName> InInputsToFade, TArrayView<const FVertexName> InOutputsToFade);
			void EnqueueEndFadeOperatorTransform(const INode& InNode);

			void EnqueueRemoveEdgeOperatorTransform(const INode& InFromNode, const FVertexName& InFromVertex, const INode& InToNode, const FVertexName& InToVertex, const INode& InReplacementLiteralNode);
			void EnqueueFadeAndRemoveEdgeOperatorTransform(const INode& InFromNode, const FVertexName& InFromVertex, const INode& InToNode, const FVertexName& InToVertex, const INode& InReplacementLiteralNode);
			void EnqueueAddEdgeOperatorTransform(const INode& InFromNode, const FVertexName& InFromVertex, const INode& InToNode, const FVertexName& InToVertex, const INode* InPriorLiteralNode);
			void EnqueueFadeAndAddEdgeOperatorTransform(const INode& InFromNode, const FVertexName& InFromVertex, const INode& InToNode, const FVertexName& InToVertex, const INode* InPriorLiteralNode);

			void AddDataEdgeInternal(const INode& InFromNode, const FVertexName& InFromVertex, const FGuid& InToNodeID, const INode& InToNode, const FVertexName& InToVertex);

			using FCreateTransformFunctionRef = TFunctionRef<TUniquePtr<IDynamicOperatorTransform>(const FOperatorSettings& InOperatorSettings, const FMetasoundEnvironment& InEnvironment)>;

			TUniquePtr<IDynamicOperatorTransform> CreateAddOperatorTransform(const INode& InNode, EExecutionOrderInsertLocation InLocation, const FOperatorSettings& InOperatorSettings, const FMetasoundEnvironment& InEnvironment) const;

			void EnqueueTransformOnOperatorQueues(FCreateTransformFunctionRef InFunc);

			FOperatorBuilder OperatorBuilder;
			FGraph Graph;

			struct FDynamicOperatorInfo
			{
				FOperatorSettings OperatorSettings;
				FMetasoundEnvironment Environment;
				TWeakPtr< TSpscQueue< TUniquePtr< IDynamicOperatorTransform >>> Queue;
			};

			TArray<FDynamicOperatorInfo> OperatorInfos;
			TArray<FOperatorID> CurrentOperatorOrder;

			struct FLiteralNodeID
			{
				FGuid ToNode;
				FVertexName ToVertex;
			};
			friend bool operator<(const FLiteralNodeID& InLHS, const FLiteralNodeID& InRHS);

			TSortedMap<FLiteralNodeID, TUniquePtr<INode>> LiteralNodeMap;
		};
	}
}

