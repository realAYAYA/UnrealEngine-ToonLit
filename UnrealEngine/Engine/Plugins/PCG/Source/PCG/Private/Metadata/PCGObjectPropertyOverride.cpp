// Copyright Epic Games, Inc. All Rights Reserved.

#include "Metadata/PCGObjectPropertyOverride.h"

#include "PCGContext.h"
#include "PCGData.h"
#include "PCGModule.h"
#include "PCGParamData.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"

#include "GameFramework/Actor.h"

#define LOCTEXT_NAMESPACE "PCGObjectPropertyOverride"

namespace PCGObjectPropertyOverrideHelpers
{
	FPCGPinProperties CreateObjectPropertiesOverridePin(FName Label, const FText& Tooltip)
	{
		FPCGPinProperties ObjectOverridePinProperties(Label, EPCGDataType::Param, /*bAllowMultipleConnections=*/false, /*bAllowMultipleData=*/false, Tooltip);
		ObjectOverridePinProperties.SetAdvancedPin();
		return ObjectOverridePinProperties;
	}

	void ApplyOverridesFromParams(const TArray<FPCGObjectPropertyOverrideDescription>& InObjectPropertyOverrideDescriptions, AActor* TargetActor, FName OverridesPinLabel, FPCGContext* Context)
	{
		if (!Context)
		{
			return;
		}

		const TArray<FPCGTaggedData> OverrideInputs = Context->InputData.GetInputsByPin(OverridesPinLabel);

		if (OverrideInputs.Num() == 0)
		{
			return;
		}
		else if (OverrideInputs.Num() > 1)
		{
			PCGLog::LogWarningOnGraph(FText::Format(LOCTEXT("MoreThanOneData", "More than one data was found on pin '{0}'. Only using the first one."), FText::FromName(OverridesPinLabel)), Context);
		}

		const UPCGParamData* ParamData = Cast<const UPCGParamData>(OverrideInputs[0].Data);

		if (!ParamData)
		{
			PCGLog::LogErrorOnGraph(LOCTEXT("InvalidActorOverrideData", "Invalid input data type for Actor Property Overrides pin, must be of type Param."), Context);
			return;
		}

		FPCGObjectOverrides ActorOverrides(TargetActor);
		ActorOverrides.Initialize(InObjectPropertyOverrideDescriptions, TargetActor, ParamData, Context);
		if (!ActorOverrides.Apply(/*InputKeyIndex=*/0)) // Use the First Entry of the param data for override (similar to what is done in Parameter Overrides in FPCGContext)
		{
			PCGLog::LogErrorOnGraph(FText::Format(LOCTEXT("ApplyOverrideFailed", "Failed to apply property overrides to actor '%s' from an attribute set."), FText::FromName(TargetActor->GetClass()->GetFName())), Context);
		}
	}
}

void FPCGObjectSingleOverride::Initialize(const FPCGAttributePropertySelector& InputSelector, const FString& OutputProperty, const UStruct* TemplateClass, const UPCGData* SourceData, FPCGContext* Context)
{
	InputKeys = PCGAttributeAccessorHelpers::CreateConstKeys(SourceData, InputSelector);
	ObjectOverrideInputAccessor = PCGAttributeAccessorHelpers::CreateConstAccessor(SourceData, InputSelector);

	if (!ObjectOverrideInputAccessor.IsValid())
	{
		PCGLog::LogWarningOnGraph(FText::Format(NSLOCTEXT("PCGObjectPropertyOverride", "OverrideInputInvalid", "ObjectOverride for input '{0}' is invalid or unsupported. Check the attribute or property selection."), InputSelector.GetDisplayText()), Context);
		return;
	}

	const FPCGAttributePropertySelector OutputSelector = FPCGAttributePropertySelector::CreateSelectorFromString(OutputProperty);
	// TODO: Move implementation into a new helper: PCGAttributeAccessorHelpers::CreatePropertyAccessor(const FPCGAttributePropertySelector& Selector, UStruct* Class)
	const TArray<FString>& ExtraNames = OutputSelector.GetExtraNames();
	if (ExtraNames.IsEmpty())
	{
		ObjectOverrideOutputAccessor = PCGAttributeAccessorHelpers::CreatePropertyAccessor(FName(OutputProperty), TemplateClass);
	}
	else
	{
		TArray<FName> PropertyNames;
		PropertyNames.Reserve(ExtraNames.Num() + 1);
		PropertyNames.Add(OutputSelector.GetAttributeName());
		for (const FString& Name : ExtraNames)
		{
			PropertyNames.Add(FName(Name));
		}

		ObjectOverrideOutputAccessor = PCGAttributeAccessorHelpers::CreatePropertyChainAccessor(PropertyNames, TemplateClass);
	}

	if (!ObjectOverrideOutputAccessor.IsValid())
	{
		PCGLog::LogWarningOnGraph(FText::Format(NSLOCTEXT("PCGObjectPropertyOverride", "OverrideOutputInvalid", "ObjectOverride for object property '{0}' is invalid or unsupported. Check the attribute or property selection."), FText::FromString(OutputProperty)), Context);
		return;
	}

	if (!PCG::Private::IsBroadcastableOrConstructible(ObjectOverrideInputAccessor->GetUnderlyingType(), ObjectOverrideOutputAccessor->GetUnderlyingType()))
	{
		PCGLog::LogWarningOnGraph(
			FText::Format(
				NSLOCTEXT("PCGObjectPropertyOverride", "TypesIncompatible", "ObjectOverride cannot set input '{0}' to output '{1}'. Cannot convert type '{2}' to type '{3}'. Will be skipped."),
				InputSelector.GetDisplayText(),
				FText::FromString(OutputProperty),
				PCG::Private::GetTypeNameText(ObjectOverrideInputAccessor->GetUnderlyingType()),
				PCG::Private::GetTypeNameText(ObjectOverrideOutputAccessor->GetUnderlyingType())),
			Context);

		ObjectOverrideInputAccessor.Reset();
		ObjectOverrideOutputAccessor.Reset();
		return;
	}

	auto CreateGetterSetter = [this](auto Dummy)
	{
		using Type = decltype(Dummy);

		ObjectOverrideFunction = &FPCGObjectSingleOverride::ApplyImpl<Type>;
	};

	PCGMetadataAttribute::CallbackWithRightType(ObjectOverrideOutputAccessor->GetUnderlyingType(), CreateGetterSetter);
}

bool FPCGObjectSingleOverride::IsValid() const
{
	return InputKeys.IsValid() && ObjectOverrideInputAccessor.IsValid() && ObjectOverrideOutputAccessor.IsValid() && ObjectOverrideFunction;
}

bool FPCGObjectSingleOverride::Apply(int32 InputKeyIndex, IPCGAttributeAccessorKeys& OutputKey)
{
	return Invoke(ObjectOverrideFunction, this, InputKeyIndex, OutputKey);
}

#undef LOCTEXT_NAMESPACE
