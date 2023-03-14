// Copyright Epic Games, Inc. All Rights Reserved.
#include "ColorCorrectRegionsSubsystem.h"
#include "ColorCorrectRegionsModule.h"
#include "ColorCorrectRegionsSceneViewExtension.h"
#include "ColorCorrectRegionsStencilManager.h"
#include "ColorCorrectWindow.h"

#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "SceneViewExtension.h"


#if WITH_EDITOR
#include "Editor.h"
#endif

#define LOCTEXT_NAMESPACE "CCR"

static TAutoConsoleVariable<int32> CVarCCRPriorityIncrement(
	TEXT("r.CCR.PriorityIncrementAmount"),
	1,
	TEXT("Affects the priority increment of a newly created Color Correct Region."));

namespace
{
	bool IsRegionValid(AColorCorrectRegion* InRegion, UWorld* CurrentWorld)
	{
		// There some cases in which actor can belong to a different world or the world without subsystem.
		// Example: when editing a blueprint deriving from AVPCRegion.
		// We also check if the actor is being dragged from the content browser.
#if WITH_EDITOR
		return InRegion && !InRegion->bIsEditorPreviewActor && InRegion->GetWorld() == CurrentWorld;
#else
		return InRegion && InRegion->GetWorld() == CurrentWorld;
#endif
	}

	void AssignNewPriorityIfNeeded(AColorCorrectRegion* InRegion, const TArray<AColorCorrectRegion*>& RegionsPriorityBased)
	{
		int32 HighestPriority = 0;
		bool bAssignNewPriority = InRegion->Priority == 0;
		for (const AColorCorrectRegion* Region : RegionsPriorityBased)
		{
			if (InRegion->Priority == Region->Priority)
			{
				bAssignNewPriority = true;
			}
			HighestPriority = HighestPriority < Region->Priority ? Region->Priority : HighestPriority;
		}
		if (bAssignNewPriority)
		{
			InRegion->Priority = HighestPriority + (HighestPriority == 0 ? 1 : FMath::Max(CVarCCRPriorityIncrement.GetValueOnAnyThread(), 1));
		}
	}
}

void UColorCorrectRegionsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
#if WITH_EDITOR
	if (GetWorld()->WorldType == EWorldType::Editor)
	{
		GEngine->OnLevelActorAdded().AddUObject(this, &UColorCorrectRegionsSubsystem::OnActorSpawned);
		GEngine->OnLevelActorDeleted().AddUObject(this, &UColorCorrectRegionsSubsystem::OnActorDeleted);
		GEngine->OnLevelActorListChanged().AddUObject(this, &UColorCorrectRegionsSubsystem::OnLevelActorListChanged);
		GEditor->RegisterForUndo(this);

		FEditorDelegates::OnDuplicateActorsBegin.AddUObject(this, &UColorCorrectRegionsSubsystem::OnDuplicateActorsBegin);
		FEditorDelegates::OnDuplicateActorsEnd.AddUObject(this, &UColorCorrectRegionsSubsystem::OnDuplicateActorsEnd);

		FEditorDelegates::OnEditPasteActorsBegin.AddUObject(this, &UColorCorrectRegionsSubsystem::OnDuplicateActorsBegin);
		FEditorDelegates::OnEditPasteActorsEnd.AddUObject(this, &UColorCorrectRegionsSubsystem::OnDuplicateActorsEnd);
	}
#endif
	// In some cases (like nDisplay nodes) EndPlay is not guaranteed to be called when level is removed.
	GetWorld()->OnLevelsChanged().AddUObject(this, &UColorCorrectRegionsSubsystem::OnLevelsChanged);
	// Initializing Scene view extension responsible for rendering regions.
	PostProcessSceneViewExtension = FSceneViewExtensions::NewExtension<FColorCorrectRegionsSceneViewExtension>(this);
}

void UColorCorrectRegionsSubsystem::Deinitialize()
{
#if WITH_EDITOR
	if (GetWorld()->WorldType == EWorldType::Editor)
	{
		GEngine->OnLevelActorAdded().RemoveAll(this);
		GEngine->OnLevelActorDeleted().RemoveAll(this);
		GEngine->OnLevelActorListChanged().RemoveAll(this);
		GEditor->UnregisterForUndo(this);

		FEditorDelegates::OnDuplicateActorsBegin.RemoveAll(this);
		FEditorDelegates::OnDuplicateActorsEnd.RemoveAll(this);

		FEditorDelegates::OnEditPasteActorsBegin.RemoveAll(this);
		FEditorDelegates::OnEditPasteActorsEnd.RemoveAll(this);
	}
#endif
	GetWorld()->OnLevelsChanged().RemoveAll(this);
	for (AColorCorrectRegion* Region : RegionsPriorityBased)
	{
		Region->Cleanup();
	}
	RegionsPriorityBased.Reset();	
	
	for (AColorCorrectRegion* Region : RegionsDistanceBased)
	{
		Region->Cleanup();
	}
	RegionsDistanceBased.Reset();

	// Prevent this SVE from being gathered, in case it is kept alive by a strong reference somewhere else.
	{
		PostProcessSceneViewExtension->IsActiveThisFrameFunctions.Empty();

		FSceneViewExtensionIsActiveFunctor IsActiveFunctor;

		IsActiveFunctor.IsActiveFunction = [](const ISceneViewExtension* SceneViewExtension, const FSceneViewExtensionContext& Context)
		{
			return TOptional<bool>(false);
		};

		PostProcessSceneViewExtension->IsActiveThisFrameFunctions.Add(IsActiveFunctor);
	}

	PostProcessSceneViewExtension.Reset();
	PostProcessSceneViewExtension = nullptr;
}

void UColorCorrectRegionsSubsystem::OnActorSpawned(AActor* InActor)
{
	AColorCorrectRegion* AsRegion = Cast<AColorCorrectRegion>(InActor);
	if (IsRegionValid(AsRegion, GetWorld()))
	{
		FScopeLock RegionScopeLock(&RegionAccessCriticalSection);
		EColorCorrectRegionsType CCRType = AsRegion->Type;
		// We wouldn't have to do a check here except in case of nDisplay we need to populate this list during OnLevelsChanged 
		// because nDisplay can release Actors while those are marked as BeginningPlay. Therefore we want to avoid 
		// adding regions twice.
		bool bIsDistanceBased = Cast<AColorCorrectionWindow>(InActor) != nullptr;
		TArray<AColorCorrectRegion*>* RegionsToAddTo = bIsDistanceBased ? &RegionsDistanceBased : &RegionsPriorityBased;
		if (!bIsDistanceBased && AsRegion->Priority == 0)
		{
			AssignNewPriorityIfNeeded(AsRegion, RegionsPriorityBased);
		}
		
		if (!RegionsToAddTo->Contains(AsRegion))
		{
			RegionsToAddTo->Add(AsRegion);
			// Distance based CCR can only be sorted on render, when View info is available.
			if (!bIsDistanceBased)
			{
				SortRegionsByPriority();
			}
		}
	}

	if (bDuplicationStarted)
	{
		DuplicatedActors.Add(InActor);
	}
}

void UColorCorrectRegionsSubsystem::OnActorDeleted(AActor* InActor)
{
	AColorCorrectRegion* AsRegion = Cast<AColorCorrectRegion>(InActor);
	if (AsRegion 
#if WITH_EDITORONLY_DATA
		&& !AsRegion->bIsEditorPreviewActor)
#else
		)
#endif
	{
		AsRegion->Cleanup();

#if WITH_EDITOR
		FColorCorrectRegionsStencilManager::OnCCRRemoved(GetWorld(), AsRegion);
#endif

		FScopeLock RegionScopeLock(&RegionAccessCriticalSection);
		RegionsPriorityBased.Remove(AsRegion);
		RegionsDistanceBased.Remove(AsRegion);
	}
}

void UColorCorrectRegionsSubsystem::OnDuplicateActorsEnd()
{
	bDuplicationStarted = false; 

#if WITH_EDITOR
	if (GEditor)
	{
		GEditor->BeginTransaction(LOCTEXT("PerActorCCActorAssigned", "Per actor CC Actor Assigned"));
	}
#endif

	for (AActor* DuplicatedActor : DuplicatedActors)
	{
		if (AColorCorrectRegion* AsRegion = Cast<AColorCorrectRegion>(DuplicatedActor))
		{
			AssignNewPriorityIfNeeded(AsRegion, RegionsPriorityBased);
		}
		else
		{
			FColorCorrectRegionsStencilManager::CleanActor(DuplicatedActor);
		}
	}

	DuplicatedActors.Empty();

#if WITH_EDITOR
	if (GEditor)
	{
		this->Modify();
		GEditor->EndTransaction();
	}
#endif
}

void UColorCorrectRegionsSubsystem::SortRegionsByPriority()
{
	FScopeLock RegionScopeLock(&RegionAccessCriticalSection);

	RegionsPriorityBased.Sort([](const AColorCorrectRegion& A, const AColorCorrectRegion& B) {
		// Regions with the same priority could potentially cause flickering on overlap
		return A.Priority < B.Priority;
	});
}

void UColorCorrectRegionsSubsystem::SortRegionsByDistance(const FVector& ViewLocation)
{
	FScopeLock RegionScopeLock(&RegionAccessCriticalSection);
	TMap<AColorCorrectRegion*, double> DistanceMap;
	for (AColorCorrectRegion* Region : RegionsDistanceBased)
	{
		FColorCorrectRenderProxyPtr State = Region->GetCCProxy_RenderThread();
		FVector CameraToRegionVec = (State->BoxOrigin - ViewLocation);
		DistanceMap.Add(Region, CameraToRegionVec.Dot(CameraToRegionVec));
	}

	RegionsDistanceBased.Sort([&DistanceMap](const AColorCorrectRegion& A, const AColorCorrectRegion& B) {
		// Regions with the same distance could potentially cause flickering on overlap
		return DistanceMap[&A] > DistanceMap[&B];
	});

}

void UColorCorrectRegionsSubsystem::AssignStencilIdsToPerActorCC(AColorCorrectRegion* Region, bool bIgnoreUserNotificaion, bool bSoftAssign)
{
#if WITH_EDITOR
	if (!bSoftAssign && GEditor)
	{
		GEditor->BeginTransaction(LOCTEXT("PerActorCCActorAssigned", "Per actor CC Actor Assigned"));
	}
#endif
	FColorCorrectRegionsStencilManager::AssignStencilIdsToAllActorsForCCR(GetWorld(), Region, bIgnoreUserNotificaion, bSoftAssign);

#if WITH_EDITOR
	if (!bSoftAssign && GEditor)
	{
		this->Modify();
		GEditor->EndTransaction();
	}
#endif
}

void UColorCorrectRegionsSubsystem::ClearStencilIdsToPerActorCC(AColorCorrectRegion* Region)
{
#if WITH_EDITOR
	if (GEditor)
	{
		GEditor->BeginTransaction(LOCTEXT("PerActorCCActorRemoved", "Per actor CC Actor Removed"));
	}
#endif

	FColorCorrectRegionsStencilManager::RemoveStencilNumberForSelectedRegion(GetWorld(), Region);
	this->Modify();

#if WITH_EDITOR
	if (GEditor)
	{
		GEditor->EndTransaction();
	}
#endif
}

void UColorCorrectRegionsSubsystem::CheckAssignedActorsValidity(AColorCorrectRegion* Region)
{
	FColorCorrectRegionsStencilManager::CheckAssignedActorsValidity(Region);
}

void UColorCorrectRegionsSubsystem::RefreshStenciIdAssignmentForAllCCR()
{
	for (TActorIterator<AColorCorrectRegion> It(GetWorld()); It; ++It)
	{
		AColorCorrectRegion* AsRegion = *It;
		if (IsRegionValid(AsRegion, GetWorld()))
		{
			// Uncoment this if you want the invalid actors to be removed automatically.
			// However after removal, undo/redo will not re-assign this actor back to CCR.
			//FColorCorrectRegionsStencilManager::ClearInvalidActorsForSelectedRegion(AsRegion);
			AsRegion->PerAffectedActorStencilData.Empty();
			FColorCorrectRegionsStencilManager::AssignStencilIdsToAllActorsForCCR(GetWorld(), AsRegion, true, true);
			FColorCorrectRegionsStencilManager::RemoveStencilNumberForSelectedRegion(GetWorld(), AsRegion);
		}
	}
}

void UColorCorrectRegionsSubsystem::RefreshRegions()
{
	FScopeLock RegionScopeLock(&RegionAccessCriticalSection);
	RegionsPriorityBased.Reset();
	RegionsDistanceBased.Reset();
	for (TActorIterator<AColorCorrectRegion> It(GetWorld()); It; ++It)
	{
		AColorCorrectRegion* AsRegion = *It;
		if (IsRegionValid(AsRegion, GetWorld()))
		{
			if (!Cast<AColorCorrectionWindow>(AsRegion))
			{
				RegionsPriorityBased.Add(AsRegion);
			}
			else
			{
				RegionsDistanceBased.Add(AsRegion);
			}
		}
	}

	SortRegionsByPriority();

	RefreshStenciIdAssignmentForAllCCR();
}

#undef LOCTEXT_NAMESPACE