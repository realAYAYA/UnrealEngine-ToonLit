// Copyright Epic Games, Inc. All Rights Reserved.

#include "Playable/Playables/AvaPlayableLevelStreaming.h"

#include "AvaRemoteControlRebind.h"
#include "AvaRemoteControlUtils.h"
#include "AvaScene.h"
#include "Camera/CameraActor.h"
#include "Engine/Level.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/LocalPlayer.h"
#include "Framework/AvaGameInstance.h"
#include "Framework/AvaInstanceSettings.h"
#include "Framework/AvaSoftAssetPtr.h"
#include "IAvaMediaModule.h"
#include "Playable/AvaPlayableGroup.h"
#include "Playable/AvaPlayableGroupManager.h"
#include "Playback/AvaPlaybackUtils.h"
#include "Streaming/LevelStreamingDelegates.h"
#include "UObject/Package.h"
#include "Viewport/AvaCameraManager.h"

#define LOCTEXT_NAMESPACE "AvaPlayableLevelStreaming"

namespace UE::AvaMedia::LevelStreamingPlayable::Private
{
	AAvaScene* FindAvaScene(const ULevel* InLevel)
	{
		AAvaScene* AvaScene = nullptr;
		InLevel->Actors.FindItemByClass(&AvaScene);
		return AvaScene;
	}

	AActor* FindActorByName(const ULevel* InLevel, const FName& InActorName)
	{
		for (AActor* Actor : InLevel->Actors)
		{
			if (Actor && Actor->GetFName() == InActorName)
			{
				return Actor;
			}
		}
		return nullptr;
	}

	bool IsCameraActor(const AActor* InActor)
	{
		return InActor->IsA<ACameraActor>();
	}
	
	AActor* FindFirstCameraActor(const ULevel* InLevel)
	{
		for (AActor* Actor : InLevel->Actors)
		{
			if (Actor && IsCameraActor(Actor))
			{
				return Actor;
			}
		}
		return nullptr;
	}

	FName GetStartupCameraName(const AAvaScene* InAvaScene)
	{
		return InAvaScene ? InAvaScene->GetStartupCameraName() : NAME_None;
	}

	AActor* FindCameraActor(const ULevel* InLevel, FName InStartupCameraName)
	{
		if (InStartupCameraName != NAME_None)
		{
			AActor* CameraActor = FindActorByName(InLevel, InStartupCameraName);
			if (CameraActor && IsCameraActor(CameraActor))
			{
				return CameraActor;
			}
		}
		return nullptr;
	}

	AActor* FindStartupCameraActor(const ULevel* InLevel, FName InStartupCameraName, const AAvaScene* InAvaScene)
	{
		if (!IsValid(InLevel))
		{
			return nullptr;
		}

		// 1- Find by name from the parent scene.
		if (AActor* FoundCameraActor = FindCameraActor(InLevel, InStartupCameraName))
		{
			return FoundCameraActor;
		}

		// 2- Find by name from dependent scene (if any). Dependent levels may not have an ava scene.
		if (AActor* FoundCameraActor = FindCameraActor(InLevel, GetStartupCameraName(InAvaScene)))
		{
			return FoundCameraActor;
		}

		// 3- For now we fallback to looking for the first camera actor in the level.
		return FindFirstCameraActor(InLevel);
	}

	EAvaPlayableStatus GetPlayableStatusFromLevelStreaming(const ULevelStreaming* InLevelStreaming)
	{
		if (!InLevelStreaming)
		{
			return EAvaPlayableStatus::Unloaded;
		}

		switch (InLevelStreaming->GetLevelStreamingState())
		{
			case ELevelStreamingState::Removed:
				return EAvaPlayableStatus::Unloaded;
			
			case ELevelStreamingState::Unloaded:
				// If the LevelStreaming was not loaded and has just been made to be loading, the status will still be "unloaded" but
				// we consider it is loading.
				return InLevelStreaming->ShouldBeLoaded() ? EAvaPlayableStatus::Loading : EAvaPlayableStatus::Unloaded;
			
			case ELevelStreamingState::FailedToLoad:
				return EAvaPlayableStatus::Error;
			
			case ELevelStreamingState::Loading:
				return EAvaPlayableStatus::Loading;
			
			case ELevelStreamingState::LoadedNotVisible:
			case ELevelStreamingState::MakingVisible:
			case ELevelStreamingState::MakingInvisible:
				return EAvaPlayableStatus::Loaded;
			
			case ELevelStreamingState::LoadedVisible:
				return EAvaPlayableStatus::Visible;
			
			default:
				return EAvaPlayableStatus::Error;
		}
	}
	
	static int32 GetPlayableStatusRelevanceShouldBeUnloaded(EAvaPlayableStatus InStatus)
	{
		switch (InStatus)
		{
			case EAvaPlayableStatus::Unknown:
				return 0;
			case EAvaPlayableStatus::Error:
				return 1;
			case EAvaPlayableStatus::Unloaded:
				return 2; // Weakest (desired)
			case EAvaPlayableStatus::Loading:
				return 3;
			case EAvaPlayableStatus::Loaded:
				return 4;
			case EAvaPlayableStatus::Visible:
				return 5;
			default:
				return 0;
		}
	}

	static int32 GetPlayableStatusRelevanceShouldBeLoadedOrVisible(EAvaPlayableStatus InStatus)
	{
		switch (InStatus)
		{
			case EAvaPlayableStatus::Unknown:
				return 0;
			case EAvaPlayableStatus::Error:
				return 1;
			case EAvaPlayableStatus::Unloaded:
				return 5;
			case EAvaPlayableStatus::Loading:
				return 4;
			case EAvaPlayableStatus::Loaded:
				return 3;
			case EAvaPlayableStatus::Visible:
				return 2; // Weakest (desired)
			default:
				return 0;
		}
	}
	
	/**
	 * Returns the relevance of the status for comparison with another status
	 * in order to determine the combined status of a playable according to what it should be.
	 */
	static int32 GetPlayableStatusRelevance(EAvaPlayableStatus InStatus, bool bInShouldBeLoaded, bool bInShouldBeVisible)
	{
		// The "desired" status is the weakest of the valid statues.
		if (bInShouldBeLoaded || bInShouldBeVisible)
		{
			return GetPlayableStatusRelevanceShouldBeLoadedOrVisible(InStatus);
		}
		
		return GetPlayableStatusRelevanceShouldBeUnloaded(InStatus);
	}
}

bool UAvaPlayableLevelStreaming::LoadAsset(const FAvaSoftAssetPtr& InSourceAsset, bool bInInitiallyVisible)
{
	if (!PlayableGroup)
	{
		return false;
	}

	// Ensure world is created. Does nothing if already created.
	PlayableGroup->ConditionalCreateWorld();

	const FAvaInstanceSettings& PlaybackInstanceSettings = IAvaMediaModule::Get().GetAvaInstanceSettings();
	bLoadSubPlayables = PlaybackInstanceSettings.bEnableLoadSubPlayables;
	
	check(InSourceAsset.GetAssetType() == EMotionDesignAssetType::World);
	const bool bAssetLoading = LoadLevel(TSoftObjectPtr<UWorld>(InSourceAsset.ToSoftObjectPath()), bInInitiallyVisible); 
	if (bAssetLoading)
	{
		PlayableGroup->NotifyLevelStreaming(this);
	}
	return bAssetLoading;
}

bool UAvaPlayableLevelStreaming::UnloadAsset()
{
	if (Scene)
	{
		FAvaRemoteControlUtils::UnregisterRemoteControlPreset(Scene->GetRemoteControlPreset());
	}

	UnloadSubPlayables();
	
	// Ripped from UPocketLevelInstance::BeginDestroy()
	if (LevelStreaming)
	{
		LevelStreaming->bShouldBlockOnUnload = false;
		LevelStreaming->SetShouldBeVisible(false);
		LevelStreaming->SetShouldBeLoaded(false);
		LevelStreaming->SetIsRequestingUnloadAndRemoval(true);
		LevelStreaming->OnLevelShown.RemoveAll(this);
		LevelStreaming->OnLevelLoaded.RemoveAll(this);

		const ULevel* const Level = LevelStreaming->GetLoadedLevel();
		if (IsValid(Level) && Level->GetPackage())
		{
			// Hack so that FLevelStreamingGCHelper::PrepareStreamedOutLevelForGC unloads the level.
			Level->GetPackage()->SetPackageFlags(PKG_PlayInEditor);
		}
	}
	LevelStreaming = nullptr;
	Scene = nullptr;
	SourceLevel.Reset();
	return true;
}

EAvaPlayableStatus UAvaPlayableLevelStreaming::GetPlayableStatus() const
{
	using namespace UE::AvaMedia::LevelStreamingPlayable::Private;

	bool bShouldBeLoaded = LevelStreaming ? LevelStreaming->ShouldBeLoaded() : false;
	bool bShouldBeVisible = LevelStreaming ? LevelStreaming->ShouldBeVisible() : false;

	EAvaPlayableStatus MostRelevantStatus = EAvaPlayableStatus::Unknown;
	int32 HighestRelevance = -1;

	auto CompareAndSetMostRelevantStatus = [&MostRelevantStatus, &HighestRelevance, bShouldBeLoaded, bShouldBeVisible](EAvaPlayableStatus InStatus)
	{
		const int32 CurrentRelevance = GetPlayableStatusRelevance(InStatus, bShouldBeLoaded, bShouldBeVisible);
		if (CurrentRelevance > HighestRelevance)
		{
			MostRelevantStatus = InStatus;
			HighestRelevance = CurrentRelevance;
		}
	};
	
	// Check sub playables.
	for (const UAvaPlayableLevelStreaming* SubPlayable : SubPlayables)
	{
		CompareAndSetMostRelevantStatus(SubPlayable->GetPlayableStatus());
	}

	CompareAndSetMostRelevantStatus(GetPlayableStatusFromLevelStreaming(LevelStreaming));

	return MostRelevantStatus;
}

IAvaSceneInterface* UAvaPlayableLevelStreaming::GetSceneInterface() const
{
	if (Scene)
	{
		return static_cast<IAvaSceneInterface*>(Scene);
	}
	return nullptr;
}

bool UAvaPlayableLevelStreaming::ApplyCamera()
{
	using namespace UE::AvaMedia::LevelStreamingPlayable;

	if (!Scene)
	{
		return false;
	}

	bool bSetupDone = false;
	UAvaPlayable* PlayableOwningCamera = nullptr;

	// Camera setup.
	if (AActor* CameraActor = FindStartupCameraActor(NAME_None, &PlayableOwningCamera))
	{
		// New code using player controller.
		// Note: doesn't work yet because there is no player controller in Motion Design game instance, but it works in PIE.
		if (APlayerController* PlayerController = GetPlayableGroup()->GetPlayWorld()->GetFirstPlayerController())
		{
			PlayerController->SetViewTargetWithBlend(CameraActor);
			GetPlayableGroup()->SetLastAppliedCameraPlayable(PlayableOwningCamera);
			bSetupDone = true;
		}
		else
		{
			// If there is no player controller, fallback to Motion Design camera manager, if the game instance has it.
			const UAvaGameInstance* AvaGameInstance = Cast<UAvaGameInstance>(GetPlayableGroup()->GetGameInstance());			
			if (const UAvaGameViewportClient* ViewportClient = AvaGameInstance ? AvaGameInstance->GetAvaGameViewportClient() : nullptr)
			{
				// Old code using ava camera manager. To retire.
				constexpr bool bIsCanvasController = false;
				ViewportClient->GetCameraManager()->Init(Scene->GetPlaybackObject(), bIsCanvasController);
				ViewportClient->GetCameraManager()->SetViewTarget(CameraActor);
				GetPlayableGroup()->SetLastAppliedCameraPlayable(PlayableOwningCamera);
				bSetupDone = true;
				UE_LOG(LogAvaPlayable, Log, TEXT("No player controller found in \"%s\" - Using Ava Camera Manager instead."), *LevelStreaming->PackageNameToLoad.ToString())
			}
			else
			{
				UE_LOG(LogAvaPlayable, Warning, TEXT("No player controller found in \"%s\" - Ava Camera Manager not available either."), *LevelStreaming->PackageNameToLoad.ToString());				
			}
		}
	}
	else
	{
		UE_LOG(LogAvaPlayable, Error, TEXT("Failed to find camera \"%s\", or any camera actor, in loaded level."),
			*Private::GetStartupCameraName(Scene).ToString());
	}
	return bSetupDone;
}

bool UAvaPlayableLevelStreaming::GetShouldBeVisible() const
{
	return LevelStreaming ? LevelStreaming->GetShouldBeVisibleFlag() : false;
}

void UAvaPlayableLevelStreaming::SetShouldBeVisible(bool bInShouldBeVisible)
{
	if (LevelStreaming)
	{
		LevelStreaming->SetShouldBeVisible(bInShouldBeVisible);
	}

	for (UAvaPlayableLevelStreaming* SubPlayable : SubPlayables)
	{
		SubPlayable->UpdateVisibilityFromParents();
	}
}

bool UAvaPlayableLevelStreaming::LoadLevel(const TSoftObjectPtr<UWorld>& InSourceLevel, bool bInInitiallyVisible)
{
	if (SourceLevel == InSourceLevel)
	{
		// Already started loading.
		return false;
	}

	if (!PlayableGroup)
	{
		return false;
	}
	
	bool bSuccess = false;
	const FVector Origin(0,0,0);

	ULevelStreamingDynamic::FLoadLevelInstanceParams Params(PlayableGroup->GetPlayWorld()
			, InSourceLevel.GetLongPackageName()
			, FTransform(FRotator::ZeroRotator, Origin));
	Params.bLoadAsTempPackage = true;
	Params.bInitiallyVisible  = bInInitiallyVisible;

	LevelStreaming = ULevelStreamingDynamic::LoadLevelInstance(Params, bSuccess);

	if (!bSuccess || !LevelStreaming)
	{
		UE_LOG(LogAvaPlayable, Error, TEXT("[%s]: Failed to load level instance `%s`."), *GetPathNameSafe(this), *InSourceLevel.ToString());
		return false;
	}

	SourceLevel = InSourceLevel;
	LevelStreaming->SetShouldBeLoaded(true);
	LevelStreaming->SetShouldBeVisible(bInInitiallyVisible);
	return true;
}

void UAvaPlayableLevelStreaming::OnLevelStreamingStateChanged(UWorld* InWorld
	, const ULevelStreaming* InLevelStreaming
	, ULevel* InLevelIfLoaded
	, ELevelStreamingState InPreviousState
	, ELevelStreamingState InNewState)
{
	// Filter out levels we don't care about.
	if (InLevelStreaming != LevelStreaming)
	{
		return;
	}
	
	if (InNewState == ELevelStreamingState::FailedToLoad)
	{
		UE_LOG(LogAvaPlayable, Error, TEXT("Level \"%s\" failed to load."), *InLevelStreaming->PackageNameToLoad.ToString());		
	}
	else if (InNewState == ELevelStreamingState::LoadedNotVisible || InNewState == ELevelStreamingState::LoadedVisible)
	{
		const ULevel* const Level = LevelStreaming->GetLoadedLevel();
		if (!IsValid(Level))
		{
			return;
		}

		if (UWorld* OuterWorld = Level->GetTypedOuter<UWorld>())
		{
			// Workaround to avoid UEditorEngine::CheckForWorldGCLeaks killing the editor.
			// Change the sub-level's world to be a "persistent" world type.
			OuterWorld->WorldType = EWorldType::GamePreview;

			if (bLoadSubPlayables)
			{
				LoadSubPlayables(OuterWorld);
			}
		}

		// Workaround to destroy the Linker Load so that it does not keep the underlying File Opened
		if (Level->GetPackage())
		{
			FAvaPlaybackUtils::FlushPackageLoading(Level->GetPackage());
		}
		
		// Resolve the ava scene for the other operations.
		ResolveScene(Level);

		OnPlayableStatusChanged().Broadcast(this);

		for (const TObjectKey<UAvaPlayableLevelStreaming>& ParentPlayableKey : ParentPlayables)
		{
			if (UAvaPlayableLevelStreaming* ParentPlayable = ParentPlayableKey.ResolveObjectPtr())
			{
				ParentPlayable->OnLevelStreamingPlayableStatusChanged(this);
			}
		}

		OnLevelStreamingPlayableStatusChanged(this);
	}
}

void UAvaPlayableLevelStreaming::OnLevelStreamingPlayableStatusChanged(UAvaPlayableLevelStreaming* InPlayable)
{
	// OnPlay (camera setup, animations, etc) can only be done when the level is visible (components must be active).
	// With camera rig, we also need to make sure the rig level is loaded and visible.

	if (GetPlayableStatus() == EAvaPlayableStatus::Visible)
	{
		if (bOnPlayQueued)
		{
			bOnPlayQueued = false;
			OnPlay();
		}
	}
}

void UAvaPlayableLevelStreaming::BindDelegates()
{
	if (!FLevelStreamingDelegates::OnLevelStreamingStateChanged.IsBoundToObject(this))
	{
		FLevelStreamingDelegates::OnLevelStreamingStateChanged.AddUObject(this, &UAvaPlayableLevelStreaming::OnLevelStreamingStateChanged);
	}
}

void UAvaPlayableLevelStreaming::UnbindDelegates()
{
	FLevelStreamingDelegates::OnLevelStreamingStateChanged.RemoveAll(this);
}

ULevel* UAvaPlayableLevelStreaming::GetLoadedLevel() const
{
	if (!LevelStreaming)
	{
		return nullptr;	
	}
	
	ULevel* Level = LevelStreaming->GetLoadedLevel();
	return IsValid(Level) ? Level : nullptr;
}

void UAvaPlayableLevelStreaming::ResolveScene(const ULevel* InLevel)
{
	if (!IsValid(Scene))
	{
		Scene = UE::AvaMedia::LevelStreamingPlayable::Private::FindAvaScene((InLevel));
		if (Scene)
		{
			FAvaRemoteControlUtils::RegisterRemoteControlPreset(Scene->GetRemoteControlPreset(), /*bInEnsureUniqueId*/ true);
			FAvaRemoteControlRebind::RebindUnboundEntities(Scene->GetRemoteControlPreset(), InLevel);
		}
		else
		{
			UE_LOG(LogAvaPlayable, Error, TEXT("Loaded level \"%s\" is not an Motion Design level."), *LevelStreaming->PackageNameToLoad.ToString());
		}
	}
}

AActor* UAvaPlayableLevelStreaming::FindStartupCameraActor(FName InStartupCameraName, UAvaPlayable** OutParentPlayable)
{
	using namespace UE::AvaMedia::LevelStreamingPlayable;

	// TODO: Configurable priorities for camera selection.
	
	if (InStartupCameraName == NAME_None && Scene)
	{
		InStartupCameraName = Scene->GetStartupCameraName();
	}
	
	if (AActor* FoundCameraActor = Private::FindStartupCameraActor(GetLoadedLevel(), InStartupCameraName, Scene))
	{
		if (OutParentPlayable)
		{
			*OutParentPlayable = this;
		}
		return FoundCameraActor;
	}

	for (UAvaPlayableLevelStreaming* SubPlayable : SubPlayables)
	{
		if (AActor* FoundCameraActor = SubPlayable->FindStartupCameraActor(InStartupCameraName, OutParentPlayable))
		{
			return FoundCameraActor;
		}
	}
	
	return nullptr;
}

bool UAvaPlayableLevelStreaming::InitPlayable(const FPlayableCreationInfo& InPlayableInfo)
{
	// For now, we share all the levels in the same instance group. We may do sub-grouping later.
	PlayableGroup = InPlayableInfo.PlayableGroup ?
		InPlayableInfo.PlayableGroup : InPlayableInfo.PlayableGroupManager->GetOrCreateSharedLevelGroup(InPlayableInfo.ChannelName, false);

	const bool bInitSuccess = Super::InitPlayable(InPlayableInfo);
	
	if (bInitSuccess)
	{
		BindDelegates();
	}
	return bInitSuccess;
}

void UAvaPlayableLevelStreaming::OnPlay()
{
	if (!LevelStreaming || !GetPlayableGroup())
	{
		return;
	}
	
	if (!LevelStreaming->GetShouldBeVisibleFlag())
	{
		// Can't make visible immediately if part of a transition with other playables and the others are not ready.
		GetPlayableGroup()->RequestSetVisibility(this, true);
	}

	const ULevel* const Level = LevelStreaming->GetLoadedLevel();
	if (!IsValid(Level))
	{
		// Level is not yet loaded, queue the action when it gets loaded.
		// Remark: we could either do this when the event is received or poll on the next tick.
		const ELevelStreamingState LevelStreamingState = LevelStreaming->GetLevelStreamingState();
		ensure(LevelStreaming->ShouldBeLoaded());
		if (LevelStreamingState != ELevelStreamingState::FailedToLoad)
		{
			bOnPlayQueued = true;
		}
		else
		{
			UE_LOG(LogAvaPlayable, Error, TEXT("Level \"%s\" is not loading. Current Streaming State: \"%s\"."),
				*LevelStreaming->PackageNameToLoad.ToString(), EnumToString(LevelStreamingState));			
		}
		return;
	}

	// Check if the level is visible. We can't do the actual camera setup, or animation
	// if the level is not yet visible as the components are inactive.
	const ELevelStreamingState LevelStreamingState = LevelStreaming->GetLevelStreamingState();
	if (LevelStreamingState != ELevelStreamingState::LoadedVisible)
	{
		bOnPlayQueued = true;
		return;
	}

	// Ensure scene is resolved.
	ResolveScene(Level);

	if (!Scene)
	{
		return;
	}

	ApplyCamera();
}

void UAvaPlayableLevelStreaming::OnEndPlay()
{
	// Ensure the level is hidden and clear dirty flag because those are transient and shouldn't be saved.
	if (LevelStreaming)
	{
		const ULevel* const Level = LevelStreaming->GetLoadedLevel();
		if (IsValid(Level))
		{
			if (UPackage* LevelPackage = Level->GetPackage())
			{
				LevelPackage->ClearDirtyFlag();
			}
		}

		LevelStreaming->SetShouldBeVisible(false);
	}
}

void UAvaPlayableLevelStreaming::BeginDestroy()
{
	UnbindDelegates();
	Super::BeginDestroy();
}

void UAvaPlayableLevelStreaming::LoadSubPlayables(const UWorld* InLevelInstanceWorld)
{
	check(InLevelInstanceWorld);
	
	for (const ULevelStreaming* SubLevelStreaming : InLevelInstanceWorld->GetStreamingLevels())
	{
		if (SubLevelStreaming)
		{
			GetOrLoadSubPlayable(SubLevelStreaming);
		}
	}
}

void UAvaPlayableLevelStreaming::UnloadSubPlayables()
{
	for (UAvaPlayableLevelStreaming* SubPlayable : SubPlayables)
	{
		if (SubPlayable)
		{
			SubPlayable->ParentPlayables.Remove(this);

			// Shared Sub-Playables will be unloaded if they no longer have any parent
			// playables to keep them alive.
			if (!SubPlayable->HasParentPlayables())
			{
				SubPlayable->UnloadAsset();
				if (UAvaPlayableGroup* ParentPlayableGroup = SubPlayable->GetPlayableGroup())
				{
					ParentPlayableGroup->UnregisterPlayable(SubPlayable);
				}
			}
		}
	}
	
	SubPlayables.Reset();
}

void UAvaPlayableLevelStreaming::GetOrLoadSubPlayable(const ULevelStreaming* InLevelStreaming)
{
	if (!PlayableGroup)
	{
		return;
	}

	const FSoftObjectPath SourceAssetPath = InLevelStreaming->GetWorldAsset().ToSoftObjectPath();

	// Check already loaded sub-playables
	for (const TObjectPtr<UAvaPlayableLevelStreaming>& SubPlayable : SubPlayables)
	{
		if (SubPlayable->GetSourceAssetPath() == SourceAssetPath)
		{
			return; // Already loaded.
		}
	}
	
	// For now, sub playables are shared globally, i.e. unique instance per group.
	// Todo: We could support instancing scope (i.e. global vs local). Would require additional asset/scene info.
	TArray<UAvaPlayable*> FoundPlayables;
	PlayableGroup->FindPlayablesBySourceAssetPath(SourceAssetPath, FoundPlayables);
	for (UAvaPlayable* FoundPlayable : FoundPlayables)
	{
		if (UAvaPlayableLevelStreaming* ExistingPlayable = Cast<UAvaPlayableLevelStreaming>(FoundPlayable))
		{
			// Only use the existing playable if it is a sub-playable already.
			if (ExistingPlayable->HasParentPlayables())
			{
				AddSubPlayable(ExistingPlayable);
				return;
			}
		}
	}

	if (UAvaPlayableLevelStreaming* NewPlayable = CreateSubPlayable(PlayableGroup, SourceAssetPath))
	{
		// TODO: Propagate more stuff from InLevelStreaming. Needs to reach LoadLevel.
		const FAvaSoftAssetPtr AssetPtr = { UWorld::StaticClass(), TSoftObjectPtr(SourceAssetPath)};
		if (NewPlayable->LoadAsset(AssetPtr, GetShouldBeVisible()))
		{
			AddSubPlayable(NewPlayable);
		}
		else
		{
			PlayableGroup->UnregisterPlayable(NewPlayable);
		}
	}
}

UAvaPlayableLevelStreaming* UAvaPlayableLevelStreaming::CreateSubPlayable(UAvaPlayableGroup* InPlayableGroup, const FSoftObjectPath& InSourceAssetPath)
{
	if (!InPlayableGroup)
	{
		return nullptr;
	}

	UAvaPlayableLevelStreaming* NewPlayable = NewObject<UAvaPlayableLevelStreaming>(GEngine);
	
	const FPlayableCreationInfo PlayableCreationInfo =
		{
			InPlayableGroup->GetPlayableGroupManager(),
			{ UWorld::StaticClass(), TSoftObjectPtr(InSourceAssetPath) },
			FName(),
			InPlayableGroup
		};
	
	if (NewPlayable && !NewPlayable->InitPlayable(PlayableCreationInfo))
	{
		// final setup may fail, in this case the playable is discarded.
		return nullptr;
	}

	return NewPlayable;
}

void UAvaPlayableLevelStreaming::AddSubPlayable(UAvaPlayableLevelStreaming* InSubPlayable)
{
	if (InSubPlayable)
	{
		SubPlayables.AddUnique(InSubPlayable);
		InSubPlayable->ParentPlayables.Add(this);
	}
}

void UAvaPlayableLevelStreaming::RemoveSubPlayable(UAvaPlayableLevelStreaming* InSubPlayable)
{
	if (InSubPlayable)
	{
		SubPlayables.Remove(InSubPlayable);
		InSubPlayable->ParentPlayables.Remove(this);
	}
}

bool UAvaPlayableLevelStreaming::HasParentPlayables() const
{
	for (const TObjectKey<UAvaPlayableLevelStreaming>& ParentPlayableKey : ParentPlayables)
	{
		if (ParentPlayableKey.ResolveObjectPtr())
		{
			return true;
		}
	}
	return false;
}

void UAvaPlayableLevelStreaming::UpdateVisibilityFromParents()
{
	bool bShouldBeVisible = false;
	
	for (const TObjectKey<UAvaPlayableLevelStreaming>& ParentPlayableKey : ParentPlayables)
	{
		if (const UAvaPlayableLevelStreaming* ParentPlayable = ParentPlayableKey.ResolveObjectPtr())
		{
			if (ParentPlayable->GetShouldBeVisible())
			{
				bShouldBeVisible = true;
				break;
			}
		}
	}

	SetShouldBeVisible(bShouldBeVisible);
}


#undef LOCTEXT_NAMESPACE
