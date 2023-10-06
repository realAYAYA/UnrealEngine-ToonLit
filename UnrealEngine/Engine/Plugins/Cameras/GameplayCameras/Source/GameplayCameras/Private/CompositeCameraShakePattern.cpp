// Copyright Epic Games, Inc. All Rights Reserved.

#include "CompositeCameraShakePattern.h"
#include "Camera/PlayerCameraManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CompositeCameraShakePattern)

void UCompositeCameraShakePattern::GetShakePatternInfoImpl(FCameraShakeInfo& OutInfo) const
{
	// Set the duration to custom (since we want to handle timing ourselves),
	// but let's set a hint duration e.g. for when we are shown as a clip in
	// a sequence. The hint duration is the max of all our children's durations.
	OutInfo.Duration = FCameraShakeDuration::Custom();

	for (UCameraShakePattern* Pattern : ChildPatterns)
	{
		if (Pattern != nullptr)
		{
			FCameraShakeInfo ChildInfo;
			Pattern->GetShakePatternInfo(ChildInfo);

			if (ChildInfo.Duration.IsInfinite())
			{
				// If one of our children is infinite, we are infinite.
				OutInfo.Duration = ChildInfo.Duration;
				break;
			}
			else
			{
				// If a child has a fixed duration, or custom duration with hint,
				// let's include that in our own hint.
				OutInfo.Duration = FCameraShakeDuration::Custom(
					FMath::Max(ChildInfo.Duration.Get(), OutInfo.Duration.Get()));
			}
		}
	}
}

void UCompositeCameraShakePattern::StartShakePatternImpl(const FCameraShakeStartParams& Params)
{
	for (UCameraShakePattern* Pattern : ChildPatterns)
	{
		if (Pattern != nullptr)
		{
			// Start the child pattern.
			Pattern->StartShakePattern(Params);
		}
	}
}

void UCompositeCameraShakePattern::UpdateShakePatternImpl(const FCameraShakeUpdateParams& Params, FCameraShakeUpdateResult& OutResult)
{
	UCameraShakeBase* ShakeInstance = GetShakeInstance();
	checkf(ShakeInstance, TEXT("Running a shake pattern without an outer shake instance"));

	FCameraShakeUpdateParams ChildParams(Params);

	for (UCameraShakePattern* Pattern : ChildPatterns)
	{
		if (Pattern != nullptr && !Pattern->IsFinished())
		{
			// Let the child pattern run on the current result, with its own blending weight.
			FCameraShakeUpdateResult ChildResult;
			Pattern->UpdateShakePattern(ChildParams, ChildResult);

			if (!Pattern->IsFinished())
			{
				// Apply this result and pass it on to the next child pattern.
				FCameraShakeApplyResultParams ApplyParams;
				ApplyParams.Scale = Params.GetTotalScale();
				ApplyParams.PlaySpace = ShakeInstance->GetPlaySpace();
				ApplyParams.UserPlaySpaceMatrix = ShakeInstance->GetUserPlaySpaceMatrix();
				ApplyParams.CameraManager = ShakeInstance->GetCameraManager();
				// This applies the current pattern's update to the parameters we'll pass
				// to the next one.
				UCameraShakeBase::ApplyResult(ApplyParams, ChildResult, ChildParams.POV);
			}
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

	FCameraShakeScrubParams ChildParams(Params);

	for (UCameraShakePattern* Pattern : ChildPatterns)
	{
		if (Pattern != nullptr) // Don't check for IsFinished here, we might scrub anywhere.
		{
			// Let the child pattern run on the current result, with its own blending weight.
			FCameraShakeUpdateResult ChildResult;
			Pattern->ScrubShakePattern(Params, ChildResult);

			if (!Pattern->IsFinished())
			{
				// Apply this result and pass it on to the next child pattern.
				FCameraShakeApplyResultParams ApplyParams;
				ApplyParams.Scale = Params.GetTotalScale();
				ApplyParams.PlaySpace = ShakeInstance->GetPlaySpace();
				ApplyParams.UserPlaySpaceMatrix = ShakeInstance->GetUserPlaySpaceMatrix();
				// This applies the current pattern's scrubbing to the parameters we'll pass
				// to the next one.
				UCameraShakeBase::ApplyResult(ApplyParams, ChildResult, ChildParams.POV);
			}
		}
	}

	// All our children patterns have applied their logic to our temp view, so we can put this in
	// our own result, marking it as absolute.
	OutResult.Location = ChildParams.POV.Location;
	OutResult.Rotation = ChildParams.POV.Rotation;
	OutResult.FOV = ChildParams.POV.FOV;
	OutResult.Flags = ECameraShakeUpdateResultFlags::ApplyAsAbsolute;
}

bool UCompositeCameraShakePattern::IsFinishedImpl() const
{
	// We're not finished if any of our child patterns is not finished.
	for (UCameraShakePattern* Pattern : ChildPatterns)
	{
		if (Pattern != nullptr && !Pattern->IsFinished())
		{
			return false;
		}
	}
	return true;
}

void UCompositeCameraShakePattern::StopShakePatternImpl(const FCameraShakeStopParams& Params)
{
	// Stop all our children.
	for (UCameraShakePattern* Pattern : ChildPatterns)
	{
		if (Pattern != nullptr)
		{
			Pattern->StopShakePattern(Params);
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
}

