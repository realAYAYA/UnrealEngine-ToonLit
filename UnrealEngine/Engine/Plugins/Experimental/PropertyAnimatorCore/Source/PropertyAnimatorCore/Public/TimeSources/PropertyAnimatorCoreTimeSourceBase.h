// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PropertyAnimatorCoreTimeSourceBase.generated.h"

class UPropertyAnimatorCoreBase;

/**
 * Abstract base class for time source used by property animators
 * Can be transient or saved to disk if contains user set data
 */
UCLASS(MinimalAPI, Abstract)
class UPropertyAnimatorCoreTimeSourceBase : public UObject
{
	GENERATED_BODY()

	friend class UPropertyAnimatorCoreSubsystem;

public:
	UPropertyAnimatorCoreTimeSourceBase()
		: UPropertyAnimatorCoreTimeSourceBase(NAME_None)
	{}

	UPropertyAnimatorCoreTimeSourceBase(const FName& InSourceName)
		: TimeSourceName(InSourceName)
	{}

	void ActivateTimeSource();
	void DeactivateTimeSource();

	bool IsTimeSourceActive() const
	{
		return bTimeSourceActive;
	}

	TOptional<double> GetConditionalTimeElapsed();

	/** Get the animator, this time source is on */
	UPropertyAnimatorCoreBase* GetAnimator() const;

	FName GetTimeSourceName() const
	{
		return TimeSourceName;
	}

protected:
	/** Returns the time elapsed for animators */
	virtual double GetTimeElapsed()
	{
		return 0;
	}

	/** Checks if this time source is ready to be used by the animator */
	virtual bool IsTimeSourceReady() const
	{
		return false;
	}

	/** Check if the time elapsed is valid based on the context */
	virtual bool IsValidTimeElapsed(double InTimeElapsed) const
	{
		return true;
	}

	/** Time source CDO is registered by subsystem */
	virtual void OnTimeSourceRegistered() {}

	/** Time source CDO is unregistered by subsystem */
	virtual void OnTimeSourceUnregistered() {}

	/** Time source is active on the animator */
	virtual void OnTimeSourceActive() {}

	/** Time source is inactive on the animator */
	virtual void OnTimeSourceInactive() {}

private:
	/** Only used to display time elapsed */
	UPROPERTY(Transient, VisibleInstanceOnly, Category="Animator", meta=(NoResetToDefault))
	double TimeElapsed = 0;

	/** Name used to display this time source to the user */
	UPROPERTY(Transient)
	FName TimeSourceName;

	bool bTimeSourceActive = false;
};