// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGSubgraph.h"

#include "PCGComponent.h"
#include "PCGPin.h"
#include "PCGSubsystem.h"
#include "Data/PCGUserParametersData.h"
#include "Helpers/PCGSettingsHelpers.h"

#include "Algo/Find.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGSubgraph)

namespace PCGSubgraphSettings
{
	void RemoveAdvancedModeOnConnectedPins(const UPCGGraph* Subgraph, TArray<FPCGPinProperties>& InOutPinProperties, const bool bIsInput)
	{
		const UPCGNode* SubgraphNode = bIsInput ? Subgraph->GetInputNode() : Subgraph->GetOutputNode();
		check(SubgraphNode);

		for (FPCGPinProperties& PinProperties : InOutPinProperties)
		{
			const UPCGPin* Pin = bIsInput ? SubgraphNode->GetOutputPin(PinProperties.Label) : SubgraphNode->GetInputPin(PinProperties.Label);
			if (ensure(Pin) && Pin->IsConnected())
			{
				PinProperties.bAdvancedPin = false;
			}
		}
	}
}

UPCGGraph* UPCGBaseSubgraphSettings::GetSubgraph() const
{
	UPCGGraphInterface* SubgraphInterface = GetSubgraphInterface();
	return SubgraphInterface ? SubgraphInterface->GetGraph() : nullptr;
}

void UPCGBaseSubgraphSettings::PostInitProperties()
{
	Super::PostInitProperties();

#if WITH_EDITOR
	if (UPCGGraphInterface* Subgraph = GetSubgraphInterface())
	{
		Subgraph->OnGraphChangedDelegate.AddUObject(this, &UPCGBaseSubgraphSettings::OnSubgraphChanged);
	}
#endif
}

void UPCGBaseSubgraphSettings::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	if (UPCGGraphInterface* Subgraph = GetSubgraphInterface())
	{
		// We might have already connected in PostInitProperties
		// To be sure, remove it and re-add it.
		Subgraph->OnGraphChangedDelegate.RemoveAll(this);
		Subgraph->OnGraphChangedDelegate.AddUObject(this, &UPCGBaseSubgraphSettings::OnSubgraphChanged);
	}
#endif
}

void UPCGBaseSubgraphSettings::SetSubgraph(UPCGGraphInterface* InGraph)
{
#if WITH_EDITOR
	if (UPCGGraphInterface* Subgraph = GetSubgraphInterface())
	{
		Subgraph->OnGraphChangedDelegate.RemoveAll(this);
	}
#endif // WITH_EDITOR

	SetSubgraphInternal(InGraph);

#if WITH_EDITOR
	if (UPCGGraphInterface* Subgraph = GetSubgraphInterface())
	{
		Subgraph->OnGraphChangedDelegate.AddUObject(this, &UPCGBaseSubgraphSettings::OnSubgraphChanged);
	}
#endif // WITH_EDITOR
}

void UPCGBaseSubgraphSettings::BeginDestroy()
{
#if WITH_EDITOR
	if (UPCGGraphInterface* Subgraph = GetSubgraphInterface())
	{
		Subgraph->OnGraphChangedDelegate.RemoveAll(this);
	}
#endif

	Super::BeginDestroy();
}

#if WITH_EDITOR
void UPCGBaseSubgraphSettings::PreEditChange(FProperty* PropertyAboutToChange)
{
	if (PropertyAboutToChange && IsStructuralProperty(PropertyAboutToChange->GetFName()))
	{
		if (UPCGGraphInterface* Subgraph = GetSubgraphInterface())
		{
			Subgraph->OnGraphChangedDelegate.RemoveAll(this);
		}
	}

	Super::PreEditChange(PropertyAboutToChange);
}

void UPCGBaseSubgraphSettings::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property && IsStructuralProperty(PropertyChangedEvent.Property->GetFName()))
	{
		if (UPCGGraphInterface* Subgraph = GetSubgraphInterface())
		{
			Subgraph->OnGraphChangedDelegate.AddUObject(this, &UPCGBaseSubgraphSettings::OnSubgraphChanged);
		}
	}
}

void UPCGBaseSubgraphSettings::GetTrackedActorTags(FPCGTagToSettingsMap& OutTagToSettings, TArray<TObjectPtr<const UPCGGraph>>& VisitedGraphs) const
{
	if (UPCGGraph* Subgraph = GetSubgraph())
	{
		Subgraph->GetTrackedTagsToSettings(OutTagToSettings, VisitedGraphs);
	}
}

bool UPCGBaseSubgraphSettings::IsStructuralProperty(const FName& InPropertyName) const
{
	return InPropertyName == GET_MEMBER_NAME_CHECKED(UPCGSettingsInterface, bEnabled) || Super::IsStructuralProperty(InPropertyName);
}

void UPCGBaseSubgraphSettings::OnSubgraphChanged(UPCGGraphInterface* InGraph, EPCGChangeType ChangeType)
{
	if (InGraph == GetSubgraphInterface())
	{
		OnSettingsChangedDelegate.Broadcast(this, (ChangeType | EPCGChangeType::Settings));

		// Also rebuild the overrides
		InitializeCachedOverridableParams(/*bReset=*/true);
	}
}
#endif // WITH_EDITOR

TArray<FPCGPinProperties> UPCGBaseSubgraphSettings::InputPinProperties() const
{
	if (UPCGGraph* Subgraph = GetSubgraph())
	{
		TArray<FPCGPinProperties> InputPins = Subgraph->GetInputNode()->InputPinProperties();
		PCGSubgraphSettings::RemoveAdvancedModeOnConnectedPins(Subgraph, InputPins, /*bIsInput=*/true);
		return InputPins;
	}
	else
	{
		return Super::InputPinProperties();
	}
}

TArray<FPCGPinProperties> UPCGBaseSubgraphSettings::OutputPinProperties() const
{
	if (UPCGGraph* Subgraph = GetSubgraph())
	{
		TArray<FPCGPinProperties> OutputPins = Subgraph->GetOutputNode()->OutputPinProperties();
		PCGSubgraphSettings::RemoveAdvancedModeOnConnectedPins(Subgraph, OutputPins, /*bIsInput=*/false);
		return OutputPins;
	}
	else
	{
		return Super::OutputPinProperties();
	}
}

void UPCGBaseSubgraphSettings::FixingOverridableParamPropertyClass(FPCGSettingsOverridableParam& Param) const
{
	bool bFound = false;
	const UPCGGraph* PCGGraph = GetSubgraph();

	if (PCGGraph && !Param.PropertiesNames.IsEmpty())
	{
		if (const FInstancedPropertyBag* UserParameterStruct = PCGGraph->GetUserParametersStruct())
		{
			const UScriptStruct* ScriptStruct = UserParameterStruct->GetPropertyBagStruct();
			if (ScriptStruct && ScriptStruct->FindPropertyByName(Param.PropertiesNames[0]))
			{
				Param.PropertyClass = ScriptStruct;
				bFound = true;
			}
		}
	}

	if (!bFound)
	{
		Super::FixingOverridableParamPropertyClass(Param);
	}
}

#if WITH_EDITOR
TArray<FPCGSettingsOverridableParam> UPCGBaseSubgraphSettings::GatherOverridableParams() const
{
	TArray<FPCGSettingsOverridableParam> OverridableParams = Super::GatherOverridableParams();

	const UPCGGraph* PCGGraph = GetSubgraph();
	if (PCGGraph)
	{
		if (const FInstancedPropertyBag* UserParameterStruct = PCGGraph->GetUserParametersStruct())
		{
			if (const UScriptStruct* ScriptStruct = UserParameterStruct->GetPropertyBagStruct())
			{
				PCGSettingsHelpers::FPCGGetAllOverridableParamsConfig Config;
				Config.bExcludeSuperProperties = true;
				Config.ExcludePropertyFlags = CPF_DisableEditOnInstance;
				OverridableParams.Append(PCGSettingsHelpers::GetAllOverridableParams(ScriptStruct, Config));
			}
		}
	}

	return OverridableParams;
}
#endif // WITH_EDITOR

UPCGSubgraphSettings::UPCGSubgraphSettings(const FObjectInitializer& InObjectInitializer)
	: Super(InObjectInitializer)
{
	SubgraphInstance = InObjectInitializer.CreateDefaultSubobject<UPCGGraphInstance>(this, TEXT("PCGSubgraphInstance"));
}

UPCGNode* UPCGSubgraphSettings::CreateNode() const
{
	return NewObject<UPCGSubgraphNode>();
}

FName UPCGSubgraphSettings::AdditionalTaskName() const
{
	if (UPCGGraph* TargetSubgraph = GetSubgraph())
	{
		return TargetSubgraph->GetFName();
	}
	else
	{
		return TEXT("Invalid subgraph");
	}
}

void UPCGSubgraphSettings::SetSubgraphInternal(UPCGGraphInterface* InGraph)
{
	SubgraphInstance->SetGraph(InGraph);
}

void UPCGSubgraphSettings::PostLoad()
{
#if WITH_EDITOR
	if (Subgraph_DEPRECATED)
	{
		SubgraphInstance->SetGraph(Subgraph_DEPRECATED);
		Subgraph_DEPRECATED = nullptr;
	}
#endif // WITH_EDITOR

	Super::PostLoad();
}

#if WITH_EDITOR
UObject* UPCGSubgraphSettings::GetJumpTargetForDoubleClick() const
{
	// Note that there is a const_cast done behind the scenes in Cast.
	// And this behavior is already used in similar part of UE.
	return Cast<UObject>(GetSubgraph());
}

bool UPCGSubgraphSettings::IsStructuralProperty(const FName& InPropertyName) const
{
	// Force structural if name is none. We are probably in a undo/redo situation
	return (InPropertyName == NAME_None) || (InPropertyName == GET_MEMBER_NAME_CHECKED(UPCGSubgraphSettings, SubgraphInstance)) || Super::IsStructuralProperty(InPropertyName);
}
#endif

FPCGElementPtr UPCGSubgraphSettings::CreateElement() const
{
	return MakeShared<FPCGSubgraphElement>();
}

TObjectPtr<UPCGGraph> UPCGBaseSubgraphNode::GetSubgraph() const
{
	UPCGGraphInterface* SubgraphInterface = GetSubgraphInterface();
	return SubgraphInterface ? SubgraphInterface->GetGraph() : nullptr;
}

TObjectPtr<UPCGGraphInterface> UPCGSubgraphNode::GetSubgraphInterface() const
{
	TObjectPtr<UPCGSubgraphSettings> Settings = Cast<UPCGSubgraphSettings>(GetSettings());
	return Settings ? Settings->GetSubgraphInterface() : nullptr;
}

void* FPCGSubgraphContext::GetUnsafeExternalContainerForOverridableParam(const FPCGSettingsOverridableParam& InParam)
{
	const UPCGSubgraphSettings* Settings = GetInputSettings<UPCGSubgraphSettings>();
	check(Settings);
	const UPCGGraphInterface* Graph = Settings->GetSubgraphInterface();
	const FInstancedPropertyBag* UserParameters = Graph ? Graph->GetUserParametersStruct() : nullptr;

	if (UserParameters && !InParam.PropertiesNames.IsEmpty() && UserParameters->FindPropertyDescByName(InParam.PropertiesNames[0]) && GraphInstanceParametersOverride.IsValid())
	{
		return GraphInstanceParametersOverride.GetMutableMemory();
	}
	else
	{
		return nullptr;
	}
}

FPCGContext* FPCGSubgraphElement::Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node)
{
	FPCGSubgraphContext* Context = new FPCGSubgraphContext();
	Context->InputData = InputData;
	Context->SourceComponent = SourceComponent;
	Context->Node = Node;

	// Only duplicate the UserParameters if we have overriable params and we have at least one param pin connected.
	const UPCGSubgraphSettings* Settings = Context->GetInputSettings<UPCGSubgraphSettings>();
	check(Settings);
	const TArray<FPCGSettingsOverridableParam>& OverridableParams = Settings->OverridableParams();

	const UPCGGraphInterface* Graph = Settings->GetSubgraphInterface();
	const FInstancedPropertyBag* UserParameters = Graph ? Graph->GetUserParametersStruct() : nullptr;
	FConstStructView UserParametersView = UserParameters ? UserParameters->GetValue() : FConstStructView{};

	if (!OverridableParams.IsEmpty() && UserParametersView.IsValid())
	{
		bool bHasParamConnected = InputData.GetParamsWithDeprecation(Node) != nullptr;

		int32 Index = 0;
		while (!bHasParamConnected && Index < OverridableParams.Num())
		{
			bHasParamConnected |= !InputData.GetParamsByPin(OverridableParams[Index++].Label).IsEmpty();
		}

		if (bHasParamConnected)
		{
			Context->GraphInstanceParametersOverride = FInstancedStruct(UserParametersView);
		}
	}

	return Context;
}

bool FPCGSubgraphElement::ExecuteInternal(FPCGContext* InContext) const
{
	FPCGSubgraphContext* Context = static_cast<FPCGSubgraphContext*>(InContext);

	const UPCGSubgraphSettings* Settings = Context->GetInputSettings<UPCGSubgraphSettings>();
	check(Settings);

	const UPCGSubgraphNode* SubgraphNode = Cast<const UPCGSubgraphNode>(Context->Node);
	if (SubgraphNode && SubgraphNode->bDynamicGraph)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGSubgraphElement::Execute);

		if (!Context->bScheduledSubgraph)
		{
			UPCGGraph* Subgraph = Settings->GetSubgraph();
			UPCGSubsystem* Subsystem = Context->SourceComponent.IsValid() ? Context->SourceComponent->GetSubsystem() : nullptr;

			if (Subsystem && Subgraph)
			{
				// Dispatch graph to execute with the given information we have
				// using this node's task id as additional inputs
				FPCGTaskId SubgraphTaskId = Subsystem->ScheduleGraph(Subgraph, Context->SourceComponent.Get(), MakeShared<FPCGInputForwardingElement>(Context->InputData), {});

				Context->SubgraphTaskId = SubgraphTaskId;
				Context->bScheduledSubgraph = true;
				Context->bIsPaused = true;

				// add a trivial task after the output task that wakes up this task
				Subsystem->ScheduleGeneric([Context]() {
					// Wake up the current task
					Context->bIsPaused = false;
					return true;
					}, Context->SourceComponent.Get(), { Context->SubgraphTaskId });

				return false;
			}
			else
			{
				// Job cannot run; cancel
				Context->OutputData.bCancelExecution = true;
				return true;
			}
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
				ensure(Subsystem->GetOutputData(Context->SubgraphTaskId, Context->OutputData));
			}
			else
			{
				// Job cannot run, cancel
				Context->OutputData.bCancelExecution = true;
			}

			return true;
		}
	}
	else
	{
		// Don't forward overrides
		if (Settings->HasOverridableParams())
		{
			Context->OutputData.TaggedData.Reserve(Context->InputData.TaggedData.Num());
			const TArray<FPCGPinProperties> InputPins = Settings->DefaultInputPinProperties();

			for (const FPCGTaggedData& InputData : Context->InputData.TaggedData)
			{
				if (!InputData.Data)
				{
					continue;
				}

				// Discard params that don't have a pin on the subgraph input node
				if (!InputData.Data->IsA<UPCGParamData>() || Algo::FindByPredicate(InputPins, [Pin = InputData.Pin](const FPCGPinProperties& PinProperty) { return PinProperty.Label == Pin;}))
				{
					Context->OutputData.TaggedData.Add(InputData);
				}
			}
		}
		else
		{
			Context->OutputData = Context->InputData;
		}

		// Also create a new data containing information about the original subgraph and the parameters override
		// It is used mainly by the UseParameterGetElement to access the correct value.
		if (const UPCGGraphInterface* SubgraphInterface = Settings->GetSubgraphInterface())
		{
			UPCGUserParametersData* UserParamData = NewObject<UPCGUserParametersData>();
			UserParamData->OriginalGraph = SubgraphInterface;
			if (Context->GraphInstanceParametersOverride.IsValid())
			{
				UserParamData->UserParameters = std::move(Context->GraphInstanceParametersOverride);
			}

			FPCGTaggedData& TaggedData = Context->OutputData.TaggedData.Emplace_GetRef();
			TaggedData.Data = UserParamData;
			TaggedData.Tags.Add(PCGBaseSubgraphConstants::UserParameterTagData);
			// Mark this data pinless, since it is internal data, not meant to be shown in the graph editor.
			TaggedData.bPinlessData = true;
		}

		return true;
	}
}

FPCGInputForwardingElement::FPCGInputForwardingElement(const FPCGDataCollection& InputToForward)
	: Input(InputToForward)
{

}

bool FPCGInputForwardingElement::ExecuteInternal(FPCGContext* Context) const
{
	Context->OutputData = Input;
	return true;
}
