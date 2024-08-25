// Copyright Epic Games, Inc. All Rights Reserved.

#include "LevelViewportUtils.h"

#include "EVCamTargetViewportID.h"
#include "VCamComponent.h"
#include "Misc/ScopeExit.h"
#include "Output/VCamOutputProviderBase.h"
#include "Util/VCamViewportLocker.h"

#include "Containers/UnrealString.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#if WITH_EDITOR
#include "LevelEditorViewport.h"
#include "SLevelViewport.h"
#endif

namespace UE::VCamCore::LevelViewportUtils::Private
{
	namespace Locking
	{
		enum class EViewportFlags
		{
			None,
			IsNotUsed   = 1 << 0,
			ForceUse	= 1 << 1,
		};
		ENUM_CLASS_FLAGS(EViewportFlags)
		
#if WITH_EDITOR
		static void UpdateLockStateForEditor(FVCamViewportLockState& ViewportLockState, EVCamTargetViewportID ViewportID, bool bNewLockState, AActor& ActorToLockWith)
		{
			TSharedPtr<SLevelViewport> Viewport = GetLevelViewport(ViewportID);
			FLevelEditorViewportClient* LevelViewportClient = Viewport.IsValid()
				? &Viewport->GetLevelViewportClient()
				: nullptr;
			if (LevelViewportClient)
			{
				AActor* const CurrentLockActor = LevelViewportClient->GetActorLock().LockedActor.Get();
				AActor* const CurrentCinematicLockActor = LevelViewportClient->GetCinematicActorLock().LockedActor.Get();;
				ON_SCOPE_EXIT
				{
					// Get it again because the GetActorLock value may have changed
					ViewportLockState.LastKnownEditorLockActor = LevelViewportClient->GetActorLock().LockedActor.Get();
				};

				const bool bIsLockedByAnotherSystem = CurrentLockActor || CurrentCinematicLockActor;
				// Do not override the lock if another system has a lock
				const bool bCanLock = !bIsLockedByAnotherSystem || !LevelViewportClient->bLockedCameraView;
				const bool bCanUnlock = CurrentLockActor == &ActorToLockWith;
				
				if (bNewLockState && bCanLock)
				{
					ViewportLockState.bWasLockedToViewport = true;
					
					// Scenario:
					// 1. Two VCams in the world
					// 2. 1st locks to viewport 1, then 2nd locks viewport 1
					// 3. External system, like Level Sequencer, takes lock (e.g. record and then review a take)
					// 4. External system clears its own lock (e.g. finish reviewing take)
					// 5. The 2nd Vcam should now lock viewport 1 - not the 1st VCam
					UVCamComponent* Component = ViewportLockState.LastKnownEditorLockActor.IsValid()
						? ViewportLockState.LastKnownEditorLockActor->FindComponentByClass<UVCamComponent>()
						: nullptr;
					if (Component
						// Avoid pointless recursion
						&& Component->GetOwner() != &ActorToLockWith)
					{
						// It could be that the other component's update was skipped and it points back to ActorToLockWith
						// That would cause infinite recursion.
						// This case is unlikely and it has never happened to me but I'm adding this logic to be 100% safe.
						static TArray<UVCamComponent*, TInlineAllocator<4>> InfiniteRecursionProtection;
						if (LIKELY(!InfiniteRecursionProtection.Contains(Component)))
						{
							InfiniteRecursionProtection.Push(Component);
							Component->UpdateActorViewportLocks();
							InfiniteRecursionProtection.Pop();
							return;
						}
						// In unlikely case of infinite recursion, the tail will just set the actor lock now
					}
					
					LevelViewportClient->SetActorLock(&ActorToLockWith);
					LevelViewportClient->bLockedCameraView = true;
				}
				else if (!bNewLockState && ViewportLockState.bWasLockedToViewport && bCanUnlock)
				{
					ViewportLockState.bWasLockedToViewport = false;
					
					LevelViewportClient->SetActorLock(nullptr);
					LevelViewportClient->bLockedCameraView = false;
				}
			}
		}
#endif

		static void UpdateLockStateForGame(FVCamViewportLockState& ViewportLockState, const FWorldContext& Context, bool bNewLockState, AActor& ActorToLockWith)
		{
			UWorld* ActorWorld = Context.World();
			if (ActorWorld && ActorWorld->GetGameInstance())
			{
				APlayerController* PlayerController = ActorWorld->GetGameInstance()->GetFirstLocalPlayerController(ActorWorld);
				if (PlayerController)
				{
					if (bNewLockState && !ViewportLockState.bWasLockedToViewport)
					{
						ViewportLockState.Backup_ViewTarget = PlayerController->GetViewTarget();
						PlayerController->SetViewTarget(&ActorToLockWith);
						ViewportLockState.bWasLockedToViewport = true;
					}
					else if (!bNewLockState && ViewportLockState.bWasLockedToViewport)
					{
						PlayerController->SetViewTarget(ViewportLockState.Backup_ViewTarget.Get());
						ViewportLockState.Backup_ViewTarget = nullptr;
						ViewportLockState.bWasLockedToViewport = false;
					}
				}
			}
		}
		
		static void UpdateLockState(FVCamViewportLockState& ViewportLockState, EVCamTargetViewportID ViewportID, EViewportFlags ViewportFlags, AActor& ActorToLockWith)
		{
			const bool bForceUse = (ViewportFlags & EViewportFlags::ForceUse) != EViewportFlags::None;
			const bool bIsUsed = (ViewportFlags & EViewportFlags::IsNotUsed) == EViewportFlags::None;
			const bool bNewLockState = bForceUse || (ViewportLockState.bLockViewportToCamera && bIsUsed);

#if WITH_EDITOR
			ViewportLockState.bIsForceLocked = bForceUse;
#endif
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
#if WITH_EDITOR
				if (Context.WorldType == EWorldType::Editor)
				{
					UpdateLockStateForEditor(ViewportLockState, ViewportID, bNewLockState, ActorToLockWith);
				}
				else
#endif
				{
					UpdateLockStateForGame(ViewportLockState, Context, bNewLockState, ActorToLockWith);
				}
			}
		}
	}

	void UpdateViewportLocksFromOutputs(TArray<TObjectPtr<UVCamOutputProviderBase>> OutputProviders, FVCamViewportLocker& LockData, AActor& ActorToLockWith)
	{
		TSet<EVCamTargetViewportID> Viewports;
		TSet<EVCamTargetViewportID> ForcefullyLockedViewports;
		
		Algo::TransformIf(OutputProviders, Viewports,
			[](TObjectPtr<UVCamOutputProviderBase> Output){ return Output && Output->IsActive(); },
			[](TObjectPtr<UVCamOutputProviderBase> Output) { return Output->GetTargetViewport(); }
			);
		Algo::TransformIf(OutputProviders, ForcefullyLockedViewports,
			[](TObjectPtr<UVCamOutputProviderBase> Output){ return Output && Output->IsActive() && Output->NeedsForceLockToViewport(); },
			[](TObjectPtr<UVCamOutputProviderBase> Output) { return Output->GetTargetViewport(); }
			);
		
		check(LockData.Locks.Num() == 4);
		for (TPair<EVCamTargetViewportID, FVCamViewportLockState>& ViewportData : LockData.Locks)
		{
			const bool bIsForceLocked = ForcefullyLockedViewports.Contains(ViewportData.Key);
			const bool bIsNotUsed = !Viewports.Contains(ViewportData.Key);
			
			Locking::EViewportFlags Flags = Locking::EViewportFlags::None;
			Flags |= bIsForceLocked ? Locking::EViewportFlags::ForceUse : Locking::EViewportFlags::None;
			Flags |= bIsNotUsed ? Locking::EViewportFlags::IsNotUsed : Locking::EViewportFlags::None;
			
			Locking::UpdateLockState(ViewportData.Value, ViewportData.Key, Flags, ActorToLockWith);
		}
	}

	void UnlockAllViewports(FVCamViewportLocker& LockData, AActor& ActorToLockWith)
	{
		for (TPair<EVCamTargetViewportID, FVCamViewportLockState>& ViewportData : LockData.Locks)
		{
			Locking::UpdateLockState(ViewportData.Value, ViewportData.Key, Locking::EViewportFlags::IsNotUsed, ActorToLockWith);
		}
	}

#if WITH_EDITOR
	TSharedPtr<SLevelViewport> GetLevelViewport(EVCamTargetViewportID TargetViewport)
	{
		TSharedPtr<SLevelViewport> OutLevelViewport = nullptr;

		if (!GEditor)
		{
			return nullptr;
		}
	
		for (FLevelEditorViewportClient* Client : GEditor->GetLevelViewportClients())
		{
			// We only care about the fully rendered 3D viewport...seems like there should be a better way to check for this
			if (Client->IsOrtho())
			{
				continue;
			}

			TSharedPtr<SLevelViewport> LevelViewport = StaticCastSharedPtr<SLevelViewport>(Client->GetEditorViewportWidget());
			if (!LevelViewport.IsValid())
			{
				continue;
			}
		
			const FString WantedViewportString = GetConfigKeyFor(TargetViewport);
			const FString ViewportConfigKey = LevelViewport->GetConfigKey().ToString();
			if (ViewportConfigKey.Contains(*WantedViewportString, ESearchCase::CaseSensitive, ESearchDir::FromStart))
			{
				return LevelViewport;
			}
		}

		return OutLevelViewport;
	}
	
	FString GetConfigKeyFor(EVCamTargetViewportID TargetViewport)
	{
		/*
		 * TL;DR:
		 * - "Viewport %d" selects he viewport from Window > Viewport x
		 * - ".Viewport1" SEEMS to be the viewport that is rendered always 
		 *
		 * The GEditor->GetLevelViewportClients() up above usually returns viewports with the following keys:
		 * - FourPanes2x2.Viewport 1.Viewport1
		 * - FourPanes2x2.Viewport 2.Viewport1
		 * - FourPanes2x2.Viewport 3.Viewport1
		 * - FourPanes2x2.Viewport 4.Viewport1
		 *
		 * More viewports may be returned. Notable example is when Camera Cuts are enabled (i.e. ISequencer::SetPerspectiveViewportCameraCutEnabled(true))
		 * In that case, it may look like this:
		 * - FourPanes2x2.Viewport 1.Viewport0
		 * - FourPanes2x2.Viewport 1.Viewport1
		 * - FourPanes2x2.Viewport 1.Viewport2
		 * - FourPanes2x2.Viewport 1.Viewport3
		 * - FourPanes2x2.Viewport 2.Viewport0
		 * - [...]
		 * - FourPanes2x2.Viewport 2.Viewport3
		 * - [...]
		 * - FourPanes2x2.Viewport 4.Viewport3
		 *
		 * It seems like the viewport that is rendered however is always the one that ends in Viewport1.
		 */
		return FString::Printf(TEXT("Viewport %d.Viewport1"), static_cast<int32>(TargetViewport) + 1);
	}
#endif
}