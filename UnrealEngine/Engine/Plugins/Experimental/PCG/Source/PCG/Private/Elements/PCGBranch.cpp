// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGBranch.h"

#include "PCGCommon.h"
#include "PCGContext.h"
#include "PCGPin.h"
#include "Elements/PCGGather.h"

#define LOCTEXT_NAMESPACE "FPCGBranchElement"

namespace PCGBranchConstants
{
	const FName OutputLabelA = TEXT("Output A");
	const FName OutputLabelB = TEXT("Output B");
	const FText NodeTitleBase = LOCTEXT("NodeTitle", "Branch");
}

#if WITH_EDITOR
FText UPCGBranchSettings::GetDefaultNodeTitle() const
{
	// TODO: This should statically update or dynamically update, if overridden, for which branch was taken, ie. Branch (Output A)
	return PCGBranchConstants::NodeTitleBase;
}

FText UPCGBranchSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip", "Control flow node that will route the input to either Output A or Output B, based on the 'Output To B' property - which can also be overridden.");
}
#endif // WITH_EDITOR

EPCGDataType UPCGBranchSettings::GetCurrentPinTypes(const UPCGPin* Pin) const
{
	check(Pin);
	if (Pin->IsOutputPin())
	{
		const EPCGDataType InputTypeUnion = GetTypeUnionOfIncidentEdges(PCGPinConstants::DefaultInputLabel);
		return InputTypeUnion != EPCGDataType::None ? InputTypeUnion : EPCGDataType::Any;
	}

	return Super::GetCurrentPinTypes(Pin);
}

TArray<FPCGPinProperties> UPCGBranchSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGBranchConstants::OutputLabelA,
		EPCGDataType::Any,
		/*bInAllowMultipleConnections=*/true,
		/*bAllowMultipleData=*/true,
		LOCTEXT("OutputPinTooltipA", "Will only route input if 'Output To B' (overridable) is false"));
	PinProperties.Emplace(PCGBranchConstants::OutputLabelB,
		EPCGDataType::Any,
		/*bInAllowMultipleConnections=*/true,
		/*bAllowMultipleData=*/true,
		LOCTEXT("OutputPinTooltipB", "Will only route input if 'Output To B' (overridable) is true"));

	return PinProperties;
}

FPCGElementPtr UPCGBranchSettings::CreateElement() const
{
	return MakeShared<FPCGBranchElement>();
}

bool FPCGBranchElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGBranchElement::ExecuteInternal);

	const UPCGBranchSettings* Settings = Context->GetInputSettings<UPCGBranchSettings>();
	check(Settings);

	const FName SelectedPinLabel = Settings->bOutputToB ? PCGBranchConstants::OutputLabelB : PCGBranchConstants::OutputLabelA;

	// Reuse the functionality of the Gather Node
	Context->OutputData = PCGGather::GatherDataForPin(Context->InputData, PCGPinConstants::DefaultInputLabel, SelectedPinLabel);

	return true;
}

#undef LOCTEXT_NAMESPACE