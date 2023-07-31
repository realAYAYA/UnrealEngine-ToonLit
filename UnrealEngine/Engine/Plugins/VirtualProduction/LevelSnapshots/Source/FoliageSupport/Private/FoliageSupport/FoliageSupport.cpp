// Copyright Epic Games, Inc. All Rights Reserved.

#include "FoliageSupport/FoliageSupport.h"

#include "FoliageSupport/InstancedFoliageActorData.h"
#include "Selection/PropertySelection.h"
#include "Selection/PropertySelectionMap.h"
#include "Params/ObjectSnapshotSerializationData.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "FoliageHelper.h"
#include "ILevelSnapshotsModule.h"
#include "InstancedFoliageActor.h"
#include "SnapshotCustomVersion.h"
#include "ActorPartition/ActorPartitionSubsystem.h"
#if WITH_EDITOR
#include "FoliageEditModule.h"
#endif

namespace UE::LevelSnapshots::Foliage::Private::Internal
{
	static void EnableRequiredFoliageProperties(ILevelSnapshotsModule& Module)
	{
		FProperty* InstanceReorderTable = UInstancedStaticMeshComponent::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UInstancedStaticMeshComponent, InstanceReorderTable));
		
		FProperty* SortedInstances = UHierarchicalInstancedStaticMeshComponent::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UHierarchicalInstancedStaticMeshComponent, SortedInstances));
		FProperty* NumBuiltInstances = UHierarchicalInstancedStaticMeshComponent::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UHierarchicalInstancedStaticMeshComponent, NumBuiltInstances));
		FProperty* NumBuiltRenderInstances = UHierarchicalInstancedStaticMeshComponent::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UHierarchicalInstancedStaticMeshComponent, NumBuiltRenderInstances));
		FProperty* BuiltInstanceBounds = UHierarchicalInstancedStaticMeshComponent::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UHierarchicalInstancedStaticMeshComponent, BuiltInstanceBounds));
		FProperty* UnbuiltInstanceBounds = UHierarchicalInstancedStaticMeshComponent::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UHierarchicalInstancedStaticMeshComponent, UnbuiltInstanceBounds));
		FProperty* UnbuiltInstanceBoundsList = UHierarchicalInstancedStaticMeshComponent::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UHierarchicalInstancedStaticMeshComponent, UnbuiltInstanceBoundsList));
		FProperty* bEnableDensityScaling = UHierarchicalInstancedStaticMeshComponent::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UHierarchicalInstancedStaticMeshComponent, bEnableDensityScaling));
		FProperty* OcclusionLayerNumNodes = UHierarchicalInstancedStaticMeshComponent::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UHierarchicalInstancedStaticMeshComponent, OcclusionLayerNumNodes));
		FProperty* CacheMeshExtendedBounds = UHierarchicalInstancedStaticMeshComponent::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UHierarchicalInstancedStaticMeshComponent, CacheMeshExtendedBounds));
		FProperty* bDisableCollision = UHierarchicalInstancedStaticMeshComponent::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UHierarchicalInstancedStaticMeshComponent, bDisableCollision));
		FProperty* InstanceCountToRender = UHierarchicalInstancedStaticMeshComponent::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UHierarchicalInstancedStaticMeshComponent, InstanceCountToRender));
		
		if (ensure(InstanceReorderTable))
		{
			Module.AddExplicitilySupportedProperties({
				InstanceReorderTable,
				SortedInstances,
				NumBuiltInstances,
				NumBuiltRenderInstances,
				BuiltInstanceBounds,
				UnbuiltInstanceBounds,
				UnbuiltInstanceBoundsList,
				bEnableDensityScaling,
				OcclusionLayerNumNodes,
				CacheMeshExtendedBounds,
				bDisableCollision,
				InstanceCountToRender
			});
		}
	}
}

void UE::LevelSnapshots::Foliage::Private::FFoliageSupport::Register(ILevelSnapshotsModule& Module)
{
	Internal::EnableRequiredFoliageProperties(Module);
	
	const TSharedRef<FFoliageSupport> FoliageSupport = MakeShared<FFoliageSupport>();
	Module.RegisterRestorabilityOverrider(FoliageSupport);
	Module.RegisterRestorationListener(FoliageSupport);
	Module.RegisterCustomObjectSerializer(AInstancedFoliageActor::StaticClass(), FoliageSupport);
}

UE::LevelSnapshots::ISnapshotRestorabilityOverrider::ERestorabilityOverride UE::LevelSnapshots::Foliage::Private::FFoliageSupport::IsActorDesirableForCapture(const AActor* Actor)
{
	// Foliage's not allowed by default because it is hidden from the scene outliner
	return Actor->GetClass() == AInstancedFoliageActor::StaticClass() || FFoliageHelper::IsOwnedByFoliage(Actor)
		? ERestorabilityOverride::Allow : ERestorabilityOverride::DoNotCare;
}

void UE::LevelSnapshots::Foliage::Private::FFoliageSupport::OnTakeSnapshot(UObject* EditorObject, ICustomSnapshotSerializationData& DataStorage)
{
	AInstancedFoliageActor* FoliageActor = Cast<AInstancedFoliageActor>(EditorObject);
	check(FoliageActor);

	DataStorage.WriteObjectAnnotation(FObjectAnnotator::CreateLambda([FoliageActor](FArchive& Archive)
	{
		FInstancedFoliageActorData FoliageData;
		FoliageData.Save(Archive, FoliageActor);
	}));
}


namespace UE::LevelSnapshots::FoliageSupport::Internal
{
	static void RebuildChangedFoliageComponents(AInstancedFoliageActor* FoliageActor, const FPropertySelectionMap& SelectionMap, bool bWasRecreated)
	{
		const FRestorableObjectSelection FoliageSelection = SelectionMap.GetObjectSelection(FoliageActor);
		const FAddedAndRemovedComponentInfo* RecreatedComponentInfo = FoliageSelection.GetComponentSelection();
		
		TInlineComponentArray<UHierarchicalInstancedStaticMeshComponent*> Components(FoliageActor);
		for (UHierarchicalInstancedStaticMeshComponent* Comp : Components)
		{
			const FRestorableObjectSelection ObjectSelection = SelectionMap.GetObjectSelection(Comp);

			const FPropertySelection* CompPropertySelection = ObjectSelection.GetPropertySelection();
			const bool bHasChangedProperties = CompPropertySelection && !CompPropertySelection->IsEmpty();
			const bool bComponentWasRecreated = RecreatedComponentInfo && Algo::FindByPredicate(RecreatedComponentInfo->SnapshotComponentsToAdd, [Comp](TWeakObjectPtr<UActorComponent> SnapshotComp)
			{
				return SnapshotComp->GetFName().IsEqual(Comp->GetFName());
			}) != nullptr;
			
			if (bWasRecreated || bHasChangedProperties || bComponentWasRecreated)
			{
				// Unregister and register recreates render state: otherwise instances won't show
				Comp->UnregisterComponent();
				
				// This recomputes transforms and shadows
				constexpr bool bAsync = false;
				constexpr bool bForceUpdate = true;
				Comp->BuildTreeIfOutdated(bAsync, bForceUpdate);
				
				Comp->RegisterComponent();
			}
		}
	}
	
	static void UpdateFoliageUI()
	{
#if WITH_EDITOR
		IFoliageEditModule& FoliageEditModule = FModuleManager::Get().GetModuleChecked<IFoliageEditModule>("FoliageEdit");
		FoliageEditModule.UpdateMeshList();
#endif
	}
}

void UE::LevelSnapshots::Foliage::Private::FFoliageSupport::PostApplyToEditorObject(UObject* Object, const ICustomSnapshotSerializationData& DataStorage, const FPropertySelectionMap& SelectionMap)
{
	AInstancedFoliageActor* FoliageActor = Cast<AInstancedFoliageActor>(Object);
	check(FoliageActor);
	
	// Track this actor for safety so we know for sure that the functions were called in the order we expected them to
	CurrentFoliageActor = FoliageActor;
	DataStorage.ReadObjectAnnotation(FObjectAnnotator::CreateLambda([this, &SelectionMap, FoliageActor](FArchive& Archive)
	{
		FInstancedFoliageActorData CurrentFoliageData;
		if (Archive.CustomVer(FSnapshotCustomVersion::GUID) < FSnapshotCustomVersion::CustomSubobjectSoftObjectPathRefactor)
		{
			Archive << CurrentFoliageData;
		}
		
#if WITH_EDITOR
		// Actor might not have been modified because none of its uproperties were changed
		FoliageActor->Modify();
#endif

		const bool bWasRecreated = SelectionMap.GetDeletedActorsToRespawn().Contains(FoliageActor);
		CurrentFoliageData.ApplyTo(Archive, FoliageActor, SelectionMap, bWasRecreated);
		FoliageSupport::Internal::RebuildChangedFoliageComponents(FoliageActor, SelectionMap, bWasRecreated);

		FoliageSupport::Internal::UpdateFoliageUI();
	}));

	// Rest is done in PostApplySnapshotToActor (need access to the property selection map)
}

void UE::LevelSnapshots::Foliage::Private::FFoliageSupport::PostApplySnapshotToActor(const FApplySnapshotToActorParams& Params)
{
	// This is the order of operations up until now
	// 1. OnTakeSnapshot > Set initial data
	// 2. User requests apply to world
	// 3. FindOrRecreateSubobjectInEditorWorld allocates the UFoliageType subobjects
	// 4. PreRemoveComponent
	// 5. PostApplySnapshotProperties loads the data that OnTakeSnapshot saved
	
	if (AInstancedFoliageActor* FoliageActor = Cast<AInstancedFoliageActor>(Params.Actor))
	{
		checkf(FoliageActor == CurrentFoliageActor.Get(), TEXT("PostApplySnapshotToActor was not directly followed by PostApplySnapshotProperties. Investigate."));
		CurrentFoliageActor.Reset();
	}
}

void UE::LevelSnapshots::Foliage::Private::FFoliageSupport::PreApplySnapshotProperties(const FApplySnapshotPropertiesParams& Params)
{
	// RF_Transactional is temporarily required because Level Snapshots changes components differently than Foliage originally designed.
	// Needed for correct undo / redo 
	if (UHierarchicalInstancedStaticMeshComponent* Comp = Cast<UHierarchicalInstancedStaticMeshComponent>(Params.Object); Comp && CurrentFoliageActor.IsValid() && Comp->IsIn(CurrentFoliageActor.Get()))
	{
		Comp->SetFlags(RF_Transactional);
	}
}

void UE::LevelSnapshots::Foliage::Private::FFoliageSupport::PostApplySnapshotProperties(const FApplySnapshotPropertiesParams& Params)
{
	if (UHierarchicalInstancedStaticMeshComponent* Comp = Cast<UHierarchicalInstancedStaticMeshComponent>(Params.Object); Comp && CurrentFoliageActor.IsValid() && Comp->IsIn(CurrentFoliageActor.Get()))
	{
		Comp->ClearFlags(RF_Transactional);
	}
}

void UE::LevelSnapshots::Foliage::Private::FFoliageSupport::PreRecreateActor(UWorld* World, TSubclassOf<AActor> ActorClass, FActorSpawnParameters& InOutSpawnParameters)
{
	if (ActorClass->IsChildOf(AInstancedFoliageActor::StaticClass()))
	{
		InOutSpawnParameters.bCreateActorPackage = true;

		// See AInstancedFoliageActor::GetDefault
		ULevel* CurrentLevel = World->GetCurrentLevel();
		if (CurrentLevel && CurrentLevel->GetWorld()->GetSubsystem<UActorPartitionSubsystem>()->IsLevelPartition())
		{
			InOutSpawnParameters.ObjectFlags |= RF_Transient;
		}
	}
}

void UE::LevelSnapshots::Foliage::Private::FFoliageSupport::PostRecreateActor(AActor* RecreatedActor)
{
	if (AInstancedFoliageActor* FoliageActor = Cast<AInstancedFoliageActor>(RecreatedActor))
	{
		RecreatedActor->GetLevel()->InstancedFoliageActor = FoliageActor;
	}
}

void UE::LevelSnapshots::Foliage::Private::FFoliageSupport::PreRemoveActor(const FPreRemoveActorParams& Params)
{
	if (AInstancedFoliageActor* FoliageActor = Cast<AInstancedFoliageActor>(Params.ActorToRemove))
	{
		FoliageActor->GetLevel()->InstancedFoliageActor.Reset();
	}
}

namespace UE::LevelSnapshots::Foliage::Private::Internal
{
	static UFoliageType* FindFoliageInfoFor(UHierarchicalInstancedStaticMeshComponent* Component)
	{
		AInstancedFoliageActor* Foliage = Cast<AInstancedFoliageActor>(Component->GetOwner());
		if (!LIKELY(Foliage))
		{
			return nullptr;
		}

		for (auto FoliageIt = Foliage->GetFoliageInfos().CreateConstIterator(); FoliageIt; ++FoliageIt)
		{
			if (FoliageIt->Value->Implementation->IsOwnedComponent(Component))
			{
				return FoliageIt->Key;
			}
		}

		return nullptr;
	}

	static UFoliageType* FindFoliageInfoFor(UActorComponent* Component)
	{
		if (UHierarchicalInstancedStaticMeshComponent* Comp = Cast<UHierarchicalInstancedStaticMeshComponent>(Component))
		{
			return FindFoliageInfoFor(Comp);
		}
		return nullptr;
	}
}

void UE::LevelSnapshots::Foliage::Private::FFoliageSupport::PreRemoveComponent(UActorComponent* ComponentToRemove)
{
	if (UFoliageType* FoliagType = Internal::FindFoliageInfoFor(ComponentToRemove))
	{
		AInstancedFoliageActor* FoliageActor = Cast<AInstancedFoliageActor>(ComponentToRemove->GetOwner());
		FoliageActor->RemoveFoliageType(&FoliagType, 1);
	}
}
