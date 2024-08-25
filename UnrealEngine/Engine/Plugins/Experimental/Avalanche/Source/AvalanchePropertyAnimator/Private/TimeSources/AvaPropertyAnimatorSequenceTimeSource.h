// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TimeSources/PropertyAnimatorCoreRangeTimeSource.h"
#include "AvaPropertyAnimatorSequenceTimeSource.generated.h"

class ISequencer;
class UAvaSequence;
class UAvaSequencePlayer;
class UMovieSceneSequence;

/**
 * Time source that follows specific sequence time
 */
UCLASS()
class UAvaPropertyAnimatorSequenceTimeSource : public UPropertyAnimatorCoreRangeTimeSource
{
	GENERATED_BODY()

public:
	UAvaPropertyAnimatorSequenceTimeSource()
		: UPropertyAnimatorCoreRangeTimeSource(TEXT("Sequence"))
	{}

	//~ Begin UObject
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent) override;
#endif // WITH_EDITOR
	//~ End UObject

	void SetSequenceName(const FString& InSequenceName);
	FString GetSequenceName() const
	{
		return SequenceName;
	}

	//~ Begin UPropertyAnimatorCoreTimeSourceBase
	virtual double GetTimeElapsed() override;
	virtual bool IsTimeSourceReady() const override;
	//~ End UPropertyAnimatorCoreTimeSourceBase

protected:
	//~ Begin UPropertyAnimatorCoreTimeSourceBase
	virtual void OnTimeSourceRegistered() override;
	virtual void OnTimeSourceUnregistered() override;
	virtual void OnTimeSourceActive() override;
	virtual void OnTimeSourceInactive() override;
	//~ End UPropertyAnimatorCoreTimeSourceBase

	UFUNCTION()
	TArray<FString> GetSequenceNames() const;

	void OnSequenceChanged();

	void OnSequenceStarted(UAvaSequencePlayer* InPlayer, UAvaSequence* InSequence);
	void OnSequenceFinished(UAvaSequencePlayer* InPlayer, UAvaSequence* InSequence);

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetSequenceName", Getter="GetSequenceName", Category="Animator", meta=(GetOptions="GetSequenceNames"))
	FString SequenceName;

	UPROPERTY()
	TWeakObjectPtr<UMovieSceneSequence> SequenceWeak;

	UPROPERTY(Transient)
	TWeakObjectPtr<UAvaSequencePlayer> SequencePlayerWeak;

private:
#if WITH_EDITOR
	void OnSequencerCreated(TSharedRef<ISequencer> InSequencer);

	TSharedPtr<ISequencer> GetSequencer() const;

	// Last sequencer registered
	TWeakPtr<ISequencer> SequencerWeak;

	// Handle only for CDO object
	FDelegateHandle OnSequencerCreatedHandle;
#endif // WITH_EDITOR
};