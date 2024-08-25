// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGApplyOnActor.h"

#include "PCGContext.h"
#include "PCGModule.h"
#include "Helpers/PCGHelpers.h"

#include "GameFramework/Actor.h"

#define LOCTEXT_NAMESPACE "PCGApplyOnActorElement"

namespace PCGApplyOnActorConstants
{
	const FText DependencyTooltip = LOCTEXT("DependencyPinTooltip", "Data passed to this pin will be used to order execution but will otherwise not contribute to the results of this node.");
	const FName ActorPropertyOverridesLabel = TEXT("Property Overrides");
	const FText ActorPropertyOverridesTooltip = LOCTEXT("ActorOverrideToolTip", "Provide property overrides for the target actor. The attribute name must match the InputSource name in the actor property override description.");
}

TArray<FPCGPinProperties> UPCGApplyOnActorSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPinConstants::DefaultDependencyOnlyLabel, EPCGDataType::Any, /*bAllowMultipleConnections=*/true, /*bAllowMultipleData=*/true, PCGApplyOnActorConstants::DependencyTooltip);
	PinProperties.Add(PCGObjectPropertyOverrideHelpers::CreateObjectPropertiesOverridePin(PCGApplyOnActorConstants::ActorPropertyOverridesLabel, PCGApplyOnActorConstants::ActorPropertyOverridesTooltip));
	return PinProperties;
}

FPCGElementPtr UPCGApplyOnActorSettings::CreateElement() const
{
	return MakeShared<FPCGApplyOnActorElement>();
}

bool FPCGApplyOnActorElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGApplyOnActorElement::Execute);

	check(Context);

	const UPCGApplyOnActorSettings* Settings = Context->GetInputSettings<UPCGApplyOnActorSettings>();
	check(Settings);

	AActor* TargetActor = Settings->TargetActor.Get() ? Settings->TargetActor.Get() : Context->GetTargetActor(nullptr);
	if (!TargetActor)
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("InvalidTargetActor", "Invalid target actor."));
		return true;
	}

	PCGObjectPropertyOverrideHelpers::ApplyOverridesFromParams(Settings->PropertyOverrideDescriptions, TargetActor, PCGApplyOnActorConstants::ActorPropertyOverridesLabel, Context);

	for (UFunction* Function : PCGHelpers::FindUserFunctions(TargetActor->GetClass(), Settings->PostProcessFunctionNames, { UPCGFunctionPrototypes::GetPrototypeWithNoParams() }, Context))
	{
		TargetActor->ProcessEvent(Function, nullptr);
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
