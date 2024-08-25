// Copyright Epic Games, Inc. All Rights Reserved.

#include "BoneControllers/AnimNode_Steering.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNodeFunctionRef.h"
#include "Animation/AnimRootMotionProvider.h"
#include "HAL/IConsoleManager.h"
#include "Animation/AnimTrace.h"
#include "BoneControllers/AnimNode_OffsetRootBone.h"
#include "Kismet/KismetMathLibrary.h"
#include "Logging/LogVerbosity.h"
#include "VisualLogger/VisualLogger.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNode_Steering)

void FAnimNode_Steering::UpdateInternal(const FAnimationUpdateContext& Context)
{
	FAnimNode_SkeletalControlBase::UpdateInternal(Context);

	if (UE::AnimationWarping::FRootOffsetProvider* RootOffsetProvider = Context.GetMessage<UE::AnimationWarping::FRootOffsetProvider>())
	{
		RootBoneTransform = RootOffsetProvider->GetRootTransform();
	}
	else
	{
		RootBoneTransform = Context.AnimInstanceProxy->GetComponentTransform();
	}
}

void FAnimNode_Steering::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_SkeletalControlBase::Initialize_AnyThread(Context);
	bResetFilter = true;
}


void FAnimNode_Steering::EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms)
{
	const float DeltaSeconds = Output.AnimInstanceProxy->GetDeltaSeconds();

	if (DeltaSeconds > 0)
	{
		if (bEnableTargetSmoothing)
		{
			if(bResetFilter)
			{
				FilteredTarget = TargetOrientation;
				TargetSmoothingState.Reset();
				bResetFilter = false;
			}
			
			FilteredTarget = UKismetMathLibrary::QuaternionSpringInterp(FilteredTarget, TargetOrientation, TargetSmoothingState,
								SmoothTargetStiffness, SmoothTargetDamping, DeltaSeconds, 1, 0, true);

			TargetOrientation = FilteredTarget;
		}
		
		if (Alpha > 0.0f)
		{
			const UE::Anim::IAnimRootMotionProvider* RootMotionProvider = UE::Anim::IAnimRootMotionProvider::Get();
			ensureMsgf(RootMotionProvider, TEXT("Steering expected a valid root motion delta provider interface."));

			if (RootMotionProvider)
			{
				FTransform ThisFrameRootMotionTransform = FTransform::Identity;
				if (RootMotionProvider->ExtractRootMotion(Output.CustomAttributes, ThisFrameRootMotionTransform))
				{
					if (ThisFrameRootMotionTransform.GetTranslation().Length() / DeltaSeconds > DisableSteeringBelowSpeed)
					{
						FQuat RootBoneRotation = RootBoneTransform.GetRotation();

						UE_VLOG_ARROW(Output.AnimInstanceProxy->GetAnimInstanceObject(), "Steering", Display,
							RootBoneTransform.GetLocation(),
							RootBoneTransform.GetLocation()  + RootBoneRotation.GetRightVector() * 90,
							FColor::Green, TEXT(""));
						
						UE_VLOG_ARROW(Output.AnimInstanceProxy->GetAnimInstanceObject(), "Steering", Display,
							RootBoneTransform.GetLocation(),
							RootBoneTransform.GetLocation()  + TargetOrientation.GetRightVector() * 100,
							FColor::Blue, TEXT(""));

						FQuat DeltaToTargetOrientation =  RootBoneRotation.Inverse() * TargetOrientation;

						if (AnimatedTargetTime > 0)
						{
							if (UAnimSequenceBase* AnimSequence = Cast<UAnimSequenceBase>(CurrentAnimAsset))
							{
								FTransform PredictedRootMotionTransform = AnimSequence->ExtractRootMotion(CurrentAnimAssetTime, AnimatedTargetTime, true);
								FQuat PredictedRootMotionQuat = PredictedRootMotionTransform.GetRotation();
								float PredictedRootMotionYaw = FRotator(PredictedRootMotionQuat).Yaw;
							
								if (fabs(PredictedRootMotionYaw) > RootMotionThreshold)
								{
									UE_VLOG_ARROW(Output.AnimInstanceProxy->GetAnimInstanceObject(), "Steering", Display,
										RootBoneTransform.GetLocation(),
										RootBoneTransform.GetLocation()  + (PredictedRootMotionQuat * RootBoneRotation).GetRightVector() * 100,
										FColor::Orange, TEXT(""));

									float YawToTargetOrientation = FRotator(DeltaToTargetOrientation).Yaw;

									// pick the rotation direction that is the shortest path from the endpoint of the current animated rotation
									if (PredictedRootMotionYaw - YawToTargetOrientation > 180)
									{
										YawToTargetOrientation += 360;
									}
									else if (YawToTargetOrientation - PredictedRootMotionYaw > 180)
									{
										YawToTargetOrientation -=360;
									}

									float Ratio =  YawToTargetOrientation / PredictedRootMotionYaw ;

									FRotator ThisFrameRootMotionRotation(ThisFrameRootMotionTransform.GetRotation());
									ThisFrameRootMotionRotation.Yaw *= Ratio;
								
									ThisFrameRootMotionTransform.SetRotation(FQuat(ThisFrameRootMotionRotation));
									RootMotionProvider->OverrideRootMotion(ThisFrameRootMotionTransform, Output.CustomAttributes);
								
									return;
								}
							}
						}
						
						if (ProceduralTargetTime > DeltaSeconds)
						{	
							DeltaToTargetOrientation = FQuat::Slerp(FQuat::Identity, DeltaToTargetOrientation,  DeltaSeconds/ProceduralTargetTime);
						}

						ThisFrameRootMotionTransform.SetRotation(FQuat::Slerp(ThisFrameRootMotionTransform.GetRotation(), DeltaToTargetOrientation, Alpha));
						RootMotionProvider->OverrideRootMotion(ThisFrameRootMotionTransform, Output.CustomAttributes);
					}
				}
			}
		}
	}
}
