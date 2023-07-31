// Copyright Epic Games, Inc. All Rights Reserved.

#include "Physics/Experimental/PhysScene_Chaos.h"

#include "CoreMinimal.h"
#include "GameDelegates.h"

#include "Async/AsyncWork.h"
#include "Async/ParallelFor.h"
#include "Engine/Engine.h"

#include "Misc/CoreDelegates.h"
#include "Misc/ScopeLock.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "PhysicsReplication.h"
#include "Physics/Experimental/PhysicsUserData_Chaos.h"
#include "PhysicsEngine/PhysicsCollisionHandler.h"

#include "PhysicsSolver.h"
#include "ChaosSolversModule.h"
#include "ChaosLog.h"
#include "ChaosStats.h"

#include "Field/FieldSystem.h"

#include "PhysicsProxy/PerSolverFieldSystem.h"
#include "PhysicsProxy/GeometryCollectionPhysicsProxy.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"
#include "PhysicsProxy/SkeletalMeshPhysicsProxy.h"
#include "PhysicsProxy/StaticMeshPhysicsProxy.h"
#include "PhysicsProxy/JointConstraintProxy.h"
#include "Chaos/UniformGrid.h"
#include "Chaos/BoundingVolume.h"
#include "Chaos/Framework/DebugSubstep.h"
#include "Chaos/PBDSpringConstraints.h"
#include "Chaos/PBDJointConstraints.h"
#include "Chaos/PerParticleGravity.h"
#include "PBDRigidActiveParticlesBuffer.h"
#include "Chaos/GeometryParticlesfwd.h"
#include "Chaos/Box.h"
#include "EventsData.h"
#include "EventManager.h"
#include "RewindData.h"
#include "Chaos/PhysicsSolverBaseImpl.h"
#include "Chaos/Defines.h"

#if !UE_BUILD_SHIPPING
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

TAutoConsoleVariable<int32> CVar_ChaosDrawHierarchyEnable(TEXT("P.Chaos.DrawHierarchy.Enable"), 0, TEXT("Enable / disable drawing of the physics hierarchy"));
TAutoConsoleVariable<int32> CVar_ChaosDrawHierarchyCells(TEXT("P.Chaos.DrawHierarchy.Cells"), 0, TEXT("Enable / disable drawing of the physics hierarchy cells"));
TAutoConsoleVariable<int32> CVar_ChaosDrawHierarchyBounds(TEXT("P.Chaos.DrawHierarchy.Bounds"), 1, TEXT("Enable / disable drawing of the physics hierarchy bounds"));
TAutoConsoleVariable<int32> CVar_ChaosDrawHierarchyObjectBounds(TEXT("P.Chaos.DrawHierarchy.ObjectBounds"), 1, TEXT("Enable / disable drawing of the physics hierarchy object bounds"));
TAutoConsoleVariable<int32> CVar_ChaosDrawHierarchyCellElementThresh(TEXT("P.Chaos.DrawHierarchy.CellElementThresh"), 128, TEXT("Num elements to consider \"high\" for cell colouring when rendering."));
TAutoConsoleVariable<int32> CVar_ChaosDrawHierarchyDrawEmptyCells(TEXT("P.Chaos.DrawHierarchy.DrawEmptyCells"), 1, TEXT("Whether to draw cells that are empty when cells are enabled."));
TAutoConsoleVariable<int32> CVar_ChaosUpdateKinematicsOnDeferredSkelMeshes(TEXT("P.Chaos.UpdateKinematicsOnDeferredSkelMeshes"), 1, TEXT("Whether to defer update kinematics for skeletal meshes."));

#endif

int32 GEnableKinematicDeferralStartPhysicsCondition = 1;
FAutoConsoleVariableRef CVar_EnableKinematicDeferralStartPhysicsCondition(TEXT("p.EnableKinematicDeferralStartPhysicsCondition"), GEnableKinematicDeferralStartPhysicsCondition, TEXT("If is 1, allow kinematics to be deferred in start physics (probably only called from replication tick). If 0, no deferral in startphysics."));

DECLARE_CYCLE_STAT(TEXT("Update Kinematics On Deferred SkelMeshes"), STAT_UpdateKinematicsOnDeferredSkelMeshesChaos, STATGROUP_Physics);

#if WITH_EDITOR
#include "Editor.h"
#endif

struct FPendingAsyncPhysicsCommand
{
	int32 PhysicsStep;
	TWeakObjectPtr<UObject> OwningObject;
	TFunction<void()> Command;
};


class FAsyncPhysicsTickCallback : public Chaos::TSimCallbackObject<
	Chaos::FSimCallbackNoInput,
	Chaos::FSimCallbackNoOutput,
	Chaos::ESimCallbackOptions::Presimulate | Chaos::ESimCallbackOptions::RunOnFrozenGameThread>
{
public:

	TSet<UActorComponent*> AsyncPhysicsTickComponents;
	TSet<AActor*> AsyncPhysicsTickActors;
	TArray<FPendingAsyncPhysicsCommand> PendingCommands;

	virtual void OnPreSimulate_Internal() override
	{
		using namespace Chaos;

		for (int32 Idx = 0; Idx < PendingCommands.Num(); ++Idx)
		{
			const int32 CurrentFrame = static_cast<FPBDRigidsSolver*>(GetSolver())->GetCurrentFrame();

			FPendingAsyncPhysicsCommand* PendingCommand = &PendingCommands[Idx];
			bool bRemove = PendingCommand->OwningObject.IsStale();
			if (!bRemove && PendingCommands[Idx].PhysicsStep <= CurrentFrame)
			{
				PendingCommands[Idx].Command();
				bRemove = true;
			}

			if (bRemove)
			{
				PendingCommands.RemoveAt(Idx);	//Need to keep functions in order. If this is slow we could try going in reverse order, but expecting number of commands to be low per frame
				--Idx;
			}
		}

		const FReal DeltaTime = GetDeltaTime_Internal();
		const FReal SimTime = GetSimTime_Internal();
		//TODO: handle case where callbacks modify AsyncPhysicsTickComponents or AsyncPhysicsTickActors
		for (UActorComponent* Component : AsyncPhysicsTickComponents)
		{
			Component->AsyncPhysicsTickComponent(DeltaTime, SimTime);
		}

		for(AActor* Actor : AsyncPhysicsTickActors)
		{
			Actor->AsyncPhysicsTickActor(DeltaTime, SimTime);
		}
	}
};

DEFINE_LOG_CATEGORY_STATIC(LogFPhysScene_ChaosSolver, Log, All);

void DumpHierarchyStats(const TArray<FString>& Args)
{
#if !UE_BUILD_SHIPPING
	if(FChaosSolversModule* Module = FChaosSolversModule::GetModule())
	{
		int32 MaxElems = 0;
		Module->DumpHierarchyStats(&MaxElems);

		if(Args.Num() > 0 && Args[0] == TEXT("UPDATERENDER"))
		{
			CVar_ChaosDrawHierarchyCellElementThresh->Set(MaxElems);
		}
	}
#endif
}

static FAutoConsoleCommand Command_DumpHierarchyStats(TEXT("p.chaos.dumphierarcystats"), TEXT("Outputs current collision hierarchy stats to the output log"), FConsoleCommandWithArgsDelegate::CreateStatic(DumpHierarchyStats));

#if !UE_BUILD_SHIPPING
class FSpacialDebugDraw : public Chaos::ISpacialDebugDrawInterface<Chaos::FReal>
{
public:

	FSpacialDebugDraw(UWorld* InWorld)
		: World(InWorld)
	{

	}

	virtual void Box(const Chaos::FAABB3& InBox, const Chaos::FVec3& InLinearColor, float InThickness) override
	{
		// LWC_TODO: Remove FVector cast here.
		DrawDebugBox(World, InBox.Center(), InBox.Extents(), FQuat::Identity, FLinearColor(FVector(InLinearColor)).ToFColor(true), false, -1.0f, SDPG_Foreground, InThickness);
	}


	virtual void Line(const Chaos::FVec3& InBegin, const Chaos::FVec3& InEnd, const Chaos::FVec3& InLinearColor, float InThickness) override
	{
		DrawDebugLine(World, InBegin, InEnd, FLinearColor(FVector(InLinearColor)).ToFColor(true), false, -1.0f, SDPG_Foreground, InThickness);
	}

private:
	UWorld* World;
};
#endif

class FPhysicsThreadSyncCaller : public FTickableGameObject
{
public:
#if CHAOS_WITH_PAUSABLE_SOLVER
	DECLARE_MULTICAST_DELEGATE(FOnUpdateWorldPause);
	FOnUpdateWorldPause OnUpdateWorldPause;
#endif

	FPhysicsThreadSyncCaller()
	{
		ChaosModule = FChaosSolversModule::GetModule();
		check(ChaosModule);

		WorldCleanupHandle = FWorldDelegates::OnPostWorldCleanup.AddRaw(this, &FPhysicsThreadSyncCaller::OnWorldDestroyed);
	}

	~FPhysicsThreadSyncCaller()
	{
		if(WorldCleanupHandle.IsValid())
		{
			FWorldDelegates::OnPostWorldCleanup.Remove(WorldCleanupHandle);
		}
	}

	virtual void Tick(float DeltaTime) override
	{
		if(ChaosModule->IsPersistentTaskRunning())
		{
			ChaosModule->SyncTask();

#if !UE_BUILD_SHIPPING
			DebugDrawSolvers();
#endif
		}

#if CHAOS_WITH_PAUSABLE_SOLVER
		// Check each physics scene's world status and update the corresponding solver's pause state
		OnUpdateWorldPause.Broadcast();
#endif
	}

	virtual TStatId GetStatId() const override
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(PhysicsThreadSync, STATGROUP_Tickables);
	}

	virtual bool IsTickableInEditor() const override
	{
		return false;
	}

private:

	void OnWorldDestroyed(UWorld* InWorld, bool bSessionEnded, bool bCleanupResources)
	{
		// This should really only sync if it's the right world, but for now always sync on world destroy.
		if(ChaosModule->IsPersistentTaskRunning())
		{
			ChaosModule->SyncTask(true);
		}
	}

#if !UE_BUILD_SHIPPING
	void DebugDrawSolvers()
	{
		using namespace Chaos;
		const bool bDrawHier = CVar_ChaosDrawHierarchyEnable.GetValueOnGameThread() != 0;
		const bool bDrawCells = CVar_ChaosDrawHierarchyCells.GetValueOnGameThread() != 0;
		const bool bDrawEmptyCells = CVar_ChaosDrawHierarchyDrawEmptyCells.GetValueOnGameThread() != 0;
		const bool bDrawBounds = CVar_ChaosDrawHierarchyBounds.GetValueOnGameThread() != 0;
		const bool bDrawObjectBounds = CVar_ChaosDrawHierarchyObjectBounds.GetValueOnGameThread() != 0;

		UWorld* WorldPtr = nullptr;
		const TIndirectArray<FWorldContext>& WorldContexts = GEngine->GetWorldContexts();
		for(const FWorldContext& Context : WorldContexts)
		{
			UWorld* TestWorld = Context.World();
			if(TestWorld && (Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::PIE))
			{
				WorldPtr = TestWorld;
			}
		}

		if(!WorldPtr)
		{
			// Can't debug draw without a valid world
			return;
		}

		FSpacialDebugDraw DrawInterface(WorldPtr);

		const TArray<FPhysicsSolverBase*>& Solvers = ChaosModule->GetAllSolvers();

		for(FPhysicsSolverBase* Solver : Solvers)
		{
			if(bDrawHier)
			{
#if TODO_REIMPLEMENT_SPATIAL_ACCELERATION_ACCESS
				if (const ISpatialAcceleration<Chaos::FReal, 3>* SpatialAcceleration = Solver->GetSpatialAcceleration())
				{
					SpatialAcceleration->DebugDraw(&DrawInterface);
					Solver->ReleaseSpatialAcceleration();
				}
#endif
				
#if 0
				if (const Chaos::TBoundingVolume<FPBDRigidParticles>* BV = SpatialAcceleration->Cast<TBoundingVolume<FPBDRigidParticles>>())
				{

					const TUniformGrid<Chaos::FReal, 3>& Grid = BV->GetGrid();

					if (bDrawBounds)
					{
						const FVector Min = Grid.MinCorner();
						const FVector Max = Grid.MaxCorner();

						DrawDebugBox(WorldPtr, (Min + Max) / 2, (Max - Min) / 2, FQuat::Identity, FColor::Cyan, false, -1.0f, SDPG_Foreground, 1.0f);
					}

					if (bDrawObjectBounds)
					{
						const TArray<FAABB3>& Boxes = BV->GetWorldSpaceBoxes();
						for (const FAABB3& Box : Boxes)
						{
							DrawDebugBox(WorldPtr, Box.Center(), Box.Extents() / 2.0f, FQuat::Identity, FColor::Cyan, false, -1.0f, SDPG_Foreground, 1.0f);
						}
					}

					if (bDrawCells)
					{
						// Reduce the extent very slightly to differentiate cell colors
						const FVector CellExtent = Grid.Dx() * 0.95;

						const TVec3<int32>& CellCount = Grid.Counts();
						for (int32 CellsX = 0; CellsX < CellCount[0]; ++CellsX)
						{
							for (int32 CellsY = 0; CellsY < CellCount[1]; ++CellsY)
							{
								for (int32 CellsZ = 0; CellsZ < CellCount[2]; ++CellsZ)
								{
									const TArray<int32>& CellList = BV->GetElements()(CellsX, CellsY, CellsZ);
									const int32 NumEntries = CellList.Num();

									const Chaos::FReal TempFraction = FMath::Min<Chaos::FReal>(NumEntries / (Chaos::FReal)CVar_ChaosDrawHierarchyCellElementThresh.GetValueOnGameThread(), 1.0f);

									const FColor CellColor = FColor::MakeRedToGreenColorFromScalar(1.0f - TempFraction);

									if (NumEntries > 0 || bDrawEmptyCells)
									{
										DrawDebugBox(WorldPtr, Grid.Location(TVec3<int32>(CellsX, CellsY, CellsZ)), CellExtent / 2.0f, FQuat::Identity, CellColor, false, -1.0f, SDPG_Foreground, 0.5f);
									}
								}
							}
						}
					}
				}
#endif
			}
		}
	}
#endif

	FChaosSolversModule* ChaosModule;
	FDelegateHandle WorldCleanupHandle;
};
static FPhysicsThreadSyncCaller* SyncCaller;

#if WITH_EDITOR
// Singleton class to register pause/resume/single-step/pre-end handles to the editor
// and issue the pause/resume/single-step commands to the Chaos' module.
class FPhysScene_ChaosPauseHandler final
{
public:
	explicit FPhysScene_ChaosPauseHandler(FChaosSolversModule* InChaosModule)
		: ChaosModule(InChaosModule)
	{
		check(InChaosModule);
		// Add editor pause/step handles
		FEditorDelegates::BeginPIE.AddRaw(this, &FPhysScene_ChaosPauseHandler::ResumeSolvers);
		FEditorDelegates::EndPIE.AddRaw(this, &FPhysScene_ChaosPauseHandler::PauseSolvers);
		FEditorDelegates::PausePIE.AddRaw(this, &FPhysScene_ChaosPauseHandler::PauseSolvers);
		FEditorDelegates::ResumePIE.AddRaw(this, &FPhysScene_ChaosPauseHandler::ResumeSolvers);
		FEditorDelegates::SingleStepPIE.AddRaw(this, &FPhysScene_ChaosPauseHandler::SingleStepSolvers);
	}

	~FPhysScene_ChaosPauseHandler()
	{
		// No need to remove the editor pause/step delegates, it'll be too late for that on exit.
	}

private:
	void PauseSolvers(bool /*bIsSimulating*/) { ChaosModule->PauseSolvers(); }
	void ResumeSolvers(bool /*bIsSimulating*/) { ChaosModule->ResumeSolvers(); }
	void SingleStepSolvers(bool /*bIsSimulating*/) { ChaosModule->SingleStepSolvers(); }

private:
	FChaosSolversModule* ChaosModule;
};
static TUniquePtr<FPhysScene_ChaosPauseHandler> PhysScene_ChaosPauseHandler;
#endif

static void CopyParticleData(Chaos::FPBDRigidParticles& ToParticles, const int32 ToIndex, Chaos::FPBDRigidParticles& FromParticles, const int32 FromIndex)
{
	ToParticles.X(ToIndex) = FromParticles.X(FromIndex);
	ToParticles.R(ToIndex) = FromParticles.R(FromIndex);
	ToParticles.V(ToIndex) = FromParticles.V(FromIndex);
	ToParticles.W(ToIndex) = FromParticles.W(FromIndex);
	ToParticles.M(ToIndex) = FromParticles.M(FromIndex);
	ToParticles.InvM(ToIndex) = FromParticles.InvM(FromIndex);
	ToParticles.I(ToIndex) = FromParticles.I(FromIndex);
	ToParticles.InvI(ToIndex) = FromParticles.InvI(FromIndex);
	ToParticles.SetGeometry(ToIndex, FromParticles.Geometry(FromIndex));	//question: do we need to deal with dynamic geometry?
	ToParticles.CollisionParticles(ToIndex) = MoveTemp(FromParticles.CollisionParticles(FromIndex));
	ToParticles.DisabledRef(ToIndex) = FromParticles.Disabled(FromIndex);
	ToParticles.SetSleeping(ToIndex, FromParticles.Sleeping(FromIndex));
}

/** Struct to remember a pending component transform change */
struct FPhysScenePendingComponentTransform_Chaos
{
	/** New transform from physics engine */
	FVector NewTranslation;
	FQuat NewRotation;
	/** Component to move */
	TWeakObjectPtr<UPrimitiveComponent> OwningComp;
	bool bHasValidTransform;
	Chaos::EWakeEventEntry WakeEvent;
	
	FPhysScenePendingComponentTransform_Chaos(UPrimitiveComponent* InOwningComp, const FVector& InNewTranslation, const FQuat& InNewRotation, const Chaos::EWakeEventEntry InWakeEvent)
		: NewTranslation(InNewTranslation)
		, NewRotation(InNewRotation)
		, OwningComp(InOwningComp)
		, bHasValidTransform(true)
		, WakeEvent(InWakeEvent)
	{}

	FPhysScenePendingComponentTransform_Chaos(UPrimitiveComponent* InOwningComp, const Chaos::EWakeEventEntry InWakeEvent)
		: OwningComp(InOwningComp)
		, bHasValidTransform(false)
		, WakeEvent(InWakeEvent)
	{}

};

FPhysScene_Chaos::FPhysScene_Chaos(AActor* InSolverActor
#if CHAOS_DEBUG_NAME
	, const FName& DebugName
#endif
)
	: Super(InSolverActor ? InSolverActor->GetWorld() : nullptr
		, UPhysicsSettings::Get()->bTickPhysicsAsync ? UPhysicsSettings::Get()->AsyncFixedTimeStepSize : -1
#if CHAOS_DEBUG_NAME
		, DebugName
#endif
	)
	, PhysicsReplication(nullptr)
	, SolverActor(InSolverActor)
	, LastEventDispatchTime(Chaos::FReal(-1))
#if WITH_EDITOR
	, SingleStepCounter(0)
#endif
#if CHAOS_WITH_PAUSABLE_SOLVER
	, bIsWorldPaused(false)
#endif
{
	LLM_SCOPE(ELLMTag::ChaosScene);

	PhysicsProxyToComponentMap.Reset();
	ComponentToPhysicsProxyMap.Reset();

#if WITH_EDITOR
	if(!PhysScene_ChaosPauseHandler)
	{
		PhysScene_ChaosPauseHandler = MakeUnique<FPhysScene_ChaosPauseHandler>(ChaosModule);
	}
#endif

	Chaos::FEventManager* EventManager = SceneSolver->GetEventManager();
	EventManager->RegisterHandler<Chaos::FCollisionEventData>(Chaos::EEventType::Collision, this, &FPhysScene_Chaos::HandleCollisionEvents);

	//Initialize unique ptrs that are just here to allow forward declare. This should be reworked todo(ocohen)
#if TODO_FIX_REFERENCES_TO_ADDARRAY
	BodyInstances = MakeUnique<Chaos::TArrayCollectionArray<FBodyInstance*>>();
	Scene.GetSolver()->GetEvolution()->GetParticles().AddArray(BodyInstances.Get());
#endif

	// Create replication manager
	PhysicsReplication = PhysicsReplicationFactory.IsValid() ? PhysicsReplicationFactory->Create(this) : new FPhysicsReplication(this);

	FPhysicsDelegates::OnPhysSceneInit.Broadcast(this);
}

FPhysScene_Chaos::~FPhysScene_Chaos()
{
	if (AsyncPhysicsTickCallback)
	{
		SceneSolver->UnregisterAndFreeSimCallbackObject_External(AsyncPhysicsTickCallback);
	}

	// Must ensure deferred components do not hold onto scene pointer.
	ProcessDeferredCreatePhysicsState();

	FPhysicsDelegates::OnPhysSceneTerm.Broadcast(this);

	if (IPhysicsReplicationFactory* RawReplicationFactory = FPhysScene_Chaos::PhysicsReplicationFactory.Get())
	{
		RawReplicationFactory->Destroy(PhysicsReplication);
	}
	else if(PhysicsReplication)
	{
		delete PhysicsReplication;
	}

#if CHAOS_WITH_PAUSABLE_SOLVER
	if (SyncCaller)
	{
		SyncCaller->OnUpdateWorldPause.RemoveAll(this);
	}
#endif
}

#if WITH_EDITOR
bool FPhysScene_Chaos::IsOwningWorldEditor() const
{
	const UWorld* WorldPtr = GetOwningWorld();
	const TIndirectArray<FWorldContext>& WorldContexts = GEngine->GetWorldContexts();
	for (const FWorldContext& Context : WorldContexts)
	{
		if (WorldPtr)
		{
			if (WorldPtr == Context.World())
			{
				if (Context.WorldType == EWorldType::Editor)
				{
					return true;
				}
			}
		}
	}

	return false;
}
#endif

AActor* FPhysScene_Chaos::GetSolverActor() const
{
	return SolverActor.Get();
}

void FPhysScene_Chaos::RegisterForCollisionEvents(UPrimitiveComponent* Component)
{
	CollisionEventRegistrations.AddUnique(Component);
}

void FPhysScene_Chaos::UnRegisterForCollisionEvents(UPrimitiveComponent* Component)
{
	CollisionEventRegistrations.Remove(Component);
}

template<typename ObjectType>
void AddPhysicsProxy(ObjectType* InObject, Chaos::FPhysicsSolver* InSolver)
{
	ensure(false);
}

void FPhysScene_Chaos::AddObject(UPrimitiveComponent* Component, FSkeletalMeshPhysicsProxy* InObject)
{
	AddToComponentMaps(Component, InObject);
	ensure(false);
}

void FPhysScene_Chaos::AddObject(UPrimitiveComponent* Component, FStaticMeshPhysicsProxy* InObject)
{
	AddToComponentMaps(Component, InObject);
	ensure(false);
}

void FPhysScene_Chaos::AddObject(UPrimitiveComponent* Component, Chaos::FSingleParticlePhysicsProxy* InObject)
{
	AddToComponentMaps(Component, InObject);
	ensure(false);
}

void FPhysScene_Chaos::AddObject(UPrimitiveComponent* Component, FGeometryCollectionPhysicsProxy* InObject)
{
	AddToComponentMaps(Component, InObject);

	Chaos::FPhysicsSolver* Solver = GetSolver();
	Solver->RegisterObject(InObject);
}

template<typename ObjectType>
void RemovePhysicsProxy(ObjectType* InObject, Chaos::FPhysicsSolver* InSolver, FChaosSolversModule* InModule)
{
	check(IsInGameThread());


	const bool bDedicatedThread = false;

	// Remove the object from the solver
	InSolver->EnqueueCommandImmediate([InSolver, InObject, bDedicatedThread]()
	{
#if CHAOS_PARTICLEHANDLE_TODO
		InSolver->UnregisterObject(InObject);
#endif
		InObject->OnRemoveFromScene();

		if (!bDedicatedThread)
		{
			InObject->SyncBeforeDestroy();
			delete InObject;
		}

	});
}

void FPhysScene_Chaos::RemoveObject(FSkeletalMeshPhysicsProxy* InObject)
{
	ensure(false);
#if 0
	Chaos::FPhysicsSolver* Solver = InObject->GetSolver();
	const int32 NumRemoved = Solver->UnregisterObject(InObject);

	if(NumRemoved == 0)
	{
		UE_LOG(LogChaos, Warning, TEXT("Attempted to remove an object that wasn't found in its solver's gamethread storage - it's likely the solver has been mistakenly changed."));
	}

	RemoveFromComponentMaps(InObject);

	RemovePhysicsProxy(InObject, Solver, ChaosModule);
#endif
}

void FPhysScene_Chaos::RemoveObject(FStaticMeshPhysicsProxy* InObject)
{
	ensure(false);
#if 0
	Chaos::FPhysicsSolver* Solver = InObject->GetSolver();

	const int32 NumRemoved = Solver->UnregisterObject(InObject);

	if(NumRemoved == 0)
	{
		UE_LOG(LogChaos, Warning, TEXT("Attempted to remove an object that wasn't found in its solver's gamethread storage - it's likely the solver has been mistakenly changed."));
	}

	RemoveFromComponentMaps(InObject);

	RemovePhysicsProxy(InObject, Solver, ChaosModule);
#endif
}

void FPhysScene_Chaos::RemoveObject(Chaos::FSingleParticlePhysicsProxy* InObject)
{
	ensure(false);
#if 0
	Chaos::FPhysicsSolver* Solver = InObject->GetSolver();

	const int32 NumRemoved = Solver->UnregisterObject(InObject);

	if (NumRemoved == 0)
	{
		UE_LOG(LogChaos, Warning, TEXT("Attempted to remove an object that wasn't found in its solver's gamethread storage - it's likely the solver has been mistakenly changed."));
	}

	RemoveFromComponentMaps(InObject);

	RemovePhysicsProxy(InObject, Solver, ChaosModule);
#endif
}

void FPhysScene_Chaos::RemoveObject(FGeometryCollectionPhysicsProxy* InObject)
{
	Chaos::FPhysicsSolver* Solver = InObject->GetSolver<Chaos::FPhysicsSolver>();

	for (TUniquePtr<Chaos::TGeometryParticle<Chaos::FReal, 3>>& GTParticleUnique : InObject->GetExternalParticles())
	{
		Chaos::TGeometryParticle<Chaos::FReal, 3>* GTParticle = GTParticleUnique.Get();
		RemoveActorFromAccelerationStructureImp(GTParticle);
		if(Solver)
		{
			Solver->UpdateParticleInAccelerationStructure_External(GTParticle, /*bDelete=*/true); // ensures deletion will be applied to structures passed from physics thread.
		}
	}

	if(Solver)
	{
		Solver->UnregisterObject(InObject);
	}

	RemoveFromComponentMaps(InObject);
}

FPhysicsReplication* FPhysScene_Chaos::GetPhysicsReplication()
{
	return PhysicsReplication;
}

void FPhysScene_Chaos::SetPhysicsReplication(FPhysicsReplication* InPhysicsReplication)
{
	PhysicsReplication = InPhysicsReplication;
}

void FPhysScene_Chaos::AddReferencedObjects(FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(Collector);
#if WITH_EDITOR

	for (TPair<IPhysicsProxyBase*, UPrimitiveComponent*>& Pair : PhysicsProxyToComponentMap)
	{
		Collector.AddReferencedObject(Pair.Get<1>());
	}
#endif
}

FBodyInstance* FPhysScene_Chaos::GetBodyInstanceFromProxy(const IPhysicsProxyBase* PhysicsProxy) const
{
	FBodyInstance* BodyInstance = nullptr;
	if (PhysicsProxy && PhysicsProxy->GetType() == EPhysicsProxyType::SingleParticleProxy)
	{
		const Chaos::FRigidBodyHandle_External& RigidBodyHandle = static_cast<const Chaos::FSingleParticlePhysicsProxy*>(PhysicsProxy)->GetGameThreadAPI();
		BodyInstance = FPhysicsUserData::Get<FBodyInstance>(RigidBodyHandle.UserData());
	}
	// found none, let's see if there's an owning component in the scene
	if (BodyInstance == nullptr)
	{
		if (UPrimitiveComponent* const OwningComponent = GetOwningComponent<UPrimitiveComponent>(PhysicsProxy))
		{
			BodyInstance = OwningComponent->GetBodyInstance();
		}
	}
	return BodyInstance;
}

const FBodyInstance* FPhysScene_Chaos::GetBodyInstanceFromProxyAndShape(IPhysicsProxyBase* InProxy, int32 InShapeIndex) const
{
	const FBodyInstance* ProxyInstance = GetBodyInstanceFromProxy(InProxy);

	if(ProxyInstance && InShapeIndex != INDEX_NONE)
	{
		PhysicsInterfaceTypes::FInlineShapeArray ShapeHandles;
		FPhysicsInterface::GetAllShapes_AssumedLocked(ProxyInstance->GetPhysicsActorHandle(), ShapeHandles);

		if(ShapeHandles.IsValidIndex(InShapeIndex))
		{
			ProxyInstance = ProxyInstance->GetOriginalBodyInstance(ShapeHandles[InShapeIndex]);
		}
	}

	return ProxyInstance;
}

FCollisionNotifyInfo& FPhysScene_Chaos::GetPendingCollisionForContactPair(const void* P0, const void* P1, bool& bNewEntry)
{
	const FUniqueContactPairKey Key = { P0, P1 };
	const int32* PendingNotifyIdx = ContactPairToPendingNotifyMap.Find(Key);
	if (PendingNotifyIdx)
	{
		// we already have one for this pair
		bNewEntry = false;
		return PendingCollisionNotifies[*PendingNotifyIdx];
	}

	// make a new entry
	bNewEntry = true;
	int32 NewIdx = PendingCollisionNotifies.AddZeroed();
	return PendingCollisionNotifies[NewIdx];
}

void FPhysScene_Chaos::HandleCollisionEvents(const Chaos::FCollisionEventData& Event)
{
	ContactPairToPendingNotifyMap.Reset();

	TMap<IPhysicsProxyBase*, TArray<int32>> const& PhysicsProxyToCollisionIndicesMap = Event.PhysicsProxyToCollisionIndices.PhysicsProxyToIndicesMap;
	Chaos::FCollisionDataArray const& CollisionData = Event.CollisionData.AllCollisionsArray;
	const Chaos::FReal MinDeltaVelocityThreshold = UPhysicsSettings::Get()->MinDeltaVelocityForHitEvents;
	int32 NumCollisions = CollisionData.Num();

	if (NumCollisions > 0 && LastEventDispatchTime < Event.CollisionData.TimeCreated)
	{
		LastEventDispatchTime = Event.CollisionData.TimeCreated;

		// look through all the components that someone is interested in, and see if they had a collision
		// note that we only need to care about the interaction from the POV of the registered component,
		// since if anyone wants notifications for the other component it hit, it's also registered and we'll get to that elsewhere in the list
		for (TArray<UPrimitiveComponent*>::TIterator It(CollisionEventRegistrations); It; ++It)
		{
			UPrimitiveComponent* const Comp0 = *It;
			const TArray<IPhysicsProxyBase*>* PhysicsProxyArray = GetOwnedPhysicsProxies(Comp0);

			if (PhysicsProxyArray)
			{
				for (IPhysicsProxyBase* PhysicsProxy0 : *PhysicsProxyArray)
				{
					TArray<int32> const* const CollisionIndices = PhysicsProxyToCollisionIndicesMap.Find(PhysicsProxy0);
					if (CollisionIndices)
					{
						for (int32 EncodedCollisionIdx : *CollisionIndices)
						{
							bool bSwapOrder;
							int32 CollisionIdx = Chaos::FEventManager::DecodeCollisionIndex(EncodedCollisionIdx, bSwapOrder);

							Chaos::FCollidingData const& CollisionDataItem = CollisionData[CollisionIdx];
							IPhysicsProxyBase* const PhysicsProxy1 = bSwapOrder ? CollisionDataItem.Proxy1 : CollisionDataItem.Proxy2;

							// Are the proxies pending destruction? If they are no longer tracked by the PhysScene, the proxy is deleted or pending deletion.
							if (GetOwningComponent<UPrimitiveComponent>(PhysicsProxy0) == nullptr || GetOwningComponent<UPrimitiveComponent>(PhysicsProxy1) == nullptr)
							{
								continue;
							}

							bool bNewEntry = false;
							FCollisionNotifyInfo& NotifyInfo = GetPendingCollisionForContactPair(PhysicsProxy0, PhysicsProxy1, bNewEntry);

							// #note: we only notify on the first contact, though we will still accumulate the impulse data from subsequent contacts
							const FVector NormalImpulse = FVector::DotProduct(CollisionDataItem.AccumulatedImpulse, CollisionDataItem.Normal) * CollisionDataItem.Normal;	// project impulse along normal
							const FVector FrictionImpulse = FVector(CollisionDataItem.AccumulatedImpulse) - NormalImpulse; // friction is component not along contact normal
							NotifyInfo.RigidCollisionData.TotalNormalImpulse += NormalImpulse;
							NotifyInfo.RigidCollisionData.TotalFrictionImpulse += FrictionImpulse;

							if (bNewEntry)
							{
								// fill in legacy contact data
								NotifyInfo.bCallEvent0 = true;
								// if Comp1 wants this event too, it will get its own pending collision entry, so we leave it false

								// Get swapped velocity deltas
								const FVector& DeltaVelocity1 = bSwapOrder
									? CollisionDataItem.DeltaVelocity2
									: CollisionDataItem.DeltaVelocity1;
								const FVector& DeltaVelocity2 = bSwapOrder
									? CollisionDataItem.DeltaVelocity1
									: CollisionDataItem.DeltaVelocity2;
									
								NotifyInfo.Info0.SetFrom(GetBodyInstanceFromProxyAndShape(PhysicsProxy0, CollisionDataItem.ShapeIndex1), DeltaVelocity1);
								NotifyInfo.Info1.SetFrom(GetBodyInstanceFromProxyAndShape(PhysicsProxy1, CollisionDataItem.ShapeIndex2), DeltaVelocity2);

								// in some case ( like with geometry collections ) we don't have a body instance so the component part will null, we need to handle that 
								if (NotifyInfo.Info0.Component == nullptr)
								{
									NotifyInfo.Info0.Component = GetOwningComponent<UPrimitiveComponent>(PhysicsProxy0);
									NotifyInfo.Info0.Actor = (NotifyInfo.Info0.Component != nullptr) ? NotifyInfo.Info0.Component->GetOwner() : nullptr;
								}
								if (NotifyInfo.Info1.Component == nullptr)
								{
									NotifyInfo.Info1.Component = GetOwningComponent<UPrimitiveComponent>(PhysicsProxy1);
									NotifyInfo.Info1.Actor = (NotifyInfo.Info1.Component != nullptr) ? NotifyInfo.Info1.Component->GetOwner() : nullptr;
								}

								FRigidBodyContactInfo& NewContact = NotifyInfo.RigidCollisionData.ContactInfos.AddZeroed_GetRef();
								NewContact.ContactNormal = CollisionDataItem.Normal;
								NewContact.ContactPosition = CollisionDataItem.Location;
								NewContact.ContactPenetration = CollisionDataItem.PenetrationDepth;
								NewContact.bContactProbe = CollisionDataItem.bProbe;
								NotifyInfo.RigidCollisionData.bIsVelocityDeltaUnderThreshold =
									DeltaVelocity1.IsNearlyZero(MinDeltaVelocityThreshold) &&
									DeltaVelocity2.IsNearlyZero(MinDeltaVelocityThreshold);
								
								Chaos::FChaosPhysicsMaterial* InternalMat1 = CollisionDataItem.Mat1.Get();
								Chaos::FChaosPhysicsMaterial* InternalMat2 = CollisionDataItem.Mat2.Get();
								
								NewContact.PhysMaterial[0] = InternalMat1 ? FPhysicsUserData::Get<UPhysicalMaterial>(InternalMat1->UserData) : nullptr;
								NewContact.PhysMaterial[1] = InternalMat2 ? FPhysicsUserData::Get<UPhysicalMaterial>(InternalMat2->UserData) : nullptr;

								if(bSwapOrder)
								{
									NotifyInfo.RigidCollisionData.SwapContactOrders();
								}
							}
						}
					}
				}
			}
		}
	}

	// Tell the world and actors about the collisions
	DispatchPendingCollisionNotifies();
}

void FPhysScene_Chaos::DispatchPendingCollisionNotifies()
{
	UWorld const* const OwningWorld = GetOwningWorld();

	// Let the game-specific PhysicsCollisionHandler process any physics collisions that took place
	if (OwningWorld != nullptr && OwningWorld->PhysicsCollisionHandler != nullptr)
	{
		OwningWorld->PhysicsCollisionHandler->HandlePhysicsCollisions_AssumesLocked(PendingCollisionNotifies);
	}

	// Fire any collision notifies in the queue.
	for (FCollisionNotifyInfo& NotifyInfo : PendingCollisionNotifies)
	{
		//		if (NotifyInfo.RigidCollisionData.ContactInfos.Num() > 0)
		{
			if (NotifyInfo.bCallEvent0 && /*NotifyInfo.IsValidForNotify() && */ NotifyInfo.Info0.Actor.IsValid())
			{
				NotifyInfo.Info0.Actor->DispatchPhysicsCollisionHit(NotifyInfo.Info0, NotifyInfo.Info1, NotifyInfo.RigidCollisionData);
			}

			// CHAOS: don't call event 1, because the code below will generate the reflexive hit data as separate entries
		}
	}

	PendingCollisionNotifies.Reset();
}

#if CHAOS_WITH_PAUSABLE_SOLVER
void FPhysScene_Chaos::OnUpdateWorldPause()
{
	// Check game pause
	bool bIsPaused = false;
	const AActor* const Actor = GetSolverActor();
	if (Actor)
	{
		const UWorld* World = Actor->GetWorld();
		if (World)
		{
			// Use a simpler version of the UWorld::IsPaused() implementation that doesn't take the editor pause into account.
			// This is because OnUpdateWorldPause() is usually called within a tick update that happens well after that 
			// the single step flag has been used and cleared up, and the solver will stay paused otherwise.
			// The editor single step is handled separately with an editor delegate that pauses/single-steps all threads at once.
			const AWorldSettings* const Info = World->GetWorldSettings(/*bCheckStreamingPersistent=*/false, /*bChecked=*/false);
			bIsPaused = ((Info && Info->GetPauserPlayerState() && World->TimeSeconds >= World->PauseDelay) ||
				(World->bRequestedBlockOnAsyncLoading && World->GetNetMode() == NM_Client) ||
				(World && GEngine->ShouldCommitPendingMapChange(World)));
		}
	}

#if TODO_REIMPLEMENT_SOLVER_PAUSING
	if (bIsWorldPaused != bIsPaused)
	{
		bIsWorldPaused = bIsPaused;
		// Update solver pause status
		Chaos::IDispatcher* const PhysDispatcher = ChaosModule->GetDispatcher();
		if (PhysDispatcher)
		{
			UE_LOG(LogFPhysScene_ChaosSolver, Verbose, TEXT("FPhysScene_Chaos::OnUpdateWorldPause() pause status changed for actor %s, bIsPaused = %d"), Actor ? *Actor->GetName() : TEXT("None"), bIsPaused);
			PhysDispatcher->EnqueueCommandImmediate(SceneSolver, [bIsPaused](Chaos::FPhysicsSolver* Solver)
			{
				Solver->SetPaused(bIsPaused);
			});
		}
	}
#endif
}
#endif  // #if CHAOS_WITH_PAUSABLE_SOLVER

void FPhysScene_Chaos::AddToComponentMaps(UPrimitiveComponent* Component, IPhysicsProxyBase* InObject)
{
	if (Component != nullptr && InObject != nullptr)
	{
		PhysicsProxyToComponentMap.Add(InObject, Component);

		TArray<IPhysicsProxyBase*>* ProxyArray = ComponentToPhysicsProxyMap.Find(Component);
		if (ProxyArray == nullptr)
		{
			TArray<IPhysicsProxyBase*> NewProxyArray;
			NewProxyArray.Add(InObject);
			ComponentToPhysicsProxyMap.Add(Component, NewProxyArray);
		}
		else
		{
			ProxyArray->Add(InObject);
		}
	}
}

void FPhysScene_Chaos::RemoveFromComponentMaps(IPhysicsProxyBase* InObject)
{
	UPrimitiveComponent** const Component = PhysicsProxyToComponentMap.Find(InObject);
	if (Component)
	{
		TArray<IPhysicsProxyBase*>* ProxyArray = ComponentToPhysicsProxyMap.Find(*Component);
		if (ProxyArray)
		{
			ProxyArray->Remove(InObject);
			if (ProxyArray->Num() == 0)
			{
				ComponentToPhysicsProxyMap.Remove(*Component);
			}		
		}
	}

	PhysicsProxyToComponentMap.Remove(InObject);
}

void FPhysScene_Chaos::OnWorldBeginPlay()
{
	Chaos::FPhysicsSolver* Solver = GetSolver();

#if WITH_EDITOR
	const UWorld* WorldPtr = GetOwningWorld();
	const TIndirectArray<FWorldContext>& WorldContexts = GEngine->GetWorldContexts();
	for (const FWorldContext& Context : WorldContexts)
	{
		if (Context.WorldType == EWorldType::Editor)
		{
			UWorld* World = Context.World();
			if (World)
			{
				auto PhysScene = World->GetPhysicsScene();
				if (PhysScene)
				{
					auto InnerSolver = PhysScene->GetSolver();
					if (InnerSolver)
					{
						InnerSolver->SetIsPaused_External(true);
					}
				}
			}
		}
	}
#endif

}

void FPhysScene_Chaos::OnWorldEndPlay()
{
	Chaos::FPhysicsSolver* Solver = GetSolver();

#if WITH_EDITOR
	const UWorld* WorldPtr = GetOwningWorld();
	const TIndirectArray<FWorldContext>& WorldContexts = GEngine->GetWorldContexts();
	for (const FWorldContext& Context : WorldContexts)
	{
		if (Context.WorldType == EWorldType::Editor)
		{
			UWorld* World = Context.World();
			if (World)
			{
				auto PhysScene = World->GetPhysicsScene();
				if (PhysScene)
				{
					auto InnerSolver = PhysScene->GetSolver();
					if (InnerSolver)
					{
						InnerSolver->SetIsPaused_External(false);
					}
				}
			}
		}
	}

	// Mark PIE modified objects dirty - couldn't do this during the run because
	// it's silently ignored
	for(UObject* Obj : PieModifiedObjects)
	{
		Obj->Modify();
	}

	PieModifiedObjects.Reset();
#endif

	PhysicsProxyToComponentMap.Reset();
	ComponentToPhysicsProxyMap.Reset();
}

void FPhysScene_Chaos::AddAggregateToScene(const FPhysicsAggregateHandle& InAggregate)
{

}

void FPhysScene_Chaos::SetOwningWorld(UWorld* InOwningWorld)
{
	Owner = InOwningWorld;

#if WITH_EDITOR
	if (IsOwningWorldEditor())
	{
		GetSolver()->SetIsPaused_External(false);
	}
#endif

}

UWorld* FPhysScene_Chaos::GetOwningWorld()
{
	return Cast<UWorld>(Owner);
}

const UWorld* FPhysScene_Chaos::GetOwningWorld() const
{
	return Cast<const UWorld>(Owner);
}

void FPhysScene_Chaos::RemoveBodyInstanceFromPendingLists_AssumesLocked(FBodyInstance* BodyInstance, int32 SceneType)
{

}

void FPhysScene_Chaos::AddCustomPhysics_AssumesLocked(FBodyInstance* BodyInstance, FCalculateCustomPhysics& CalculateCustomPhysics)
{
	CalculateCustomPhysics.ExecuteIfBound(MDeltaTime, BodyInstance);
}

void FPhysScene_Chaos::AddForce_AssumesLocked(FBodyInstance* BodyInstance, const FVector& Force, bool bAllowSubstepping, bool bAccelChange)
{
	using namespace Chaos;

	FPhysicsActorHandle& Handle = BodyInstance->GetPhysicsActorHandle();
	if (FPhysicsInterface::IsValid(Handle))
	{
		Chaos::FRigidBodyHandle_External& Body_External = Handle->GetGameThreadAPI();
		EObjectStateType ObjectState = Body_External.ObjectState();
		Body_External.SetObjectState(EObjectStateType::Dynamic);

		if (bAccelChange)
		{
			const Chaos::FReal Mass = Body_External.M();
			const Chaos::FVec3 Acceleration = Force * Mass;
			Body_External.AddForce(Acceleration);
		}
		else
		{
			Body_External.AddForce(Force);
		}
	}
}

void FPhysScene_Chaos::AddForceAtPosition_AssumesLocked(FBodyInstance* BodyInstance, const FVector& Force, const FVector& Position, bool bAllowSubstepping, bool bIsLocalForce /*= false*/)
{
	using namespace Chaos;

	FPhysicsActorHandle& Handle = BodyInstance->GetPhysicsActorHandle();
	if (ensure(FPhysicsInterface::IsValid(Handle)))
	{
		Chaos::FRigidBodyHandle_External& Body_External = Handle->GetGameThreadAPI();
		EObjectStateType ObjectState = Body_External.ObjectState();
		const Chaos::FVec3 WorldCOM = FParticleUtilitiesGT::GetCoMWorldPosition(&Body_External);

		Body_External.SetObjectState(EObjectStateType::Dynamic);

		if (bIsLocalForce)
		{
			const Chaos::FRigidTransform3 CurrentTransform = FParticleUtilitiesGT::GetActorWorldTransform(&Body_External);
			const Chaos::FVec3 WorldPosition = CurrentTransform.TransformPosition(Position);
			const Chaos::FVec3 WorldForce = CurrentTransform.TransformVector(Force);
			const Chaos::FVec3 WorldTorque = Chaos::FVec3::CrossProduct(WorldPosition - WorldCOM, WorldForce);
			Body_External.AddForce(WorldForce);
			Body_External.AddTorque(WorldTorque);
		}
		else
		{
			const Chaos::FVec3 WorldTorque = Chaos::FVec3::CrossProduct(Position - WorldCOM, Force);
			Body_External.AddForce(Force);
			Body_External.AddTorque(WorldTorque);
		}
	}
}

void FPhysScene_Chaos::AddRadialForceToBody_AssumesLocked(FBodyInstance* BodyInstance, const FVector& Origin, const float Radius, const float Strength, const uint8 Falloff, bool bAccelChange, bool bAllowSubstepping)
{
	FPhysicsActorHandle& Handle = BodyInstance->GetPhysicsActorHandle();
	if (ensure(FPhysicsInterface::IsValid(Handle)))
	{
		Chaos::FRigidBodyHandle_External& Body_External = Handle->GetGameThreadAPI();
		Chaos::EObjectStateType ObjectState = Body_External.ObjectState();
		if (CHAOS_ENSURE(ObjectState == Chaos::EObjectStateType::Dynamic || ObjectState == Chaos::EObjectStateType::Sleeping))
		{
			const Chaos::FVec3 WorldCOM = Chaos::FParticleUtilitiesGT::GetCoMWorldPosition(&Body_External);

			Chaos::FVec3 Direction = WorldCOM - Origin;
			const Chaos::FReal Distance = Direction.Size();
			if (Distance > Radius)
			{
				return;
			}

			Body_External.SetObjectState(Chaos::EObjectStateType::Dynamic);

			if (Distance < 1e-4)
			{
				Direction = Chaos::FVec3(1, 0, 0);
			}
			else
			{
				Direction = Direction.GetUnsafeNormal();
			}
			Chaos::FVec3 Force(0, 0, 0);
			CHAOS_ENSURE(Falloff < RIF_MAX);
			if (Falloff == ERadialImpulseFalloff::RIF_Constant)
			{
				Force = Strength * Direction;
			}
			if (Falloff == ERadialImpulseFalloff::RIF_Linear)
			{
				Force = (Radius - Distance) / Radius * Strength * Direction;
			}
			if (bAccelChange)
			{
				const Chaos::FReal Mass = Body_External.M();
				const Chaos::FVec3 Acceleration = Force * Mass;
				Body_External.AddForce(Acceleration);
			}
			else
			{
				Body_External.AddForce(Force);
			}
		}
	}
}

void FPhysScene_Chaos::ClearForces_AssumesLocked(FBodyInstance* BodyInstance, bool bAllowSubstepping)
{
	FPhysicsActorHandle& Handle = BodyInstance->GetPhysicsActorHandle();
	if (ensure(FPhysicsInterface::IsValid(Handle)))
	{
		Handle->GetGameThreadAPI().ClearForces();
	}
}

void FPhysScene_Chaos::AddTorque_AssumesLocked(FBodyInstance* BodyInstance, const FVector& Torque, bool bAllowSubstepping, bool bAccelChange)
{
	using namespace Chaos;

	FPhysicsActorHandle& Handle = BodyInstance->GetPhysicsActorHandle();
	if (ensure(FPhysicsInterface::IsValid(Handle)))
	{
		Chaos::FRigidBodyHandle_External& Body_External = Handle->GetGameThreadAPI();
		EObjectStateType ObjectState = Body_External.ObjectState();
		if (CHAOS_ENSURE(ObjectState == EObjectStateType::Dynamic || ObjectState == EObjectStateType::Sleeping))
		{
			if (bAccelChange)
			{
				Body_External.AddTorque(FParticleUtilitiesXR::GetWorldInertia(&Body_External) * Torque);
			}
			else
			{
				Body_External.AddTorque(Torque);
			}
		}
	}
}

void FPhysScene_Chaos::ClearTorques_AssumesLocked(FBodyInstance* BodyInstance, bool bAllowSubstepping)
{
	FPhysicsActorHandle& Handle = BodyInstance->GetPhysicsActorHandle();
	if (ensure(FPhysicsInterface::IsValid(Handle)))
	{
		Handle->GetGameThreadAPI().ClearTorques();
	}
}

void FPhysScene_Chaos::SetKinematicTarget_AssumesLocked(FBodyInstance* BodyInstance, const FTransform& TargetTM, bool bAllowSubstepping)
{
	// #todo : Implement
	//for now just pass it into actor directly
	FPhysInterface_Chaos::SetKinematicTarget_AssumesLocked(BodyInstance->GetPhysicsActorHandle(), TargetTM);
}

bool FPhysScene_Chaos::GetKinematicTarget_AssumesLocked(const FBodyInstance* BodyInstance, FTransform& OutTM) const
{
	OutTM = FPhysicsInterface::GetKinematicTarget_AssumesLocked(BodyInstance->ActorHandle);
	return true;
}


bool FPhysScene_Chaos::MarkForPreSimKinematicUpdate(USkeletalMeshComponent* InSkelComp, ETeleportType InTeleport, bool bNeedsSkinning)
{
#if !UE_BUILD_SHIPPING
	const bool bDeferredUpdate = CVar_ChaosUpdateKinematicsOnDeferredSkelMeshes.GetValueOnGameThread() != 0;
	if (!bDeferredUpdate)
	{
		return false;
	}
#endif

	// If null, or pending kill, do nothing
	if (IsValid(InSkelComp))
	{
		// If we are already flagged, just need to update info
		if (InSkelComp->DeferredKinematicUpdateIndex != INDEX_NONE)
		{
			FDeferredKinematicUpdateInfo& Info = DeferredKinematicUpdateSkelMeshes[InSkelComp->DeferredKinematicUpdateIndex].Value;

			// If we are currently not going to teleport physics, but this update wants to, we 'upgrade' it
			if (Info.TeleportType == ETeleportType::None && InTeleport == ETeleportType::TeleportPhysics)
			{
				Info.TeleportType = ETeleportType::TeleportPhysics;
			}

			// If we need skinning, remember that
			if (bNeedsSkinning)
			{
				Info.bNeedsSkinning = true;
			}
		}
		// We are not flagged yet..
		else
		{
			// Set info and add to map
			FDeferredKinematicUpdateInfo Info;
			Info.TeleportType = InTeleport;
			Info.bNeedsSkinning = bNeedsSkinning;
			InSkelComp->DeferredKinematicUpdateIndex = DeferredKinematicUpdateSkelMeshes.Num();
			DeferredKinematicUpdateSkelMeshes.Emplace(InSkelComp, Info);
		}
	}

	return true;
}

void FPhysScene_Chaos::ClearPreSimKinematicUpdate(USkeletalMeshComponent* InSkelComp)
{
	if (InSkelComp != nullptr)
	{
		const int32 DeferredKinematicUpdateIndex = InSkelComp->DeferredKinematicUpdateIndex;
		if (DeferredKinematicUpdateIndex != INDEX_NONE)
		{
			DeferredKinematicUpdateSkelMeshes.Last().Key->DeferredKinematicUpdateIndex = DeferredKinematicUpdateIndex;
			DeferredKinematicUpdateSkelMeshes.RemoveAtSwap(InSkelComp->DeferredKinematicUpdateIndex);
			InSkelComp->DeferredKinematicUpdateIndex = INDEX_NONE;
		}
	}
}

// Collect all the actors that need moving, along with their transforms
// Extracted from USkeletalMeshComponent::UpdateKinematicBonesToAnim
// @todo(chaos): merge this functionality back into USkeletalMeshComponent
template<typename T_ACTORCONTAINER, typename T_TRANSFORMCONTAINER>
void GatherActorsAndTransforms(
	USkeletalMeshComponent* SkelComp, 
	const TArray<FTransform>& InComponentSpaceTransforms, 
	ETeleportType Teleport, 
	bool bNeedsSkinning, 
	T_ACTORCONTAINER& KinematicUpdateActors,
	T_TRANSFORMCONTAINER& KinematicUpdateTransforms,
	T_ACTORCONTAINER& TeleportActors,
	T_TRANSFORMCONTAINER& TeleportTransforms)
{
	bool bTeleport = Teleport == ETeleportType::TeleportPhysics;
	const UPhysicsAsset* PhysicsAsset = SkelComp->GetPhysicsAsset();
	const FTransform& CurrentLocalToWorld = SkelComp->GetComponentTransform();
	const int32 NumBodies = SkelComp->Bodies.Num();
	for (int32 i = 0; i < NumBodies; i++)
	{
		FBodyInstance* BodyInst = SkelComp->Bodies[i];
		FPhysicsActorHandle& ActorHandle = BodyInst->ActorHandle;
		if (bTeleport || !BodyInst->IsInstanceSimulatingPhysics())
		{
			const int32 BoneIndex = BodyInst->InstanceBoneIndex;
			if (BoneIndex != INDEX_NONE)
			{
				const FTransform BoneTransform = InComponentSpaceTransforms[BoneIndex] * CurrentLocalToWorld;
				if (!bTeleport)
				{
					KinematicUpdateActors.Add(ActorHandle);
					KinematicUpdateTransforms.Add(BoneTransform);
				}
				else
				{
					TeleportActors.Add(ActorHandle);
					TeleportTransforms.Add(BoneTransform);
				}
				if (!PhysicsAsset->SkeletalBodySetups[i]->bSkipScaleFromAnimation)
				{
					const FVector& MeshScale3D = CurrentLocalToWorld.GetScale3D();
					if (MeshScale3D.IsUniform())
					{
						BodyInst->UpdateBodyScale(BoneTransform.GetScale3D());
					}
					else
					{
						BodyInst->UpdateBodyScale(MeshScale3D);
					}
				}
			}
		}
	}
}

// Move all actors that need teleporting
void ProcessTeleportActors(FPhysScene_Chaos& Scene, const TArrayView<FPhysicsActorHandle>& ActorHandles, const TArrayView<FTransform>& Transforms)
{
	int32 NumActors = ActorHandles.Num();
	if (NumActors > 0)
	{
		for (int32 ActorIndex = 0; ActorIndex < NumActors; ++ActorIndex)
		{
			const FPhysicsActorHandle& ActorHandle = ActorHandles[ActorIndex];
			Chaos::FRigidBodyHandle_External& Body_External = ActorHandle->GetGameThreadAPI();
			const FTransform& ActorTransform = Transforms[ActorIndex];
			Body_External.SetX(ActorTransform.GetLocation(), false);	// only set dirty once in SetR
			Body_External.SetR(ActorTransform.GetRotation());
			Body_External.UpdateShapeBounds();
		}

		Scene.UpdateActorsInAccelerationStructure(ActorHandles);
	}
}

// Set all actor kinematic targets
void ProcessKinematicTargetActors(FPhysScene_Chaos& Scene, const TArrayView<FPhysicsActorHandle>& ActorHandles, const TArrayView<FTransform>& Transforms)
{
	// TODO - kinematic targets
	ProcessTeleportActors(Scene, ActorHandles, Transforms);
}

void FPhysScene_Chaos::DeferPhysicsStateCreation(UPrimitiveComponent* Component)
{
	if (Component)
	{
		UBodySetup* Setup = Component->GetBodySetup();
		if (Setup)
		{
			DeferredCreatePhysicsStateComponents.Add(Component);
			Component->DeferredCreatePhysicsStateScene = this;
		}
	}
}

void FPhysScene_Chaos::RemoveDeferredPhysicsStateCreation(UPrimitiveComponent* Component)
{
	DeferredCreatePhysicsStateComponents.Remove(Component);
	Component->DeferredCreatePhysicsStateScene = nullptr;
}

void FPhysScene_Chaos::ProcessDeferredCreatePhysicsState()
{
	SCOPE_CYCLE_COUNTER(STAT_ProcessDeferredCreatePhysicsState)
	TRACE_CPUPROFILER_EVENT_SCOPE(FPhysScene_Chaos::ProcessDeferredCreatePhysicsState)

	// Gather body setups, difficult to gather in advance, as we must be able to remove setups if all components referencing are removed,
	// otherwise risk using a deleted setup. If we can assume a component's bodysetup will not change, can try reference counting setups.
	TSet<UBodySetup*> UniqueBodySetups;
	for (UPrimitiveComponent* PrimitiveComponent : DeferredCreatePhysicsStateComponents)
	{
		if (PrimitiveComponent->ShouldCreatePhysicsState())
		{
			UBodySetup* Setup = PrimitiveComponent->GetBodySetup();
			if (Setup)
			{
				UniqueBodySetups.Add(Setup);
			}
		}
	}

	TArray<UBodySetup*> BodySetups = UniqueBodySetups.Array();
	ParallelFor( TEXT("CreatePhysicsMeshes.PF"), BodySetups.Num(),1, [this, &BodySetups](int32 Index)
	{
		BodySetups[Index]->CreatePhysicsMeshes();
	});

	// TODO explore parallelization of other physics initialization, not trivial and likely to break stuff.
	for (UPrimitiveComponent* PrimitiveComponent : DeferredCreatePhysicsStateComponents)
	{
		const bool bPendingKill = PrimitiveComponent->GetOwner() && !IsValid(PrimitiveComponent->GetOwner());
		if (!bPendingKill && PrimitiveComponent->ShouldCreatePhysicsState() && PrimitiveComponent->IsPhysicsStateCreated() == false)
		{
			PrimitiveComponent->OnCreatePhysicsState();
			PrimitiveComponent->GlobalCreatePhysicsDelegate.Broadcast(PrimitiveComponent);
		}

		PrimitiveComponent->DeferredCreatePhysicsStateScene = nullptr;
	}

	DeferredCreatePhysicsStateComponents.Reset();
}

// Collect the actors and transforms of all the bodies we have to move, and process them in bulk
// to avoid locks in the Spatial Acceleration and the Solver's Dirty Proxy systems.
void FPhysScene_Chaos::UpdateKinematicsOnDeferredSkelMeshes()
{
	SCOPE_CYCLE_COUNTER(STAT_UpdateKinematicsOnDeferredSkelMeshesChaos);

	// Holds start index in actor pool for each skeletal mesh.
	TArray<int32, TInlineAllocator<64>> SkeletalMeshStartIndexArray;

	TArray<FPhysicsActorHandle, TInlineAllocator<64>>TeleportActorsPool;
	TArray<IPhysicsProxyBase*, TInlineAllocator<64>> ProxiesToDirty;

	struct BodyInstanceScalePair
	{
		FBodyInstance* BodyInstance;
		FVector Scale;
	};
	TQueue<BodyInstanceScalePair, EQueueMode::Mpsc> BodiesUpdatingScale;

	
	// Count max number of bodies to determine actor pool size.
	{
		SkeletalMeshStartIndexArray.Reserve(DeferredKinematicUpdateSkelMeshes.Num());

		int32 TotalBodies = 0;
		for (const TPair<USkeletalMeshComponent*, FDeferredKinematicUpdateInfo>& DeferredKinematicUpdate : DeferredKinematicUpdateSkelMeshes)
		{
			SkeletalMeshStartIndexArray.Add(TotalBodies);

			USkeletalMeshComponent* SkelComp = DeferredKinematicUpdate.Key;
			if (!SkelComp->bEnablePerPolyCollision)
			{
				TotalBodies += SkelComp->Bodies.Num();
			}
		}

		// Actors pool is spare, initialize to nullptr.
		TeleportActorsPool.AddZeroed(TotalBodies);
		ProxiesToDirty.Reserve(TotalBodies);
	}

	// Gather proxies that need to be dirtied before paralell loop, and update any per poly collision skeletal meshes.
	{
		for (const TPair<USkeletalMeshComponent*, FDeferredKinematicUpdateInfo>& DeferredKinematicUpdate : DeferredKinematicUpdateSkelMeshes)
		{
			USkeletalMeshComponent* SkelComp = DeferredKinematicUpdate.Key;
			const FDeferredKinematicUpdateInfo& Info = DeferredKinematicUpdate.Value;

			if (!SkelComp->bEnablePerPolyCollision)
			{
				const int32 NumBodies = SkelComp->Bodies.Num();
				for (int32 i = 0; i < NumBodies; i++)
				{
					FBodyInstance* BodyInst = SkelComp->Bodies[i];
					FPhysicsActorHandle& ActorHandle = BodyInst->ActorHandle;
					if (!BodyInst->IsInstanceSimulatingPhysics())
					{
						const int32 BoneIndex = BodyInst->InstanceBoneIndex;
						if (BoneIndex != INDEX_NONE)
						{
							IPhysicsProxyBase* Proxy = ActorHandle;
							if (Proxy && Proxy->GetDirtyIdx() == INDEX_NONE)
							{
								ProxiesToDirty.Add(Proxy);
							}
						}
					}
				}
			}
			else
			{
				// TODO: acceleration for per-poly collision
				SkelComp->UpdateKinematicBonesToAnim(SkelComp->GetComponentSpaceTransforms(), Info.TeleportType, Info.bNeedsSkinning, EAllowKinematicDeferral::DisallowDeferral);
			}
		}
	}

	// Mark all body's proxies as dirty, as this is not threadsafe and cannot be done in parallel loop.
	if (ProxiesToDirty.Num() > 0)
	{
		// Assumes all particles have the same solver, safe for now, maybe not in the future.
		IPhysicsProxyBase* Proxy = ProxiesToDirty[0];
		auto* Solver = Proxy->GetSolver<Chaos::FPhysicsSolverBase>();
		Solver->AddDirtyProxiesUnsafe(ProxiesToDirty);
	}

	{
		Chaos::PhysicsParallelFor(DeferredKinematicUpdateSkelMeshes.Num(), [&](int32 Index)
		{
			const TPair<USkeletalMeshComponent*, FDeferredKinematicUpdateInfo>& DeferredKinematicUpdate = DeferredKinematicUpdateSkelMeshes[Index];
			USkeletalMeshComponent* SkelComp = DeferredKinematicUpdate.Key;
			const FDeferredKinematicUpdateInfo& Info = DeferredKinematicUpdate.Value;

			SkelComp->DeferredKinematicUpdateIndex = INDEX_NONE;

			if (!SkelComp->bEnablePerPolyCollision)
			{
				const UPhysicsAsset* PhysicsAsset = SkelComp->GetPhysicsAsset();
				const FTransform& CurrentLocalToWorld = SkelComp->GetComponentTransform();
				const int32 NumBodies = SkelComp->Bodies.Num();
				const TArray<FTransform>& ComponentSpaceTransforms = SkelComp->GetComponentSpaceTransforms();
				
				const int32 ActorPoolStartIndex = SkeletalMeshStartIndexArray[Index];
				for (int32 i = 0; i < NumBodies; i++)
				{
					FBodyInstance* BodyInst = SkelComp->Bodies[i];
					FPhysicsActorHandle& ActorHandle = BodyInst->ActorHandle;
					Chaos::FRigidBodyHandle_External& Body_External = ActorHandle->GetGameThreadAPI();
					if (!BodyInst->IsInstanceSimulatingPhysics())
					{
						const int32 BoneIndex = BodyInst->InstanceBoneIndex;
						if (BoneIndex != INDEX_NONE)
						{
							const FTransform BoneTransform = ComponentSpaceTransforms[BoneIndex] * CurrentLocalToWorld;

							TeleportActorsPool[ActorPoolStartIndex + i] = ActorHandle;

							// TODO: Kinematic targets. Check Teleport type on FDeferredKinematicUpdateInfo and don't always teleport.
							Body_External.SetX(BoneTransform.GetLocation(), false);	// only set dirty once in SetR
							Body_External.SetR(BoneTransform.GetRotation());
							Body_External.UpdateShapeBounds(BoneTransform);

							if (!PhysicsAsset->SkeletalBodySetups[i]->bSkipScaleFromAnimation)
							{
								const FVector& MeshScale3D = CurrentLocalToWorld.GetScale3D();
								if (MeshScale3D.IsUniform())
								{
									BodiesUpdatingScale.Enqueue(BodyInstanceScalePair({ BodyInst, BoneTransform.GetScale3D() }));
								}
								else
								{
									BodiesUpdatingScale.Enqueue(BodyInstanceScalePair({ BodyInst, MeshScale3D }));
								}
							}
						}
					}
				}
			}
		});
	}

	// Process bodies updating scale
	BodyInstanceScalePair BodyScalePair;
	while(BodiesUpdatingScale.Dequeue(BodyScalePair))
	{
		// TODO: Add optional arg to prevent UpdateBodyScale from updating acceleration structure.
		// We already do this below. May not actually matter.
		BodyScalePair.BodyInstance->UpdateBodyScale(BodyScalePair.Scale);
	}


	UpdateActorsInAccelerationStructure(TeleportActorsPool);

	DeferredKinematicUpdateSkelMeshes.Reset();
}

void FPhysScene_Chaos::AddPendingOnConstraintBreak(FConstraintInstance* ConstraintInstance, int32 SceneType)
{

}

void FPhysScene_Chaos::AddPendingSleepingEvent(FBodyInstance* BI, ESleepEvent SleepEventType, int32 SceneType)
{

}

TArray<FCollisionNotifyInfo>& FPhysScene_Chaos::GetPendingCollisionNotifies(int32 SceneType)
{
	return MNotifies;
}

bool FPhysScene_Chaos::SupportsOriginShifting()
{
	return false;
}

void FPhysScene_Chaos::ApplyWorldOffset(FVector InOffset)
{
	check(InOffset.Size() == 0);
}

float FPhysScene_Chaos::OnStartFrame(float InDeltaTime)
{
	using namespace Chaos;
	float UseDeltaTime = InDeltaTime;

	SCOPE_CYCLE_COUNTER(STAT_Scene_StartFrame);

#if WITH_EDITOR
	if (IsOwningWorldEditor())
	{
		// Ensure editor solver is enabled
		GetSolver()->SetIsPaused_External(false);

		UseDeltaTime = 0.0f;
	}
#endif
	ensure(DeferredCreatePhysicsStateComponents.Num() == 0);

	// CVar determines if this happens before or after phys replication.
	if (GEnableKinematicDeferralStartPhysicsCondition == 0)
	{
		// Update any skeletal meshes that need their bone transforms sent to physics sim
		UpdateKinematicsOnDeferredSkelMeshes();
	}
	
	if (PhysicsReplication)
	{
		PhysicsReplication->Tick(UseDeltaTime);
	}

	// CVar determines if this happens before or after phys replication.
	if (GEnableKinematicDeferralStartPhysicsCondition)
	{
		// Update any skeletal meshes that need their bone transforms sent to physics sim
		UpdateKinematicsOnDeferredSkelMeshes();
	}

	OnPhysScenePreTick.Broadcast(this,UseDeltaTime);
	OnPhysSceneStep.Broadcast(this,UseDeltaTime);

	return UseDeltaTime;
}

bool FPhysScene_Chaos::HandleExecCommands(const TCHAR* Cmd, FOutputDevice* Ar)
{
	return false;
}

void FPhysScene_Chaos::ListAwakeRigidBodies(bool bIncludeKinematic)
{

}

int32 FPhysScene_Chaos::GetNumAwakeBodies() const
{
	int32 Count = 0;
#if TODO_REIMPLEMENT_GET_RIGID_PARTICLES
	uint32 ParticlesSize = Solver->GetRigidParticles().Size();
	for(uint32 ParticleIndex = 0; ParticleIndex < ParticlesSize; ++ParticleIndex)
	{
		if(!(SceneSolver->GetRigidParticles().Disabled(ParticleIndex) || SceneSolver->GetRigidParticles().Sleeping(ParticleIndex)))
		{
			Count++;
		}
	}
#endif
	return Count;
}

void FPhysScene_Chaos::StartAsync()
{

}

bool FPhysScene_Chaos::HasAsyncScene() const
{
	return false;
}

void FPhysScene_Chaos::SetPhysXTreeRebuildRate(int32 RebuildRate)
{

}

void FPhysScene_Chaos::EnsureCollisionTreeIsBuilt(UWorld* World)
{

}

void FPhysScene_Chaos::KillVisualDebugger()
{

}

void FPhysScene_Chaos::OnSyncBodies(Chaos::FPhysicsSolverBase* Solver)
{
	using namespace Chaos;
	TArray<FPhysScenePendingComponentTransform_Chaos> PendingTransforms;
	TSet<FGeometryCollectionPhysicsProxy*> GCProxies;

	FPhysicsCommand::ExecuteWrite(this, [&Solver, &PendingTransforms](FPhysScene* PhysScene)
	{
		auto RigidLambda = [&PhysScene, &PendingTransforms](Chaos::FSingleParticlePhysicsProxy* Proxy)
		{
			FPBDRigidParticle* DirtyParticle = Proxy->GetRigidParticleUnsafe();

			if(FBodyInstance* BodyInstance = FPhysicsUserData::Get<FBodyInstance>(DirtyParticle->UserData()))
			{
				if(BodyInstance->OwnerComponent.IsValid())
				{
					if (Chaos::SyncKinematicOnGameThread == 0 && BodyInstance->IsInstanceSimulatingPhysics() == false)
					{
						return;
					}
					UPrimitiveComponent* OwnerComponent = BodyInstance->OwnerComponent.Get();
					if(OwnerComponent != nullptr)
					{
						bool bPendingMove = false;
							FRigidTransform3 NewTransform(DirtyParticle->X(),DirtyParticle->R());

							if(!NewTransform.EqualsNoScale(OwnerComponent->GetComponentTransform()))
							{
								if (BodyInstance->InstanceBodyIndex == INDEX_NONE)
								{

								bPendingMove = true;
								const FVector MoveBy = NewTransform.GetLocation() - OwnerComponent->GetComponentTransform().GetLocation();
								const FQuat NewRotation = NewTransform.GetRotation();
								PendingTransforms.Add(FPhysScenePendingComponentTransform_Chaos(OwnerComponent,MoveBy,NewRotation,Proxy->GetWakeEvent()));

							}

								PhysScene->UpdateActorInAccelerationStructure(BodyInstance->ActorHandle);
						}

						if(Proxy->GetWakeEvent() != Chaos::EWakeEventEntry::None && !bPendingMove)
						{
							PendingTransforms.Add(FPhysScenePendingComponentTransform_Chaos(OwnerComponent,Proxy->GetWakeEvent()));
						}
						Proxy->ClearEvents();
					}
				}
			}
		};

		auto ConstraintLambda = [&PendingTransforms](FJointConstraintPhysicsProxy* Proxy)
		{
			Chaos::FJointConstraint* Constraint = Proxy->GetConstraint();

			if (Constraint->GetOutputData().bIsBreaking)
			{
				if (FConstraintInstanceBase* ConstraintInstance = (Constraint) ? FPhysicsUserData_Chaos::Get<FConstraintInstanceBase>(Constraint->GetUserData()) : nullptr)
				{
					FConstraintBrokenDelegateWrapper CBD(ConstraintInstance);
					CBD.DispatchOnBroken();
				}

				Constraint->GetOutputData().bIsBreaking = false;
			}

			if (Constraint->GetOutputData().bDriveTargetChanged)
			{
				if (FConstraintInstanceBase* ConstraintInstance = (Constraint) ? FPhysicsUserData_Chaos::Get<FConstraintInstanceBase>(Constraint->GetUserData()) : nullptr)
				{
					FPlasticDeformationDelegateWrapper CPD(ConstraintInstance);
					CPD.DispatchPlasticDeformation();
				}

				Constraint->GetOutputData().bDriveTargetChanged = false;
			}

		};

		Solver->PullPhysicsStateForEachDirtyProxy_External(RigidLambda, ConstraintLambda);

	});

	for (const FPhysScenePendingComponentTransform_Chaos& ComponentTransform : PendingTransforms)
	{
		if (ComponentTransform.OwningComp != nullptr)
		{
			AActor* OwnerPtr = ComponentTransform.OwningComp->GetOwner();

			if (ComponentTransform.bHasValidTransform)
			{
				ComponentTransform.OwningComp->MoveComponent(ComponentTransform.NewTranslation, ComponentTransform.NewRotation, false, NULL, MOVECOMP_SkipPhysicsMove);
			}

			if (IsValid(OwnerPtr))
			{
				OwnerPtr->CheckStillInWorld();
			}
		}

		if (ComponentTransform.OwningComp != nullptr)
		{
			if (ComponentTransform.WakeEvent != Chaos::EWakeEventEntry::None)
			{
				ComponentTransform.OwningComp->DispatchWakeEvents(ComponentTransform.WakeEvent == EWakeEventEntry::Awake ? ESleepEvent::SET_Wakeup : ESleepEvent::SET_Sleep, NAME_None);
			}
		}
	}
}

FPhysicsConstraintHandle 
FPhysScene_Chaos::AddSpringConstraint(const TArray< TPair<FPhysicsActorHandle, FPhysicsActorHandle> >& Constraint)
{
	// #todo : Implement
	return FPhysicsConstraintHandle();
}

void FPhysScene_Chaos::RemoveSpringConstraint(const FPhysicsConstraintHandle& Constraint)
{
	// #todo : Implement
}

FConstraintBrokenDelegateWrapper::FConstraintBrokenDelegateWrapper(FConstraintInstanceBase* ConstraintInstance)
	: OnConstraintBrokenDelegate(ConstraintInstance->OnConstraintBrokenDelegate)
	, ConstraintIndex(ConstraintInstance->ConstraintIndex)
{

}

void FConstraintBrokenDelegateWrapper::DispatchOnBroken()
{
	OnConstraintBrokenDelegate.ExecuteIfBound(ConstraintIndex);
}

FPlasticDeformationDelegateWrapper::FPlasticDeformationDelegateWrapper(FConstraintInstanceBase* ConstraintInstance)
	: OnPlasticDeformationDelegate(ConstraintInstance->OnPlasticDeformationDelegate)
	, ConstraintIndex(ConstraintInstance->ConstraintIndex)
{

}

void FPlasticDeformationDelegateWrapper::DispatchPlasticDeformation()
{
	OnPlasticDeformationDelegate.ExecuteIfBound(ConstraintIndex);
}

void FPhysScene_Chaos::ResimNFrames(const int32 NumFramesRequested)
{
	//needs to run on physics thread from a special location
	//todo: flag solver
#if 0
	QUICK_SCOPE_CYCLE_COUNTER(ResimNFrames);
	using namespace Chaos;
	auto Solver = GetSolver();
	if(FRewindData* RewindData = Solver->GetRewindData())
	{
		const int32 FramesSaved = RewindData->GetFramesSaved() - 2;	//give 2 frames buffer because right at edge we have a hard time
		const int32 NumFrames = FMath::Min(NumFramesRequested,FramesSaved);
		if(NumFrames > 0)
		{
			const int32 LatestFrame = RewindData->CurrentFrame();
			const int32 FirstFrame = LatestFrame - NumFrames;
			if(ensure(Solver->GetRewindData()->RewindToFrame(FirstFrame)))
			{
				//resim as single threaded
				const auto PreThreading = Solver->GetThreadingMode();
				Solver->SetThreadingMode_External(EThreadingModeTemp::SingleThread);
				for(int Frame = FirstFrame; Frame < LatestFrame; ++Frame)
				{
					Solver->AdvanceAndDispatch_External(RewindData->GetDeltaTimeForFrame(Frame));
					Solver->UpdateGameThreadStructures();
				}

				Solver->SetThreadingMode_External(PreThreading);

#if !UE_BUILD_SHIPPING
				const TArray<FDesyncedParticleInfo> DesyncedParticles = Solver->GetRewindData()->ComputeDesyncInfo();
				if(DesyncedParticles.Num())
				{
					UE_LOG(LogChaos,Log,TEXT("Resim had %d desyncs"),DesyncedParticles.Num());
					for(const FDesyncedParticleInfo& Info : DesyncedParticles)
					{
						const FBodyInstance* BI = FPhysicsUserData_Chaos::Get<FBodyInstance>(Info.Particle->UserData());
						const FBox Bounds = BI->GetBodyBounds();
						FVector Center,Extents;
						Bounds.GetCenterAndExtents(Center,Extents);
						DrawDebugBox(GetOwningWorld(),Center,Extents,FQuat::Identity, Info.MostDesynced == ESyncState::HardDesync ? FColor::Red : FColor::Yellow, /*bPersistentLines=*/ false, /*LifeTime=*/ 3);
					}
				}
#endif
			}
		}
	}
#endif
}

void FPhysScene_Chaos::EnableAsyncPhysicsTickCallback()
{
	if (AsyncPhysicsTickCallback == nullptr)
	{
		AsyncPhysicsTickCallback = SceneSolver->CreateAndRegisterSimCallbackObject_External<FAsyncPhysicsTickCallback>();
	}
}

void FPhysScene_Chaos::RegisterAsyncPhysicsTickComponent(UActorComponent* Component)
{
	EnableAsyncPhysicsTickCallback();
	AsyncPhysicsTickCallback->AsyncPhysicsTickComponents.Add(Component);
}

void FPhysScene_Chaos::UnregisterAsyncPhysicsTickComponent(UActorComponent* Component)
{
	if (AsyncPhysicsTickCallback)
	{
		AsyncPhysicsTickCallback->AsyncPhysicsTickComponents.Remove(Component);
	}
}

void FPhysScene_Chaos::RegisterAsyncPhysicsTickActor(AActor* Actor)
{
	EnableAsyncPhysicsTickCallback();
	AsyncPhysicsTickCallback->AsyncPhysicsTickActors.Add(Actor);
}

void FPhysScene_Chaos::UnregisterAsyncPhysicsTickActor(AActor* Actor)
{
	if (AsyncPhysicsTickCallback)
	{
		AsyncPhysicsTickCallback->AsyncPhysicsTickActors.Remove(Actor);
	}
}

void FPhysScene_Chaos::EnqueueAsyncPhysicsCommand(int32 PhysicsStep, UObject* OwningObject, const TFunction<void()>& Command)
{
	EnableAsyncPhysicsTickCallback();
	AsyncPhysicsTickCallback->PendingCommands.Add({ PhysicsStep, TWeakObjectPtr<UObject>(OwningObject), Command });
}

TSharedPtr<IPhysicsReplicationFactory> FPhysScene_Chaos::PhysicsReplicationFactory;

