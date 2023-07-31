// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGElement.h"
#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGModule.h"
#include "PCGSettings.h"
#include "Elements/PCGDebugElement.h"
#include "Elements/PCGSelfPruning.h"
#include "Graph/PCGGraphCache.h"

bool IPCGElement::Execute(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(IPCGElement::Execute);
	check(Context && Context->NumAvailableTasks > 0 && Context->CurrentPhase < EPCGExecutionPhase::Done);
	check(Context->bIsRunningOnMainThread || !CanExecuteOnlyOnMainThread(Context));

	while (Context->CurrentPhase != EPCGExecutionPhase::Done)
	{
		bool bExecutionPostponed = false;

		switch (Context->CurrentPhase)
		{
			case EPCGExecutionPhase::NotExecuted: // Fall-through
			{
				PreExecute(Context);
				break;
			}

			case EPCGExecutionPhase::PrepareData:
			{
				FScopedCallTimer CallTimer(*this, Context);
				if (PrepareDataInternal(Context))
				{
					Context->CurrentPhase = EPCGExecutionPhase::Execute;
				}
				else
				{
					bExecutionPostponed = true;
				}
				break;
			}

			case EPCGExecutionPhase::Execute:
			{
				FScopedCallTimer CallTimer(*this, Context);
				if (ExecuteInternal(Context))
				{
					Context->CurrentPhase = EPCGExecutionPhase::PostExecute;
				}
				else
				{
					bExecutionPostponed = true;
				}
				break;
			}

			case EPCGExecutionPhase::PostExecute:
			{
				PostExecute(Context);
				break;
			}

			default: // should not happen
			{
				check(0);
				break;
			}
		}

		if (bExecutionPostponed || 
			Context->ShouldStop() ||
			(!Context->bIsRunningOnMainThread && CanExecuteOnlyOnMainThread(Context))) // phase change might require access to main thread
		{
			break;
		}
	}

	return Context->CurrentPhase == EPCGExecutionPhase::Done;
}

void IPCGElement::PreExecute(FPCGContext* Context) const
{
	// Check for early outs (task cancelled + node disabled)
	// Early out to stop execution
	if (Context->InputData.bCancelExecution || (!Context->SourceComponent.IsExplicitlyNull() && !Context->SourceComponent.IsValid()))
	{
		Context->OutputData.bCancelExecution = true;

		if (IsCancellable())
		{
			// Skip task completely
			Context->CurrentPhase = EPCGExecutionPhase::Done;
			return;
		}
	}

	// Prepare to move to prepare data phase
	Context->CurrentPhase = EPCGExecutionPhase::PrepareData;

	const UPCGSettings* Settings = Context->GetInputSettings<UPCGSettings>();
	if (!Settings)
	{
		return;
	}

	if (Settings->ExecutionMode == EPCGSettingsExecutionMode::Disabled)
	{
		//Pass-through - no execution
		Context->OutputData = Context->InputData;
		Context->CurrentPhase = EPCGExecutionPhase::PostExecute;
	}
	else
	{
		// Perform input filtering
		/** TODO - Placeholder feature */
		if (!Settings->FilterOnTags.IsEmpty())
		{
			// Move any of the inputs that don't have the tags to the outputs as a pass-through
			// NOTE: this breaks a bit the ordering of inputs, however, there's no obvious way around it
			TArray<FPCGTaggedData> FilteredTaggedData;
			for (FPCGTaggedData& TaggedData : Context->InputData.TaggedData)
			{
				if (TaggedData.Tags.Intersect(Settings->FilterOnTags).IsEmpty())
				{
					if (Settings->bPassThroughFilteredOutInputs)
					{
						Context->OutputData.TaggedData.Add(TaggedData);
					}
				}
				else // input has the required tags
				{
					FilteredTaggedData.Add(TaggedData);
				}
			}

			Context->InputData.TaggedData = MoveTemp(FilteredTaggedData);
			Context->BypassedOutputCount = Context->OutputData.TaggedData.Num();
		}
	}
}

bool IPCGElement::PrepareDataInternal(FPCGContext* Context) const
{
	return true;
}

void IPCGElement::PostExecute(FPCGContext* Context) const
{
	// Cleanup and validate output
	CleanupAndValidateOutput(Context);

#if WITH_EDITOR
	PCGE_LOG(Log, "Executed in (%f)s and (%d) call(s)", Context->ElapsedTime, Context->ExecutionCount);
#endif

	const UPCGSettings* Settings = Context->GetInputSettings<UPCGSettings>();

	// Apply tags on output
	/** TODO - Placeholder feature */
	if (Settings && !Settings->TagsAppliedOnOutput.IsEmpty())
	{
		for (int32 TaggedDataIdx = Context->BypassedOutputCount; TaggedDataIdx < Context->OutputData.TaggedData.Num(); ++TaggedDataIdx)
		{
			Context->OutputData.TaggedData[TaggedDataIdx].Tags.Append(Settings->TagsAppliedOnOutput);
		}
	}

	// Additional debug things (check for duplicates),
#if WITH_EDITOR
	if (Settings && Settings->DebugSettings.bCheckForDuplicates)
	{
		FPCGDataCollection ElementInputs = Context->InputData;
		FPCGDataCollection ElementOutputs = Context->OutputData;

		Context->InputData = ElementOutputs;
		Context->OutputData = FPCGDataCollection();

		PCGE_LOG(Verbose, "Performing remove duplicate points test (perf warning)");
		PCGSelfPruningElement::Execute(Context, EPCGSelfPruningType::RemoveDuplicates, 0.0f, false);

		Context->InputData = ElementInputs;
		Context->OutputData = ElementOutputs;
	}
#endif

	Context->CurrentPhase = EPCGExecutionPhase::Done;
}

#if WITH_EDITOR
void IPCGElement::DebugDisplay(FPCGContext* Context) const
{
	const UPCGSettings* Settings = Context->GetInputSettings<UPCGSettings>();
	if (Settings && (Settings->ExecutionMode == EPCGSettingsExecutionMode::Debug || Settings->ExecutionMode == EPCGSettingsExecutionMode::Isolated))
	{
		FPCGDataCollection ElementInputs = Context->InputData;
		FPCGDataCollection ElementOutputs = Context->OutputData;

		Context->InputData = ElementOutputs;
		Context->OutputData = FPCGDataCollection();

		PCGDebugElement::ExecuteDebugDisplay(Context);

		Context->InputData = ElementInputs;
		Context->OutputData = ElementOutputs;

		// Null out the output if this node is executed in isolation
		if (Settings->ExecutionMode == EPCGSettingsExecutionMode::Isolated)
		{
			Context->OutputData.bCancelExecution = true;
		}
	}
}

void IPCGElement::ResetTimers()
{
	FScopeLock Lock(&TimersLock);
	Timers.Empty();
	CurrentTimerIndex = 0;

}
#endif // WITH_EDITOR

void IPCGElement::CleanupAndValidateOutput(FPCGContext* Context) const
{
	check(Context);
	const UPCGSettings* Settings = Context->GetInputSettings<UPCGSettings>();

	if (!IsPassthrough() && Settings)
	{
		// Cleanup any residual labels if the node isn't supposed to produce them
		// TODO: this is a bit of a crutch, could be refactored out if we review the way we push tagged data
		TArray<FPCGPinProperties> OutputPinProperties = Settings->OutputPinProperties();
		if(OutputPinProperties.Num() == 1)
		{
			for (FPCGTaggedData& TaggedData : Context->OutputData.TaggedData)
			{
				TaggedData.Pin = OutputPinProperties[0].Label;
			}
		}

		// Validate all out data for errors in labels
#if WITH_EDITOR
		if (Settings->ExecutionMode != EPCGSettingsExecutionMode::Disabled)
		{
			for (FPCGTaggedData& TaggedData : Context->OutputData.TaggedData)
			{
				int32 MatchIndex = OutputPinProperties.IndexOfByPredicate([&TaggedData](const FPCGPinProperties& InProp) { return TaggedData.Pin == InProp.Label; });
				if (MatchIndex == INDEX_NONE)
				{
					PCGE_LOG(Warning, "Output generated for pin %s but cannot be routed", *TaggedData.Pin.ToString());
				}
				// TODO: Temporary fix for Settings directly from InputData (ie. from elements with code and not PCG nodes)
				else if(TaggedData.Data && !(OutputPinProperties[MatchIndex].AllowedTypes & TaggedData.Data->GetDataType()) && TaggedData.Data->GetDataType() != EPCGDataType::Settings)
				{
					PCGE_LOG(Warning, "Output generated for pin %s does not have a compatible type: %s", *TaggedData.Pin.ToString(), *UEnum::GetValueAsString(TaggedData.Data->GetDataType()));
				}
			}
		}
#endif
	}
}

#if WITH_EDITOR
IPCGElement::FScopedCallTimer::FScopedCallTimer(const IPCGElement& InOwner, FPCGContext* InContext)
	: Owner(InOwner), Context(InContext)
{
	StartTime = FPlatformTime::Seconds();
}

IPCGElement::FScopedCallTimer::~FScopedCallTimer()
{
	const double EndTime = FPlatformTime::Seconds();
	const double ElapsedTime = EndTime - StartTime;
	Context->ElapsedTime += ElapsedTime;
	Context->ExecutionCount++;

	constexpr int MaxNumberOfTrackedTimers = 100;
	{
		FScopeLock Lock(&Owner.TimersLock);
		if (Owner.Timers.Num() < MaxNumberOfTrackedTimers)
		{
			Owner.Timers.Add(ElapsedTime);
		}
		else
		{
			Owner.Timers[Owner.CurrentTimerIndex] = ElapsedTime;
		}
		Owner.CurrentTimerIndex = (Owner.CurrentTimerIndex + 1) % MaxNumberOfTrackedTimers;
	}
}
#endif // WITH_EDITOR

FPCGContext* FSimplePCGElement::Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node)
{
	FPCGContext* Context = new FPCGContext();
	Context->InputData = InputData;
	Context->SourceComponent = SourceComponent;
	Context->Node = Node;

	return Context;
}