// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Physics/ImmediatePhysics/ImmediatePhysicsChaos/ImmediatePhysicsCore_Chaos.h"
#include "Physics/ImmediatePhysics/ImmediatePhysicsChaos/ImmediatePhysicsActorHandle_Chaos.h"

#include "Chaos/Vector.h"

#include "Engine/EngineTypes.h"

namespace Chaos
{
	class FPBDJointSettings;
}

namespace ImmediatePhysics_Chaos
{
	/** handle associated with a physics joint. This is the proper way to read/write to the physics simulation */
	struct FJointHandle
	{
	public:
		using FChaosConstraintContainer = Chaos::FPBDJointConstraints;
		using FChaosConstraintHandle = typename Chaos::FPBDJointConstraintHandle;
		using FPBDJointSettings = Chaos::FPBDJointSettings;

		ENGINE_API FJointHandle(FChaosConstraintContainer* InConstraints, FConstraintInstance* ConstraintInstance, FActorHandle* InActor1, FActorHandle* InActor2);
		ENGINE_API FJointHandle(FChaosConstraintContainer* InConstraints, const FPBDJointSettings& ConstraintSettings, FActorHandle* InActor1, FActorHandle* InActor2);
		ENGINE_API ~FJointHandle();

		ENGINE_API FChaosConstraintHandle* GetConstraint();
		ENGINE_API const FChaosConstraintHandle* GetConstraint() const;

		ENGINE_API const Chaos::TVec2<FActorHandle*>& GetActorHandles();
		ENGINE_API const Chaos::TVec2<const FActorHandle*>& GetActorHandles() const;

		ENGINE_API void SetSoftLinearSettings(bool bLinearSoft, FReal LinearStiffness, FReal LinearDamping);

	private:
		void SetActorInertiaConditioningDirty();
		
		Chaos::TVec2<FActorHandle*> ActorHandles;
		FChaosConstraintContainer* Constraints;
		FChaosConstraintHandle* ConstraintHandle;
	};
}
