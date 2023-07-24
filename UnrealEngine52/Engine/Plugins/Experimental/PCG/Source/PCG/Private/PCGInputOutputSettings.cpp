// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGInputOutputSettings.h"

#include "PCGContext.h"
#include "PCGPin.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGInputOutputSettings)

#define LOCTEXT_NAMESPACE "PCGInputOutputElement"

bool FPCGInputOutputElement::ExecuteInternal(FPCGContext* Context) const
{
	// Essentially a pass-through element
	Context->OutputData = Context->InputData;
	return true;
}

UPCGGraphInputOutputSettings::UPCGGraphInputOutputSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	StaticInLabels.Emplace(PCGPinConstants::DefaultInputLabel, LOCTEXT("InputOutputInPinTooltip",
		"Provides the same result as the Input pin."
	));
	StaticAdvancedInLabels.Emplace(PCGInputOutputConstants::DefaultInputLabel, LOCTEXT("InputOutputInputPinTooltip",
		"Takes the output of the Actor pin and if the 'Input Type' setting on the PCG Component is set to Landscape, combines it with the result of the Landscape pin. "
		"If the Actor data is two dimensional it will be projected onto the landscape, otherwise it will be intersected."
	));
	StaticAdvancedInLabels.Emplace(PCGInputOutputConstants::DefaultActorLabel, LOCTEXT("InputOutputActorPinTooltip",
		"If this is a partitioned component, then this will be the intersection of the current partition actor bounds with the following. "
		"If the actor is a Landscape Proxy, then this provide a landscape data. "
		"Otherwise if the actor is a volume, this will provide a volume shape matching the actor bounds. "
		"Otherwise if the 'Parse Actor Components' setting is enabled on the PCG Component, this will be all compatible components on the actor (Landscape Splines, Splines, Shapes, Primitives) unioned together. "
		"Otherwise a single point will be provided at the actor position."
	));
	StaticAdvancedInLabels.Emplace(PCGInputOutputConstants::DefaultOriginalActorLabel, LOCTEXT("InputOutputOriginalActorPinTooltip",
		"If the actor is a partition actor, this will pull data from the generating PCG actor. Otherwise it will provide the same data as the Actor pin."
	));
	StaticAdvancedInLabels.Emplace(PCGInputOutputConstants::DefaultLandscapeLabel,
		LOCTEXT("InputOutputLandscapePinTooltip", "Provides the landscape represented by this actor if it is a Landscape Proxy, otherwise it returns any landscapes overlapping this actor in the level."
	));
	StaticAdvancedInLabels.Emplace(PCGInputOutputConstants::DefaultLandscapeHeightLabel, LOCTEXT("InputOutputLandscapeHeightPinTooltip",
		"Similar to Landscape pin, but only provides height data and not other layers."
	));
	
	StaticOutLabels.Emplace(PCGPinConstants::DefaultOutputLabel);
}

void UPCGGraphInputOutputSettings::PostLoad()
{
	Super::PostLoad();

	if (!PinLabels_DEPRECATED.IsEmpty())
	{
		for (const FName& PinLabel : PinLabels_DEPRECATED)
		{
			CustomPins.Emplace(PinLabel);
		}

		PinLabels_DEPRECATED.Reset();
	}
}

TArray<FPCGPinProperties> UPCGGraphInputOutputSettings::GetPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	const bool bIsInputPin = bIsInput;
	const EPCGDataType DefaultPinDataType = bIsInput ? EPCGDataType::Spatial : EPCGDataType::Any;
	Algo::Transform(StaticLabels(), PinProperties, [bIsInputPin, DefaultPinDataType](const FLabelAndTooltip& InLabelAndTooltip) {
		return FPCGPinProperties(InLabelAndTooltip.Label, DefaultPinDataType, /*bMultiConnections=*/true, /*bMultiData=*/true, InLabelAndTooltip.Tooltip);
	});
	

	Algo::Transform(StaticAdvancedLabels(), PinProperties, [this, DefaultPinDataType](const FLabelAndTooltip& InLabelAndTooltip) {
		const bool bIsLandscapePin = (InLabelAndTooltip.Label == PCGInputOutputConstants::DefaultLandscapeLabel || InLabelAndTooltip.Label == PCGInputOutputConstants::DefaultLandscapeHeightLabel);
		const EPCGDataType PinType = bIsLandscapePin ? EPCGDataType::Surface : DefaultPinDataType;
		FPCGPinProperties Res(InLabelAndTooltip.Label, PinType, /*bMultiConnection=*/true, /*bMultiData=*/false, InLabelAndTooltip.Tooltip);
		Res.bAdvancedPin = true;
		return Res;
	});


	PinProperties.Append(CustomPins);
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGGraphInputOutputSettings::InputPinProperties() const
{
	return GetPinProperties();
}

TArray<FPCGPinProperties> UPCGGraphInputOutputSettings::OutputPinProperties() const
{
	return GetPinProperties();
}

TArray<FPCGPinProperties> UPCGGraphInputOutputSettings::DefaultInputPinProperties() const
{
	// It is important for serialization that this is not modified, or it could break existing graphs.
	TArray<FPCGPinProperties> PinProperties;
	const bool bIsInputPin = bIsInput;
	const EPCGDataType DefaultPinDataType = bIsInput ? EPCGDataType::Spatial : EPCGDataType::Any;
	Algo::Transform(StaticLabels(), PinProperties, [bIsInputPin, DefaultPinDataType](const FLabelAndTooltip& InLabelAndTooltip) {
		return FPCGPinProperties(InLabelAndTooltip.Label, DefaultPinDataType, /*bMultiConnections=*/true, /*bMultiData=*/true, InLabelAndTooltip.Tooltip);
	});

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGGraphInputOutputSettings::DefaultOutputPinProperties() const
{
	return DefaultInputPinProperties();
}

const FPCGPinProperties& UPCGGraphInputOutputSettings::AddCustomPin(const FPCGPinProperties& NewCustomPinProperties)
{
	Modify();
	int32 Index = CustomPins.Add(NewCustomPinProperties);
	FixCustomPinProperties();
	return CustomPins[Index];
}

bool UPCGGraphInputOutputSettings::IsCustomPin(const UPCGPin* InPin) const
{
	check(InPin);
	return CustomPins.Contains(InPin->Properties);
}

#if WITH_EDITOR
void UPCGGraphInputOutputSettings::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property)
	{
		// Label has changed if we have modified "CustomPins" array
		bool bLabelChanged = PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UPCGGraphInputOutputSettings, CustomPins);

		if (!bLabelChanged)
		{
			// Or might has changed if an element of the array "CustomPin" was modified.
			bLabelChanged = PropertyChangedEvent.MemberProperty &&
				PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UPCGGraphInputOutputSettings, CustomPins) &&
				PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(FPCGPinProperties, Label);
		}

		if (bLabelChanged)
		{
			FixCustomPinProperties();
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif // WITH_EDITOR

void UPCGGraphInputOutputSettings::FixCustomPinProperties()
{
	// No need to fix if we have no custom pins
	if (CustomPins.IsEmpty())
	{
		return;
	}

	TSet<FName> AllLabels;

	// First gather all our static labels
	for (FLabelAndTooltip LabelAndTooltip : StaticLabels())
	{
		AllLabels.Emplace(LabelAndTooltip.Label);
	}

	for (FLabelAndTooltip LabelAndTooltip : StaticAdvancedLabels())
	{
		AllLabels.Emplace(LabelAndTooltip.Label);
	}

	bool bWasModified = false;

	for (FPCGPinProperties& CustomPinProperties : CustomPins)
	{
		// Avoid "None" pin label
		if (CustomPinProperties.Label == NAME_None)
		{
			if (!bWasModified)
			{
				bWasModified = true;
				Modify();
			}

			CustomPinProperties.Label = PCGInputOutputConstants::DefaultNewCustomPinName;
		}

		uint32 Count = 1;
		FString OriginalLabel = CustomPinProperties.Label.ToString();

		while (AllLabels.Contains(CustomPinProperties.Label))
		{
			if (!bWasModified)
			{
				bWasModified = true;
				Modify();
			}

			CustomPinProperties.Label = FName(FString::Printf(TEXT("%s%d"), *OriginalLabel, Count++));
		}

		AllLabels.Emplace(CustomPinProperties.Label);
	}
}

#undef LOCTEXT_NAMESPACE
