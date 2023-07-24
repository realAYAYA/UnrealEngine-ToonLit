// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGSpawnActor.h"

#include "PCGComponent.h"
#include "PCGGraph.h"
#include "PCGManagedResource.h"
#include "PCGSubsystem.h"
#include "Data/PCGPointData.h"
#include "Data/PCGSpatialData.h"
#include "Grid/PCGPartitionActor.h"
#include "Helpers/PCGActorHelpers.h"
#include "Helpers/PCGHelpers.h"
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
				const FPCGAttributePropertySelector& InputSelector = Overrides[i].InputSource;
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

UPCGNode* UPCGSpawnActorSettings::CreateNode() const
{
	return NewObject<UPCGSpawnActorNode>();
}

FPCGElementPtr UPCGSpawnActorSettings::CreateElement() const
{
	return MakeShared<FPCGSpawnActorElement>();
}

UPCGGraphInterface* UPCGSpawnActorSettings::GetSubgraphInterface() const
{
	if(!TemplateActorClass || TemplateActorClass->HasAnyClassFlags(CLASS_Abstract))
	{
		return nullptr;
	}

	TArray<UActorComponent*> PCGComponents;
	UPCGActorHelpers::GetActorClassDefaultComponents(TemplateActorClass, PCGComponents, UPCGComponent::StaticClass());

	if (PCGComponents.IsEmpty())
	{
		return nullptr;
	}

	for (UActorComponent* Component : PCGComponents)
	{
		if (UPCGComponent* PCGComponent = Cast<UPCGComponent>(Component))
		{
			// If there is no graph, there is no graph instance
			if (PCGComponent->GetGraph() && PCGComponent->bActivated)
			{
				return PCGComponent->GetGraphInstance();
			}
		}
	}

	return nullptr;
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
	SetupBlueprintEvent();

	if (TemplateActorClass)
	{
		if (TemplateActor)
		{
			TemplateActor->ConditionalPostLoad();
		}

		RefreshTemplateActor();
	}
#endif // WITH_EDITOR
}

#if WITH_EDITOR
bool UPCGSpawnActorSettings::IsStructuralProperty(const FName& InPropertyName) const
{
	return InPropertyName == GET_MEMBER_NAME_CHECKED(UPCGSpawnActorSettings, TemplateActorClass) || 
		InPropertyName == GET_MEMBER_NAME_CHECKED(UPCGSpawnActorSettings, Option) ||
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
	if (TemplateActorClass)
	{
		AActor* NewTemplateActor = NewObject<AActor>(this, TemplateActorClass, NAME_None, RF_ArchetypeObject | RF_Transactional | RF_Public);

		if (TemplateActor)
		{
			UEngine::FCopyPropertiesForUnrelatedObjectsParams Options;
			Options.bNotifyObjectReplacement = true;
			UEngine::CopyPropertiesForUnrelatedObjects(TemplateActor, NewTemplateActor, Options);

			TemplateActor->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_ForceNoResetLoaders);
		}

		TemplateActor = NewTemplateActor;
	}
	else
	{
		TemplateActor = nullptr;
	}
}

bool FPCGSpawnActorElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCSpawnActorElement::Execute);

	const UPCGSpawnActorSettings* Settings = Context->GetInputSettings<UPCGSpawnActorSettings>();
	check(Settings);

	// Early out
	if(!Settings->TemplateActorClass || Settings->TemplateActorClass->HasAnyClassFlags(CLASS_Abstract))
	{
		const FText ClassName = Settings->TemplateActorClass ? FText::FromString(Settings->TemplateActorClass->GetFName().ToString()) : FText::FromName(NAME_None);
		PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("InvalidTemplateActorClass", "Invalid template actor class '{0}'"), ClassName));
		return true;
	}

	if (!ensure(Settings->TemplateActor && Settings->TemplateActor->IsA(Settings->TemplateActorClass)))
	{
		return true;
	}

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

	TArray<UFunction*> PostSpawnFunctions;
	for (FName PostSpawnFunctionName : Settings->PostSpawnFunctionNames)
	{
		if (PostSpawnFunctionName == NAME_None)
		{
			continue;
		}

		if (UFunction* PostSpawnFunction = Settings->TemplateActorClass->FindFunctionByName(PostSpawnFunctionName))
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
			PCGE_LOG(Warning, GraphAndLog, FText::Format(LOCTEXT("FunctionNotFound", "PostSpawnFunction '{0}' was not found in class '{1}'"), FText::FromName(PostSpawnFunctionName), FText::FromName(Settings->TemplateActorClass->GetFName())));
		}
	}

	const bool bForceDisableActorParsing = (Settings->bForceDisableActorParsing);

	// Pass-through exclusions & settings
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputs();
	for (const FPCGTaggedData& Input : Inputs)
	{
		const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Input.Data);

		if (!SpatialData)
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("InvalidInputData", "Invalid input data"));
			continue;
		}

		AActor* TargetActor = Context->GetTargetActor(SpatialData);

		if (!TargetActor)
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("InvalidTargetActor", "Invalid target actor"));
			continue;
		}

		const bool bHasAuthority = !Context->SourceComponent.IsValid() || (Context->SourceComponent->GetOwner() && Context->SourceComponent->GetOwner()->HasAuthority());
		const bool bSpawnedActorsRequireAuthority = Settings->TemplateActor->GetIsReplicated();

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

		// Spawn actors/populate ISM
		if (!bFullySkippedDueToReuse && Settings->Option == EPCGSpawnActorOption::CollapseActors)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FPCGSpawnActorElement::ExecuteInternal::CollapseActors);
			TArray<UActorComponent*> Components;
			UPCGActorHelpers::GetActorClassDefaultComponents(Settings->TemplateActorClass, Components, UStaticMeshComponent::StaticClass());
			
			TMap<FPCGISMCBuilderParameters, TArray<FTransform>> MeshDescriptorTransforms;

			for (UActorComponent* Component : Components)
			{
				if (UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(Component))
				{
					FPCGISMCBuilderParameters Params;
					Params.Descriptor.InitFrom(StaticMeshComponent);
					// TODO: No custom data float support?

					TArray<FTransform>& Transforms = MeshDescriptorTransforms.FindOrAdd(Params);

					if (UInstancedStaticMeshComponent* InstancedStaticMeshComponent = Cast<UInstancedStaticMeshComponent>(StaticMeshComponent))
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
				}
			}
				
			for(const TPair<FPCGISMCBuilderParameters, TArray<FTransform>>& ISMCBuilderTransforms : MeshDescriptorTransforms)
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
		else if (!bFullySkippedDueToReuse && Settings->Option != EPCGSpawnActorOption::CollapseActors && (bHasAuthority || !bSpawnedActorsRequireAuthority))
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FPCGSpawnActorElement::ExecuteInternal::SpawnActors);
			AActor* TemplateActor = Settings->ActorOverrides.IsEmpty() ? Settings->TemplateActor.Get() : DuplicateObject(Settings->TemplateActor, GetTransientPackage());

			PCGSpawnActorHelpers::FActorOverrides ActorOverrides(Settings->ActorOverrides, TemplateActor, PointData);

			FActorSpawnParameters SpawnParams;
			SpawnParams.Owner = TargetActor;
			SpawnParams.Template = TemplateActor;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

			if (PCGHelpers::IsRuntimeOrPIE())
			{
				SpawnParams.ObjectFlags |= RF_Transient;
			}

			const bool bForceCallGenerate = Settings->bGenerationTrigger == EPCGSpawnActorGenerationTrigger::ForceGenerate;
#if WITH_EDITOR
			const bool bOnLoadCallGenerate = Settings->bGenerationTrigger == EPCGSpawnActorGenerationTrigger::Default;
#else
			const bool bOnLoadCallGenerate = (Settings->bGenerationTrigger == EPCGSpawnActorGenerationTrigger::Default ||
				Settings->bGenerationTrigger == EPCGSpawnActorGenerationTrigger::DoNotGenerateInEditor);
#endif
			UPCGSubsystem* Subsystem = (Context->SourceComponent.Get() ? Context->SourceComponent->GetSubsystem() : nullptr);

			// Try to reuse actors if the are preexisting
			TArray<UPCGManagedActors*> ReusedManagedActorsResources;
			if (CVarAllowActorReuse.GetValueOnAnyThread())
			{
				if (Context->DependenciesCrc.IsValid())
				{
					Context->SourceComponent->ForEachManagedResource([&ReusedManagedActorsResources, &Context](UPCGManagedResource* InResource)
					{
						if (UPCGManagedActors* Resource = Cast<UPCGManagedActors>(InResource))
						{
							if (Resource->GetCrc().IsValid() && Resource->GetCrc() == Context->DependenciesCrc)
							{
								ReusedManagedActorsResources.Add(Resource);
							}
						}
					});
				}
			}

			TArray<AActor*> ProcessedActors;
			const bool bActorsHavePCGComponents = (Settings->GetSubgraphInterface() != nullptr);

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
				ManagedActors->SetCrc(Context->DependenciesCrc);

				for (int32 i = 0; i < Points.Num(); ++i)
				{
					const FPCGPoint& Point = Points[i];

					ActorOverrides.Apply(i);

					AActor* GeneratedActor = TargetActor->GetWorld()->SpawnActor(Settings->TemplateActorClass, &Point.Transform, SpawnParams);

					if (!GeneratedActor)
					{
						PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("ActorSpawnFailed", "Failed to spawn actor on point with index {0}"), i));
						continue;
					}

					// HACK: until UE-62747 is fixed, we have to force set the scale after spawning the actor
					GeneratedActor->SetActorRelativeScale3D(Point.Transform.GetScale3D());
					GeneratedActor->Tags.Append(NewActorTags);
					GeneratedActor->AttachToActor(TargetActor, FAttachmentTransformRules::KeepWorldTransform);

					for (UFunction* PostSpawnFunction : PostSpawnFunctions)
					{
						GeneratedActor->ProcessEvent(PostSpawnFunction, nullptr);
					}

					ManagedActors->GeneratedActors.Add(GeneratedActor);

					if (bActorsHavePCGComponents)
					{
						ProcessedActors.Add(GeneratedActor);
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
					if(Settings->Option == EPCGSpawnActorOption::NoMerging)
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

							PCGComponent->Generate();
						}
					}
					else
					{
						PCGComponent->bActivated = false;
					}
				}
			}
		}

		// Finally, pass through the input, in all cases: 
		// - if it's not merged, will be the input points directly
		// - if it's merged but there is no subgraph, will be the input points directly
		// - if it's merged and there is a subgraph, we'd need to pass the data for it to be given to the subgraph
		Outputs.Add(Input);
	}

	return true;
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
