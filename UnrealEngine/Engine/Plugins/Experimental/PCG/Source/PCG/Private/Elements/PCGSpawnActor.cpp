// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGSpawnActor.h"

#include "PCGComponent.h"
#include "PCGGraph.h"
#include "PCGManagedResource.h"
#include "PCGSubsystem.h"
#include "Data/PCGPointData.h"
#include "Data/PCGSpatialData.h"
#include "Graph/PCGStackContext.h"
#include "Grid/PCGPartitionActor.h"
#include "Helpers/PCGActorHelpers.h"
#include "Helpers/PCGHelpers.h"
#include "Helpers/PCGPointDataPartition.h"
#include "Metadata/Accessors/IPCGAttributeAccessor.h"
#include "Metadata/Accessors/PCGAttributeAccessorKeys.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/Engine.h"
#include "ISMPartition/ISMComponentDescriptor.h"
#include "UObject/Package.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGSpawnActor)

#define LOCTEXT_NAMESPACE "PCGSpawnActorElement"

static TAutoConsoleVariable<bool> CVarAllowActorReuse(
	TEXT("pcg.Actor.AllowReuse"),
	true,
	TEXT("Controls whether PCG spawned actors can be reused and skipped when re-executing"));

namespace PCGSpawnActorHelpers
{
	struct FActorSingleOverride
	{
		using ApplyOverrideFunction = TFunction<void(int32, const IPCGAttributeAccessorKeys&, IPCGAttributeAccessorKeys&)>;

		FActorSingleOverride(const FPCGAttributePropertySelector& InputSelector, const FString& OutputProperty, AActor* TemplateActor, const UPCGPointData* PointData)
		{
			ActorOverrideInputAccessor = PCGAttributeAccessorHelpers::CreateConstAccessor(PointData, InputSelector);
			ActorOverrideOutputAccessor = PCGAttributeAccessorHelpers::CreatePropertyAccessor(FName(OutputProperty), TemplateActor->GetClass());

			if (!ActorOverrideInputAccessor.IsValid() || !ActorOverrideOutputAccessor.IsValid())
			{
				UE_LOG(LogPCG, Warning, TEXT("ActorOverride from input %s or output %s is invalid or unsupported. Will be skipped."), *InputSelector.GetName().ToString(), *OutputProperty);
				return;
			}

			if (!PCG::Private::IsBroadcastable(ActorOverrideInputAccessor->GetUnderlyingType(), ActorOverrideOutputAccessor->GetUnderlyingType()))
			{
				UE_LOG(LogPCG, Warning, TEXT("ActorOverride cannot set input %s to output %s. Types are incompatibles. Will be skipped."), *InputSelector.GetName().ToString(), *OutputProperty);
				ActorOverrideInputAccessor.Reset();
				ActorOverrideOutputAccessor.Reset();
				return;
			}

			auto CreateGetterSetter = [this](auto Dummy)
			{
				using Type = decltype(Dummy);

				ActorOverrideFunction = [this](int32 Index, const IPCGAttributeAccessorKeys& InputKeys, IPCGAttributeAccessorKeys& OutputKey)
				{
					if (!IsValid())
					{
						return;
					}

					Type Value{};
					if (ActorOverrideInputAccessor->Get<Type>(Value, Index, InputKeys, EPCGAttributeAccessorFlags::AllowBroadcast))
					{
						ActorOverrideOutputAccessor->Set<Type>(Value, OutputKey);
					}
				};
			};

			PCGMetadataAttribute::CallbackWithRightType(ActorOverrideOutputAccessor->GetUnderlyingType(), CreateGetterSetter);
		}

		bool IsValid() const
		{
			return ActorOverrideInputAccessor.IsValid() && ActorOverrideOutputAccessor.IsValid() && ActorOverrideFunction;
		}

		void Apply(int32 Index, const IPCGAttributeAccessorKeys& InputKeys, IPCGAttributeAccessorKeys& OutputKey)
		{
			ActorOverrideFunction(Index, InputKeys, OutputKey);
		}

	private:
		TUniquePtr<const IPCGAttributeAccessor> ActorOverrideInputAccessor;
		TUniquePtr<IPCGAttributeAccessor> ActorOverrideOutputAccessor;
		ApplyOverrideFunction ActorOverrideFunction;
	};

	struct FActorOverrides
	{
		FActorOverrides(const TArray<FPCGActorPropertyOverride>& Overrides, AActor* TemplateActor, const UPCGPointData* PointData)
			: InputKeys(PointData->GetPoints())
			, OutputKey(TemplateActor)
		{
			ActorSingleOverrides.Reserve(Overrides.Num());

			for (int32 i = 0; i < Overrides.Num(); ++i)
			{
				FPCGAttributePropertyInputSelector InputSelector = Overrides[i].InputSource.CopyAndFixLast(PointData);
				const FString& OutputProperty = Overrides[i].PropertyTarget;

				ActorSingleOverrides.Emplace(InputSelector, OutputProperty, TemplateActor, PointData);
			}
		}

		void Apply(int32 Index)
		{
			for (FActorSingleOverride& ActorSingleOverride : ActorSingleOverrides)
			{
				if (ActorSingleOverride.IsValid())
				{
					ActorSingleOverride.Apply(Index, InputKeys, OutputKey);
				}
			}
		}

	private:
		FPCGAttributeAccessorKeysPoints InputKeys;
		FPCGAttributeAccessorKeysSingleObjectPtr<AActor> OutputKey;
		TArray<FActorSingleOverride> ActorSingleOverrides;
	};
}

class FPCGSpawnActorPartitionByAttribute : public FPCGPointDataPartitionBase<FPCGSpawnActorPartitionByAttribute, TSubclassOf<AActor>>
{
public:
	FPCGSpawnActorPartitionByAttribute(FName InSpawnAttribute)
		: FPCGPointDataPartitionBase<FPCGSpawnActorPartitionByAttribute, TSubclassOf<AActor>>()
		, SpawnAttribute(InSpawnAttribute)
	{
	}

	bool InitializeForPointData(const UPCGPointData* PointData)
	{
		if (!PointData || !PointData->ConstMetadata())
		{
			return false;
		}

		FPCGAttributePropertyInputSelector InputSource;
		InputSource.SetAttributeName(SpawnAttribute);
		InputSource = InputSource.CopyAndFixLast(PointData);
		SpawnAttributeAccessor = PCGAttributeAccessorHelpers::CreateConstAccessor(PointData, InputSource);
		SpawnAttributeKeys = PCGAttributeAccessorHelpers::CreateConstKeys(PointData, InputSource);

		return SpawnAttributeAccessor.IsValid() && SpawnAttributeKeys.IsValid();
	}

	void AddToPartitionData(FPCGPointDataPartitionBase::Element* SelectedElement, const UPCGPointData* ParentPointData, const FPCGPoint& Point)
	{
		check(SelectedElement);
		if (!SelectedElement->PartitionData)
		{
			SelectedElement->PartitionData = NewObject<UPCGPointData>();
			SelectedElement->PartitionData->InitializeFromData(ParentPointData);
		}

		check(SelectedElement->PartitionData);
		SelectedElement->PartitionData->GetMutablePoints().Add(Point);
	}

	FPCGPointDataPartitionBase::Element* SelectPoint(const FPCGPoint& Point, int32 PointIndex)
	{
		FString ActorPathString;
		if (!SpawnAttributeAccessor->Get<FString>(ActorPathString, PointIndex, *SpawnAttributeKeys))
		{
			return nullptr;
		}

		FSoftObjectPath ActorPath(ActorPathString);
		TSoftClassPtr<AActor> ActorClassSoftPtr(ActorPath);

		UClass* ActorClass = ActorClassSoftPtr.LoadSynchronous();

		if (!ActorClass)
		{
			UBlueprint* Blueprint = Cast<UBlueprint>(ActorPath.TryLoad());
			if (Blueprint)
			{
				ActorClass = Blueprint->GeneratedClass.Get();
			}
		}
		
		if (ActorClass && ActorClass->IsChildOf<AActor>())
		{
			return &ElementMap.FindOrAdd(ActorClass);
		}

		return nullptr;
	}

	// Disables time-slicing altogether because the code isn't setup for this yet
	int32 TimeSlicingCheckFrequency() const { return std::numeric_limits<int>::max(); }

public:
	TUniquePtr<const IPCGAttributeAccessor> SpawnAttributeAccessor;
	TUniquePtr<const IPCGAttributeAccessorKeys> SpawnAttributeKeys;
	FName SpawnAttribute = NAME_None;
};

UPCGNode* UPCGSpawnActorSettings::CreateNode() const
{
	return NewObject<UPCGSpawnActorNode>();
}

FPCGElementPtr UPCGSpawnActorSettings::CreateElement() const
{
	return MakeShared<FPCGSpawnActorElement>();
}

UPCGGraphInterface* UPCGSpawnActorSettings::GetGraphInterfaceFromActorSubclass(TSubclassOf<AActor> InTemplateActorClass)
{
	if (!InTemplateActorClass || InTemplateActorClass->HasAnyClassFlags(CLASS_Abstract))
	{
		return nullptr;
	}

	UPCGGraphInterface* Result = nullptr;

	AActor::ForEachComponentOfActorClassDefault<UPCGComponent>(InTemplateActorClass, [&](const UPCGComponent* PCGComponent)
	{
		// If there is no graph, there is no graph instance
		if (PCGComponent->GetGraph() && PCGComponent->bActivated)
		{
			Result = PCGComponent->GetGraphInstance();
			return false;
		}
		
		return true;
	});

	return Result;
}

UPCGGraphInterface* UPCGSpawnActorSettings::GetSubgraphInterface() const
{
	return GetGraphInterfaceFromActorSubclass(TemplateActorClass);
}

void UPCGSpawnActorSettings::BeginDestroy()
{
#if WITH_EDITOR
	TeardownBlueprintEvent();
#endif // WITH_EDITOR

	Super::BeginDestroy();
}

#if WITH_EDITOR
void UPCGSpawnActorSettings::SetupBlueprintEvent()
{
	if (UBlueprintGeneratedClass* BlueprintClass = Cast<UBlueprintGeneratedClass>(TemplateActorClass))
	{
		if (UBlueprint* Blueprint = Cast<UBlueprint>(BlueprintClass->ClassGeneratedBy))
		{
			Blueprint->OnChanged().AddUObject(this, &UPCGSpawnActorSettings::OnBlueprintChanged);
		}
	}
}

void UPCGSpawnActorSettings::TeardownBlueprintEvent()
{
	if (UBlueprintGeneratedClass* BlueprintClass = Cast<UBlueprintGeneratedClass>(TemplateActorClass))
	{
		if (UBlueprint* Blueprint = Cast<UBlueprint>(BlueprintClass->ClassGeneratedBy))
		{
			Blueprint->OnChanged().RemoveAll(this);
		}
	}
}
#endif

void UPCGSpawnActorSettings::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	// Apply data deprecation - deprecated in UE 5.3
	if (bGenerationTrigger_DEPRECATED != EPCGSpawnActorGenerationTrigger::Default)
	{
		GenerationTrigger = bGenerationTrigger_DEPRECATED;
		bGenerationTrigger_DEPRECATED = EPCGSpawnActorGenerationTrigger::Default;
	}

	// Since the template actor editing is set to false by default, this needs to be corrected on post-load for proper deprecation
	if (TemplateActor)
	{
		bAllowTemplateActorEditing = true;
	}

	SetupBlueprintEvent();

	if (TemplateActorClass)
	{
		if (TemplateActor)
		{
			TemplateActor->ConditionalPostLoad();
		}
	}

	RefreshTemplateActor();
#endif // WITH_EDITOR
}

#if WITH_EDITOR
bool UPCGSpawnActorSettings::IsStructuralProperty(const FName& InPropertyName) const
{
	return InPropertyName == GET_MEMBER_NAME_CHECKED(UPCGSpawnActorSettings, TemplateActorClass) || 
		InPropertyName == GET_MEMBER_NAME_CHECKED(UPCGSpawnActorSettings, Option) ||
		InPropertyName == GET_MEMBER_NAME_CHECKED(UPCGSpawnActorSettings, bSpawnByAttribute) ||
		Super::IsStructuralProperty(InPropertyName);
}
#endif // WITH_EDITOR

TObjectPtr<UPCGGraphInterface> UPCGSpawnActorNode::GetSubgraphInterface() const
{
	TObjectPtr<UPCGSpawnActorSettings> Settings = Cast<UPCGSpawnActorSettings>(GetSettings());
	return (Settings && Settings->Option != EPCGSpawnActorOption::NoMerging) ? Settings->GetSubgraphInterface() : nullptr;
}

#if WITH_EDITOR
void UPCGSpawnActorSettings::PreEditChange(FProperty* PropertyAboutToChange)
{
	Super::PreEditChange(PropertyAboutToChange);

	if (PropertyAboutToChange && PropertyAboutToChange->GetFName() == GET_MEMBER_NAME_CHECKED(UPCGSpawnActorSettings, TemplateActorClass))
	{
		TeardownBlueprintEvent();
	}
}

void UPCGSpawnActorSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property)
	{
		const FName& PropertyName = PropertyChangedEvent.Property->GetFName();

		if (PropertyName == GET_MEMBER_NAME_CHECKED(UPCGSpawnActorSettings, TemplateActorClass))
		{
			SetupBlueprintEvent();
			RefreshTemplateActor();
		}
		else if (PropertyName == GET_MEMBER_NAME_CHECKED(UPCGSpawnActorSettings, bAllowTemplateActorEditing))
		{
			RefreshTemplateActor();
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UPCGSpawnActorSettings::PreEditUndo()
{
	TeardownBlueprintEvent();

	Super::PreEditUndo();
}

void UPCGSpawnActorSettings::PostEditUndo()
{
	Super::PostEditUndo();

	SetupBlueprintEvent();
	RefreshTemplateActor();
}

void UPCGSpawnActorSettings::OnBlueprintChanged(UBlueprint* InBlueprint)
{
	RefreshTemplateActor();
	DirtyCache();
	OnSettingsChangedDelegate.Broadcast(this, EPCGChangeType::Settings);
}
#endif // WITH_EDITOR

void UPCGSpawnActorSettings::RefreshTemplateActor()
{
	// Implementation note: this is similar to the child actor component implementation
	if (TemplateActorClass && bAllowTemplateActorEditing)
	{
		const bool bCreateNewTemplateActor = (!TemplateActor || TemplateActor->GetClass() != TemplateActorClass);

		if (bCreateNewTemplateActor)
		{
			AActor* NewTemplateActor = NewObject<AActor>(GetTransientPackage(), TemplateActorClass, NAME_None, RF_ArchetypeObject | RF_Transactional | RF_Public);

			if (TemplateActor)
			{
				UEngine::FCopyPropertiesForUnrelatedObjectsParams Options;
				Options.bNotifyObjectReplacement = true;
				UEngine::CopyPropertiesForUnrelatedObjects(TemplateActor, NewTemplateActor, Options);

				TemplateActor->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors);

				TMap<UObject*, UObject*> OldToNew;
				OldToNew.Emplace(TemplateActor, NewTemplateActor);
				GEngine->NotifyToolsOfObjectReplacement(OldToNew);

				TemplateActor->MarkAsGarbage();
			}

			TemplateActor = NewTemplateActor;

			// Record initial object state in case we're in a transaction context.
			TemplateActor->Modify();

			// Outer to this object
			TemplateActor->Rename(nullptr, this, REN_DoNotDirty | REN_DontCreateRedirectors | REN_ForceNoResetLoaders);
		}
	}
	else
	{
		if (TemplateActor)
		{
			TemplateActor->MarkAsGarbage();
		}

		TemplateActor = nullptr;
	}
}

bool FPCGSpawnActorElement::ExecuteInternal(FPCGContext* InContext) const
{
	FPCGSubgraphContext* Context = static_cast<FPCGSubgraphContext*>(InContext);

	const UPCGSpawnActorSettings* Settings = Context->GetInputSettings<UPCGSpawnActorSettings>();
	check(Settings);

	if (!Context->bScheduledSubgraph)
	{
		return SpawnAndPrepareSubgraphs(Context, Settings);
	}
	else if (Context->bIsPaused)
	{
		// Should not happen once we skip it in the graph executor
		return false;
	}
	else
	{
		// TODO: Currently, we don't gather results from subgraphs, but we could (in a single pin).
		return true;
	}
}

bool FPCGSpawnActorElement::SpawnAndPrepareSubgraphs(FPCGSubgraphContext* Context, const UPCGSpawnActorSettings* Settings) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCSpawnActorElement::Execute);

	// Early out
	if (!Settings->bSpawnByAttribute)
	{
		if (!Settings->TemplateActorClass || Settings->TemplateActorClass->HasAnyClassFlags(CLASS_Abstract))
		{
			const FText ClassName = Settings->TemplateActorClass ? FText::FromString(Settings->TemplateActorClass->GetFName().ToString()) : FText::FromName(NAME_None);
			PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("InvalidTemplateActorClass", "Invalid template actor class '{0}'"), ClassName));
			return true;
		}

		if (!ensure(!Settings->TemplateActor || Settings->TemplateActor->IsA(Settings->TemplateActorClass)))
		{
			return true;
		}
	}

	UPCGSubsystem* Subsystem = Context->SourceComponent.IsValid() ? Context->SourceComponent->GetSubsystem() : nullptr;

	// Check if we can reuse existing resources - note that this is done on a per-settings basis when collapsed,
	// Otherwise we'll check against merged crc 
	bool bFullySkippedDueToReuse = false;

	if (CVarAllowActorReuse.GetValueOnAnyThread())
	{
		// Compute CRC if it has not been computed (it likely isn't, but this is to futureproof this)
		if (!Context->DependenciesCrc.IsValid())
		{
			GetDependenciesCrc(Context->InputData, Settings, Context->SourceComponent.Get(), Context->DependenciesCrc);
		}

		if (Context->DependenciesCrc.IsValid())
		{
			if (Settings->Option == EPCGSpawnActorOption::CollapseActors)
			{
				TArray<UPCGManagedISMComponent*> MISMCs;
				Context->SourceComponent->ForEachManagedResource([&MISMCs, &Context](UPCGManagedResource* InResource)
				{
					if (UPCGManagedISMComponent* Resource = Cast<UPCGManagedISMComponent>(InResource))
					{
						if (Resource->GetCrc().IsValid() && Resource->GetCrc() == Context->DependenciesCrc)
						{
							MISMCs.Add(Resource);
						}
					}
				});

				for (UPCGManagedISMComponent* MISMC : MISMCs)
				{
					MISMC->MarkAsReused();
				}

				if (!MISMCs.IsEmpty())
				{
					bFullySkippedDueToReuse = true;
				}
			}
		}
	}

	// Pass-through exclusions & settings
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

#if WITH_EDITOR
	const bool bGenerateOutputsWithActorReference = (Settings->Option != EPCGSpawnActorOption::CollapseActors);
#else
	const bool bGenerateOutputsWithActorReference = (Settings->Option != EPCGSpawnActorOption::CollapseActors) && Context->Node && Context->Node->IsOutputPinConnected(PCGPinConstants::DefaultOutputLabel);
#endif

	const bool bHasAuthority = !Context->SourceComponent.IsValid() || (Context->SourceComponent->GetOwner() && Context->SourceComponent->GetOwner()->HasAuthority());

	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputs();
	for (const FPCGTaggedData& Input : Inputs)
	{
		const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Input.Data);

		if (!SpatialData)
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("InvalidInputData", "Invalid input data"));
			continue;
		}

		AActor* TargetActor = Settings->RootActor.Get() ? Settings->RootActor.Get() : Context->GetTargetActor(nullptr);

		if (!TargetActor)
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("InvalidTargetActor", "Invalid target actor. Ensure TargetActor member is initialized when creating SpatialData."));
			continue;
		}

		// First, create target instance transforms
		const UPCGPointData* PointData = SpatialData->ToPointData(Context);

		if (!PointData)
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("NoPointDataInInput", "Unable to get point data from input"));
			continue;
		}

		const TArray<FPCGPoint>& Points = PointData->GetPoints();

		if (Points.IsEmpty())
		{
			PCGE_LOG(Verbose, LogOnly, LOCTEXT("SkippedNoPoints", "Skipped - no points"));
			continue;
		}

		auto SpawnOrCollapse = [this, bHasAuthority, &Context, &TargetActor, &Settings](TSubclassOf<AActor> TemplateActorClass, AActor* TemplateActor, FPCGTaggedData& Output, const UPCGPointData* PointData, UPCGPointData* OutPointData)
		{
			const bool bSpawnedActorsRequireAuthority = (TemplateActor ? TemplateActor->GetIsReplicated() : CastChecked<AActor>(TemplateActorClass->GetDefaultObject())->GetIsReplicated());

			if (Settings->Option == EPCGSpawnActorOption::CollapseActors)
			{
				CollapseIntoTargetActor(Context, TargetActor, TemplateActorClass, PointData);
			}
			else if (bHasAuthority || !bSpawnedActorsRequireAuthority)
			{
				SpawnActors(Context, TargetActor, TemplateActorClass, TemplateActor, Output, PointData, OutPointData);
			}
		};

		UPCGPointData* OutPointData = nullptr;
		if (bGenerateOutputsWithActorReference)
		{
			OutPointData = NewObject<UPCGPointData>();
			OutPointData->InitializeFromData(PointData);
		}

		FPCGTaggedData Output = Input;

		if (Settings->bSpawnByAttribute && (!bFullySkippedDueToReuse || bGenerateOutputsWithActorReference))
		{
			FPCGSpawnActorPartitionByAttribute Selector(Settings->SpawnAttribute);
			int32 CurrentPointIndex = 0;

			// Selection is still needed if are fully skipped in order to write to the OutPointData.
			Selector.SelectPoints(*Context, PointData, CurrentPointIndex, OutPointData);

			if (!bFullySkippedDueToReuse)
			{
				for (auto& Element : Selector.ElementMap)
				{
					FPCGTaggedData PartialInput = Input;
					PartialInput.Data = Element.Value.PartitionData;

					SpawnOrCollapse(Element.Key, nullptr, PartialInput, Element.Value.PartitionData, OutPointData);

					// Exception case here: if we've spawned actors but are merging the PCG inputs,
					// normally this node is taken as a subgraph node (e.g. no need to do anything more than forwarding the inputs)
					// However, if we`re in the spawn by attribute case, we need to dispatch it here.
					if (Settings->Option != EPCGSpawnActorOption::NoMerging && Subsystem)
					{
						// TODO: maybe consider a version that would support multi PCG
						if (UPCGGraphInterface* GraphInterface = UPCGSpawnActorSettings::GetGraphInterfaceFromActorSubclass(Element.Key))
						{
							FPCGDataCollection SubgraphInputData;
							SubgraphInputData.TaggedData.Add(PartialInput);

							// Prepare the invocation stack - which is the stack up to this node, and then this node, then a loop index
							FPCGStack InvocationStack = ensure(Context->Stack) ? *Context->Stack : FPCGStack();

							UPCGGraph* Graph = GraphInterface->GetGraph();

#if WITH_EDITOR
							Graph->OnGraphDynamicallyExecutedDelegate.Broadcast(Graph, Context->SourceComponent, InvocationStack);
#endif

							FPCGTaskId SubgraphTaskId = Subsystem->ScheduleGraph(Graph,
								Context->SourceComponent.Get(),
								MakeShared<FPCGTrivialElement>(),// TODO: prepare user parameters like in subgraph/loop
								MakeShared<FPCGInputForwardingElement>(SubgraphInputData),
								{},
								&InvocationStack);

							if (SubgraphTaskId != InvalidPCGTaskId)
							{
								Context->SubgraphTaskIds.Add(SubgraphTaskId);
							}
						}
					}
				}
			}
		}
		else if(!bFullySkippedDueToReuse)
		{
			// Spawn actors/populate ISM
			FPCGTaggedData InputCopy = Input;
			SpawnOrCollapse(Settings->TemplateActorClass, Settings->TemplateActor, InputCopy, PointData, OutPointData);
		}

		// Update the data in the output to the final data gathered
		if (OutPointData)
		{
			Output.Data = OutPointData;
		}

		// Finally, pass through the input, in all cases: 
		// - if it's not merged, will be the input points directly
		// - if it's merged but there is no subgraph, will be the input points directly
		// - if it's merged and there is a subgraph, we'd need to pass the data for it to be given to the subgraph
		Outputs.Add(Output);
	}

	// If we've dispatched dynamic execution, we should queue a task here to wait for those
	if (!Context->SubgraphTaskIds.IsEmpty())
	{
		Context->bScheduledSubgraph = true;
		Context->bIsPaused = true;

		Subsystem->ScheduleGeneric([Context]() {
			// Wake up the current task
			Context->bIsPaused = false;
			return true;
		}, Context->SourceComponent.Get(), Context->SubgraphTaskIds);

		return false;
	}
	else
	{
		return true;
	}
}

void FPCGSpawnActorElement::CollapseIntoTargetActor(FPCGSubgraphContext* Context, AActor* TargetActor, TSubclassOf<AActor> TemplateActorClass, const UPCGPointData* PointData) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGSpawnActorElement::ExecuteInternal::CollapseActors);
	check(Context && TargetActor && PointData);

	const TArray<FPCGPoint>& Points = PointData->GetPoints();
	if (Points.IsEmpty())
	{
		return;
	}

	const UPCGSpawnActorSettings* Settings = Context->GetInputSettings<UPCGSpawnActorSettings>();
	check(Settings);

	TMap<FPCGISMCBuilderParameters, TArray<FTransform>> MeshDescriptorTransforms;

	AActor::ForEachComponentOfActorClassDefault<UStaticMeshComponent>(TemplateActorClass, [&MeshDescriptorTransforms](const UStaticMeshComponent* StaticMeshComponent)
	{
		FPCGISMCBuilderParameters Params;
		Params.Descriptor.InitFrom(StaticMeshComponent);
		// TODO: No custom data float support?

		TArray<FTransform>& Transforms = MeshDescriptorTransforms.FindOrAdd(Params);

		if (const UInstancedStaticMeshComponent* InstancedStaticMeshComponent = Cast<UInstancedStaticMeshComponent>(StaticMeshComponent))
		{
			const int32 NumInstances = InstancedStaticMeshComponent->GetInstanceCount();
			Transforms.Reserve(Transforms.Num() + NumInstances);

			for (int32 InstanceIndex = 0; InstanceIndex < NumInstances; InstanceIndex++)
			{
				FTransform InstanceTransform;
				if (InstancedStaticMeshComponent->GetInstanceTransform(InstanceIndex, InstanceTransform))
				{
					Transforms.Add(InstanceTransform);
				}
			}
		}
		else
		{
			Transforms.Add(StaticMeshComponent->GetRelativeTransform());
		}

		return true;
	});

	for (const TPair<FPCGISMCBuilderParameters, TArray<FTransform>>& ISMCBuilderTransforms : MeshDescriptorTransforms)
	{
		const FPCGISMCBuilderParameters& ISMCParams = ISMCBuilderTransforms.Key;

		UPCGManagedISMComponent* MISMC = UPCGActorHelpers::GetOrCreateManagedISMC(TargetActor, Context->SourceComponent.Get(), Settings->UID, ISMCParams);
		if (!MISMC)
		{
			continue;
		}

		MISMC->SetCrc(Context->DependenciesCrc);

		UInstancedStaticMeshComponent* ISMC = MISMC->GetComponent();
		check(ISMC);

		const TArray<FTransform>& ISMCTransforms = ISMCBuilderTransforms.Value;

		TArray<FTransform> Transforms;
		Transforms.Reserve(Points.Num() * ISMCTransforms.Num());
		for (int32 PointIndex = 0; PointIndex < Points.Num(); PointIndex++)
		{
			const FPCGPoint& Point = Points[PointIndex];
			for (int32 TransformIndex = 0; TransformIndex < ISMCTransforms.Num(); TransformIndex++)
			{
				const FTransform& Transform = ISMCTransforms[TransformIndex];
				Transforms.Add(Transform * Point.Transform);
			}
		}

		// Fill in custom data (?)
		ISMC->AddInstances(Transforms, false, true);
		ISMC->UpdateBounds();

		PCGE_LOG(Verbose, LogOnly, FText::Format(LOCTEXT("InstanceCreationInfo", "Added {0} instances of mesh '{1}' to ISMC '{2}' on actor '{3}'"),
			Transforms.Num(), FText::FromString(ISMC->GetStaticMesh().GetName()), FText::FromString(ISMC->GetName()), FText::FromString(TargetActor->GetActorNameOrLabel())));
	}
}

void FPCGSpawnActorElement::SpawnActors(FPCGSubgraphContext* Context, AActor* TargetActor, TSubclassOf<AActor> InTemplateActorClass, AActor* InTemplateActor, FPCGTaggedData& Output, const UPCGPointData* PointData, UPCGPointData* OutPointData) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGSpawnActorElement::ExecuteInternal::SpawnActors);
	check(Context && TargetActor && PointData);

	const TArray<FPCGPoint>& Points = PointData->GetPoints();
	if (Points.IsEmpty())
	{
		return;
	}

	int32 OutPointOffset = 0;
	FPCGMetadataAttribute<FString>* ActorReferenceAttribute = nullptr;

	if (OutPointData)
	{
		OutPointOffset = OutPointData->GetMutablePoints().Num();
		OutPointData->GetMutablePoints().Append(Points);
		ActorReferenceAttribute = OutPointData->MutableMetadata()->FindOrCreateAttribute<FString>(PCGPointDataConstants::ActorReferenceAttribute, FString(), /*bAllowsInterpolation=*/false, /*bOverrideParent=*/false, /*bOverwriteIfTypeMismatch=*/false);
	}

	const UPCGSpawnActorSettings* Settings = Context->GetInputSettings<UPCGSpawnActorSettings>();
	check(Settings && Settings->Option != EPCGSpawnActorOption::CollapseActors);

	TArray<UFunction*> PostSpawnFunctions;
	for (FName PostSpawnFunctionName : Settings->PostSpawnFunctionNames)
	{
		if (PostSpawnFunctionName == NAME_None)
		{
			continue;
		}

		if (UFunction* PostSpawnFunction = InTemplateActorClass->FindFunctionByName(PostSpawnFunctionName))
		{
			if (PostSpawnFunction->NumParms != 0)
			{
				PCGE_LOG(Warning, GraphAndLog, FText::Format(LOCTEXT("ParametersMissing", "PostSpawnFunction '{0}' requires parameters. We only support parameter-less functions. Call will be skipped."), FText::FromName(PostSpawnFunctionName)));
			}
			else
			{
				PostSpawnFunctions.Add(PostSpawnFunction);
			}
		}
		else
		{
			PCGE_LOG(Warning, GraphAndLog, FText::Format(LOCTEXT("FunctionNotFound", "PostSpawnFunction '{0}' was not found in class '{1}'"), FText::FromName(PostSpawnFunctionName), FText::FromName(InTemplateActorClass->GetFName())));
		}
	}

	const bool bForceDisableActorParsing = (Settings->bForceDisableActorParsing);

	AActor* TemplateActor = nullptr;
	if (InTemplateActor)
	{
		if (Settings->ActorOverrides.IsEmpty())
		{
			TemplateActor = InTemplateActor;
		}
		else
		{
			TemplateActor = DuplicateObject(InTemplateActor, GetTransientPackage());
		}
	}
	else
	{
		if (Settings->ActorOverrides.IsEmpty())
		{
			TemplateActor = Cast<AActor>(InTemplateActorClass->GetDefaultObject());
		}
		else
		{
			TemplateActor = NewObject<AActor>(GetTransientPackage(), InTemplateActorClass, NAME_None, RF_ArchetypeObject);
		}
	}

	check(TemplateActor);

	PCGSpawnActorHelpers::FActorOverrides ActorOverrides(Settings->ActorOverrides, TemplateActor, PointData);

	FActorSpawnParameters SpawnParams;
	SpawnParams.Template = TemplateActor;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	if (PCGHelpers::IsRuntimeOrPIE())
	{
		SpawnParams.ObjectFlags |= RF_Transient;
	}

	const bool bForceCallGenerate = (Settings->GenerationTrigger == EPCGSpawnActorGenerationTrigger::ForceGenerate);
#if WITH_EDITOR
	const bool bOnLoadCallGenerate = (Settings->GenerationTrigger == EPCGSpawnActorGenerationTrigger::Default);
#else
	const bool bOnLoadCallGenerate = (Settings->GenerationTrigger == EPCGSpawnActorGenerationTrigger::Default ||
		Settings->GenerationTrigger == EPCGSpawnActorGenerationTrigger::DoNotGenerateInEditor);
#endif
	UPCGSubsystem* Subsystem = (Context->SourceComponent.Get() ? Context->SourceComponent->GetSubsystem() : nullptr);

	// Try to reuse actors if the are preexisting
	TArray<UPCGManagedActors*> ReusedManagedActorsResources;
	FPCGCrc InputDependenciesCrc;
	if (CVarAllowActorReuse.GetValueOnAnyThread())
	{
		FPCGDataCollection SingleInputCollection;
		SingleInputCollection.TaggedData.Add(Output);
		// TODO: review this, it might make more sense to do a full crc here
		SingleInputCollection.Crc = SingleInputCollection.ComputeCrc(/*bFullDataCrc=*/false);

		GetDependenciesCrc(SingleInputCollection, Settings, Context->SourceComponent.Get(), InputDependenciesCrc);

		if (InputDependenciesCrc.IsValid())
		{
			Context->SourceComponent->ForEachManagedResource([&ReusedManagedActorsResources, &InputDependenciesCrc, &Context](UPCGManagedResource* InResource)
			{
				if (UPCGManagedActors* Resource = Cast<UPCGManagedActors>(InResource))
				{
					if (Resource->GetCrc().IsValid() && Resource->GetCrc() == InputDependenciesCrc)
					{
						ReusedManagedActorsResources.Add(Resource);
					}
				}
			});
		}
	}

	TArray<AActor*> ProcessedActors;
	const bool bActorsHavePCGComponents = (UPCGSpawnActorSettings::GetGraphInterfaceFromActorSubclass(InTemplateActorClass) != nullptr);

	if (!ReusedManagedActorsResources.IsEmpty())
	{
		// If the actors are fully independent, we might need to make sure to call Generate if the underlying graph has changed - e.g. if the actor is dirty
		for (UPCGManagedActors* ManagedActors : ReusedManagedActorsResources)
		{
			check(ManagedActors);
			ManagedActors->MarkAsReused();

			// There's no setup to be done, just generation if we're in the no-merge case, so keep track of these actors only in this case
			if (bActorsHavePCGComponents && Settings->Option == EPCGSpawnActorOption::NoMerging)
			{
				for (TSoftObjectPtr<AActor>& ManagedActorPtr : ManagedActors->GeneratedActors)
				{
					if (AActor* ManagedActor = ManagedActorPtr.Get())
					{
						ProcessedActors.Add(ManagedActor);
					}
				}
			}
		}
	}
	else
	{
		TArray<FName> NewActorTags = GetNewActorTags(Context, TargetActor, Settings->bInheritActorTags, Settings->TagsToAddOnActors);

		// Create managed resource for actor tracking
		UPCGManagedActors* ManagedActors = NewObject<UPCGManagedActors>(Context->SourceComponent.Get());
		ManagedActors->SetCrc(InputDependenciesCrc);

		// If generated actors are not directly attached, place them in a subfolder for tidiness.
		FString GeneratedActorsFolderPath;
#if WITH_EDITOR
		PCGHelpers::GetGeneratedActorsFolderPath(TargetActor, GeneratedActorsFolderPath);
#endif

		for (int32 i = 0; i < Points.Num(); ++i)
		{
			const FPCGPoint& Point = Points[i];

			ActorOverrides.Apply(i);

			AActor* GeneratedActor = TargetActor->GetWorld()->SpawnActor(InTemplateActorClass, &Point.Transform, SpawnParams);

			if (!GeneratedActor)
			{
				PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("ActorSpawnFailed", "Failed to spawn actor on point with index {0}"), i));
				continue;
			}

			// HACK: until UE-62747 is fixed, we have to force set the scale after spawning the actor
			GeneratedActor->SetActorRelativeScale3D(Point.Transform.GetScale3D());
			GeneratedActor->Tags.Append(NewActorTags);
			PCGHelpers::AttachToParent(GeneratedActor, TargetActor, Settings->AttachOptions, GeneratedActorsFolderPath);

			for (UFunction* PostSpawnFunction : PostSpawnFunctions)
			{
				GeneratedActor->ProcessEvent(PostSpawnFunction, nullptr);
			}

			ManagedActors->GeneratedActors.Add(GeneratedActor);

			if (bActorsHavePCGComponents)
			{
				ProcessedActors.Add(GeneratedActor);
			}

			// Write to out data the actor reference
			if (OutPointData && ActorReferenceAttribute)
			{
				FPCGPoint& OutPoint = OutPointData->GetMutablePoints()[i + OutPointOffset];
				OutPointData->Metadata->InitializeOnSet(OutPoint.MetadataEntry);
				ActorReferenceAttribute->SetValue(OutPoint.MetadataEntry, FSoftObjectPath(GeneratedActor).ToString());
			}
		}

		Context->SourceComponent->AddToManagedResources(ManagedActors);

		PCGE_LOG(Verbose, LogOnly, FText::Format(LOCTEXT("GenerationInfo", "Generated {0} actors"), Points.Num()));
	}

	// Setup & Generate on PCG components if needed
	for (AActor* Actor : ProcessedActors)
	{
		TInlineComponentArray<UPCGComponent*, 1> PCGComponents;
		Actor->GetComponents(PCGComponents);

		for (UPCGComponent* PCGComponent : PCGComponents)
		{
			if (Settings->Option == EPCGSpawnActorOption::NoMerging)
			{
				if (bForceDisableActorParsing)
				{
					PCGComponent->bParseActorComponents = false;
				}

				if (bForceCallGenerate || (bOnLoadCallGenerate && PCGComponent->GenerationTrigger == EPCGComponentGenerationTrigger::GenerateOnLoad))
				{
					if (Subsystem)
					{
						Subsystem->RegisterOrUpdatePCGComponent(PCGComponent);
					}

					// TODO: use ScheduleGraph if we want to pass user parameters
					FPCGTaskId SubgraphTaskId = PCGComponent->GenerateLocalGetTaskId(/*bForce=*/true);
					if (SubgraphTaskId != InvalidPCGTaskId)
					{
						Context->SubgraphTaskIds.Add(SubgraphTaskId);
					}
				}
			}
			else // otherwise, they will be taken care of as-if a subgraph (either dynamically or statically)
			{
				PCGComponent->bActivated = false;
			}
		}
	}
}

TArray<FName> FPCGSpawnActorElement::GetNewActorTags(FPCGContext* Context, AActor* TargetActor, bool bInheritActorTags, const TArray<FName>& AdditionalTags) const
{
	TArray<FName> NewActorTags;
	// Prepare actor tags
	if (bInheritActorTags)
	{
		// Special case: if the current target actor is a partition, we'll reach out
		// and find the original actor tags
		if (APCGPartitionActor* PartitionActor = Cast<APCGPartitionActor>(TargetActor))
		{
			if (UPCGComponent* OriginalComponent = PartitionActor->GetOriginalComponent(Context->SourceComponent.Get()))
			{
				check(OriginalComponent->GetOwner());
				NewActorTags = OriginalComponent->GetOwner()->Tags;
			}
		}
		else
		{
			NewActorTags = TargetActor->Tags;
		}
	}

	NewActorTags.AddUnique(PCGHelpers::DefaultPCGActorTag);

	for (const FName& AdditionalTag : AdditionalTags)
	{
		NewActorTags.AddUnique(AdditionalTag);
	}

	return NewActorTags;
}

#if WITH_EDITOR
EPCGSettingsType UPCGSpawnActorSettings::GetType() const
{
	return GetSubgraph() ? EPCGSettingsType::Subgraph : EPCGSettingsType::Spawner;
}
#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE
