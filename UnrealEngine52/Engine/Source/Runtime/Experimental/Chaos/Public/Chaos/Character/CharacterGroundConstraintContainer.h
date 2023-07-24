// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Core.h"

#include "Chaos/Character/CharacterGroundConstraintSettings.h"
#include "Chaos/ConstraintHandle.h"
#include "Chaos/Evolution/IndexedConstraintContainer.h"
#include "Chaos/GeometryParticles.h"
#include "Chaos/PBDConstraintContainer.h"

namespace ChaosTest
{
	class CharacterGroundConstraintContainerTest;
}

namespace Chaos
{
	class FCharacterGroundConstraintContainer;
	namespace Private
	{
		class FCharacterGroundConstraintContainerSolver;
	}

	/// Physics thread side representation of a character ground constraint
	/// Data accessed through this class is synced from the game thread
	/// representation FCharacterGroundConstraint
	class CHAOS_API FCharacterGroundConstraintHandle final : public TIntrusiveConstraintHandle<FCharacterGroundConstraintHandle>
	{
	public:
		using Base = TIntrusiveConstraintHandle<FCharacterGroundConstraintHandle>;
		using FConstraintContainer = FCharacterGroundConstraintContainer;

		FCharacterGroundConstraintHandle()
			: TIntrusiveConstraintHandle<FCharacterGroundConstraintHandle>()
		{
		}

		static const FConstraintHandleTypeID& StaticType()
		{
			static FConstraintHandleTypeID STypeID(TEXT("FCharacterGroundConstraintHandle"), &FIndexedConstraintHandle::StaticType());
			return STypeID;
		}

		ESyncState GetSyncState() const { return SyncState; }
		void SetSyncState(ESyncState InSyncState) { SyncState = InSyncState; }

		EResimType GetResimType() const { return ResimType; }
		bool GetEnabledDuringResim() const { return bEnabledDuringResim; }
		void SetEnabledDuringResim(bool bEnabled) { bEnabledDuringResim = bEnabled; }

		virtual void SetEnabled(bool InEnabled) override { bDisabled = !InEnabled; }
		virtual bool IsEnabled() const { return !bDisabled; }

		/// Currently settings and data are only modifiable on the game thread
		/// via FCharacterGroundConstraint
		const FCharacterGroundConstraintSettings& GetSettings() const { return Settings; }
		const FCharacterGroundConstraintDynamicData& GetData() const { return Data; }

		// Declared final so that TPBDConstraintGraphRuleImpl::AddToGraph() does not need to hit vtable
		virtual FParticlePair GetConstrainedParticles() const override final { return { CharacterParticle, GroundParticle }; }
		FGeometryParticleHandle* GetCharacterParticle() const { return CharacterParticle; }
		FGeometryParticleHandle* GetGroundParticle() const { return GroundParticle; }

		// Get the force applied by the solver due to this constraint. Units are ML/T^2
		FVec3 GetSolverAppliedForce() const { return SolverAppliedForce; }

		// Get the torque applied by the solver due to this constraint. Units are ML^2/T^2
		FVec3 GetSolverAppliedTorque() const { return SolverAppliedTorque; }

	private:
		friend class FCharacterGroundConstraintContainer; // For creation
		friend class FCharacterGroundConstraintProxy; // For setting the data from the game thread constraint
		friend class Private::FCharacterGroundConstraintContainerSolver; // For setting the solver force and torque
		friend class ChaosTest::CharacterGroundConstraintContainerTest; // For testing internals

		FCharacterGroundConstraintSettings Settings;
		FCharacterGroundConstraintDynamicData Data;
		FVec3 SolverAppliedForce = FVec3::ZeroVector;
		FVec3 SolverAppliedTorque = FVec3::ZeroVector;
		FGeometryParticleHandle* CharacterParticle;
		FGeometryParticleHandle* GroundParticle;
		bool bDisabled = false;
		bool bEnabledDuringResim;
		EResimType ResimType = EResimType::FullResim;
		ESyncState SyncState = ESyncState::InSync;
	};

	/// Container class for all character ground constraints on the physics thread
	class CHAOS_API FCharacterGroundConstraintContainer : public FPBDConstraintContainer
	{
	public:
		using Base = FPBDConstraintContainer;
		using FConstraints = TArrayView<FCharacterGroundConstraintHandle* const>;
		using FConstConstraints = TArrayView<const FCharacterGroundConstraintHandle* const>;

		FCharacterGroundConstraintContainer();
		virtual ~FCharacterGroundConstraintContainer();

		int32 NumConstraints() const { return Constraints.Num(); }

		FCharacterGroundConstraintHandle* AddConstraint(
			const FCharacterGroundConstraintSettings& InConstraintSettings,
			const FCharacterGroundConstraintDynamicData& InConstraintData,
			FGeometryParticleHandle* CharacterParticle,
			FGeometryParticleHandle* GroundParticle = nullptr);

		void RemoveConstraint(FCharacterGroundConstraintHandle* Constraint);

		FConstraints GetConstraints() { return MakeArrayView(Constraints); }
		FConstConstraints GetConstConstraints() const { return MakeArrayView(Constraints); }

		FCharacterGroundConstraintHandle* GetConstraint(int32 ConstraintIndex) { check(ConstraintIndex < NumConstraints()); return Constraints[ConstraintIndex]; }
		const FCharacterGroundConstraintHandle* GetConstraint(int32 ConstraintIndex) const { check(ConstraintIndex < NumConstraints()); return Constraints[ConstraintIndex]; }

		//////////////////////////////////////////////////////////////////////////
		// FConstraintContainer Implementation
		virtual TUniquePtr<FConstraintContainerSolver> CreateSceneSolver(const int32 Priority) override final;
		virtual TUniquePtr<FConstraintContainerSolver> CreateGroupSolver(const int32 Priority) override final;
		virtual int32 GetNumConstraints() const override final { return Constraints.Num(); }
		virtual void ResetConstraints() override final {}
		virtual void AddConstraintsToGraph(Private::FPBDIslandManager& IslandManager) override final;
		virtual void PrepareTick() override final;
		virtual void UnprepareTick() override final;
		virtual void DisconnectConstraints(const TSet<TGeometryParticleHandle<FReal, 3>*>& RemovedParticles) override final;

		//////////////////////////////////////////////////////////////////////////
		// Required API from FConstraintContainer
		void SetConstraintEnabled(int32 ConstraintIndex, bool bEnabled);
		bool IsConstraintEnabled(int32 ConstraintIndex) const { check(ConstraintIndex < NumConstraints()); return Constraints[ConstraintIndex]->IsEnabled(); }

	private:
		bool CanEvaluate(const FCharacterGroundConstraintHandle* Constraint) const;

		TObjectPool<FCharacterGroundConstraintHandle> ConstraintPool;
		TArray<FCharacterGroundConstraintHandle*> Constraints;
	};
}