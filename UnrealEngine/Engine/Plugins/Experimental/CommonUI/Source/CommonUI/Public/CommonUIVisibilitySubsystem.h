// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Subsystems/LocalPlayerSubsystem.h"
#include "GameplayTagContainer.h"
#include "CommonUIVisibilitySubsystem.generated.h"

class UWidget;
class ULocalPlayer;
class APlayerController;
struct FGameplayTagContainer;
class UCommonUIVisibilitySubsystem;
enum class ECommonInputType : uint8;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHardwareVisibilityTagsChangedDynamicEvent, UCommonUIVisibilitySubsystem*, TagSubsystem);

UCLASS(DisplayName = "UI Visibility Subsystem")
class COMMONUI_API UCommonUIVisibilitySubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	static UCommonUIVisibilitySubsystem* Get(const ULocalPlayer* LocalPlayer);
	static UCommonUIVisibilitySubsystem* GetChecked(const ULocalPlayer* LocalPlayer);

	UCommonUIVisibilitySubsystem();
	
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	DECLARE_EVENT_OneParam(UCommonUIVisibilitySubsystem, FHardwareVisibilityTagsChangedEvent, UCommonUIVisibilitySubsystem*);
	FHardwareVisibilityTagsChangedEvent OnVisibilityTagsChanged;

	/**
	 * Get the visibility tags currently in play (the combination of platform traits and current input tags).
	 * These can change over time, if input mode changes, or other groups are removed/added.
	 */
	const FGameplayTagContainer& GetVisibilityTags() const { return ComputedVisibilityTags; }
	
	/* Returns true if the player currently has the specified visibility tag
	 * (note: this value should not be cached without listening for OnVisibilityTagsChanged as it can change at runtime)
	 */
	bool HasVisibilityTag(const FGameplayTag VisibilityTag) const { return ComputedVisibilityTags.HasTag(VisibilityTag); }

	void AddUserVisibilityCondition(const FGameplayTag UserTag);
	void RemoveUserVisibilityCondition(const FGameplayTag UserTag);

#if WITH_EDITOR
	static void SetDebugVisibilityConditions(const FGameplayTagContainer& TagsToEnable, const FGameplayTagContainer& TagsToSuppress);
#endif

protected:
	void RefreshVisibilityTags();
	void OnInputMethodChanged(ECommonInputType CurrentInputType);
	FGameplayTagContainer ComputeVisibilityTags() const;

private:
	FGameplayTagContainer ComputedVisibilityTags;
	FGameplayTagContainer UserVisibilityTags;
#if WITH_EDITOR
	static FGameplayTagContainer DebugTagsToEnable;
	static FGameplayTagContainer DebugTagsToSuppress;
#endif
};