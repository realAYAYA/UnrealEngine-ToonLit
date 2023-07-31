// Copyright Epic Games, Inc. All Rights Reserved.

#include "CompositeCameraShakePattern.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CompositeCameraShakePattern)

void UCompositeCameraShakePattern::GetShakePatternInfoImpl(FCameraShakeInfo& OutInfo) const
{
	// We will manage our duration ourselves.
	//
	// While we could return the maximum duration of all our children, the problem lies with
	// the Stop method, when bImmediately is false: it means we have to move our current time
	// to the beginning of the longest blend-out among our children. We could also set the
	// blend in/out times to the maximum blend time of all our children, but frankly we don't
	// need the make the base class to compute all kinds of stuff we don't need anyway. So
	// it's simpler to use a custom duration and get on with it.
	OutInfo.Duration = FCameraShakeDuration::Custom();
}

void UCompositeCameraShakePattern::StartShakePatternImpl(const FCameraShakeStartParams& Params)
{
	// Create states for all our children patterns.
	ChildStates.Reset();
	ChildStates.Reserve(ChildPatterns.Num());

	for (UCameraShakePattern* Pattern : ChildPatterns)
	{
		FCameraShakeState& ChildState = ChildStates.Emplace_GetRef();

		if (Pattern != nullptr)
		{
			// Initialize the new child state.
			FCameraShakeInfo ChildInfo;
			Pattern->GetShakePatternInfo(ChildInfo);
			ChildState.Initialize(ChildInfo);

			// Start the child pattern.
			Pattern->StartShakePattern(Params);
		}
	}
}

void UCompositeCameraShakePattern::UpdateShakePatternImpl(const FCameraShakeUpdateParams& Params, FCameraShakeUpdateResult& OutResult)
{
	UCameraShakeBase* ShakeInstance = GetShakeInstance();
	checkf(ShakeInstance, TEXT("Running a shake pattern without an outer shake instance"));

	// Update each of our children states.
	FCameraShakeUpdateParams ChildParams(Params);

	for (uint32 Index = 0, Num = ChildPatterns.Num(); Index < Num; ++Index)
	{
		FCameraShakeState& PatternState = ChildStates[Index];
		UCameraShakePattern* Pattern = ChildPatterns[Index];
		if (Pattern != nullptr)
		{
			// Update the child state.
			float ChildBlendingWeight = PatternState.Update(Params.DeltaTime);
			if (!PatternState.IsActive())
			{
				continue;
			}

			// Let the child pattern run on the current result, with its own blending weight.
			ChildParams.BlendingWeight = Params.BlendingWeight * ChildBlendingWeight;

			FCameraShakeUpdateResult ChildResult;

			Pattern->UpdateShakePattern(ChildParams, ChildResult);

			if (IsChildPatternFinished(PatternState, Pattern))
			{
				// This pattern just ended now. Reset its state and move on to the next.
				ChildStates[Index] = FCameraShakeState();
				continue;
			}
			
			// Apply this result and pass it on to the next child pattern.
			FCameraShakeApplyResultParams ApplyParams;
			ApplyParams.Scale = ChildParams.GetTotalScale();
			ApplyParams.PlaySpace = ShakeInstance->GetPlaySpace();
			ApplyParams.UserPlaySpaceMatrix = ShakeInstance->GetUserPlaySpaceMatrix();
			// This applies the current pattern's update to the parameters we'll pass
			// to the next one.
			UCameraShakeBase::ApplyResult(ApplyParams, ChildResult, ChildParams.POV);
		}
	}

	// All our children patterns have applied their logic to our temp view, so we can put this in
	// our own result, marking it as absolute.
	OutResult.Location = ChildParams.POV.Location;
	OutResult.Rotation = ChildParams.POV.Rotation;
	OutResult.FOV = ChildParams.POV.FOV;
	OutResult.Flags = ECameraShakeUpdateResultFlags::ApplyAsAbsolute;
}

void UCompositeCameraShakePattern::ScrubShakePatternImpl(const FCameraShakeScrubParams& Params, FCameraShakeUpdateResult& OutResult)
{
	// This method is similar to the UpdateShakePatternImpl method above, but calls the scrub methods
	// instead of the update methods.

	UCameraShakeBase* ShakeInstance = GetShakeInstance();
	checkf(ShakeInstance, TEXT("Running a shake pattern without an outer shake instance"));

	// Scrub each of our children states.
	FCameraShakeScrubParams ChildParams(Params);

	for (uint32 Index = 0, Num = ChildPatterns.Num(); Index < Num; ++Index)
	{
		FCameraShakeState& PatternState = ChildStates[Index];
		if (!PatternState.IsActive())
		{
			// This pattern was finished for some time already.
			continue;
		}

		UCameraShakePattern* Pattern = ChildPatterns[Index];
		if (Pattern != nullptr)
		{
			// Scrub the child state.
			float ChildBlendingWeight = PatternState.Scrub(Params.AbsoluteTime);

			// Let the child pattern run on the current result, with its own blending weight.
			ChildParams.BlendingWeight = Params.BlendingWeight * ChildBlendingWeight;

			FCameraShakeUpdateResult ChildResult;

			Pattern->ScrubShakePattern(ChildParams, ChildResult);

			if (IsChildPatternFinished(PatternState, Pattern))
			{
				// This pattern just ended now. Reset its state and move on to the next.
				ChildStates[Index] = FCameraShakeState();
				continue;
			}
			
			// Apply this result and pass it on to the next child pattern.
			FCameraShakeApplyResultParams ApplyParams;
			ApplyParams.Scale = ChildParams.GetTotalScale();
			ApplyParams.PlaySpace = ShakeInstance->GetPlaySpace();
			ApplyParams.UserPlaySpaceMatrix = ShakeInstance->GetUserPlaySpaceMatrix();
			// This applies the current pattern's scrubbing to the parameters we'll pass
			// to the next one.
			UCameraShakeBase::ApplyResult(ApplyParams, ChildResult, ChildParams.POV);
		}
	}

	// All our children patterns have applied their logic to our temp view, so we can put this in
	// our own result, marking it as absolute.
	OutResult.Location = ChildParams.POV.Location;
	OutResult.Rotation = ChildParams.POV.Rotation;
	OutResult.FOV = ChildParams.POV.FOV;
	OutResult.Flags = ECameraShakeUpdateResultFlags::ApplyAsAbsolute;
}

bool UCompositeCameraShakePattern::IsChildPatternFinished(const FCameraShakeState& ChildState, const UCameraShakePattern* ChildPattern) const
{
	if (ChildState.IsActive())
	{
		if (ChildState.HasDuration())
		{
			return ChildState.GetElapsedTime() >= ChildState.GetDuration();
		}
		else if (ChildPattern)
		{
			return ChildPattern->IsFinished();
		}
	}
	return true;
}

bool UCompositeCameraShakePattern::IsFinishedImpl() const
{
	// We're not finished if any of our child patterns is not finished.
	for (uint32 Index = 0, Num = ChildPatterns.Num(); Index < Num; ++Index)
	{
		const UCameraShakePattern* Pattern = ChildPatterns[Index];
		const FCameraShakeState& PatternState = ChildStates[Index];

		const bool bIsChildFinished = IsChildPatternFinished(PatternState, Pattern);
		if (!bIsChildFinished)
		{
			return false;
		}
	}
	return true;
}

void UCompositeCameraShakePattern::StopShakePatternImpl(const FCameraShakeStopParams& Params)
{
	// Stop all our children.
	for (uint32 Index = 0, Num = ChildPatterns.Num(); Index < Num; ++Index)
	{
		UCameraShakePattern* Pattern = ChildPatterns[Index];
		FCameraShakeState& PatternState = ChildStates[Index];

		if (PatternState.IsActive())
		{
			PatternState.Stop(Params.bImmediately);

			if (Pattern != nullptr)
			{
				Pattern->StopShakePattern(Params);
			}
		}
	}
}

void UCompositeCameraShakePattern::TeardownShakePatternImpl()
{
	// Teardown are all our children.
	for (UCameraShakePattern* Pattern : ChildPatterns)
	{
		if (Pattern != nullptr)
		{
			Pattern->TeardownShakePattern();
		}
	}

	ChildStates.Reset();
}


