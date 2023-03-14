// Copyright Epic Games, Inc. All Rights Reserved.

#include "LandscapePatchComponent.h"

#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Landscape.h"
#include "LandscapePatchLogging.h"
#include "LandscapePatchManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LandscapePatchComponent)

#define LOCTEXT_NAMESPACE "LandscapePatch"

namespace LandscapePatchComponentLocals
{
#if WITH_EDITOR

	ALandscapePatchManager* FindExistingPatchManagerForLandscape(ALandscape* Landscape)
	{
		for (int LayerIndex = 0; LayerIndex < Landscape->LandscapeLayers.Num(); ++LayerIndex)
		{
			TArray<ALandscapeBlueprintBrushBase*> LayerBrushes = Landscape->GetBrushesForLayer(LayerIndex);
			ALandscapeBlueprintBrushBase** Found = LayerBrushes.FindByPredicate(
				[](ALandscapeBlueprintBrushBase* Candidate) { return Cast<ALandscapePatchManager>(Candidate) != nullptr; });
			if (Found)
			{
				return Cast<ALandscapePatchManager>(*Found);
			}
		}

		return nullptr;
	}

	ALandscapePatchManager* CreateNewPatchManagerForLandscape(ALandscape* Landscape)
	{
		if (!ensure(Landscape->CanHaveLayersContent()))
		{
			return nullptr;
		}

		FString BrushActorString = FString::Format(TEXT("{0}_{1}"), { Landscape->GetActorLabel(), ALandscapePatchManager::StaticClass()->GetName() });
		FName BrushActorName = MakeUniqueObjectName(Landscape->GetOuter(), ALandscapePatchManager::StaticClass(), FName(BrushActorString));
		FActorSpawnParameters SpawnParams;
		SpawnParams.Name = BrushActorName;
		SpawnParams.bAllowDuringConstructionScript = true; // This can be called by construction script if the actor being added to the world is part of a blueprint

		ALandscapePatchManager* PatchManager = Landscape->GetWorld()->SpawnActor<ALandscapePatchManager>(ALandscapePatchManager::StaticClass(), SpawnParams);
		if (PatchManager)
		{
			PatchManager->SetActorLabel(BrushActorString);
			PatchManager->SetTargetLandscape(Landscape);
		}

		return PatchManager;
	}

#endif
}

// Note that this is not allowed to be editor-only
ULandscapePatchComponent::ULandscapePatchComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Causes OnUpdateTransform to be called when the parent is moved. Note that this is better to do here in the
	// constructor, otherwise we'd need to do it both in OnComponentCreated and PostLoad.
	// We could keep this false if we were to register to TransformUpdated, since that gets broadcast either way.
	// TODO: Currently, neither TransformUpdated nor OnUpdateTransform are triggered when parent's transform is changed
	bWantsOnUpdateTransform = true;
}

void ULandscapePatchComponent::SetIsEnabled(bool bEnabledIn)
{
	if (bEnabledIn == bIsEnabled)
	{
		return;
	}

	bIsEnabled = bEnabledIn;
	RequestLandscapeUpdate();
}

#if WITH_EDITOR
void ULandscapePatchComponent::OnComponentCreated()
{
	using namespace LandscapePatchComponentLocals;

	Super::OnComponentCreated();

	// Mark whether we're creating from scratch or from a copy
	bWasCopy = bPropertiesCopiedIndicator;
	bPropertiesCopiedIndicator = true;

	UWorld* World = GetWorld();
	if (IsTemplate() || World->WorldType != EWorldType::Editor)
	{
		return;
	}

	if (PatchManager.IsValid())
	{
		// If we copied over a patch manager, presumably Landscape should be
		// copied over as well, but might as well do this to be safe.
		Landscape = PatchManager->GetOwningLandscape();

		if (!PatchManager->ContainsPatch(this))
		{
			SetPatchManager(PatchManager.Get());
		}
	}
	else if (Landscape.IsValid())
	{
		// If we copied over a patch with a landscape but no manager.
		SetLandscape(Landscape.Get());
	}
	else
	{
		// See if the level has a height patch manager to which we can add ourselves
		TActorIterator<ALandscapePatchManager> ManagerIterator(World);
		if (!!ManagerIterator)
		{
			SetPatchManager(*ManagerIterator);
		}
		else
		{
			// If no existing manager, find some landscape and add a new one.
			for (TActorIterator<ALandscape> LandscapeIterator(World); LandscapeIterator; ++LandscapeIterator)
			{
				if (LandscapeIterator->CanHaveLayersContent())
				{
					Landscape = *LandscapeIterator;
					SetPatchManager(CreateNewPatchManagerForLandscape(Landscape.Get()));
					break;
				}
			}
			if (!PatchManager.IsValid())
			{
				UE_LOG(LogLandscapePatch, Warning, TEXT("Unable to find a landscape with edit layers enabled. Unable to create patch manager."));
			}
		}
	}
}

void ULandscapePatchComponent::PostLoad()
{
	Super::PostLoad();

	bPropertiesCopiedIndicator = true;
	bLoadedButNotYetRegistered = true;
}

void ULandscapePatchComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	if (PatchManager.IsValid())
	{
		PatchManager->RemovePatch(this);
	}

	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

void ULandscapePatchComponent::OnRegister()
{
	Super::OnRegister();


	if (bLoadedButNotYetRegistered)
	{
		bLoadedButNotYetRegistered = false;
		return;
	}

	// TODO: Currently the main reason to invalidate the landscape on registration is to respond
	// to detail panel changes of the parent's transform. However we may be able to catch a wide
	// variety of changes this way, so we'll need to see if we can get rid of other invalidations.
	// Also, we should make the invalidation conditional on whether we actually modify any relevant
	// properties by having a virtual method that compares and updates a stored hash of them.
	if (IsEnabled())
	{
		RequestLandscapeUpdate();
	}
}

void ULandscapePatchComponent::GetActorDescProperties(FPropertyPairsMap& PropertyPairsMap) const
{
	Super::GetActorDescProperties(PropertyPairsMap);

	if (Landscape.IsValid())
	{
		PropertyPairsMap.AddProperty(ALandscape::AffectsLandscapeActorDescProperty, *Landscape->GetLandscapeGuid().ToString());
	}
}

TStructOnScope<FActorComponentInstanceData> ULandscapePatchComponent::GetComponentInstanceData() const
{
	return MakeStructOnScope<FActorComponentInstanceData, FLandscapePatchComponentInstanceData>(this);
}

FLandscapePatchComponentInstanceData::FLandscapePatchComponentInstanceData(const ULandscapePatchComponent* Patch)
: FSceneComponentInstanceData(Patch)
{
	ALandscapePatchManager* PatchManager = Patch->GetPatchManager();
	if (PatchManager)
	{
		IndexInManager = PatchManager->GetIndexOfPatch(Patch);

#if WITH_EDITOR
		bGaveMissingPatchManagerWarning = Patch->bGaveMissingPatchManagerWarning;
		bGaveNotInPatchManagerWarning = Patch->bGaveNotInPatchManagerWarning;
		bGaveMissingLandscapeWarning = Patch->bGaveMissingLandscapeWarning;
#endif
	}
}

void ULandscapePatchComponent::OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport)
{
	Super::OnUpdateTransform(UpdateTransformFlags, Teleport);

	if (IsEnabled())
	{
		RequestLandscapeUpdate();
	}
}

void ULandscapePatchComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	using namespace LandscapePatchComponentLocals;

	// Do a bunch of checks to make sure that we don't try to do anything when the editing is happening inside the blueprint editor.
	UWorld* World = GetWorld();
	if (IsTemplate() || !IsValid(this) || !IsValid(World) || World->WorldType != EWorldType::Editor)
	{
		return;
	}

	// If we're changing the owning landscape or patch manaager, there's some work we need to do to remove/add 
	// ourselves from/to the proper brush managers.
	if (PropertyChangedEvent.Property 
		&& (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(ULandscapePatchComponent, Landscape)))
	{
		SetLandscape(Landscape.Get());
	}
	else if (PropertyChangedEvent.Property
		&& (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(ULandscapePatchComponent, PatchManager)))
	{
		SetPatchManager(PatchManager.Get());
	}

	if (IsEnabled() || (PropertyChangedEvent.Property
		&& (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(ULandscapePatchComponent, bIsEnabled))))
	{
		RequestLandscapeUpdate();
	}

	// It is important that this super call happen after the above, because inside a blueprint actor, the call triggers a
	// rerun of the construction scripts, which will destroy the component and mess with our ability to do the above adjustments
	// properly (IsValid(this) returns false, the patch manager has the patch removed so it complains when we try to trigger
	// the update, etc).
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

// Called after rerunning construction scripts (when patch is part of a blueprint) to make sure that the newly created 
// instance of the patch is placed in the same order in the patch  manager as the destroyed instance.
void ULandscapePatchComponent::ApplyComponentInstanceData(FLandscapePatchComponentInstanceData* ComponentInstanceData)
{
	if (ComponentInstanceData)
	{
		if (ComponentInstanceData->IndexInManager >= 0 && PatchManager.IsValid())
		{
			PatchManager->MovePatchToIndex(this, ComponentInstanceData->IndexInManager);
		}

#if WITH_EDITOR
		bGaveMissingPatchManagerWarning = ComponentInstanceData->bGaveMissingPatchManagerWarning;
		bGaveNotInPatchManagerWarning = ComponentInstanceData->bGaveNotInPatchManagerWarning;
		bGaveMissingLandscapeWarning = ComponentInstanceData->bGaveMissingLandscapeWarning;
#endif
	}
}

void ULandscapePatchComponent::SetLandscape(ALandscape* NewLandscape)
{
#if WITH_EDITOR

	using namespace LandscapePatchComponentLocals;

	// We early out here if the landscape stays the same and we have a patch manager because it is inconvenient to accidentally swap patch
	// managers if there are multiple in the same landscape.
	if (Landscape.Get() == NewLandscape && (
		(Landscape.IsNull() && PatchManager.IsNull()) || (Landscape.IsValid() && PatchManager.IsValid() && PatchManager->GetOwningLandscape() == Landscape.Get())))
	{
		return;
	}

	Landscape = NewLandscape;

	if (!NewLandscape)
	{
		SetPatchManager(nullptr);
		return;
	}

	UWorld* World = GetWorld();
	if (IsTemplate() || !IsValid(World) || World->WorldType != EWorldType::Editor)
	{
		return;
	}

	// If landscape was valid, try to find a patch manager inside that landscape.
	if (Landscape->CanHaveLayersContent())
	{
		ALandscapePatchManager* ManagerToUse = FindExistingPatchManagerForLandscape(Landscape.Get());
		if (!ManagerToUse)
		{
			ManagerToUse = CreateNewPatchManagerForLandscape(Landscape.Get());
		}

		SetPatchManager(ManagerToUse);
	}
	else
	{
		UE_LOG(LogLandscapePatch, Warning, TEXT("Landscape target for height patch did not have edit layers enabled. Unable to create patch manager."));
		Landscape = nullptr;
		SetPatchManager(nullptr);
	}

#endif // WITH_EDITOR
}

void ULandscapePatchComponent::SetPatchManager(ALandscapePatchManager* NewPatchManager)
{
	// Could have put the WITH_EDITOR block around just GetOwningLandscape, but might as well
	// surround everything since nothing is currently expected to work for a patch manager at runtime.
#if WITH_EDITOR

	if (PreviousPatchManager == NewPatchManager && NewPatchManager && NewPatchManager->ContainsPatch(this))
	{
		return;
	}
	ResetWarnings();

	if (PreviousPatchManager.IsValid())
	{
		PreviousPatchManager->RemovePatch(this);
	}

	PatchManager = NewPatchManager;
	if (NewPatchManager)
	{
		UWorld* World = GetWorld();
		if (!IsTemplate() && IsValid(World) && World->WorldType == EWorldType::Editor)
		{
			PatchManager->AddPatch(this);
		}
		Landscape = PatchManager->GetOwningLandscape();
	}
	else
	{
		Landscape = nullptr;
	}

	PreviousPatchManager = NewPatchManager;

#endif // WITH_EDITOR
}

ALandscapePatchManager* ULandscapePatchComponent::GetPatchManager() const
{
	return PatchManager.Get();
}

void ULandscapePatchComponent::MoveToTop()
{
	if (PatchManager.IsValid() && PatchManager->ContainsPatch(this))
	{
		PatchManager->Modify();
		PatchManager->RemovePatch(this);
		PatchManager->AddPatch(this);
		if (IsEnabled())
		{
			PatchManager->RequestLandscapeUpdate();
		}
	}
}

void ULandscapePatchComponent::RequestLandscapeUpdate()
{
#if WITH_EDITOR
	UWorld* World = GetWorld();
	// Note that aside from the usual guard against doing things in the blueprint editor, the check of WorldType
	// here also prevents us from doing the request while cooking, where WorldType is set to Inactive. Otherwise
	// we would issue a warning below while cooking when PatchManager is not saved (and not reset in the construction
	// script).
	if (IsTemplate() || World->WorldType != EWorldType::Editor)
	{
		return;
	}

	// Helper for getting name strings in the warning messages below.
	auto GetNameString = [](UObject* PotentiallyNullObject) {
		FString ToReturn;
		if (PotentiallyNullObject)
		{
			PotentiallyNullObject->GetName(ToReturn);
		}
		return ToReturn;
	};

	if (!PatchManager.IsValid())
	{
		if (!bGaveMissingPatchManagerWarning)
		{
			UE_LOG(LogLandscapePatch, Warning, TEXT("Patch does not have a valid patch manager. "
				"Set the landscape or patch manager on the patch. (Package: %s, Actor: %s"), 
				*GetNameString(GetPackage()), *GetNameString(GetAttachmentRootActor()));
			bGaveMissingPatchManagerWarning = true;
		}
		return;
	}

	bool bRequestUpdate = true;
	if (!PatchManager->ContainsPatch(this))
	{
		if (!bGaveNotInPatchManagerWarning)
		{
			UE_LOG(LogLandscapePatch, Warning, TEXT("Patch's patch manager does not contain this patch. "
				"Perhaps the manager was not saved? Reset the patch manager on the patch. (Package: %s, Actor: %s)"), 
				*GetNameString(GetPackage()), *GetNameString(GetAttachmentRootActor()));
			bGaveNotInPatchManagerWarning = true;
		}
		bRequestUpdate = false;
	}
	if (!IsValid(PatchManager->GetOwningLandscape()))
	{
		if (!bGaveMissingLandscapeWarning)
		{
			UE_LOG(LogLandscapePatch, Warning, TEXT("Patch's patch manager does not have a valid owning "
				"landscape. Perhaps the landscape was not saved? Reset the landscape on the manager. (Package: %s, Manager: %s)"), 
				*GetNameString(GetPackage()), *GetNameString(PatchManager.Get()));
			bGaveMissingLandscapeWarning = true;
		}
		bRequestUpdate = false;
	}

	if(bRequestUpdate)
	{
		ResetWarnings();
		PatchManager->RequestLandscapeUpdate();
	}
#endif
}

#if WITH_EDITOR
void ULandscapePatchComponent::ResetWarnings()
{
	bGaveMissingPatchManagerWarning = false;
	bGaveNotInPatchManagerWarning = false;
	bGaveMissingLandscapeWarning = false;
}
#endif

FTransform ULandscapePatchComponent::GetLandscapeHeightmapCoordsToWorld() const
{
	if (PatchManager.IsValid())
	{
		return PatchManager->GetHeightmapCoordsToWorld();
	}
	return FTransform::Identity;
}

#undef LOCTEXT_NAMESPACE

