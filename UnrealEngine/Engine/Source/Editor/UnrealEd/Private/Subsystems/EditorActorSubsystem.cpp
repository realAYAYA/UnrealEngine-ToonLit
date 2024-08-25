// Copyright Epic Games, Inc. All Rights Reserved.

#include "Subsystems/EditorActorSubsystem.h"

#include "CoreMinimal.h"
#include "Engine/MeshMerging.h"
#include "GameFramework/Actor.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UnrealEdGlobals.h"
#include "ScopedTransaction.h"
#include "Engine/Brush.h"
#include "Editor.h"
#include "Editor/UnrealEdEngine.h"
#include "AssetSelection.h"
#include "Subsystems/UnrealEditorSubsystem.h"
#include "UObject/UObjectIterator.h"
#include "Utils.h"
#include "ActorEditorUtils.h"
#include "Layers/LayersSubsystem.h"
#include "EditorScriptingHelpers.h"
#include "EngineUtils.h"
#include "Misc/FileHelper.h"
#include "Engine/Selection.h"
#include "LevelEditorViewport.h"
#include "Settings/LevelEditorViewportSettings.h"
#include "Settings/LevelEditorMiscSettings.h"
#include "Subsystems/EditorElementSubsystem.h"
#include "UObject/Stack.h"

#include "Elements/Framework/EngineElementsLibrary.h"
#include "Elements/Framework/TypedElementHandle.h"

#define LOCTEXT_NAMESPACE "EditorActorUtilities"

namespace InternalActorUtilitiesSubsystemLibrary
{
	template<class T>
	bool IsEditorLevelActor(T* Actor)
	{
		bool bResult = false;
		if (Actor && IsValidChecked(Actor))
		{
			UWorld* World = Actor->GetWorld();
			if (World && World->WorldType == EWorldType::Editor)
			{
				bResult = true;
			}
		}
		return bResult;
	}

	template<class T>
	TArray<T*> GetAllLoadedObjects()
	{
		TArray<T*> Result;

		if (!EditorScriptingHelpers::CheckIfInEditorAndPIE())
		{
			return Result;
		}

		const EObjectFlags ExcludeFlags = RF_ClassDefaultObject;
		for (TObjectIterator<T> It(ExcludeFlags, true, EInternalObjectFlags::Garbage); It; ++It)
		{
			T* Obj = *It;
			if (InternalActorUtilitiesSubsystemLibrary::IsEditorLevelActor(Obj))
			{
				Result.Add(Obj);
			}
		}

		return Result;
	}

	AActor* SpawnActor(const TCHAR* MessageName, UObject* ObjToUse, FVector Location, FRotator Rotation, bool bTransient)
	{
		UUnrealEditorSubsystem* UnrealEditorSubsystem = GEditor->GetEditorSubsystem<UUnrealEditorSubsystem>();

		if (!UnrealEditorSubsystem || !EditorScriptingHelpers::CheckIfInEditorAndPIE())
		{
			return nullptr;
		}

		if (!ObjToUse)
		{
			UE_LOG(LogUtils, Error, TEXT("%s. ObjToUse is not valid."), MessageName);
			return nullptr;
		}

		UWorld* World = UnrealEditorSubsystem->GetEditorWorld();
		if (!World)
		{
			UE_LOG(LogUtils, Error, TEXT("%s. Can't spawn the actor because there is no world."), MessageName);
			return nullptr;
		}

		ULevel* DesiredLevel = World->GetCurrentLevel();
		if (!DesiredLevel)
		{
			UE_LOG(LogUtils, Error, TEXT("%s. Can't spawn the actor because there is no Level."), MessageName);
			return nullptr;
		}

		GEditor->ClickLocation = Location;
		GEditor->ClickPlane = FPlane(Location, FVector::UpVector);

		EObjectFlags NewObjectFlags = RF_Transactional;

		if (bTransient)
		{
			NewObjectFlags |= RF_Transient;
		}

		UActorFactory* FactoryToUse = nullptr;
		bool bSelectActors = false;
		TArray<AActor*> Actors = FLevelEditorViewportClient::TryPlacingActorFromObject(DesiredLevel, ObjToUse, bSelectActors, NewObjectFlags, FactoryToUse);

		if (Actors.Num() == 0 || Actors[0] == nullptr)
		{
			UE_LOG(LogUtils, Warning, TEXT("%s. No actor was spawned."), MessageName);
			return nullptr;
		}

		for (AActor* Actor : Actors)
		{
			if (Actor)
			{
				Actor->SetActorLocationAndRotation(Location, Rotation, false, nullptr, ETeleportType::TeleportPhysics);
			}
		}

		return Actors[0];
	}
}

void UEditorActorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	FEditorDelegates::OnNewActorsDropped.AddUObject(this, &UEditorActorSubsystem::BroadcastEditNewActorsDropped);
	FEditorDelegates::OnNewActorsPlaced.AddUObject(this, &UEditorActorSubsystem::BroadcastEditNewActorsPlaced);

	FEditorDelegates::OnEditCutActorsBegin.AddUObject(this, &UEditorActorSubsystem::BroadcastEditCutActorsBegin);
	FEditorDelegates::OnEditCutActorsEnd.AddUObject(this, &UEditorActorSubsystem::BroadcastEditCutActorsEnd);

	FEditorDelegates::OnEditCopyActorsBegin.AddUObject(this, &UEditorActorSubsystem::BroadcastEditCopyActorsBegin);
	FEditorDelegates::OnEditCopyActorsEnd.AddUObject(this, &UEditorActorSubsystem::BroadcastEditCopyActorsEnd);

	FEditorDelegates::OnEditPasteActorsBegin.AddUObject(this, &UEditorActorSubsystem::BroadcastEditPasteActorsBegin);
	FEditorDelegates::OnEditPasteActorsEnd.AddUObject(this, &UEditorActorSubsystem::BroadcastEditPasteActorsEnd);

	FEditorDelegates::OnDuplicateActorsBegin.AddUObject(this, &UEditorActorSubsystem::BroadcastDuplicateActorsBegin);
	FEditorDelegates::OnDuplicateActorsEnd.AddUObject(this, &UEditorActorSubsystem::BroadcastDuplicateActorsEnd);

	FEditorDelegates::OnDeleteActorsBegin.AddUObject(this, &UEditorActorSubsystem::BroadcastDeleteActorsBegin);
	FEditorDelegates::OnDeleteActorsEnd.AddUObject(this, &UEditorActorSubsystem::BroadcastDeleteActorsEnd);
}

void UEditorActorSubsystem::Deinitialize()
{
	FEditorDelegates::OnNewActorsDropped.RemoveAll(this);
	FEditorDelegates::OnNewActorsPlaced.RemoveAll(this);
	FEditorDelegates::OnEditCutActorsBegin.RemoveAll(this);
	FEditorDelegates::OnEditCutActorsEnd.RemoveAll(this);
	FEditorDelegates::OnEditCopyActorsBegin.RemoveAll(this);
	FEditorDelegates::OnEditCopyActorsEnd.RemoveAll(this);
	FEditorDelegates::OnEditPasteActorsBegin.RemoveAll(this);
	FEditorDelegates::OnEditPasteActorsEnd.RemoveAll(this);
	FEditorDelegates::OnDuplicateActorsBegin.RemoveAll(this);
	FEditorDelegates::OnDuplicateActorsEnd.RemoveAll(this);
	FEditorDelegates::OnDeleteActorsBegin.RemoveAll(this);
	FEditorDelegates::OnDeleteActorsEnd.RemoveAll(this);

}

/** To fire before an Actor is Dropped */
void UEditorActorSubsystem::BroadcastEditNewActorsDropped(const TArray<UObject*>& DroppedObjects, const TArray<AActor*>& DroppedActors)
{
	OnNewActorsDropped.Broadcast(DroppedObjects, DroppedActors);
}

/** To fire before an Actor is Placed */
void UEditorActorSubsystem::BroadcastEditNewActorsPlaced(UObject* ObjToUse, const TArray<AActor*>& PlacedActors)
{
	OnNewActorsPlaced.Broadcast(ObjToUse, PlacedActors);
}

/** To fire before an Actor is Cut */
void UEditorActorSubsystem::BroadcastEditCutActorsBegin()
{
	OnEditCutActorsBegin.Broadcast();
}

/** To fire after an Actor is Cut */
void UEditorActorSubsystem::BroadcastEditCutActorsEnd()
{
	OnEditCutActorsEnd.Broadcast();
}

/** To fire before an Actor is Copied */
void UEditorActorSubsystem::BroadcastEditCopyActorsBegin()
{
	OnEditCopyActorsBegin.Broadcast();
}

/** To fire after an Actor is Copied */
void UEditorActorSubsystem::BroadcastEditCopyActorsEnd()
{
	OnEditCopyActorsEnd.Broadcast();
}

/** To fire before an Actor is Pasted */
void UEditorActorSubsystem::BroadcastEditPasteActorsBegin()
{
	OnEditPasteActorsBegin.Broadcast();
}

/** To fire after an Actor is Pasted */
void UEditorActorSubsystem::BroadcastEditPasteActorsEnd()
{
	OnEditPasteActorsEnd.Broadcast();
}

/** To fire before an Actor is duplicated */
void UEditorActorSubsystem::BroadcastDuplicateActorsBegin()
{
	OnDuplicateActorsBegin.Broadcast();
}

/** To fire after an Actor is duplicated */
void UEditorActorSubsystem::BroadcastDuplicateActorsEnd()
{
	OnDuplicateActorsEnd.Broadcast();
}

/** To fire before an Actor is Deleted */
void UEditorActorSubsystem::BroadcastDeleteActorsBegin()
{
	OnDeleteActorsBegin.Broadcast();
}

/** To fire after an Actor is Deleted */
void UEditorActorSubsystem::BroadcastDeleteActorsEnd()
{
	OnDeleteActorsEnd.Broadcast();
}

void UEditorActorSubsystem::DuplicateSelectedActors(UWorld* InWorld)
{
	if (!GEditor || !InWorld)
	{
		return;
	}

	bool bComponentsSelected = GEditor->GetSelectedComponentCount() > 0;
	//@todo locked levels - if all actor levels are locked, cancel the transaction
	const FScopedTransaction Transaction(bComponentsSelected ? NSLOCTEXT("UnrealEd", "DuplicateComponents", "Duplicate Components") : NSLOCTEXT("UnrealEd", "DuplicateActors", "Duplicate Actors"));

	FEditorDelegates::OnDuplicateActorsBegin.Broadcast();

	// duplicate selected
	ABrush::SetSuppressBSPRegeneration(true);
	GEditor->edactDuplicateSelected(InWorld->GetCurrentLevel(), GetDefault<ULevelEditorViewportSettings>()->GridEnabled);
	ABrush::SetSuppressBSPRegeneration(false);

	// Find out if any of the selected actors will change the BSP.
	// and only then rebuild BSP as this is expensive. 
	const FSelectedActorInfo& SelectedActors = AssetSelectionUtils::GetSelectedActorInfo();
	if (SelectedActors.bHaveBrush)
	{
		GEditor->RebuildAlteredBSP(); // Update the Bsp of any levels containing a modified brush
	}

	FEditorDelegates::OnDuplicateActorsEnd.Broadcast();

	GEditor->RedrawLevelEditingViewports();
}

void UEditorActorSubsystem::DeleteSelectedActors(UWorld* InWorld)
{
	if (!GEditor || !InWorld)
	{
		return;
	}

	bool bComponentsSelected = GEditor->GetSelectedComponentCount() > 0;

	const FScopedTransaction Transaction(bComponentsSelected ? NSLOCTEXT("UnrealEd", "DeleteComponents", "Delete Components") : NSLOCTEXT("UnrealEd", "DeleteActors", "Delete Actors"));

	FEditorDelegates::OnDeleteActorsBegin.Broadcast();
	const bool bCheckRef = GetDefault<ULevelEditorMiscSettings>()->bCheckReferencesOnDelete;
	GEditor->edactDeleteSelected(InWorld, true, bCheckRef, bCheckRef);
	FEditorDelegates::OnDeleteActorsEnd.Broadcast();
}

void UEditorActorSubsystem::InvertSelection(UWorld* InWorld)
{
	if (!GUnrealEd || !InWorld)
	{
		return;
	}

	const FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "SelectInvert", "Select Invert"));
	GUnrealEd->edactSelectInvert(InWorld);
}

void UEditorActorSubsystem::SelectAll(UWorld* InWorld)
{
	if (!GUnrealEd || !InWorld)
	{
		return;
	}

	const FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "SelectAll", "Select All"));
	GUnrealEd->edactSelectAll(InWorld);
}

void UEditorActorSubsystem::SelectAllChildren(bool bRecurseChildren)
{
	if (!GUnrealEd)
	{
		return;
	}

	FText TransactionLabel;
	if (bRecurseChildren)
	{
		TransactionLabel = NSLOCTEXT("UnrealEd", "SelectAllDescendants", "Select All Descendants");
	}
	else
	{
		TransactionLabel = NSLOCTEXT("UnrealEd", "SelectAllChildren", "Select All Children");
	}

	const FScopedTransaction Transaction(TransactionLabel);
	GUnrealEd->edactSelectAllChildren(bRecurseChildren);
}

TArray<AActor*> UEditorActorSubsystem::GetAllLevelActors()
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);
	TArray<AActor*> Result;

	UUnrealEditorSubsystem* UnrealEditorSubsystem = GEditor->GetEditorSubsystem<UUnrealEditorSubsystem>();

	if (UnrealEditorSubsystem && EditorScriptingHelpers::CheckIfInEditorAndPIE())
	{
		//Default iterator only iterates over active levels.
		const EActorIteratorFlags Flags = EActorIteratorFlags::SkipPendingKill;
		for (TActorIterator<AActor> It(UnrealEditorSubsystem->GetEditorWorld(), AActor::StaticClass(), Flags); It; ++It)
		{
			AActor* Actor = *It;
			if (Actor->IsEditable() &&
				Actor->IsListedInSceneOutliner() &&					// Only add actors that are allowed to be selected and drawn in editor
				!Actor->IsTemplate() &&								// Should never happen, but we never want CDOs
				!Actor->HasAnyFlags(RF_Transient) &&				// Don't add transient actors in non-play worlds
				!FActorEditorUtils::IsABuilderBrush(Actor) &&		// Don't add the builder brush
				!Actor->IsA(AWorldSettings::StaticClass()))			// Don't add the WorldSettings actor, even though it is technically editable
			{
				Result.Add(*It);
			}
		}
	}

	return Result;
}

TArray<UActorComponent*> UEditorActorSubsystem::GetAllLevelActorsComponents()
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingHelpers::CheckIfInEditorAndPIE())
	{
		return TArray<UActorComponent*>();
	}

	return InternalActorUtilitiesSubsystemLibrary::GetAllLoadedObjects<UActorComponent>();
}

TArray<AActor*> UEditorActorSubsystem::GetSelectedLevelActors()
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	TArray<AActor*> Result;

	if (!EditorScriptingHelpers::CheckIfInEditorAndPIE())
	{
		return Result;
	}

	for (FSelectionIterator Iter(*GEditor->GetSelectedActors()); Iter; ++Iter)
	{
		AActor* Actor = Cast<AActor>(*Iter);
		if (InternalActorUtilitiesSubsystemLibrary::IsEditorLevelActor(Actor))
		{
			Result.Add(Actor);
		}
	}

	return Result;
}

void UEditorActorSubsystem::SetSelectedLevelActors(const TArray<class AActor*>& ActorsToSelect)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	TArray<AActor*> Result;

	if (!EditorScriptingHelpers::CheckIfInEditorAndPIE())
	{
		return;
	}

	if (GEdSelectionLock)
	{
		UE_LOG(LogUtils, Warning, TEXT("SetSelectedLevelActors. The editor selection is currently locked."));
		return;
	}

	USelection* SelectedActors = GEditor->GetSelectedActors();
	SelectedActors->Modify();
	if (ActorsToSelect.Num() > 0)
	{
		SelectedActors->BeginBatchSelectOperation();
		GEditor->SelectNone(false, true, false);
		for (AActor* Actor : ActorsToSelect)
		{
			if (InternalActorUtilitiesSubsystemLibrary::IsEditorLevelActor(Actor))
			{
				if (!GEditor->CanSelectActor(Actor, true))
				{
					UE_LOG(LogUtils, Warning, TEXT("SetSelectedLevelActors. Can't select actor '%s'."), *Actor->GetName());
					continue;
				}
				GEditor->SelectActor(Actor, true, false);
			}
		}
		SelectedActors->EndBatchSelectOperation();
		GEditor->NoteSelectionChange();
	}
	else
	{
		GEditor->SelectNone(true, true, false);
	}
}

void UEditorActorSubsystem::ClearActorSelectionSet()
{
	GEditor->GetSelectedActors()->Modify();
	GEditor->GetSelectedActors()->DeselectAll();
	GEditor->NoteSelectionChange();
}

void UEditorActorSubsystem::SelectNothing()
{
	GEditor->GetSelectedActors()->Modify();
	GEditor->SelectNone(true, true, false);
}

void UEditorActorSubsystem::SetActorSelectionState(AActor* Actor, bool bShouldBeSelected)
{
	GEditor->GetSelectedActors()->Modify();
	GEditor->SelectActor(Actor, bShouldBeSelected, /*bNotify=*/ false);
	GEditor->NoteSelectionChange();
}

AActor* UEditorActorSubsystem::GetActorReference(FString PathToActor)
{
	return Cast<AActor>(StaticFindObject(AActor::StaticClass(), GEditor->GetEditorWorldContext().World(), *PathToActor, false));
}


AActor* UEditorActorSubsystem::SpawnActorFromObject(UObject* ObjToUse, FVector Location, FRotator Rotation, bool bTransient)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingHelpers::CheckIfInEditorAndPIE())
	{
		return nullptr;
	}

	if (!ObjToUse)
	{
		UE_LOG(LogUtils, Error, TEXT("SpawnActorFromObject. ObjToUse is not valid."));
		return nullptr;
	}

	return InternalActorUtilitiesSubsystemLibrary::SpawnActor(TEXT("SpawnActorFromObject"), ObjToUse, Location, Rotation, bTransient);
}

AActor* UEditorActorSubsystem::SpawnActorFromClass(TSubclassOf<class AActor> ActorClass, FVector Location, FRotator Rotation, bool bTransient)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingHelpers::CheckIfInEditorAndPIE())
	{
		return nullptr;
	}

	if (!ActorClass.Get())
	{
		FFrame::KismetExecutionMessage(TEXT("SpawnActorFromClass. ActorClass is not valid."), ELogVerbosity::Error);
		return nullptr;
	}

	return InternalActorUtilitiesSubsystemLibrary::SpawnActor(TEXT("SpawnActorFromClass"), ActorClass.Get(), Location, Rotation, bTransient);
}

bool UEditorActorSubsystem::DestroyActor(AActor* ActorToDestroy)
{
	TArray<AActor*> ActorsToDestroy;
	ActorsToDestroy.Add(ActorToDestroy);
	return DestroyActors(ActorsToDestroy);
}

bool UEditorActorSubsystem::DestroyActors(const TArray<AActor*>& ActorsToDestroy)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	const FScopedTransaction Transaction(LOCTEXT("DeleteActors", "Delete Actors"));
	
	if (!EditorScriptingHelpers::CheckIfInEditorAndPIE())
	{
		return false;
	}

	for (AActor* ActorToDestroy : ActorsToDestroy)
	{
		if (!ActorToDestroy)
		{
			UE_LOG(LogUtils, Error, TEXT("DestroyActors. An actor to destroy is invalid."));
			return false;
		}
		if (!InternalActorUtilitiesSubsystemLibrary::IsEditorLevelActor(ActorToDestroy))
		{  
			UE_LOG(LogUtils, Error, TEXT("DestroyActors. An actor to destroy is not part of the world editor."));
			return false;
		}
	}

	UUnrealEditorSubsystem* UnrealEditorSubsystem = GEditor->GetEditorSubsystem<UUnrealEditorSubsystem>();

	if (!UnrealEditorSubsystem)
	{
		return false;
	}

	UWorld* World = UnrealEditorSubsystem->GetEditorWorld();
	if (!World)
	{
		UE_LOG(LogUtils, Error, TEXT("DestroyActors. Can't destroy actors because there is no world."));
		return false;
	}

	FEditorDelegates::OnDeleteActorsBegin.Broadcast();
	
	// Make sure these actors are no longer selected
	USelection* ActorSelection = GEditor->GetSelectedActors();
	if (UTypedElementSelectionSet* SelectionSet = ActorSelection->GetElementSelectionSet())
	{
		TArray<FTypedElementHandle> ActorHandles;
		ActorHandles.Reserve(ActorsToDestroy.Num());
		for (AActor* ActorToDestroy : ActorsToDestroy)
		{
			if (FTypedElementHandle ActorHandle = UEngineElementsLibrary::AcquireEditorActorElementHandle(ActorToDestroy, /*bAllowCreate*/false))
			{
				ActorHandles.Add(ActorHandle);
			}
		}
	
		const FTypedElementSelectionOptions SelectionOptions = FTypedElementSelectionOptions()
			.SetAllowHidden(true)
			.SetAllowGroups(false)
			.SetWarnIfLocked(false)
			.SetChildElementInclusionMethod(ETypedElementChildInclusionMethod::Recursive);

		SelectionSet->DeselectElements(ActorHandles, SelectionOptions);
	}

	ULayersSubsystem* Layers = GEditor->GetEditorSubsystem<ULayersSubsystem>();
	Layers->DisassociateActorsFromLayers(ActorsToDestroy);

	bool SuccessfullyDestroyedAll = true;
	
	for (AActor* ActorToDestroy : ActorsToDestroy)
	{
		if (!World->EditorDestroyActor(ActorToDestroy, true))
		{
			SuccessfullyDestroyedAll = false;
		}
	}

	FEditorDelegates::OnDeleteActorsEnd.Broadcast();
	
	return SuccessfullyDestroyedAll;
}

TArray<class AActor*> UEditorActorSubsystem::ConvertActors(const TArray<class AActor*>& Actors, TSubclassOf<class AActor> ActorClass, const FString& StaticMeshPackagePath)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	TArray<class AActor*> Result;

	if (!EditorScriptingHelpers::CheckIfInEditorAndPIE())
	{
		return Result;
	}

	if (ActorClass.Get() == nullptr)
	{
		UE_LOG(LogUtils, Error, TEXT("ConvertActorWith. The ActorClass is not valid."));
		return Result;
	}

	UUnrealEditorSubsystem* UnrealEditorSubsystem = GEditor->GetEditorSubsystem<UUnrealEditorSubsystem>();

	if (!UnrealEditorSubsystem)
	{
		return Result;
	}

	FString PackagePath = StaticMeshPackagePath;
	if (!PackagePath.IsEmpty())
	{
		FString FailureReason;
		PackagePath = EditorScriptingHelpers::ConvertAnyPathToLongPackagePath(PackagePath, FailureReason);
		if (PackagePath.IsEmpty())
		{
			UE_LOG(LogUtils, Error, TEXT("ConvertActorWith. %s"), *FailureReason);
			return Result;
		}
	}

	TArray<class AActor*> ActorToConvert;
	ActorToConvert.Reserve(Actors.Num());
	for (AActor* Actor : Actors)
	{
		if (Actor == nullptr || !IsValidChecked(Actor))
		{
			continue;
		}

		UWorld* ActorWorld = Actor->GetWorld();
		if (ActorWorld == nullptr)
		{
			UE_LOG(LogUtils, Warning, TEXT("ConvertActorWith. %s is not in a world. The actor will be skipped."), *Actor->GetActorLabel());
			continue;
		}
		if (ActorWorld->WorldType != EWorldType::Editor)
		{
			UE_LOG(LogUtils, Warning, TEXT("ConvertActorWith. %s is not in an editor world. The actor will be skipped."), *Actor->GetActorLabel());
			continue;
		}

		ULevel* CurrentLevel = Actor->GetLevel();
		if (CurrentLevel == nullptr)
		{
			UE_LOG(LogUtils, Warning, TEXT("ConvertActorWith. %s must be in a valid level. The actor will be skipped."), *Actor->GetActorLabel());
			continue;
		}

		if (Cast<ABrush>(Actor) && PackagePath.Len() == 0)
		{
			UE_LOG(LogUtils, Warning, TEXT("ConvertActorWith. %s is a Brush and not package path was provided. The actor will be skipped."), *Actor->GetActorLabel());
			continue;
		}

		ActorToConvert.Add(Actor);
	}

	if (ActorToConvert.Num() != 0)
	{
		const bool bUseSpecialCases = false; // Don't use special cases, they are a bit too exhaustive and create dialog
		GEditor->DoConvertActors(ActorToConvert, ActorClass.Get(), TSet<FString>(), bUseSpecialCases, StaticMeshPackagePath);
		Result.Reserve(GEditor->GetSelectedActorCount());
		for (auto Itt = GEditor->GetSelectedActorIterator(); Itt; ++Itt)
		{
			Result.Add(CastChecked<AActor>(*Itt));
		}
	}

	UE_LOG(LogUtils, Log, TEXT("ConvertActorWith. %d conversions occurred."), Result.Num());
	return Result;
}

AActor* UEditorActorSubsystem::DuplicateActor(AActor* ActorToDuplicate, UWorld* ToWorld/*= nullptr*/, FVector Offset/* = FVector::ZeroVector*/)
{
	TArray<AActor*> Duplicate = DuplicateActors({ ActorToDuplicate }, ToWorld, Offset);
	return (Duplicate.Num() > 0) ? Duplicate[0] : nullptr;
}

TArray<AActor*> UEditorActorSubsystem::DuplicateActors(const TArray<AActor*>& ActorsToDuplicate, UWorld* InToWorld/*= nullptr*/, FVector Offset/* = FVector::ZeroVector*/)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	FScopedTransaction Transaction(LOCTEXT("DuplicateActors", "Duplicate Actors"));

	UUnrealEditorSubsystem* UnrealEditorSubsystem = GEditor->GetEditorSubsystem<UUnrealEditorSubsystem>();

	if (!UnrealEditorSubsystem || !EditorScriptingHelpers::CheckIfInEditorAndPIE())
	{
		return TArray<AActor*>();
	}

	UWorld* ToWorld = InToWorld ? InToWorld : UnrealEditorSubsystem->GetEditorWorld();

	if (!ToWorld)
	{
		return TArray<AActor*>();
	}

	FEditorDelegates::OnDuplicateActorsBegin.Broadcast();

	TArray<AActor*> NewActors;
	ABrush::SetSuppressBSPRegeneration(true);
	GUnrealEd->DuplicateActors(ActorsToDuplicate, NewActors, ToWorld->GetCurrentLevel(), Offset);
	ABrush::SetSuppressBSPRegeneration(false);

	// Find out if any of the actors will change the BSP.
	// and only then rebuild BSP as this is expensive. 
	if (NewActors.FindItemByClass<ABrush>())
	{
		GEditor->RebuildAlteredBSP(); // Update the BSP of any levels containing a modified brush
	}

	FEditorDelegates::OnDuplicateActorsEnd.Broadcast();

	return NewActors;
}

bool UEditorActorSubsystem::SetActorTransform(AActor* InActor, const FTransform& InWorldTransform)
{
	if (!InActor)
	{
		FFrame::KismetExecutionMessage(TEXT("Cannot set the transfrom of a nullptr actor."), ELogVerbosity::Error);
		return false;
	}

	if (UEditorElementSubsystem* ElementSubsystem = GEditor->GetEditorSubsystem<UEditorElementSubsystem>())
	{
		if (FTypedElementHandle ActorElementHandle = UEngineElementsLibrary::AcquireEditorActorElementHandle(InActor))
		{
			return ElementSubsystem->SetElementTransform(ActorElementHandle, InWorldTransform);
		}
	}

	return false;
}

bool UEditorActorSubsystem::SetComponentTransform(USceneComponent* InSceneComponent, const FTransform& InWorldTransform)
{
	if (!InSceneComponent)
	{
		FFrame::KismetExecutionMessage(TEXT("Cannot set the transform of a nullptr SceneComponent."), ELogVerbosity::Error);
		return false;
	}

	if (UEditorElementSubsystem* ElementSubsystem = GEditor->GetEditorSubsystem<UEditorElementSubsystem>())
	{
		if (FTypedElementHandle ComponentElementHandle = UEngineElementsLibrary::AcquireEditorComponentElementHandle(InSceneComponent))
		{
			return ElementSubsystem->SetElementTransform(ComponentElementHandle, InWorldTransform);
		}
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
