// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/CollisionResolutionTypes.h"
#include "Chaos/Collision/CollisionApplyType.h"
#include "Chaos/Collision/CollisionConstraintAllocator.h"
#include "Chaos/Collision/CollisionContext.h"
#include "Chaos/Collision/PBDCollisionConstraint.h"
#include "Chaos/Collision/PBDCollisionConstraintHandle.h"
#include "Chaos/Collision/PBDCollisionSolverSettings.h"
#include "Chaos/PBDConstraintContainer.h"
#include "Framework/BufferedData.h"

#include <memory>
#include <queue>
#include <sstream>
#include "BoundingVolume.h"
#include "AABBTree.h"

namespace Chaos
{
class FImplicitObject;
class FPBDCollisionConstraints;
class FPBDRigidsSOAs;
class FPBDCollisionConstraint;

using FRigidBodyContactConstraintsPostComputeCallback = TFunction<void()>;
using FRigidBodyContactConstraintsPostApplyCallback = TFunction<void(const FReal Dt, const TArray<FPBDCollisionConstraintHandle*>&)>;
using FRigidBodyContactConstraintsPostApplyPushOutCallback = TFunction<void(const FReal Dt, const TArray<FPBDCollisionConstraintHandle*>&, bool)>;


namespace Private
{
	// The type of solver to use for collisions
	enum class ECollisionSolverType
	{
		GaussSeidel,
		GaussSeidelSimd,
		PartialJacobi,
	};
}

/**
 * A container and solver for collision constraints.
 * 
 * @todo(chaos): remove handles array
 */
class CHAOS_API FPBDCollisionConstraints : public FPBDConstraintContainer
{
public:
	friend class FPBDCollisionConstraintHandle;

	using Base = FPBDIndexedConstraintContainer;

	// Collision constraints have intrusive pointers. An array of constraint pointers can be uased as an array of handle pointers
	using FHandles = TArrayView<FPBDCollisionConstraint* const>;
	using FConstHandles = TArrayView<const FPBDCollisionConstraint* const>;

	// For use by dependent types
	using FConstraintContainerHandle = FPBDCollisionConstraintHandle;		// Used by constraint rules

	FPBDCollisionConstraints(const FPBDRigidsSOAs& InParticles, 
		TArrayCollectionArray<bool>& Collided, 
		const TArrayCollectionArray<TSerializablePtr<FChaosPhysicsMaterial>>& PhysicsMaterials, 
		const TArrayCollectionArray<TUniquePtr<FChaosPhysicsMaterial>>& PerParticlePhysicsMaterials,
		const THandleArray<FChaosPhysicsMaterial>* const SimMaterials,
		const int32 NumCollisionsPerBlock = 1000,
		const FReal RestitutionThreshold = FReal(2000));

	virtual ~FPBDCollisionConstraints();

	/**
	 * Whether this container provides constraint handles (simple solvers do not need them)
	 */
	bool GetHandlesEnabled() const { return bHandlesEnabled; }

	/**
	 * Put the container in "no handles" mode for use with simple solver. Must be called when empty of constraints (ideally right after creation).
	 */
	void DisableHandles();

	/**
	 * @brief Enable or disable determinism.
	 * Support for determinism requires that we sort active constraints each tick, so there is additional cost.
	*/
	void SetIsDeterministic(const bool bInIsDeterministic)
	{
		bIsDeterministic = bInIsDeterministic;
	}

	/**
	 *  Clears the list of active constraints.
	 * @todo(chaos): This is only required because of the way events work (see AdvanceOneTimeStepTask::DoWork)
	*/
	void BeginFrame();

	/**
	*  Destroy all constraints 
	*/
	void Reset();


	/**
	 * @brief Called before collision detection to reset contacts
	*/
	void BeginDetectCollisions();

	/**
	 * @brief Called after collision detection to finalize the contacts
	*/
	void EndDetectCollisions();

	/**
	 * @brief Called after collision resolution in order to detect probes
	 */
	void DetectProbeCollisions(FReal Dt);

	/**
	 * Apply modifiers to particle pair midphases
	 */
	void ApplyMidPhaseModifier(const TArray<ISimCallbackObject*>& MidPhaseModifiers, FReal Dt);

	/**
	 * Apply modifiers to the constraints and specify which constraints should be disabled.
	 * You would probably call this in the PostComputeCallback. Prefer this to calling RemoveConstraints in a loop,
	 * so you don't have to worry about constraint iterator/indices changing.
	 */
	void ApplyCollisionModifier(const TArray<ISimCallbackObject*>& CollisionModifiers, FReal Dt);


	/**
	* Remove the constraints associated with the ParticleHandle.
	*/
	void RemoveConstraints(const TSet<FGeometryParticleHandle*>&  ParticleHandle);

	/**
	 * @brief Remove all constraints associated with the particles - called when particles are destroyed
	*/
	virtual void DisconnectConstraints(const TSet<FGeometryParticleHandle*>& ParticleHandles) override;

	/**
	* Disable the constraints associated with the ParticleHandle.
	*/
	void DisableConstraints(const TSet<FGeometryParticleHandle*>& ParticleHandle) {}


	//
	// FConstraintContainer Implementation
	//
	virtual int32 GetNumConstraints() const override final { return NumConstraints(); }
	virtual void ResetConstraints() override final { Reset(); }
	virtual void AddConstraintsToGraph(Private::FPBDIslandManager& IslandManager) override final;
	virtual void PrepareTick() override final {}
	virtual void UnprepareTick() override final {}

	virtual TUniquePtr<FConstraintContainerSolver> CreateSceneSolver(const int32 Priority) override final;

	virtual TUniquePtr<FConstraintContainerSolver> CreateGroupSolver(const int32 Priority) override final;

	// The type of solver we are creating
	Private::ECollisionSolverType GetSolverType() const
	{
		return CollisionSolverType;
	}

	// Set the solver type. NOTE: Any previously created solvers will not be recreated at this level. (See FPBDRigidsEvolutionGBF::UpdateCollisionSolverType)
	void SetSolverType(const Private::ECollisionSolverType InSolverType)
	{
		CollisionSolverType = InSolverType;
	}

	//
	// Member Access
	//

	void SetCanDisableContacts(bool bInCanDisableContacts)
	{
		bCanDisableContacts = bInCanDisableContacts;
	}

	bool GetCanDisableContacts() const
	{
		return bCanDisableContacts;
	}

	void SetRestitutionThreshold(FReal InRestitutionThreshold)
	{
		RestitutionThreshold = InRestitutionThreshold;
	}

	FReal GetRestitutionThreshold() const
	{
		return RestitutionThreshold;
	}

	void SetCollisionsEnabled(bool bInEnableCollisions)
	{
		bEnableCollisions = bInEnableCollisions;
	}

	bool GetCollisionsEnabled() const
	{
		return bEnableCollisions;
	}

	void SetRestitutionEnabled(bool bInEnableRestitution)
	{
		bEnableRestitution = bInEnableRestitution;
	}

	bool GetRestitutionEnabled() const
	{
		return bEnableRestitution;
	}

	void SetGravity(const FVec3& InGravity)
	{
		GravityDirection = InGravity;
		GravitySize = GravityDirection.SafeNormalize();
	}

	FVec3 GetGravityDirection() const
	{
		return GravityDirection;
	}

	FReal GetGravitySize() const
	{
		return GravitySize;
	}

	void SetMaxPushOutVelocity(const FReal InMaxPushOutVelocity)
	{
		SolverSettings.MaxPushOutVelocity = InMaxPushOutVelocity;
	}

	void SetPositionFrictionIterations(const int32 InNumIterations)
	{
		SolverSettings.NumPositionFrictionIterations = InNumIterations;
	}

	void SetVelocityFrictionIterations(const int32 InNumIterations)
	{
		SolverSettings.NumVelocityFrictionIterations = InNumIterations;
	}

	void SetPositionShockPropagationIterations(const int32 InNumIterations)
	{
		SolverSettings.NumPositionShockPropagationIterations = InNumIterations;
	}

	void SetVelocityShockPropagationIterations(const int32 InNumIterations)
	{
		SolverSettings.NumVelocityShockPropagationIterations = InNumIterations;
	}

	int32 NumConstraints() const
	{
		return GetConstraints().Num();
	}

	TArrayView<FPBDCollisionConstraint* const> GetConstraints() const
	{
		return ConstraintAllocator.GetConstraints();
	}

	FHandles GetConstraintHandles() const;
	FConstHandles GetConstConstraintHandles() const;

	const FPBDCollisionConstraint& GetConstraint(int32 Index) const;

	Private::FCollisionConstraintAllocator& GetConstraintAllocator() { return ConstraintAllocator; }

	void UpdateConstraintMaterialProperties(FPBDCollisionConstraint& Contact);

	const FPBDCollisionSolverSettings& GetSolverSettings() const { return SolverSettings; }

	const FCollisionDetectorSettings& GetDetectorSettings() const { return DetectorSettings; }

	void SetDetectorSettings(const FCollisionDetectorSettings& InSettings)
	{
		DetectorSettings = InSettings;
	}

	void SetCullDistance(const FReal InCullDistance)
	{
		DetectorSettings.BoundsExpansion = InCullDistance;
	}

protected:
	FPBDCollisionConstraint& GetConstraint(int32 Index);

	// Call PruneParticleEdgeCollisions on all particles with ECollisionConstraintFlags::CCF_SmoothEdgeCollisions set in CollisionFlags
	void PruneEdgeCollisions();

private:
	const FPBDRigidsSOAs& Particles;

	Private::FCollisionConstraintAllocator ConstraintAllocator;
	int32 NumActivePointConstraints;
	TArray<FPBDCollisionConstraintHandle*> TempCollisions;	// Reused from tick to tick to build contact lists

	TArrayCollectionArray<bool>& MCollided;
	const TArrayCollectionArray<TSerializablePtr<FChaosPhysicsMaterial>>& MPhysicsMaterials;
	const TArrayCollectionArray<TUniquePtr<FChaosPhysicsMaterial>>& MPerParticlePhysicsMaterials;
	const THandleArray<FChaosPhysicsMaterial>* const SimMaterials;

	FReal RestitutionThreshold;
	bool bEnableCollisions;
	bool bEnableRestitution;
	bool bHandlesEnabled;
	bool bEnableEdgePruning;
	bool bIsDeterministic;

	// This is passed to IterationParameters. If true, then an iteration can cull a contact
	// permanently (ie, for the remaining iterations) if it is ignored due to culldistance.
	// This improves performance, but can decrease stability if contacts are culled prematurely.
	bool bCanDisableContacts;

	Private::ECollisionSolverType CollisionSolverType;

	// Used to determine constraint directions
	FVec3 GravityDirection;
	FReal GravitySize;

	// Settings for the low-level collision solvers
	FPBDCollisionSolverSettings SolverSettings;

	// Settings for collision detection
	FCollisionDetectorSettings DetectorSettings;
};

//
//
// Inlined FPBDCollisionConstraintHandle functions. Here to avoid circular deps
//
//

inline const FPBDCollisionConstraints* FPBDCollisionConstraintHandle::ConcreteContainer() const
{
	return static_cast<FPBDCollisionConstraints*>(ConstraintContainer);
}

inline FPBDCollisionConstraints* FPBDCollisionConstraintHandle::ConcreteContainer()
{
	return static_cast<FPBDCollisionConstraints*>(ConstraintContainer);
}

inline const FPBDCollisionConstraint& FPBDCollisionConstraintHandle::GetContact() const
{
	return *GetConstraint();
}

inline FPBDCollisionConstraint& FPBDCollisionConstraintHandle::GetContact()
{
	return *GetConstraint();
}

inline bool FPBDCollisionConstraintHandle::GetCCDEnabled() const
{
	return GetContact().GetCCDEnabled();
}

inline void FPBDCollisionConstraintHandle::SetEnabled(bool InEnabled)
{
	GetContact().SetDisabled(!InEnabled);
}

inline bool FPBDCollisionConstraintHandle::IsEnabled() const
{
	return !GetContact().GetDisabled();
}

inline bool FPBDCollisionConstraintHandle::IsProbe() const
{
	return GetContact().GetIsProbe();
}

inline FVec3 FPBDCollisionConstraintHandle::GetAccumulatedImpulse() const
{
	return GetContact().AccumulatedImpulse;
}

inline FParticlePair FPBDCollisionConstraintHandle::GetConstrainedParticles() const
{
	return { GetContact().GetParticle0(), GetContact().GetParticle1() };
}

}
