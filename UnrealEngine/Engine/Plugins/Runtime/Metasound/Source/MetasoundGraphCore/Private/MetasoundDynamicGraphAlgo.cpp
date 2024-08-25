// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundDynamicGraphAlgo.h"

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "MetasoundDataReference.h"
#include "MetasoundGraphAlgoPrivate.h"
#include "MetasoundLog.h"
#include "MetasoundTrace.h"
#include "MetasoundVertex.h"
#include "MetasoundVertexData.h"

namespace Metasound
{
	namespace DynamicGraph
	{
		namespace DynamicGraphAlgoPrivate
		{
			template<typename EntryType, typename FunctionType>
			void UpdateTableEntry(const TArray<FOperatorID>& InOperatorOrder, const FOperatorID& InOperatorID, IOperator* InOperator, TArray<EntryType>& InOutTable, FunctionType InFunction)
			{
				using namespace DirectedGraphAlgo;

				int32 EntryIndex = InOutTable.IndexOfByPredicate([&](const EntryType& InEntry) { return InEntry.OperatorID == InOperatorID; });

				int32 OperatorIndex = InOperatorOrder.Find(InOperatorID);
				bool bEntryShouldExist = (nullptr != InOperator) && (nullptr != InFunction) && (INDEX_NONE != OperatorIndex); // Remove operators if they have no function for this table, or if they or not in the operator execution order. 
				bool bEntryCurrentlyExists = (EntryIndex != INDEX_NONE);

				if (bEntryCurrentlyExists && bEntryShouldExist)
				{
					// Update the existing entry
					InOutTable[EntryIndex].Function = InFunction;
					InOutTable[EntryIndex].Operator = InOperator;
				}
				else if (bEntryCurrentlyExists)
				{
					// Remove the existing entry
					InOutTable.RemoveAt(EntryIndex);
				}
				else if (bEntryShouldExist)
				{
					check(InOperator);
					// Add new entry
					int32 PreceedingOperatorIndex = OperatorIndex - 1;
					int32 InsertLocation = 0; // Default to inserting at the beginning
					while (PreceedingOperatorIndex >= 0)
					{
						FOperatorID PreceedingOperatorID = InOperatorOrder[PreceedingOperatorIndex];
						PreceedingOperatorIndex--;

						int32 PreceedingEntryIndex = InOutTable.IndexOfByPredicate([&](const EntryType& InEntry) { return InEntry.OperatorID == PreceedingOperatorID; });
						if (PreceedingEntryIndex != INDEX_NONE)
						{
							InsertLocation = PreceedingEntryIndex + 1;
							break;
						}
					}

					InOutTable.Insert(EntryType{InOperatorID, *InOperator, InFunction}, InsertLocation);
				}
			}

		} // namespace DynamicGraphAlgoPrivate

		void UpdateGraphRuntimeTableEntries(const FOperatorID& InOperatorID, IOperator* InOperator, FDynamicGraphOperatorData& InOutGraphOperatorData)
		{
			using namespace DynamicGraphAlgoPrivate;
			METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::DynamicGraphAlgo::UpdateOperatorRuntimeTableEntries)

			UpdateTableEntry(InOutGraphOperatorData.OperatorOrder, InOperatorID, InOperator, InOutGraphOperatorData.ExecuteTable, InOperator ? InOperator->GetExecuteFunction() : nullptr);
			UpdateTableEntry(InOutGraphOperatorData.OperatorOrder, InOperatorID, InOperator, InOutGraphOperatorData.PostExecuteTable, InOperator ? InOperator->GetPostExecuteFunction() : nullptr);
			UpdateTableEntry(InOutGraphOperatorData.OperatorOrder, InOperatorID, InOperator, InOutGraphOperatorData.ResetTable, InOperator ? InOperator->GetResetFunction() : nullptr);
		}

		// Apply updates to data references through all the operators by following connections described in the FOperatorInfo map.
		void PropagateBindUpdate(FOperatorID InInitialOperatorID, const FVertexName& InVertexName, const FAnyDataReference& InNewReference, FDynamicGraphOperatorData& InOutGraphOperatorData)
		{
			using namespace DirectedGraphAlgo;
			using namespace DynamicGraphAlgoPrivate;

			METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::DynamicGraphAlgo::PropagateBindUpdate)

			struct FInputToUpdate
			{
				FOperatorID OperatorID;
				FVertexName VertexName;
				FAnyDataReference DataReference;
			};

			TArray<FInputToUpdate> PropagateStack;
			PropagateStack.Emplace(FInputToUpdate{InInitialOperatorID, InVertexName, InNewReference});

			TArray<FVertexDataState> InitialOutputState;
			TSortedVertexNameMap<FAnyDataReference> OutputUpdates;
			while (PropagateStack.Num())
			{
				METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::DynamicGraphAlgo::PropagateBindUpdate_Iteration)
				FInputToUpdate Current = PropagateStack.Pop();
				if (FOperatorInfo* OpInfo = InOutGraphOperatorData.OperatorMap.Find(Current.OperatorID))
				{
					IOperator& Operator = *(OpInfo->Operator);
					// Get current outputs
					InitialOutputState.Reset();
					GetVertexInterfaceDataState(OpInfo->VertexData.GetOutputs(), InitialOutputState);

					// Set new input.
					OpInfo->VertexData.GetInputs().SetVertex(Current.VertexName, MoveTemp(Current.DataReference));

					// Bind inputs and outputs
					Operator.BindInputs(OpInfo->VertexData.GetInputs());
					Operator.BindOutputs(OpInfo->VertexData.GetOutputs());

					// Update execute/postexecute/reset tables in case those have changed after rebinding.
					UpdateGraphRuntimeTableEntries(Current.OperatorID, &Operator, InOutGraphOperatorData);

					// See if binding altered the outputs. 
					OutputUpdates.Reset();
					CompareVertexInterfaceDataToPriorState(OpInfo->VertexData.GetOutputs(), InitialOutputState, OutputUpdates);

					// Any updates to the outputs need to be propagated through the graph.
					for (const TPair<FVertexName, FAnyDataReference>& OutputUpdate : OutputUpdates)
					{
						const FVertexName& OutputVertexName = OutputUpdate.Get<0>();
						const FAnyDataReference& OutputDataReference = OutputUpdate.Get<1>();

						if (const TArray<FGraphOperatorData::FVertexDestination>* Destinations = OpInfo->OutputConnections.Find(OutputVertexName))
						{
							for (const FGraphOperatorData::FVertexDestination& Destination : *Destinations)
							{
								PropagateStack.Push({Destination.OperatorID, Destination.VertexName, OutputDataReference});
							}
						}
					}
				}
				else
				{
					UE_LOG(LogMetaSound, Error, TEXT("Failed to rebind graph operator state. Could not find operator info with ID %s"), *LexToString(Current.OperatorID));
				}
			}
		}

		FExecuteEntry::FExecuteEntry(DirectedGraphAlgo::FOperatorID InOperatorID, IOperator& InOperator, IOperator::FExecuteFunction InFunc)
		: OperatorID(InOperatorID)
		, Operator(&InOperator)
		, Function(InFunc)
		{
		}

		FPostExecuteEntry::FPostExecuteEntry(DirectedGraphAlgo::FOperatorID InOperatorID, IOperator& InOperator, IOperator::FPostExecuteFunction InFunc)
		: OperatorID(InOperatorID)
		, Operator(&InOperator)
		, Function(InFunc)
		{
		}

		FResetEntry::FResetEntry(DirectedGraphAlgo::FOperatorID InOperatorID, IOperator& InOperator, IOperator::FResetFunction InFunc)
		: OperatorID(InOperatorID)
		, Operator(&InOperator)
		, Function(InFunc)
		{
		}

		FDynamicGraphOperatorData::FDynamicGraphOperatorData(const FOperatorSettings& InSettings)
		: DirectedGraphAlgo::FGraphOperatorData(InSettings)
		{
		}

		FDynamicGraphOperatorData::FDynamicGraphOperatorData(DirectedGraphAlgo::FGraphOperatorData&& InGraphOperatorData)
		: DirectedGraphAlgo::FGraphOperatorData(MoveTemp(InGraphOperatorData))
		{
			InitTables();
		}

		FDynamicGraphOperatorData::FDynamicGraphOperatorData(DirectedGraphAlgo::FGraphOperatorData&& InGraphOperatorData, const FDynamicOperatorUpdateCallbacks& InCallbacks)
		: DirectedGraphAlgo::FGraphOperatorData(MoveTemp(InGraphOperatorData))
		, OperatorUpdateCallbacks(InCallbacks)
		{
			InitTables();
		}

		void FDynamicGraphOperatorData::InitTables()
		{
			// Populate execute/postexecute/reset stacks
			for (const FOperatorID& OperatorID : OperatorOrder)
			{
				if (FOperatorInfo* OperatorInfo = OperatorMap.Find(OperatorID))
				{
					check(OperatorInfo->Operator.IsValid());
					IOperator& Operator = *(OperatorInfo->Operator);

					if (IOperator::FExecuteFunction ExecuteFunc = Operator.GetExecuteFunction())
					{
						ExecuteTable.Emplace(FExecuteEntry{OperatorID, Operator, ExecuteFunc});
					}

					if (IOperator::FPostExecuteFunction PostExecuteFunc = Operator.GetPostExecuteFunction())
					{
						PostExecuteTable.Emplace(FPostExecuteEntry{OperatorID, Operator, PostExecuteFunc});
					}

					if (IOperator::FResetFunction ResetFunc = Operator.GetResetFunction())
					{
						ResetTable.Emplace(FResetEntry{OperatorID, Operator, ResetFunc});
					}
				}
			}
		}

		void UpdateOutputVertexData(FDynamicGraphOperatorData& InOutGraphOperatorData)
		{
			using namespace DirectedGraphAlgo;
			METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::DynamicGraphAlgo::UpdateOutputVertexData)

			// Iterate through the output operators and force their output data references
			// to be reflected in the graph's FOutputVertexInterfaceData
			for (const TPair<FVertexName, FOperatorID>& OutputVertexInfo : InOutGraphOperatorData.OutputVertexMap)
			{
				const FVertexName& VertexName = OutputVertexInfo.Get<0>();
				const FOperatorID& OperatorID = OutputVertexInfo.Get<1>();

				if (const FGraphOperatorData::FOperatorInfo* OperatorInfo = InOutGraphOperatorData.OperatorMap.Find(OperatorID))
				{
					if (const FAnyDataReference* Ref = OperatorInfo->VertexData.GetOutputs().FindDataReference(OutputVertexInfo.Get<0>()))
					{
						InOutGraphOperatorData.VertexData.GetOutputs().SetVertex(VertexName, *Ref);

						if (InOutGraphOperatorData.OperatorUpdateCallbacks.OnOutputUpdated)
						{
							InOutGraphOperatorData.OperatorUpdateCallbacks.OnOutputUpdated(VertexName, InOutGraphOperatorData.VertexData.GetOutputs());
						}
					}
					else if (InOutGraphOperatorData.VertexData.GetOutputs().IsVertexBound(VertexName))
					{
						UE_LOG(LogMetaSound, Error, TEXT("Output vertex (%s) lost data reference after rebinding graph"), *VertexName.ToString());
					}
				}
				else
				{
					UE_LOG(LogMetaSound, Error, TEXT("Failed to update graph operator outputs. Could not find output operator info with ID %s for vertex %s"), *LexToString(OperatorID), *VertexName.ToString());
				}
			}
		}

		void RebindWrappedOperator(const FOperatorID& InOperatorID, FOperatorInfo& InOperatorInfo, FDynamicGraphOperatorData& InOutGraphOperatorData)
		{
			using namespace DirectedGraphAlgo;
			using namespace DynamicGraphAlgoPrivate;

			check(InOperatorInfo.Operator);
			METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::DynamicGraphAlgo::RebindWrappedOperator)

			/* Bind and diff the graph's interface to determine if there is an update to any vertices */

			// Cache current state of operators output data. 
			FOutputVertexInterfaceData& OutputVertexData = InOperatorInfo.VertexData.GetOutputs();
			TArray<FVertexDataState> InitialVertexDataState;
			GetVertexInterfaceDataState(OutputVertexData, InitialVertexDataState);

			// Bind the operator to trigger any updates. 
			InOperatorInfo.Operator->BindInputs(InOperatorInfo.VertexData.GetInputs());
			InOperatorInfo.Operator->BindOutputs(InOperatorInfo.VertexData.GetOutputs());

			// Update any execution tables that need updating after wrapping
			UpdateGraphRuntimeTableEntries(InOperatorID, InOperatorInfo.Operator.Get(), InOutGraphOperatorData);

			// Determine if there have been changes to `OutputVertexData`. 
			TSortedVertexNameMap<FAnyDataReference> OutputsToUpdate;
			CompareVertexInterfaceDataToPriorState(OutputVertexData, InitialVertexDataState, OutputsToUpdate);

			// If there have been any changes to `OutputVertexData`, then these need to be propagated
			// through the graph to route them to operator inputs and to handle any knock-on updates
			// to other data references. 

			// Update the graph data by propagating the updates through the inputs nodes. 
			for (const TPair<FVertexName, FAnyDataReference>& OutputToUpdate : OutputsToUpdate)
			{
				const FVertexName& VertexName = OutputToUpdate.Get<0>();
				if (const TArray<FGraphOperatorData::FVertexDestination>* Destinations  = InOperatorInfo.OutputConnections.Find(VertexName))
				{
					for (const FGraphOperatorData::FVertexDestination& Destination : *Destinations)
					{
						PropagateBindUpdate(Destination.OperatorID, Destination.VertexName, OutputToUpdate.Get<1>(), InOutGraphOperatorData);
						
					}
				}
			}

			// Refresh output vertex interface data in case any graph output 
			// nodes were updated when bind updates were propagated through 
			// the graph.
			UpdateOutputVertexData(InOutGraphOperatorData);
		}
		
		void RebindGraphInputs(FInputVertexInterfaceData& InOutVertexData, FDynamicGraphOperatorData& InOutGraphOperatorData)
		{
			using namespace DirectedGraphAlgo;
			METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::DynamicGraphAlgo::RebindGraphInputs)

			// Bind and diff the graph's interface to determine if there is an update to any vertices
			FInputVertexInterfaceData& InputVertexData = InOutGraphOperatorData.VertexData.GetInputs();
			TArray<FVertexDataState> InitialVertexDataState;
			GetVertexInterfaceDataState(InputVertexData, InitialVertexDataState);

			// Binding an input vertex interface may update `InputVertexData`
			InOutVertexData.Bind(InputVertexData);

			// Determine if there have been changes to `InputVertexData`. 
			TSortedVertexNameMap<FAnyDataReference> GraphInputsToUpdate;
			CompareVertexInterfaceDataToPriorState(InputVertexData, InitialVertexDataState, GraphInputsToUpdate);

			// If there have been any changes to `InputVertexData`, then these need to be propagated
			// through the graph to route them to operator inputs and to handle any knock-on updates
			// to other data references. 
			if (GraphInputsToUpdate.Num() > 0)
			{
				// Update the graph data by propagating the updates through the inputs nodes. 
				for (const TPair<FVertexName, FAnyDataReference>& InputToUpdate : GraphInputsToUpdate)
				{
					const FVertexName& VertexName = InputToUpdate.Get<0>();
					if (const FOperatorID* OperatorID = InOutGraphOperatorData.InputVertexMap.Find(VertexName))
					{
						PropagateBindUpdate(*OperatorID, VertexName, InputToUpdate.Get<1>(), InOutGraphOperatorData);
					}
					else
					{
						UE_LOG(LogMetaSound, Error, TEXT("No input operator exists for input vertex %s"), *VertexName.ToString());
					}
				}

				// Refresh output vertex interface data in case any output nodes were updated
				// when bind updates were propagated through the graph.
				UpdateOutputVertexData(InOutGraphOperatorData);
			}
		}

		void RebindGraphOutputs(FOutputVertexInterfaceData& InOutVertexData, FDynamicGraphOperatorData& InOutGraphOperatorData)
		{
			METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::DynamicGraphAlgo::RebindGraphOutputs)
			// Output rebinding does not alter data references in an operator. Here we can get away with
			// simply reading the latest values.
			InOutVertexData.Bind(InOutGraphOperatorData.VertexData.GetOutputs());
		}
	}
}
