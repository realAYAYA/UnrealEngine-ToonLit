// Copyright Epic Games, Inc. All Rights Reserved.

#include "Camera/CameraShakeBase.h"
#include "Camera/CameraAnimationHelper.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Engine.h"
#include "IXRTrackingSystem.h"
#include "Misc/EnumClassFlags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CameraShakeBase)

DECLARE_CYCLE_STAT(TEXT("CameraShakeStartShake"), STAT_StartShake, STATGROUP_Game);
DECLARE_CYCLE_STAT(TEXT("CameraShakeUpdateShake"), STAT_UpdateShake, STATGROUP_Game);

UCameraShakeBase::UCameraShakeBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bSingleInstance(false)
	, ShakeScale(1.f)
	, PlaySpace(ECameraShakePlaySpace::CameraLocal)
{
	RootShakePattern = static_cast<UCameraShakePattern*>(ObjectInitializer.CreateDefaultSubobject(
			this, 
			TEXT("RootShakePattern"), 
			UCameraShakePattern::StaticClass(),
			nullptr,	// No class to create by default
			false,		// Not required
			false		// Not transient
			));
}

FCameraShakeDuration UCameraShakeBase::GetCameraShakeDuration() const
{
	FCameraShakeInfo TempInfo;
	GetShakeInfo(TempInfo);
	return TempInfo.Duration;
}

void FCameraShakeState::Initialize(const FCameraShakeInfo& InShakeInfo)
{
	ShakeInfo = InShakeInfo;

	// Cache a few things about the shake.
	bHasBlendIn = ShakeInfo.BlendIn > 0.f;
	bHasBlendOut = ShakeInfo.BlendOut > 0.f;

	// Initialize our running state.
	const bool bIsRestarting = bIsActive;
	if (!bIsRestarting)
	{
		// Set the active state.
		ElapsedTime = 0.f;
		bIsActive = true;
	}
	else
	{
		// Single instance shake is being restarted... let's see if we need to
		// reverse a blend out into a blend in.
		if (ShakeInfo.Duration.IsFixed() && bHasBlendIn && bHasBlendOut)
		{
			const float BlendOutStartTime = ShakeInfo.Duration.Get() - ShakeInfo.BlendOut;
			if (ElapsedTime > BlendOutStartTime)
			{
				// We had started blending out... let's start at an equivalent weight into the blend in.
				const float BlendOutCurrentTime = ElapsedTime - BlendOutStartTime;
				ElapsedTime = ShakeInfo.BlendIn * (1.f - BlendOutCurrentTime / ShakeInfo.BlendOut);
				// Because this means we are shortening the shake (by the amount that we start into the
				// blend in, instead of starting from zero), we need to lengthen the shake to make it
				// last the same duration as it's supposed to.
				ShakeInfo.Duration = FCameraShakeDuration(ShakeInfo.Duration.Get() + ElapsedTime);
			}
			else
			{
				// We had not started blending out, so we were at 100%. Let's go back to the beginning
				// but skip the blend in time.
				ElapsedTime = 0.f;
				bHasBlendIn = false;
				ShakeInfo.BlendIn = 0.f;
			}
		}
		else
		{
			// We either don't have blending, or our shake pattern is doing custom stuff.
			ElapsedTime = 0.f;
		}
	}
}

float FCameraShakeState::Update(float DeltaTime)
{
	// If we have a fixed duration for our shake, we can do all the time-keeping stuff ourselves.
	// This includes figuring out if the shake is finished, and what kind of blend in/out weight
	// we should apply.
	float BlendingWeight = 1.f;
	if (HasDuration())
	{
		// Advance progress into the shake.
		const float ShakeDuration = ShakeInfo.Duration.Get();
		ElapsedTime = ElapsedTime + DeltaTime;
		if (ElapsedTime < 0.f || ElapsedTime >= ShakeDuration)
		{
			// The shake has ended, or hasn't started yet.
			bIsActive = false;
			return 0.f;
		}

		// Blending in?
		if (bHasBlendIn && ElapsedTime < ShakeInfo.BlendIn)
		{
			BlendingWeight *= (ElapsedTime / ShakeInfo.BlendIn);
		}

		// Blending out?
		const float DurationRemaining = (ShakeDuration - ElapsedTime);
		if (bHasBlendOut && DurationRemaining < ShakeInfo.BlendOut)
		{
			BlendingWeight *= (DurationRemaining / ShakeInfo.BlendOut);
		}
	}
	return BlendingWeight;
}

float FCameraShakeState::Scrub(float AbsoluteTime)
{
	float BlendingWeight = 1.f;
	if (HasDuration())
	{
		// Reset the state to active, at the beginning, and update from there.
		bIsActive = true;
		ElapsedTime = 0.f;
		return Update(AbsoluteTime);
	}
	return BlendingWeight;
}

bool FCameraShakeState::Stop(bool bImmediately)
{
	if (HasDuration())
	{
		// If we have duration information, we can set our time-keeping accordingly to stop the shake.
		// For stopping immediately, we just go to the end right away.
		// For stopping with a "graceful" blend-out:
		// - If we are already blending out, let's keep doing that and not change anything.
		// - If we are not, move to the start of the blend out.
		const float ShakeDuration = ShakeInfo.Duration.Get();
		if (bImmediately || !bHasBlendOut)
		{
			ElapsedTime = ShakeDuration;
		}
		else
		{
			const float BlendOutStartTime = ShakeDuration - ShakeInfo.BlendOut;
			if (ElapsedTime < BlendOutStartTime)
			{
				ElapsedTime = BlendOutStartTime;
			}
		}
		return true;
	}
	return false;
}

FCameraShakeUpdateParams FCameraShakeScrubParams::ToUpdateParams() const
{
	FCameraShakeUpdateParams UpdateParams;
	UpdateParams.DeltaTime = AbsoluteTime;
	UpdateParams.ShakeScale = ShakeScale;
	UpdateParams.DynamicScale = DynamicScale;
	UpdateParams.BlendingWeight = BlendingWeight;
	UpdateParams.POV = POV;
	return UpdateParams;
}

void UCameraShakeBase::GetCameraShakeBlendTimes(float& OutBlendIn, float& OutBlendOut) const
{
	FCameraShakeInfo TempInfo;
	GetShakeInfo(TempInfo);
	OutBlendIn = TempInfo.BlendIn;
	OutBlendOut = TempInfo.BlendOut;
}

void UCameraShakeBase::SetRootShakePattern(UCameraShakePattern* InPattern)
{
	if (ensureMsgf(!State.IsActive(), TEXT("Can't change the root shake pattern while the shake is running!")))
	{
		RootShakePattern = InPattern;
	}
}

void UCameraShakeBase::GetShakeInfo(FCameraShakeInfo& OutInfo) const
{
	if (RootShakePattern)
	{
		RootShakePattern->GetShakePatternInfo(OutInfo);
	}
}

void UCameraShakeBase::StartShake(APlayerCameraManager* Camera, float Scale, ECameraShakePlaySpace InPlaySpace, FRotator UserPlaySpaceRot)
{
	SCOPE_CYCLE_COUNTER(STAT_StartShake);

	// Check that we were correctly stopped before we are asked to play again.
	// Note that single-instance shakes can be restarted while they're running.
	checkf(!State.IsActive() || bSingleInstance, TEXT("Starting to play a shake that was already playing."));

	// Remember the various settings for this run.
	// Note that the camera manager can be null, for example in unit tests.
	CameraManager = Camera;
	ShakeScale = Scale;
	PlaySpace = InPlaySpace;
	UserPlaySpaceMatrix = (InPlaySpace == ECameraShakePlaySpace::UserDefined) ? 
		FRotationMatrix(UserPlaySpaceRot) : FRotationMatrix::Identity;

	// Acquire info about the shake we're running, and initialize our transient state.
	const bool bIsRestarting = State.IsActive();
	FCameraShakeInfo ActiveInfo;
	GetShakeInfo(ActiveInfo);
	State.Initialize(ActiveInfo);

	// Let the root pattern initialize itself.
	if (RootShakePattern)
	{
		FCameraShakeStartParams StartParams;
		StartParams.bIsRestarting = bIsRestarting;
		RootShakePattern->StartShakePattern(StartParams);
	}
}

void UCameraShakeBase::UpdateAndApplyCameraShake(float DeltaTime, float Alpha, FMinimalViewInfo& InOutPOV)
{
	SCOPE_CYCLE_COUNTER(STAT_UpdateShake);
	SCOPE_CYCLE_UOBJECT(This, this);

	checkf(State.IsActive(), TEXT("Updating a camera shake that wasn't started with a call to StartShake!"));

	// Update our state, and early out if we have gone over the duration of the shake.
	float BlendingWeight = State.Update(DeltaTime);
	if (!State.IsActive())
	{
		return;
	}

	// Make the sub-class do the actual work.
	FCameraShakeUpdateParams Params(InOutPOV);
	Params.DeltaTime = DeltaTime;
	Params.ShakeScale = ShakeScale;
	Params.DynamicScale = Alpha;
	Params.BlendingWeight = BlendingWeight;
	
	// Result object is initialized with zero values since the default flags make us handle it
	// as an additive offset.
	FCameraShakeUpdateResult Result;

	if (RootShakePattern)
	{
		RootShakePattern->UpdateShakePattern(Params, Result);
	}

	// Apply the result to the given view info.
	FCameraShakeApplyResultParams ApplyParams;
	ApplyParams.Scale = Params.GetTotalScale();
	ApplyParams.PlaySpace = PlaySpace;
	ApplyParams.UserPlaySpaceMatrix = UserPlaySpaceMatrix;
	ApplyParams.CameraManager = CameraManager;
	ApplyResult(ApplyParams, Result, InOutPOV);
}

void UCameraShakeBase::ScrubAndApplyCameraShake(float AbsoluteTime, float Alpha, FMinimalViewInfo& InOutPOV)
{
	// This code is similar to the above UpdateAndApplyCameraShake method, but calls the scrub method
	// on the state manager and root pattern instead of the update method.
	
	SCOPE_CYCLE_COUNTER(STAT_UpdateShake);
	SCOPE_CYCLE_UOBJECT(This, this);

	checkf(State.IsActive(), TEXT("Updating a camera shake that wasn't started with a call to StartShake!"));

	float BlendingWeight = State.Scrub(AbsoluteTime);
	if (!State.IsActive())
	{
		return;
	}

	// Make the sub-class do the actual work.
	FCameraShakeScrubParams Params(InOutPOV);
	Params.AbsoluteTime = AbsoluteTime;
	Params.ShakeScale = ShakeScale;
	Params.DynamicScale = Alpha;
	Params.BlendingWeight = BlendingWeight;

	// Result object is initialized with zero values since the default flags make us handle it
	// as an additive offset.
	FCameraShakeUpdateResult Result;

	if (RootShakePattern)
	{
		RootShakePattern->ScrubShakePattern(Params, Result);
	}

	// Apply the result to the given view info.
	FCameraShakeApplyResultParams ApplyParams;
	ApplyParams.Scale = Params.GetTotalScale();
	ApplyParams.PlaySpace = PlaySpace;
	ApplyParams.UserPlaySpaceMatrix = UserPlaySpaceMatrix;
	ApplyParams.CameraManager = CameraManager;
	ApplyResult(ApplyParams, Result, InOutPOV);
}

bool UCameraShakeBase::IsFinished() const
{
	if (State.IsActive())
	{
		switch (State.GetShakeInfo().Duration.GetDurationType())
		{
			case ECameraShakeDurationType::Fixed:
				// If we have duration information, we can simply figure out ourselves if
				// we are finished.
				return State.GetElapsedTime() >= State.GetDuration();

			case ECameraShakeDurationType::Infinite:
				return false;

			case ECameraShakeDurationType::Custom:
				if (RootShakePattern)
				{
					// Ask the root pattern whether it's finished.
					return RootShakePattern->IsFinished();
				}
				else
				{
					// We have no root pattern, we don't have anything to do.
					return true;
				}
		}
	}
	// We're not active, so we're finished.
	return true;
}

void UCameraShakeBase::StopShake(bool bImmediately)
{
	if (!ensureMsgf(State.IsActive(), TEXT("Stopping a shake that wasn't active")))
	{
		return;
	}

	// Make our transient state as stopping or stopped.
	State.Stop(bImmediately);

	// Let the root pattern do any custom logic.
	if (RootShakePattern)
	{
		FCameraShakeStopParams StopParams;
		StopParams.bImmediately = bImmediately;
		RootShakePattern->StopShakePattern(StopParams);
	}
}

void UCameraShakeBase::TeardownShake()
{
	if (RootShakePattern)
	{
		RootShakePattern->TeardownShakePattern();
	}

	State = FCameraShakeState();
}

void UCameraShakeBase::ApplyResult(const FCameraShakeApplyResultParams& ApplyParams, const FCameraShakeUpdateResult& InResult, FMinimalViewInfo& InOutPOV)
{
	FCameraShakeUpdateResult TempResult(InResult);

	// If the sub-class gave us a delta-transform, we can help with some of the basic functionality
	// of a camera shake... namely: apply shake scaling, system limits, and play space transformation.
	if (!EnumHasAnyFlags(TempResult.Flags, ECameraShakeUpdateResultFlags::ApplyAsAbsolute))
	{
		if (!EnumHasAnyFlags(TempResult.Flags, ECameraShakeUpdateResultFlags::SkipAutoScale))
		{
			ApplyScale(ApplyParams.Scale, TempResult);
		}

		ApplyLimits(InOutPOV, TempResult);

		if (!EnumHasAnyFlags(TempResult.Flags, ECameraShakeUpdateResultFlags::SkipAutoPlaySpace))
		{
			ApplyPlaySpace(ApplyParams.PlaySpace, ApplyParams.UserPlaySpaceMatrix, InOutPOV, TempResult);
		}
	}

	// Now we can apply the shake to the camera matrix.
	if (EnumHasAnyFlags(TempResult.Flags, ECameraShakeUpdateResultFlags::ApplyAsAbsolute))
	{
		InOutPOV.Location = TempResult.Location;
		InOutPOV.Rotation = TempResult.Rotation;
		InOutPOV.FOV = TempResult.FOV;
	}
	else
	{
		InOutPOV.Location += TempResult.Location;
		InOutPOV.Rotation += TempResult.Rotation;
		InOutPOV.FOV += TempResult.FOV;
	}

	// It's weird but the post-process settings go directly on the camera manager, not on the view info.
	if (ApplyParams.CameraManager.IsValid() && TempResult.PostProcessBlendWeight > 0.f)
	{
		ApplyParams.CameraManager->AddCachedPPBlend(TempResult.PostProcessSettings, TempResult.PostProcessBlendWeight);
	}
}

void UCameraShakeBase::ApplyScale(const FCameraShakeUpdateParams& Params, FCameraShakeUpdateResult& InOutResult) const
{
	return ApplyScale(Params.GetTotalScale(), InOutResult);
}

void UCameraShakeBase::ApplyScale(float Scale, FCameraShakeUpdateResult& InOutResult)
{
	InOutResult.Location *= Scale;
	InOutResult.Rotation *= Scale;
	InOutResult.FOV *= Scale;
	InOutResult.PostProcessBlendWeight *= Scale;
}

void UCameraShakeBase::ApplyLimits(const FMinimalViewInfo& InPOV, FCameraShakeUpdateResult& InOutResult)
{
	// Don't allow shake to flip pitch past vertical, if not using a headset.
	// If using a headset, we can't limit the camera locked to your head.
	if (!GEngine->XRSystem.IsValid() || !GEngine->XRSystem->IsHeadTrackingAllowed())
	{
		// Find normalized result when combined, and remove any offset that would push it past the limit.
		const float NormalizedInputPitch = FRotator::NormalizeAxis(InPOV.Rotation.Pitch);
		const float NormalizedOutputPitchOffset = FRotator::NormalizeAxis(InOutResult.Rotation.Pitch);
		InOutResult.Rotation.Pitch = FMath::ClampAngle(NormalizedInputPitch + NormalizedOutputPitchOffset, -89.9f, 89.9f) - NormalizedInputPitch;
	}
}

void UCameraShakeBase::ApplyPlaySpace(const FCameraShakeUpdateParams& Params, FCameraShakeUpdateResult& InOutResult) const
{
	ApplyPlaySpace(PlaySpace, UserPlaySpaceMatrix, Params.POV, InOutResult);
}

void UCameraShakeBase::ApplyPlaySpace(ECameraShakePlaySpace PlaySpace, FMatrix UserPlaySpaceMatrix, const FMinimalViewInfo& InPOV, FCameraShakeUpdateResult& InOutResult)
{
	// Orient the shake according to the play space.
	const bool bIsCameraLocal = (PlaySpace == ECameraShakePlaySpace::CameraLocal);
	const FCameraAnimationHelperOffset CameraOffset { InOutResult.Location, InOutResult.Rotation };
	if (bIsCameraLocal)
	{
		FCameraAnimationHelper::ApplyOffset(InPOV, CameraOffset, InOutResult.Location, InOutResult.Rotation);
	}
	else
	{
		FCameraAnimationHelper::ApplyOffset(UserPlaySpaceMatrix, InPOV, CameraOffset, InOutResult.Location, InOutResult.Rotation);
	}

	// We have a final location/rotation for the camera, so it should be applied verbatim.
	InOutResult.Flags = (InOutResult.Flags | ECameraShakeUpdateResultFlags::ApplyAsAbsolute);

	// And since we set that flag, we need to make the FOV absolute too.
	InOutResult.FOV = InPOV.FOV + InOutResult.FOV;
}

UCameraShakePattern::UCameraShakePattern(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UCameraShakeBase* UCameraShakePattern::GetShakeInstance() const
{
	return GetTypedOuter<UCameraShakeBase>();
}

void UCameraShakePattern::GetShakePatternInfo(FCameraShakeInfo& OutInfo) const
{
	GetShakePatternInfoImpl(OutInfo);
}

void UCameraShakePattern::StartShakePattern(const FCameraShakeStartParams& Params)
{
	StartShakePatternImpl(Params);
}

void UCameraShakePattern::UpdateShakePattern(const FCameraShakeUpdateParams& Params, FCameraShakeUpdateResult& OutResult)
{
	UpdateShakePatternImpl(Params, OutResult);
}

void UCameraShakePattern::ScrubShakePattern(const FCameraShakeScrubParams& Params, FCameraShakeUpdateResult& OutResult)
{
	ScrubShakePatternImpl(Params, OutResult);
}

bool UCameraShakePattern::IsFinished() const
{
	return IsFinishedImpl();
}

void UCameraShakePattern::StopShakePattern(const FCameraShakeStopParams& Params)
{
	StopShakePatternImpl(Params);
}

void UCameraShakePattern::TeardownShakePattern()
{
	TeardownShakePatternImpl();
}


