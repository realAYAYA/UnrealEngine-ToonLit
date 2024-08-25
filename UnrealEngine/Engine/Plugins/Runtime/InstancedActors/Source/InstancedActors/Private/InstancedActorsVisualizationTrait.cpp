// Copyright Epic Games, Inc. All Rights Reserved.

#include "InstancedActorsVisualizationTrait.h"
#include "InstancedActorsData.h"
#include "InstancedActorsManager.h"
#include "InstancedActorsRepresentationActorManagement.h"
#include "InstancedActorsRepresentationSubsystem.h"
#include "InstancedActorsSettingsTypes.h"
#include "InstancedActorsTypes.h"
#include "MassEntityTemplateRegistry.h"
#include "MassCommonFragments.h"
#include "MassActorSubsystem.h"


UInstancedActorsVisualizationTrait::UInstancedActorsVisualizationTrait(const FObjectInitializer& ObjectInitializer)
{
	Params.RepresentationActorManagementClass = UInstancedActorsRepresentationActorManagement::StaticClass();
	Params.LODRepresentation[EMassLOD::High] = EMassRepresentationType::HighResSpawnedActor;
	Params.LODRepresentation[EMassLOD::Medium] = EMassRepresentationType::HighResSpawnedActor;
	// @todo Re-enable this with proper handling of Enable / Disable 'kept' actors, in 
	// UInstancedActorsRepresentationActorManagement::SetActorEnabled, including replication to client.
	Params.bKeepLowResActors = false;

	bAllowServerSideVisualization = true;
	RepresentationSubsystemClass = UInstancedActorsRepresentationSubsystem::StaticClass();

	bRegisterStaticMeshDesc = false;
}

void UInstancedActorsVisualizationTrait::InitializeFromInstanceData(UInstancedActorsData& InInstanceData)
{
	InstanceData = &InInstanceData;

	HighResTemplateActor = InstanceData->ActorClass;

	const bool bIsDedicatedServer = InInstanceData.GetManagerChecked().IsNetMode(NM_DedicatedServer);
	if (!bIsDedicatedServer)
	{
		// Don't attempt to spawn actors natively on clients. Instead, rely on bForceActorRepresentationForExternalActors to 
		// switch to Actor representation once replicated actors are set explicitly in UInstancedActorsData::SetReplicatedActor 
		// and maintain this until the actor is destroyed by the server, whereupon we'll fall back to StaticMeshInstance
		// again.
		Params.LODRepresentation[EMassLOD::High] = EMassRepresentationType::StaticMeshInstance;
		Params.LODRepresentation[EMassLOD::Medium] = EMassRepresentationType::StaticMeshInstance;
		Params.bForceActorRepresentationForExternalActors = true;
	}

	const FInstancedActorsSettings& Settings = InstanceData->GetSettings<const FInstancedActorsSettings>();
	
	StaticMeshInstanceDesc = InstanceData->GetDefaultVisualizationChecked().VisualizationDesc.ToMassVisualizationDesc();

	if (StaticMeshInstanceDesc.Meshes.IsEmpty())
	{
		ensure(InstanceData->GetDefaultVisualizationChecked().ISMComponents.IsEmpty());
		Params.LODRepresentation[EMassLOD::Low] = EMassRepresentationType::None;

		if (!bIsDedicatedServer)
		{
			// Don't attempt to switch to ISMC representation on clients for no mesh classes (which would otherwise crash)
			// Let the server spawn these actors and replicate them to clients.
			Params.LODRepresentation[EMassLOD::High] = EMassRepresentationType::None;
			Params.LODRepresentation[EMassLOD::Medium] = EMassRepresentationType::None;
		}
	}

	double SettingsMaxInstanceDistance = FInstancedActorsSettings::DefaultMaxInstanceDistance;
	float LODDistanceScale = 1.0f;
	double GlobalMaxInstanceDistanceScale = 1.0;
	Settings.ComputeLODDistanceData(SettingsMaxInstanceDistance, GlobalMaxInstanceDistanceScale, LODDistanceScale);

	LODParams.LODDistance[EMassLOD::High] = 0.f;
	LODParams.LODDistance[EMassLOD::Medium] = Settings.MaxActorDistance;
	LODParams.LODDistance[EMassLOD::Low] = Settings.MaxActorDistance;
	LODParams.LODDistance[EMassLOD::Off] = SettingsMaxInstanceDistance * GlobalMaxInstanceDistanceScale;
}

void UInstancedActorsVisualizationTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	check(InstanceData.IsValid());

	Super::BuildTemplate(BuildContext, World);

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);

	BuildContext.AddFragment<FTransformFragment>();
	BuildContext.AddFragment<FMassActorFragment>();

	FInstancedActorsDataSharedFragment ManagerSharedFragment;
	ManagerSharedFragment.InstanceData = InstanceData;
	const uint32 SubsystemHash = UE::StructUtils::GetStructCrc32(FConstStructView::Make(ManagerSharedFragment));
	FSharedStruct SubsystemFragment = EntityManager.GetOrCreateSharedFragmentByHash<FInstancedActorsDataSharedFragment>(SubsystemHash, ManagerSharedFragment);

	FInstancedActorsDataSharedFragment* AsShared = SubsystemFragment.GetPtr<FInstancedActorsDataSharedFragment>();
	if (ensure(AsShared))
	{
		// this can happen when we unload data and then stream it back again - we end up with the same path object, but different pointer.
		// The old instance should be garbage now
		if (AsShared->InstanceData != InstanceData)
		{
			ensure(AsShared->InstanceData.IsValid() == false);
			AsShared->InstanceData = InstanceData;
		}
		
		// we also need to make sure the bulk LOD is reset since the shared fragment can survive the death of the original 
		// InstanceData while preserving the "runtime" value, which will mess up newly spawned entities.
		AsShared->BulkLOD = EInstancedActorsBulkLOD::MAX;

		InstanceData->SetSharedInstancedActorDataStruct(SubsystemFragment);
	}
	// not adding SubsystemFragment do BuildContext on purpose, we temporarily use shared fragments to store IAD information.
	// To be moved to InstancedActorSubsystem in the future

	// ActorInstanceFragment will get initialized by UInstancedActorsInitializerProcessor
	BuildContext.AddFragment_GetRef<FMassActorInstanceFragment>();

	FInstancedActorsFragment& InstancedActorFragment = BuildContext.AddFragment_GetRef<FInstancedActorsFragment>();
	InstancedActorFragment.InstanceData = InstanceData;

	// @todo Implement version of AddVisualDescWithISMComponent that supports multiple ISMCs and use that here
	if (ensure(InstanceData.IsValid()))
	{
		FMassRepresentationFragment& RepresentationFragment = BuildContext.GetFragmentChecked<FMassRepresentationFragment>();
		RepresentationFragment.StaticMeshDescHandle = InstanceData->GetDefaultVisualizationChecked().MassStaticMeshDescHandle;

		if (RepresentationFragment.LowResTemplateActorIndex == INDEX_NONE)
		{
			// if there's no "low res actor" we reuse the high-res one, otherwise we risk the visualization actor getting 
			// removed when switching from EMassLOD::High down to EMassLOD::Medium
			RepresentationFragment.LowResTemplateActorIndex = RepresentationFragment.HighResTemplateActorIndex;
		}
	}
}
