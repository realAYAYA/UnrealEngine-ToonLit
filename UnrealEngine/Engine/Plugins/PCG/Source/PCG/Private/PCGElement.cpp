// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGElement.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGGraph.h"
#include "PCGSubsystem.h"
#include "Data/PCGPointData.h"
#include "Elements/PCGDebugElement.h"

#include "HAL/IConsoleManager.h"
#include "Utils/PCGExtraCapture.h"

#define LOCTEXT_NAMESPACE "PCGElement"

static TAutoConsoleVariable<bool> CVarPCGValidatePointMetadata(
	TEXT("pcg.debug.ValidatePointMetadata"),
	true,
	TEXT("Controls whether we validate that the metadata entry keys on the output point data are consistent"));

#if WITH_EDITOR
#define PCG_ELEMENT_EXECUTION_BREAKPOINT() \
	if (Context && Context->GetInputSettingsInterface() && Context->GetInputSettingsInterface()->bBreakDebugger) \
	{ \
		UE_DEBUG_BREAK(); \
	}
#else
#define PCG_ELEMENT_EXECUTION_BREAKPOINT()
#endif

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
				PCG_ELEMENT_EXECUTION_BREAKPOINT();

				PreExecute(Context);

				// Will override the settings if there is any override.
				Context->OverrideSettings();

				break;
			}

			case EPCGExecutionPhase::PrepareData:
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(EPCGExecutionPhase::PrepareData);
				PCG_ELEMENT_EXECUTION_BREAKPOINT();

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
				PCG_ELEMENT_EXECUTION_BREAKPOINT();

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
				PCG_ELEMENT_EXECUTION_BREAKPOINT();

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
	
	// Output data Crc
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(IPCGElement::PostExecute::CRC);

		// Some nodes benefit from computing an actual CRC from the data. This can halt the propagation of change/executions through the graph. For
		// data like landscapes we will never have a full accurate data crc for it so we'll tend to assume changed which triggers downstream
		// execution. Performing a detailed CRC of output data can detect real change in the data and halt the cascade of execution.
		const bool bShouldComputeFullOutputDataCrc = ShouldComputeFullOutputDataCrc(Context);

		// Compute Crc from output data which will include output pin labels.
		Context->OutputData.ComputeCrcs(bShouldComputeFullOutputDataCrc);
	}

#if WITH_EDITOR
	// Register the element to the component indicating the element has run and can have dynamic tracked keys.
	if (Settings && Settings->CanDynamicallyTrackKeys() && Context->SourceComponent.IsValid())
	{
		Context->SourceComponent->RegisterDynamicTracking(Settings, {});
	}
#endif // WITH_EDITOR

	Context->CurrentPhase = EPCGExecutionPhase::Done;
}

void IPCGElement::Abort(FPCGContext* Context) const
{
	AbortInternal(Context);
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

	const UPCGPin* PassThroughInputPin = Context->Node->GetPassThroughInputPin();
	const UPCGPin* PassThroughOutputPin = Context->Node->GetPassThroughOutputPin();
	if (!PassThroughInputPin || !PassThroughOutputPin)
	{
		// No pin to grab pass through data from or to pass data to.
		return;
	}

	const EPCGDataType OutputType = PassThroughOutputPin->GetCurrentTypes();

	// Grab data from pass-through pin, push it all to output pin
	Context->OutputData.TaggedData = Context->InputData.GetInputsByPin(PassThroughInputPin->Properties.Label);
	for (FPCGTaggedData& Data : Context->OutputData.TaggedData)
	{
		Data.Pin = PassThroughOutputPin->Properties.Label;
	}

	// Pass through input data if both it and the output are params, or if the output type supports it (e.g. if we have a incoming
	// surface connected to an input pin of type Any, do not pass the surface through to an output pin of type Point).
	auto InputDataShouldPassThrough = [OutputType](const FPCGTaggedData& InData)
	{
		EPCGDataType InputType = InData.Data ? InData.Data->GetDataType() : EPCGDataType::None;
		const bool bInputTypeNotWiderThanOutputType = !(InputType & ~OutputType);

		// Right now we allow edges from Spatial to Concrete. This can happen for example if a point processing node
		// is receiving a Spatial data, and the node is disabled, it will want to pass the Spatial data through. In the
		// future we will force collapses/conversions. For now, allow an incoming Spatial to pass out through a Concrete.
		// TODO remove!
		const bool bAllowSpatialToConcrete = !!(InputType & EPCGDataType::Spatial) && !!(OutputType & EPCGDataType::Concrete);

		return (InputType != EPCGDataType::Param || OutputType == EPCGDataType::Param) && (bInputTypeNotWiderThanOutputType || bAllowSpatialToConcrete);
	};

	// Now remove any non-params edges, and if only one edge should come through, remove the others
	if (Settings->OnlyPassThroughOneEdgeWhenDisabled())
	{
		// Find first incoming non-params data that is coming through the pass through pin
		TArray<FPCGTaggedData> InputsOnFirstPin = Context->InputData.GetInputsByPin(PassThroughInputPin->Properties.Label);
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
		TArray<FPCGTaggedData> InputsOnFirstPin = Context->InputData.GetInputsByPin(PassThroughInputPin->Properties.Label);
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

FPCGContext* IPCGElement::CreateContext()
{
	return new FPCGContext();
}

#if WITH_EDITOR
void IPCGElement::DebugDisplay(FPCGContext* Context) const
{
	// Check Debug flag.
	const UPCGSettingsInterface* SettingsInterface = Context->GetInputSettingsInterface();
	if (!SettingsInterface || !SettingsInterface->bDebug)
	{
		return;
	}

	// If graph is being inspected, only display Debug if the component is being inspected, or in the HiGen case also display if
	// this component is a parent of an inspected component (because this data is available to child components).

	// If the graph is not being inspected, or the current component is being inspected, then we know we should display
	// debug, if not then we do further checks.
	UPCGGraph* Graph = Context->SourceComponent.Get() ? Context->SourceComponent->GetGraph() : nullptr;
	if (Graph && Graph->IsInspecting() && !Context->SourceComponent->IsInspecting() && Graph->DebugFlagAppliesToIndividualComponents())
	{
		// If we're no doing HiGen, or if the current component is not a local component (and therefore will not have children),
		// then do not display debug.
		if (!Graph->IsHierarchicalGenerationEnabled())
		{
			return;
		}

		// If a child of this component is being inspected (a local component on smaller grid and overlapping) then we still show debug,
		// because this data is available to that child for use.
		if (UPCGSubsystem* Subsystem = UPCGSubsystem::GetInstance(Context->SourceComponent->GetWorld()))
		{
			const uint32 ThisGenerationGridSize = Context->SourceComponent->GetGenerationGridSize();

			bool bFoundInspectedChildComponent = false;
			Subsystem->ForAllOverlappingComponentsInHierarchy(Context->SourceComponent.Get(), [ThisGenerationGridSize, &bFoundInspectedChildComponent](UPCGComponent* InLocalComponent)
			{
				if (InLocalComponent->GetGenerationGridSize() < ThisGenerationGridSize && InLocalComponent->IsInspecting())
				{
					bFoundInspectedChildComponent = true;
				}
			});

			// If no inspected child component then don't display debug.
			if (!bFoundInspectedChildComponent)
			{
				return;
			}
		}
	}

	FPCGDataCollection ElementInputs = Context->InputData;
	FPCGDataCollection ElementOutputs = Context->OutputData;

	Context->InputData = ElementOutputs;
	Context->OutputData = FPCGDataCollection();

	PCGDebugElement::ExecuteDebugDisplay(Context);

	Context->InputData = ElementInputs;
	Context->OutputData = ElementOutputs;
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
				if (!TaggedData.bPinlessData)
				{
					TaggedData.Pin = OutputPinProperties[0].Label;
				}				
			}
		}

		// Validate all out data for errors in labels
#if WITH_EDITOR
		if (SettingsInterface->bEnabled)
		{
			// remove null outputs
			Context->OutputData.TaggedData.RemoveAll([this, Context](const FPCGTaggedData& TaggedData){

				if (TaggedData.Data == nullptr)
				{
					PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("NullPinOutputData", "Invalid output(s) generated for pin '{0}'"), FText::FromName(TaggedData.Pin)));
					return true;
				}

				return false;
			});


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
				else if (ensure(TaggedData.Data))
				{
					// Try to get dynamic current pin types, otherwise settle for static types
					const UPCGPin* OutputPin = Context->Node ? Context->Node->GetOutputPin(OutputPinProperties[MatchIndex].Label) : nullptr;
					const EPCGDataType PinTypes = OutputPin ? OutputPin->GetCurrentTypes() : OutputPinProperties[MatchIndex].AllowedTypes;

					const bool bTypesOverlap = !!(PinTypes & TaggedData.Data->GetDataType());
					const bool bTypeIsSubset = !(~PinTypes & TaggedData.Data->GetDataType());
					// TODO: Temporary fix for Settings directly from InputData (ie. from elements with code and not PCG nodes)
					if ((!bTypesOverlap || !bTypeIsSubset) && TaggedData.Data->GetDataType() != EPCGDataType::Settings)
					{
						PCGE_LOG(Warning, GraphAndLog, FText::Format(LOCTEXT("OutputIncompatibleType", "Output data generated for pin '{0}' does not have a compatible type: '{1}'. Consider using more specific/narrower input pin types, or more general/wider output pin types."), FText::FromName(TaggedData.Pin), FText::FromString(UEnum::GetValueAsString(TaggedData.Data->GetDataType()))));
					}
				}

				if (CVarPCGValidatePointMetadata.GetValueOnAnyThread())
				{
					if (const UPCGPointData* PointData = Cast<UPCGPointData>(TaggedData.Data))
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

FPCGContext* IPCGElement::Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node)
{
	FPCGContext* Context = CreateContext();
	Context->InputData = InputData;
	Context->SourceComponent = SourceComponent;
	Context->Node = Node;

	return Context;
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

	// Start from a random prime.
	FPCGCrc Crc(1000003);

	// The cached data CRCs are computed in FPCGGraphExecutor::BuildTaskInput and incorporate data CRC, tags, output pin label and input pin label.
	for (const FPCGCrc& DataCrc : InInput.DataCrcs)
	{
		Crc.Combine(DataCrc);
	}

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

#undef LOCTEXT_NAMESPACE
