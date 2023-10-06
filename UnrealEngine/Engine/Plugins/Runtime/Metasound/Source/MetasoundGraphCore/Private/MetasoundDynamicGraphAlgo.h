// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "MetasoundDynamicOperatorTransactor.h"
#include "MetasoundGraphAlgoPrivate.h"
#include "MetasoundOperatorInterface.h"
#include "MetasoundVertexData.h"

namespace Metasound
{
	namespace DynamicGraph
	{
		struct FDynamicGraphOperatorData;

		using FOperatorInfo = DirectedGraphAlgo::FGraphOperatorData::FOperatorInfo;
		using FOperatorID = DirectedGraphAlgo::FOperatorID;

		/* Convenience wrapper for execute function of an IOperator. */
		struct FExecuteEntry final
		{
			FExecuteEntry(FOperatorID InOperatorID, IOperator& InOperator, IOperator::FExecuteFunction InFunc);

			void Execute()
			{
				check(Operator);
				check(Function);
				Function(Operator);
			}

			FOperatorID OperatorID;
			IOperator* Operator;
			IOperator::FExecuteFunction Function;	
		};

		/* Convenience wrapper for post execute function of an IOperator. */
		struct FPostExecuteEntry final
		{
			FPostExecuteEntry(FOperatorID InOperatorID, IOperator& InOperator, IOperator::FPostExecuteFunction InFunc);

			void PostExecute()
			{
				check(Operator);
				check(Function);
				Function(Operator);
			}

			FOperatorID OperatorID;
			IOperator* Operator;
			IOperator::FPostExecuteFunction Function;	
		};

		/* Convenience wrapper for reset function of an IOperator. */
		struct FResetEntry final
		{
			FResetEntry(FOperatorID InOperatorID, IOperator& InOperator, IOperator::FResetFunction InFunc);

			void Reset(const IOperator::FResetParams& InParams)
			{
				check(Operator);
				check(Function);
				Function(Operator, InParams);
			}

			FOperatorID OperatorID;
			IOperator* Operator;
			IOperator::FResetFunction Function;	
		};

		/** Collection of data needed to support a dynamic operator*/
		struct FDynamicGraphOperatorData : DirectedGraphAlgo::FGraphOperatorData
		{
			FDynamicGraphOperatorData(const FOperatorSettings& InSettings);
			FDynamicGraphOperatorData(DirectedGraphAlgo::FGraphOperatorData&& InGraphOperatorData);
			FDynamicGraphOperatorData(DirectedGraphAlgo::FGraphOperatorData&& InGraphOperatorData, const FDynamicOperatorUpdateCallbacks& InCallbacks);

			// Initialize the Execute/PostExecute/Reset tables.
			void InitTables();

			// A collection of optional callbacks which can be invoked when various
			// updates are made to this collection of data.
			FDynamicOperatorUpdateCallbacks OperatorUpdateCallbacks;

			TArray<FExecuteEntry> ExecuteTable;
			TArray<FPostExecuteEntry> PostExecuteTable;
			TArray<FResetEntry> ResetTable;
		};

		/** Update the runtime table entries.
		 * 
		 * This updates the function tables in the FDynamicGraphOperatorData for a given operator. For an operator to exist in the function tables
		 * it must provide a function to be called from GetExecutionFunction(), GetPostExecutionFunction() and/or GetResetFunction() and the operator's ID
		 * must also be present in the FDynamicGraphOperatorData's operator order array.  If either of these are not true, then the operator will 
		 * not have an entry in a specific execution table. 
		 * 
		 * Note: This function does not update the order of the entry in the execution table. 
		 * 
		 * @param InOperatorID - ID of operator to update.
		 * @param InOperator - Pointer to operator.
		 * @param InOutGraphOperatorData - Structure containing the operator order and runtime tables.
		 */
		void UpdateGraphRuntimeTableEntries(const FOperatorID& InOperatorID, IOperator* InOperator, FDynamicGraphOperatorData& InOutGraphOperatorData);

		/** Propagate FVertexInterfaceData updates through the operators in the Dynamic Graph Operator Data. 
		 *
		 * A change to an operator's input may result in a change to the operator's output. The updates to the
		 * operator's output and any subsequent knock-on operator output updates need to be propagated through
		 * all the relevant operators in the graph.
		 *
		 * @param InInitialOperatorID - ID of operator which will have it's input updated.
		 * @param InVertexName - Vertex name on the initial operator which will have it's input updated.
		 * @param InNewReference - New data reference to apply to the operators input vertex.
		 * @param InOutGraphOperatorData - Graph data which will be updated with new references. 
		 */
		void PropagateBindUpdate(FOperatorID InInitialOperatorID, const FVertexName& InVertexName, const FAnyDataReference& InNewReference, FDynamicGraphOperatorData& InOutGraphOperatorData);

		/** Iterate through the output operators and make force that their output data references
		 * are reflected in the graph's FOutputVertexInterfaceData 
		 */
		void UpdateOutputVertexData(FDynamicGraphOperatorData& InOutGraphOperatorData);

		/** Rebinds an operator which is wrapping another operator. */
		void RebindWrappedOperator(const FOperatorID& InOperatorID, FOperatorInfo& InOperatorInfo, FDynamicGraphOperatorData& InGraphOperatorData);

		/** Rebind the graph inputs, updating internal operator bindings as needed. */
		void RebindGraphInputs(FInputVertexInterfaceData& InOutVertexData, FDynamicGraphOperatorData& InOutGraphOperatorData);

		/** Rebind the graph inputs, updating internal operator bindings as needed. */
		void RebindGraphOutputs(FOutputVertexInterfaceData& InOutVertexData, FDynamicGraphOperatorData& InOutGraphOperatorData);
	}
}

