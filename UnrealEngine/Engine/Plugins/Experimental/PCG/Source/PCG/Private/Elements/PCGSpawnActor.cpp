// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGSpawnActor.h"

#include "PCGComponent.h"
#include "PCGHelpers.h"
#include "PCGManagedResource.h"

#include "Data/PCGPointData.h"
#include "Grid/PCGPartitionActor.h"
#include "Helpers/PCGActorHelpers.h"
#include "Helpers/PCGSettingsHelpers.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"

UPCGNode* UPCGSpawnActorSettings::CreateNode() const
{
	return NewObject<UPCGSpawnActorNode>();
}

FPCGElementPtr UPCGSpawnActorSettings::CreateElement() const
{
	return MakeShared<FPCGSpawnActorElement>();
}

UPCGGraph* UPCGSpawnActorSettings::GetSubgraph() const
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
			if (PCGComponent->GetGraph() && PCGComponent->bActivated)
			{
				return PCGComponent->GetGraph();
			}
		}
	}

	return nullptr;
}

#if WITH_EDITOR
bool UPCGSpawnActorSettings::IsStructuralProperty(const FName& InPropertyName) const
{
	return InPropertyName == GET_MEMBER_NAME_CHECKED(UPCGSpawnActorSettings, TemplateActorClass) || 
		InPropertyName == GET_MEMBER_NAME_CHECKED(UPCGSpawnActorSettings, Option) ||
		Super::IsStructuralProperty(InPropertyName);
}
#endif // WITH_EDITOR

TObjectPtr<UPCGGraph> UPCGSpawnActorNode::GetSubgraph() const
{
	TObjectPtr<UPCGSpawnActorSettings> Settings = Cast<UPCGSpawnActorSettings>(DefaultSettings);
	return (Settings && Settings->Option != EPCGSpawnActorOption::NoMerging) ? Settings->GetSubgraph() : nullptr;
}

bool FPCGSpawnActorElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCSpawnActorElement::Execute);

	const UPCGSpawnActorSettings* Settings = Context->GetInputSettings<UPCGSpawnActorSettings>();
	check(Settings);

	// Early out
	if(!Settings->TemplateActorClass || Settings->TemplateActorClass->HasAnyClassFlags(CLASS_Abstract))
	{
		PCGE_LOG(Error, "Invalid template actor class (%s)", Settings->TemplateActorClass ? *Settings->TemplateActorClass->GetFName().ToString() : TEXT("None"));
		return true;
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
				PCGE_LOG(Warning, "PostSpawnFunction \"%s\" requires parameters. We only support parameter-less functions. Will skip the call.", *PostSpawnFunctionName.ToString());
			}
			else
			{
				PostSpawnFunctions.Add(PostSpawnFunction);
			}
		}
		else
		{
			PCGE_LOG(Warning, "PostSpawnFunction \"%s\" was not found in class \"%s\".", *PostSpawnFunctionName.ToString(), *Settings->TemplateActorClass->GetFName().ToString());
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
			PCGE_LOG(Error, "Invalid input data");
			continue;
		}

		AActor* TargetActor = SpatialData->TargetActor.Get();

		if (!TargetActor)
		{
			PCGE_LOG(Error, "Invalid target actor");
			continue;
		}

		const bool bHasAuthority = !Context->SourceComponent.IsValid() || (Context->SourceComponent->GetOwner() && Context->SourceComponent->GetOwner()->HasAuthority());
		const bool bSpawnedActorsRequireAuthority = Settings->TemplateActorClass->GetDefaultObject<AActor>()->GetIsReplicated();

		// First, create target instance transforms
		const UPCGPointData* PointData = SpatialData->ToPointData(Context);

		if (!PointData)
		{
			PCGE_LOG(Error, "Unable to get point data from input");
			continue;
		}

		const TArray<FPCGPoint>& Points = PointData->GetPoints();

		if (Points.IsEmpty())
		{
			PCGE_LOG(Verbose, "Skipped - no points");
			continue;
		}

		// Spawn actors/populate ISM
		{
			UInstancedStaticMeshComponent* ISMC = nullptr;
			TArray<FTransform> Instances;

			// If we are collapsing actors, we need to get the mesh & prep the ISMC
			if (Settings->Option == EPCGSpawnActorOption::CollapseActors)
			{
				TArray<UActorComponent*> Components;
				UPCGActorHelpers::GetActorClassDefaultComponents(Settings->TemplateActorClass, Components, UStaticMeshComponent::StaticClass());
				UStaticMeshComponent* FirstSMC = nullptr;
				UStaticMesh* Mesh = nullptr;

				for (UActorComponent* Component : Components)
				{
					if (UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(Component))
					{
						FirstSMC = SMC;
						Mesh = SMC->GetStaticMesh();
						if (Mesh)
						{
							break;
						}
					}
				}

				if (Mesh)
				{
					FPCGISMCBuilderParameters Params;
					Params.Mesh = Mesh;

					ISMC = UPCGActorHelpers::GetOrCreateISMC(TargetActor, Context->SourceComponent.Get(), Params);
					UEngine::CopyPropertiesForUnrelatedObjects(FirstSMC, ISMC);
				}
				else
				{
					PCGE_LOG(Error, "No supported mesh found");
				}
			}

			{
				TRACE_CPUPROFILER_EVENT_SCOPE(FPCGSpawnActorElement::ExecuteInternal::SpawnActors);
				if (Settings->Option == EPCGSpawnActorOption::CollapseActors && ISMC)
				{
					for (const FPCGPoint& Point : Points)
					{
						Instances.Add(Point.Transform);
					}
				}
				else if (Settings->Option != EPCGSpawnActorOption::CollapseActors && (bHasAuthority || !bSpawnedActorsRequireAuthority))
				{
					FActorSpawnParameters SpawnParams;
					SpawnParams.Owner = TargetActor;
					SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

					TArray<FName> NewActorTags = GetNewActorTags(Context, TargetActor, Settings->bInheritActorTags, Settings->TagsToAddOnActors);

					// Create managed resource for actor tracking
					UPCGManagedActors* ManagedActors = NewObject<UPCGManagedActors>(Context->SourceComponent.Get());

					for (const FPCGPoint& Point : Points)
					{
						AActor* GeneratedActor = TargetActor->GetWorld()->SpawnActor(Settings->TemplateActorClass, &Point.Transform, SpawnParams);
						// HACK: until UE-62747 is fixed, we have to force set the scale after spawning the actor
						GeneratedActor->SetActorRelativeScale3D(Point.Transform.GetScale3D());
						GeneratedActor->Tags.Append(NewActorTags);
						GeneratedActor->AttachToActor(TargetActor, FAttachmentTransformRules::KeepWorldTransform);

						for (UFunction* PostSpawnFunction : PostSpawnFunctions)
						{
							GeneratedActor->ProcessEvent(PostSpawnFunction, nullptr);
						}

						ManagedActors->GeneratedActors.Add(GeneratedActor);

						// If the actor spawned has a PCG component, either generate it or mark it as generated if we pass through its inputs
						TArray<UPCGComponent*> PCGComponents;
						GeneratedActor->GetComponents(PCGComponents);

						for (UPCGComponent* PCGComponent : PCGComponents)
						{
							if (Settings->Option == EPCGSpawnActorOption::NoMerging)
							{
								if (bForceDisableActorParsing)
								{
									PCGComponent->bParseActorComponents = false;
								}

								PCGComponent->Generate();
							}
							else
							{
								PCGComponent->bActivated = false;
							}
						}
					}

					Context->SourceComponent->AddToManagedResources(ManagedActors);

					PCGE_LOG(Verbose, "Generated %d actors", Points.Num());
				}
			}

			// Finalize
			if (ISMC && Instances.Num() > 0)
			{
				ISMC->NumCustomDataFloats = 0;
				ISMC->AddInstances(Instances, false, true);
				ISMC->UpdateBounds();

				PCGE_LOG(Verbose, "Added %d ISM instances", Instances.Num());
			}
		}

		// Finally, pass through the input, in all cases: 
		// - if it's not merged, will be the input points directly
		// - if it's merged but there is no subgraph, will be the input points directly
		// - if it's merged and there is a subgraph, we'd need to pass the data for it to be given to the subgraph
		{
			Outputs.Add(Input);
		}
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