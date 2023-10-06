// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGFilterByType.h"

#include "PCGContext.h"
#include "PCGPin.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGFilterByType)

#define LOCTEXT_NAMESPACE "PCGFilterByType"

#if WITH_EDITOR
FText UPCGFilterByTypeSettings::GetDefaultNodeTitle() const
{
	const FText TypeText = FText::FromString(StaticEnum<EPCGDataType>()->GetDisplayNameTextByValue(static_cast<int64>(TargetType)).ToString());
	return FText::Format(LOCTEXT("NodeTitleFormat", "Filter - {0}"), TypeText);
}

FText UPCGFilterByTypeSettings::GetNodeTooltipText() const
{
	return LOCTEXT("FilterByTypeNodeTooltip", "Filters data in the collection according to data type");
}
#endif

EPCGDataType UPCGFilterByTypeSettings::GetCurrentPinTypes(const UPCGPin* InPin) const
{
	check(InPin);
	if (!InPin->IsOutputPin())
	{
		return Super::GetCurrentPinTypes(InPin);
	}

	return TargetType;
}

FPCGElementPtr UPCGFilterByTypeSettings::CreateElement() const
{
	return MakeShared<FPCGFilterByTypeElement>();
}

bool FPCGFilterByTypeElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGFilterByTypeElement::Execute);
	check(InContext);

	const UPCGFilterByTypeSettings* Settings = InContext->GetInputSettings<UPCGFilterByTypeSettings>();
	check(Settings);

	TArray<FPCGTaggedData> Inputs = InContext->InputData.TaggedData;
	TArray<FPCGTaggedData>& Outputs = InContext->OutputData.TaggedData;

	for (const FPCGTaggedData& Input : Inputs)
	{
		if (!Input.Data)
		{
			continue;
		}

		// No match if data does not overlap in type with the target, or if data is broader than the target
		const bool bDataTypeOverlapsWithTarget = !!(Input.Data->GetDataType() & Settings->TargetType);
		const bool bDataTypeBroaderThanTarget = !!(Input.Data->GetDataType() & ~Settings->TargetType);
		if (!bDataTypeOverlapsWithTarget || bDataTypeBroaderThanTarget)
		{
			continue;
		}

		Outputs.Add(Input);
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
