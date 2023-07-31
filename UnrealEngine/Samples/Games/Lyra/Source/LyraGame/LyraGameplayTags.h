// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "GameplayTagContainer.h"
#include "HAL/Platform.h"

class UGameplayTagsManager;

/**
 * FLyraGameplayTags
 *
 *	Singleton containing native gameplay tags.
 */
struct FLyraGameplayTags
{
public:

	static const FLyraGameplayTags& Get() { return GameplayTags; }

	static void InitializeNativeTags();

	static FGameplayTag FindTagByString(FString TagString, bool bMatchPartialString = false);

public:

	FGameplayTag Ability_ActivateFail_IsDead;
	FGameplayTag Ability_ActivateFail_Cooldown;
	FGameplayTag Ability_ActivateFail_Cost;
	FGameplayTag Ability_ActivateFail_TagsBlocked;
	FGameplayTag Ability_ActivateFail_TagsMissing;
	FGameplayTag Ability_ActivateFail_Networking;
	FGameplayTag Ability_ActivateFail_ActivationGroup;

	FGameplayTag Ability_Behavior_SurvivesDeath;

	FGameplayTag InputTag_Move;
	FGameplayTag InputTag_Look_Mouse;
	FGameplayTag InputTag_Look_Stick;
	FGameplayTag InputTag_Crouch;
	FGameplayTag InputTag_AutoRun;

	// Initialization states for the GameFrameworkComponentManager, these are registered in order by LyraGameInstance and some actors will skip right to GameplayReady

	/** Actor/component has initially spawned and can be extended */
	FGameplayTag InitState_Spawned;

	/** All required data has been loaded/replicated and is ready for initialization */
	FGameplayTag InitState_DataAvailable;

	/** The available data has been initialized for this actor/component, but it is not ready for full gameplay */
	FGameplayTag InitState_DataInitialized;

	/** The actor/component is fully ready for active gameplay */
	FGameplayTag InitState_GameplayReady;

	FGameplayTag GameplayEvent_Death;
	FGameplayTag GameplayEvent_Reset;
	FGameplayTag GameplayEvent_RequestReset;

	FGameplayTag SetByCaller_Damage;
	FGameplayTag SetByCaller_Heal;

	FGameplayTag Cheat_GodMode;
	FGameplayTag Cheat_UnlimitedHealth;

	FGameplayTag Status_Crouching;
	FGameplayTag Status_AutoRunning;
	FGameplayTag Status_Death;
	FGameplayTag Status_Death_Dying;
	FGameplayTag Status_Death_Dead;

	FGameplayTag Movement_Mode_Walking;
	FGameplayTag Movement_Mode_NavWalking;
	FGameplayTag Movement_Mode_Falling;
	FGameplayTag Movement_Mode_Swimming;
	FGameplayTag Movement_Mode_Flying;
	FGameplayTag Movement_Mode_Custom;

	TMap<uint8, FGameplayTag> MovementModeTagMap;
	TMap<uint8, FGameplayTag> CustomMovementModeTagMap;

protected:

	void AddAllTags(UGameplayTagsManager& Manager);
	void AddTag(FGameplayTag& OutTag, const ANSICHAR* TagName, const ANSICHAR* TagComment);
	void AddMovementModeTag(FGameplayTag& OutTag, const ANSICHAR* TagName, uint8 MovementMode);
	void AddCustomMovementModeTag(FGameplayTag& OutTag, const ANSICHAR* TagName, uint8 CustomMovementMode);

private:

	static FLyraGameplayTags GameplayTags;
};
