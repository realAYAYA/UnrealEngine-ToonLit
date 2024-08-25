// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Animation/AnimCurveTypes.h"
#include "Animation/AttributesRuntime.h"
#include "LODPose.h"

namespace UE::AnimNext
{
	/*
	 * Key Frame State
	 * 
	 * This struct holds sampled keyframe state or interpolated keyframe state.
	 * It holds a pose, trajectory, curves, attributes, etc that one might find in
	 * an animation sequence or as an output of an animation node.
	 */
	struct ANIMNEXT_API FKeyframeState
	{
		// Joint transforms at a particular LOD (on memstack)
		FLODPoseStack Pose;

		// Float curves at a particular LOD (on memstack)
		FBlendedCurve Curves;

		// Attributes at a particular LOD (on memstack)
		UE::Anim::FStackAttributeContainer Attributes;

		[[nodiscard]] bool IsValid() const { return Pose.IsValid(); }
		[[nodiscard]] bool IsAdditive() const { return Pose.IsAdditive(); }
	};
}
