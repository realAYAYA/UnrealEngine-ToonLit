// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CADKernel/Core/Types.h"

namespace UE::CADKernel
{
class FProgress;
class FProgressBase;

class CADKERNEL_API FProgressManager
{
	friend class FProgress;

protected:

	FProgress* RootProgress;
	FProgress* CurrentProgress;

	FProgress* GetRoot() const
	{
		return RootProgress;
	}

	virtual void ActivateProgressBar(bool bActive)
	{}

	/**
	 * Update the progress bar
	 */
	virtual void Update()
	{}

	double GetProgression() const;

	/**
	 * @return a string describing the current step hierarchy
	 */
	virtual FString GetCurrentStep() const;

	virtual void SetCurrentProgress(FProgress* InProgress);

	FProgress* GetCurrentProgress() const
	{
		return CurrentProgress;
	}

public:
	FProgressManager()
		: RootProgress(nullptr)
		, CurrentProgress(nullptr)
	{}
};

class CADKERNEL_API FProgress
{
	friend class FProgressManager;
private:
	FString Name;

	FProgress* Parent = nullptr;
	FProgress* UnderlyingProgress = nullptr;

	int32 StepCount;
	int32 Progression = 0;

public:
	FProgress(int32 InStepCount, const FString& InStepName = FString());

	FProgress(const FString& StepName = FString())
		: FProgress(1, StepName)
	{
	}

	virtual ~FProgress();

	const FString& GetName()
	{
		return Name;
	}

	FProgress* GetParent() const
	{
		return Parent;
	}

	/**
	 * Increase the advancement of of one step
	 */
	void Increase(int32 StepSize = 1);

protected:

	bool IsRoot() const
	{
		return Parent == nullptr;
	}

	void AddUnderlying(FProgress* Progress)
	{
		UnderlyingProgress = Progress;
	}

	void UnderlyingFinished(FProgress* Progress)
	{
		Progression++;
		UnderlyingProgress = nullptr;
	}

	double GetProgression() const
	{
		double Ratio = ((double)Progression / (double)StepCount);

		if (UnderlyingProgress)
		{
			Ratio += UnderlyingProgress->GetProgression() / (double)StepCount;
		}
		return Ratio;
	}
};
}

