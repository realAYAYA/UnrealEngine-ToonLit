// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "HAL/Platform.h"
#include "Templates/SharedPointer.h"

class AActor;
class FString;
class SLevelViewport;
class UVCamOutputProviderBase;

enum class EVCamTargetViewportID : uint8;
struct FVCamViewportLocker;

namespace UE::VCamCore::LevelViewportUtils::Private
{
	/** Locks or unlocks all viewports which are used by OutputProviders depending on the corresponding FVCamViewportLockState::bLockViewportToCamera flag. All other viewports will be unlocked. */
	void UpdateViewportLocksFromOutputs(TArray<TObjectPtr<UVCamOutputProviderBase>> OutputProviders, FVCamViewportLocker& LockData, AActor& ActorToLockWith);
	/** Sets all viewports to be unlocked. */
	void UnlockAllViewports(FVCamViewportLocker& LockData, AActor& ActorToLockWith);

#if WITH_EDITOR
	/** Gets the level viewport identified by TargetViewport */
	TSharedPtr<SLevelViewport> GetLevelViewport(EVCamTargetViewportID TargetViewport);
	
	/** Gets the what SLevelViewport::GetConfigKey would return or None in the case of EVCamTargetViewportID::CurrentlySelected */
	FString GetConfigKeyFor(EVCamTargetViewportID TargetViewport);
#endif
};
