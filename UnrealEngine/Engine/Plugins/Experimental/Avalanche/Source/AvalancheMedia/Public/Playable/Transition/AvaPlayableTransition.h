// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaMediaDefines.h"
#include "Playable/AvaPlayableRemoteControlValues.h"
#include "UObject/Object.h"
#include "AvaPlayableTransition.generated.h"

class UAvaPlayable;
class UAvaPlayableGroup;

/**
 * @brief Defines the playable entry role in the transition.
 */
enum class EAvaPlayableTransitionEntryRole : uint8
{
	/** The playable corresponding to this entry is a entering the scene. */
	Enter,
	/** The playable corresponding to this entry is already in the scene and may react to the transition, but is otherwise neutral. */
	Playing,
	/** The playable corresponding to this entry is already in the scene but is commanded to exit. */
	Exit
};

UCLASS()
class AVALANCHEMEDIA_API UAvaPlayableTransition : public UObject
{
	GENERATED_BODY()
	
public:
	virtual bool Start();
	virtual void Stop();
	virtual bool IsRunning() const { return false; }
	virtual void Tick(double InDeltaSeconds) {}

	void SetTransitionFlags(EAvaPlayableTransitionFlags InFlags);
	void SetEnterPlayables(TArray<TWeakObjectPtr<UAvaPlayable>>&& InPlayablesWeak);
	void SetPlayingPlayables(TArray<TWeakObjectPtr<UAvaPlayable>>&& InPlayablesWeak);
	void SetExitPlayables(TArray<TWeakObjectPtr<UAvaPlayable>>&& InPlayablesWeak);
	
	bool IsEnterPlayable(UAvaPlayable* InPlayable) const;
	bool IsPlayingPlayable(UAvaPlayable* InPlayable) const;
	bool IsExitPlayable(UAvaPlayable* InPlayable) const;

	void SetEnterPlayableValues(TArray<TSharedPtr<FAvaPlayableRemoteControlValues>>&& InPlayableValues);
	
	/** This is called during the transition evaluation to indicate discarded playables. */
	void MarkPlayableAsDiscard(UAvaPlayable* InPlayable);

	/** Returns information on this transition suitable for logging. */
	virtual FString GetPrettyInfo() const;

	EAvaPlayableTransitionFlags GetTransitionFlags() const { return TransitionFlags; }
	
protected:
	UAvaPlayable* FindPlayable(const FGuid& InstanceId) const;
	
protected:
	EAvaPlayableTransitionFlags TransitionFlags = EAvaPlayableTransitionFlags::None;
	
	TArray<TSharedPtr<FAvaPlayableRemoteControlValues>> EnterPlayableValues;
	
	TArray<TWeakObjectPtr<UAvaPlayable>> EnterPlayablesWeak;
	TArray<TWeakObjectPtr<UAvaPlayable>> PlayingPlayablesWeak;
	TArray<TWeakObjectPtr<UAvaPlayable>> ExitPlayablesWeak;

	/** Keep track of the discarded playables so events can be sent when the transition ends. */
	TArray<TWeakObjectPtr<UAvaPlayable>> DiscardPlayablesWeak;

	TSet<TWeakObjectPtr<UAvaPlayableGroup>> PlayableGroupsWeak;
};

class AVALANCHEMEDIA_API FAvaPlayableTransitionBuilder
{
public:
	FAvaPlayableTransitionBuilder();

	void AddEnterPlayableValues(const TSharedPtr<FAvaPlayableRemoteControlValues>& InValues);
	
	bool AddEnterPlayable(UAvaPlayable* InPlayable);
	bool AddPlayingPlayable(UAvaPlayable* InPlayable);
	bool AddExitPlayable(UAvaPlayable* InPlayable);
	
	bool AddPlayable(UAvaPlayable* InPlayable, EAvaPlayableTransitionEntryRole InPlayableRole)
	{
		switch(InPlayableRole)
		{
			case EAvaPlayableTransitionEntryRole::Enter:
				return AddEnterPlayable(InPlayable);
			case EAvaPlayableTransitionEntryRole::Playing:
				return AddPlayingPlayable(InPlayable);
			case EAvaPlayableTransitionEntryRole::Exit:
				return AddExitPlayable(InPlayable);
		}
		return false;
	}

	UAvaPlayableTransition* MakeTransition(UObject* InOuter);

private:
	TArray<TSharedPtr<FAvaPlayableRemoteControlValues>> EnterPlayableValues;

	TArray<TWeakObjectPtr<UAvaPlayable>> EnterPlayablesWeak;
	TArray<TWeakObjectPtr<UAvaPlayable>> PlayingPlayablesWeak;
	TArray<TWeakObjectPtr<UAvaPlayable>> ExitPlayablesWeak;
};