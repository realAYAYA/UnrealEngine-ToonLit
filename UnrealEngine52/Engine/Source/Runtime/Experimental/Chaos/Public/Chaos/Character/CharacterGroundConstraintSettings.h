// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#include "Chaos/Core.h"
#include "Chaos/Transform.h"
#include "Chaos/Vector.h"

namespace Chaos
{
	class CHAOS_API FCharacterGroundConstraintSettings
	{
	public:
		FCharacterGroundConstraintSettings()
			: VerticalAxis(FVec3(0, 0, 1))
			, TargetHeight(0)
			, RadialForceLimit(1000)
			, TorqueLimit(1000)
			, CosMaxWalkableSlopeAngle(0.633)
			, DampingFactor(0.0)
			, AssumedOnGroundHeight(5.0)
			, UserData(nullptr)
		{
		}

		bool operator==(const FCharacterGroundConstraintSettings& Other) const
		{
			return !FMemory::Memcmp(this, &Other, sizeof(*this));
		}

		FVec3 VerticalAxis;				/// World space up direction (default z axis)
		FReal TargetHeight;				/// Desired distance from the character body to the ground
		FReal RadialForceLimit;			/// How much force the character can apply parallel to the ground plane to reach the target position
		FReal TorqueLimit;				///	How much torque the character can apply about the vertical axis to reach the target facing direction
		FReal CosMaxWalkableSlopeAngle;	/// Cosine of the maximum angle in degrees that the character is allowed to walk on
		FReal DampingFactor;			/// Applies a damping to the vertical ground constraint making it softer. Units: /T
		FReal AssumedOnGroundHeight;	/// Below this height the character is assumed to be on the ground and can apply force/torque to reach the target position and facing 
		void* UserData;					/// Reserved for user data
	};

	class CHAOS_API FCharacterGroundConstraintDynamicData
	{
	public:
		FCharacterGroundConstraintDynamicData()
			: GroundNormal(FVec3(0, 0, 1))
			, TargetDeltaPosition(FVec3(0, 0, 0))
			, TargetDeltaFacing(0)
			, GroundDistance(0)
		{
		}

		bool operator==(const FCharacterGroundConstraintDynamicData& Other) const
		{
			return !FMemory::Memcmp(this, &Other, sizeof(*this));
		}

		FVec3 GroundNormal;				/// World space ground normal
		FVec3 TargetDeltaPosition;		///	Target linear movement vector. Will be projected onto ground plane
		FReal TargetDeltaFacing;		/// Target rotation in radians about the vertical axis
		FReal GroundDistance;			/// Distance from the character body to the ground
	};

} // namespace Chaos