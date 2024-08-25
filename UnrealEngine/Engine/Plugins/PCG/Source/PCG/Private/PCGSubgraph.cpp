// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGSubgraph.h"

#include "PCGComponent.h"
#include "PCGPin.h"
#include "PCGSubsystem.h"
#include "Data/PCGUserParametersData.h"
#include "Graph/PCGStackContext.h"
#include "Helpers/PCGDynamicTrackingHelpers.h"
#include "Helpers/PCGSettingsHelpers.h"

#include "Algo/Find.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGSubgraph)

#define LOCTEXT_NAMESPACE "PCGSubgraphElement"

namespace PCGSubgraphSettings
{
	void RemoveAdvancedAndInvisibleOnConnectedPins(const UPCGGraph* Subgraph, TArray<FPCGPinProperties>& InOutPinProperties, const bool bIsInput)
	{
		const UPCGNode* SubgraphNode = bIsInput ? Subgraph->GetInputNode() : Subgraph->GetOutputNode();
		check(SubgraphNode);

		for (FPCGPinProperties& PinProperties : InOutPinProperties)
		{
			const UPCGPin* Pin = bIsInput ? SubgraphNode->GetOutputPin(PinProperties.Label) : SubgraphNode->GetInputPin(PinProperties.Label);
			if (ensure(Pin))
			{
				PinProperties.bInvisiblePin = false;

				if (Pin->IsConnected() && PinProperties.IsAdvancedPin())
				{
					PinProperties.SetNormalPin();
				}
			}
		}
	}
}

#if WITH_EDITOR
void UPCGBaseSubgraphSettings::SetupCallbacks()
{
	UPCGGraphInterface* Subgraph = GetSubgraphInterface();

	if (Subgraph && !Subgraph->OnGraphChangedDelegate.IsBoundToObject(this))
	{
		Subgraph->OnGraphChangedDelegate.AddUObject(this, &UPCGBaseSubgraphSettings::OnSubgraphChanged);
	}
}

void UPCGBaseSubgraphSettings::TeardownCallbacks()
{
	if (UPCGGraphInterface* Subgraph = GetSubgraphInterface())
	{
		Subgraph->OnGraphChangedDelegate.RemoveAll(this);
	}
}
#endif // WITH_EDITOR

UPCGGraph* UPCGBaseSubgraphSettings::GetSubgraph() const
{
	UPCGGraphInterface* SubgraphInterface = GetSubgraphInterface();
	return SubgraphInterface ? SubgraphInterface->GetGraph() : nullptr;
}

void UPCGBaseSubgraphSettings::PostInitProperties()
{
	Super::PostInitProperties();

#if WITH_EDITOR
	SetupCallbacks();
#endif
}

void UPCGBaseSubgraphSettings::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	SetupCallbacks();
#endif
}

void UPCGBaseSubgraphSettings::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);

#if WITH_EDITOR
	SetupCallbacks();
#endif
}

void UPCGBaseSubgraphSettings::PostEditImport()
{
	Super::PostEditImport();

#if WITH_EDITOR
	SetupCallbacks();
#endif
}

void UPCGBaseSubgraphSettings::SetSubgraph(UPCGGraphInterface* InGraph)
{
#if WITH_EDITOR
	TeardownCallbacks();
#endif // WITH_EDITOR

	SetSubgraphInternal(InGraph);

#if WITH_EDITOR
	SetupCallbacks();
#endif // WITH_EDITOR

	// Also, reconstruct overrides
	InitializeCachedOverridableParams(/*bReset=*/true);
}

void UPCGBaseSubgraphSettings::BeginDestroy()
{
#if WITH_EDITOR
	TeardownCallbacks();
#endif

	Super::BeginDestroy();
}

#if WITH_EDITOR
void UPCGBaseSubgraphSettings::PreEditUndo()
{
	Super::PreEditUndo();

	TeardownCallbacks();
}

void UPCGBaseSubgraphSettings::PostEditUndo()
{
	Super::PostEditUndo();

	SetupCallbacks();
}

void UPCGBaseSubgraphSettings::PreEditChange(FProperty* PropertyAboutToChange)
{
	if (PropertyAboutToChange && !!(GetChangeTypeForProperty(PropertyAboutToChange->GetFName()) & EPCGChangeType::Structural))
	{
		TeardownCallbacks();
	}

	Super::PreEditChange(PropertyAboutToChange);
}

void UPCGBaseSubgraphSettings::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property && !!(GetChangeTypeForProperty(PropertyChangedEvent.Property->GetFName()) & EPCGChangeType::Structural))
	{
		SetupCallbacks();
	}
}

void UPCGBaseSubgraphSettings::GetStaticTrackedKeys(FPCGSelectionKeyToSettingsMap& OutKeysToSettings, TArray<TObjectPtr<const UPCGGraph>>& VisitedGraphs) const
{
	if (UPCGGraph* Subgraph = GetSubgraph())
	{
		Subgraph->GetTrackedActorKeysToSettings(OutKeysToSettings, VisitedGraphs);
	}
}

EPCGChangeType UPCGBaseSubgraphSettings::GetChangeTypeForProperty(const FName& InPropertyName) const
{
	EPCGChangeType ChangeType = Super::GetChangeTypeForProperty(InPropertyName) | EPCGChangeType::Cosmetic;

	if (InPropertyName == GET_MEMBER_NAME_CHECKED(UPCGSettingsInterface, bEnabled))
	{
		ChangeType |= EPCGChangeType::Structural;
	}

	return ChangeType;
}

void UPCGBaseSubgraphSettings::OnSubgraphChanged(UPCGGraphInterface* InGraph, EPCGChangeType ChangeType)
{
	if (InGraph == GetSubgraphInterface())
	{
		// Only add settings if not cosmetic - we don't want to promote a cosmetic change to something deeper.
		if (ChangeType != EPCGChangeType::Cosmetic)
		{
			ChangeType |= EPCGChangeType::Settings;
		}

		OnSettingsChangedDelegate.Broadcast(this, ChangeType);

		// Also rebuild the overrides
		InitializeCachedOverridableParams(/*bReset=*/true);
	}
}
#endif // WITH_EDITOR

TArray<FPCGPinProperties> UPCGBaseSubgraphSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> InputPins;
	if (UPCGGraph* Subgraph = GetSubgraph())
	{
		InputPins = Subgraph->GetInputNode()->InputPinProperties();
		PCGSubgraphSettings::RemoveAdvancedAndInvisibleOnConnectedPins(Subgraph, InputPins, /*bIsInput=*/true);
	}
	else
	{
		InputPins = Super::InputPinProperties();
		// Considering this is likely a case where we'll have dynamic graph dispatch, don't make the default input pins required
		for (FPCGPinProperties& PinProperties : InputPins)
		{
			if (PinProperties.IsRequiredPin())
			{
				PinProperties.SetNormalPin();
			}
		}
	}

	return InputPins;
}

TArray<FPCGPinProperties> UPCGBaseSubgraphSettings::OutputPinProperties() const
{
	if (UPCGGraph* Subgraph = GetSubgraph())
	{
		TArray<FPCGPinProperties> OutputPins = Subgraph->GetOutputNode()->OutputPinProperties();
		PCGSubgraphSettings::RemoveAdvancedAndInvisibleOnConnectedPins(Subgraph, OutputPins, /*bIsInput=*/false);
		return OutputPins;
	}
	else
	{
		// Here we do not want the base class implementation as it forces Spatial but that might not be the case here
		TArray<FPCGPinProperties> OutputPins;
		OutputPins.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Any);
		return OutputPins;
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

FString UPCGSubgraphSettings::GetAdditionalTitleInformation() const
{
#if WITH_EDITOR
	if (IsPropertyOverriddenByPin(GET_MEMBER_NAME_CHECKED(UPCGSubgraphSettings, SubgraphOverride)))
	{
		// Subgraphs with the subgraph override pin connected should not display any asset path.
		return FString();
	}
#endif

	if (UPCGGraph* TargetSubgraph = GetSubgraph())
	{
		// Use the same transformation than in the palette view to add spaces between uppercase characters
		return FName::NameToDisplayString(TargetSubgraph->GetName(), /*bIsBool=*/false);
	}
	else
	{
		return LOCTEXT("NodeTitleExtendedInvalidSubgraph", "Missing Subgraph").ToString();
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

EPCGChangeType UPCGSubgraphSettings::GetChangeTypeForProperty(const FName& InPropertyName) const
{
	EPCGChangeType ChangeType = Super::GetChangeTypeForProperty(InPropertyName);

	// Force structural if name is none. We are probably in a undo/redo situation
	if ((InPropertyName == NAME_None) || (InPropertyName == GET_MEMBER_NAME_CHECKED(UPCGSubgraphSettings, SubgraphInstance)))
	{
		ChangeType |= EPCGChangeType::Structural;
	}

	return ChangeType;
}
#endif

FPCGElementPtr UPCGSubgraphSettings::CreateElement() const
{
	return MakeShared<FPCGSubgraphElement>();
}

UPCGGraphInterface* UPCGSubgraphSettings::GetSubgraphInterface() const
{
	// The only place when SubgraphOverride is not null, is when we execute a dynamic subgraph.
	// Everywhere else (for UI, parameter overrides, normal subgraph flow, etc...) we will use the SubgraphInstance.
	return SubgraphOverride ? SubgraphOverride.Get() : SubgraphInstance.Get(); 
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
		return !ParamToCheck.PropertiesNames.IsEmpty() && ParamToCheck.PropertiesNames.Last() == PropertyName;
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

void FPCGSubgraphContext::InitializeUserParametersStruct()
{
	// Only duplicate the UserParameters if we have overridable params and we have at least one param pin connected.
	GraphInstanceParametersOverride.Reset();

	// Will return the OG settings the first time this is called (when initializing the element/context)
	// and will contain the "hardcoded" graph in the subgraph node.
	// If subgraph is overridden, this be called a second time with the SubgraphInstance containing the updated overridden subgraph.
	const UPCGBaseSubgraphSettings* Settings = GetInputSettings<UPCGBaseSubgraphSettings>();
	check(Settings);

	const UClass* SettingsClass = Settings->GetClass();
	check(SettingsClass);

	const TArray<FPCGSettingsOverridableParam>& OverridableParams = Settings->OverridableParams();

	const UPCGGraphInterface* Graph = Settings->GetSubgraphInterface();
	const FInstancedPropertyBag* UserParameters = Graph ? Graph->GetUserParametersStruct() : nullptr;
	FConstStructView UserParametersView = UserParameters ? UserParameters->GetValue() : FConstStructView{};

	if (!OverridableParams.IsEmpty() && UserParametersView.IsValid())
	{
		bool bHasParamConnected = !InputData.GetParamsByPin(PCGPinConstants::DefaultParamsLabel).IsEmpty();

		int32 Index = 0;
		while (!bHasParamConnected && Index < OverridableParams.Num())
		{
			// Discard any override that is a property of the settings (we are looking for overrides for the graph instance)
			// We use the first property name, since it will be the one related to this settings properties.
			const FName PropertyName = OverridableParams[Index].PropertiesNames.IsEmpty() ? NAME_None : OverridableParams[Index].PropertiesNames[0];
			const FName Label = OverridableParams[Index].Label;
			++Index;

			if (PropertyName != NAME_None && SettingsClass->FindPropertyByName(PropertyName))
			{
				continue;
			}

			bHasParamConnected |= !InputData.GetParamsByPin(Label).IsEmpty();
		}

		if (bHasParamConnected)
		{
			GraphInstanceParametersOverride = FInstancedStruct(UserParametersView);
		}
	}
}

void FPCGSubgraphContext::UpdateOverridesWithOverriddenGraph()
{
	// We have a "catch-22" kind of problem here. When we initialize the subgraph element, we look for the graph "hardcoded"
	// in the settings to duplicate its user parameters and override it with the override pins. But Subgraph override is also coming from
	// the override pins, meaning that without adding some kind of "read order" on the override, we have to read them twice. It's less efficient
	// but makes things simpler to understand. We have a first override read to get the Subgraph Override, then if it is set, we have a second
	// read with the user parameters of the subgraph override.
	InitializeUserParametersStruct();
	if (GraphInstanceParametersOverride.IsValid())
	{
		OverrideSettings();
	}
}

FPCGContext* FPCGSubgraphElement::Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node)
{
	FPCGSubgraphContext* Context = new FPCGSubgraphContext();
	Context->InputData = InputData;
	Context->SourceComponent = SourceComponent;
	Context->Node = Node;

	Context->InitializeUserParametersStruct();

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

	// We also need to make sure we are not forwarding any UserParameterData from previous parents. So we remove all of them.
	OutputData.TaggedData.RemoveAllSwap([](const FPCGTaggedData& OutData) { return Cast<UPCGUserParametersData>(OutData.Data); });

	// Note for the future: Dynamic subgraphs are forwarding any data coming from the PreTask of their parent, so if you ever get data that should not be there,
	// you should probably do some filtering, like the UPCGUserParametersData.
}

void FPCGSubgraphElement::PrepareSubgraphUserParameters(const UPCGSubgraphSettings* Settings, FPCGSubgraphContext* Context, FPCGDataCollection& OutputData) const
{
	// Also create a new data containing information about the original subgraph and the parameters override
	// It is used mainly by the UserParameterGetElement to access the correct value.
	// By construction, there should be one and only one of this data. (Filtering of previous data is done in PrepareSubgraphData)
	if (const UPCGGraphInterface* SubgraphInterface = Settings->GetSubgraphInterface())
	{
		UPCGUserParametersData* UserParamData = NewObject<UPCGUserParametersData>();

		if (Context->GraphInstanceParametersOverride.IsValid())
		{
			UserParamData->UserParameters = std::move(Context->GraphInstanceParametersOverride);
		}
		else if (const FInstancedPropertyBag* InstancedPropertyBag = SubgraphInterface->GetUserParametersStruct())
		{
			// FIXME: Copy is done there.
			UserParamData->UserParameters = InstancedPropertyBag->GetValue();
		}
		else
		{
			// Do nothing, we still want to have a User Parameter Data to indicate we are in a subgraph context.
		}

		FPCGTaggedData& TaggedData = OutputData.TaggedData.Emplace_GetRef();
		TaggedData.Data = UserParamData;
		TaggedData.Tags.Add(PCGBaseSubgraphConstants::UserParameterTagData);
		// Mark this data pinless, since it is internal data, not meant to be shown in the graph editor.
		TaggedData.bPinlessData = true;
	}
}

bool FPCGSubgraphElement::IsPassthrough(const UPCGSettings* InSettings) const
{
	const UPCGSubgraphSettings* Settings = Cast<UPCGSubgraphSettings>(InSettings);
	return (!Settings || (Settings->bEnabled && Settings->GetSubgraph()));
}

bool FPCGSubgraphElement::ExecuteInternal(FPCGContext* InContext) const
{
	FPCGSubgraphContext* Context = static_cast<FPCGSubgraphContext*>(InContext);

	const UPCGSubgraphSettings* Settings = Context->GetInputSettings<UPCGSubgraphSettings>();
	check(Settings);

	const bool bIsDynamic = Settings->IsDynamicGraph();

	if (bIsDynamic && !Context->bScheduledSubgraph && Settings->SubgraphOverride)
	{
#if WITH_EDITOR
		FPCGDynamicTrackingHelper::AddSingleDynamicTrackingKey(Context, FPCGSelectionKey::CreateFromPath(Settings->SubgraphOverride), /*bIsCulled=*/false);
#endif // WITH_EDITOR

		Context->UpdateOverridesWithOverriddenGraph();
	}

	UPCGGraph* Subgraph = Settings->GetSubgraph();

	// Implementation note: recursivity test here must be consequential with the way the compilation has been done,
	// otherwise the other tasks will not behave as expected.
	// If the current graph is present in the subgraph downstream, then this must be a dynamic graph execution
	bool bIsRecursive = false;
	if (Subgraph)
	{
		if (Context->Stack)
		{
			bIsRecursive = Subgraph->Contains(Context->Stack->GetGraphForCurrentFrame());
		}
		else if(Context->SourceComponent.Get())
		{
			bIsRecursive = Subgraph->Contains(Context->SourceComponent->GetGraph());
		}
	}

	if (bIsDynamic || bIsRecursive)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGSubgraphElement::Execute);

		if (!Context->bScheduledSubgraph)
		{
			UPCGSubsystem* Subsystem = Context->SourceComponent.IsValid() ? Context->SourceComponent->GetSubsystem() : nullptr;

			if (Subsystem && Subgraph)
			{
				// Dispatch graph to execute with the given information we have
				// using this node's task id as additional inputs
				FPCGDataCollection PreSubgraphInputData;
				PrepareSubgraphUserParameters(Settings, Context, PreSubgraphInputData);

				FPCGDataCollection SubgraphInputData;
				PrepareSubgraphData(Settings, Context, Context->InputData, SubgraphInputData);

				// At this point, if we're in a recursive context and we have no input, we must terminate execution
				if (bIsRecursive && SubgraphInputData.TaggedData.IsEmpty())
				{
					return true;
				}

				// Prepare the invocation stack - which is the stack up to this node, and then this node
				FPCGStack InvocationStack = ensure(Context->Stack) ? *Context->Stack : FPCGStack();
				InvocationStack.GetStackFramesMutable().Emplace(Context->Node);

				// Higen is not allowed in dynamic subgraphs, entire subgraph is executed on the same grid as this subgraph node.
				FPCGTaskId SubgraphTaskId = Subsystem->ScheduleGraph(
					Subgraph,
					Context->SourceComponent.Get(),
					MakeShared<FPCGInputForwardingElement>(PreSubgraphInputData),
					MakeShared<FPCGInputForwardingElement>(SubgraphInputData),
					/*Dependencies=*/{},
					&InvocationStack,
					/*bAllowHierarchicalGeneration=*/false);

				if (SubgraphTaskId != InvalidPCGTaskId)
				{
					Context->SubgraphTaskIds.Add(SubgraphTaskId);
					Context->bScheduledSubgraph = true;
					Context->bIsPaused = true;
					
					// add a trivial task after the output task that wakes up this task
					Subsystem->ScheduleGeneric(
						[Context]() // Normal execution: Wake up the current task
						{
							Context->bIsPaused = false;
							return true;
						}, 
						[Context]() // On Abort: Wake up and cancel the execution
						{
							Context->bIsPaused = false;
							Context->OutputData.bCancelExecution = true;
							Context->SubgraphTaskIds.Reset();
							return true;
						},
						Context->SourceComponent.Get(), 
						Context->SubgraphTaskIds);

					return false;
				}
				else
				{
					// Scheduling failed - early out
					Context->OutputData.bCancelExecution = true;
					return true;
				}
			}
			else if (!Subgraph)
			{
				// Simple pass-through
				Context->OutputData.TaggedData = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
				return true;
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
					Subsystem->GetOutputData(Context->SubgraphTaskIds[0], Context->OutputData);
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
	// Root any previously unrooted data, needed here because the context does not exist yet and we need to ensure that the input is not garbage collected
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
	// Remove from rootset during the execution if we had previously done so. After execution, the graph cache will keep track of these references
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