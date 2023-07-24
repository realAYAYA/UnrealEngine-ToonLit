// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/ClothSimulationProxy.h"
#include "ChaosClothAsset/ClothAsset.h"
#include "ChaosClothAsset/ClothAssetPrivate.h"
#include "ChaosClothAsset/ClothComponent.h"
#include "ChaosClothAsset/ClothSimulationMesh.h"
#include "ChaosClothAsset/ClothSimulationModel.h"
#include "ChaosCloth/ChaosClothingSimulationCloth.h"
#include "ChaosCloth/ChaosClothingSimulationCollider.h"
#include "ChaosCloth/ChaosClothingSimulationSolver.h"
#include "ChaosCloth/ChaosClothVisualization.h"
#include "PhysicsEngine/PhysicsSettings.h"

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

			ClothSimulationProxy.Tick_PhysicsThread();
		}

	private:
		FClothSimulationProxy& ClothSimulationProxy;
	};

	FClothSimulationProxy::FClothSimulationProxy(const UChaosClothComponent& InClothComponent)
		: ClothComponent(InClothComponent)
		, Solver(MakeUnique<::Chaos::FClothingSimulationSolver>())
		, Visualization(MakeUnique<::Chaos::FClothVisualization>(Solver.Get()))
		, MaxDeltaTime(UPhysicsSettings::Get()->MaxPhysicsDeltaTime)
	{
		using namespace ::Chaos;

		check(IsInGameThread());

		// Need a valid context to initialize the mesh
		constexpr bool bIsInitialization = true;
		ClothSimulationContext.Fill(ClothComponent, 0.f, MaxDeltaTime, bIsInitialization);

		// Create mesh node
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
		const int32 MeshIndex = Meshes.Emplace(MakeUnique<FClothSimulationMesh>(*ClothSimulationModel, ClothSimulationContext, DebugName));

		// Create collider node
PRAGMA_DISABLE_DEPRECATION_WARNINGS  // TODO: CHAOS_IS_CLOTHINGSIMULATIONMESH_ABSTRACT
		const FReferenceSkeleton* const ReferenceSkeleton = &ClothAsset->GetRefSkeleton();
		const int32 ColliderIndex = ClothComponent.GetPhysicsAsset() ? Colliders.Emplace(MakeUnique<FClothingSimulationCollider>(ClothComponent.GetPhysicsAsset(), ReferenceSkeleton)) : INDEX_NONE;
PRAGMA_ENABLE_DEPRECATION_WARNINGS

		//// Set the external collision data to get updated at every frame
		//Colliders[ColliderIndex]->SetCollisionData(&ExternalCollisionData);

		// Create cloth node
		const int32 ClothIndex = Cloths.Emplace(MakeUnique<FClothingSimulationCloth>(
PRAGMA_DISABLE_DEPRECATION_WARNINGS  // TODO: CHAOS_IS_CLOTHINGSIMULATIONMESH_ABSTRACT
			Meshes[MeshIndex].Get(),
PRAGMA_ENABLE_DEPRECATION_WARNINGS
			ColliderIndex != INDEX_NONE ? TArray<FClothingSimulationCollider*>({ Colliders[ColliderIndex].Get() }) : TArray<FClothingSimulationCollider*>(),
			0,
			FClothingSimulationCloth::EMassMode::Density,  //	(FClothingSimulationCloth::EMassMode)ClothConfig->MassMode,
			0.35f,  //	ClothConfig->GetMassValue(),
			0.0001f,  //	ClothConfig->MinPerParticleMass,
			TVec2<FRealSingle>(1.f),  //	TVec2<FRealSingle>(ClothConfig->EdgeStiffnessWeighted.Low, ClothConfig->EdgeStiffnessWeighted.High),
			TVec2<FRealSingle>(1.f),  //	TVec2<FRealSingle>(ClothConfig->BendingStiffnessWeighted.Low, ClothConfig->BendingStiffnessWeighted.High),
			0.f,  //	ClothConfig->BucklingRatio,
			TVec2<FRealSingle>(1.f),  //	TVec2<FRealSingle>(ClothConfig->BucklingStiffnessWeighted.Low, ClothConfig->BucklingStiffnessWeighted.High),
			false,  //	ClothConfig->bUseBendingElements,
			TVec2<FRealSingle>(1.f),  //	TVec2<FRealSingle>(ClothConfig->AreaStiffnessWeighted.Low, ClothConfig->AreaStiffnessWeighted.High),
			0.f,  //	ClothConfig->VolumeStiffness,
			false,  //	ClothConfig->bUseThinShellVolumeConstraints,
			TVec2<FRealSingle>(1.f),   //	TVec2<FRealSingle>(ClothConfig->TetherStiffness.Low, ClothConfig->TetherStiffness.High),  // Animatable
			TVec2<FRealSingle>(1.f),   //	TVec2<FRealSingle>(ClothConfig->TetherScale.Low, ClothConfig->TetherScale.High),  // Animatable
			FClothingSimulationCloth::ETetherMode::Geodesic,  //	ClothConfig->bUseGeodesicDistance ? FClothingSimulationCloth::ETetherMode::Geodesic : FClothingSimulationCloth::ETetherMode::Euclidean,
			1.f,  //	/*MaxDistancesMultiplier =*/ 1.f,  // Animatable
			TVec2<FRealSingle>(0.f),  //	TVec2<FRealSingle>(ClothConfig->AnimDriveStiffness.Low, ClothConfig->AnimDriveStiffness.High),  // Animatable
			TVec2<FRealSingle>(0.f),  //	TVec2<FRealSingle>(ClothConfig->AnimDriveDamping.Low, ClothConfig->AnimDriveDamping.High),  // Animatable
			0.f,  //	ClothConfig->ShapeTargetStiffness,  // TODO: This is now deprecated
			false,  //	false, // bUseXPBDEdgeSprings
			false,  //	false, // bUseXPBDBendingElements
			false,  //	false, // bUseXPBDAreaSprings
			.5f,   //	ClothConfig->GravityScale,
			false,  //	ClothConfig->bUseGravityOverride,
			TVec3<FRealSingle>(0.f, 0.f, -980.665f),  //	ClothConfig->Gravity,
			TVec3<FRealSingle>(0.75f),  //	ClothConfig->LinearVelocityScale,
			0.75f,  //	ClothConfig->AngularVelocityScale,
			1.f,  //	ClothConfig->FictitiousAngularScale,
			TVec2<FRealSingle>(0.035f, 1.f),  //	TVec2<FRealSingle>(ClothConfig->Drag.Low, ClothConfig->Drag.High),  // Animatable
			TVec2<FRealSingle>(0.035f, 1.f),  //	TVec2<FRealSingle>(ClothConfig->Lift.Low, ClothConfig->Lift.High),  // Animatable
			false,  //	ClothConfig->bUsePointBasedWindModel,
			TVec2<FRealSingle>(0.f),  //	TVec2<FRealSingle>(ClothConfig->Pressure.Low, ClothConfig->Pressure.High),  // Animatable
			0.01f,  //	ClothConfig->DampingCoefficient,
			0.f,  //	ClothConfig->LocalDampingCoefficient,
			1.f,  //	ClothConfig->CollisionThickness,
			0.8f,  //	ClothConfig->FrictionCoefficient,
			false,  //	ClothConfig->bUseCCD,
			false,  //	ClothConfig->bUseSelfCollisions,
			2.f,  //	ClothConfig->SelfCollisionThickness,
			0.f,  //	ClothConfig->SelfCollisionFriction,
			false,  //	ClothConfig->bUseSelfIntersections,
			false,  //	ClothConfig->bUseLegacyBackstop,
			false,  //	/*bUseLODIndexOverride =*/ false,
			INDEX_NONE));  //	/*LODIndexOverride =*/ INDEX_NONE));
		
		Cloths[ClothIndex]->SetAerodynamicsProperties(TVec2<FRealSingle>(0.5f, 1.f), TVec2<FRealSingle>(0.35f, 1.f), 1.225e-6f, TVec3<FRealSingle>(-30.f, -30.f, 200.f));

		// Add cloth to solver
		Solver->AddCloth(Cloths[ClothIndex].Get());
	}

	FClothSimulationProxy::~FClothSimulationProxy()
	{
		CompleteParallelSimulation_GameThread();
	}

	void FClothSimulationProxy::Tick_GameThread(float DeltaTime)
	{
		SCOPE_CYCLE_COUNTER(STAT_ClothSimulationProxy_TickGame);

		// Fill a new context, note the context is also needed when the simulation is suspended
		ClothSimulationContext.Fill(ClothComponent, DeltaTime, MaxDeltaTime);

		if (DeltaTime > 0.f && !ClothComponent.IsSimulationSuspended())
		{
			// Start the the cloth simulation thread
			ParallelTask = TGraphTask<FClothSimulationProxyParallelTask>::CreateTask(nullptr, ENamedThreads::GameThread).ConstructAndDispatchWhenReady(*this);
		}
		else
		{
			// It still needs to write back to the GT cache as the context has changed
			WriteSimulationData_GameThread();
		}
	}

	void FClothSimulationProxy::Tick_PhysicsThread()
	{
		using namespace ::Chaos;

		TRACE_CPUPROFILER_EVENT_SCOPE(FClothSimulationProxy_TickPhysics);
		SCOPE_CYCLE_COUNTER(STAT_ClothSimulationProxy_TickPhysics);
		if (ClothSimulationContext.DeltaTime == 0.f)
		{
			return;
		}

		const double StartTime = FPlatformTime::Seconds();
		const float PrevSimulationTime = SimulationTime;  // Copy the atomic to prevent a re-read

		const bool bNeedsReset = (ClothSimulationContext.bReset || PrevSimulationTime == 0.f);  // Reset on the first frame too since the simulation is created in bind pose, and not in start pose
		const bool bNeedsTeleport = ClothSimulationContext.bTeleport;
		bIsTeleported = bNeedsTeleport;

		// Update Solver animatable parameters
		Solver->SetLocalSpaceLocation((FVec3)ClothSimulationContext.ComponentTransform.GetLocation(), bNeedsReset);
		Solver->SetLocalSpaceRotation((FQuat)ClothSimulationContext.ComponentTransform.GetRotation());
		Solver->SetWindVelocity(ClothSimulationContext.WindVelocity);
		Solver->SetGravity(ClothSimulationContext.WorldGravity);
		Solver->SetNumIterations(8);  // TODO: Solver parameters
		Solver->SetMaxNumIterations(10);  // TODO: Solver parameters
		Solver->SetNumSubsteps(1);  // TODO: Solver parameters

		// Check teleport modes
		for (const TUniquePtr<FClothingSimulationCloth>& Cloth : Cloths)
		{
			// Update Cloth animatable parameters while in the cloth loop
			Cloth->SetMaxDistancesMultiplier(1000.0);

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
		Solver->Update((Softs::FSolverReal)ClothSimulationContext.DeltaTime);

		// Keep the actual used number of iterations for the stats
		NumIterations = Solver->GetNumUsedIterations();

		// Update simulation time in ms (and provide an instant average instead of the value in real-time)
		const float CurrSimulationTime = (float)((FPlatformTime::Seconds() - StartTime) * 1000.);
		static const float SimulationTimeDecay = 0.03f; // 0.03 seems to provide a good rate of update for the instant average
		SimulationTime = PrevSimulationTime ? PrevSimulationTime + (CurrSimulationTime - PrevSimulationTime) * SimulationTimeDecay : CurrSimulationTime;

		// Visualization
		Visualization->DrawPhysMeshWired();
		Visualization->DrawCollision();
		Visualization->DrawOpenEdges();
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
			WriteSimulationData_GameThread();
		}
	}

	void FClothSimulationProxy::WriteSimulationData_GameThread()
	{
		using namespace ::Chaos;

		check(IsInGameThread());

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

			if (Cloth->GetOffset(Solver.Get()) == INDEX_NONE || Cloth->GetLODIndex(Solver.Get()) == INDEX_NONE)
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
			ReferenceBoneTransform *= ClothSimulationContext.ComponentTransform;
			ReferenceBoneTransform.SetScale3D(FVector(1.0f));  // Scale is already baked in the cloth mesh

			// Set the world space transform to be this cloth's reference bone
			FClothSimulData& Data = CurrentSimulationData.FindOrAdd(AssetIndex);
			Data.Transform = ReferenceBoneTransform;
			Data.ComponentRelativeTransform = ReferenceBoneTransform.GetRelativeTransform(ClothSimulationContext.ComponentTransform);

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
}
