// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGFilterByAttribute.h"

#include "PCGContext.h"
#include "PCGData.h"
#include "Metadata/Accessors/IPCGAttributeAccessor.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"

#define LOCTEXT_NAMESPACE "PCGFilterByAttributeElement"

#if WITH_EDITOR
FText UPCGFilterByAttributeSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Filter Data By Attribute");
}
#endif // WITH_EDITOR

FString UPCGFilterByAttributeSettings::GetAdditionalTitleInformation() const
{
#if WITH_EDITOR
	if (IsPropertyOverriddenByPin(GET_MEMBER_NAME_CHECKED(UPCGFilterByAttributeSettings, Attribute)))
	{
		return FString();
	}
	else
#endif
	{
		return Attribute.ToString();
	}
}

FPCGElementPtr UPCGFilterByAttributeSettings::CreateElement() const
{
	return MakeShared<FPCGFilterByAttributeElement>();
}

bool FPCGFilterByAttributeElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGFilterByAttributeElement::Execute);
	check(Context);

	const UPCGFilterByAttributeSettings* Settings = Context->GetInputSettings<UPCGFilterByAttributeSettings>();
	check(Settings);

	FPCGAttributePropertySelector Selector;
	Selector.Update(Settings->Attribute.ToString());

	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	for (const FPCGTaggedData& Input : Inputs)
	{
		FPCGTaggedData& Output = Outputs.Add_GetRef(Input);
		Output.Pin = PCGPinConstants::DefaultOutFilterLabel;

		if (!Input.Data)
		{
			continue;
		}

		TUniquePtr<const IPCGAttributeAccessor> Accessor = PCGAttributeAccessorHelpers::CreateConstAccessor(Input.Data, Selector, /*bQuiet=*/true);

		if (Accessor && Accessor.IsValid())
		{
			Output.Pin = PCGPinConstants::DefaultInFilterLabel;
		}
	}

	return true;
}

#undef LOCTEXT_NAMESPACE