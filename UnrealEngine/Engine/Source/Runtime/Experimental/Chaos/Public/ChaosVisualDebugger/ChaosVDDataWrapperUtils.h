// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_CHAOS_VISUAL_DEBUGGER
#include "Chaos/Collision/ContactPoint.h"
#include "Chaos/ParticleHandleFwd.h"
#include "DataWrappers/ChaosVDCollisionDataWrappers.h"

namespace Chaos
{
	class FParticlePairMidPhase;
	class FPBDCollisionConstraint;
}

class FChaosVisualDebuggerTrace;
struct FChaosVDConstraint;
struct FChaosVDParticlePairMidPhase;
struct FChaosVDParticleDataWrapper;

/**
 * Helper class used to build Chaos Visual Debugger data wrappers, without directly referencing chaos' types in them.
 *
 * @note: This is needed for now because we want to keep the data wrapper structs/classes on a separate module where possible, but if we reference Chaos's types
 * directly we will end up with a circular dependency issue because the ChaosVDRuntime module will need the Chaos module but the Chaos module will need the ChaosVDRuntime module to use the structs
 * Once development is done and can we commit to backward compatibility, this helper class might go away (trough the proper deprecation process)
 */
class FChaosVDDataWrapperUtils
{
private:

	/** Takes a FManifoldPoint and copies the relevant data to the CVD counterpart */
	static void CopyManifoldPointsToDataWrapper(const Chaos::FManifoldPoint& InCopyFrom, FChaosVDManifoldPoint& OutCopyTo);

	/** Takes a FManifoldPointResult and copies the relevant data to the CVD counterpart */
	static void CopyManifoldPointResultsToDataWrapper(const Chaos::FManifoldPointResult& InCopyFrom, FChaosVDManifoldPoint& OutCopyTo);

	/** Creates and populates a FChaosVDParticleDataWrapper with the data of the provided FGeometryParticleHandle */
	static FChaosVDParticleDataWrapper BuildParticleDataWrapperFromParticle(const Chaos::FGeometryParticleHandle* ParticleHandlePtr);

	/** Creates and populates a FChaosVDConstraint with the data of the provided FPBDCollisionConstraint */
	static FChaosVDConstraint BuildConstraintDataWrapperFromConstraint(const Chaos::FPBDCollisionConstraint& InConstraint);

	/** Creates and populates a FChaosVDParticlePairMidPhase with the data of the provided FParticlePairMidPhase */
	static FChaosVDParticlePairMidPhase BuildMidPhaseDataWrapperFromMidPhase(const Chaos::FParticlePairMidPhase& InMidPhase);

	/** Converts a Chaos::FVec3 to a FVector. It is worth notice that FVector is double precision and FVec3 is single */
	static FVector ConvertToFVector(const Chaos::FVec3f& VectorRef) { return FVector(VectorRef); }

	friend FChaosVisualDebuggerTrace;
};
#endif //WITH_CHAOS_VISUAL_DEBUGGER
