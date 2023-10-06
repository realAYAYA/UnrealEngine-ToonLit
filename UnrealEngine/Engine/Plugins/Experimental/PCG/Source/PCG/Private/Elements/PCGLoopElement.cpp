// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGLoopElement.h"

#include "PCGComponent.h"
#include "PCGModule.h"
#include "PCGPin.h"
#include "PCGSubsystem.h"
#include "Graph/PCGStackContext.h"
#include "Helpers/PCGHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGLoopElement)

#define LOCTEXT_NAMESPACE "PCGLoopElement"

#if WITH_EDITOR
FText UPCGLoopSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Loop");
}

FText UPCGLoopSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip", "Executes the specified Subgraph for each data on the loop pins (or on the first pin if no specific loop pins are provided), keeping the rest constant.");
}
#endif

FPCGElementPtr UPCGLoopSettings::CreateElement() const
{
	return MakeShared<FPCGLoopElement>();
}

FName UPCGLoopSettings::AdditionalTaskName() const
{
	if (UPCGGraph* TargetSubgraph = GetSubgraph())
	{
		return FName(FText::Format(LOCTEXT("NodeTitleExtended", "Loop Subgraph - {0}"), FText::FromName(TargetSubgraph->GetFName())).ToString());
	}
	else
	{
		return FName(LOCTEXT("NodeTitleExtendedInvalidSubgraph", "Loop Subgraph - Invalid Subgraph").ToString());
	}
}

bool FPCGLoopElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGLoopElement::Execute);

	FPCGSubgraphContext* Context = static_cast<FPCGSubgraphContext*>(InContext);

	const UPCGLoopSettings* Settings = Context->GetInputSettings<UPCGLoopSettings>();
	check(Settings);

	// If we haven't scheduled the subgraph yet (1st call) - this is part of the FPCGSubgraphContext, see FPCGSubgraphElement for more info
	if (!Context->bScheduledSubgraph)
	{
		if (Settings->SubgraphInstance && Settings->SubgraphOverride)
		{
			// If OriginalSettings is null, then we ARE the original settings, and writing over the existing graph is incorrect (and potentially a race condition)
			check(Settings->OriginalSettings);
			Settings->SubgraphInstance->SetGraph(Settings->SubgraphOverride);
		}

		UPCGGraph* Subgraph = Settings->GetSubgraph();
		UPCGSubsystem* Subsystem = Context->SourceComponent.IsValid() ? Context->SourceComponent->GetSubsystem() : nullptr;

		if (!Subsystem || !Subgraph)
		{
			// Job cannot run; cancel
			if (!Subgraph)
			{
				PCGE_LOG(Error, GraphAndLog, LOCTEXT("InvalidSubgraph", "Cannot loop on an invalid subgraph"));
			}
			
			Context->OutputData.bCancelExecution = true;
			return true;
		}

		TArray<FPCGDataCollection> LoopDataCollection;
		FPCGDataCollection FixedInputDataCollection;
		PrepareLoopDataCollections(Context, Settings, LoopDataCollection, FixedInputDataCollection);

		// Early out if there are no data on the loop pin
		if (LoopDataCollection.IsEmpty() || LoopDataCollection[0].TaggedData.IsEmpty())
		{
			PCGE_LOG(Verbose, LogOnly, LOCTEXT("EmptyLoopCollection", "Loop data is empty - will not do anything"));
			return true;
		}

		// Early out if the loop collection do not contain the same amount of entries
		for (int LoopDataIndex = 1; LoopDataIndex < LoopDataCollection.Num(); ++LoopDataIndex)
		{
			if (LoopDataCollection[LoopDataIndex].TaggedData.Num() != LoopDataCollection[0].TaggedData.Num())
			{
				PCGE_LOG(Error, GraphAndLog, LOCTEXT("MismatchLoopCollections", "Data to loop on does not have the same number of entries for all pins!"));
				return true;
			}
		}

		FPCGDataCollection PreSubgraphDataCollection;
		PrepareSubgraphUserParameters(Settings, Context, PreSubgraphDataCollection);

		FPCGElementPtr PreGraphElement = MakeShared<FPCGInputForwardingElement>(PreSubgraphDataCollection);

		FPCGDataCollection FixedDataCollection;
		PrepareSubgraphData(Settings, Context, FixedInputDataCollection, FixedDataCollection);

		// Dispatch the subgraph for each loop entries we have.
		// Implementation note: even if the execution gets cancelled, these tasks would get cancelled because they are associated to the current source component
		for (int EntryIndex = 0; EntryIndex < LoopDataCollection[0].TaggedData.Num(); ++EntryIndex)
		{
			// The final input for a given loop iteration is going to be the fixed data + the entry at the current loop index in all the gathered loop collections
			FPCGDataCollection InputDataCollection = FixedDataCollection;
			for (int LoopCollectionIndex = 0; LoopCollectionIndex < LoopDataCollection.Num(); ++LoopCollectionIndex)
			{
				InputDataCollection.TaggedData.Insert(LoopDataCollection[LoopCollectionIndex].TaggedData[EntryIndex], LoopCollectionIndex);
			}

			// Prepare the invocation stack - which is the stack up to this node, and then this node, then a loop index
			FPCGStack InvocationStack = ensure(Context->Stack) ? *Context->Stack : FPCGStack();

			TArray<FPCGStackFrame>& StackFrames = InvocationStack.GetStackFramesMutable();
			StackFrames.Reserve(StackFrames.Num() + 2);
			StackFrames.Emplace(Context->Node);
			StackFrames.Emplace(EntryIndex);

#if WITH_EDITOR
			Subgraph->OnGraphDynamicallyExecutedDelegate.Broadcast(Subgraph, Context->SourceComponent, InvocationStack);
#endif

			FPCGTaskId SubgraphTaskId = Subsystem->ScheduleGraph(Subgraph, Context->SourceComponent.Get(), PreGraphElement, MakeShared<FPCGInputForwardingElement>(InputDataCollection), {}, &InvocationStack);

			Context->SubgraphTaskIds.Add(SubgraphTaskId);
		}

		Context->bScheduledSubgraph = true;
		Context->bIsPaused = true;

		// add a trivial task after the output tasks that wakes up this task
		Subsystem->ScheduleGeneric([Context]()
		{
			// Wake up the current task
			Context->bIsPaused = false;
			return true;
		}, Context->SourceComponent.Get(), Context->SubgraphTaskIds);

		return false;
	}
	else if (Context->bIsPaused)
	{
		// Should not happen once we skip it in the graph executor
		return false;
	}
	else
	{
		// when woken up, get the output data from the subgraph
		// and copy it to the current context output data, and finally return true
		UPCGSubsystem* Subsystem = Context->SourceComponent->GetSubsystem();
		if (Subsystem)
		{
			for (FPCGTaskId SubgraphTaskId : Context->SubgraphTaskIds)
			{
				FPCGDataCollection SubgraphOutput;
				ensure(Subsystem->GetOutputData(SubgraphTaskId, SubgraphOutput));

				Context->OutputData.TaggedData.Append(SubgraphOutput.TaggedData);
			}
		}
		else
		{
			// Job cannot run, cancel
			Context->OutputData.bCancelExecution = true;
		}

		return true;
	}
}

void FPCGLoopElement::PrepareLoopDataCollections(FPCGContext* Context, const UPCGLoopSettings* Settings, TArray<FPCGDataCollection>& LoopDataCollection, FPCGDataCollection& FixedInputDataCollection) const
{
	TArray<FName> LoopPinNames;
	TArray<FString> PinsFromSettings = PCGHelpers::GetStringArrayFromCommaSeparatedString(Settings->LoopPins);

	for (const FString& PinLabel : PinsFromSettings)
	{
		LoopPinNames.Emplace(PinLabel);
	}
	
	// If no named pins have been provided, default to the first pin
	if (LoopPinNames.IsEmpty())
	{
		const UPCGSubgraphNode* LoopNode = Cast<const UPCGSubgraphNode>(Context->Node);
		check(LoopNode);

		if (const UPCGPin* LoopPin = LoopNode->GetFirstConnectedInputPin())
		{
			LoopPinNames.Add(LoopPin->Properties.Label);
		}
	}

	LoopDataCollection.Reset();
	LoopDataCollection.SetNum(LoopPinNames.Num());

	for (const FPCGTaggedData& TaggedData : Context->InputData.TaggedData)
	{
		int LoopPinIndex = LoopPinNames.IndexOfByKey(TaggedData.Pin);
		if (LoopPinIndex != INDEX_NONE)
		{
			LoopDataCollection[LoopPinIndex].TaggedData.Add(TaggedData);
		}
		else
		{
			FixedInputDataCollection.TaggedData.Add(TaggedData);
		}
	}
}

#undef LOCTEXT_NAMESPACE