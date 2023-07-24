// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Core.h"

#include "Chaos/ConstraintHandle.h"
#include "Chaos/Collision/CollisionApplyType.h"
#include "Chaos/Evolution/IndexedConstraintContainer.h"
#include "Chaos/ParticleHandleFwd.h"
#include "Chaos/PBDJointConstraintTypes.h"
#include "Chaos/PBDJointConstraintData.h"
#include "Chaos/GeometryParticles.h"

namespace Chaos
{
	namespace Private
	{
		class FPBDJointContainerSolver;
		class FPBDIslandManager;
	}

	/**
	 * A handle to a joint constraint held in a joint container (FPBDJointConstraints) by index.
	*/
	class CHAOS_API FPBDJointConstraintHandle final : public TIndexedContainerConstraintHandle<FPBDJointConstraints>
	{
	public:
		using Base = TIndexedContainerConstraintHandle<FPBDJointConstraints>;
		using FConstraintContainer = FPBDJointConstraints;

		FPBDJointConstraintHandle();
		FPBDJointConstraintHandle(FConstraintContainer* InConstraintContainer, int32 InConstraintIndex);

		void SetConstraintEnabled(bool bEnabled);

		void CalculateConstraintSpace(FVec3& OutXa, FMatrix33& OutRa, FVec3& OutXb, FMatrix33& OutRb) const;

		bool IsConstraintEnabled() const;
		bool IsConstraintBroken() const;
		bool IsConstraintBreaking() const;
		void ClearConstraintBreaking();
		bool IsDriveTargetChanged() const;
		void ClearDriveTargetChanged();
		FVec3 GetLinearImpulse() const;
		FVec3 GetAngularImpulse() const;

		const FPBDJointSettings& GetSettings() const;
		const FPBDJointSettings& GetJointSettings() const { return GetSettings(); }	//needed for property macros

		void SetSettings(const FPBDJointSettings& Settings);
		
		TVec2<FGeometryParticleHandle*> GetConstrainedParticles() const override final;

		static const FConstraintHandleTypeID& StaticType()
		{
			static FConstraintHandleTypeID STypeID(TEXT("FJointConstraintHandle"), &FIndexedConstraintHandle::StaticType());
			return STypeID;
		}

		ESyncState SyncState() const;
		void SetSyncState(ESyncState SyncState);

		void SetEnabledDuringResim(bool bEnabled);
		EResimType ResimType() const;

		UE_DEPRECATED(5.2, "No longer used")
		int32 GetConstraintIsland() const;
		UE_DEPRECATED(5.2, "No longer used")
		int32 GetConstraintLevel() const;
		UE_DEPRECATED(5.2, "No longer used")
		int32 GetConstraintColor() const;

	protected:
		using Base::ConstraintIndex;
		using Base::ConcreteContainer;
	private:
		bool bLinearPlasticityInitialized;
		bool bAngularPlasticityInitialized;
	};

	/**
	 * Peristent variable state for a joint
	*/
	class CHAOS_API FPBDJointState
	{
	public:
		FPBDJointState();

		int32 Island;
		int32 Level;
		int32 Color;
		int32 IslandSize;
		bool bDisabled;
		bool bBroken;
		bool bBreaking;
		bool bDriveTargetChanged;
		FVec3 LinearImpulse;
		FVec3 AngularImpulse;
		EResimType ResimType = EResimType::FullResim;
		ESyncState SyncState = ESyncState::InSync;
		bool bEnabledDuringResim = true;
	};

	/**
	 * A set of joint restricting up to 6 degrees of freedom, with linear and angular limits.
	 */
	class CHAOS_API FPBDJointConstraints : public FPBDConstraintContainer
	{
	public:
		using Base = FPBDConstraintContainer;

		using FConstraintContainerHandle = FPBDJointConstraintHandle;
		using FConstraintHandleAllocator = TConstraintHandleAllocator<FPBDJointConstraints>;
		using FTransformPair = TVector<FRigidTransform3, 2>;
		using FHandles = TArray<FConstraintContainerHandle*>;

		FPBDJointConstraints();

		virtual ~FPBDJointConstraints();

		/**
		 * Get the solver settings (used by FPBDJointContainerSolver)
		*/
		const FPBDJointSolverSettings& GetSettings() const;

		/*
		* Modify the solver settings
		*/
		void SetSettings(const FPBDJointSolverSettings& InSettings);

		/**
		* Whether to use a linear or non-linear joint solver. Non-linear is more stable but much more expensive.
		* A linear solver is used by default (see FPBDJointSolverSettings).
		*/
		void SetUseLinearJointSolver(const bool bInEnable) { Settings.bUseLinearSolver = bInEnable; }

		/**
		 * Whether to sort the joints internally. Sort will be triggered on any tick when a joint was added. Only needed for RBAN.
		*/
		void SetSortEnabled(const bool bInEnable) { Settings.bSortEnabled = bInEnable; }

		/**
		 * Get the number of constraints.
		 */
		int32 NumConstraints() const;

		/**
		 * Add a constraint with particle-space constraint offsets.
		 * @todo(chaos): clean up this set of functions (now that ConnectorTransforms is in the settings, calling AddConstraint then SetSettings leads 
		 * to unexpected behaviour - overwriting the ConnectorTransforms with Identity)
		 */
		FConstraintContainerHandle* AddConstraint(const FParticlePair& InConstrainedParticles, const FRigidTransform3& WorldConstraintFrame);
		FConstraintContainerHandle* AddConstraint(const FParticlePair& InConstrainedParticles, const FTransformPair& ConnectorTransforms);
		FConstraintContainerHandle* AddConstraint(const FParticlePair& InConstrainedParticles, const FPBDJointSettings& InConstraintSettings);

		/**
		 * Remove the specified constraint.
		 */
		void RemoveConstraint(int ConstraintIndex);
		void RemoveConstraints(const TSet<TGeometryParticleHandle<FReal, 3>*>& RemovedParticles) {}

		/*
		* Disconnect the constraints from the attached input particles. 
		* This will set the constrained Particle elements to nullptr and 
		* set the Enable flag to false.
		* 
		* The constraint is unuseable at this point and pending deletion. 
		*/
		void DisconnectConstraints(const TSet<TGeometryParticleHandle<FReal, 3>*>& RemovedParticles);

		/*
		 * Whether the constraint is enabled
		 */
		bool IsConstraintEnabled(int32 ConstraintIndex) const;

		/*
		 * Whether the constraint is broken
		 */
		bool IsConstraintBroken(int32 ConstraintIndex) const;

		/*
		 * Whether the constraint was broken this frame (transient flag for use by event system)
		 */
		bool IsConstraintBreaking(int32 ConstraintIndex) const;

		/*
		 * Clear the transient constraint braking state (called by event system when it has used the flag)
		 */
		void ClearConstraintBreaking(int32 ConstraintIndex);

		/*
		 * Whether the drive target has changed
		 */
		bool IsDriveTargetChanged(int32 ConstraintIndex) const;

		/*
		 * Clear the drive target state
		 */
		void ClearDriveTargetChanged(int32 ConstraintIndex);

		/*
		 * Enable or disable a constraints
		 */
		void SetConstraintEnabled(int32 ConstraintIndex, bool bEnabled);

		/*
		* Set Drive Target Changed State
		*/
		void SetDriveTargetChanged(int32 ConstraintIndex, bool bTargetChanged);

		/*
		 * Force a constraints to break
		 */
		void BreakConstraint(int32 ConstraintIndex);

		/**
		 * Repair a broken constraints (does not adjust particle positions)
		 */
		void FixConstraint(int32 ConstraintIndex);

		/**
		 * Set the break callback. This will be called once after the constraint solver phase on the \
		 * tick when the constraint is broken. There is only one callback allowed - it is usually a 
		 * method on the owner (e.g., the Evolution object) which will probably dispatch its own event.
		*/
		void SetBreakCallback(const FJointBreakCallback& Callback);

		/**
		 * Remove the previously assigned break callback.
		*/
		void ClearBreakCallback();

		/**
		 * All of the constraints in the container, including inactive
		*/
		FHandles& GetConstraintHandles()
		{
			return Handles;
		}

		/**
		 * All of the constraints in the container, including inactive
		*/
		const FHandles& GetConstConstraintHandles() const
		{
			return Handles;
		}

		/**
		 * Get a joint constraint by index
		*/
		const FConstraintContainerHandle* GetConstraintHandle(int32 ConstraintIndex) const;

		/**
		 * Get a joint constraint by index
		*/
		FConstraintContainerHandle* GetConstraintHandle(int32 ConstraintIndex);

		/**
		 * Get the particles that are affected by the specified constraint.
		 */
		const FParticlePair& GetConstrainedParticles(int32 ConstraintIndex) const;

		/**
		 * Get the settings for a joint constraint by index
		*/
		const FPBDJointSettings& GetConstraintSettings(int32 ConstraintIndex) const;

		/**
		 * Set the settings for a joint constraint by index
		*/
		void SetConstraintSettings(int32 ConstraintIndex, const FPBDJointSettings& InConstraintSettings);

		/**
		 * Set the linear drive target for a constraint by index
		*/
		void SetLinearDrivePositionTarget(int32 ConstraintIndex, FVec3 InLinearDrivePositionTarget);

		/**
		 * Set the angular drive target for a constraint by index
		*/
		void SetAngularDrivePositionTarget(int32 ConstraintIndex, FRotation3 InAngularDrivePositionTarget);

		/**
		* The total linear impulse applied by the constraint
		*/
		FVec3 GetConstraintLinearImpulse(int32 ConstraintIndex) const;

		/**
		* The total linear angular applied by the constraint
		*/
		FVec3 GetConstraintAngularImpulse(int32 ConstraintIndex) const;

		ESyncState GetConstraintSyncState(int32 ConstraintIndex) const;
		void SetConstraintSyncState(int32 ConstraintIndex, ESyncState SyncState);
		
		void SetConstraintEnabledDuringResim(int32 ConstraintIndex, bool bEnabled);
		
		EResimType GetConstraintResimType(int32 ConstraintIndex) const;

		//
		// FConstraintContainer Implementation
		//
		virtual TUniquePtr<FConstraintContainerSolver> CreateSceneSolver(const int32 Priority) override final;
		virtual TUniquePtr<FConstraintContainerSolver> CreateGroupSolver(const int32 Priority) override final;
		virtual int32 GetNumConstraints() const override final { return NumConstraints(); }
		virtual void ResetConstraints() override final {}
		virtual void AddConstraintsToGraph(Private::FPBDIslandManager& IslandManager) override final;
		virtual void PrepareTick() override final;
		virtual void UnprepareTick() override final;

		// Called by the joint solver at the end of the constraint solver phase
		void SetSolverResults(const int32 ConstraintIndex, const FVec3& LinearImpulse, const FVec3& AngularImpulse, const bool bIsBroken, const FSolverBody* SolverBody0, const FSolverBody* SolverBody1);

		// @todo(chaos): only needed for RBAN, and should be private or moved to the solver
		int32 GetConstraintIsland(int32 ConstraintIndex) const;
		int32 GetConstraintLevel(int32 ConstraintIndex) const;
		int32 GetConstraintColor(int32 ConstraintIndex) const;

		// @todo(chaos): functionality no longer supported here (see FPBDJointContainerSolver)
		UE_DEPRECATED(5.2, "Joint Solver API moved to FPBDJointContainerSolver")
		void AddBodies(FSolverBodyContainer& SolverBodyContainer) {}
		UE_DEPRECATED(5.2, "Joint Solver API moved to FPBDJointContainerSolver")
		void GatherInput(const FReal Dt){}
		UE_DEPRECATED(5.2, "Joint Solver API moved to FPBDJointContainerSolver")
		void ScatterOutput(const FReal Dt){}
		UE_DEPRECATED(5.2, "Joint Solver API moved to FPBDJointContainerSolver")
		void ApplyPositionConstraints(const FReal Dt, const int32 It, const int32 NumIts) {}
		UE_DEPRECATED(5.2, "Joint Solver API moved to FPBDJointContainerSolver")
		void ApplyVelocityConstraints(const FReal Dt, const int32 It, const int32 NumIts) {}
		UE_DEPRECATED(5.2, "Joint Solver API moved to FPBDJointContainerSolver")
		void ApplyProjectionConstraints(const FReal Dt, const int32 It, const int32 NumIts) {}
		UE_DEPRECATED(5.2, "Joint Solver API moved to FPBDJointContainerSolver")
		void AddBodies(const TArrayView<int32>& ConstraintIndices, FSolverBodyContainer& SolverBodyContainer) {}
		UE_DEPRECATED(5.2, "Joint Solver API moved to FPBDJointContainerSolver")
		void GatherInput(const TArrayView<int32>& ConstraintIndices, const FReal Dt) {}
		UE_DEPRECATED(5.2, "Joint Solver API moved to FPBDJointContainerSolver")
		void ScatterOutput(const TArrayView<int32>& ConstraintIndices, const FReal Dt) {}
		UE_DEPRECATED(5.2, "Joint Solver API moved to FPBDJointContainerSolver")
		void ApplyPositionConstraints(const TArrayView<int32>& ConstraintIndices, const FReal Dt, const int32 It, const int32 NumIts) {}
		UE_DEPRECATED(5.2, "Joint Solver API moved to FPBDJointContainerSolver")
		void ApplyVelocityConstraints(const TArrayView<int32>& ConstraintIndices, const FReal Dt, const int32 It, const int32 NumIts) {}
		UE_DEPRECATED(5.2, "Joint Solver API moved to FPBDJointContainerSolver")
		void ApplyProjectionConstraints(const TArrayView<int32>& ConstraintIndices, const FReal Dt, const int32 It, const int32 NumIts) {}
		UE_DEPRECATED(5.2, "Joint Solver API moved to FPBDJointContainerSolver")
		void GatherInput(const int32 ConstraintIndex, const FReal Dt) {}
		UE_DEPRECATED(5.2, "Joint Solver API moved to FPBDJointContainerSolver")
		void ScatterOutput(const int32 ConstraintIndex, const FReal Dt) {}

	private:
		friend class FPBDJointConstraintHandle;

		void GetConstrainedParticleIndices(const int32 ConstraintIndex, int32& Index0, int32& Index1) const;
		void CalculateConstraintSpace(int32 ConstraintIndex, FVec3& OutX0, FMatrix33& OutR0, FVec3& OutX1, FMatrix33& OutR1) const;
		
		void ColorConstraints();
		void SortConstraints();

		bool ShouldBeInGraph(const int32 ConstraintIndex) const;

		void ApplyPlasticityLimits(const int32 ConstraintIndex, const FSolverBody& SolverBody0, const FSolverBody& SolverBody1);

		void SetConstraintBroken(int32 ConstraintIndex, bool bBroken);
		void SetConstraintBreaking(int32 ConstraintIndex, bool bBreaking);

		FPBDJointSolverSettings Settings;

		TArray<FPBDJointSettings> ConstraintSettings;
		TArray<FParticlePair> ConstraintParticles;
		TArray<FPBDJointState> ConstraintStates;

		FHandles Handles;
		FConstraintHandleAllocator HandleAllocator;
		bool bJointsDirty;

		FJointBreakCallback BreakCallback;
	};

}
