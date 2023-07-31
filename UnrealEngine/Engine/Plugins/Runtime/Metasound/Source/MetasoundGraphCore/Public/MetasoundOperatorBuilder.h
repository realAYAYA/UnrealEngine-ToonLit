// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "MetasoundBuilderInterface.h"
#include "MetasoundEnvironment.h"
#include "MetasoundGraphOperator.h"
#include "MetasoundNodeInterface.h"
#include "MetasoundOperatorBuilderSettings.h"
#include "MetasoundOperatorInterface.h"
#include "MetasoundVertexData.h"
#include "Templates/UniquePtr.h"

namespace Metasound
{
	// Forward declare.
	class FDirectedGraphAlgoAdapter;
	class FOperatorSettings;
	class FMetasoundEnvironment;

	/** FOperatorBuilder builds an IOperator from an IGraph. */
	class METASOUNDGRAPHCORE_API FOperatorBuilder : public IOperatorBuilder
	{
		public:

			/** FOperatorBuilder constructor.
			 *
			 * @param InBuilderSettings  - Settings to configure builder options.
			 */
			FOperatorBuilder(const FOperatorBuilderSettings& InBuilderSettings);

			virtual ~FOperatorBuilder();

			/** Create an IOperator from an IGraph.
			 *
			 * @param InParams   - Params of the current build
			 * @param OutResults - Results data pertaining to the given build operator result.
			 *
			 * @return A TUniquePtr to an IOperator. If the processes was unsuccessful, 
			 *         the returned pointer will contain a nullptr and be invalid.
			 */
			virtual TUniquePtr<IOperator> BuildGraphOperator(const FBuildGraphOperatorParams& InParams, FBuildResults& OutResults) const override;

		private:

			using FNodeEdgeMultiMap = TMultiMap<const INode*, const FDataEdge*>;
			using FNodeDestinationMap = TMap<const INode*, const FInputDataDestination*>;
			using FNodeVertexInterfaceDataMap = TMap<const INode*, FVertexInterfaceData>;
			using FOperatorPtr = TUniquePtr<IOperator>;

			// Handles build status of current build operation.
			struct FBuildStatus
			{
				// Enumeration of build status states. 
				//
				// Note: plain enum used here instead of enum class so that implicit 
				// conversion to int32 can be utilized. It is assumed that the 
				// build status int32 values increase as the build status deteriorates.
				// Build statuses are merged by taking the maximum int32 value of
				// the EStatus. 
				enum EStatus
				{
					// No error has been encountered.
					NoError = 0,
					
					// A non fatal error has been encountered.
					NonFatalError = 1,

					// A fatal error has been encountered.
					FatalError = 2
				};


				FBuildStatus() = default;

				FBuildStatus(FBuildStatus::EStatus InStatus)
				:	Value(InStatus)
				{
				}

				// Merge build statuses by taking the maximum of EStatus.
				FBuildStatus& operator |= (FBuildStatus RHS)
				{
					Value = Value > RHS.Value ? Value : RHS.Value;
					return *this;
				}

				operator EStatus() const
				{
					return Value;
				}

			private:
				EStatus Value = NoError;
			};

			struct FBuildContext
			{
				const IGraph& Graph;
				const FDirectedGraphAlgoAdapter& AlgoAdapter;
				const FOperatorSettings& Settings;
				const FMetasoundEnvironment& Environment;
				const FOperatorBuilderSettings& BuilderSettings;

				FBuildResults& Results;

				TArray<FOperatorPtr> Operators;
				FNodeVertexInterfaceDataMap DataReferences;

				FBuildContext(
					const IGraph& InGraph,
					const FDirectedGraphAlgoAdapter& InAlgoAdapter,
					const FOperatorSettings& InSettings,
					const FMetasoundEnvironment& InEnvironment,
					const FOperatorBuilderSettings& bInBuilderSettings,
					FBuildResults& OutResults)
				:	Graph(InGraph)
				,	AlgoAdapter(InAlgoAdapter)
				,	Settings(InSettings)
				,	Environment(InEnvironment)
				,	BuilderSettings(bInBuilderSettings)
				,	Results(OutResults)
				{
				}
			};

			// Perform topological sort using depth first algorithm.
			FBuildStatus DepthFirstTopologicalSort(FBuildContext& InOutContext, TArray<const INode*>& OutNodes) const;

			// Perform topological sort using kahns algorithm.
			FBuildStatus KahnsTopologicalSort(FBuildContext& InOutContext, TArray<const INode*>& OutNodes) const;

			// Prune unreachable nodes from InOutNodes
			FBuildStatus PruneNodes(FBuildContext& InOutContext, TArray<const INode*>& InOutNodes) const;

			// Get all input data references for a given node for inputs provided internally to the graph.
			FBuildStatus GatherInputDataReferences(FBuildContext& InOutContext, const INode* InNode, const FNodeEdgeMultiMap& InEdgeMap, FInputVertexInterfaceData& OutVertexData) const;

			// Get all input data references for a given node for inputs provided externally to the graph.
			FBuildStatus GatherExternalInputDataReferences(FBuildContext& InContext, const INode* InNode, const FNodeDestinationMap& InNodeDestinationMap, const FInputVertexInterfaceData& InExternalCollection, FInputVertexInterfaceData& OutVertexData) const;

			// Graphs all internal graph references and places them in output map.
			void GatherInternalGraphDataReferences(FBuildContext& InOutContext, TMap<FGuid, FDataReferenceCollection>& OutNodeVertexData) const;

			// Validates whether all operator outputs are bound to data references. 
			FBuildStatus ValidateOperatorOutputsAreBound(const INode& InNode, const FOutputVertexInterfaceData& InVertexData) const;

			// Get all input/output data references for a given graph.
			FBuildStatus GatherGraphDataReferences(FBuildContext& InOutContext, FVertexInterfaceData& OutVertexData) const;

			// Call the operator factories for the nodes
			FBuildStatus CreateOperators(FBuildContext& InOutContext, const TArray<const INode*>& InSortedNodes, const FInputVertexInterfaceData& InExternalInputData) const;

			// Create the final graph operator from the provided build context.
			TUniquePtr<IOperator> CreateGraphOperator(FBuildContext& InOutContext) const;

			FBuildStatus::EStatus GetMaxErrorLevel() const;

			FOperatorBuilderSettings BuilderSettings;
	};
}

