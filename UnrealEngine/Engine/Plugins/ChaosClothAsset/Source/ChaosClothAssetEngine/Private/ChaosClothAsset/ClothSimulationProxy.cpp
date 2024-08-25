// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/ClothSimulationProxy.h"
#include "ChaosClothAsset/ClothAsset.h"
#include "ChaosClothAsset/ClothAssetPrivate.h"
#include "ChaosClothAsset/ClothComponent.h"
#include "ChaosClothAsset/ClothSimulationContext.h"
#include "ChaosClothAsset/ClothSimulationMesh.h"
#include "ChaosClothAsset/ClothSimulationModel.h"
#include "ChaosCloth/ChaosClothingSimulationCloth.h"
#include "ChaosCloth/ChaosClothingSimulationCollider.h"
#include "ChaosCloth/ChaosClothingSimulationConfig.h"
#include "ChaosCloth/ChaosClothingSimulationSolver.h"
#include "ChaosCloth/ChaosClothVisualization.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "ClothingSimulation.h"

#if INTEL_ISPC
#include "ClothSimulationProxy.ispc.generated.h"
#endif

DECLARE_CYCLE_STAT(TEXT("ClothSimulationProxy Tick Game"), STAT_ClothSimulationProxy_TickGame, STATGROUP_ChaosClothAsset);
DECLARE_CYCLE_STAT(TEXT("ClothSimulationProxy Tick Physics"), STAT_ClothSimulationProxy_TickPhysics, STATGROUP_ChaosClothAsset);
DECLARE_CYCLE_STAT(TEXT("ClothSimulationProxy Write Simulation Data"), STAT_ClothSimulationProxy_WriteSimulationData, STATGROUP_ChaosClothAsset);
DECLARE_CYCLE_STAT(TEXT("ClothSimulationProxy Calculate Bounds"), STAT_ClothSimulationProxy_CalculateBounds, STATGROUP_ChaosClothAsset);
DECLARE_CYCLE_STAT(TEXT("ClothSimulationProxy End Parallel Cloth Task"), STAT_ClothSimulationProxy_EndParallelClothTask, STATGROUP_ChaosClothAsset);

CSV_DECLARE_CATEGORY_MODULE_EXTERN(ENGINE_API, Animation);

namespace UE::Chaos::ClothAsset
{
#if INTEL_ISPC && !UE_BUILD_SHIPPING
	static_assert(sizeof(ispc::FVector3f) == sizeof(FVector3f), "sizeof(ispc::FVector3f) != sizeof(FVector3f)");
	static_assert(sizeof(ispc::FTransform) == sizeof(::Chaos::FRigidTransform3), "sizeof(ispc::FTransform) != sizeof(::Chaos::FRigidTransform3)");

	bool bTransformClothSimulData_ISPC_Enabled = true;
	FAutoConsoleVariableRef CVarTransformClothSimukDataISPCEnabled(TEXT("p.ChaosClothAsset.TransformClothSimulData.ISPC"), bTransformClothSimulData_ISPC_Enabled, TEXT("Whether to use ISPC optimizations when transforming simulation data back to reference bone space."));
#endif

	float DeltaTimeDecay = 0.03;
	FAutoConsoleVariableRef CVarDeltaTimeDecay(TEXT("p.ChaosClothAsset.DeltaTimeDecay"), DeltaTimeDecay, TEXT("Delta Time smoothing decay (1 = no smoothing)"));

	static FAutoConsoleTaskPriority CPrio_ClothSimulationProxyParallelTask(
		TEXT("TaskGraph.TaskPriorities.ClothSimulationProxyParallelTask"),
		TEXT("Task and thread priority for the cloth simulation proxy."),
		ENamedThreads::HighThreadPriority, // If we have high priority task threads, then use them...
		ENamedThreads::NormalTaskPriority, // .. at normal task priority
		ENamedThreads::HighTaskPriority);  // If we don't have high priority threads, then use normal priority threads at high task priority instead

	class FClothSimulationProxyParallelTask
	{
	public:
		FClothSimulationProxyParallelTask(FClothSimulationProxy& InClothSimulationProxy)
			: ClothSimulationProxy(InClothSimulationProxy)
		{
		}

		TStatId GetStatId() const
		{
			RETURN_QUICK_DECLARE_CYCLE_STAT(FClothSimulationProxyParallelTask, STATGROUP_TaskGraphTasks);
		}

		static ENamedThreads::Type GetDesiredThread()
		{
			static IConsoleVariable* const CVarClothPhysicsUseTaskThread = IConsoleManager::Get().FindConsoleVariable(TEXT("p.ClothPhysics.UseTaskThread"));

			if (CVarClothPhysicsUseTaskThread && CVarClothPhysicsUseTaskThread->GetBool())
			{
				return CPrio_ClothSimulationProxyParallelTask.Get();
			}
			return ENamedThreads::GameThread;
		}

		static ESubsequentsMode::Type GetSubsequentsMode()
		{
			return ESubsequentsMode::TrackSubsequents;
		}

		void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
		{
			FScopeCycleCounterUObject ContextScope(ClothSimulationProxy.ClothComponent.GetSkinnedAsset());
			CSV_SCOPED_TIMING_STAT(Animation, Cloth);

			ClothSimulationProxy.Tick();
		}

	private:
		FClothSimulationProxy& ClothSimulationProxy;
	};

	FClothSimulationProxy::FClothSimulationProxy(const UChaosClothComponent& InClothComponent)
		: ClothComponent(InClothComponent)
		, ClothSimulationContext(MakeUnique<FClothSimulationContext>())
		, Solver(nullptr)
		, Visualization(nullptr)
		, MaxDeltaTime(UPhysicsSettings::Get()->MaxPhysicsDeltaTime)
	{
		using namespace ::Chaos;

		check(IsInGameThread());

		// Create solver config simulation thread object first. Need to know which solver type we're creating.
		const int32 SolverConfigIndex = Configs.Emplace(MakeUnique<FClothingSimulationConfig>(ClothComponent.GetPropertyCollections()));  // TODO: Use a separate solver config for outfits

		// Use new SoftsEvolution, not PBDEvolution.
		constexpr bool bUseLegacySolver = false;
		Solver = MakeUnique<::Chaos::FClothingSimulationSolver>(Configs[SolverConfigIndex].Get(), bUseLegacySolver);
		Visualization = MakeUnique<::Chaos::FClothVisualization>(Solver.Get());

		// Need a valid context to initialize the mesh
		constexpr bool bIsInitialization = true;
		constexpr Softs::FSolverReal NoAdvanceDt = 0.f;
		ClothSimulationContext->Fill(ClothComponent, NoAdvanceDt, MaxDeltaTime, bIsInitialization);

		// Setup startup transforms
		constexpr bool bNeedsReset = true;
		Solver->SetLocalSpaceLocation((FVec3)ClothSimulationContext->ComponentTransform.GetLocation(), bNeedsReset);
		Solver->SetLocalSpaceRotation((FQuat)ClothSimulationContext->ComponentTransform.GetRotation());

		// Create mesh simulation thread object
		const UChaosClothAsset* const ClothAsset = ClothComponent.GetClothAsset();
		if (!ClothAsset)
		{
			return;
		}
		ClothSimulationModel = ClothAsset->GetClothSimulationModel();
		check(ClothSimulationModel.IsValid());

		FString DebugName;
#if !UE_BUILD_SHIPPING
		DebugName = ClothComponent.GetOwner() ?
			FString::Format(TEXT("{0}|{1}"), { ClothComponent.GetOwner()->GetName(), ClothComponent.GetName() }) :
			ClothComponent.GetName();
#endif
		const int32 MeshIndex = Meshes.Emplace(MakeUnique<FClothSimulationMesh>(*ClothSimulationModel, *ClothSimulationContext, DebugName));

		// Create collider simulation thread object
PRAGMA_DISABLE_DEPRECATION_WARNINGS  // TODO: CHAOS_IS_CLOTHINGSIMULATIONMESH_ABSTRACT
		const FReferenceSkeleton* const ReferenceSkeleton = &ClothAsset->GetRefSkeleton();
		const int32 ColliderIndex = ClothComponent.GetPhysicsAsset() ? Colliders.Emplace(MakeUnique<FClothingSimulationCollider>(ClothComponent.GetPhysicsAsset(), ReferenceSkeleton)) : INDEX_NONE;
PRAGMA_ENABLE_DEPRECATION_WARNINGS

		//Colliders[ColliderIndex]->SetCollisionData(&ExternalCollisionData);  // TODO: External collision data

		// Create cloth config simulation thread object
		const int32 ClothConfigIndex = Configs.Emplace(MakeUnique<FClothingSimulationConfig>(ClothComponent.GetPropertyCollections()));

		// Create cloth simulation thread object
		constexpr uint32 GroupId = 0;

		const int32 ClothIndex = Cloths.Emplace(MakeUnique<FClothingSimulationCloth>(
			Configs[ClothConfigIndex].Get(),
PRAGMA_DISABLE_DEPRECATION_WARNINGS  // TODO: CHAOS_IS_CLOTHINGSIMULATIONMESH_ABSTRACT
			Meshes[MeshIndex].Get(),
PRAGMA_ENABLE_DEPRECATION_WARNINGS
			ColliderIndex != INDEX_NONE ? TArray<FClothingSimulationCollider*>({ Colliders[ColliderIndex].Get() }) : TArray<FClothingSimulationCollider*>(),
			GroupId));

		Solver->AddCloth(Cloths[ClothIndex].Get());
		Cloths[ClothIndex]->Reset();

		// Update cloth stats
		NumCloths = 1;
		NumKinematicParticles = Cloths[ClothIndex]->GetNumActiveKinematicParticles();
		NumDynamicParticles = Cloths[ClothIndex]->GetNumActiveDynamicParticles();

		// Set start pose (update the context, then the solver without advancing the simulation)
		ClothSimulationContext->Fill(ClothComponent, NoAdvanceDt, MaxDeltaTime);
		Solver->Update((Softs::FSolverReal)ClothSimulationContext->DeltaTime);
	}

	FClothSimulationProxy::~FClothSimulationProxy()
	{
		CompleteParallelSimulation_GameThread();
	}

	bool FClothSimulationProxy::Tick_GameThread(float DeltaTime)
	{
		SCOPE_CYCLE_COUNTER(STAT_ClothSimulationProxy_TickGame);

		// Fill a new context, note the context is also needed when the simulation is suspended
		constexpr bool bIsInitializationFalse = false;
		FillSimulationContext(DeltaTime, bIsInitializationFalse);
		Solver->SetEnableSolver(ShouldEnableSolver(Solver->GetEnableSolver()));

		const bool bUseCache = ClothSimulationContext->CacheData.HasData();
		const bool bCreateParallelTask = (DeltaTime > 0.f && !ClothComponent.IsSimulationSuspended()) || bUseCache;
		if (bCreateParallelTask)
		{
			InitializeConfigs();

			// Start the the cloth simulation thread
			ParallelTask = TGraphTask<FClothSimulationProxyParallelTask>::CreateTask(nullptr, ENamedThreads::GameThread).ConstructAndDispatchWhenReady(*this);

			return true;  // Simulating
		}

		// It still needs to write back to the GT cache as the context has changed
		WriteSimulationData();

		return false;  // Not simulating
	}

	void FClothSimulationProxy::Tick()
	{
		using namespace ::Chaos;

		TRACE_CPUPROFILER_EVENT_SCOPE(FClothSimulationProxy_TickPhysics);
		SCOPE_CYCLE_COUNTER(STAT_ClothSimulationProxy_TickPhysics);
		const bool bUseCache = ClothSimulationContext->CacheData.HasData();

		if (ClothSimulationContext->DeltaTime == 0.f && !bUseCache)
		{
			return;
		}
		// Filter delta time to smoothen time variations and prevent unwanted vibrations
		const Softs::FSolverReal DeltaTime = (Softs::FSolverReal)ClothSimulationContext->DeltaTime;
		const Softs::FSolverReal PrevDeltaTime = Solver->GetDeltaTime() > 0.f ? Solver->GetDeltaTime() : DeltaTime;
		const Softs::FSolverReal SmoothedDeltaTime = PrevDeltaTime + (DeltaTime - PrevDeltaTime) * (Softs::FSolverReal)DeltaTimeDecay;

		const double StartTime = FPlatformTime::Seconds();
		const float PrevSimulationTime = SimulationTime;  // Copy the atomic to prevent a re-read

		const bool bNeedsReset = (ClothSimulationContext->bReset || PrevSimulationTime == 0.f);  // Reset on the first frame too since the simulation is created in bind pose, and not in start pose
		const bool bNeedsTeleport = ClothSimulationContext->bTeleport;
		bIsTeleported = bNeedsTeleport;

		// Update Solver animatable parameters
		Solver->SetLocalSpaceLocation((FVec3)ClothSimulationContext->ComponentTransform.GetLocation(), bNeedsReset);
		Solver->SetLocalSpaceRotation((FQuat)ClothSimulationContext->ComponentTransform.GetRotation());
		Solver->SetWindVelocity(ClothSimulationContext->WindVelocity);
		Solver->SetGravity(ClothSimulationContext->WorldGravity);
		Solver->EnableClothGravityOverride(true);
		Solver->SetVelocityScale(!bNeedsReset ? (FReal)ClothSimulationContext->VelocityScale * (FReal)SmoothedDeltaTime / DeltaTime : 1.f);

		// Check teleport modes
		for (const TUniquePtr<FClothingSimulationCloth>& Cloth : Cloths)
		{
			// Update Cloth animatable parameters while in the cloth loop
			if (bNeedsReset)
			{
				Cloth->Reset();
			}
			if (bNeedsTeleport)
			{
				Cloth->Teleport();
			}
		}

		// Step the simulation
		if (Solver->GetEnableSolver() || !bUseCache)
		{
			Solver->Update(SmoothedDeltaTime);
		}
		else
		{
			Solver->UpdateFromCache(ClothSimulationContext->CacheData);
		}

		// Keep the actual used number of iterations for the stats
		NumIterations = Solver->GetNumUsedIterations();
		NumSubsteps = Solver->GetNumUsedSubsteps();

		// Update simulation time in ms (and provide an instant average instead of the value in real-time)
		const float CurrSimulationTime = (float)((FPlatformTime::Seconds() - StartTime) * 1000.);
		static const float SimulationTimeDecay = 0.03f; // 0.03 seems to provide a good rate of update for the instant average
		SimulationTime = PrevSimulationTime ? PrevSimulationTime + (CurrSimulationTime - PrevSimulationTime) * SimulationTimeDecay : CurrSimulationTime;

		// Update particle counts (could have changed if lod changed)
		NumKinematicParticles = 0;
		NumDynamicParticles = 0;
		int32 FirstActiveClothParticleRangeId = INDEX_NONE;
		for (const TUniquePtr<FClothingSimulationCloth>& Cloth : Cloths)
		{
			NumKinematicParticles += Cloth->GetNumActiveKinematicParticles();
			NumDynamicParticles += Cloth->GetNumActiveDynamicParticles();
			if (FirstActiveClothParticleRangeId == INDEX_NONE && Cloth->GetNumActiveDynamicParticles() > 0)
			{
				FirstActiveClothParticleRangeId = Cloth->GetParticleRangeId(Solver.Get ());
			}
		}
		if (FirstActiveClothParticleRangeId != INDEX_NONE)
		{
			LastLinearSolveError = Solver->GetLinearSolverError(FirstActiveClothParticleRangeId);
			LastLinearSolveIterations = Solver->GetNumLinearSolverIterations(FirstActiveClothParticleRangeId);
		}
		else
		{
			LastLinearSolveError = 0.f;
			LastLinearSolveIterations = 0;
		}

		// Visualization
#if CHAOS_DEBUG_DRAW
		static const TConsoleVariableData<bool>* const DebugDrawLocalSpaceCVar = IConsoleManager::Get().FindTConsoleVariableDataBool(TEXT("p.ChaosCloth.DebugDrawLocalSpace"));
		static const TConsoleVariableData<bool>* const DebugDrawBoundsCVar = IConsoleManager::Get().FindTConsoleVariableDataBool(TEXT("p.ChaosCloth.DebugDrawBounds"));
		static const TConsoleVariableData<bool>* const DebugDrawGravityCVar = IConsoleManager::Get().FindTConsoleVariableDataBool(TEXT("p.ChaosCloth.DebugDrawGravity"));
		static const TConsoleVariableData<bool>* const DebugDrawPhysMeshWiredCVar = IConsoleManager::Get().FindTConsoleVariableDataBool(TEXT("p.ChaosCloth.DebugDrawPhysMeshWired"));
		static const TConsoleVariableData<bool>* const DebugDrawAnimMeshWiredCVar = IConsoleManager::Get().FindTConsoleVariableDataBool(TEXT("p.ChaosCloth.DebugDrawAnimMeshWired"));
		static const TConsoleVariableData<bool>* const DebugDrawPointVelocitiesCVar = IConsoleManager::Get().FindTConsoleVariableDataBool(TEXT("p.ChaosCloth.DebugDrawPointVelocities"));
		static const TConsoleVariableData<bool>* const DebugDrawAnimNormalsCVar = IConsoleManager::Get().FindTConsoleVariableDataBool(TEXT("p.ChaosCloth.DebugDrawAnimNormals"));
		static const TConsoleVariableData<bool>* const DebugDrawPointNormalsCVar = IConsoleManager::Get().FindTConsoleVariableDataBool(TEXT("p.ChaosCloth.DebugDrawPointNormals"));
		static const TConsoleVariableData<bool>* const DebugDrawCollisionCVar = IConsoleManager::Get().FindTConsoleVariableDataBool(TEXT("p.ChaosCloth.DebugDrawCollision"));
		static const TConsoleVariableData<bool>* const DebugDrawBackstopsCVar = IConsoleManager::Get().FindTConsoleVariableDataBool(TEXT("p.ChaosCloth.DebugDrawBackstops"));
		static const TConsoleVariableData<bool>* const DebugDrawBackstopDistancesCVar = IConsoleManager::Get().FindTConsoleVariableDataBool(TEXT("p.ChaosCloth.DebugDrawBackstopDistances"));
		static const TConsoleVariableData<bool>* const DebugDrawMaxDistancesCVar = IConsoleManager::Get().FindTConsoleVariableDataBool(TEXT("p.ChaosCloth.DebugDrawMaxDistances"));
		static const TConsoleVariableData<bool>* const DebugDrawAnimDriveCVar = IConsoleManager::Get().FindTConsoleVariableDataBool(TEXT("p.ChaosCloth.DebugDrawAnimDrive"));
		static const TConsoleVariableData<bool>* const DebugDrawEdgeConstraintCVar = IConsoleManager::Get().FindTConsoleVariableDataBool(TEXT("p.ChaosCloth.DebugDrawEdgeConstraint"));
		static const TConsoleVariableData<bool>* const DebugDrawBendingConstraintCVar = IConsoleManager::Get().FindTConsoleVariableDataBool(TEXT("p.ChaosCloth.DebugDrawBendingConstraint"));
		static const TConsoleVariableData<bool>* const DebugDrawLongRangeConstraintCVar = IConsoleManager::Get().FindTConsoleVariableDataBool(TEXT("p.ChaosCloth.DebugDrawLongRangeConstraint"));
		static const TConsoleVariableData<bool>* const DebugDrawWindForcesCVar = IConsoleManager::Get().FindTConsoleVariableDataBool(TEXT("p.ChaosCloth.DebugDrawWindForces"));
		static const TConsoleVariableData<bool>* const DebugDrawSelfCollisionCVar = IConsoleManager::Get().FindTConsoleVariableDataBool(TEXT("p.ChaosCloth.DebugDrawSelfCollision"));
		static const TConsoleVariableData<bool>* const DebugDrawSelfIntersectionCVar = IConsoleManager::Get().FindTConsoleVariableDataBool(TEXT("p.ChaosCloth.DebugDrawSelfIntersection"));

		if (DebugDrawLocalSpaceCVar && DebugDrawLocalSpaceCVar->GetValueOnAnyThread()) { Visualization->DrawLocalSpace(); }
		if (DebugDrawBoundsCVar && DebugDrawBoundsCVar->GetValueOnAnyThread()) { Visualization->DrawBounds(); }
		if (DebugDrawGravityCVar && DebugDrawGravityCVar->GetValueOnAnyThread()) { Visualization->DrawGravity(); }
		if (DebugDrawPhysMeshWiredCVar && DebugDrawPhysMeshWiredCVar->GetValueOnAnyThread()) { Visualization->DrawPhysMeshWired(); }
		if (DebugDrawAnimMeshWiredCVar && DebugDrawAnimMeshWiredCVar->GetValueOnAnyThread()) { Visualization->DrawAnimMeshWired(); }
		if (DebugDrawPointVelocitiesCVar && DebugDrawPointVelocitiesCVar->GetValueOnAnyThread()) { Visualization->DrawPointVelocities(); }
		if (DebugDrawAnimNormalsCVar && DebugDrawAnimNormalsCVar->GetValueOnAnyThread()) { Visualization->DrawAnimNormals(); }
		if (DebugDrawPointNormalsCVar && DebugDrawPointNormalsCVar->GetValueOnAnyThread()) { Visualization->DrawPointNormals(); }
		if (DebugDrawCollisionCVar && DebugDrawCollisionCVar->GetValueOnAnyThread()) { Visualization->DrawCollision(); }
		if (DebugDrawBackstopsCVar && DebugDrawBackstopsCVar->GetValueOnAnyThread()) { Visualization->DrawBackstops(); }
		if (DebugDrawBackstopDistancesCVar && DebugDrawBackstopDistancesCVar->GetValueOnAnyThread()) { Visualization->DrawMaxDistances(); }
		if (DebugDrawMaxDistancesCVar && DebugDrawMaxDistancesCVar->GetValueOnAnyThread()) { Visualization->DrawMaxDistances(); }
		if (DebugDrawAnimDriveCVar && DebugDrawAnimDriveCVar->GetValueOnAnyThread()) { Visualization->DrawAnimDrive(); }
		if (DebugDrawEdgeConstraintCVar && DebugDrawEdgeConstraintCVar->GetValueOnAnyThread()) { Visualization->DrawEdgeConstraint(); }
		if (DebugDrawBendingConstraintCVar && DebugDrawBendingConstraintCVar->GetValueOnAnyThread()) { Visualization->DrawBendingConstraint(); }
		if (DebugDrawLongRangeConstraintCVar && DebugDrawLongRangeConstraintCVar->GetValueOnAnyThread()) { Visualization->DrawLongRangeConstraint(); }
		if (DebugDrawWindForcesCVar && DebugDrawWindForcesCVar->GetValueOnAnyThread()) { Visualization->DrawWindAndPressureForces(); }
		if (DebugDrawSelfCollisionCVar && DebugDrawSelfCollisionCVar->GetValueOnAnyThread()) { Visualization->DrawSelfCollision(); }
		if (DebugDrawSelfIntersectionCVar && DebugDrawSelfIntersectionCVar->GetValueOnAnyThread()) { Visualization->DrawSelfIntersection(); }
#endif  // #if CHAOS_DEBUG_DRAW
	}

	void FClothSimulationProxy::CompleteParallelSimulation_GameThread()
	{
		check(IsInGameThread());

		if (IsValidRef(ParallelTask))
		{
			SCOPE_CYCLE_COUNTER(STAT_ClothSimulationProxy_EndParallelClothTask);
			CSV_SCOPED_SET_WAIT_STAT(Cloth);

			// There's a simulation in flight
			FTaskGraphInterface::Get().WaitUntilTaskCompletes(ParallelTask, ENamedThreads::GameThread);

			// No longer need this task, it has completed
			ParallelTask.SafeRelease();

			// Write back to the GT cache
			WriteSimulationData();
		}
	}


	void FClothSimulationProxy::WriteSimulationData()
	{
		using namespace ::Chaos;

		CSV_SCOPED_TIMING_STAT(Animation, Cloth);
		TRACE_CPUPROFILER_EVENT_SCOPE(FClothSimulationProxy_WriteSimulationData);
		SCOPE_CYCLE_COUNTER(STAT_ClothSimulationProxy_WriteSimulationData);

		USkinnedMeshComponent* LeaderPoseComponent = nullptr;
		if (ClothComponent.LeaderPoseComponent.IsValid())
		{
			LeaderPoseComponent = ClothComponent.LeaderPoseComponent.Get();

			// Check if our bone map is actually valid, if not there is no clothing data to build
			if (!ClothComponent.GetLeaderBoneMap().Num())
			{
				CurrentSimulationData.Reset();
				return;
			}
		}

		if (!Cloths.Num())
		{
			CurrentSimulationData.Reset();
			return;
		}

		// Reset map when new cloths have appeared
		if (CurrentSimulationData.Num() != Cloths.Num())
		{
			CurrentSimulationData.Reset();
		}

		// Get the solver's local space
		const FVec3& LocalSpaceLocation = Solver->GetLocalSpaceLocation(); // Note: Since the ReferenceSpaceTransform can be suspended with the simulation, it is important that the suspended local space location is used too in order to get the simulation data back into reference space

		// Retrieve the component's bones transforms
		const TArray<FTransform>& ComponentSpaceTransforms = LeaderPoseComponent ? LeaderPoseComponent->GetComponentSpaceTransforms() : ClothComponent.GetComponentSpaceTransforms();

		// Set the simulation data for each of the cloths
		for (const TUniquePtr<FClothingSimulationCloth>& Cloth : Cloths)
		{
			const int32 AssetIndex = Cloth->GetGroupId();

			if (!Cloth->GetMesh())
			{
				CurrentSimulationData.Remove(AssetIndex);  // Ensures that the cloth vertex factory won't run unnecessarily
				continue;  // Invalid or empty cloth
			}

			// If the LOD has changed while the simulation is suspended, the cloth still needs to be updated with the correct LOD data
PRAGMA_DISABLE_DEPRECATION_WARNINGS  // TODO: CHAOS_IS_CLOTHINGSIMULATIONMESH_ABSTRACT
			const int32 LODIndex = Cloth->GetMesh()->GetLODIndex();
PRAGMA_ENABLE_DEPRECATION_WARNINGS
			if (LODIndex != Cloth->GetLODIndex(Solver.Get()))
			{
				Solver->Update(Softs::FSolverReal(0.));  // Update for LOD switching, but do not simulate
			}

			if (Cloth->GetParticleRangeId(Solver.Get()) == INDEX_NONE || Cloth->GetLODIndex(Solver.Get()) == INDEX_NONE)
			{
				CurrentSimulationData.Remove(AssetIndex);  // Ensures that the cloth vertex factory won't run unnecessarily
				continue;  // No valid LOD, there's nothing to write out
			}

			// Get the reference bone index for this cloth
			const int32 ReferenceBoneIndex = LeaderPoseComponent ? ClothComponent.GetLeaderBoneMap()[Cloth->GetReferenceBoneIndex()] : Cloth->GetReferenceBoneIndex();
			if (!ComponentSpaceTransforms.IsValidIndex(ReferenceBoneIndex))
			{
				UE_CLOG(!bHasInvalidReferenceBoneTransforms, LogChaosClothAsset, Warning, TEXT("Failed to write back clothing simulation data for component %s as bone transforms are invalid."), *ClothComponent.GetName());
				bHasInvalidReferenceBoneTransforms = true;
				CurrentSimulationData.Reset();
				return;
			}

			// Get the reference transform used in the current animation pose
			FTransform ReferenceBoneTransform = ComponentSpaceTransforms[ReferenceBoneIndex];
			ReferenceBoneTransform *= ClothSimulationContext->ComponentTransform;
			ReferenceBoneTransform.SetScale3D(FVector(1.0f));  // Scale is already baked in the cloth mesh

			// Set the world space transform to be this cloth's reference bone
			FClothSimulData& Data = CurrentSimulationData.FindOrAdd(AssetIndex);
			Data.Transform = ReferenceBoneTransform;
			Data.ComponentRelativeTransform = ReferenceBoneTransform.GetRelativeTransform(ClothSimulationContext->ComponentTransform);

			// Retrieve the last reference space transform used for this cloth
			// Note: This won't necessary match the current bone reference transform when the simulation is paused,
			//       and still allows for the correct positioning of the sim data while the component is animated.
			FRigidTransform3 ReferenceSpaceTransform = Cloth->GetReferenceSpaceTransform();
			ReferenceSpaceTransform.AddToTranslation(-LocalSpaceLocation);

			// Copy positions and normals
			Data.Positions = Cloth->GetParticlePositions(Solver.Get());
			Data.Normals = Cloth->GetParticleNormals(Solver.Get());

			// Transform into the cloth reference simulation space used at the time of simulation
			check(Data.Positions.Num() == Data.Normals.Num())
#if INTEL_ISPC
			if (bTransformClothSimulData_ISPC_Enabled)
			{
				// ISPC is assuming float input here
				check(sizeof(ispc::FVector3f) == Data.Positions.GetTypeSize());
				check(sizeof(ispc::FVector3f) == Data.Normals.GetTypeSize());

				ispc::TransformClothSimulData(
					(ispc::FVector3f*)Data.Positions.GetData(),
					(ispc::FVector3f*)Data.Normals.GetData(),
					(ispc::FTransform&)ReferenceSpaceTransform,
					Data.Positions.Num());
			}
			else
#endif
			{
				for (int32 Index = 0; Index < Data.Positions.Num(); ++Index)
				{
					Data.Positions[Index] = FVec3f(ReferenceSpaceTransform.InverseTransformPosition(FVec3(Data.Positions[Index])));
					Data.Normals[Index] = FVec3f(ReferenceSpaceTransform.InverseTransformVector(FVec3(-Data.Normals[Index])));
				}
			}

			// Set the current LOD these data apply to, so that the correct deformer mappings can be applied
PRAGMA_DISABLE_DEPRECATION_WARNINGS  // TODO: CHAOS_IS_CLOTHINGSIMULATIONMESH_ABSTRACT
			Data.LODIndex = Cloth->GetMesh()->GetOwnerLODIndex(LODIndex);  // The owner component LOD index can be different to the cloth mesh LOD index
PRAGMA_ENABLE_DEPRECATION_WARNINGS
		}
	}

	const TMap<int32, FClothSimulData>& FClothSimulationProxy::GetCurrentSimulationData_AnyThread() const
	{
		// This is called during EndOfFrameUpdates, usually in a parallel-for loop. We need to be sure that
		// the cloth task (if there is one) is complete, but it cannot be waited for here. See OnPreEndOfFrameUpdateSync
		// which is called just before EOF updates and is where we would have waited for the cloth task.
		if (!IsValidRef(ParallelTask) || ParallelTask->IsComplete())
		{
			return CurrentSimulationData;
		}
		static const TMap<int32, FClothSimulData> EmptyClothSimulationData;
		return EmptyClothSimulationData;
	}

	FBoxSphereBounds FClothSimulationProxy::CalculateBounds_AnyThread() const
	{
		SCOPE_CYCLE_COUNTER(STAT_ClothSimulationProxy_CalculateBounds);

		check(Solver);
		if (!IsValidRef(ParallelTask) || ParallelTask->IsComplete())
		{
			FBoxSphereBounds Bounds = Solver->CalculateBounds();

			// The component could be moving while the simulation is suspended so getting the bounds
			// in world space isn't good enough and the bounds origin needs to be continuously updated
			Bounds = Bounds.TransformBy(FTransform((FQuat)Solver->GetLocalSpaceRotation(), (FVector)Solver->GetLocalSpaceLocation()).Inverse());

			return Bounds;
		}
		return FBoxSphereBounds(ForceInit);
	}

	void FClothSimulationProxy::InitializeConfigs()
	{
		// Replace physics thread's configs with the game thread's configs
		for (const TUniquePtr<::Chaos::FClothingSimulationConfig>& Config : Configs)
		{
			Config->Initialize(ClothComponent.GetPropertyCollections());  // TODO: Outfit and multi-cloths/solver config update
		}
		// TODO: separate solver config lod from cloth component
		Solver->SetSolverLOD(ClothSimulationContext->LodIndex);
	}

	void FClothSimulationProxy::FillSimulationContext(float DeltaTime, bool bIsInitialization)
	{
		ClothSimulationContext->Fill(ClothComponent, DeltaTime, MaxDeltaTime, bIsInitialization, CacheData.Get());
		CacheData.Reset();
	}

	bool FClothSimulationProxy::ShouldEnableSolver(bool bSolverCurrentlyEnabled) const
	{
		switch (SolverMode)
		{
		case ESolverMode::EnableSolverForSimulateRecord:
			return true;
		case ESolverMode::DisableSolverForPlayback:
			return false;
		case ESolverMode::Default:
		default:
			if (ClothSimulationContext->CacheData.HasData())
			{
				return false;
			}
		}
		return bSolverCurrentlyEnabled;
	}
}
