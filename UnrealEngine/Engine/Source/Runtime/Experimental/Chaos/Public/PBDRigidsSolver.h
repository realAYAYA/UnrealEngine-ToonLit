// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Defines.h"
#include "Chaos/Framework/MultiBufferResource.h"
#include "Chaos/Framework/PhysicsProxy.h"
#include "Chaos/Framework/PhysicsSolverBase.h"
#include "Chaos/PBDRigidParticles.h"
#include "Chaos/PBDRigidsEvolutionGBF.h"
#include "Chaos/PBDCollisionConstraints.h"
#include "Chaos/PBDRigidDynamicSpringConstraints.h"
#include "Chaos/PBDPositionConstraints.h"
#include "Chaos/PBDSuspensionConstraints.h"
#include "Chaos/PBDJointConstraints.h"
#include "Chaos/PerParticleGravity.h"
#include "Chaos/ParticleHandle.h"
#include "Chaos/Transform.h"
#include "Chaos/Framework/PhysicsProxy.h"
#include "CoreMinimal.h"
#include "Containers/Queue.h"
#include "EventManager.h"
#include "Field/FieldSystem.h"
#include "PBDRigidActiveParticlesBuffer.h"
#include "PhysicsProxy/SingleParticlePhysicsProxyFwd.h"
#include "PhysicsProxy/JointConstraintProxy.h"
#include "PhysicsProxy/SuspensionConstraintProxy.h"
#include "SolverEventFilters.h"
#include "Chaos/PBDRigidsEvolutionFwd.h"
#include "ChaosSolversModule.h"

class FPhysInterface_Chaos;
struct FChaosSolverConfiguration;
class FSkeletalMeshPhysicsProxy;
class FStaticMeshPhysicsProxy;
class FPerSolverFieldSystem;

#define PBDRIGID_PREALLOC_COUNT 1024
#define KINEMATIC_GEOM_PREALLOC_COUNT 100
#define GEOMETRY_PREALLOC_COUNT 100

extern int32 ChaosSolverParticlePoolNumFrameUntilShrink;

namespace ChaosTest
{
	template <typename TSolver>
	void AdvanceSolverNoPushHelper(TSolver* Solver, Chaos::FReal Dt);
}

/**
*
*/
namespace Chaos
{
	class FPersistentPhysicsTask;
	class FChaosArchive;
	class FSingleParticleProxy;
	class FGeometryParticleBuffer;

	CHAOS_API extern int32 RewindCaptureNumFrames;

	template <typename T,typename R,int d>
	class ISpatialAccelerationCollection;

	class FAccelerationStructureHandle;

	enum class ELockType : uint8
	{
		Read,
		Write
	};

	template<ELockType LockType>
	struct TSolverSimMaterialScope
	{
		TSolverSimMaterialScope() = delete;
	};

	/**
	*
	*/
	class CHAOS_API FPBDRigidsSolver : public FPhysicsSolverBase
	{

		FPBDRigidsSolver(const EMultiBufferMode BufferingModeIn, UObject* InOwner, FReal AsyncDt);
		virtual ~FPBDRigidsSolver();

	public:

		typedef FPhysicsSolverBase Super;

		friend class FPersistentPhysicsTask;
		friend class ::FChaosSolversModule;

		template<EThreadingMode Mode>
		friend class FDispatcher;
		
		friend class FEventDefaults;

		friend class FPhysInterface_Chaos;
		friend class FPhysScene_ChaosInterface;
		friend class FPBDRigidDirtyParticlesBuffer;

		void* PhysSceneHack;	//This is a total hack for now to get at the owning scene

		typedef FPBDRigidsSOAs FParticlesType;
		typedef FPBDRigidDirtyParticlesBuffer FDirtyParticlesBuffer;

		typedef Chaos::FGeometryParticle FParticle;
		typedef Chaos::FGeometryParticleHandle FHandle;
		typedef Chaos::FPBDRigidsEvolutionGBF FPBDRigidsEvolution;

		typedef FPBDRigidDynamicSpringConstraints FRigidDynamicSpringConstraints;
		typedef FPBDPositionConstraints FPositionConstraints;

		using FJointConstraints = FPBDJointConstraints;
		//
		// Execution API
		//

		void ChangeBufferMode(Chaos::EMultiBufferMode InBufferMode);

		//
		//  Object API
		//

		void RegisterObject(FSingleParticlePhysicsProxy* Proxy);
		void UnregisterObject(FSingleParticlePhysicsProxy* Proxy);

		void RegisterObject(FGeometryCollectionPhysicsProxy* InProxy);
		void UnregisterObject(FGeometryCollectionPhysicsProxy* InProxy);

		void RegisterObject(Chaos::FJointConstraint* GTConstraint);
		void UnregisterObject(Chaos::FJointConstraint* GTConstraint);

		void RegisterObject(Chaos::FSuspensionConstraint* GTConstraint);
		void UnregisterObject(Chaos::FSuspensionConstraint* GTConstraint);

		//
		//  Simulation API
		//

		/**/
		FDirtyParticlesBuffer* GetDirtyParticlesBuffer() const { return MDirtyParticlesBuffer.Get(); }

		int32 NumJointConstraints() const;
		int32 NumCollisionConstraints() const;

		//Make friend with unit test code so we can verify some behavior
		template <typename TSolver>
		friend void ChaosTest::AdvanceSolverNoPushHelper(TSolver* Solver, FReal Dt);

		/**/
		void Reset();

		/**/
		void StartingSceneSimulation();

		/**/
		void CompleteSceneSimulation();

		/**/
		void UpdateGameThreadStructures();



		/**/
		void SetCurrentFrame(const int32 CurrentFrameIn) { CurrentFrame = CurrentFrameIn; }
		int32& GetCurrentFrame() { return CurrentFrame; }

		/**/
		void SetPositionIterations(const int32 InNumIterations) { GetEvolution()->SetNumPositionIterations(InNumIterations); }
		void SetVelocityIterations(const int32 InNumIterations) { GetEvolution()->SetNumVelocityIterations(InNumIterations); }
		void SetProjectionIterations(const int32 InNumIterations) { GetEvolution()->SetNumProjectionIterations(InNumIterations); }
		void SetCollisionCullDistance(const FReal InCullDistance) { GetEvolution()->GetCollisionDetector().SetBoundsExpansion(InCullDistance); }
		void SetCollisionMaxPushOutVelocity(const FReal InMaxPushOutVelocity) { GetEvolution()->GetCollisionConstraints().SetMaxPushOutVelocity(InMaxPushOutVelocity); }

		/**/
		void SetGenerateCollisionData(bool bDoGenerate) { GetEventFilters()->SetGenerateCollisionEvents(bDoGenerate); }
		void SetGenerateBreakingData(bool bDoGenerate)
		{
			GetEventFilters()->SetGenerateBreakingEvents(bDoGenerate);
			GetEvolution()->GetRigidClustering().SetGenerateClusterBreaking(bDoGenerate);
		}
		void SetGenerateTrailingData(bool bDoGenerate) { GetEventFilters()->SetGenerateTrailingEvents(bDoGenerate); }
		void SetGenerateRemovalData(bool bDoGenerate) { GetEventFilters()->SetGenerateRemovalEvents(bDoGenerate); }
		void SetCollisionFilterSettings(const FSolverCollisionFilterSettings& InCollisionFilterSettings) { GetEventFilters()->GetCollisionFilter()->UpdateFilterSettings(InCollisionFilterSettings); }
		void SetBreakingFilterSettings(const FSolverBreakingFilterSettings& InBreakingFilterSettings) { GetEventFilters()->GetBreakingFilter()->UpdateFilterSettings(InBreakingFilterSettings); }
		void SetTrailingFilterSettings(const FSolverTrailingFilterSettings& InTrailingFilterSettings) { GetEventFilters()->GetTrailingFilter()->UpdateFilterSettings(InTrailingFilterSettings); }
		void SetRemovalFilterSettings(const FSolverRemovalFilterSettings& InRemovalFilterSettings) { GetEventFilters()->GetRemovalFilter()->UpdateFilterSettings(InRemovalFilterSettings); }

		/**
		 * @brief True if the simulation is running in deterministic mode
		 * This will be true if determinism is explicitly requested (via SetIsDeterministic()) or if required
		 * by some otyher system like Rewind/Resim support.
		*/
		bool IsDetemerministic() const;

		/**
		 * @brief Request that the sim be determinitic (or not)
		 * @note Even if set to false, the sim may still be deterministic if some other feature is enabled and requires it.
		 * @see IsDetemerministic()
		*/
		void SetIsDeterministic(const bool bInIsDeterministic);

		/**/
		FJointConstraints& GetJointConstraints() { return MEvolution->GetJointConstraints(); }
		const FJointConstraints& GetJointConstraints() const { return MEvolution->GetJointConstraints(); }

		FPBDSuspensionConstraints& GetSuspensionConstraints() { return MEvolution->GetSuspensionConstraints(); }
		const FPBDSuspensionConstraints& GetSuspensionConstraints() const { return MEvolution->GetSuspensionConstraints(); }
		void SetSuspensionTarget(Chaos::FSuspensionConstraint* GTConstraint, const FVector& TargetPos, const FVector& Normal, bool Enabled);

		void EnableRewindCapture(int32 NumFrames, bool InUseCollisionResimCache, TUniquePtr<IRewindCallback>&& RewindCallback = TUniquePtr<IRewindCallback>());

		/**/
		FPBDRigidsEvolution* GetEvolution() { return MEvolution.Get(); }
		FPBDRigidsEvolution* GetEvolution() const { return MEvolution.Get(); }

		FParticlesType& GetParticles() { return Particles; }
		const FParticlesType& GetParticles() const { return Particles; }
		
		/**/
		FEventManager* GetEventManager() { return MEventManager.Get(); }

		/**/
		FSolverEventFilters* GetEventFilters() { return MSolverEventFilters.Get(); }
		FSolverEventFilters* GetEventFilters() const { return MSolverEventFilters.Get(); }

		/**/
		void SyncEvents_GameThread();

		/**/
		void PostTickDebugDraw(FReal Dt) const;

		// Visual debugger (VDB) push methods
		void PostEvolutionVDBPush() const;

		TArray<FGeometryCollectionPhysicsProxy*>& GetGeometryCollectionPhysicsProxies_Internal()
		{
			return GeometryCollectionPhysicsProxies_Internal;
		}

		const TArray<FJointConstraintPhysicsProxy*>& GetJointConstraintPhysicsProxies_Internal() const
		{
			return JointConstraintPhysicsProxies_Internal;
		}

		/** Events hooked up to the Chaos material manager */
		void UpdateMaterial(Chaos::FMaterialHandle InHandle, const Chaos::FChaosPhysicsMaterial& InNewData);
		void CreateMaterial(Chaos::FMaterialHandle InHandle, const Chaos::FChaosPhysicsMaterial& InNewData);
		void DestroyMaterial(Chaos::FMaterialHandle InHandle);
		void UpdateMaterialMask(Chaos::FMaterialMaskHandle InHandle, const Chaos::FChaosPhysicsMaterialMask& InNewData);
		void CreateMaterialMask(Chaos::FMaterialMaskHandle InHandle, const Chaos::FChaosPhysicsMaterialMask& InNewData);
		void DestroyMaterialMask(Chaos::FMaterialMaskHandle InHandle);

		/** Access to the internal material mirrors */
		const THandleArray<FChaosPhysicsMaterial>& GetQueryMaterials_External() const { return QueryMaterials_External; }
		const THandleArray<FChaosPhysicsMaterialMask>& GetQueryMaterialMasks_External() const { return QueryMaterialMasks_External; }
		const THandleArray<FChaosPhysicsMaterial>& GetSimMaterials() const { return SimMaterials; }
		const THandleArray<FChaosPhysicsMaterialMask>& GetSimMaterialMasks() const { return SimMaterialMasks; }

		/** Copy the simulation material list to the query material list, to be done when the SQ commits an update */
		void SyncQueryMaterials_External();

		void FinalizeRewindData(const TParticleView<FPBDRigidParticles>& DirtyParticles);
		bool RewindUsesCollisionResimCache() const { return bUseCollisionResimCache; }

		FPerSolverFieldSystem& GetPerSolverField() { return *PerSolverField; }
		const FPerSolverFieldSystem& GetPerSolverField() const { return *PerSolverField; }

		void UpdateExternalAccelerationStructure_External(ISpatialAccelerationCollection<FAccelerationStructureHandle,FReal,3>*& ExternalStructure);
		const ISpatialAccelerationCollection<FAccelerationStructureHandle, FReal, 3>* GetInternalAccelerationStructure_Internal() const
		{
			return MEvolution->GetSpatialAcceleration();
		}

		/** Apply a solver configuration to this solver, set externally by the owner of a solver (see UPhysicsSettings for world solver settings) */
		void ApplyConfig(const FChaosSolverConfiguration& InConfig);

		virtual bool AreAnyTasksPending() const override
		{
			if (IsPendingTasksComplete() == false || GetEvolution()->AreAnyTasksPending())
			{
				return true;
			}

			return false;
		}

		void BeginDestroy();

		/** Update the particles parameters based on field evaluation */
		void FieldParameterUpdateCallback(
			Chaos::FPBDPositionConstraints& PositionTarget,
			TMap<int32, int32>& TargetedParticles);

		/** Update the particles forces based on field evaluation */
		void FieldForcesUpdateCallback();

		// Update the counter in Stats and the CSV profiler
		void UpdateStatCounters() const;
		void UpdateExpensiveStatCounters() const;

		// Access particle proxy from physics thread useful for cross thread communication
		FSingleParticlePhysicsProxy* GetParticleProxy_PT(const FGeometryParticleHandle& Handle);
		const FSingleParticlePhysicsProxy* GetParticleProxy_PT(const FGeometryParticleHandle& Handle) const;

	private:

		/**/
		void BufferPhysicsResults();
	
		/**/
		virtual void PrepareAdvanceBy(const FReal DeltaTime) override;
		virtual void AdvanceSolverBy(const FSubStepInfo& SubStepInfo) override;
		virtual void PushPhysicsState(const FReal ExternalDt, const int32 NumSteps, const int32 NumExternalSteps) override;
		virtual void SetExternalTimestampConsumed_Internal(const int32 Timestamp) override;

		void UpdateIsDeterministic();

		//
		// Solver Data
		//
		int32 CurrentFrame;
		bool bHasFloor;
		bool bIsFloorAnalytic;
		FReal FloorHeight;
		bool bIsDeterministic;

		FParticleUniqueIndicesMultithreaded UniqueIndices;
		FParticlesType Particles;
		TUniquePtr<FPBDRigidsEvolution> MEvolution;
		TUniquePtr<FEventManager> MEventManager;
		TUniquePtr<FSolverEventFilters> MSolverEventFilters;
		TUniquePtr<FDirtyParticlesBuffer> MDirtyParticlesBuffer;

		//
		// Proxies
		//
		TSharedPtr<FCriticalSection> MCurrentLock;
		TSparseArray< FSingleParticlePhysicsProxy* > SingleParticlePhysicsProxies_PT;
		TArray< FGeometryCollectionPhysicsProxy* > GeometryCollectionPhysicsProxies_Internal; // PT
		TArray< FJointConstraintPhysicsProxy* > JointConstraintPhysicsProxies_Internal; // PT

		TUniquePtr<FPerSolverFieldSystem> PerSolverField;


		// Physics material mirrors for the solver. These should generally stay in sync with the global material list from
		// the game thread. This data is read only in the solver as we should never need to update it here. External threads can
		// Enqueue commands to change parameters.
		//
		// There are two copies here to enable SQ to lock only the solvers that it needs to handle the material access during a query
		// instead of having to lock the entire physics state of the runtime.
		
		THandleArray<FChaosPhysicsMaterial> QueryMaterials_External;
		THandleArray<FChaosPhysicsMaterialMask> QueryMaterialMasks_External;
		THandleArray<FChaosPhysicsMaterial> SimMaterials;
		THandleArray<FChaosPhysicsMaterialMask> SimMaterialMasks;

		struct FPendingDestroyInfo
		{
			FSingleParticlePhysicsProxy* Proxy;
			int32 DestroyOnStep;
			FGeometryParticleHandle* Handle;
			FUniqueIdx UniqueIdx;
		};

		TArray<FPendingDestroyInfo> PendingDestroyPhysicsProxy;
		TArray<FGeometryCollectionPhysicsProxy*> PendingDestroyGeometryCollectionPhysicsProxy;


		void ProcessSinglePushedData_Internal(FPushPhysicsData& PushData);
		virtual void ProcessPushedData_Internal(FPushPhysicsData& PushData) override;
		void DestroyPendingProxies_Internal();

		virtual void ConditionalApplyRewind_Internal() override;
	};

	template<>
	struct TSolverSimMaterialScope<ELockType::Read>
	{
		TSolverSimMaterialScope() = delete;


		explicit TSolverSimMaterialScope(FPhysicsSolverBase* InSolver)
			: Solver(InSolver)
		{
			check(Solver);
			Solver->SimMaterialLock.ReadLock();
		}

		~TSolverSimMaterialScope()
		{
			Solver->SimMaterialLock.ReadUnlock();
		}

	private:
		FPhysicsSolverBase* Solver;
	};

	template<>
	struct TSolverSimMaterialScope<ELockType::Write>
	{
		TSolverSimMaterialScope() = delete;

		explicit TSolverSimMaterialScope(FPhysicsSolverBase* InSolver)
			: Solver(InSolver)
		{
			check(Solver);
			Solver->SimMaterialLock.WriteLock();
		}

		~TSolverSimMaterialScope()
		{
			Solver->SimMaterialLock.WriteUnlock();
		}

	private:
		FPhysicsSolverBase* Solver;
	};

}; // namespace Chaos
