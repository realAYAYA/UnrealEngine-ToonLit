// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Physics/ImmediatePhysics/ImmediatePhysicsChaos/ImmediatePhysicsCore_Chaos.h"
#include "Physics/ImmediatePhysics/ImmediatePhysicsChaos/ImmediatePhysicsActorHandle_Chaos.h"

#include "Chaos/Vector.h"

#include "Engine/EngineTypes.h"

namespace ImmediatePhysics_Chaos
{
	/** handle associated with a physics joint. This is the proper way to read/write to the physics simulation */
	struct ENGINE_API FJointHandle
	{
	public:
		using FChaosConstraintContainer = Chaos::FPBDJointConstraints;
		using FChaosConstraintHandle = typename Chaos::FPBDJointConstraintHandle;

		FJointHandle(FChaosConstraintContainer* InConstraints, FConstraintInstance* ConstraintInstance, FActorHandle* InActor1, FActorHandle* InActor2);
		~FJointHandle();

		FChaosConstraintHandle* GetConstraint();
		const FChaosConstraintHandle* GetConstraint() const;

		const Chaos::TVec2<FActorHandle*>& GetActorHandles();
		const Chaos::TVec2<const FActorHandle*>& GetActorHandles() const;

		void SetSoftLinearSettings(bool bLinearSoft, FReal LinearStiffness, FReal LinearDamping);

	private:
		Chaos::TVec2<FActorHandle*> ActorHandles;
		FChaosConstraintContainer* Constraints;
		FChaosConstraintHandle* ConstraintHandle;
	};
}
