// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "TakePreset.generated.h"

class ULevelSequence;

/**
 * Take preset that is stored as an asset comprising a ULevelSequence, and a set of actor recording sources
 */
UCLASS(BlueprintType)
class TAKESCORE_API UTakePreset : public UObject
{
public:

	GENERATED_BODY()

	UTakePreset(const FObjectInitializer& ObjInit);

	/**
	 * Get this preset's level sequence that is used as a template for a new take recording
	 */
	ULevelSequence* GetLevelSequence() const
	{
		return LevelSequence;
	}

	/**
	 * Retrieve this preset's level sequence template, creating one if necessary
	 */
	ULevelSequence* GetOrCreateLevelSequence();

	/**
	 * Forcibly re-create this preset's level sequence template, even if one already exists
	 */
	void CreateLevelSequence();

	/**
	 * Copy the specified template preset into this instance. Copies the level sequence and all its recording meta-data.
	 */
	void CopyFrom(UTakePreset* TemplatePreset);

	/**
	 * Copy the specified level-sequence into this instance. Copies the level sequence and all its recording meta-data.
	 */
	void CopyFrom(ULevelSequence* TemplateLevelSequence);

	/**
	 * Bind onto an event that is triggered when this preset's level sequence has been changed
	 */
	FDelegateHandle AddOnLevelSequenceChanged(const FSimpleDelegate& InHandler);

	/**
	 * Remove a previously bound handler for the  event that is triggered when this preset's level sequence has been changed
	 */
	void RemoveOnLevelSequenceChanged(FDelegateHandle DelegateHandle);

	/**
	 * Allocate the transient preset used by the take recorder.
	 */
	static UTakePreset* AllocateTransientPreset(UTakePreset* TemplatePreset);
private:

	virtual void PostTransacted(const FTransactionObjectEvent& TransactionEvent) override;

	/** Instanced level sequence template that is used to define a starting point for a new take recording */
	UPROPERTY(Instanced)
	TObjectPtr<ULevelSequence> LevelSequence;

	FSimpleMulticastDelegate OnLevelSequenceChangedEvent;
};
