// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassLWIStaticMeshManager.h"
#include "MassLWITypes.h"
#include "MassLWISubsystem.h"
#include "MassLWITypes.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "MassInstancedStaticMeshComponent.h"
#include "MassCommonFragments.h"
#include "MassEntitySubsystem.h"
#include "MassEntityView.h"
#include "MassEntityQuery.h"
#include "MassRepresentationSubsystem.h"
#include "MassSpawnerSubsystem.h"
#include "MassSpawnerTypes.h"
#include "MassSpawnLocationProcessor.h"
#include "MassVisualizationTrait.h"
#include "VisualLogger/VisualLogger.h"


namespace UE::Mass::Tweakables
{
	bool bDestroyEntitiesOnEndPlay = true;

	FAutoConsoleVariableRef CLWIVars[] = {
		{TEXT("mass.LWI.DestroyEntitiesOnEndPlay"), bDestroyEntitiesOnEndPlay, TEXT("Whether we should destroy LWI-sources entities when the original LWI manager ends play")},
	};
}

//-----------------------------------------------------------------------------
// AMassLWIStaticMeshManager
//-----------------------------------------------------------------------------
void AMassLWIStaticMeshManager::BeginPlay()
{
	Super::BeginPlay();

	UWorld* World = GetWorld();
	if (World != nullptr && IsRegisteredWithMass() == false)
	{
		if (UMassLWISubsystem* LWIxMass = World->GetSubsystem<UMassLWISubsystem>())
		{
			LWIxMass->RegisterLWIManager(*this);
		}
	}
}

void AMassLWIStaticMeshManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UWorld* World = GetWorld();
	if (World && IsRegisteredWithMass())
	{
		if (UE::Mass::Tweakables::bDestroyEntitiesOnEndPlay && Entities.Num())
		{
			check(InstanceTransforms.Num() == 0);
			InstanceTransforms.Reset(Entities.Num());

			if (UMassEntitySubsystem* EntitySubsystem = UWorld::GetSubsystem<UMassEntitySubsystem>(GetWorld()))
			{
				FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
				StoreMassDataInActor(EntityManager);
			}
		}

		if (UMassLWISubsystem* LWIxMass = World->GetSubsystem<UMassLWISubsystem>())
		{
			LWIxMass->UnregisterLWIManager(*this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void AMassLWIStaticMeshManager::TransferDataToMass(FMassEntityManager& EntityManager)
{
	if (InstanceTransforms.Num() == 0)
	{
		return;
	}

	if (MassTemplateID.IsValid() == false && RepresentedClass)
	{
		CreateMassTemplate(EntityManager);
	}

	if (MassTemplateID.IsValid() && InstancedStaticMeshComponent && FinalizedTemplate)
	{
		const FVector& ManagerLocation = GetActorLocation();

		UMassSpawnerSubsystem* SpawnerSystem = UWorld::GetSubsystem<UMassSpawnerSubsystem>(GetWorld());
		check(SpawnerSystem);
		const int32 NumEntities = InstanceTransforms.Num();
		
		FMassTransformsSpawnData SpawnLocationData;
		SpawnLocationData.bRandomize = false;
		SpawnLocationData.Transforms = InstanceTransforms;
		if (ManagerLocation.IsNearlyZero() == false)
		{
			for (FTransform& EntityTransform : SpawnLocationData.Transforms)
			{
				EntityTransform.AddToTranslation(ManagerLocation);
			}
		}

		FConstStructView SpawnLocationDataView = FConstStructView::Make(SpawnLocationData);
		SpawnerSystem->SpawnEntities(FinalizedTemplate->GetTemplateID(), NumEntities, SpawnLocationDataView, UMassSpawnLocationProcessor::StaticClass(), Entities);

		// destroy actors that have already been created
		for (const TPair<int32, TObjectPtr<AActor>>& ActorPair : Actors)
		{
			if (ActorPair.Value)
			{
				ActorPair.Value->OnDestroyed.RemoveAll(this);
				ActorPair.Value->Destroy();
			}
		}
		Actors.Reset();

		InstanceTransforms.Reset();
		ValidIndices.Reset();
		FreeIndices.Reset();
		
		InstancedStaticMeshComponent->UnregisterComponent();
		InstancedStaticMeshComponent->DestroyComponent();
	}
}

void AMassLWIStaticMeshManager::StoreMassDataInActor(FMassEntityManager& EntityManager)
{
	TArray<FMassArchetypeEntityCollection> EntityCollectionsToDestroy;
	UE::Mass::Utils::CreateEntityCollections(EntityManager, Entities, FMassArchetypeEntityCollection::FoldDuplicates, EntityCollectionsToDestroy);

	// we create a query to "recreate" contents of InstanceTransforms just in case BeginPlay will be get
	// called again for this LWIManager, which can happen with actor streaming
	FMassEntityQuery LocationQuery;
	LocationQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);

	FMassExecutionContext ExecutionContext(EntityManager);
	for (FMassArchetypeEntityCollection& Collection : EntityCollectionsToDestroy)
	{
		LocationQuery.ForEachEntityChunk(Collection, EntityManager, ExecutionContext, [this](FMassExecutionContext& Context)
		{
			TConstArrayView<FTransformFragment> TransformsList = Context.GetFragmentView<FTransformFragment>();
			for (const FTransformFragment& Fragment : TransformsList)
			{
				InstanceTransforms.Add(Fragment.GetTransform());
			}
		});

		EntityManager.BatchDestroyEntityChunks(Collection);
	}

	Entities.Reset();
}

int32 AMassLWIStaticMeshManager::FindIndexForEntity(const FMassEntityHandle Entity) const
{
	return Entities.IndexOfByKey(Entity);
}

AMassLWIStaticMeshManager* AMassLWIStaticMeshManager::GetMassLWIManagerForEntity(const FMassEntityManager& EntityManager, const FMassEntityHandle Entity)
{
	FMassEntityView EntityView(EntityManager, Entity);
	if (EntityView.IsSet())
	{	
		const FMassLWIManagerSharedFragment* LWIManagerSharedFragmentPtr = EntityView.GetSharedFragmentDataPtr<FMassLWIManagerSharedFragment>();
		if (ensure(LWIManagerSharedFragmentPtr))
		{
			AMassLWIStaticMeshManager* LWIManager = LWIManagerSharedFragmentPtr->LWIManager.Get();
			return LWIManager;
		}
	}

	return nullptr;
}

void AMassLWIStaticMeshManager::CreateMassTemplate(FMassEntityManager& EntityManager)
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	UMassSpawnerSubsystem* SpawnerSystem = World->GetSubsystem<UMassSpawnerSubsystem>(World);
	if (!ensure(SpawnerSystem))
	{
		//+complain
		return;
	}

	UMassLWISubsystem* LWIxMass = World->GetSubsystem<UMassLWISubsystem>();
	if (!ensure(LWIxMass))
	{
		return;
	}

	const FMassEntityConfig* ClassConfig = LWIxMass->GetConfigForClass(RepresentedClass);
	if (!ClassConfig || ClassConfig->IsEmpty())
	{
		UE_VLOG_UELOG(this, LogMass, Log, TEXT("%s failed to find a calid entity config for %s class. This LWI manager won't transfer data to Mass.")
			, *UObjectBaseUtility::GetName(), *GetNameSafe(RepresentedClass));
		return;
	}

	const UMassVisualizationTrait* VisTrait = Cast<const UMassVisualizationTrait>(ClassConfig->FindTrait(UMassVisualizationTrait::StaticClass()));
	if (!ensureMsgf(VisTrait, TEXT("The config used doesn't contain a VisualizationTrait, which is required for LWIxMass to function")))
	{
		return;
	}

	UMassRepresentationSubsystem* RepresentationSubsystem = nullptr;
	
	const FMassEntityTemplate& SourceTemplate = ClassConfig->GetOrCreateEntityTemplate(*World);
	// what we want to do now is to modify the config to point at the specific static mesh and actor class
	FMassEntityTemplateData NewTemplate(SourceTemplate);

		
	const FMassArchetypeSharedFragmentValues& SharedFragmentValues = SourceTemplate.GetSharedFragmentValues();
	const TArray<FSharedStruct>& SharedFragments = SharedFragmentValues.GetSharedFragments();
	for (FSharedStruct SharedFragment : SharedFragments)
	{
		if (FMassRepresentationSubsystemSharedFragment* AsRepresentationSubsystemSharedFragment = SharedFragment.GetPtr<FMassRepresentationSubsystemSharedFragment>())
		{
			RepresentationSubsystem = AsRepresentationSubsystemSharedFragment->RepresentationSubsystem;
			break;
		}
	}
	
	FStaticMeshInstanceVisualizationDesc StaticMeshInstanceDesc = VisTrait->StaticMeshInstanceDesc;
	
	// we don't care about the mesh that has been set in the source template, we're overriding it anyway.
	StaticMeshInstanceDesc.Meshes.SetNumZeroed(1);
	FMassStaticMeshInstanceVisualizationMeshDesc& MeshDesc = StaticMeshInstanceDesc.Meshes[0];

	if (!MeshDesc.ISMComponentClass)
	{
		MeshDesc.ISMComponentClass = UMassInstancedStaticMeshComponent::StaticClass();
	}

	// forcing the "full range" since we only ever expect there to be one mesh for the relevant ISM component
	MeshDesc.SetSignificanceRange(EMassLOD::High, EMassLOD::Max);
	
	MeshDesc.Mesh = StaticMesh.Get();

	if (!ensure(MeshDesc.Mesh))
	{
		MeshDesc.Mesh = StaticMesh.LoadSynchronous();
		if (!ensure(MeshDesc.Mesh))
		{
			UE_VLOG_UELOG(this, LogMassLWI, Error, TEXT("%s: Unable to load mesh asset %s"), *UObject::GetName(), *StaticMesh.GetLongPackageName());
			return;
		}
	}

	if (InstancedStaticMeshComponent)
	{
		MeshDesc.MaterialOverrides = InstancedStaticMeshComponent->OverrideMaterials;
		MeshDesc.bCastShadows = (InstancedStaticMeshComponent->CastShadow != 0);
	}

	check(MeshDesc.Mesh);
	NewTemplate.SetTemplateName(MeshDesc.Mesh->GetName());

	if (RepresentationSubsystem)
	{
		FMassRepresentationFragment& RepresentationFragment = NewTemplate.AddFragment_GetRef<FMassRepresentationFragment>();
		RepresentationFragment.StaticMeshDescIndex = RepresentationSubsystem->FindOrAddStaticMeshDesc(StaticMeshInstanceDesc);
		const int32 TemplateActorIndex = RepresentationSubsystem->FindOrAddTemplateActor(RepresentedClass);
		RepresentationFragment.HighResTemplateActorIndex = TemplateActorIndex;
		// leaving for reference here, since at some point we probably will need to set it.
		//RepresentationFragment.LowResTemplateActorIndex = TemplateActorIndex;
	}

	FMassLWIManagerSharedFragment LWIManagerSharedFragment;
	LWIManagerSharedFragment.LWIManager = this;
	const uint32 LWIManagerHash = UE::StructUtils::GetStructCrc32(FConstStructView::Make(LWIManagerSharedFragment));
	FSharedStruct LWIManagerFragment = EntityManager.GetOrCreateSharedFragmentByHash<FMassLWIManagerSharedFragment>(LWIManagerHash, LWIManagerSharedFragment);
	NewTemplate.AddSharedFragment(LWIManagerFragment);

	FMassEntityTemplateRegistry& TemplateRegistry = SpawnerSystem->GetMutableTemplateRegistryInstance();
	const FString ObjectPath = GetPathName();
	const uint32 FlavorHash = HashCombine(GetTypeHash(GetNameSafe(RepresentedClass)), GetTypeHash(ObjectPath));
	MassTemplateID = FMassEntityTemplateIDFactory::MakeFlavor(SourceTemplate.GetTemplateID(), FlavorHash);

	const TSharedRef<FMassEntityTemplate>& NewFinalizedTemplate = TemplateRegistry.FindOrAddTemplate(MassTemplateID, MoveTemp(NewTemplate));
	MassTemplateID = NewFinalizedTemplate->GetTemplateID();
	FinalizedTemplate = NewFinalizedTemplate;

}

void AMassLWIStaticMeshManager::MarkRegisteredWithMass(const FMassLWIManagerRegistrationHandle RegistrationIndex)
{
	check(MassRegistrationHandle.IsValid() == false && RegistrationIndex.IsValid() == true);
	new (&MassRegistrationHandle) FMassLWIManagerRegistrationHandle(RegistrationIndex);
}

void AMassLWIStaticMeshManager::MarkUnregisteredWithMass()
{
	new (&MassRegistrationHandle) FMassLWIManagerRegistrationHandle();
}
