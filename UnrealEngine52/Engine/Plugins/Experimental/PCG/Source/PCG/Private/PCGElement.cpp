// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGElement.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGGraph.h"
#include "Data/PCGPointData.h"
#include "Elements/PCGDebugElement.h"
#include "Elements/PCGSelfPruning.h"

#include "HAL/IConsoleManager.h"
#include "Utils/PCGExtraCapture.h"

#define LOCTEXT_NAMESPACE "PCGElement"

static TAutoConsoleVariable<bool> CVarPCGValidatePointMetadata(
	TEXT("pcg.debug.ValidatePointMetadata"),
	true,
	TEXT("Controls whether we validate that the metadata entry keys on the output point data are consistent"));

bool IPCGElement::Execute(FPCGContext* Context) const
{
	check(Context && Context->AsyncState.NumAvailableTasks > 0 && Context->CurrentPhase < EPCGExecutionPhase::Done);
	check(Context->AsyncState.bIsRunningOnMainThread || !CanExecuteOnlyOnMainThread(Context));

	while (Context->CurrentPhase != EPCGExecutionPhase::Done)
	{
		PCGUtils::FScopedCall ScopedCall(*this, Context);
		bool bExecutionPostponed = false;

		switch (Context->CurrentPhase)
		{
			case EPCGExecutionPhase::NotExecuted:
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(EPCGExecutionPhase::NotExecuted);
				PreExecute(Context);

				// Will override the settings if there is any override.
				Context->OverrideSettings();

				break;
			}

			case EPCGExecutionPhase::PrepareData:
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(EPCGExecutionPhase::PrepareData);

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
				TRACE_CPUPROFILER_EVENT_SCOPE(EPCGExecutionPhase::Execute);
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
				TRACE_CPUPROFILER_EVENT_SCOPE(EPCGExecutionPhase::PostExecute);
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
			Context->AsyncState.ShouldStop() ||
			(!Context->AsyncState.bIsRunningOnMainThread && CanExecuteOnlyOnMainThread(Context))) // phase change might require access to main thread
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

	const UPCGSettingsInterface* SettingsInterface = Context->GetInputSettingsInterface();
	const UPCGSettings* Settings = SettingsInterface ? SettingsInterface->GetSettings() : nullptr;

	if (!SettingsInterface || !Settings)
	{
		return;
	}

	if (!SettingsInterface->bEnabled)
	{
		//Pass-through - no execution
		DisabledPassThroughData(Context);

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

	const UPCGSettingsInterface* SettingsInterface = Context->GetInputSettingsInterface();
	const UPCGSettings* Settings = SettingsInterface ? SettingsInterface->GetSettings() : nullptr;

	// Apply tags on output
	/** TODO - Placeholder feature */
	if (Settings && !Settings->TagsAppliedOnOutput.IsEmpty())
	{
		for (int32 TaggedDataIdx = Context->BypassedOutputCount; TaggedDataIdx < Context->OutputData.TaggedData.Num(); ++TaggedDataIdx)
		{
			Context->OutputData.TaggedData[TaggedDataIdx].Tags.Append(Settings->TagsAppliedOnOutput);
		}
	}
	
	// Output data Crc
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(IPCGElement::PostExecute::CRC);

		// Some nodes benefit from computing an actual CRC from the data. This can halt the propagation of change/executions through the graph. For
		// data like landscapes we will never have a full accurate data crc for it so we'll tend to assume changed which triggers downstream
		// execution. Performing a detailed CRC of output data can detect real change in the data and halt the cascade of execution.
		const bool bShouldComputeFullOutputDataCrc = ShouldComputeFullOutputDataCrc();

		// Compute Crc from output data
		Context->OutputData.Crc = Context->OutputData.ComputeCrc(bShouldComputeFullOutputDataCrc);
	}

	// Additional debug things (check for duplicates),
#if WITH_EDITOR
	if (SettingsInterface && SettingsInterface->DebugSettings.bCheckForDuplicates)
	{
		FPCGDataCollection ElementInputs = Context->InputData;
		FPCGDataCollection ElementOutputs = Context->OutputData;

		Context->InputData = ElementOutputs;
		Context->OutputData = FPCGDataCollection();

		PCGE_LOG(Verbose, LogOnly, LOCTEXT("PerformingDuplicatePointTest", "Performing remove duplicate points test (perf warning)"));
		PCGSelfPruningElement::Execute(Context, EPCGSelfPruningType::RemoveDuplicates, 0.0f, false);

		Context->InputData = ElementInputs;
		Context->OutputData = ElementOutputs;
	}
#endif

	Context->CurrentPhase = EPCGExecutionPhase::Done;
}

void IPCGElement::DisabledPassThroughData(FPCGContext* Context) const
{
	check(Context);

	const UPCGSettings* Settings = Context->GetInputSettings<UPCGSettings>();
	check(Settings);

	if (!Context->Node)
	{
		// Full pass-through if we don't have a node
		Context->OutputData = Context->InputData;
		return;
	}

	if (Context->Node->GetInputPins().Num() == 0 || Context->Node->GetOutputPins().Num() == 0)
	{
		// No input pins or not output pins, return nothing
		return;
	}

	const UPCGPin* PassThroughPin = Context->Node->GetPassThroughInputPin();
	if (PassThroughPin == nullptr)
	{
		// No pin to grab pass through data from
		return;
	}

	// Grab data from pass-through pin
	Context->OutputData.TaggedData = Context->InputData.GetInputsByPin(PassThroughPin->Properties.Label);

	const EPCGDataType OutputType = Context->Node->GetOutputPins()[0]->Properties.AllowedTypes;

	// Pass through input data if it is not params, and if the output type supports it (e.g. if we have a incoming
	// surface connected to an input pin of type Any, do not pass the surface through to an output pin of type Point).
	auto InputDataShouldPassThrough = [OutputType](const FPCGTaggedData& InData)
	{
		EPCGDataType InputType = InData.Data ? InData.Data->GetDataType() : EPCGDataType::None;
		const bool bInputTypeNotWiderThanOutputType = !(InputType & ~OutputType);

		// Right now we allow edges from Spatial to Concrete. This can happen for example if a point processing node
		// is receving a Spatial data, and the node is disabled, it will want to pass the Spatial data through. In the
		// future we will force collapses/conversions. For now, allow an incoming Spatial to pass out through a Concrete.
		// TODO remove!
		const bool bAllowSpatialToConcrete = !!(InputType & EPCGDataType::Spatial) && !!(OutputType & EPCGDataType::Concrete);

		return InputType != EPCGDataType::Param && (bInputTypeNotWiderThanOutputType || bAllowSpatialToConcrete);
	};

	// Now remove any non-params edges, and if only one edge should come through, remove the others
	if (Settings->OnlyPassThroughOneEdgeWhenDisabled())
	{
		// Find first incoming non-params data that is coming through the pass through pin
		TArray<FPCGTaggedData> InputsOnFirstPin = Context->InputData.GetInputsByPin(PassThroughPin->Properties.Label);
		const int FirstNonParamsDataIndex = InputsOnFirstPin.IndexOfByPredicate(InputDataShouldPassThrough);

		if (FirstNonParamsDataIndex != INDEX_NONE)
		{
			// Remove everything except the data we found above
			for (int Index = Context->OutputData.TaggedData.Num() - 1; Index >= 0; --Index)
			{
				if (Index != FirstNonParamsDataIndex)
				{
					Context->OutputData.TaggedData.RemoveAt(Index);
				}
			}
		}
		else
		{
			// No data found to return
			Context->OutputData.TaggedData.Empty();
		}
	}
	else
	{
		// Remove any incoming non-params data that is coming through the pass through pin
		TArray<FPCGTaggedData> InputsOnFirstPin = Context->InputData.GetInputsByPin(PassThroughPin->Properties.Label);
		for (int Index = InputsOnFirstPin.Num() - 1; Index >= 0; --Index)
		{
			const FPCGTaggedData& Data = InputsOnFirstPin[Index];

			if (!InputDataShouldPassThrough(Data))
			{
				Context->OutputData.TaggedData.RemoveAt(Index);
			}
		}
	}
}

#if WITH_EDITOR
void IPCGElement::DebugDisplay(FPCGContext* Context) const
{
	const UPCGSettingsInterface* SettingsInterface = Context->GetInputSettingsInterface();
	if (SettingsInterface && SettingsInterface->bDebug)
	{
		FPCGDataCollection ElementInputs = Context->InputData;
		FPCGDataCollection ElementOutputs = Context->OutputData;

		Context->InputData = ElementOutputs;
		Context->OutputData = FPCGDataCollection();

		PCGDebugElement::ExecuteDebugDisplay(Context);

		Context->InputData = ElementInputs;
		Context->OutputData = ElementOutputs;
	}
}

#endif // WITH_EDITOR

void IPCGElement::CleanupAndValidateOutput(FPCGContext* Context) const
{
	check(Context);
	const UPCGSettingsInterface* SettingsInterface = Context->GetInputSettingsInterface();
	const UPCGSettings* Settings = SettingsInterface ? SettingsInterface->GetSettings() : nullptr;

	// Implementation note - disabled passthrough nodes can happen only in subgraphs/ spawn actor nodes
	// which will behave properly when disabled. 
	if (Settings && !IsPassthrough(Settings))
	{
		// Cleanup any residual labels if the node isn't supposed to produce them
		// TODO: this is a bit of a crutch, could be refactored out if we review the way we push tagged data
		TArray<FPCGPinProperties> OutputPinProperties = Settings->AllOutputPinProperties();
		if(OutputPinProperties.Num() == 1)
		{
			for (FPCGTaggedData& TaggedData : Context->OutputData.TaggedData)
			{
				TaggedData.Pin = OutputPinProperties[0].Label;
			}
		}

		// Validate all out data for errors in labels
#if WITH_EDITOR
		if (SettingsInterface->bEnabled)
		{
			for (FPCGTaggedData& TaggedData : Context->OutputData.TaggedData)
			{
				const int32 MatchIndex = OutputPinProperties.IndexOfByPredicate([&TaggedData](const FPCGPinProperties& InProp) { return TaggedData.Pin == InProp.Label; });
				if (MatchIndex == INDEX_NONE)
				{
					// Only display an error if we expected this data to have a pin.
					if (!TaggedData.bPinlessData)
					{
						PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("OutputCannotBeRouted", "Output data generated for non-existent output pin '{0}'"), FText::FromName(TaggedData.Pin)));
					}
				}
				else if (TaggedData.Data)
				{
					const bool bTypesOverlap = !!(OutputPinProperties[MatchIndex].AllowedTypes & TaggedData.Data->GetDataType());
					const bool bTypeIsSubset = !(~OutputPinProperties[MatchIndex].AllowedTypes & TaggedData.Data->GetDataType());
					// TODO: Temporary fix for Settings directly from InputData (ie. from elements with code and not PCG nodes)
					if ((!bTypesOverlap || !bTypeIsSubset) && TaggedData.Data->GetDataType() != EPCGDataType::Settings)
					{
						PCGE_LOG(Warning, GraphAndLog, FText::Format(LOCTEXT("OutputIncompatibleType", "Output data generated for pin '{0}' does not have a compatible type: '{1}'. Consider using more specific/narrower input pin types, or more general/wider output pin types."), FText::FromName(TaggedData.Pin), FText::FromString(UEnum::GetValueAsString(TaggedData.Data->GetDataType()))));
					}
				}

				if (CVarPCGValidatePointMetadata.GetValueOnAnyThread())
				{
					if (UPCGPointData* PointData = Cast<UPCGPointData>(TaggedData.Data))
					{
						const TArray<FPCGPoint>& NewPoints = PointData->GetPoints();
						const int32 MaxMetadataEntry = PointData->Metadata ? PointData->Metadata->GetItemCountForChild() : 0;

						bool bHasError = false;

						for(int32 PointIndex = 0; PointIndex < NewPoints.Num() && !bHasError; ++PointIndex)
						{
							bHasError |= (NewPoints[PointIndex].MetadataEntry >= MaxMetadataEntry);
						}

						if (bHasError)
						{
							PCGE_LOG(Warning, GraphAndLog, FText::Format(LOCTEXT("OutputMissingPointMetadata", "Output generated for pin '{0}' does not have valid point metadata"), FText::FromName(TaggedData.Pin)));
						}
					}
				}
			}
		}
#endif
	}
}

bool IPCGElement::IsCacheableInstance(const UPCGSettingsInterface* InSettingsInterface) const
{
	if (InSettingsInterface)
	{
		if (!InSettingsInterface->bEnabled)
		{
			return false;
		}
		else
		{
			return IsCacheable(InSettingsInterface->GetSettings());
		}
	}
	else
	{
		return false;
	}
}

void IPCGElement::GetDependenciesCrc(const FPCGDataCollection& InInput, const UPCGSettings* InSettings, UPCGComponent* InComponent, FPCGCrc& OutCrc) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*FString::Printf(TEXT("IPCGElement::GetDependenciesCrc (%s)"), InSettings ? *InSettings->GetFName().ToString() : TEXT("")));
	FPCGCrc Crc = InInput.Crc;

	if (InSettings)
	{
		const FPCGCrc& SettingsCrc = InSettings->GetCachedCrc();
		if (ensure(SettingsCrc.IsValid()))
		{
			Crc.Combine(SettingsCrc);
		}
	}

	if (InComponent)
	{
		Crc.Combine(InComponent->Seed);
	}

	OutCrc = Crc;
}

FPCGContext* FSimplePCGElement::Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node)
{
	FPCGContext* Context = new FPCGContext();
	Context->InputData = InputData;
	Context->SourceComponent = SourceComponent;
	Context->Node = Node;

	return Context;
}

#undef LOCTEXT_NAMESPACE
