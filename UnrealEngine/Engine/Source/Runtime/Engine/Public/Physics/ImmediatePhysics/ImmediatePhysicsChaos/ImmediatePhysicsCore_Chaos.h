// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Physics/ImmediatePhysics/ImmediatePhysicsChaos/ImmediatePhysicsDeclares_Chaos.h"
#include "Physics/ImmediatePhysics/ImmediatePhysicsShared/ImmediatePhysicsCore.h"

#include "Chaos/Core.h"
#include "Chaos/ParticleHandleFwd.h"
#include "Chaos/PBDRigidsEvolutionFwd.h"

namespace Chaos
{
	class FImplicitObject;
	class FBasicBroadPhase;
	class FBasicCollisionDetector;
	class FPBDJointConstraintHandle;
	class FPBDJointConstraints;
	class FPerShapeData;
	template<class T> class TArrayCollectionArray;
	struct FKinematicGeometryParticleParameters;
	template<typename T, int D> class TKinematicTarget;
	template<typename T> class TPBDConstraintIslandRule;
	struct FPBDRigidParticleParameters;
	class FPBDRigidsSOAs;
	template<typename T> class TSimpleConstraintRule;

}

namespace ImmediatePhysics_Chaos
{
	using FReal = Chaos::FReal;
	using FRealSingle = Chaos::FRealSingle;
	//const int Dimensions = 3;

	using EActorType = ImmediatePhysics_Shared::EActorType;
	using EForceType = ImmediatePhysics_Shared::EForceType;

	using FKinematicTarget = Chaos::TKinematicTarget<FReal, 3>;
}

struct FBodyInstance;
struct FConstraintInstance;

// Used to define out code that still has to be implemented to match PhysX
#define IMMEDIATEPHYSICS_CHAOS_TODO 0
