// Copyright Epic Games, Inc. All Rights Reserved.

#include "CADKernel/UI/Progress.h"
#include "CADKernel/UI/Message.h"
#include "CADKernel/Utils/Util.h"
#include "CADKernel/Core/System.h"

namespace UE::CADKernel
{

void FProgressManager::SetCurrentProgress(FProgress* InNewCurrent)
{
	if (!RootProgress)
	{
		RootProgress = InNewCurrent;
		CurrentProgress = RootProgress;
	}
	else if (InNewCurrent == nullptr)
	{
		RootProgress = nullptr;
		CurrentProgress = nullptr;
	}

	CurrentProgress = InNewCurrent;
	Update();
}

double FProgressManager::GetProgression() const
{
	if (!RootProgress) 
	{
		return 0.;
	}
	return RootProgress->GetProgression();
}

FString FProgressManager::GetCurrentStep() const
{
	FString CurrentStepStr;

	for (FProgress* Progress = GetCurrentProgress(); Progress; Progress = Progress->GetParent())
	{
		if (!Progress->GetName().IsEmpty())
		{
			CurrentStepStr = Progress->GetName() + TEXT(" -> ") + CurrentStepStr;
		}
	}
	CurrentStepStr.RemoveFromEnd(TEXT(" -> "));
	return CurrentStepStr;
}


FProgress::FProgress(int32 InStepCount, const FString& InStepName)
: Name(InStepName)
, StepCount(InStepCount)
{
	Parent = FSystem::Get().GetProgressManager().GetCurrentProgress();

	if (Parent)
	{
		Parent->AddUnderlying(this);
	}

	FSystem::Get().GetProgressManager().SetCurrentProgress(this);
}

FProgress::~FProgress()
{
	if (Parent)
	{
		Parent->UnderlyingFinished(this);
	}

	FSystem::Get().GetProgressManager().SetCurrentProgress(Parent);
	FSystem::Get().GetProgressManager().Update();
}

void FProgress::Increase(int32 StepSize)
{
	Progression += StepSize;
	FSystem::Get().GetProgressManager().Update();
}

}