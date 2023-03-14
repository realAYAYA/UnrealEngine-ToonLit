// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGSubgraph.h"
#include "PCGGraph.h"
#include "PCGComponent.h"
#include "PCGSubsystem.h"

void UPCGBaseSubgraphSettings::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	if (UPCGGraph* Subgraph = GetSubgraph())
	{
		Subgraph->OnGraphChangedDelegate.AddUObject(this, &UPCGSubgraphSettings::OnSubgraphChanged);
	}
#endif
}

void UPCGBaseSubgraphSettings::BeginDestroy()
{
#if WITH_EDITOR
	if (UPCGGraph* Subgraph = GetSubgraph())
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
		if (UPCGGraph* Subgraph = GetSubgraph())
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
		if (UPCGGraph* Subgraph = GetSubgraph())
		{
			Subgraph->OnGraphChangedDelegate.AddUObject(this, &UPCGSubgraphSettings::OnSubgraphChanged);
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

void UPCGBaseSubgraphSettings::OnSubgraphChanged(UPCGGraph* InGraph, EPCGChangeType ChangeType)
{
	if (InGraph == GetSubgraph())
	{
		OnSettingsChangedDelegate.Broadcast(this, (ChangeType | EPCGChangeType::Settings));
	}
}
#endif // WITH_EDITOR

TArray<FPCGPinProperties> UPCGBaseSubgraphSettings::InputPinProperties() const
{
	if (UPCGGraph* Subgraph = GetSubgraph())
	{
		return Subgraph->GetInputNode()->InputPinProperties();
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
		return Subgraph->GetOutputNode()->OutputPinProperties();
	}
	else
	{
		return Super::OutputPinProperties();
	}
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

#if WITH_EDITOR
UObject* UPCGSubgraphSettings::GetJumpTargetForDoubleClick() const
{
	// Note that there is a const_cast done behind the scenes in Cast.
	// And this behavior is already used in similar part of UE.
	return Cast<UObject>(Subgraph);
}

bool UPCGSubgraphSettings::IsStructuralProperty(const FName& InPropertyName) const
{
	return (InPropertyName == GET_MEMBER_NAME_CHECKED(UPCGSubgraphSettings, Subgraph)) || Super::IsStructuralProperty(InPropertyName);
}
#endif

FPCGElementPtr UPCGSubgraphSettings::CreateElement() const
{
	return MakeShared<FPCGSubgraphElement>();
}

TObjectPtr<UPCGGraph> UPCGSubgraphNode::GetSubgraph() const
{
	TObjectPtr<UPCGSubgraphSettings> Settings = Cast<UPCGSubgraphSettings>(DefaultSettings);
	return Settings ? Settings->Subgraph : nullptr;
}

FPCGContext* FPCGSubgraphElement::Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node)
{
	FPCGSubgraphContext* Context = new FPCGSubgraphContext();
	Context->InputData = InputData;
	Context->SourceComponent = SourceComponent;
	Context->Node = Node;

	return Context;
}

bool FPCGSubgraphElement::ExecuteInternal(FPCGContext* InContext) const
{
	FPCGSubgraphContext* Context = static_cast<FPCGSubgraphContext*>(InContext);

	const UPCGSubgraphNode* SubgraphNode = Cast<const UPCGSubgraphNode>(Context->Node);
	if (SubgraphNode && SubgraphNode->bDynamicGraph)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGSubgraphElement::Execute);

		if (!Context->bScheduledSubgraph)
		{
			const UPCGSubgraphSettings* Settings = Context->GetInputSettings<UPCGSubgraphSettings>();
			check(Settings);
			UPCGGraph* Subgraph = Settings->Subgraph;

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
		Context->OutputData = Context->InputData;
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