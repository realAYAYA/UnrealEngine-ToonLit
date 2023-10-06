// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGSubgraph.h"

#include "PCGComponent.h"
#include "PCGPin.h"
#include "PCGSubsystem.h"
#include "Data/PCGUserParametersData.h"
#include "Graph/PCGStackContext.h"
#include "Helpers/PCGSettingsHelpers.h"

#include "Algo/Find.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGSubgraph)

#define LOCTEXT_NAMESPACE "PCGSubgraphElement"

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

	// Also, reconstruct overrides
	InitializeCachedOverridableParams(/*bReset=*/true);
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

void UPCGBaseSubgraphSettings::GetTrackedActorKeys(FPCGActorSelectionKeyToSettingsMap& OutKeysToSettings, TArray<TObjectPtr<const UPCGGraph>>& VisitedGraphs) const
{
	if (UPCGGraph* Subgraph = GetSubgraph())
	{
		Subgraph->GetTrackedActorKeysToSettings(OutKeysToSettings, VisitedGraphs);
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
void UPCGSubgraphSettings::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UPCGSubgraphSettings, SubgraphInstance))
	{
		// Also rebuild the overrides
		InitializeCachedOverridableParams(/*bReset=*/true);
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

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

bool UPCGSubgraphSettings::IsDynamicGraph() const
{
	UPCGNode* Node = Cast<UPCGNode>(GetOuter());

	if (!Node && OriginalSettings)
	{
		Node = Cast<UPCGNode>(OriginalSettings->GetOuter());

	}

	if (!Node)
	{
		return false;
	}

	const FName PropertyName = GET_MEMBER_NAME_CHECKED(UPCGSubgraphSettings, SubgraphOverride);

	const FPCGSettingsOverridableParam* Param = CachedOverridableParams.FindByPredicate([PropertyName](const FPCGSettingsOverridableParam& ParamToCheck)
	{
		return !ParamToCheck.PropertiesNames.IsEmpty() && ParamToCheck.PropertiesNames[0] == PropertyName;
	});

	if (Param)
	{
		if (const UPCGPin* Pin = Node->GetInputPin(Param->Label))
		{
			return Pin->IsConnected();
		}
	}

	return false;
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
	const UPCGBaseSubgraphSettings* Settings = Context->GetInputSettings<UPCGBaseSubgraphSettings>();
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

void FPCGSubgraphElement::PrepareSubgraphData(const UPCGSubgraphSettings* Settings, FPCGSubgraphContext* Context, const FPCGDataCollection& InputData, FPCGDataCollection& OutputData) const
{
	// Don't forward overrides
	if (Settings->HasOverridableParams())
	{
		OutputData.TaggedData.Reserve(InputData.TaggedData.Num());
		const TArray<FPCGPinProperties> InputPins = Settings->DefaultInputPinProperties();

		for (const FPCGTaggedData& Input : InputData.TaggedData)
		{
			if (!Input.Data)
			{
				continue;
			}

			// Discard params that don't have a pin on the subgraph input node
			if (!Input.Data->IsA<UPCGParamData>() || Algo::FindByPredicate(InputPins, [Pin = Input.Pin](const FPCGPinProperties& PinProperty) { return PinProperty.Label == Pin; }))
			{
				OutputData.TaggedData.Add(Input);
			}
		}
	}
	else
	{
		OutputData = InputData;
	}
}

void FPCGSubgraphElement::PrepareSubgraphUserParameters(const UPCGSubgraphSettings* Settings, FPCGSubgraphContext* Context, FPCGDataCollection& OutputData) const
{
	// Also create a new data containing information about the original subgraph and the parameters override
	// It is used mainly by the UserParameterGetElement to access the correct value.
	if (const UPCGGraphInterface* SubgraphInterface = Settings->GetSubgraphInterface())
	{
		UPCGUserParametersData* UserParamData = NewObject<UPCGUserParametersData>();
		UserParamData->OriginalGraph = SubgraphInterface;
		if (Context->GraphInstanceParametersOverride.IsValid())
		{
			UserParamData->UserParameters = std::move(Context->GraphInstanceParametersOverride);
		}

		FPCGTaggedData& TaggedData = OutputData.TaggedData.Emplace_GetRef();
		TaggedData.Data = UserParamData;
		TaggedData.Tags.Add(PCGBaseSubgraphConstants::UserParameterTagData);
		// Mark this data pinless, since it is internal data, not meant to be shown in the graph editor.
		TaggedData.bPinlessData = true;
	}
}

bool FPCGSubgraphElement::IsCacheable(const UPCGSettings* InSettings) const
{
	const UPCGSubgraphSettings* Settings = Cast<const UPCGSubgraphSettings>(InSettings);
	return (!Settings || !Settings->IsDynamicGraph());
}

bool FPCGSubgraphElement::ExecuteInternal(FPCGContext* InContext) const
{
	FPCGSubgraphContext* Context = static_cast<FPCGSubgraphContext*>(InContext);

	const UPCGSubgraphSettings* Settings = Context->GetInputSettings<UPCGSubgraphSettings>();
	check(Settings);

	// TODO: only prevents A->A recursion, not A->B->A or cycles in general
	if (Context->SourceComponent->GetGraph() == Settings->GetSubgraph())
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("FailedRecursiveSubgraph", "PCGGraph cannot include itself as a subgraph, subgraph will not be executed."));
		return true;
	}

	if (Settings->IsDynamicGraph())
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGSubgraphElement::Execute);

		if (!Context->bScheduledSubgraph)
		{
			if (Settings->SubgraphInstance)
			{
				// If OriginalSettings is null, then we ARE the original settings, and writing over the existing graph is incorrect (and potentially a race condition)
				check(Settings->OriginalSettings);
				Settings->SubgraphInstance->SetGraph(Settings->SubgraphOverride);
			}

			UPCGGraph* Subgraph = Settings->GetSubgraph();
			UPCGSubsystem* Subsystem = Context->SourceComponent.IsValid() ? Context->SourceComponent->GetSubsystem() : nullptr;

			if (Subsystem && Subgraph)
			{
				// Dispatch graph to execute with the given information we have
				// using this node's task id as additional inputs
				FPCGDataCollection PreSubgraphInputData;
				PrepareSubgraphUserParameters(Settings, Context, PreSubgraphInputData);

				FPCGDataCollection SubgraphInputData;
				PrepareSubgraphData(Settings, Context, Context->InputData, SubgraphInputData);

				// Prepare the invocation stack - which is the stack up to this node, and then this node
				FPCGStack InvocationStack = ensure(Context->Stack) ? *Context->Stack : FPCGStack();
				InvocationStack.GetStackFramesMutable().Emplace(Context->Node);

#if WITH_EDITOR
				Subgraph->OnGraphDynamicallyExecutedDelegate.Broadcast(Subgraph, Context->SourceComponent, InvocationStack);
#endif

				FPCGTaskId SubgraphTaskId = Subsystem->ScheduleGraph(Subgraph, Context->SourceComponent.Get(), MakeShared<FPCGInputForwardingElement>(PreSubgraphInputData), MakeShared<FPCGInputForwardingElement>(SubgraphInputData), {}, &InvocationStack);

				if (SubgraphTaskId != InvalidPCGTaskId)
				{
					Context->SubgraphTaskIds.Add(SubgraphTaskId);
					Context->bScheduledSubgraph = true;
					Context->bIsPaused = true;
					
					// add a trivial task after the output task that wakes up this task
					Subsystem->ScheduleGeneric([Context]() {
						// Wake up the current task
						Context->bIsPaused = false;
						return true;
					}, Context->SourceComponent.Get(), Context->SubgraphTaskIds);

					return false;
				}
				else
				{
					// Scheduling failed - early out
					Context->OutputData.bCancelExecution = true;
					return true;
				}
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
				if (Context->SubgraphTaskIds.Num() > 0)
				{
					// This element does not support multiple results/dispatches. Note that this would be easy to fix;
					// as we just can call the GetOutputData on a fresh data collection and merge it afterwards in the output data,
					// but this is not needed here so we will keep the full assignment & benefit from the crc as well.
					ensure(Context->SubgraphTaskIds.Num() == 1);
					ensure(Subsystem->GetOutputData(Context->SubgraphTaskIds[0], Context->OutputData));
				}
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
		// This node acts as both the pre-graph node and the input node so it should have both the user parameters & the actual inputs
		PrepareSubgraphData(Settings, Context, Context->InputData, Context->OutputData);
		PrepareSubgraphUserParameters(Settings, Context, Context->OutputData);
		return true;
	}
}

FPCGInputForwardingElement::FPCGInputForwardingElement(const FPCGDataCollection& InputToForward)
	: Input(InputToForward)
{
	// Root any previously unrooted data, so we don't need to make sure the caller is still around until this is executed
	for (const FPCGTaggedData& TaggedData : Input.TaggedData)
	{
		if (TaggedData.Data && !TaggedData.Data->IsRooted())
		{
			UPCGData* DataToRoot = const_cast<UPCGData*>(TaggedData.Data.Get());
			DataToRoot->AddToRoot();
			RootedData.Add(DataToRoot);
		}
	}
}

bool FPCGInputForwardingElement::ExecuteInternal(FPCGContext* Context) const
{
	// Remove from rootset during the execution; data will be re-rooted by the normal process in the graph executor
	for (UPCGData* DataToUnroot : RootedData)
	{
		ensure(DataToUnroot->IsRooted());
		DataToUnroot->RemoveFromRoot();
	}

	const_cast<FPCGInputForwardingElement*>(this)->RootedData.Reset();

	Context->OutputData = Input;
	return true;
}

#undef LOCTEXT_NAMESPACE