// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Set.h"
#include "Delegates/IDelegateInstance.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/WeakInterfacePtr.h"
#include "AvaSequenceSubsystem.generated.h"

class IAvaSequenceController;
class IAvaSequencePlaybackObject;
class IAvaSequenceProvider;
class UAvaSequence;
class ULevel;

UCLASS()
class AVALANCHESEQUENCE_API UAvaSequenceSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UAvaSequenceSubsystem() = default;

	static UAvaSequenceSubsystem* Get(UObject* InPlaybackContext);

	static TSharedRef<IAvaSequenceController> CreateSequenceController(UAvaSequence& InSequence, IAvaSequencePlaybackObject* InPlaybackObject);

	IAvaSequencePlaybackObject* FindOrCreatePlaybackObject(ULevel* InLevel, IAvaSequenceProvider& InSequenceProvider);

	IAvaSequencePlaybackObject* FindPlaybackObject(ULevel* InLevel) const;

	void AddPlaybackObject(IAvaSequencePlaybackObject* InPlaybackObject);

	void RemovePlaybackObject(IAvaSequencePlaybackObject* InPlaybackObject);

protected:
	//~ Begin UWorldSubsystem
	virtual bool DoesSupportWorldType(const EWorldType::Type InWorldType) const override;
	//~ End UWorldSubsystem

private:
	bool EnsureLevelIsAppropriate(ULevel*& InLevel) const;

	TArray<TWeakInterfacePtr<IAvaSequencePlaybackObject>> PlaybackObjects;
};
