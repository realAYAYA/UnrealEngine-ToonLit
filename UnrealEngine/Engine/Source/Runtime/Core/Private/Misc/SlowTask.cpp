// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/SlowTask.h"

#include "Containers/Array.h"
#include "CoreTypes.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/AssertionMacros.h"
#include "Misc/FeedbackContext.h"
#include "Misc/SlowTaskStack.h"

bool FSlowTask::ShouldCreateThrottledSlowTask()
{
	static double LastThrottledSlowTaskTime = 0;

	double CurrentTime = FPlatformTime::Seconds();
	if (CurrentTime - LastThrottledSlowTaskTime > 0.1)
	{
		LastThrottledSlowTaskTime = CurrentTime;
		return true;
	}

	return false;
}

FSlowTask::FSlowTask(float InAmountOfWork, const FText& InDefaultMessage, bool bInEnabled, FFeedbackContext& InContext)
	: DefaultMessage(InDefaultMessage)
	, FrameMessage()
	, TotalAmountOfWork(InAmountOfWork)
	, CompletedWork(0)
	, CurrentFrameScope(0)
	, Visibility(ESlowTaskVisibility::Default)
	, StartTime(FPlatformTime::Seconds())
	, bEnabled(bInEnabled && IsInGameThread())
	, bCreatedDialog(false)		// only set to true if we create a dialog
	, Context(InContext)
	, bSkipRecursiveDialogCreation(false)
{
	// If we have no work to do ourselves, create an arbitrary scope so that any actions performed underneath this still contribute to this one.
	if (TotalAmountOfWork == 0.f)
	{
		TotalAmountOfWork = CurrentFrameScope = 1.f;
	}
}

void FSlowTask::MakeRecursiveDialogIfNeeded()
{
	if (bEnabled)
	{
		if (bSkipRecursiveDialogCreation)
		{
			MakeDialogIfNeeded();
			return;
		}

		bSkipRecursiveDialogCreation = true;
		for (FSlowTask* Scope : Context.ScopeStack)
		{
			if (Scope->MakeDialogIfNeeded())
			{
				// Some dialog in the hierarchy wants to be called back
				bSkipRecursiveDialogCreation = false;
			}
		}
	}
}

bool FSlowTask::MakeDialogIfNeeded()
{
	if (bEnabled && !bCreatedDialog && OpenDialogThreshold.IsSet())
	{
		if (static_cast<float>(FPlatformTime::Seconds() - StartTime) < OpenDialogThreshold.GetValue())
		{
			// Let our caller know that we need to be called back
			return true;
		}

		MakeDialog(bDelayedDialogShowCancelButton, bDelayedDialogAllowInPIE);
	}

	return false;
}

void FSlowTask::Initialize()
{
	if (bEnabled)
	{
		Context.ScopeStack.Push(this);
	}
}

void FSlowTask::ForceRefresh(FFeedbackContext& Context)
{
	// We force refresh twice to account for r.oneframethreadlag in slate renderer to avoid
	// missing any visual cue when important transition occurs.
	Context.RequestUpdateUI(true);
	Context.RequestUpdateUI(true);
}

void FSlowTask::Destroy()
{
	if (bEnabled)
	{
		if (bCreatedDialog)
		{
			checkSlow(GIsSlowTask);

			// Make sure we see the progress fully updated just before destroying it
			ForceRefresh(Context);

			Context.FinalizeSlowTask();
		}

		FSlowTaskStack& Stack = Context.ScopeStack;
		checkSlow(Stack.Num() != 0 && Stack.Last() == this);

		FSlowTask* Task = Stack.Last();
		if (ensureMsgf(Task == this, TEXT("Out-of-order scoped task construction/destruction")))
		{
			Stack.Pop(false);
		}
		else
		{
			Stack.RemoveSingleSwap(this, false);
		}

		if (Stack.Num() != 0)
		{
			// Stop anything else contributing to the parent frame
			FSlowTask* Parent = Stack.Last();
			Parent->EnterProgressFrame(0, Parent->FrameMessage);

			Parent->Context.RequestUpdateUI();
		}
	}
}

void FSlowTask::MakeDialogDelayed(float Threshold, bool bShowCancelButton, bool bAllowInPIE)
{
	OpenDialogThreshold = Threshold;
	bDelayedDialogShowCancelButton = bShowCancelButton;
	bDelayedDialogAllowInPIE = bAllowInPIE;
}

void FSlowTask::EnterProgressFrame(float ExpectedWorkThisFrame, const FText& Text)
{
	if (bEnabled)
	{
		check(IsInGameThread());

		// Should actually be FrameMessage = Text; but this code is to investigate crashes in FSlowTask::GetCurrentMessage()
		if (!Text.IsEmpty())
		{
			FrameMessage = Text;
		}
		else
		{
			FrameMessage = FText::GetEmpty();
		}
		CompletedWork += CurrentFrameScope;

		const float WorkRemaining = TotalAmountOfWork - CompletedWork;
		// Add a small threshold here because when there are a lot of tasks, numerical imprecision can add up and trigger this.
		ensureMsgf(ExpectedWorkThisFrame <= 1.01f * TotalAmountOfWork - CompletedWork, TEXT("Work overflow in slow task. Please revise call-site to account for entire progress range."));
		CurrentFrameScope = FMath::Min(WorkRemaining, ExpectedWorkThisFrame);

		TickProgress();
	}
}

void FSlowTask::TickProgress()
{
	if (bEnabled)
	{
		check(IsInGameThread());

		// Make sure OS events are getting through while the task is being processed
		FPlatformMisc::PumpMessagesForSlowTask();

		MakeRecursiveDialogIfNeeded();
	
		Context.RequestUpdateUI();
	}
}

const FText& FSlowTask::GetCurrentMessage() const
{
	return FrameMessage.IsEmpty() ? DefaultMessage : FrameMessage;
}

void FSlowTask::MakeDialog(bool bShowCancelButton, bool bAllowInPIE)
{
	const bool bIsDisabledByPIE = Context.IsPlayingInEditor() && !bAllowInPIE;
	const bool bIsDialogAllowed = bEnabled && !GIsSilent && !bIsDisabledByPIE && !IsRunningCommandlet() && IsInGameThread() && Visibility != ESlowTaskVisibility::Invisible;
	if (!GIsSlowTask && bIsDialogAllowed)
	{
		Context.StartSlowTask(GetCurrentMessage(), bShowCancelButton);
		if (GIsSlowTask)
		{
			bCreatedDialog = true;

			// Refresh UI after dialog has been created
			ForceRefresh(Context);
		}
	}
}

bool FSlowTask::ShouldCancel() const
{
	if (bEnabled && GIsSlowTask)
	{
		check(IsInGameThread()); // FSlowTask is only meant to be used on the main thread currently

		// update the UI from time to time (throttling is done in RequestUpdateUI) so that the cancel button interaction can be detected: 
		Context.RequestUpdateUI();

		return Context.ReceivedUserCancel();
	}
	return false;
}
