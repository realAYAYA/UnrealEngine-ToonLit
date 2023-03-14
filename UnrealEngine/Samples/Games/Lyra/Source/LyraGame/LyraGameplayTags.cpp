// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraGameplayTags.h"

#include "Containers/Array.h"
#include "Engine/EngineTypes.h"
#include "GameplayTagsManager.h"
#include "Logging/LogCategory.h"
#include "Logging/LogMacros.h"
#include "LyraLogChannels.h"
#include "Trace/Detail/Channel.h"
#include "UObject/NameTypes.h"

FLyraGameplayTags FLyraGameplayTags::GameplayTags;

void FLyraGameplayTags::InitializeNativeTags()
{
	UGameplayTagsManager& Manager = UGameplayTagsManager::Get();

	GameplayTags.AddAllTags(Manager);

	// Notify manager that we are done adding native tags.
	Manager.DoneAddingNativeTags();
}

void FLyraGameplayTags::AddAllTags(UGameplayTagsManager& Manager)
{
	AddTag(Ability_ActivateFail_IsDead, "Ability.ActivateFail.IsDead", "Ability failed to activate because its owner is dead.");
	AddTag(Ability_ActivateFail_Cooldown, "Ability.ActivateFail.Cooldown", "Ability failed to activate because it is on cool down.");
	AddTag(Ability_ActivateFail_Cost, "Ability.ActivateFail.Cost", "Ability failed to activate because it did not pass the cost checks.");
	AddTag(Ability_ActivateFail_TagsBlocked, "Ability.ActivateFail.TagsBlocked", "Ability failed to activate because tags are blocking it.");
	AddTag(Ability_ActivateFail_TagsMissing, "Ability.ActivateFail.TagsMissing", "Ability failed to activate because tags are missing.");
	AddTag(Ability_ActivateFail_Networking, "Ability.ActivateFail.Networking", "Ability failed to activate because it did not pass the network checks.");
	AddTag(Ability_ActivateFail_ActivationGroup, "Ability.ActivateFail.ActivationGroup", "Ability failed to activate because of its activation group.");

	AddTag(Ability_Behavior_SurvivesDeath, "Ability.Behavior.SurvivesDeath", "An ability with this type tag should not be canceled due to death.");

	AddTag(InputTag_Move, "InputTag.Move", "Move input.");
	AddTag(InputTag_Look_Mouse, "InputTag.Look.Mouse", "Look (mouse) input.");
	AddTag(InputTag_Look_Stick, "InputTag.Look.Stick", "Look (stick) input.");
	AddTag(InputTag_Crouch, "InputTag.Crouch", "Crouch input.");
	AddTag(InputTag_AutoRun, "InputTag.AutoRun", "Auto-run input.");

	AddTag(InitState_Spawned, "InitState.Spawned", "1: Actor/component has initially spawned and can be extended");
	AddTag(InitState_DataAvailable, "InitState.DataAvailable", "2: All required data has been loaded/replicated and is ready for initialization");
	AddTag(InitState_DataInitialized, "InitState.DataInitialized", "3: The available data has been initialized for this actor/component, but it is not ready for full gameplay");
	AddTag(InitState_GameplayReady, "InitState.GameplayReady", "4: The actor/component is fully ready for active gameplay");

	AddTag(GameplayEvent_Death, "GameplayEvent.Death", "Event that fires on death. This event only fires on the server.");
	AddTag(GameplayEvent_Reset, "GameplayEvent.Reset", "Event that fires once a player reset is executed.");
	AddTag(GameplayEvent_RequestReset, "GameplayEvent.RequestReset", "Event to request a player's pawn to be instantly replaced with a new one at a valid spawn location.");

	AddTag(SetByCaller_Damage, "SetByCaller.Damage", "SetByCaller tag used by damage gameplay effects.");
	AddTag(SetByCaller_Heal, "SetByCaller.Heal", "SetByCaller tag used by healing gameplay effects.");

	AddTag(Cheat_GodMode, "Cheat.GodMode", "GodMode cheat is active on the owner.");
	AddTag(Cheat_UnlimitedHealth, "Cheat.UnlimitedHealth", "UnlimitedHealth cheat is active on the owner.");

	AddTag(Status_Crouching, "Status.Crouching", "Target is crouching.");
	AddTag(Status_AutoRunning, "Status.AutoRunning", "Target is auto-running.");
	AddTag(Status_Death, "Status.Death", "Target has the death status.");
	AddTag(Status_Death_Dying, "Status.Death.Dying", "Target has begun the death process.");
	AddTag(Status_Death_Dead, "Status.Death.Dead", "Target has finished the death process.");

	AddMovementModeTag(Movement_Mode_Walking, "Movement.Mode.Walking", MOVE_Walking);
	AddMovementModeTag(Movement_Mode_NavWalking, "Movement.Mode.NavWalking", MOVE_NavWalking);
	AddMovementModeTag(Movement_Mode_Falling, "Movement.Mode.Falling", MOVE_Falling);
	AddMovementModeTag(Movement_Mode_Swimming, "Movement.Mode.Swimming", MOVE_Swimming);
	AddMovementModeTag(Movement_Mode_Flying, "Movement.Mode.Flying", MOVE_Flying);
	AddMovementModeTag(Movement_Mode_Custom, "Movement.Mode.Custom", MOVE_Custom);
}

void FLyraGameplayTags::AddTag(FGameplayTag& OutTag, const ANSICHAR* TagName, const ANSICHAR* TagComment)
{
	OutTag = UGameplayTagsManager::Get().AddNativeGameplayTag(FName(TagName), FString(TEXT("(Native) ")) + FString(TagComment));
}

void FLyraGameplayTags::AddMovementModeTag(FGameplayTag& OutTag, const ANSICHAR* TagName, uint8 MovementMode)
{
	AddTag(OutTag, TagName, "Character movement mode tag.");
	GameplayTags.MovementModeTagMap.Add(MovementMode, OutTag);
}

void FLyraGameplayTags::AddCustomMovementModeTag(FGameplayTag& OutTag, const ANSICHAR* TagName, uint8 CustomMovementMode)
{
	AddTag(OutTag, TagName, "Character custom movement mode tag.");
	GameplayTags.CustomMovementModeTagMap.Add(CustomMovementMode, OutTag);
}

FGameplayTag FLyraGameplayTags::FindTagByString(FString TagString, bool bMatchPartialString)
{
	const UGameplayTagsManager& Manager = UGameplayTagsManager::Get();
	FGameplayTag Tag = Manager.RequestGameplayTag(FName(*TagString), false);

	if (!Tag.IsValid() && bMatchPartialString)
	{
		FGameplayTagContainer AllTags;
		Manager.RequestAllGameplayTags(AllTags, true);

		for (const FGameplayTag TestTag : AllTags)
		{
			if (TestTag.ToString().Contains(TagString))
			{
				UE_LOG(LogLyra, Display, TEXT("Could not find exact match for tag [%s] but found partial match on tag [%s]."), *TagString, *TestTag.ToString());
				Tag = TestTag;
				break;
			}
		}
	}

	return Tag;
}
