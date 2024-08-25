// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundDynamicOperator.h"

#include "Containers/Array.h"
#include "HAL/IConsoleManager.h"
#include "MetasoundDynamicOperatorAudioFade.h"
#include "MetasoundNodeInterface.h"
#include "MetasoundOperatorInterface.h"
#include "MetasoundVertexData.h"
#include "MetasoundTrace.h"
#include "Templates/SharedPointer.h"
#include "Templates/UniquePtr.h"

namespace Metasound
{
	namespace DynamicGraph
	{
		const TCHAR* LexToString(EDynamicOperatorTransformQueueAction InTransformResult)
		{
			switch (InTransformResult)
			{
				case EDynamicOperatorTransformQueueAction::Continue:
					return TEXT("Continue");

				case EDynamicOperatorTransformQueueAction::Fence:
					return TEXT("Fence");

				default:
					{
						checkNoEntry();
						return TEXT("Unhandled");
					}
			}
		}

		namespace DynamicOperatorPrivate
		{
			float MetaSoundExperimentalTransformTimeoutInSeconds = 0.010f;
			FAutoConsoleVariableRef CVarMetaSoundExperimentalTransformTimeoutInSeconds(
				TEXT("au.MetaSound.Experimental.DynamicOperatorTransformTimeoutInSeconds"),
				MetaSoundExperimentalTransformTimeoutInSeconds,
				TEXT("Sets the number of seconds allowed to process pending dynamic graph transformations for a single MetaSound render cycle .\n")
				TEXT("[Less than zero]: Disabled, [Greater than zero]: Enabled, 0.010s (default)"),
				ECVF_Default);
			
			// Table sorter helper so we don't rewrite this algorithm for each differe
			// stack type (Execute/PostExecute/Reset)
			struct FTableSorter
			{
				template<typename TableEntryType>
				static void SortTable(const TArray<FOperatorID>& OperatorOrder, TArray<TableEntryType>& InOutTable)
				{
					uint32 TargetIndex = 0;
					for (const FOperatorID& OperatorID : OperatorOrder)
					{
						uint32 CurrentIndex = InOutTable.IndexOfByPredicate([&](const TableEntryType& Entry) 
							{ 
								return Entry.OperatorID == OperatorID; 
							}
						);

						// The operator may not exist in the stack if it doesn't
						// have an applicable function.
						if (INDEX_NONE != CurrentIndex)
						{
							InOutTable.Swap(TargetIndex, CurrentIndex);
							TargetIndex++;
						}
					}
				}
			};

			template<typename VertexInterfaceDataType>
			class TScopeUnfreeze final
			{
			public:
				explicit TScopeUnfreeze(VertexInterfaceDataType& InVertexData)
				: bIsOriginallyFrozen(InVertexData.IsVertexInterfaceFrozen())
				, VertexData(InVertexData)
				{
					VertexData.SetIsVertexInterfaceFrozen(false);
				}

				~TScopeUnfreeze()
				{
					VertexData.SetIsVertexInterfaceFrozen(bIsOriginallyFrozen);
				}

			private:
				bool bIsOriginallyFrozen;
				VertexInterfaceDataType& VertexData;
			};

		} // namespace DynamicOperatorPrivate

		FDynamicOperator::FDynamicOperator(const FOperatorSettings& InSettings)
		: DynamicOperatorData(InSettings)
		{
			// Ensure that a transform queue exists. 
			if (!TransformQueue.IsValid())
			{
				TransformQueue = MakeShared<TSpscQueue<TUniquePtr<IDynamicOperatorTransform>>>();
			}
		}

		FDynamicOperator::FDynamicOperator(DirectedGraphAlgo::FGraphOperatorData&& InGraphOperatorData, TSharedPtr<TSpscQueue<TUniquePtr<IDynamicOperatorTransform>>> InTransformQueue, const FDynamicOperatorUpdateCallbacks& InOperatorUpdateCallbacks)
		: DynamicOperatorData(MoveTemp(InGraphOperatorData), InOperatorUpdateCallbacks)
		, TransformQueue(InTransformQueue)
		{
			// Ensure that a transform queue exists. 
			if (!TransformQueue.IsValid())
			{
				TransformQueue = MakeShared<TSpscQueue<TUniquePtr<IDynamicOperatorTransform>>>();
			}
		}

		void FDynamicOperator::BindInputs(FInputVertexInterfaceData& InOutVertexData)
		{
			RebindGraphInputs(InOutVertexData, DynamicOperatorData);
		}

		void FDynamicOperator::BindOutputs(FOutputVertexInterfaceData& InOutVertexData)
		{
			RebindGraphOutputs(InOutVertexData, DynamicOperatorData);
		}

		IOperator::FResetFunction FDynamicOperator::GetResetFunction()
		{
			return &StaticReset;
		}

		IOperator::FExecuteFunction FDynamicOperator::GetExecuteFunction()
		{
			return &StaticExecute;
		}

		IOperator::FPostExecuteFunction FDynamicOperator::GetPostExecuteFunction()
		{
			return &StaticPostExecute;
		}

		void FDynamicOperator::FlushEnqueuedTransforms()
		{
			METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::FDynamicOperator::FlushEnqueuedTransforms)

			while (TOptional<TUniquePtr<IDynamicOperatorTransform>> Transform = TransformQueue->Dequeue())
			{
				if (Transform.IsSet())
				{
					if (Transform->IsValid())
					{
						(*Transform)->Transform(DynamicOperatorData);
					}
				}
			}
		}

		void FDynamicOperator::ApplyTransformsUntilFence()
		{
			METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::FDynamicOperator::ApplyTransformsUntilFence)

			if (!bExecuteFenceIsSet)
			{
				while (TOptional<TUniquePtr<IDynamicOperatorTransform>> Transform = TransformQueue->Dequeue())
				{
					if (Transform.IsSet())
					{
						if (Transform->IsValid())
						{
							EDynamicOperatorTransformQueueAction Result = (*Transform)->Transform(DynamicOperatorData);
							if (EDynamicOperatorTransformQueueAction::Fence == Result)
							{
								bExecuteFenceIsSet = true;
								break;
							}
						}
					}
				}
			}
		}

		void FDynamicOperator::ApplyTransformsUntilFenceOrTimeout(double InTimeoutInSeconds)
		{
			METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::FDynamicOperator::ApplyTransformsUntilFenceOrTimeout)

			if (bExecuteFenceIsSet)
			{
				// Execute fence needs to be cleared before applying any transforms.
				return;
			}

			TOptional<TUniquePtr<IDynamicOperatorTransform>> Transform = TransformQueue->Dequeue();

			if (Transform)
			{
				check(FPlatformTime::GetSecondsPerCycle() > 0);
				const uint64 BreakTimeInCycles = FPlatformTime::Cycles64() + static_cast<uint64>(InTimeoutInSeconds / FPlatformTime::GetSecondsPerCycle());
				do
				{
					if (Transform.IsSet() && Transform->IsValid())
					{
						EDynamicOperatorTransformQueueAction Result = (*Transform)->Transform(DynamicOperatorData);
						if (EDynamicOperatorTransformQueueAction::Fence == Result)
						{
							bExecuteFenceIsSet = true;
							break;
						}
					}

					if (FPlatformTime::Cycles64() >= BreakTimeInCycles)
					{
						UE_LOG(LogMetaSound, Verbose, TEXT("Transforms exceeded duration."));
						break;
					}
				}
				while((Transform = TransformQueue->Dequeue()));
			}
		}

		void FDynamicOperator::Execute()
		{
			using namespace DynamicOperatorPrivate;

			if (MetaSoundExperimentalTransformTimeoutInSeconds > 0)
			{
				ApplyTransformsUntilFenceOrTimeout(MetaSoundExperimentalTransformTimeoutInSeconds);
			}
			else
			{
				ApplyTransformsUntilFence();
			}

			for (FExecuteEntry& Entry : DynamicOperatorData.ExecuteTable)
			{
				Entry.Execute();
			}
		}

		void FDynamicOperator::PostExecute()
		{
			for (FPostExecuteEntry& Entry : DynamicOperatorData.PostExecuteTable)
			{
				Entry.PostExecute();
			}

			bExecuteFenceIsSet = false;
		}

		void FDynamicOperator::Reset(const IOperator::FResetParams& InParams)
		{
			FlushEnqueuedTransforms();
			for (FResetEntry& Entry : DynamicOperatorData.ResetTable)
			{
				Entry.Reset(InParams);
			}
		}

		void FDynamicOperator::StaticReset(IOperator* InOperator, const IOperator::FResetParams& InParams)
		{
			static_cast<FDynamicOperator*>(InOperator)->Reset(InParams);
		}

		void FDynamicOperator::StaticExecute(IOperator* InOperator)
		{
			static_cast<FDynamicOperator*>(InOperator)->Execute();
		}

		void FDynamicOperator::StaticPostExecute(IOperator* InOperator)
		{
			static_cast<FDynamicOperator*>(InOperator)->PostExecute();
		}

		EDynamicOperatorTransformQueueAction FNullTransform::Transform(FDynamicGraphOperatorData& InGraphOperatorData)
		{
			return EDynamicOperatorTransformQueueAction::Continue;
		}

		// Set the order of operators in the graph
		FSetOperatorOrder::FSetOperatorOrder(TArray<FOperatorID> InOrder)
		: Order(MoveTemp(InOrder))
		{
		}

		EDynamicOperatorTransformQueueAction FSetOperatorOrder::Transform(FDynamicGraphOperatorData& InOutGraphOperatorData)
		{
			METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::DynamicOperator::SetOperatorOrder)

			using namespace DynamicOperatorPrivate;

			InOutGraphOperatorData.OperatorOrder = Order;

			// Sort operator tables to be in the correct order.
			FTableSorter::SortTable(Order, InOutGraphOperatorData.ExecuteTable);
			FTableSorter::SortTable(Order, InOutGraphOperatorData.PostExecuteTable);
			FTableSorter::SortTable(Order, InOutGraphOperatorData.ResetTable);

			return EDynamicOperatorTransformQueueAction::Continue;
		}

		// Add an operator to the grpah
		FAddOperator::FAddOperator(FOperatorID InOperatorID, EExecutionOrderInsertLocation InLocation, FOperatorInfo&& InInfo)
		: OperatorID(InOperatorID)
		, Location(InLocation)
		, OperatorInfo(MoveTemp(InInfo))
		{
		}

		EDynamicOperatorTransformQueueAction FAddOperator::Transform(FDynamicGraphOperatorData& InGraphOperatorData)
		{
			METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::DynamicOperator::AddOperator)

			if (OperatorInfo.Operator.IsValid())
			{
				IOperator* Operator = OperatorInfo.Operator.Get();

				if (FOperatorInfo* ExistingInfo = InGraphOperatorData.OperatorMap.Find(OperatorID))
				{
					// The options here are not good. The prior operator will be 
					// removed and replaced with this new operator.
					// Another option would be to leave the existing operator unchanged. 
					// Neither option is satisfactory.
					UE_LOG(LogMetaSound, Warning, TEXT("Overriding existing operator with the same operator ID %d. Duplicate operator IDs will lead to undefined behavior. Remove existing operators before adding a new one with the same ID"), OperatorID);

					FRemoveOperator(OperatorID).Transform(InGraphOperatorData);
				}

				InGraphOperatorData.OperatorMap.Add(OperatorID, MoveTemp(OperatorInfo));
				switch (Location)
				{
					case EExecutionOrderInsertLocation::First:
						{
							InGraphOperatorData.OperatorOrder.Insert(OperatorID, 0);
							// Update execution tables
							if (IOperator::FExecuteFunction ExecuteFunc = Operator->GetExecuteFunction())
							{
								InGraphOperatorData.ExecuteTable.Insert(FExecuteEntry(OperatorID, *Operator, ExecuteFunc), 0);
							}

							if (IOperator::FPostExecuteFunction PostExecuteFunc = Operator->GetPostExecuteFunction())
							{
								InGraphOperatorData.PostExecuteTable.Insert(FPostExecuteEntry(OperatorID, *Operator, PostExecuteFunc), 0);
							}

							if (IOperator::FResetFunction ResetFunc = Operator->GetResetFunction())
							{
								InGraphOperatorData.ResetTable.Insert(FResetEntry(OperatorID, *Operator, ResetFunc), 0);
							}
						}
						break;

					case EExecutionOrderInsertLocation::Last:
						{
							InGraphOperatorData.OperatorOrder.Add(OperatorID);
							// Update execution tables
							if (IOperator::FExecuteFunction ExecuteFunc = Operator->GetExecuteFunction())
							{
								InGraphOperatorData.ExecuteTable.Add(FExecuteEntry(OperatorID, *Operator, ExecuteFunc));
							}

							if (IOperator::FPostExecuteFunction PostExecuteFunc = Operator->GetPostExecuteFunction())
							{
								InGraphOperatorData.PostExecuteTable.Add(FPostExecuteEntry(OperatorID, *Operator, PostExecuteFunc));
							}

							if (IOperator::FResetFunction ResetFunc = Operator->GetResetFunction())
							{
								InGraphOperatorData.ResetTable.Add(FResetEntry(OperatorID, *Operator, ResetFunc));
							}
						}
						break;
				}
			}

			return EDynamicOperatorTransformQueueAction::Continue;
		}

		// Remove an operator from the graph
		FRemoveOperator::FRemoveOperator(FOperatorID InOperatorID)
		: OperatorID(InOperatorID)
		{
		}

		EDynamicOperatorTransformQueueAction FRemoveOperator::Transform(FDynamicGraphOperatorData& InGraphOperatorData) 
		{
			METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::DynamicOperator::RemoveOperator)

			InGraphOperatorData.OperatorOrder.Remove(OperatorID);
			InGraphOperatorData.OperatorMap.Remove(OperatorID);

			// Update execution tables
			InGraphOperatorData.ExecuteTable.RemoveAll([&](const FExecuteEntry& InEntry) { return InEntry.OperatorID == OperatorID; });
			InGraphOperatorData.PostExecuteTable.RemoveAll([&](const FPostExecuteEntry& InEntry) { return InEntry.OperatorID == OperatorID; });
			InGraphOperatorData.ResetTable.RemoveAll([&](const FResetEntry& InEntry) { return InEntry.OperatorID == OperatorID; });

			return EDynamicOperatorTransformQueueAction::Continue;
		}

		// Add an input to the graph
		FAddInput::FAddInput(FOperatorID InOperatorID, const FVertexName& InVertexName, FAnyDataReference InDataReference)
		: OperatorID(InOperatorID)
		, VertexName(InVertexName)
		, DataReference(InDataReference)
		{
		}

		EDynamicOperatorTransformQueueAction FAddInput::Transform(FDynamicGraphOperatorData& InGraphOperatorData)
		{
			METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::DynamicOperator::AddInput)

			FOperatorInfo* OpInfo = InGraphOperatorData.OperatorMap.Find(OperatorID);
			if (nullptr == OpInfo)
			{
				UE_LOG(LogMetaSound, Error, TEXT("Could not find operator with ID %d when adding input %s."), OperatorID, *VertexName.ToString());
				return EDynamicOperatorTransformQueueAction::Continue;
			}

			const FInputDataVertex& OperatorInputVertex = OpInfo->VertexData.GetInputs().GetVertex(VertexName);

			FInputVertexInterfaceData& GraphInputData = InGraphOperatorData.VertexData.GetInputs();
			{
				// Unfreeze interface so a new vertex can be added. 
				DynamicOperatorPrivate::TScopeUnfreeze Unfreeze(GraphInputData);

				// Update unfrozen vertex interface data.
				GraphInputData.AddVertex(OperatorInputVertex);
			}
			GraphInputData.SetVertex(VertexName, DataReference);

			InGraphOperatorData.InputVertexMap.Add(VertexName, OperatorID);

			// Update listeners that an input has been added.
			if (InGraphOperatorData.OperatorUpdateCallbacks.OnInputAdded)
			{
				InGraphOperatorData.OperatorUpdateCallbacks.OnInputAdded(VertexName, InGraphOperatorData.VertexData.GetInputs());
			}

			// Propagate the data reference update through the graph
			PropagateBindUpdate(OperatorID, VertexName, DataReference, InGraphOperatorData);

			// Refresh output vertex interface data in case any output nodes were updated
			// when bind updates were propagated through the graph.
			UpdateOutputVertexData(InGraphOperatorData);

			return EDynamicOperatorTransformQueueAction::Continue;
		}

		// Remove an input from the graph
		FRemoveInput::FRemoveInput(const FVertexName& InVertexName)
		: VertexName(InVertexName)
		{
		}

		EDynamicOperatorTransformQueueAction FRemoveInput::Transform(FDynamicGraphOperatorData& InGraphOperatorData)
		{
			METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::DynamicOperator::RemoveInput)

			InGraphOperatorData.InputVertexMap.Remove(VertexName);
			{
				// Unfreeze vertex data so vertex can be removed
				DynamicOperatorPrivate::TScopeUnfreeze Unfreeze(InGraphOperatorData.VertexData.GetInputs());
				InGraphOperatorData.VertexData.GetInputs().RemoveVertex(VertexName);
			}

			// Update listeners that an input has been removed.
			if (InGraphOperatorData.OperatorUpdateCallbacks.OnInputRemoved)
			{
				InGraphOperatorData.OperatorUpdateCallbacks.OnInputRemoved(VertexName, InGraphOperatorData.VertexData.GetInputs());
			}

			return EDynamicOperatorTransformQueueAction::Continue;
		}

		// Add an output to the graph
		FAddOutput::FAddOutput(FOperatorID InOperatorID, const FVertexName& InVertexName)
		: OperatorID(InOperatorID)
		, VertexName(InVertexName)
		{
		}

		EDynamicOperatorTransformQueueAction FAddOutput::Transform(FDynamicGraphOperatorData& InGraphOperatorData)
		{
			METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::DynamicOperator::AddOutput)

			FOperatorInfo* OpInfo = InGraphOperatorData.OperatorMap.Find(OperatorID);
			if (nullptr == OpInfo)
			{
				UE_LOG(LogMetaSound, Error, TEXT("Could not find operator with ID %d when adding output %s."), OperatorID, *VertexName.ToString());
				return EDynamicOperatorTransformQueueAction::Continue;
			}

			const FOutputDataVertex& OperatorOutputVertex = OpInfo->VertexData.GetOutputs().GetVertex(VertexName);
			const FAnyDataReference* Ref = OpInfo->VertexData.GetOutputs().FindDataReference(VertexName);

			if (nullptr == Ref)
			{
				UE_LOG(LogMetaSound, Error, TEXT("Could not find data reference when creating output %s"), *VertexName.ToString());
				return EDynamicOperatorTransformQueueAction::Continue;
			}

			FOutputVertexInterfaceData& GraphOutputData = InGraphOperatorData.VertexData.GetOutputs();
			{
				// Unfreeze interface so a new vertex can be added. 
				DynamicOperatorPrivate::TScopeUnfreeze Unfreeze(GraphOutputData);

				// Update unfrozen vertex interface data.
				GraphOutputData.AddVertex(OperatorOutputVertex);
			}
			GraphOutputData.SetVertex(VertexName, *Ref);

			InGraphOperatorData.OutputVertexMap.Add(VertexName, OperatorID);

			// Update listeners that an input has been added.
			if (InGraphOperatorData.OperatorUpdateCallbacks.OnOutputAdded)
			{
				InGraphOperatorData.OperatorUpdateCallbacks.OnOutputAdded(VertexName, InGraphOperatorData.VertexData.GetOutputs());
			}

			return EDynamicOperatorTransformQueueAction::Continue;
		}

		// Remove an output from the graph
		FRemoveOutput::FRemoveOutput(const FVertexName& InVertexName)
		: VertexName(InVertexName)
		{
		}

		EDynamicOperatorTransformQueueAction FRemoveOutput::Transform(FDynamicGraphOperatorData& InGraphOperatorData)
		{
			METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::DynamicOperator::RemoveOutput)

			InGraphOperatorData.OutputVertexMap.Remove(VertexName);
			{
				// Unfreeze vertex data so vertex can be removed
				DynamicOperatorPrivate::TScopeUnfreeze Unfreeze(InGraphOperatorData.VertexData.GetOutputs());
				InGraphOperatorData.VertexData.GetOutputs().RemoveVertex(VertexName);
			}

			// Update listeners that an output has been removed.
			if (InGraphOperatorData.OperatorUpdateCallbacks.OnOutputRemoved)
			{
				InGraphOperatorData.OperatorUpdateCallbacks.OnOutputRemoved(VertexName, InGraphOperatorData.VertexData.GetOutputs());
			}

			return EDynamicOperatorTransformQueueAction::Continue;
		}

		// Trigger a execution fence.
		EDynamicOperatorTransformQueueAction FExecuteFence::Transform(FDynamicGraphOperatorData& InGraphOperatorData)
		{
			return EDynamicOperatorTransformQueueAction::Fence;
		}

		// Connect two operators in the graph
		FConnectOperators::FConnectOperators(FOperatorID InFromOpID, const FName& InFromVert, FOperatorID InToOpID, const FName& InToVert)
		: FromOpID(InFromOpID)
		, ToOpID(InToOpID)
		, FromVert(InFromVert)
		, ToVert(InToVert)
		{
		}

		EDynamicOperatorTransformQueueAction FConnectOperators::Transform(FDynamicGraphOperatorData& InGraphOperatorData)
		{
			using namespace DirectedGraphAlgo;

			METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::DynamicOperator::ConnectOperators)

			FOperatorInfo* FromOpInfo = InGraphOperatorData.OperatorMap.Find(FromOpID);
			FOperatorInfo* ToOpInfo = InGraphOperatorData.OperatorMap.Find(ToOpID);

			if (nullptr == FromOpInfo)
			{
				UE_LOG(LogMetaSound, Error, TEXT("Could not find operator with ID %d when connecting from %d:%s to %d:%s"), FromOpID, FromOpID, *FromVert.ToString(), ToOpID, *ToVert.ToString());
				return EDynamicOperatorTransformQueueAction::Continue;
			}

			if (nullptr == ToOpInfo)
			{
				UE_LOG(LogMetaSound, Error, TEXT("Could not find operator with ID %d when connecting from %d:%s to %d:%s"), ToOpID, FromOpID, *FromVert.ToString(), ToOpID, *ToVert.ToString());
				return EDynamicOperatorTransformQueueAction::Continue;
			}

			const FAnyDataReference* FromRef = FromOpInfo->VertexData.GetOutputs().FindDataReference(FromVert);
			if (nullptr == FromRef)
			{
				UE_LOG(LogMetaSound, Error, TEXT("Could not find output data reference with vertex name %s when connecting from %d:%s to %d:%s"), *FromVert.ToString(), FromOpID, *FromVert.ToString(), ToOpID, *ToVert.ToString());
				return EDynamicOperatorTransformQueueAction::Continue;
			}

			// Propagate the data reference update through the graph
			PropagateBindUpdate(ToOpID, ToVert, *FromRef, InGraphOperatorData);

			// Refresh output vertex interface data in case any output nodes were updated
			// when bind updates were propagated through the graph.
			UpdateOutputVertexData(InGraphOperatorData);

			FromOpInfo->OutputConnections.FindOrAdd(FromVert).Add(FGraphOperatorData::FVertexDestination{ToOpID, ToVert});

			return EDynamicOperatorTransformQueueAction::Continue;
		}

		// Disconnect a specific edge
		FSwapOperatorConnection::FSwapOperatorConnection(FOperatorID InOriginalFromOpID, const FName& InOriginalFromVert, FOperatorID InNewFromOpID, const FName& InNewFromVert, FOperatorID InToOpID, const FName& InToVert)
		: ConnectTransform(InNewFromOpID, InNewFromVert, InToOpID, InToVert)
		, OriginalFromOpID(InOriginalFromOpID)
		, ToOpID(InToOpID)
		, OriginalFromVert(InOriginalFromVert)
		, ToVert(InToVert)
		{
		}

		EDynamicOperatorTransformQueueAction FSwapOperatorConnection::Transform(FDynamicGraphOperatorData& InGraphOperatorData)
		{
			using namespace DirectedGraphAlgo;

			METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::DynamicOperator::DisconnectOperators)

			// Make new connection. We can skip propagating updates and updating callbacks
			// here because those are handled in the FConnectOperators transform.
			EDynamicOperatorTransformQueueAction NextAction = ConnectTransform.Transform(InGraphOperatorData);
			check(NextAction == EDynamicOperatorTransformQueueAction::Continue); // this should always be continue since next step must be performed. 

			// Clean up the old connection.
			FOperatorInfo* OriginalFromOpInfo = InGraphOperatorData.OperatorMap.Find(OriginalFromOpID);

			if (nullptr == OriginalFromOpInfo)
			{
				UE_LOG(LogMetaSound, Error, TEXT("Could not find operator with ID %d when disconnecting from %d:%s to %d:%s"), OriginalFromOpID, OriginalFromOpID, *OriginalFromVert.ToString(), ToOpID, *ToVert.ToString());
				return EDynamicOperatorTransformQueueAction::Continue;
			}

			// Remove destinations from operator map.
			auto IsDestinationToRemove = [&](const FGraphOperatorData::FVertexDestination& Dst) 
			{ 
				return (Dst.OperatorID == ToOpID) && (Dst.VertexName == ToVert);
			};
			OriginalFromOpInfo->OutputConnections.FindOrAdd(OriginalFromVert).RemoveAll(IsDestinationToRemove);

			return EDynamicOperatorTransformQueueAction::Continue;
		}

		// Set the value on an unconnected operator input
		FSetOperatorInput::FSetOperatorInput(FOperatorID InToOpID, const FName& InToVert, const FLiteral& InLiteral, FLiteralAssignmentFunction InAssignFunc)
		: ToOpID(InToOpID)
		, ToVert(InToVert)
		, Literal(InLiteral)
		, AssignFunc(InAssignFunc)
		{
		}

		EDynamicOperatorTransformQueueAction FSetOperatorInput::Transform(FDynamicGraphOperatorData& InGraphOperatorData)
		{
			using namespace DirectedGraphAlgo;

			METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::DynamicOperator::SetOperatorInput)

			FOperatorInfo* ToOpInfo = InGraphOperatorData.OperatorMap.Find(ToOpID);

			if (nullptr == ToOpInfo)
			{
				UE_LOG(LogMetaSound, Error, TEXT("Could not find operator with ID %d when setting value for %d:%s"), ToOpID, ToOpID, *ToVert.ToString());
				return EDynamicOperatorTransformQueueAction::Continue;
			}

			const FAnyDataReference* Ref = ToOpInfo->VertexData.GetInputs().FindDataReference(ToVert);
			if (nullptr == Ref)
			{
				UE_LOG(LogMetaSound, Error, TEXT("Could not find input data reference with vertex name %s when setting %d:%s"), *ToVert.ToString(), ToOpID, *ToVert.ToString());
				return EDynamicOperatorTransformQueueAction::Continue;
			}

			AssignFunc(InGraphOperatorData.OperatorSettings, Literal, *Ref);

			return EDynamicOperatorTransformQueueAction::Continue;
		}

		// Perform several transformations at once without executing the graph.
		FAtomicTransform::FAtomicTransform(TArray<TUniquePtr<IDynamicOperatorTransform>> InTransforms)
		: Transforms(MoveTemp(InTransforms))
		{
		}

		EDynamicOperatorTransformQueueAction FAtomicTransform::Transform(FDynamicGraphOperatorData& InGraphOperatorData)
		{
			METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::DynamicOperator::AtomicTransform)

			EDynamicOperatorTransformQueueAction Result = EDynamicOperatorTransformQueueAction::Continue;

			for (TUniquePtr<IDynamicOperatorTransform>& TransformPtr : Transforms)
			{
				if (TransformPtr.IsValid())
				{
					if (EDynamicOperatorTransformQueueAction::Continue != Result)
					{
						UE_LOG(LogMetaSound, Error, TEXT("Encountered unsupported dynamic operator transform result (%s) during atomic operator trasnform."), LexToString(Result));
					}

					Result = TransformPtr->Transform(InGraphOperatorData);
				}
			}

			return Result;
		}

		FBeginAudioFadeTransform::FBeginAudioFadeTransform(FOperatorID InOperatorIDToFade, EAudioFadeType InFadeType, TArrayView<const FVertexName> InInputVerticesToFade, TArrayView<const FVertexName> InOutputVerticesToFade)
		: OperatorIDToFade(InOperatorIDToFade)
		, InitFadeState(InFadeType == EAudioFadeType::FadeIn ? FAudioFadeOperatorWrapper::EFadeState::FadingIn : FAudioFadeOperatorWrapper::EFadeState::FadingOut)
		, InputVerticesToFade(InInputVerticesToFade)
		, OutputVerticesToFade(InOutputVerticesToFade)
		{
		}

		EDynamicOperatorTransformQueueAction FBeginAudioFadeTransform::FBeginAudioFadeTransform::Transform(FDynamicGraphOperatorData& InGraphOperatorData)
		{
			METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::DynamicOperator::BeginAudioFadeTransform)

		  	if (FOperatorInfo* OperatorInfo = InGraphOperatorData.OperatorMap.Find(OperatorIDToFade))
			{
				// Make wrapped operator
				OperatorInfo->Operator = MakeUnique<FAudioFadeOperatorWrapper>(InitFadeState, InGraphOperatorData.OperatorSettings, OperatorInfo->VertexData.GetInputs(), MoveTemp(OperatorInfo->Operator), InputVerticesToFade, OutputVerticesToFade);

				// Update data references in graph
				RebindWrappedOperator(OperatorIDToFade, *OperatorInfo, InGraphOperatorData);
			}
			else
			{
				UE_LOG(LogMetaSound, Error, TEXT("Could not find operator with ID %d when applying audio fade out."), OperatorIDToFade);
			}
			
			return EDynamicOperatorTransformQueueAction::Continue;
		}

		FEndAudioFadeTransform::FEndAudioFadeTransform(FOperatorID InOperatorIDToFade)
		: OperatorIDToFade(InOperatorIDToFade)
		{
		}

		EDynamicOperatorTransformQueueAction FEndAudioFadeTransform::Transform(FDynamicGraphOperatorData& InGraphOperatorData)
		{
			METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::DynamicOperator::EndAudioFadeTransform)

		  	if (FOperatorInfo* OperatorInfo = InGraphOperatorData.OperatorMap.Find(OperatorIDToFade))
			{
				FAudioFadeOperatorWrapper* Wrapper = static_cast<FAudioFadeOperatorWrapper*>(OperatorInfo->Operator.Get());
				check(Wrapper);

				// Unwrap operator
				OperatorInfo->Operator = Wrapper->ReleaseOperator();

				// Update data references in graph
				RebindWrappedOperator(OperatorIDToFade, *OperatorInfo, InGraphOperatorData);
			}
			else
			{
				UE_LOG(LogMetaSound, Error, TEXT("Could not find operator with ID %d when applying audio fade out."), OperatorIDToFade);
			}
			
			return EDynamicOperatorTransformQueueAction::Continue;
		}
	} // namespace DynamicGraph
}

