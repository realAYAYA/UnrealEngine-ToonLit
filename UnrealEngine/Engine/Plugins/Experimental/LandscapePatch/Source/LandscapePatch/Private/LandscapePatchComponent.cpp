// Copyright Epic Games, Inc. All Rights Reserved.

#include "LandscapePatchComponent.h"

#include "CoreGlobals.h" // GIsReconstructingBlueprintInstances
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h" // FAutoConsoleVariableRef
#include "Landscape.h"
#include "LandscapePatchLogging.h"
#include "LandscapePatchManager.h"
#include "Logging/MessageLog.h"
#include "Misc/UObjectToken.h"
#include "PropertyPairsMap.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/Package.h"
#include "UObject/UObjectBaseUtility.h" // GetNameSafe
#include "UObject/UObjectIterator.h"

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

	bool bAllowAutoFindOfManagerInConstructionScript = true;
	static FAutoConsoleVariableRef CVarReAddToPatchManagerOnConstructionIfLost(
		TEXT("LandscapePatch.AllowAutoFindOfManagerInConstructionScript"),
		bAllowAutoFindOfManagerInConstructionScript,
		TEXT("If a blueprint-contained patch does not have a patch manager when its construction script is rerun, "
			"allows for a patch manager to be found automatically. This is more safe to have turned off because "
			"it can hide potential issues when patches or manager were not properly saved, and as a result, patch "
			"order could change depending on load order of actors. It is on by default for now for legacy support."));


	// A way to fix patch manager pointers. More aggressive than the map check, because it allows auto-finding
	// of a new manager.
	FAutoConsoleCommand CCmdFixPatchManagerPointers(
		TEXT("LandscapePatch.FixPatchManagerPointers"),
		TEXT("For all patches, make sure that patch manager pointers exist, contain this patch, and patches "
			"and managers are dirtied if they changed. Patches without a manager get one auto-assigned."),
		FConsoleCommandDelegate::CreateLambda([]() 
	{
		// Iterate through all patches
		for (TObjectIterator<ULandscapePatchComponent> It(
			/*AdditionalExclusionFlags = */RF_ClassDefaultObject,
			/*bIncludeDerivedClasses = */true,
			/*InInternalExclusionFlags = */EInternalObjectFlags::Garbage); It; ++It)
		{
			ULandscapePatchComponent* Patch = *It;
			if (!IsValid(Patch))
			{
				continue;
			}

			UWorld* World = Patch->GetWorld();
			if (Patch->IsTemplate() || !IsValid(World) || World->WorldType != EWorldType::Editor)
			{
				continue;
			}

			Patch->SetUpPatchManagerData();
			Patch->MarkDirtyIfModifiedInConstructionScript();
		}

		for (TObjectIterator<ALandscapePatchManager> It(
			/*AdditionalExclusionFlags = */RF_ClassDefaultObject,
			/*bIncludeDerivedClasses = */true,
			/*InInternalExclusionFlags = */EInternalObjectFlags::Garbage); It; ++It)
		{
			ALandscapePatchManager* Manager = *It;
			if (!IsValid(Manager))
			{
				continue;
			}

			UWorld* World = Manager->GetWorld();
			if (Manager->IsTemplate() || !IsValid(World) || World->WorldType != EWorldType::Editor)
			{
				continue;
			}

			if (Manager)
			{
				Manager->MarkDirtyIfModifiedInConstructionScript();
			}
		}
	}));

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

	Modify();
	bIsEnabled = bEnabledIn;
	RequestLandscapeUpdate();
}

#if WITH_EDITOR
void ULandscapePatchComponent::CheckForErrors()
{
	Super::CheckForErrors();

	using namespace LandscapePatchComponentLocals;
	
	if (!IsRealPatch())
	{
		return;
	}

	auto GetPackageAndActorArgs = [this]()
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("Package"), FText::FromString(*GetNameSafe(GetPackage())));
		Arguments.Add(TEXT("Actor"), FText::FromString(*GetNameSafe(GetAttachmentRootActor())));
		return Arguments;
	};

	if (PatchManager.IsValid() != Landscape.IsValid())
	{
		FMessageLog("MapCheck").Warning()
			->AddToken(FUObjectToken::Create(this))
			->AddToken(FTextToken::Create(FText::Format(LOCTEXT("PatchManagerAndLandscapeDisagree", "Patch has inconsistent "
				"manager and landscape pointers. (Package: {Package}, Actor: {Actor}). "
				"Fix individually or run LandscapePatch.FixPatchManagerPointers."), GetPackageAndActorArgs())))
			->AddToken(FActionToken::Create(LOCTEXT("FixInconsistentPointersButton", "Fix inconsistent pointers"), FText(),
				FOnActionTokenExecuted::CreateWeakLambda(this, [this]() 
		{
			if (PatchManager.IsValid())
			{
				SetPatchManager(PatchManager.Get());
			}
			else if (Landscape.IsValid())
			{
				SetLandscape(Landscape.Get());
			}
		})));
	}

	if (PatchManager.IsValid() && !PatchManager->ContainsPatch(this))
	{
		FMessageLog("MapCheck").Warning()
			->AddToken(FUObjectToken::Create(this))
			->AddToken(FTextToken::Create(FText::Format(LOCTEXT("PatchNotInManager", "Patch was not found in its patch manager. "
				"(Package: {Package}, Actor: {Actor}). "
				"Fix individually or run LandscapePatch.FixPatchManagerPointers."), GetPackageAndActorArgs())))
			->AddToken(FActionToken::Create(LOCTEXT("AddToManagerButton", "Add to manager"), FText(),
				FOnActionTokenExecuted::CreateWeakLambda(this, [this]() 
		{
			if (PatchManager.IsValid())
			{
				SetPatchManager(PatchManager.Get());
			}
		})));
	}

	if (bDirtiedByConstructionScript && !(GetPackage() && GetPackage()->IsDirty()))
	{
		FMessageLog("MapCheck").Warning()
			->AddToken(FUObjectToken::Create(this))
			->AddToken(FTextToken::Create(FText::Format(LOCTEXT("PatchNeedsSaving", "Patch got modified in a construction "
				"script rerun but has not been marked dirty. (Package: {Package}, Actor: {Actor}). "
				"Fix individually or run LandscapePatch.FixPatchManagerPointers."), GetPackageAndActorArgs())))
			->AddToken(FActionToken::Create(LOCTEXT("MarkDirtyButton", "Mark dirty"), FText(),
				FOnActionTokenExecuted::CreateWeakLambda(this, [this]() 
		{
			MarkPackageDirty();
			if (PatchManager.IsValid())
			{
				PatchManager->MarkDirtyIfModifiedInConstructionScript();
			}
		})));
	}
}

void ULandscapePatchComponent::OnComponentCreated()
{
	using namespace LandscapePatchComponentLocals;

	Super::OnComponentCreated();

	// Mark whether we're creating from scratch or from a copy
	bWasCopy = bPropertiesCopiedIndicator;
	bPropertiesCopiedIndicator = true;

	if (!IsRealPatch())
	{
		return;
	}

	// Doing stuff during construction script reruns is a huge pain, and ideally we would always exit right here.
	// However we preserve legacy behavior where patches added themselves to the manager from OnComponentCreated
	// even in the construction script, if bAllowAutoFindOfManagerInConstructionScript is true.
	// See ApplyComponentInstanceData to see how we try to detect that patch got modified in the construction
	// script and therefore needs dirtying.
	if (GIsReconstructingBlueprintInstances && !bAllowAutoFindOfManagerInConstructionScript)
	{
		return;
	}

	SetUpPatchManagerData();
}

void ULandscapePatchComponent::SetUpPatchManagerData()
{
	using namespace LandscapePatchComponentLocals;

	if (!IsRealPatch())
	{
		return;
	}

	if (PatchManager.IsValid())
	{
		if (Landscape != PatchManager->GetOwningLandscape())
		{
			Modify();
			Landscape = PatchManager->GetOwningLandscape();
		}

		// The patch manager might legitimately not contain the set patch manager if we're copying a patch. 
		if (!PatchManager->ContainsPatch(this))
		{
			// Note: in blueprint-contained patches, this can actually fix a case where our patch manager was
			// saved incorrectly, but we need the patch manager to be dirtied if we're failing to dirty due to
			// a load.
			if (GIsReconstructingBlueprintInstances)
			{
				UE_LOG(LogLandscapePatch, Warning, TEXT("Patch was not in the set Patch Manager and was added in a construction script rerun. "
					"The patch manager should be saved to avoid potential reorderings depending on loading order. (Package: %s, Manager: %s"),
					*GetNameSafe(PatchManager->GetPackage()), *GetNameSafe(PatchManager.Get()));
			}

			PatchManager->AddPatch(this);
		}

		// No more adjustments needed
		return;
	}

	// If we got here, we don't have a patch manager. See if we have a landscape to get it from.
	if (Landscape.IsValid())
	{
		SetLandscape(Landscape.Get());
		return;
	}

	// See if the level has a height patch manager to which we can add ourselves
	UWorld* World = GetWorld();
	if (!ensure(World))
	{
		return;
	}
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
				Modify();
				Landscape = *LandscapeIterator;
				SetPatchManager(CreateNewPatchManagerForLandscape(Landscape.Get()));
				break;
			}
		}
	}
	if (!PatchManager.IsValid())
	{
		UE_LOG(LogLandscapePatch, Warning, TEXT("Unable to find a landscape with edit layers enabled. Unable to create patch manager."));
	}
}

void ULandscapePatchComponent::MarkDirtyIfModifiedInConstructionScript()
{
	if (IsRealPatch() && bDirtiedByConstructionScript)
	{
		MarkPackageDirty();
	}
}

void ULandscapePatchComponent::PostLoad()
{
	Super::PostLoad();

	bPropertiesCopiedIndicator = true;
	bLoadedButNotYetRegistered = true;
}

void ULandscapePatchComponent::PreSave(FObjectPreSaveContext SaveContext)
{
	Super::PreSave(SaveContext);

	// If we're ssaving, then we no longer have to worry about our dirtiness.
	bDirtiedByConstructionScript = false;
}

void ULandscapePatchComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	if (PatchManager.IsValid() && !GIsReconstructingBlueprintInstances)
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

void ULandscapePatchComponent::OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport)
{
	Super::OnUpdateTransform(UpdateTransformFlags, Teleport);

	if (IsEnabled())
	{
		RequestLandscapeUpdate(/* bInUserTriggeredUpdate = */ true);
	}
}

void ULandscapePatchComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	using namespace LandscapePatchComponentLocals;

	// Do a bunch of checks to make sure that we don't try to do anything when the editing is happening inside the blueprint editor.
	if (!IsRealPatch())
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
		RequestLandscapeUpdate(/* bInUserTriggeredUpdate = */ true);
	}

	// It is important that this super call happen after the above, because inside a blueprint actor, the call triggers a
	// rerun of the construction scripts, which will destroy the component and mess with our ability to do the above adjustments
	// properly (IsValid(this) returns false, the patch manager has the patch removed so it complains when we try to trigger
	// the update, etc).
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif // WITH_EDITOR

FLandscapePatchComponentInstanceData::FLandscapePatchComponentInstanceData(const ULandscapePatchComponent* Patch)
: FSceneComponentInstanceData(Patch)
{
#if WITH_EDITOR
	using namespace LandscapePatchComponentLocals;

	if (!ensure(Patch))
	{
		return;
	}

	bGaveMissingPatchManagerWarning = Patch->bGaveMissingPatchManagerWarning;
	bGaveNotInPatchManagerWarning = Patch->bGaveNotInPatchManagerWarning;
	bGaveMissingLandscapeWarning = Patch->bGaveMissingLandscapeWarning;

	bDirtiedByConstructionScript = Patch->bDirtiedByConstructionScript;

	PatchManager = Patch->GetPatchManager();
	if (PatchManager)
	{
		IndexInManager = PatchManager->GetIndexOfPatch(Patch);
	}
#endif
}

// Called after rerunning construction scripts (when patch is part of a blueprint) to carry over extra data.
void ULandscapePatchComponent::ApplyComponentInstanceData(FLandscapePatchComponentInstanceData* ComponentInstanceData)
{
#if WITH_EDITOR
	using namespace LandscapePatchComponentLocals;

	if (!ComponentInstanceData || !IsRealPatch())
	{
		return;
	}

	bGaveMissingPatchManagerWarning = ComponentInstanceData->bGaveMissingPatchManagerWarning;
	bGaveNotInPatchManagerWarning = ComponentInstanceData->bGaveNotInPatchManagerWarning;
	bGaveMissingLandscapeWarning = ComponentInstanceData->bGaveMissingLandscapeWarning;

	bDirtiedByConstructionScript = ComponentInstanceData->bDirtiedByConstructionScript;

	if (!bAllowAutoFindOfManagerInConstructionScript)
	{
		// This is the ideal case where we don't have to worry about auto acquired manager and we
		// don't expect to have to modify the previous one in any way.
		PatchManager = ComponentInstanceData->PatchManager;
		PreviousPatchManager = PatchManager;
		Landscape = PatchManager.IsValid() ? PatchManager->GetOwningLandscape() : nullptr;

		if (PatchManager.IsValid() && ComponentInstanceData->IndexInManager >= 0)
		{
			int32 CurrentIndexInManager = PatchManager->GetIndexOfPatch(this);
			if (!ensure(CurrentIndexInManager >= 0))
			{
				PatchManager->AddPatch(this);
				PatchManager->MarkModifiedInConstructionScript();
			}
			if (!ensure(CurrentIndexInManager == ComponentInstanceData->IndexInManager))
			{
				PatchManager->MovePatchToIndex(this, ComponentInstanceData->IndexInManager);
				PatchManager->MarkModifiedInConstructionScript();
			}
		}
		return;
	}

	// If we got here, then we're going down a legacy path where we auto acquire patch manager
	// in construction script.

	if (ComponentInstanceData->PatchManager.IsValid())
	{
		// We had an existing patch manager so use that. This is mostly like the above except that we
		// have to remove ourselves from an acquired manager if there is one, and we automatically
		// add ourselves to our patch manager if it was saved without this patch. Calling SetPatchManager
		// does all of that for us, except we need to track whether we modified the patch manager we keep.

		bool bModifiedPatchManager = !ComponentInstanceData->PatchManager->ContainsPatch(this) 
			|| ComponentInstanceData->PatchManager->GetIndexOfPatch(this) != ComponentInstanceData->IndexInManager;

		SetPatchManager(ComponentInstanceData->PatchManager.Get());

		PatchManager->MovePatchToIndex(this, ComponentInstanceData->IndexInManager);

		if (bModifiedPatchManager)
		{
			ComponentInstanceData->PatchManager->MarkModifiedInConstructionScript();

			// This might get double logged because InstanceDataCache->ApplyToActor gets run twice (see AActor::ExecuteConstruction),
			// but that's ok. Also we could check for current dirtyness to avoid the "might not have" wording,
			// but we want to log a warning either way, so probably not worth it.
			UE_LOG(LogLandscapePatch, Warning, TEXT("Patch was missing from expected patch manager and was added in a "
				"construction script rerun but manager might not have been marked dirty. Save the manager to avoid "
				"potential reorderings depending on loading order. (Package: %s, Actor: %s"),
				*GetNameSafe(GetPackage()), *GetNameSafe(GetAttachmentRootActor()));
		}
		return;
	}

	// If we got to here, then we're keeping the patch manager we auto acquired.
	if (PatchManager.IsValid())
	{
		bDirtiedByConstructionScript = true;
		PatchManager->MarkModifiedInConstructionScript();

		// See comment on the other log statement above
		UE_LOG(LogLandscapePatch, Warning, TEXT("Patch acquired a patch manager in a construction script rerun but might not have been "
			"marked dirty. Save the patch and its manager to avoid potential reorderings depending on loading order. (Package: %s, Actor: %s"),
			*GetNameSafe(GetPackage()), *GetNameSafe(GetAttachmentRootActor()));
	}
	
#endif // WITH_EDITOR
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

	Modify();

	Landscape = NewLandscape;

	if (!NewLandscape)
	{
		SetPatchManager(nullptr);
		return;
	}

	if (!IsRealPatch())
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

	if (PreviousPatchManager == NewPatchManager 
		&& PatchManager == NewPatchManager
		&& !(NewPatchManager && !NewPatchManager->ContainsPatch(this)))
	{
		return;
	}

	ResetWarnings();

	if (PreviousPatchManager.IsValid())
	{
		PreviousPatchManager->RemovePatch(this);
	}

	Modify();
	PatchManager = NewPatchManager;
	if (NewPatchManager)
	{
		if (IsRealPatch())
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

void ULandscapePatchComponent::RequestLandscapeUpdate(bool bInUserTriggeredUpdate)
{
	using namespace LandscapePatchComponentLocals;

#if WITH_EDITOR
	// Note that aside from the usual guard against doing things in the blueprint editor, the check of WorldType
	// inside this call also prevents us from doing the request while cooking, where WorldType is set to Inactive. Otherwise
	// we would issue a warning below while cooking when PatchManager is not saved (and not reset in the construction
	// script).
	if (!IsRealPatch())
	{
		return;
	}

	if (!PatchManager.IsValid())
	{
		if (!bGaveMissingPatchManagerWarning)
		{
			UE_LOG(LogLandscapePatch, Warning, TEXT("Patch does not have a valid patch manager. "
				"Set the landscape or patch manager on the patch. (Package: %s, Actor: %s"), 
				*GetNameSafe(GetPackage()), *GetNameSafe(GetAttachmentRootActor()));
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
				*GetNameSafe(GetPackage()), *GetNameSafe(GetAttachmentRootActor()));
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
				*GetNameSafe(GetPackage()), *GetNameSafe(PatchManager.Get()));
			bGaveMissingLandscapeWarning = true;
		}
		bRequestUpdate = false;
	}

	if(bRequestUpdate)
	{
		ResetWarnings();
		PatchManager->RequestLandscapeUpdate(bInUserTriggeredUpdate);
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

bool ULandscapePatchComponent::IsRealPatch()
{
	UWorld* World = GetWorld();
	return !IsTemplate() && IsValid(this) && IsValid(World) && World->WorldType == EWorldType::Editor;
}

FTransform ULandscapePatchComponent::GetLandscapeHeightmapCoordsToWorld() const
{
	if (PatchManager.IsValid())
	{
		return PatchManager->GetHeightmapCoordsToWorld();
	}
	return FTransform::Identity;
}

#undef LOCTEXT_NAMESPACE

