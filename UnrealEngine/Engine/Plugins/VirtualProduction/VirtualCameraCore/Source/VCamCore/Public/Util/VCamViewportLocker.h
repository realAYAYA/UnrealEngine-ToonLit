// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EVCamTargetViewportID.h"
#include "VCamViewportLocker.generated.h"

USTRUCT()
struct FVCamViewportLockState
{
	GENERATED_BODY()
	
	/** Whether the user wants the viewport to be locked */
	UPROPERTY(EditAnywhere, Category = "Viewport", meta = (EditCondition = "!bIsForceLocked"))
	bool bLockViewportToCamera = false;
	
	/** Whether this viewport is currently locked */
	UPROPERTY(Transient)
	bool bWasLockedToViewport = false;

#if WITH_EDITORONLY_DATA
	// This property is editor-only because we use it for EditCondition only
	UPROPERTY(Transient)
	bool bIsForceLocked = false;

	/**
	 * Updated every time live link calls update (every tick).
	 * 
	 * Used for when the lock actor is switched by an external system.
	 * Once the lock actor becomes nullptr, we lock the viewport to our own virtual camera UNLESS
	 * this variable points to another virtual camera. In that case we lock to that camera.
	 * 
	 * Consider that the live link updates are not predictable.
	 */
	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> LastKnownEditorLockActor;
#endif
	
	/** Used for gameplay to restore to the previous view taget*/
	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> Backup_ViewTarget;
};

/**
 * Keeps track of which viewports are locked
 */
USTRUCT()
struct FVCamViewportLocker
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, EditFixedSize, Category = "Viewport")
	TMap<EVCamTargetViewportID, FVCamViewportLockState> Locks {
		{ EVCamTargetViewportID::Viewport1, {} },
		{ EVCamTargetViewportID::Viewport2, {} },
		{ EVCamTargetViewportID::Viewport3, {} },
		{ EVCamTargetViewportID::Viewport4, {} }
	};

	void Reset()
	{
		for (TPair<EVCamTargetViewportID, FVCamViewportLockState>& Pair : Locks)
		{
			Pair.Value.bWasLockedToViewport = false;
			Pair.Value.Backup_ViewTarget = nullptr;
		}
	}
};
