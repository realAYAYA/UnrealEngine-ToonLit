// Copyright Epic Games, Inc. All Rights Reserved.

#include "InstancedActorsData.h"
#include "InstancedActorsComponent.h"
#include "InstancedActorsCustomVersion.h"
#include "InstancedActorsInitializerProcessor.h"
#include "InstancedActorsManager.h"
#include "InstancedActorsRepresentationActorManagement.h"
#include "InstancedActorsRepresentationSubsystem.h"
#include "InstancedActorsSubsystem.h"
#include "InstancedActorsVisualizationTrait.h"
#include "InstancedActorsSettingsTypes.h"
#include "UObject/ObjectSaveContext.h"
#include "Algo/Count.h"
#include "Algo/NoneOf.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StaticMesh.h"
#include "MassActorSubsystem.h"
#include "MassCommonFragments.h"
#include "MassEntityConfigAsset.h"
#include "MassEntityTemplate.h"
#include "MassEntityView.h"
#include "MassLODFragments.h"
#include "MassLODTrait.h"
#include "MassRepresentationProcessor.h"
#include "MassSpawnerSubsystem.h"
#include "MassDistanceLODProcessor.h"
#include "MassRepresentationTypes.h"
#include "Net/UnrealNetwork.h"
#include "Misc/Crc.h"


#define DO_COLLISION_INDEX_DEBUG (1 && WITH_INSTANCEDACTORS_DEBUG)

#if DO_COLLISION_INDEX_DEBUG
#include "InstancedActorsDebug.h"
#include "VisualLogger/VisualLogger.h"
#include "MassEntitySubsystem.h"
#define VLOG_COLLISIONINDEX UE_VLOG
#define CVLOG_COLLISIONINDEX UE_CVLOG

#else
#define VLOG_COLLISIONINDEX(...)
#define CVLOG_COLLISIONINDEX(...)
#endif // DO_COLLISION_INDEX_DEBUG

DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Registered ISMs"), STAT_RegisteredISMs, STATGROUP_InstancedActorsRendering);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Total Instance Count"), STAT_TotalInstanceCount, STATGROUP_InstancedActorsRendering);

namespace UE::InstancedActors
{
	namespace CVars
	{
		bool bCookCompressedInstances = false;
		FAutoConsoleVariableRef CVarCookCompressedInstances(
			TEXT("IA.CookCompressedInstances"),
			bCookCompressedInstances,
			TEXT("Enables instances transform compression on cook for all AInstancedActorsManager's"),
			ECVF_Cheat);

		float CompressedLocationError = 0.0f;
		FAutoConsoleVariableRef CVarCompressedLocationError(
			TEXT("IA.CompressedLocationError"),
			CompressedLocationError,
			TEXT("The maximum acceptable error for compressed locations"),
			ECVF_Cheat);

		float CompressedRotationError = 1.0f;
		FAutoConsoleVariableRef CVarCompressedRotationError(
			TEXT("IA.CompressedRotationError"),
			CompressedRotationError,
			TEXT("The allowed error in degrees for a rotation"),
			ECVF_Cheat);

		float CompressedScaleError = 0.2f;
		FAutoConsoleVariableRef CVarCompressedScaleError(
			TEXT("IA.CompressedScaleError"),
			CompressedScaleError,
			TEXT("The maximum acceptable error for compressed locations"),
			ECVF_Cheat);

		bool bEnableFarDistanceRendering = true;
		FAutoConsoleVariableRef CVarEnableFarDistanceRendering(
			TEXT("IA.EnableFarDistanceRendering"),
			bEnableFarDistanceRendering,
			TEXT("Enable far distance rendering for non distance culled resources from IA.ForceLowLODStartDrawDistance upto IA.MaxDrawDistance"),
			ECVF_Default);

		int LowLODMaxTriangleCountForFarDistanceRendering = 12;
		FAutoConsoleVariableRef CVarLowLODMaxTriangleCountForFarDistanceRendering(
			TEXT("IA.LowLODMaxTriangleCountForDistanceRendering"),
			LowLODMaxTriangleCountForFarDistanceRendering,
			TEXT("Maximum amount of triangles in the lowest LOD to disable distance culling"),
			ECVF_Default);

		float ForcedLowLODStartDrawDistance = 35000.0f;
		FAutoConsoleVariableRef CVarForcedLowLODStartDrawDistance(
			TEXT("IA.ForceLowLODStartDrawDistance"),
			ForcedLowLODStartDrawDistance,
			TEXT("Distance from where forced low LODs are rendered"),
			ECVF_Default);

		float MaxDrawDistance = 150000.0f;
		FAutoConsoleVariableRef CVarMaxDrawDistance(
			TEXT("IA.MaxDrawDistance"),
			MaxDrawDistance,
			TEXT("Maximum distance up to where instances will be rendered"),
			ECVF_Cheat);

		bool ForceMaxDrawDistance = false;
		FAutoConsoleVariableRef CVarForceMaxDrawDistance(
			TEXT("IA.ForceMaxDrawDistance"),
			ForceMaxDrawDistance,
			TEXT("Always use IA.MaxDrawDistance as the max draw distance. Ignores any resource overrides. Useful for beauty shots."),
			ECVF_Cheat);

		bool bRecycleInvalidInstances = false;
		FAutoConsoleVariableRef CVarRecycleInvalidInstances(
			TEXT("IA.RecycleInvalidInstances"),
			bRecycleInvalidInstances,
			TEXT("When removing instances, IAM's simply invalidate the given instance to ensure indexing of the remaining instances is unaffected. ")
			TEXT("With RecycleInvalidInstances enabled, subsequent instancing will then 'reuse' these invalidated instance indices, avoiding the need ")
			TEXT("to add an additional instance to the manager. Useful prior to shipping when instance instance indices don't need to be maintained for ")
			TEXT("persistence compatibility."),
			ECVF_Default);

		/* @todo needs implementation
		bool bAddGuidFragment = false;
		FAutoConsoleVariableRef CVarAddGuidFragment(
			TEXT("IA.AddGuidFragment"),
			bAddGuidFragment,
			TEXT("Whether newly created entities will get a FMassGuidFragment. Affects only newly created templates."),
			ECVF_Cheat);*/

		bool bForceSyncLoadVisualizations = false;
		FAutoConsoleVariableRef CVarForceSyncLoadVisualizations(
			TEXT("IA.ForceSyncLoadVisualizations"),
			bForceSyncLoadVisualizations,
			TEXT("If enabled, forces all calls to UInstancedActorsData::AddVisualizationAsync to immediately sync load required assets and ")
			TEXT("initialize the visualization, instead of requesting async loads and deferring initialization."),
			ECVF_Default);
	} // CVars

	namespace Helpers
	{
		FORCEINLINE bool IsValidInstanceTransform(const FTransform& InstanceTransform)
		{
			return !InstanceTransform.GetScale3D().IsZero();
		}
		FORCEINLINE void InvalidateInstanceTransform(FTransform& InstanceTransform)
		{
			InstanceTransform.SetIdentityZeroScale();
		}
	} // Helpers
} // namespace InstancedActors

//-----------------------------------------------------------------------------
// UInstancedActorsData
//-----------------------------------------------------------------------------
void UInstancedActorsData::Initialize()
{
	AInstancedActorsManager& Manager = GetManagerChecked();

	// Get the settings setup nice and early.
	UInstancedActorsSubsystem& InstancedActorSubsystem = UInstancedActorsSubsystem::GetChecked(GetManager());
	SharedSettings = InstancedActorSubsystem.GetOrCompileSettingsForActorClass(ActorClass);
	const FInstancedActorsSettings* Settings = GetSettingsPtr<const FInstancedActorsSettings>();

	// Allow settings to override the actor class.
	if (Settings && Settings->bOverride_ActorClass && Settings->ActorClass)
	{
		ActorClass = Settings->ActorClass;
	}

	// Allow settings to scale NumValidInstances, effectively scaling the number of spawned entities
	// Note: This must only ever be performed if !bHasEverBegunPlay to ensure we don't double scale
	// NumValidInstances in BeginPlay + EndPlay + BeginPlay scenarios
	if (Settings && Settings->bOverride_ScaleEntityCount && !bHasEverInitialized)
	{
		/**
		 * The "Missing Chance" section below is to make sure we end up with roughly the right
		 * number of instances even if we have small numbers of instances on each IAD.
		 * For example, 90% of 1 instance will always be one or zero instances. With the code
		 * below we make sure that 90% of the time it's 1 and 10% of the time it's 0. Averaged
		 * out across all IAMs that should mean we end up with 90%.
		 */
		const float ScaledNumValidInstances = static_cast<float>(NumValidInstances) * FMath::Clamp(Settings->ScaleEntityCount, 0.0, 1.0);
		NumValidInstances = FMath::FloorToInt(ScaledNumValidInstances);
		const float MissingChance = ScaledNumValidInstances - static_cast<float>(NumValidInstances);
		if (MissingChance > UE_KINDA_SMALL_NUMBER)
		{
			// We do need this to be deterministic thus the construct below.
			const FVector ManagerLocation = Manager.GetActorLocation();
			const FIntVector IntLocation(ManagerLocation);
			const int32 NamedSeed = FCrc::StrCrc32(*ActorClass->GetName());
			const int32 RandomSeed = FCrc::TypeCrc32(IntLocation, NamedSeed);
			FRandomStream RandomStream(RandomSeed);
			if (MissingChance > RandomStream.GetFraction() && NumValidInstances < MAX_uint16)
			{
				++NumValidInstances;
			}
		}
	}

	// Get or create exemplar actor to derive entities from
	const AActor& ExemplarActor = Manager.GetInstancedActorSubsystemChecked().GetOrCreateExemplarActor(ActorClass);

	// Add default visualization at index 0
	//FInstancedActorsVisualizationDesc DefaultVisualiation = FInstancedActorsVisualizationDesc::FromActor(ExemplarActor, &UE::InstancedActors::VisualizationDescrFromActorAdditionalSteps);
	FInstancedActorsVisualizationDesc DefaultVisualiation = InstancedActorSubsystem.CreateVisualDescriptionFromActor(ExemplarActor);
	const uint8 VisualizationIndex = AddVisualization(DefaultVisualiation);
	check(VisualizationIndex == 0);

	// Create entity template
	CreateEntityTemplate(ExemplarActor);

	bHasEverInitialized = true;
}

void UInstancedActorsData::CreateEntityTemplate(const AActor& ExemplarActor)
{
	UWorld* World = GetWorld();
	check(World);

	FMassEntityManager& MassEntityManager = GetMassEntityManagerChecked();

	FMassEntityConfig EntityConfig(*this);

	UMassDistanceLODCollectorTrait* LODCollectorTrait = NewObject<UMassDistanceLODCollectorTrait>(this);
	EntityConfig.AddTrait(*LODCollectorTrait);

	UInstancedActorsVisualizationTrait* VisTrait = NewObject<UInstancedActorsVisualizationTrait>(this, GET_INSTANCEDACTORS_CONFIG_VALUE(GetStationaryVisualizationTraitClass()));
	if (UInstancedActorsVisualizationTrait* InstancedActorsVisTrait = Cast<UInstancedActorsVisualizationTrait>(VisTrait))
	{
		InstancedActorsVisTrait->InitializeFromInstanceData(*this);
	}
	EntityConfig.AddTrait(*VisTrait);

	// Allow UInstancedActorsComponent's to extend entity config
	ExemplarActor.ForEachComponent<UInstancedActorsComponent>(/*bIncludeFromChildActors*/ false, [this, &MassEntityManager, &EntityConfig](const UInstancedActorsComponent* InstancedActorComponent)
		{ InstancedActorComponent->ModifyMassEntityConfig(MassEntityManager, this, EntityConfig); });

	const FMassEntityTemplate& BaseEntityTemplate = EntityConfig.GetOrCreateEntityTemplate(*World);

	FMassEntityTemplateData ModifiedTemplate(BaseEntityTemplate);
	ModifyEntityTemplate(ModifiedTemplate, ExemplarActor);
	
	// Allow UInstancedActorsComponent's to extend entity template
	ExemplarActor.ForEachComponent<UInstancedActorsComponent>(/*bIncludeFromChildActors*/ false
		, [this, &MassEntityManager, &ModifiedTemplate](const UInstancedActorsComponent* InstancedActorComponent)
		{
			InstancedActorComponent->ModifyMassEntityTemplate(MassEntityManager, this, ModifiedTemplate);
			return true;
		});

	UMassSpawnerSubsystem* MassSpawnerSubsystem = UWorld::GetSubsystem<UMassSpawnerSubsystem>(GetWorld());
	FMassEntityTemplateRegistry& TemplateRegistry = MassSpawnerSubsystem->GetMutableTemplateRegistryInstance();
	const TSharedRef<FMassEntityTemplate>& FinalizedTemplate = TemplateRegistry.FindOrAddTemplate(FMassEntityTemplateIDFactory::Make(FGuid::NewGuid()), MoveTemp(ModifiedTemplate));
	EntityTemplateID = FinalizedTemplate->GetTemplateID();
}

void UInstancedActorsData::ModifyEntityTemplate(FMassEntityTemplateData& ModifiedTemplate, const AActor&)
{
	ModifiedTemplate.RemoveTag<FMassDistanceLODProcessorTag>();
	ModifiedTemplate.RemoveTag<FMassCollectDistanceLODViewerInfoTag>();
	ModifiedTemplate.RemoveTag<FMassVisualizationProcessorTag>();
	// not needed really, since we don't add it in any of the traits but leaving here for the reference
	// ModifiedTemplate.RemoveTag<FMassStationaryISMSwitcherProcessorTag>();
}

void UInstancedActorsData::SpawnEntities()
{
	if (NumValidInstances <= 0)
	{
		// Removal modifiers or offline instance removal may have simply invalidated all InstanceTransforms
		// entries. Free up now-superfluous invalid instance transforms memory (and satisfy IsEmpty check in EndPlay)
		InstanceTransforms.Empty();

		return;
	}

	if (!ensureMsgf(EntityTemplateID.IsValid(), TEXT("No entity template generated for %s, skipping entity creation for these entities"), *ActorClass->GetPathName()))
	{
		return;
	}

	UWorld* World = GetWorld();
	check(World);

	UMassSpawnerSubsystem* MassSpawnerSubsystem = World->GetSubsystem<UMassSpawnerSubsystem>();
	check(MassSpawnerSubsystem);

	// Prepare slots for UInstancedActorsInitializerProcessor to place corresponding entity handles in.
	// Note: We can't simply use SpawnEntities returned array directly, as we only spawn entities
	//       to for `valid` InstanceTransforms. By letting UInstancedActorsInitializerProcessor store handles for
	//       spawned entities into Entities as it uses valid transforms from InstanceTransforms, we end up with an
	//       identically indexed Entities array, for things like DestroyedInstances to look up matching entities.
	Entities.Reset();
	Entities.AddDefaulted(InstanceTransforms.Num());

	FInstancedActorsMassSpawnData SpawnData;
	SpawnData.InstanceData = this;

	// Spawn entities
	UE_LOG(LogInstancedActors, Verbose, TEXT("\t%s spawning %u entities"), *GetDebugName(/*bCompact*/ true), NumValidInstances);
	FConstStructView SpawnDataView = FConstStructView::Make(SpawnData);
	TArray<FMassEntityHandle> SpawnedEntities;
	MassSpawnerSubsystem->SpawnEntities(EntityTemplateID, NumValidInstances, SpawnDataView, UInstancedActorsInitializerProcessor::StaticClass(), SpawnedEntities);
	check(SpawnedEntities.Num() == NumValidInstances);

	checkSlow(Algo::CountIf(Entities, [](const auto& EntityHandle)
		{ return EntityHandle.IsValid(); }) == NumValidInstances);

	// Now we've seeded Mass entity locations, we can free up now-superfluous InstanceTransforms
	InstanceTransforms.Empty();
}

void UInstancedActorsData::DespawnEntities()
{
	UWorld* World = GetWorld();
	check(World);

	FMassEntityManager& MassEntityManager = GetMassEntityManagerChecked();

	UInstancedActorsRepresentationSubsystem* RepresentationSubsystem = World->GetSubsystem<UInstancedActorsRepresentationSubsystem>();
	check(RepresentationSubsystem);

	AInstancedActorsManager& Manager = GetManagerChecked();
	const FVector WorldToLocalTranslation = -Manager.GetActorLocation();
	const FTransform ManagerTransform = Manager.GetActorTransform();
	const bool bApplyManagerTranslationOnly = (Manager.GetActorQuat().IsIdentity() && Manager.GetActorScale().Equals(FVector::OneVector));

	// Reconstruct InstanceTransforms from Mass entity locations, just in case BeginPlay gets
	// called again for this manager, which can happen with actor streaming.
	//
	// Note: Destroyed instances will not restore their transforms here but the initial array size and indexing will
	//       be preserved so that once we re-spawn, destroyed entities will simply be skipped.
	//       If RemoveDestroyedInstanceEntities is called again, it will see the entities have already been removed
	//       and simply skip them.
	checkf(InstanceTransforms.IsEmpty(), TEXT("Expected %s InstanceTransforms to have been cleared after having seeding ISMCs in BeginPlay"), *GetDebugName());
	checkf(NumInstances >= Entities.Num(), TEXT("%s has somehow gained more entities that it's source InstanceTransforms"), *GetDebugName());
	InstanceTransforms.SetNumZeroed(NumInstances);
	NumValidInstances = 0;

	FMassEntityQuery InstancedActorLocationQuery;
	InstancedActorLocationQuery.AddRequirement<FInstancedActorsFragment>(EMassFragmentAccess::ReadOnly);
	InstancedActorLocationQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);

	TArray<FMassArchetypeEntityCollection> EntityCollectionsToDestroy;
	UE::Mass::Utils::CreateEntityCollections(MassEntityManager, Entities, FMassArchetypeEntityCollection::NoDuplicates, EntityCollectionsToDestroy);

	FMassExecutionContext ExecutionContext(MassEntityManager);
	for (FMassArchetypeEntityCollection& Collection : EntityCollectionsToDestroy)
	{
		InstancedActorLocationQuery.ForEachEntityChunk(Collection, MassEntityManager, ExecutionContext, [this, &WorldToLocalTranslation, &ManagerTransform](FMassExecutionContext& Context)
			{
				int32 NumEntities = Context.GetNumEntities();
				TConstArrayView<FInstancedActorsFragment> InstancedActorFragments = Context.GetFragmentView<FInstancedActorsFragment>();
				TConstArrayView<FTransformFragment> TransformsList = Context.GetFragmentView<FTransformFragment>();
				check(TransformsList.GetTypeSize() == sizeof(FTransform));

				// Re-build InstanceTransforms, being careful to put transforms back into the right index it was created from
				for (int32 Index = 0; Index < NumEntities; ++Index)
				{
					FInstancedActorsInstanceIndex InstanceIndex = InstancedActorFragments[Index].InstanceIndex;
					InstanceTransforms[InstanceIndex.GetIndex()] = TransformsList[Index].GetTransform();

					checkf(UE::InstancedActors::Helpers::IsValidInstanceTransform(InstanceTransforms[InstanceIndex.GetIndex()]), TEXT("Found Mass entity with unexpected Scale 0 'invalid' transform"));

					++NumValidInstances;
				}
			});

		// Destroy all entities while we're going
		MassEntityManager.BatchDestroyEntityChunks(Collection);
	}
	Entities.Reset();

	checkSlow(NumValidInstances == Algo::CountIf(InstanceTransforms, [](const FTransform& InstanceTransform)
		{ return UE::InstancedActors::Helpers::IsValidInstanceTransform(InstanceTransform); }));

	// Convert gathered transforms back to local space
	if (bApplyManagerTranslationOnly)
	{
		for (FTransform& InstanceTransform : InstanceTransforms)
		{
			if (UE::InstancedActors::Helpers::IsValidInstanceTransform(InstanceTransform))
			{
				InstanceTransform.AddToTranslation(WorldToLocalTranslation);
			}
		}
	}
	else
	{
		for (FTransform& InstanceTransform : InstanceTransforms)
		{
			if (UE::InstancedActors::Helpers::IsValidInstanceTransform(InstanceTransform))
			{
				InstanceTransform.SetToRelativeTransform(ManagerTransform);
			}
		}
	}

	// Reset the related shared fragments. Not removing it fully since the system doesn't support it yet and we are
	// going to reuse it anyway the next time this IA Manager gets loaded

	FInstancedActorsDataSharedFragment ManagerFragment;
	ManagerFragment.InstanceData = this;
	const uint32 FragmentHash = UE::StructUtils::GetStructCrc32(FConstStructView::Make(ManagerFragment));
	FSharedStruct SharedFragmentInstance = MassEntityManager.GetOrCreateSharedFragmentByHash<FInstancedActorsDataSharedFragment>(FragmentHash, ManagerFragment);
	FInstancedActorsDataSharedFragment* AsSharedManagerFragment = SharedFragmentInstance.GetPtr<FInstancedActorsDataSharedFragment>();
	if (ensure(AsSharedManagerFragment) && AsSharedManagerFragment->InstanceData == this)
	{
		// decrement the stats before resetting
		Manager.UpdateInstanceStats(NumInstances, AsSharedManagerFragment->BulkLOD, /*Increment=*/false);

		AsSharedManagerFragment->InstanceData = nullptr;
		AsSharedManagerFragment->BulkLOD = EInstancedActorsBulkLOD::MAX;
	}

	DEC_DWORD_STAT_BY(STAT_TotalInstanceCount, NumInstances);

	// Reregister & destroy runtime ISMCs
	RemoveAllVisualizations();

	// Reset delta list, if this actor gets recycled on the server we'll get another persistence update restoring the deltas,
	// if its recycled on the client, the network shadow state is the CDO state so we'll get this replicated again from fresh.
	InstanceDeltas.Reset(/*bMarkDirty*/ false);
}

AInstancedActorsManager* UInstancedActorsData::GetManager() const
{
	if (UObject* Outer = GetOuter())
	{
		check(Outer->IsA<AInstancedActorsManager>());
		return static_cast<AInstancedActorsManager*>(Outer);
	}
	return nullptr;
}

AInstancedActorsManager& UInstancedActorsData::GetManagerChecked() const
{
	AInstancedActorsManager* Manager = GetManager();
	check(Manager);
	return *Manager;
}

UInstancedActorsData* UInstancedActorsData::GetInstanceDataForEntity(const FMassEntityManager& EntityManager, const FMassEntityHandle EntityHandle)
{
	FMassEntityView EntityView(EntityManager, EntityHandle);
	if (EntityView.IsSet())
	{
		if (FInstancedActorsFragment* InstancedActorFragment = EntityView.GetFragmentDataPtr<FInstancedActorsFragment>())
		{
			return InstancedActorFragment->InstanceData.Get();
		}
	}

	return nullptr;
}

bool UInstancedActorsData::HasSpawnedEntities() const
{
	return !Entities.IsEmpty();
}
int32 UInstancedActorsData::GetNumInstances() const
{
	// InstanceTransforms is the authoritative cooked instance data until entities are spawned, whereupon
	// we free up the now-superfluous InstanceTransfoms list and replace it with mass entities in Entities
	// @see SpawnEntities
	return HasSpawnedEntities() ? Entities.Num() : InstanceTransforms.Num();
}

int32 UInstancedActorsData::GetNumFreeInstances() const
{
	return GetNumInstances() - NumValidInstances;
}

bool UInstancedActorsData::IsValidInstance(const FInstancedActorsInstanceHandle& InstanceHandle) const
{
	if (HasSpawnedEntities())
	{
		return InstanceHandle.InstancedActorData == this && Entities.IsValidIndex(InstanceHandle.GetIndex()) && Entities[InstanceHandle.GetIndex()].IsValid();
	}
	else
	{
		return InstanceHandle.InstancedActorData == this && InstanceTransforms.IsValidIndex(InstanceHandle.GetIndex()) && UE::InstancedActors::Helpers::IsValidInstanceTransform(InstanceTransforms[InstanceHandle.GetIndex()]);
	}
}

#if WITH_EDITOR
FInstancedActorsInstanceHandle UInstancedActorsData::AddInstance(const FTransform& Transform, const bool bWorldSpace)
{
	checkf(GetManagerChecked().HasActorBegunPlay() == false, TEXT("UInstancedActorsData doesn't yet support runtime addition of instances"));

	const bool bIsValidInstanceTransform = UE::InstancedActors::Helpers::IsValidInstanceTransform(Transform);
	if (!ensureMsgf(bIsValidInstanceTransform, TEXT("Transform must have a non-zero scale. Instanced Actors use Scale = 0 to denote invalid instances")))
	{
		return FInstancedActorsInstanceHandle();
	}

	// Do we have a free / invalid index to reuse?
	int32 NewInstanceIndex = UE::InstancedActors::CVars::bRecycleInvalidInstances ? InstanceTransforms.IndexOfByPredicate([](const FTransform& InstanceTransform)
		{ return !UE::InstancedActors::Helpers::IsValidInstanceTransform(InstanceTransform); })
		: INDEX_NONE;
	if (NewInstanceIndex != INDEX_NONE)
	{
		InstanceTransforms[NewInstanceIndex] = Transform;

		for (UInstancedStaticMeshComponent* EditorPreviewISMComponent : EditorPreviewISMComponents)
		{
			EditorPreviewISMComponent->UpdateInstanceTransform(NewInstanceIndex, Transform, bWorldSpace, /*bMarkRenderStateDirty*/ true, /*bTeleport*/ true);
		}
	}
	// Add a new instance
	else
	{
		NewInstanceIndex = InstanceTransforms.Add(Transform);

		for (UInstancedStaticMeshComponent* EditorPreviewISMComponent : EditorPreviewISMComponents)
		{
			EditorPreviewISMComponent->AddInstance(Transform, bWorldSpace);
		}
	}

	++NumValidInstances;
	check(NumValidInstances <= InstanceTransforms.Num());

	if (bWorldSpace)
	{
		const FTransform& ManagerTransform = GetManagerChecked().GetActorTransform();
		InstanceTransforms[NewInstanceIndex].SetToRelativeTransform(ManagerTransform);
	}

	ensure(AssetBounds.IsValid);
	Bounds += AssetBounds.TransformBy(InstanceTransforms[NewInstanceIndex]);

	return FInstancedActorsInstanceHandle(*this, FInstancedActorsInstanceIndex(NewInstanceIndex));
}

bool UInstancedActorsData::RemoveInstance(const FInstancedActorsInstanceHandle& InstanceToRemove)
{
	if (ensureMsgf(IsValidInstance(InstanceToRemove), TEXT("Attempting to remove invalid instance (%s) from %s"), *InstanceToRemove.GetDebugName(), *GetDebugName()))
	{
		// Invalidate instance data by setting scale 0
		UE::InstancedActors::Helpers::InvalidateInstanceTransform(InstanceTransforms[InstanceToRemove.GetIndex()]);
		--NumValidInstances;
		check(NumValidInstances >= 0);

		// Hide now-free Preview ISMC instances by setting transform with 0 scale
		const static FTransform ScaleZeroTransform(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector);
		for (UInstancedStaticMeshComponent* EditorPreviewISMComponent : EditorPreviewISMComponents)
		{
			EditorPreviewISMComponent->UpdateInstanceTransform(InstanceToRemove.GetIndex(), ScaleZeroTransform, /*bWorldSpace*/ false, /*bMarkRenderStateDirty*/ true, /*bTeleport*/ true);
		}

		// Update bounds
		ensure(AssetBounds.IsValid);
		Bounds.Init();
		for (const FTransform& InstanceTransform : InstanceTransforms)
		{
			if (UE::InstancedActors::Helpers::IsValidInstanceTransform(InstanceTransform))
			{
				Bounds += AssetBounds.TransformBy(InstanceTransform);
			}
		}

		return true;
	}

	return false;
}

bool UInstancedActorsData::SetInstanceTransform(const FInstancedActorsInstanceHandle& InstanceHandle, const FTransform& Transform, const bool bWorldSpace)
{
	checkf(GetManagerChecked().HasActorBegunPlay() == false, TEXT("UInstancedActorsData doesn't yet support runtime transformation of instances"));

	const bool bIsValidInstanceTransform = UE::InstancedActors::Helpers::IsValidInstanceTransform(Transform);
	if (!ensureMsgf(bIsValidInstanceTransform, TEXT("Transform must have a non-zero scale. Instanced Actors use Scale = 0 to denote invalid instances")))
	{
		return false;
	}

	for (UInstancedStaticMeshComponent* EditorPreviewISMComponent : EditorPreviewISMComponents)
	{
		EditorPreviewISMComponent->UpdateInstanceTransform(InstanceHandle.GetIndex(), Transform, bWorldSpace, /*bMarkRenderStateDirty*/ true, /*bTeleport*/ true);
	}

	InstanceTransforms[InstanceHandle.GetIndex()] = Transform;
	if (bWorldSpace)
	{
		const FTransform& ManagerTransform = GetManagerChecked().GetActorTransform();
		InstanceTransforms[InstanceHandle.GetIndex()].SetToRelativeTransform(ManagerTransform);
	}

	// Update bounds
	ensure(AssetBounds.IsValid);
	Bounds.Init();
	for (const FTransform& InstanceTransform : InstanceTransforms)
	{
		if (UE::InstancedActors::Helpers::IsValidInstanceTransform(InstanceTransform))
		{
			Bounds += AssetBounds.TransformBy(InstanceTransform);
		}
	}

	return true;
}
#endif // WITH_EDITOR

FInstancedActorsInstanceIndex UInstancedActorsData::GetInstanceIndexForEntity(const FMassEntityHandle EntityHandle) const
{
	FMassEntityManager& EntityManager = GetMassEntityManagerChecked();
	if (ensure(EntityManager.IsEntityValid(EntityHandle)))
	{
		const FInstancedActorsFragment* InstancedActorFragment = EntityManager.GetFragmentDataPtr<FInstancedActorsFragment>(EntityHandle);
		if (ensure(InstancedActorFragment != nullptr))
		{
			return InstancedActorFragment->InstanceIndex;
		}
	}

	return FInstancedActorsInstanceIndex();
}

FMassEntityHandle UInstancedActorsData::GetEntity(FInstancedActorsInstanceIndex InstanceIndex) const
{
	if (Entities.IsValidIndex(InstanceIndex.GetIndex()))
	{
		return Entities[InstanceIndex.GetIndex()];
	}
	return FMassEntityHandle();
}

void UInstancedActorsData::SetSharedInstancedActorDataStruct(FSharedStruct InSharedStruct)
{
	checkf(SharedInstancedActorDataStruct.IsValid() == false || SharedInstancedActorDataStruct.Identical(&InSharedStruct, 0), TEXT("We don't expect to override this value"));
	checkf(InSharedStruct.GetPtr<FInstancedActorsDataSharedFragment>(), TEXT("We expect only FInstancedActorsDataSharedFragment-base types here"));

	SharedInstancedActorDataStruct = InSharedStruct;
}

FString UInstancedActorsData::GetDebugName(bool bCompact) const
{
	if (bCompact)
	{
		return FString::Printf(TEXT("%u %s (%p)"), ID, *ActorClass->GetName(), this);
	}
	else
	{
		return FString::Printf(TEXT("%s : %u (%s)"), *GetManagerChecked().GetPathName(), ID, *ActorClass->GetName());
	}
}

void UInstancedActorsData::PostDuplicate(EDuplicateMode::Type DuplicateMode)
{
	Super::PostDuplicate(DuplicateMode);

#if WITH_EDITORONLY_DATA
	if (DuplicateMode == EDuplicateMode::PIE)
	{
		// Destroy editor preview ISMCs in PIE
		for (UInstancedStaticMeshComponent* EditorPreviewISMComponent : EditorPreviewISMComponents)
		{
			if (IsValid(EditorPreviewISMComponent))
			{
				EditorPreviewISMComponent->DestroyComponent();
			}
		}
		EditorPreviewISMComponents.Empty();
	}
#endif
}

void UInstancedActorsData::PostLoad()
{
	Super::PostLoad();

	NumInstances = InstanceTransforms.Num();

	if (!Bounds.IsValid)
	{
		// Update bounds for InstanceData saved prior to addition of Bounds property
		if (NumValidInstances > 0)
		{
			FBox MeshBounds = AInstancedActorsManager::CalculateBounds(ActorClass);
			ensure(MeshBounds.IsValid);

			Bounds.Init();
			for (const FTransform& InstanceTransform : InstanceTransforms)
			{
				if (UE::InstancedActors::Helpers::IsValidInstanceTransform(InstanceTransform))
				{
					Bounds += MeshBounds.TransformBy(InstanceTransform);
				}
			}
		}
		else
		{
			// Note: UInstancedActorsData::Bounds mustn't be 0 sized by default, otherwise the first AddInstance
			//       woulf stretch the bounds from origin - instead we want the first AddInstance to init the bounds.
			Bounds = FBox(FVector::ZeroVector, FVector::ZeroVector);
		}
	}
	ensure(Bounds.IsValid);

#if WITH_EDITORONLY_DATA
	UWorld* World = GetWorld();
	if (World && World->IsGameWorld())
	{
		// Destroy editor preview ISMCs in game worlds
		for (UInstancedStaticMeshComponent* EditorPreviewISMComponent : EditorPreviewISMComponents)
		{
			if (IsValid(EditorPreviewISMComponent))
			{
				EditorPreviewISMComponent->DestroyComponent();
			}
		}
		EditorPreviewISMComponents.Empty();
	}

	// Cache asset bounds for use during instance population for IAD's saved before
	// AssetBounds was marked non-transient.
	if (!AssetBounds.IsValid)
	{
		AssetBounds = AInstancedActorsManager::CalculateBounds(ActorClass);
	}
	ensure(AssetBounds.IsValid);
#endif

	//check(CompressedInstanceTransforms.IsEmpty());

	// Prepare instance delta list for replication and persistence
	InstanceDeltas.Initialize(*this);
}

void UInstancedActorsData::UpdateCullDistance()
{
	// NOTE: Computations are done in float but should be moved to FVector::FReal
	//		 Kept in floats because SetCullDistances, SetCachedMaxDrawDistance and ComputeBoundsDrawDistance still all use float based values and these are used to compute & set the values

	MaxDrawDistance = FLT_MAX;
	LowLODDrawDistance = FLT_MAX;

	double SettingsMaxInstanceDistance = FInstancedActorsSettings::DefaultMaxInstanceDistance;
	float LODDistanceScale = 1.0f;
	double GlobalMaxInstanceDistanceScale = 1.0;

	const FInstancedActorsSettings* Settings = GetSettingsPtr<const FInstancedActorsSettings>();
	if (Settings)
	{
		Settings->ComputeLODDistanceData(SettingsMaxInstanceDistance, GlobalMaxInstanceDistanceScale, LODDistanceScale);
	}

	for (FInstancedActorsVisualizationInfo& Visualization : InstanceVisualizations)
	{
		for (UInstancedStaticMeshComponent* ISMComponent : Visualization.ISMComponents)
		{
			if (ISMComponent->GetStaticMesh() == nullptr)
			{
				continue;
			}

			// Compute the fixed distance culled max draw distance - can't be tweaked
			// (@TODO: compute average scale of all instances?)
			const FStaticMeshRenderData* RenderData = ISMComponent->GetStaticMesh()->GetRenderData();
			if (RenderData == nullptr || RenderData->LODResources.Num() == 0)
			{
				continue;
			}

			const FVector AverageScale = FVector::OneVector;
			const FBoxSphereBounds ScaledBounds = RenderData->Bounds.TransformBy(FTransform(FRotator::ZeroRotator, FVector::ZeroVector, AverageScale));
			//const float DistanceCulledMaxDrawDistance = AFortWorldSettings::CalculateInterpolatedCullDistance(WorldSettings, ScaledBounds.SphereRadius);

			// Does the static mesh qualify for far distance rendering?
			// NOTE: currently using max triangle count of lowest LOD until all the Settings objects are setup correctly
			const int32 FarDistanceMaxTriangleCount = UE::InstancedActors::CVars::LowLODMaxTriangleCountForFarDistanceRendering;
			const int32 MaxLODIndex = RenderData->LODResources.Num() - 1;
			int32 LowLODTriangleCount = 0;
			for (int32 SectionIndex = 0; SectionIndex < RenderData->LODResources[MaxLODIndex].Sections.Num(); ++SectionIndex)
			{
				LowLODTriangleCount += RenderData->LODResources[MaxLODIndex].Sections[SectionIndex].NumTriangles;
			}

			bool bScreenSizeDistanceCulled = LowLODTriangleCount > FarDistanceMaxTriangleCount || !UE::InstancedActors::CVars::bEnableFarDistanceRendering;
			if (Settings && Settings->bOverride_bDisableAutoDistanceCulling)
			{
				bScreenSizeDistanceCulled = !Settings->bDisableAutoDistanceCulling;
			}

			// Set the correct max draw distance depending if the component is distance culled or not
			float ISMMaxDrawDistance = /*bScreenSizeDistanceCulled ? DistanceCulledMaxDrawDistance : */UE::InstancedActors::CVars::MaxDrawDistance;

			// Clamp to per instance max draw distance as well setup by the settings
			if (Settings && Settings->bOverride_MaxInstanceDistances)
			{
				ISMMaxDrawDistance = SettingsMaxInstanceDistance > 0 ? SettingsMaxInstanceDistance : UE::InstancedActors::CVars::MaxDrawDistance;
			}

			ISMMaxDrawDistance *= GlobalMaxInstanceDistanceScale;

			// Force max draw distance to the global setting, ignoring setting overrides, if requested.
			ISMMaxDrawDistance = UE::InstancedActors::CVars::ForceMaxDrawDistance ? UE::InstancedActors::CVars::MaxDrawDistance : ISMMaxDrawDistance;

			// Store the culling distances values on the ISM component itself
			const float EndCullDistance = ISMMaxDrawDistance;
			const float StartCullDistance = EndCullDistance;
			ISMComponent->SetCullDistances(StartCullDistance, EndCullDistance);
			// Culling from cache max draw distance use distance to center (would need to adjust for radius as well then?)
			// This performs coarse CPU side culling first on whole ISM - but since we deactivate the ISM when beyong the max draw distance
			// this isn't really necessary and 'fixes' the wrong culling behavior
			// @todo consider whether to call the below.
			// ISMComponent->SetCachedMaxDrawDistance(MaxDrawDistance);

			ISMComponent->SetLODDistanceScale(LODDistanceScale);

			// Generate a projection matrix: ComputeBoundsDrawDistance only uses (0, 0) and (1, 1) of this matrix.
			// (these values are the same when used during computation of the screen sizes in UStaticMesh bAutoComputeLODScreenSize
			const float HalfFOV = UE_PI * 0.25f;
			const float ScreenWidth = 1920.0f;
			const float ScreenHeight = 1080.0f;
			const FPerspectiveMatrix ProjMatrix(HalfFOV, ScreenWidth, ScreenHeight, 1.0f);

			const float SphereRadius = ScaledBounds.SphereRadius;
			const float LowLODScreenSize = RenderData->ScreenSize[MaxLODIndex].GetValue();
			float ISMLowLODDrawDistance = ComputeBoundsDrawDistance(LowLODScreenSize, SphereRadius, ProjMatrix);

			// And clamp to global forced low LOD distance as well
			ISMLowLODDrawDistance = FMath::Min(ISMLowLODDrawDistance, UE::InstancedActors::CVars::ForcedLowLODStartDrawDistance);

			// Apply the override if set on the settings object
			if (Settings && Settings->bOverride_ForceLowRepresentationLODDistance)
			{
				// Take value as is or apply min to make sure we are not pushing LODs further than the auto computed value?
				ISMLowLODDrawDistance = FMath::Min(ISMLowLODDrawDistance, Settings->ForceLowRepresentationLODDistance);
			}

			// Clamp for ISM
			// Clamp to ISMMaxDrawDistance to make sure we are not pushing the low LOD further than the final draw distance
			ISMLowLODDrawDistance = FMath::Min(ISMLowLODDrawDistance, ISMMaxDrawDistance);

			// Update shared actor data distances
			MaxDrawDistance = FMath::Min(MaxDrawDistance, ISMMaxDrawDistance);
			LowLODDrawDistance = FMath::Min(LowLODDrawDistance, ISMLowLODDrawDistance);
		}
	}
}

FMassEntityManager& UInstancedActorsData::GetMassEntityManagerChecked() const
{
	return GetManagerChecked().GetMassEntityManagerChecked();
}

void UInstancedActorsData::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams RepParams;
	RepParams.bIsPushBased = true;

	DOREPLIFETIME_WITH_PARAMS_FAST(UInstancedActorsData, InstanceDeltas, RepParams);
}

void UInstancedActorsData::SetInstanceCurrentLifecyclePhase(FInstancedActorsInstanceIndex InstanceIndex, uint8 InCurrentLifecyclePhaseIndex)
{
	AInstancedActorsManager& Manager = GetManagerChecked();
	check(Manager.HasAuthority());

	Manager.FlushNetDormancy();

	// mz@todo IA: move this section back
	// Replicate to clients
	// Note: Lifecycle persistence saves / restores directly from / to fragments rather than the delta list in
	//		 LifecycleComponent::SerializeInstancePersistenceData
	InstanceDeltas.SetCurrentLifecyclePhaseIndex(InstanceIndex, InCurrentLifecyclePhaseIndex);
}

void UInstancedActorsData::RemoveInstanceLifecyclePhaseDelta(FInstancedActorsInstanceIndex InstanceIndex)
{
	AInstancedActorsManager& Manager = GetManagerChecked();
	check(Manager.HasAuthority());

	Manager.FlushNetDormancy();

	// mz@todo IA: move this section back
	// Replicate to clients
	// Note: Lifecycle persistence saves / restores directly from / to fragments rather than the delta list in
	//		 LifecycleComponent::SerializeInstancePersistenceData
	InstanceDeltas.RemoveLifecyclePhaseDelta(InstanceIndex);
}

void UInstancedActorsData::RemoveInstanceLifecyclePhaseTimeElapsedDelta(FInstancedActorsInstanceIndex InstanceIndex)
{
	AInstancedActorsManager& Manager = GetManagerChecked();
	check(Manager.HasAuthority());

#if WITH_SERVER_CODE

	// Clean up non-replicated delta record
	InstanceDeltas.RemoveLifecyclePhaseTimeElapsedDelta(InstanceIndex);

#endif // WITH_SERVER_CODE
}

void UInstancedActorsData::ApplyInstanceDeltas()
{
	const TArray<FInstancedActorsDelta>& Deltas = InstanceDeltas.GetInstanceDeltas();

#if WITH_INSTANCEDACTORS_DEBUG
	UE_LOG(LogInstancedActors, Verbose, TEXT("Applying %d instance deltas (D: %u, L: %u, LT: %u) to %s"), Deltas.Num(), InstanceDeltas.GetNumDestroyedInstanceDeltas(), InstanceDeltas.GetNumLifecyclePhaseDeltas(), InstanceDeltas.GetNumLifecyclePhaseTimeElapsedDeltas(), *GetDebugName());
#endif

	if (!HasSpawnedEntities())
	{
		// We may have received persistence deltas before deferred entity spawning has executed. In this case, we'll early out here and
		// apply these later in AInstancedActorsManager::InitializeModifyAndSpawnEntities
		return;
	}

	FMassEntityManager& EntityManager = GetMassEntityManagerChecked();

	TArray<FInstancedActorsInstanceIndex> EntitiesToRemove;

	for (const FInstancedActorsDelta& Delta : Deltas)
	{
		ApplyInstanceDelta(EntityManager, Delta, EntitiesToRemove);
	}

	RuntimeRemoveInstances(MakeArrayView(EntitiesToRemove));
}

void UInstancedActorsData::ApplyInstanceDeltas(TConstArrayView<int32> InstanceDeltaIndices)
{
#if WITH_INSTANCEDACTORS_DEBUG
	UE_LOG(LogInstancedActors, Verbose, TEXT("Applying %d instance deltas to %s"), InstanceDeltaIndices.Num(), *GetDebugName());
#endif

	if (!HasSpawnedEntities())
	{
		// Make sure this is only because we'll *never* spawn entities (otherwise we should have by now)
		ensureMsgf(NumValidInstances == 0, TEXT("Attempting to apply delta changes to entities before they have spawned"));
		return;
	}

	FMassEntityManager& EntityManager = GetMassEntityManagerChecked();

	TArray<FInstancedActorsInstanceIndex> EntitiesToRemove;

	const TArray<FInstancedActorsDelta>& Deltas = InstanceDeltas.GetInstanceDeltas();

	for (const FInstancedActorsDelta& Delta : Deltas)
	{
		ApplyInstanceDelta(EntityManager, Delta, EntitiesToRemove);
	}

	RuntimeRemoveInstances(MakeArrayView(EntitiesToRemove));
}

void UInstancedActorsData::RollbackInstanceDeltas(TConstArrayView<int32> InstanceDeltaIndices)
{
#if WITH_INSTANCEDACTORS_DEBUG
	UE_LOG(LogInstancedActors, Verbose, TEXT("Rolling back %d instance deltas on %s"), InstanceDeltaIndices.Num(), *GetDebugName());
#endif

	if (!ensureMsgf(HasSpawnedEntities(), TEXT("Attempting to rollback delta changes to entities before they have spawned")))
	{
		return;
	}

	FMassEntityManager& EntityManager = GetMassEntityManagerChecked();
	const TArray<FInstancedActorsDelta>& Deltas = InstanceDeltas.GetInstanceDeltas();
	for (int32 InstanceDeltaIndex : InstanceDeltaIndices)
	{
		const FInstancedActorsDelta& Delta = Deltas[InstanceDeltaIndex];

		RollbackInstanceDelta(EntityManager, Delta);
	}
}

void UInstancedActorsData::ApplyInstanceDelta(FMassEntityManager& EntityManager, const FInstancedActorsDelta& InstanceDelta, TArray<FInstancedActorsInstanceIndex>& OutEntitiesToRemove)
{
	const int32 InstanceIndex = InstanceDelta.GetInstanceIndex().GetIndex();

	// Unless NumValidInstances == 0 and we never attempted to spawn entities, which we've checked for already in
	// ApplyInstanceDeltas, Entities should be fully sized to InstanceTransforms.Num. So, if InstanceIndex is an
	// invalid index, it's for an instance outside the cooked data range
	if (!ensureMsgf(Entities.IsValidIndex(InstanceIndex), TEXT("Unexpected delta for unknown instance index %d. Have instances been removed and compacted since shipping?"), InstanceIndex))
	{
		return;
	}

	const FMassEntityHandle Entity = Entities[InstanceIndex];

	if (EntityManager.IsEntityValid(Entity))
	{
		if (InstanceDelta.IsDestroyed())
		{
			OutEntitiesToRemove.Add(FInstancedActorsInstanceIndex(InstanceIndex));
		}
	}
}

void UInstancedActorsData::RollbackInstanceDelta(FMassEntityManager& EntityManager, const FInstancedActorsDelta& InstanceDelta)
{
	// mz@todo IA: move this section back
	// Client only usage for now. If this needs to run on server at some point,
	// make sure to also update server-only LifecyclePhaseTimeFragment
	check(!GetManagerChecked().IsNetMode(NM_DedicatedServer));

	const int32 InstanceIndex = InstanceDelta.GetInstanceIndex().GetIndex();

	check(Entities.IsValidIndex(InstanceIndex));
	const FMassEntityHandle Entity = Entities[InstanceIndex];

	if (EntityManager.IsEntityValid(Entity))
	{
		ensureMsgf(!InstanceDelta.IsDestroyed(), TEXT("Attempting to rollback instance destruction. Instance destruction is currently a lossy operation and can't be reverted"));
	}
}

void UInstancedActorsData::RuntimeRemoveInstances(TConstArrayView<FInstancedActorsInstanceIndex> InstancesToRemove)
{
	if (InstancesToRemove.IsEmpty())
	{
		return;
	}

	checkSlow(GetManagerChecked().GetWorld()->IsGameWorld());

	FMassEntityManager& MassEntityManager = GetMassEntityManagerChecked();

	// Block OnInstancedActorDestroyed whilst destroying entities and their actors
	bRemovingInstances = true;

	// Destroy spawned entities if we've spawned them already
	if (HasSpawnedEntities())
	{
		// Get transient entity handles for stable instance IDs
		TArray<FMassEntityHandle> EntitiesToDestroy;
		EntitiesToDestroy.Reserve(InstancesToRemove.Num());
		for (FInstancedActorsInstanceIndex InstanceToRemove : InstancesToRemove)
		{
			if (ensure(Entities.IsValidIndex(InstanceToRemove.GetIndex())))
			{
				FMassEntityHandle& EntityToRemove = Entities[InstanceToRemove.GetIndex()];
				if (EntityToRemove.IsSet())
				{
					EntitiesToDestroy.Add(EntityToRemove);
					EntityToRemove.Reset();
				}
			}
		}
		TArray<FMassArchetypeEntityCollection> EntityCollectionsToDestroy;
		UE::Mass::Utils::CreateEntityCollections(MassEntityManager, EntitiesToDestroy, FMassArchetypeEntityCollection::NoDuplicates, EntityCollectionsToDestroy);

		// Destroy entities. This will also trigger actor destruction for any spawned actors
		// for these entities.
		for (FMassArchetypeEntityCollection& EntityCollectionToDestroy : EntityCollectionsToDestroy)
		{
			MassEntityManager.BatchDestroyEntityChunks(EntityCollectionToDestroy);
		}
	}
	// Pre-empt entity spawning and simply invalidate InstanceTransform entries, preventing them from spawning later
	else
	{
		uint16 InstancedRemoved = 0;
		for (FInstancedActorsInstanceIndex InstanceToRemove : InstancesToRemove)
		{
			if (ensure(InstanceTransforms.IsValidIndex(InstanceToRemove.GetIndex())))
			{
				UE::InstancedActors::Helpers::InvalidateInstanceTransform(InstanceTransforms[InstanceToRemove.GetIndex()]);
				++InstancedRemoved;
			}
		}
		
		NumValidInstances = FMath::Clamp(NumValidInstances - InstancedRemoved, 0u, NumValidInstances);
	}

	bRemovingInstances = false;
}

void UInstancedActorsData::RuntimeRemoveAllInstances()
{
	checkSlow(GetManagerChecked().GetWorld()->IsGameWorld());

	FMassEntityManager& MassEntityManager = GetMassEntityManagerChecked();

	// Block OnInstancedActorDestroyed whilst destroying entities and their actors
	bRemovingInstances = true;

	// Destroy spawned entities if we've spawned them already
	if (HasSpawnedEntities())
	{
		TArray<FMassArchetypeEntityCollection> EntityCollectionsToDestroy;
		UE::Mass::Utils::CreateEntityCollections(MassEntityManager, Entities, FMassArchetypeEntityCollection::NoDuplicates, EntityCollectionsToDestroy);

		// Destroy entities. This will also trigger actor destruction for any spawned actors
		// for these entities.
		for (FMassArchetypeEntityCollection& EntityCollectionToDestroy : EntityCollectionsToDestroy)
		{
			MassEntityManager.BatchDestroyEntityChunks(EntityCollectionToDestroy);
		}

		// Zero out all entity handles to 'reset' them
		FMemory::Memzero(Entities.GetData(), Entities.GetTypeSize() * Entities.Num());
	}
	// Pre-empt entity spawning and simply invalidate InstanceTransform entries, preventing them from spawning later
	else
	{
		for (FTransform& InstanceTransform : InstanceTransforms)
		{
			UE::InstancedActors::Helpers::InvalidateInstanceTransform(InstanceTransform);
		}
		NumValidInstances = 0;
	}

	bRemovingInstances = false;
}

void UInstancedActorsData::EjectInstanceActor(FInstancedActorsInstanceIndex InstanceToEject, AActor& ActorToEject)
{
	UE_LOG(LogInstancedActors, Verbose, TEXT("Ejecting instanced actor: %s"), *ActorToEject.GetPathName());

	// On both client and server: 'Unlink' ActorToEject from Mass by removing the entities reference to it so we
	// don't destroy the actor when removing the mass entity below.
	AActor* UnlinkedActor = UnlinkActor(InstanceToEject);
	ensure(UnlinkedActor == &ActorToEject);

	// Mark InstanceToEject as 'destroyed' so this manager doesn't try to spawn it in the future.
	// This will also destroy the instance's Mass entity. As we've unlinked ActorToEject above,
	// it will be ignored / left spawned as-is, despite the entity destruction.
	// Note: On clients this just removes the instance for client 'prediction' of the destruction
	// which we want.
	DestroyInstance(InstanceToEject);
}

void UInstancedActorsData::SetReplicatedActor(FInstancedActorsInstanceIndex Instance, AActor& ReplicatedActor)
{
	checkf(!ReplicatedActor.HasAuthority(), TEXT("SetReplicatedActor must only be called on clients where we explictly connect actor to entities via UInstancedActorsComponent::OnRep_InstanceHandle"));

	if (!ensure(Entities.IsValidIndex(Instance.GetIndex())))
	{
		return;
	}

	const FMassEntityHandle& EntityHandle = Entities[Instance.GetIndex()];
	if (!ensure(EntityHandle.IsValid()))
	{
		return;
	}

	FMassEntityManager& EntityManager = GetMassEntityManagerChecked();
	if (!ensure(EntityManager.IsEntityValid(EntityHandle)))
	{
		return;
	}

	FMassActorFragment* ActorFragment = EntityManager.GetFragmentDataPtr<FMassActorFragment>(EntityHandle);
	if (!ensure(ActorFragment))
	{
		return;
	}

	// If an instance's actor is despawned and respawned in quick succession, due to out of order replication we may receive the new actor
	// before we've received the EndPlay for the previous one. In this case we call ResetAndUpdateHandleMap here early for the
	// previous actor, as in ClearReplicatedActor
	if (ActorFragment->IsValid() && ActorFragment->Get() != &ReplicatedActor)
	{
		ActorFragment->ResetAndUpdateHandleMap();

		check(!ActorFragment->IsValid());
	}

	// Set the new actor reference
	if (ActorFragment->Get() != &ReplicatedActor)
	{
		ActorFragment->SetAndUpdateHandleMap(EntityHandle, &ReplicatedActor, /*bIsOwnedByMass*/ false);
	}

	// Mark Instance's representation explicitly dirty to ensure it's ISMC is removed in favor of the new ReplicatedActor, even if it is in
	// a non-detailed bulk LOD
	if (UInstancedActorsSubsystem* InstancedActorSubsystem = GetManagerChecked().GetInstancedActorSubsystem())
	{
		FInstancedActorsInstanceHandle InstanceHandle(*this, Instance);
		InstancedActorSubsystem->MarkInstanceRepresentationDirty(InstanceHandle);
	}
}

void UInstancedActorsData::ClearReplicatedActor(FInstancedActorsInstanceIndex Instance, AActor& ExpectedActor)
{
	checkf(!GetManagerChecked().HasAuthority(), TEXT("ClearReplicatedActor must only be called on clients where we explictly disconnect actors from entities via UInstancedActorsComponent::EndPlay"));

	// Don't perform entity ensures here to allow for replicated actor destruction to occur after entity destruction
	if (!Entities.IsValidIndex(Instance.GetIndex()))
	{
		return;
	}

	const FMassEntityHandle& EntityHandle = Entities[Instance.GetIndex()];
	if (!EntityHandle.IsValid())
	{
		return;
	}

	FMassEntityManager& EntityManager = GetMassEntityManagerChecked();
	if (!EntityManager.IsEntityValid(EntityHandle))
	{
		return;
	}

	// If the entity is still valid though, we should still have an actor fragment
	FMassActorFragment* ActorFragment = EntityManager.GetFragmentDataPtr<FMassActorFragment>(EntityHandle);
	if (!ensure(ActorFragment))
	{
		return;
	}

	// Note: Due to out of order replication, we may have already received a new replicated actor before the EndPlay -> ClearReplicatedActor
	// for ExpectedActor so we can't ensure here for ActorFragment->Get == ExpectedActor as it may legitimately be set to another actor.
	// In this case, we will have already called ResetAndUpdateHandleMap in SetReplicatedActor when receiving the new ReplicatedActor and
	// seeing that
	if (LIKELY(ActorFragment->Get(FMassActorFragment::EActorAccess::IncludeUnreachable) == &ExpectedActor))
	{
		ActorFragment->ResetAndUpdateHandleMap();
	}
#if WITH_INSTANCEDACTORS_DEBUG
	else
	{
		// Ensure ExpectedActor really was previously removed from Mass
		UWorld* World = GetManagerChecked().GetWorld();
		check(World);
		UMassActorSubsystem* MassActorSubsystem = World->GetSubsystem<UMassActorSubsystem>();
		if (ensure(MassActorSubsystem))
		{
			FMassEntityHandle ExpectedActorEntityHandle = MassActorSubsystem->GetEntityHandleFromActor(&ExpectedActor);
			ensure(!ExpectedActorEntityHandle.IsValid());
		}
	}
#endif

	// Mark Instance's representation explicitly dirty to ensure an ISMC is instanced to replace ExpectedActor's removal, even if it is in
	// a non-detailed bulk LOD
	if (UInstancedActorsSubsystem* InstancedActorSubsystem = GetManagerChecked().GetInstancedActorSubsystem())
	{
		FInstancedActorsInstanceHandle InstanceHandle(*this, Instance);
		InstancedActorSubsystem->MarkInstanceRepresentationDirty(InstanceHandle);
	}
}

AActor* UInstancedActorsData::UnlinkActor(FInstancedActorsInstanceIndex InstanceToUnlink)
{
	// Get InstanceToUnlink's actor from Mass (if any)
	if (!ensure(Entities.IsValidIndex(InstanceToUnlink.GetIndex())))
	{
		return nullptr;
	}

	const FMassEntityHandle& EntityHandle = Entities[InstanceToUnlink.GetIndex()];
	if (!ensure(EntityHandle.IsValid()))
	{
		return nullptr;
	}

	FMassEntityManager& EntityManager = GetMassEntityManagerChecked();
	FMassEntityView EntityView = FMassEntityView::TryMakeView(EntityManager, EntityHandle);
	if (!ensure(EntityView.IsValid()))
	{
		return nullptr;
	}

	FMassActorFragment* ActorFragment = EntityView.GetFragmentDataPtr<FMassActorFragment>();
	if (!ensure(ActorFragment))
	{
		return nullptr;
	}

	AActor* Actor = ActorFragment->GetMutable();
	if (!IsValid(Actor))
	{
		return nullptr;
	}

	// Disconnect Actor from Mass
	ActorFragment->ResetAndUpdateHandleMap();

	// Cleanup any previous delegate subscriptions
	const FMassRepresentationParameters& RepresentationParams = EntityView.GetConstSharedFragmentData<FMassRepresentationParameters>();
	UInstancedActorsRepresentationActorManagement* RepresentationActorManagement = Cast<UInstancedActorsRepresentationActorManagement>(RepresentationParams.CachedRepresentationActorManagement);
	if (ensure(RepresentationActorManagement))
	{
		check(Actor);
		RepresentationActorManagement->OnActorUnlinked(*Actor);
	}

	return Actor;
}

uint8 UInstancedActorsData::AllocateVisualization()
{
	// Reuse free or create new InstanceVisualizations entry
	int32 AllocatedVisualizationIndex = InstanceVisualizationAllocationFlags.FindAndSetFirstZeroBit();
	if (AllocatedVisualizationIndex == INDEX_NONE)
	{
		AllocatedVisualizationIndex = InstanceVisualizations.AddDefaulted();
		InstanceVisualizationAllocationFlags.Add(true);

		checkf(AllocatedVisualizationIndex < TNumericLimits<uint8>::Max(), TEXT("No more than UINT8_MAX - 1 InstanceVisualizations can be defined for a single UInstancedActorsData as uint8 is used to index InstanceVisualizations and UINT8_MAX is reserved for invalid index"));
	}

	check(AllocatedVisualizationIndex != INDEX_NONE);
	check(InstanceVisualizations.IsValidIndex(AllocatedVisualizationIndex));
	check(InstanceVisualizations.Num() == InstanceVisualizationAllocationFlags.Num());
	check(InstanceVisualizationAllocationFlags[AllocatedVisualizationIndex] == true);

	return static_cast<uint8>(AllocatedVisualizationIndex);
}

void UInstancedActorsData::InitializeVisualization(uint8 AllocatedVisualizationIndex, const FInstancedActorsVisualizationDesc& VisualizationDesc)
{
	check(InstanceVisualizations.IsValidIndex(AllocatedVisualizationIndex));
	check(InstanceVisualizationAllocationFlags.IsValidIndex(AllocatedVisualizationIndex));
	check(InstanceVisualizationAllocationFlags[AllocatedVisualizationIndex] == true);
	FInstancedActorsVisualizationInfo& NewVisualization = InstanceVisualizations[AllocatedVisualizationIndex];

	// Copy descriptor
	NewVisualization.VisualizationDesc = VisualizationDesc;

	// Create ISMC's for descriptor
	AInstancedActorsManager& Manager = GetManagerChecked();
	Manager.CreateISMComponents(NewVisualization.VisualizationDesc, SharedSettings, NewVisualization.ISMComponents);
	Manager.RegisterInstanceDatasComponents(*this, NewVisualization.ISMComponents);

	// Register ISMC's with Mass & update their culling settings
	if (NewVisualization.ISMComponents.Num() >= 1)
	{
		INC_DWORD_STAT_BY(STAT_RegisteredISMs, NewVisualization.ISMComponents.Num());

		// Note these visual settings should not ultimately be used, as we are pre-creating our own
		// ISMCs in CreateISMComponents, using FISMComponentDescriptor's. We still complete
		// this description as best we can regardless to ensure Mass can't get confused, just in case.
		FStaticMeshInstanceVisualizationDesc MassVisualizationDesc = VisualizationDesc.ToMassVisualizationDesc();

		UWorld* World = Manager.GetWorld();
		check(World);
		UInstancedActorsRepresentationSubsystem* RepresentationSubsystem = World->GetSubsystem<UInstancedActorsRepresentationSubsystem>();
		check(RepresentationSubsystem);

		NewVisualization.MassStaticMeshDescHandle = RepresentationSubsystem->AddVisualDescWithISMComponents(MassVisualizationDesc, NewVisualization.ISMComponents);

		ensureMsgf(NewVisualization.MassStaticMeshDescHandle.IsValid(), TEXT("Couldn't register instance visual description for %s InstanceVisualizations[%d]"), *GetDebugName(), InstanceVisualizations.Num() - 1);

		UpdateCullDistance();
	}
}

uint8 UInstancedActorsData::AddVisualization(const FInstancedActorsVisualizationDesc& VisualizationDesc)
{
	// Reuse free or create new InstanceVisualizations entry
	uint8 NewVisualizationIndex = AllocateVisualization();

	// Init new visualization
	InitializeVisualization(NewVisualizationIndex, VisualizationDesc);

	return NewVisualizationIndex;
}

uint8 UInstancedActorsData::AddVisualizationAsync(const FInstancedActorsSoftVisualizationDesc& SoftVisualizationDesc)
{
	// See if we have any assets to load
	TArray<FSoftObjectPath> VisualizationAssetsToLoad;
	SoftVisualizationDesc.GetAssetsToLoad(VisualizationAssetsToLoad);

	// Immediately load assets if forced or no assets to async load anyway
	if (UE::InstancedActors::CVars::bForceSyncLoadVisualizations || VisualizationAssetsToLoad.IsEmpty())
	{
		// Nothing needing to load. Resolve hard visualization description and add immediately.
		FInstancedActorsVisualizationDesc VisualizationDesc(SoftVisualizationDesc);
		const uint8 NewVisualizationIndex = AddVisualization(VisualizationDesc);
		return NewVisualizationIndex;
	}
	// Async load VisualizationAssetsToLoad
	else
	{
		// 'Reserve' free or create new InstanceVisualizations entry so we can return the visualization index we're going to async load into
		uint8 ReservedVisualizationIndex = AllocateVisualization();

		check(InstanceVisualizations.IsValidIndex(ReservedVisualizationIndex));
		FInstancedActorsVisualizationInfo& ReservedVisualization = InstanceVisualizations[ReservedVisualizationIndex];

		// Async load assets and complete initialization when load completes
		ReservedVisualization.AssetLoadHandle = UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(VisualizationAssetsToLoad, FStreamableDelegate::CreateWeakLambda(this, [this, SoftVisualizationDesc, ReservedVisualizationIndex]()
			{
				// Check visualization is still allocated
				if (!ensure(InstanceVisualizations.IsValidIndex(ReservedVisualizationIndex)))
				{
					return;
				}
				check(InstanceVisualizationAllocationFlags.IsValidIndex(ReservedVisualizationIndex));
				if (!ensure(InstanceVisualizationAllocationFlags[ReservedVisualizationIndex] == true))
				{
					return;
				}
				FInstancedActorsVisualizationInfo& Visualization = InstanceVisualizations[ReservedVisualizationIndex];

				// Resolve hard visualization description
				FInstancedActorsVisualizationDesc VisualizationDesc(SoftVisualizationDesc);

				// Init reserved visualization
				InitializeVisualization(ReservedVisualizationIndex, VisualizationDesc);

				Visualization.AssetLoadHandle.Reset(); 
			}));

		return ReservedVisualizationIndex;
	}
}

void UInstancedActorsData::ForEachVisualization(TFunctionRef<bool(uint8 /*VisualizationIndex*/, const FInstancedActorsVisualizationInfo& /*Visualization*/)> InFunction, const bool bSkipAsyncLoadingVisualizations) const
{
	// Iterate allocated visualizations
	for (TConstSetBitIterator<> AllocatedVisualizationsIt(InstanceVisualizationAllocationFlags); AllocatedVisualizationsIt; ++AllocatedVisualizationsIt)
	{
		uint8 VisualizationIndex = static_cast<uint8>(AllocatedVisualizationsIt.GetIndex());

		check(InstanceVisualizations.IsValidIndex(VisualizationIndex));
		const FInstancedActorsVisualizationInfo& Visualization = InstanceVisualizations[VisualizationIndex];

		if (bSkipAsyncLoadingVisualizations && UNLIKELY(Visualization.IsAsyncLoading()))
		{
			continue;
		}

		const bool bContinue = InFunction(VisualizationIndex, Visualization);
		if (!bContinue)
		{
			break;
		}
	}
}

void UInstancedActorsData::SwitchInstanceVisualization(FInstancedActorsInstanceIndex InstanceToSwitch, uint8 NewVisualizationIndex)
{
	if (!ensure(InstanceVisualizations.IsValidIndex(NewVisualizationIndex)) || !ensure(Entities.IsValidIndex(InstanceToSwitch.GetIndex())))
	{
		return;
	}

	FMassEntityManager& MassEntityManager = GetMassEntityManagerChecked();
	const FMassEntityHandle& EntityHandle = Entities[InstanceToSwitch.GetIndex()];
	if (!ensure(MassEntityManager.IsEntityValid(EntityHandle)))
	{
		return;
	}

	const FInstancedActorsVisualizationInfo& NewVisualization = InstanceVisualizations[NewVisualizationIndex];

	FInstancedActorsMeshSwitchFragment MeshSwitchFragment;
	MeshSwitchFragment.NewStaticMeshDescHandle = NewVisualization.MassStaticMeshDescHandle;

	MassEntityManager.Defer().PushCommand<FMassCommandAddFragmentInstances>(EntityHandle, MeshSwitchFragment);
}

void UInstancedActorsData::RemoveVisualization(uint8 VisualizationIndex)
{
	if (!ensure(InstanceVisualizations.IsValidIndex(VisualizationIndex)))
	{
		return;
	}

	check(InstanceVisualizationAllocationFlags.IsValidIndex(VisualizationIndex));
	const bool bIsAllocated = InstanceVisualizationAllocationFlags[VisualizationIndex];
	if (!ensure(bIsAllocated))
	{
		return;
	}

	FInstancedActorsVisualizationInfo& RemovedVisualization = InstanceVisualizations[VisualizationIndex];

	// Cancel async loading
	if (RemovedVisualization.AssetLoadHandle.IsValid())
	{
		RemovedVisualization.AssetLoadHandle->CancelHandle();
		RemovedVisualization.AssetLoadHandle.Reset();
	}

	// Deregister ISMCs from Mass and destroy
	if (RemovedVisualization.MassStaticMeshDescHandle.IsValid())
	{
		AInstancedActorsManager& Manager = GetManagerChecked();
		UWorld* World = Manager.GetWorld();
		check(World);
		UInstancedActorsRepresentationSubsystem* RepresentationSubsystem = World->GetSubsystem<UInstancedActorsRepresentationSubsystem>();
		check(RepresentationSubsystem);
		RepresentationSubsystem->RemoveVisualDesc(RemovedVisualization.MassStaticMeshDescHandle);

		for (UInstancedStaticMeshComponent* ISMComponent : RemovedVisualization.ISMComponents)
		{
			if (ensure(IsValid(ISMComponent)))
			{
				Manager.UnregisterInstanceDatasComponent(*ISMComponent);
				ISMComponent->DestroyComponent();
				DEC_DWORD_STAT(STAT_RegisteredISMs);
			}
		}
	}

	// Reset RemovedVisualization and set it's allocation flag to false, allowing for reuse in subsequent AddVisualization calls
	RemovedVisualization = FInstancedActorsVisualizationInfo();
	check(InstanceVisualizationAllocationFlags.IsValidIndex(VisualizationIndex));
	InstanceVisualizationAllocationFlags[VisualizationIndex] = false;
}

void UInstancedActorsData::RemoveAllVisualizations()
{
	for (uint8 VisualizationIndex = 0; VisualizationIndex < InstanceVisualizations.Num(); ++VisualizationIndex)
	{
		RemoveVisualization(VisualizationIndex);
	}
	check(!InstanceVisualizationAllocationFlags.Contains(true));

	InstanceVisualizations.Reset();
	InstanceVisualizationAllocationFlags.Reset();
}

void UInstancedActorsData::DestroyInstance(FInstancedActorsInstanceIndex InstanceToDestroy)
{
	AInstancedActorsManager& Manager = GetManagerChecked();
	if (Manager.HasAuthority())
	{
		Manager.FlushNetDormancy();

		// Replicate instance destruction to client via InstanceDeltas
		InstanceDeltas.SetInstanceDestroyed(InstanceToDestroy);

		// Remove instance locally on authority
		RuntimeRemoveInstances(MakeArrayView(&InstanceToDestroy, 1));

		Manager.RequestPersistentDataSave();
	}
	else
	{
		// On clients, we 'predict' deletion of the Mass entity to make sure an ISMC
		// instance isn't added once this actor completes destruction.
		RuntimeRemoveInstances(MakeArrayView(&InstanceToDestroy, 1));
	}
}

void UInstancedActorsData::OnInstancedActorDestroyed(AActor& DestroyedActor, const FMassEntityHandle EntityHandle)
{
	// Noop if actor destruction is coming from RemoveDestroyedInstanceEntities which already
	// handles entity destruction
	if (bRemovingInstances)
	{
		return;
	}

	FInstancedActorsInstanceIndex DestroyedInstanceIndex = GetInstanceIndexForEntity(EntityHandle);
	if (ensureMsgf(DestroyedInstanceIndex.IsValid(), TEXT("Somehow OnInstancedActorDestroyed was received for an actor which we assume was spawned by InstancedActors but %s isn't tracking this entity"), *GetDebugName()))
	{
		// Before destroying the entity, thus insta-destroying the associated actor, we release Mass' reference
		// to the actor here to prevent this from happening, rather allowing the actor to destroy itself as
		// it is reporting to do so, allowing death FX etc to be played first
		AActor* UnlinkedActor = UnlinkActor(DestroyedInstanceIndex);
		ensure(UnlinkedActor == &DestroyedActor);

		// If we got this far, destruction must have been caused by player, mark the instance for destruction.
		// Note: This happens on both client & server.
		//
		// On server the instance will be added to the persisted
		// DestroyedInstances list, removed locally and replicated to clients.
		//
		// On clients, we immediately remove the instance locally to 'predict' the replicated destruction from server.
		// Local destruction of the Mass entity ensures we don't revert to ISMC representation after the actor completes
		// destruction.
		DestroyInstance(DestroyedInstanceIndex);
	}
}

bool UInstancedActorsData::OnInstancedActorMoved(AActor& MovedActor, const FMassEntityHandle EntityHandle)
{
	const FInstancedActorsSettings& Settings = GetSettings<const FInstancedActorsSettings>();

	// Eject on moved?
	if (Settings.bEjectOnActorMoved)
	{
		FMassEntityManager& EntityManager = GetMassEntityManagerChecked();
		if (ensure(EntityManager.IsEntityValid(EntityHandle)))
		{
			FMassEntityView EntityView(EntityManager, EntityHandle);
			const FTransformFragment* TransformFragment = EntityView.GetFragmentDataPtr<FTransformFragment>();
			if (ensure(TransformFragment != nullptr))
			{
				const float SquareDistanceMoved = FVector::DistSquared(TransformFragment->GetTransform().GetLocation(), MovedActor.GetActorLocation());
				if (SquareDistanceMoved > FMath::Square(Settings.ActorEjectionMovementThreshold))
				{
					const FInstancedActorsFragment* InstancedActorFragment = EntityView.GetFragmentDataPtr<FInstancedActorsFragment>();
					if (ensure(InstancedActorFragment != nullptr))
					{
						EjectInstanceActor(InstancedActorFragment->InstanceIndex, MovedActor);

						// Actor was ejected
						return true;
					}
				}
			}
		}
	}

	// Actor wasn't ejected
	return false;
}

void UInstancedActorsData::OnPersistentDataRestored()
{
	// AInstancedActorsManager::SerializeInstancePersistenceData will have restored the InstanceDeltas list and replicated that
	// for application on clients in OnRep_InstanceDeltas, here we then apply the restored delta changes on the server.
	ApplyInstanceDeltas();
}

void UInstancedActorsData::OnRep_InstanceDeltas(TConstArrayView<int32> UpdatedInstanceDeltaIndices)
{
	ApplyInstanceDeltas(UpdatedInstanceDeltaIndices);
}

void UInstancedActorsData::OnRep_PreRemoveInstanceDeltas(TConstArrayView<int32> RemovedInstanceDeltaIndices)
{
	RollbackInstanceDeltas(RemovedInstanceDeltaIndices);
}

int32 UInstancedActorsData::GetEntityIndexFromCollisionIndex(const UInstancedStaticMeshComponent& ISMComponent, const int32 CollisionIndex) const
{
	const UInstancedActorsRepresentationSubsystem* RepresentationSubsystem = UWorld::GetSubsystem<UInstancedActorsRepresentationSubsystem>(GetWorld());
	if (!ensure(RepresentationSubsystem))
	{
		return INDEX_NONE;
	}

#if DO_COLLISION_INDEX_DEBUG
	const FMassEntityManager& EntityManager = UWorld::GetSubsystem<UMassEntitySubsystem>(GetWorld())->GetEntityManager();
	VLOG_COLLISIONINDEX(this, LogInstancedActors, Verbose, TEXT("%s collision index to entity, looking for %d"), *GetNameSafe(ActorClass), CollisionIndex);
#endif // DO_COLLISION_INDEX_DEBUG

	TArray<int32>* RelevantIdMap = nullptr;

	for (const FInstancedActorsVisualizationInfo& IAVisualization : InstanceVisualizations)
	{
		const int32 ISMComponentIndex = IAVisualization.ISMComponents.Find(const_cast<UInstancedStaticMeshComponent*>(&ISMComponent));
		if (ISMComponentIndex != INDEX_NONE)
		{
			const FMassISMCSharedData* RelevantISMCData = RepresentationSubsystem->GetISMCSharedDataForDescriptionIndex(IAVisualization.MassStaticMeshDescHandle.ToIndex());

			ensureMsgf(RelevantISMCData && RelevantISMCData->GetISMComponent() == &ISMComponent, TEXT("We never expect to hit this ensure. Failing the test indicates serious mis-setup."));

			// verify its CollisionIndexToEntityIndexMap is up to date by comparing CachedTouchCounter to the one stored in FMassISMCSharedData
			//	if not, recache
			if (RelevantISMCData && (RelevantISMCData->GetComponentInstanceIdTouchCounter() != IAVisualization.CachedTouchCounter))
			{
				QUICK_SCOPE_CYCLE_COUNTER(IA_EntityIndexCaching);
				const FMassISMCSharedData::FEntityToPrimitiveIdMap& IdMap = RelevantISMCData->GetEntityPrimitiveToIdMap();

#if DO_COLLISION_INDEX_DEBUG
				VLOG_COLLISIONINDEX(this, LogInstancedActors, Verbose, TEXT("\tRecaching map. Stored touch %d, current %d"), IAVisualization.CachedTouchCounter, RelevantISMCData->GetComponentInstanceIdTouchCounter());
				VLOG_COLLISIONINDEX(this, LogInstancedActors, Verbose, TEXT("\t\t(initial) Valid entities: %d; ISM collision map size: %d"), NumValidInstances, IdMap.Num());
				CVLOG_COLLISIONINDEX(ISMComponent.GetNumInstances() != IdMap.Num(), this, LogInstancedActors, Error, TEXT("\t\tMismatch with num ISM instances: %d"), ISMComponent.GetNumInstances());

				VLOG_COLLISIONINDEX(this, LogInstancedActors, Verbose, TEXT("\t\tISM map:"));
				for (auto Pair : IdMap)
				{
					VLOG_COLLISIONINDEX(this, LogInstancedActors, Verbose, TEXT("\t\t\t%s -> %d"), *Pair.Key.DebugGetDescription(), ISMComponent.GetInstanceIndexForId(Pair.Value));
				}
#endif // DO_COLLISION_INDEX_DEBUG

				IAVisualization.CollisionIndexToEntityIndexMap.Reset();
				int32 MaxIndex = INDEX_NONE;

				IAVisualization.CollisionIndexToEntityIndexMap.Init(INDEX_NONE, ISMComponent.GetNumInstances());

				VLOG_COLLISIONINDEX(this, LogInstancedActors, Verbose, TEXT("\t\tResulting mapping:"));
				for (int32 EntityIndex = 0; EntityIndex < Entities.Num(); ++EntityIndex)
				{
					if (Entities[EntityIndex].IsValid() == false)
					{
						VLOG_COLLISIONINDEX(this, LogInstancedActors, Verbose, TEXT("\t\t\t%d: --- "), EntityIndex);
						continue;
					}
					const FMassEntityHandle EntityHandle = Entities[EntityIndex];

					if (const FPrimitiveInstanceId* PrimitiveInstanceId = IdMap.Find(EntityHandle))
					{
						VLOG_COLLISIONINDEX(this, LogInstancedActors, Verbose, TEXT("\t\t\t%d: %s collision index: %d"), EntityIndex, *EntityHandle.DebugGetDescription(), ISMComponent.GetInstanceIndexForId(*PrimitiveInstanceId));

						const int32 InstanceIndex = ISMComponent.GetInstanceIndexForId(*PrimitiveInstanceId);
						check(InstanceIndex >= 0);
						MaxIndex = FMath::Max(MaxIndex, InstanceIndex);
						if (IAVisualization.CollisionIndexToEntityIndexMap.Num() <= InstanceIndex)
						{
							const int32 FirstNewIndex = IAVisualization.CollisionIndexToEntityIndexMap.Num();
							IAVisualization.CollisionIndexToEntityIndexMap.AddUninitialized(InstanceIndex - IAVisualization.CollisionIndexToEntityIndexMap.Num() + 1);
							// skipping the last one since we're going to set it in a moment anyway
							for (int32 NewElementIndex = FirstNewIndex; NewElementIndex < IAVisualization.CollisionIndexToEntityIndexMap.Num() - 1; ++NewElementIndex)
							{
								IAVisualization.CollisionIndexToEntityIndexMap[NewElementIndex] = INDEX_NONE;
							}
						}
						IAVisualization.CollisionIndexToEntityIndexMap[InstanceIndex] = EntityIndex;
					}
#if DO_COLLISION_INDEX_DEBUG
					else
					{
						if (const bool bIsValidEntity = EntityManager.IsEntityValid(EntityHandle))
						{
							const FMassActorFragment* ActorInfo = EntityManager.GetFragmentDataPtr<FMassActorFragment>(EntityHandle);
							if (const bool bIsActor = ActorInfo && (ActorInfo->Get() != nullptr))
							{
								VLOG_COLLISIONINDEX(this, LogInstancedActors, VeryVerbose, TEXT("\t\t\t%d: %s collision index: none, is an ACTOR"), EntityIndex, *EntityHandle.DebugGetDescription());
							}
							else
							{
								VLOG_COLLISIONINDEX(this, LogInstancedActors, Error, TEXT("\t\t\t%d: %s collision index: NOT FOUND!"), EntityIndex, *EntityHandle.DebugGetDescription());
							}
						}
						else
						{
							VLOG_COLLISIONINDEX(this, LogInstancedActors, Warning, TEXT("\t\t\t%d: %s NO collision index - STALE handle"), EntityIndex, *EntityHandle.DebugGetDescription());
						}
					}
#endif // DO_COLLISION_INDEX_DEBUG
				}

				// we're pruning the array since with time there might be a big discrepancy between the original and
				// current number of instances (1000s vs 10s) and the memory wasted to always keep the large arrays
				// can quickly sum up to megabytes.
				IAVisualization.CollisionIndexToEntityIndexMap.SetNum(MaxIndex + 1);
				IAVisualization.CachedTouchCounter = RelevantISMCData->GetComponentInstanceIdTouchCounter();
			}

			RelevantIdMap = &IAVisualization.CollisionIndexToEntityIndexMap;
			break;
		}
	}

	// there's a valid case when CollisionIndex is not a valid index, namely when it would be one of the last empty
	// entries in the IAVisualization.CollisionIndexToEntityIndexMap created above - the SetNum call would cut out those
	// invalid trailing indices.
	// Regardless of the case, the CollisionIndex end up being associated with INDEX_NONE in some not-so-rare edge cases,
	// where physics hit result is being used just after actor/instance destruction (often as a result of handling of the
	// very same hit event).
	if (!ensure(RelevantIdMap) || (RelevantIdMap->IsValidIndex(CollisionIndex) == false))
	{
		return INDEX_NONE;
	}
	const int32 EntityIndex = (*RelevantIdMap)[CollisionIndex];

	VLOG_COLLISIONINDEX(this, LogInstancedActors, Verbose, TEXT("Found EntityIndex  %d"), EntityIndex);

	return EntityIndex;
}
