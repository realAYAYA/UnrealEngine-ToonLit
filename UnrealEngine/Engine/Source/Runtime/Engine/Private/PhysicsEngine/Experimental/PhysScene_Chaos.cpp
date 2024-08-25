// Copyright Epic Games, Inc. All Rights Reserved.

#include "Physics/Experimental/PhysScene_Chaos.h"



#include "Chaos/PBDJointConstraintData.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Chaos/Particle/ParticleUtilities.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "Components/SkeletalMeshComponent.h"
#include "Physics/PhysicsInterfaceUtils.h"
#include "PhysicsReplication.h"
#include "PhysicsEngine/ClusterUnionComponent.h"
#include "PhysicsEngine/ConstraintInstance.h"
#include "PhysicsEngine/PhysicsCollisionHandler.h"
#include "PhysicsEngine/PhysicsObjectExternalInterface.h"
#include "Chaos/PhysicsObjectInternalInterface.h"
#include "Physics/Experimental/ChaosEventRelay.h"
#include "EngineUtils.h"

#include "ChaosSolversModule.h"

#include "PhysicsProxy/ClusterUnionPhysicsProxy.h"
#include "PhysicsProxy/SkeletalMeshPhysicsProxy.h"
#include "PhysicsProxy/StaticMeshPhysicsProxy.h"
#include "Chaos/PendingSpatialData.h"
#include "Chaos/PhysicsSolverBaseImpl.h"
#include "Misc/CoreMisc.h"

#if WITH_EDITOR
#include "Editor.h"
#else
#include "Engine/Engine.h"
#endif

#if !UE_BUILD_SHIPPING
#include "DrawDebugHelpers.h"
#include "UObject/UObjectIterator.h"

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

bool GKinematicDeferralCheckValidBodies = true;
FAutoConsoleVariableRef CVar_KinematicDeferralCheckValidBodies(TEXT("p.KinematicDeferralCheckValidBodies"), GKinematicDeferralCheckValidBodies, TEXT("If true, don't attempt to update deferred kinematic skeletal mesh bodies which are pending delete."));

bool GKinematicDeferralUpdateExternalAccelerationStructure = false;
FAutoConsoleVariableRef CVar_KinematicDeferralUpdateExternalAccelerationStructure(TEXT("p.KinematicDeferralUpdateExternalAccelerationStructure"), GKinematicDeferralUpdateExternalAccelerationStructure, TEXT("If true, process any operations in PendingSpatialOperations_External before doing deferred kinematic updates."));
bool GKinematicDeferralLogInvalidBodies = false;
FAutoConsoleVariableRef CVar_KinematicDeferralLogInvalidBodies(TEXT("p.KinematicDeferralLogInvalidBodies"), GKinematicDeferralLogInvalidBodies, TEXT("If true and p.KinematicDeferralCheckValidBodies is true, log when an invalid body is found on kinematic update."));

float GReplicationCacheLingerForNSeconds = 3.f;
FAutoConsoleVariableRef CVar_ReplicationCacheLingerForNSeconds(TEXT("np2.ReplicationCache.LingerForNSeconds"), GReplicationCacheLingerForNSeconds, TEXT("How long to keep data in the replication cache without the actor accessing it, after this we stop caching the actors state until it tries to access it again."));

bool bGClusterUnionSyncBodiesMoveNewComponents = true;
FAutoConsoleVariableRef CVar_GClusterUnionSyncBodiesCheckDirtyFlag(TEXT("p.ClusterUnion.SyncBodiesMoveNewComponents"), bGClusterUnionSyncBodiesMoveNewComponents, TEXT("Enable a fix to ensure new components in a cluster union are moved once on add (even if the cluster is not moving)."));

DECLARE_CYCLE_STAT(TEXT("Update Kinematics On Deferred SkelMeshes"), STAT_UpdateKinematicsOnDeferredSkelMeshesChaos, STATGROUP_Physics);

struct FPendingAsyncPhysicsCommand
{
	int32 PhysicsStep;
	TWeakObjectPtr<UObject> OwningObject;
	TFunction<void()> Command;
	bool bEnableResim = true;
};

class FPhysSceneExecHandler : public FSelfRegisteringExec
{
	bool Exec_Runtime(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override
	{
#if !UE_BUILD_SHIPPING
		if(FParse::Command(&Cmd, TEXT("LIST SQ")) && InWorld)
		{
			FPhysScene_Chaos* Scene = InWorld->GetPhysicsScene();

			if(!Scene)
			{
				return false;
			}

			Ar.Logf(TEXT("----- Begin SQ Listing ----------------------------------------"));
			Ar.Logf(TEXT("----- World SQ         ----------------------------------------"));
			Ar.Logf(TEXT("SQ Data for world %s"), *InWorld->GetName());
			Scene->GetSpacialAcceleration()->DumpStatsTo(Ar);

			Ar.Logf(TEXT("----- Cluster Union SQ ----------------------------------------"));

			for(TObjectIterator<UClusterUnionComponent> Iter; Iter; ++Iter)
			{
				UClusterUnionComponent* Union = *Iter;
				if(Union->GetWorld() != InWorld)
				{
					continue;
				}

				Ar.Logf(TEXT("Inner Acceleration data for Cluster Union Component %s"), *Union->GetName());
				if(Union->GetSpatialAcceleration())
				{
					Union->GetSpatialAcceleration()->DumpStatsTo(Ar);
				}
				Ar.Logf(TEXT(""));
			}

			Ar.Logf(TEXT("----- End SQ Listing   ----------------------------------------"));

			return true;
		}
#endif

		return false;
	}
};
static FPhysSceneExecHandler GPhysSceneExecHandler;

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

		const UPhysicsSettings* PhysicsSettings = UPhysicsSettings::Get();
		const bool bAllowResim = PhysicsSettings->PhysicsPrediction.bEnablePhysicsPrediction;
		const int32 NumFrames = PhysicsSettings->GetPhysicsHistoryCount();

		TArray<int32> CommandIndicesToRemove;
		CommandIndicesToRemove.Reserve(PendingCommands.Num());

		for (int32 Idx = 0; Idx < PendingCommands.Num(); ++Idx)
		{
			const int32 CurrentFrame = static_cast<FPBDRigidsSolver*>(GetSolver())->GetCurrentFrame();

			const FPendingAsyncPhysicsCommand& PendingCommand = PendingCommands[Idx];
			bool bRemove = PendingCommand.OwningObject.IsStale() || !PendingCommand.Command;

			/* #TODO implement and re-enable resim commands. This callback must run on the main thread and resim currently does
			 * not defer its callbacks to the main thread making its execution unsafe. 

			if (!bRemove && bAllowResim && PendingCommand.bEnableResim && PendingCommand.PhysicsStep > (CurrentFrame - NumFrames))
			{
				if (PendingCommand.PhysicsStep < CurrentFrame)
				{
					if (Chaos::FRewindData* RewindData = GetSolver()->GetRewindData())
					{
						int32 ResimFrame = RewindData->GetResimFrame();
						ResimFrame = (ResimFrame == INDEX_NONE) ? PendingCommand.PhysicsStep :
							FMath::Min(ResimFrame, PendingCommand.PhysicsStep);

						RewindData->SetResimFrame(ResimFrame);
					}
				}
				else if (PendingCommand.PhysicsStep == CurrentFrame)
				{
					PendingCommand.Command();
					bRemove = true;
				}
			}
			else
			*/
			{
				if (!bRemove && PendingCommand.PhysicsStep <= CurrentFrame)
				{
					PendingCommand.Command();
					bRemove = true;
				}
			}

			if (bRemove)
			{
				CommandIndicesToRemove.Add(Idx);
			}
		}

		RemoveArrayItemsAtSortedIndices(PendingCommands, CommandIndicesToRemove);

		const FReal DeltaTime = GetDeltaTime_Internal();
		const FReal SimTime = GetSimTime_Internal();
		//TODO: handle case where callbacks modify AsyncPhysicsTickComponents or AsyncPhysicsTickActors
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_AsyncPhys_TickComponents);
			for(UActorComponent* Component : AsyncPhysicsTickComponents)
			{
				FScopeCycleCounterUObject ComponentScope(Component);
				Component->AsyncPhysicsTickComponent(DeltaTime, SimTime);
			}
		}

		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_AsyncPhys_TickActors);
			for(AActor* Actor : AsyncPhysicsTickActors)
			{
				FScopeCycleCounterUObject ActorScope(Actor);
				Actor->AsyncPhysicsTickActor(DeltaTime, SimTime);
			}
		}
	}

	virtual FName GetFNameForStatId() const override
	{
		const static FLazyName StaticName("FAsyncPhysicsTickCallback");
		return StaticName;
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
	ToParticles.SetX(ToIndex, FromParticles.GetX(FromIndex));
	ToParticles.SetR(ToIndex, FromParticles.GetR(FromIndex));
	ToParticles.SetV(ToIndex, FromParticles.GetV(FromIndex));
	ToParticles.SetW(ToIndex, FromParticles.GetW(FromIndex));
	ToParticles.M(ToIndex) = FromParticles.M(FromIndex);
	ToParticles.InvM(ToIndex) = FromParticles.InvM(FromIndex);
	ToParticles.I(ToIndex) = FromParticles.I(FromIndex);
	ToParticles.InvI(ToIndex) = FromParticles.InvI(FromIndex);
	ToParticles.SetGeometry(ToIndex, FromParticles.GetGeometry(FromIndex));	//question: do we need to deal with dynamic geometry?
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
	, LastBreakEventDispatchTime(Chaos::FReal(-1))
	, LastRemovalEventDispatchTime(Chaos::FReal(-1))
	, LastCrumblingEventDispatchTime(Chaos::FReal(-1))
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
	EventManager->RegisterHandler<Chaos::FBreakingEventData>(Chaos::EEventType::Breaking, this, &FPhysScene_Chaos::HandleBreakingEvents);
	EventManager->RegisterHandler<Chaos::FRemovalEventData>(Chaos::EEventType::Removal, this, &FPhysScene_Chaos::HandleRemovalEvents);
	EventManager->RegisterHandler<Chaos::FCrumblingEventData>(Chaos::EEventType::Crumbling, this, &FPhysScene_Chaos::HandleCrumblingEvents);


	//Initialize unique ptrs that are just here to allow forward declare. This should be reworked todo(ocohen)
#if TODO_FIX_REFERENCES_TO_ADDARRAY
	BodyInstances = MakeUnique<Chaos::TArrayCollectionArray<FBodyInstance*>>();
	Scene.GetSolver()->GetEvolution()->GetParticles().AddArray(BodyInstances.Get());
#endif

	// Create replication manager
	CreatePhysicsReplication();

	FPhysicsDelegates::OnPhysSceneInit.Broadcast(this);

	ChaosEventRelay = NewObject<UChaosEventRelay>();
}

FPhysScene_Chaos::~FPhysScene_Chaos()
{
	if (AsyncPhysicsTickCallback)
	{
		SceneSolver->UnregisterAndFreeSimCallbackObject_External(AsyncPhysicsTickCallback);
	}

	// Must ensure deferred components do not hold onto scene pointer.
	ProcessDeferredCreatePhysicsState();
	
	// Make sure physics replication is cleared before we're fully destructed
	PhysicsReplication.Reset();
	ReplicationCache.Reset();

	FPhysicsDelegates::OnPhysSceneTerm.Broadcast(this);

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
	CollisionEventRegistrations.Add(Component);
}

void FPhysScene_Chaos::UnRegisterForCollisionEvents(UPrimitiveComponent* Component)
{
	CollisionEventRegistrations.Remove(Component);
}

void FPhysScene_Chaos::RegisterForGlobalCollisionEvents(UPrimitiveComponent* Component)
{
	GlobalCollisionEventRegistrations.Add(Component);
}

void FPhysScene_Chaos::UnRegisterForGlobalCollisionEvents(UPrimitiveComponent* Component)
{
	GlobalCollisionEventRegistrations.Remove(Component);
}

void FPhysScene_Chaos::RegisterForGlobalRemovalEvents(UPrimitiveComponent* Component)
{
	GlobalRemovalEventRegistrations.Add(Component);
}

void FPhysScene_Chaos::UnRegisterForGlobalRemovalEvents(UPrimitiveComponent* Component)
{
	GlobalRemovalEventRegistrations.Remove(Component);
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

void FPhysScene_Chaos::AddObject(UPrimitiveComponent* Component, Chaos::FClusterUnionPhysicsProxy* InObject)
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

	for (TUniquePtr<Chaos::TPBDRigidParticle<Chaos::FReal, 3>>& GTParticleUnique : InObject->GetUnorderedParticles_External())
	{
		Chaos::TPBDRigidParticle<Chaos::FReal, 3>* GTParticle = GTParticleUnique.Get();
		if (GTParticle != nullptr)
		{
			RemoveActorFromAccelerationStructureImp(GTParticle);
			if (Solver)
			{
				Solver->UpdateParticleInAccelerationStructure_External(GTParticle,Chaos::EPendingSpatialDataOperation::Delete); // ensures deletion will be applied to structures passed from physics thread.
			}
		}
	}

	if(Solver)
	{
		Solver->UnregisterObject(InObject);
	}

	RemoveFromComponentMaps(InObject);
}

void FPhysScene_Chaos::RemoveObject(Chaos::FClusterUnionPhysicsProxy* InObject)
{
	Chaos::FPhysicsSolver* Solver = InObject->GetSolver<Chaos::FPhysicsSolver>();

	RemoveActorFromAccelerationStructureImp(InObject->GetParticle_External());

	if (Solver)
	{
		Solver->UpdateParticleInAccelerationStructure_External(InObject->GetParticle_External(), Chaos::EPendingSpatialDataOperation::Delete);
		Solver->UnregisterObject(InObject);
	}

	RemoveFromComponentMaps(InObject);
}

IPhysicsReplication* FPhysScene_Chaos::GetPhysicsReplication()
{
	return PhysicsReplication.Get();
}

IPhysicsReplication* FPhysScene_Chaos::CreatePhysicsReplication()
{
	// Create replication manager
	PhysicsReplication
		= PhysicsReplicationFactory.IsValid()
		? PhysicsReplicationFactory->CreatePhysicsReplication(this)
		: MakeUnique<FPhysicsReplication>(this);

	// Return ptr to the new physics rep
	return PhysicsReplication.Get();
}

void FPhysScene_Chaos::SetPhysicsReplication(IPhysicsReplication* InPhysicsReplication)
{
	PhysicsReplication = TUniquePtr<IPhysicsReplication>(InPhysicsReplication);
}

void FPhysScene_Chaos::AddReferencedObjects(FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(Collector);
	Collector.AddReferencedObject(ChaosEventRelay);
#if WITH_EDITOR

	for (auto& Pair : PhysicsProxyToComponentMap)
	{
		Collector.AddReferencedObject(Pair.Get<1>());
	}
#endif
}

template<>
UPrimitiveComponent* FPhysScene_Chaos::GetOwningComponent(const IPhysicsProxyBase* PhysicsProxy) const
{
	if (const TObjectPtr<UPrimitiveComponent>* FoundComp = PhysicsProxyToComponentMap.Find(PhysicsProxy))
	{
		return *FoundComp;
	}
	return nullptr;
}

FBodyInstance* FPhysScene_Chaos::GetBodyInstanceFromProxy(const IPhysicsProxyBase* PhysicsProxy) const
{
	FBodyInstance* BodyInstance = nullptr;
	if (PhysicsProxy)
	{
		if (PhysicsProxy->GetType() == EPhysicsProxyType::SingleParticleProxy)
		{
			const Chaos::FRigidBodyHandle_External& RigidBodyHandle = static_cast<const Chaos::FSingleParticlePhysicsProxy*>(PhysicsProxy)->GetGameThreadAPI();
			BodyInstance = ChaosInterface::GetUserData(*(static_cast<const Chaos::FSingleParticlePhysicsProxy*>(PhysicsProxy)->GetParticle_LowLevel()));
		}
		// found none, let's see if there's an owning component in the scene
		if (BodyInstance == nullptr)
		{
			if (UPrimitiveComponent* const OwningComponent = GetOwningComponent<UPrimitiveComponent>(PhysicsProxy))
			{
				BodyInstance = OwningComponent->GetBodyInstance();
			}
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

FCollisionNotifyInfo& FPhysScene_Chaos::GetPendingCollisionForContactPair(const void* P0, const void* P1, Chaos::FReal SolverTime, bool& bNewEntry)
{
	const FUniqueContactPairKey Key = { P0, P1 };
	for(TMultiMap<FUniqueContactPairKey, int32>::TConstKeyIterator It = ContactPairToPendingNotifyMap.CreateConstKeyIterator(Key); It; ++It)
	{
		int32 ExistingIdx = It.Value();
		if(FMath::IsNearlyEqual(PendingCollisionNotifies[ExistingIdx].SolverTime, SolverTime))
		{
			// we already have one for this pair
			bNewEntry = false;
			return PendingCollisionNotifies[ExistingIdx];
		}
	}

	// make a new entry
	bNewEntry = true;
	int32 NewIdx = PendingCollisionNotifies.AddZeroed();
	ContactPairToPendingNotifyMap.Add(Key, NewIdx);
	return PendingCollisionNotifies[NewIdx];
}

FORCEINLINE void FPhysScene_Chaos::HandleEachCollisionEvent(const TArray<int32>& CollisionIndices, IPhysicsProxyBase* PhysicsProxy0, UPrimitiveComponent* const Comp0, Chaos::FCollisionDataArray const& CollisionData, Chaos::FReal MinDeltaVelocityThreshold)
{
	for (int32 EncodedCollisionIdx : CollisionIndices)
	{
		bool bSwapOrder;
		int32 CollisionIdx = Chaos::FEventManager::DecodeCollisionIndex(EncodedCollisionIdx, bSwapOrder);

		Chaos::FCollidingData const& CollisionDataItem = CollisionData[CollisionIdx];
		IPhysicsProxyBase* const PhysicsProxy1 = bSwapOrder ? CollisionDataItem.Proxy1 : CollisionDataItem.Proxy2;

		if (PhysicsProxy1 == nullptr)
		{
			continue;
		}
		// Are the proxies pending destruction? If they are no longer tracked by the PhysScene, the proxy is deleted or pending deletion.
		UPrimitiveComponent* const Comp1 = GetOwningComponent<UPrimitiveComponent>(PhysicsProxy1);
		if (Comp1 == nullptr)
		{
			continue;
		}

		bool bNewEntry = false;
		FCollisionNotifyInfo& NotifyInfo = GetPendingCollisionForContactPair(PhysicsProxy0, PhysicsProxy1, CollisionDataItem.SolverTime, bNewEntry);

		// #note: we only notify on the first contact, though we will still accumulate the impulse data from subsequent contacts
		const FVector NormalImpulse = FVector::DotProduct(CollisionDataItem.AccumulatedImpulse, CollisionDataItem.Normal) * CollisionDataItem.Normal;	// project impulse along normal
		const FVector FrictionImpulse = FVector(CollisionDataItem.AccumulatedImpulse) - NormalImpulse; // friction is component not along contact normal
		NotifyInfo.RigidCollisionData.TotalNormalImpulse += NormalImpulse;
		NotifyInfo.RigidCollisionData.TotalFrictionImpulse += FrictionImpulse;

		// Get swapped velocity deltas
		const FVector& DeltaVelocity1 = bSwapOrder
			? CollisionDataItem.DeltaVelocity2
			: CollisionDataItem.DeltaVelocity1;
		const FVector& DeltaVelocity2 = bSwapOrder
			? CollisionDataItem.DeltaVelocity1
			: CollisionDataItem.DeltaVelocity2;

		// Populate additional contact information for additional hits
		FRigidBodyContactInfo& NewContact = NotifyInfo.RigidCollisionData.ContactInfos.AddZeroed_GetRef();
		NewContact.ContactNormal = CollisionDataItem.Normal;
		NewContact.ContactPosition = CollisionDataItem.Location;
		NewContact.ContactPenetration = CollisionDataItem.PenetrationDepth;
		NewContact.bContactProbe = CollisionDataItem.bProbe;
		NotifyInfo.RigidCollisionData.bIsVelocityDeltaUnderThreshold =
			DeltaVelocity1.IsNearlyZero(MinDeltaVelocityThreshold) &&
			DeltaVelocity2.IsNearlyZero(MinDeltaVelocityThreshold);

		const Chaos::FChaosPhysicsMaterial* InternalMat1 = CollisionDataItem.Mat1.Get();
		const Chaos::FChaosPhysicsMaterial* InternalMat2 = CollisionDataItem.Mat2.Get();

		NewContact.PhysMaterial[0] = InternalMat1 ? FPhysicsUserData::Get<UPhysicalMaterial>(InternalMat1->UserData) : nullptr;
		NewContact.PhysMaterial[1] = InternalMat2 ? FPhysicsUserData::Get<UPhysicalMaterial>(InternalMat2->UserData) : nullptr;

		if (bSwapOrder)
		{
			NewContact.SwapOrder();
		}

		if (bNewEntry)
		{
			// fill in legacy contact data
			NotifyInfo.bCallEvent0 = true;
			// if Comp1 wants this event too, it will get its own pending collision entry, so we leave it false

			NotifyInfo.SolverTime = CollisionDataItem.SolverTime;

			NotifyInfo.Info0.SetFrom(GetBodyInstanceFromProxyAndShape(PhysicsProxy0, CollisionDataItem.ShapeIndex1), DeltaVelocity1);
			NotifyInfo.Info1.SetFrom(GetBodyInstanceFromProxyAndShape(PhysicsProxy1, CollisionDataItem.ShapeIndex2), DeltaVelocity2);

			// in some case ( like with geometry collections ) we don't have a body instance so the component part will null, we need to handle that 
			if (NotifyInfo.Info0.Component == nullptr)
			{
				NotifyInfo.Info0.Component = Comp0;
				check(Comp0);
				NotifyInfo.Info0.Actor = Comp0->GetOwner();
			}
			if (NotifyInfo.Info1.Component == nullptr)
			{
				NotifyInfo.Info1.Component = Comp1;
				check(Comp1);
				NotifyInfo.Info1.Actor = Comp1->GetOwner();
			}
		}
	}
}

void FPhysScene_Chaos::HandleCollisionEvents(const Chaos::FCollisionEventData& Event)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_HandleCollisionEvents);
	ContactPairToPendingNotifyMap.Reset();

	TMap<IPhysicsProxyBase*, TArray<int32>> const& PhysicsProxyToCollisionIndicesMap = Event.PhysicsProxyToCollisionIndices.PhysicsProxyToIndicesMap;
	Chaos::FCollisionDataArray const& CollisionData = Event.CollisionData.AllCollisionsArray;
	const Chaos::FReal MinDeltaVelocityThreshold = UPhysicsSettings::Get()->MinDeltaVelocityForHitEvents;
	int32 NumCollisions = CollisionData.Num();

	if (NumCollisions > 0 && LastEventDispatchTime < Event.CollisionData.TimeCreated)
	{
		LastEventDispatchTime = Event.CollisionData.TimeCreated;

		// Iterate through the smallest between events registration and physics proxies
		if (PhysicsProxyToCollisionIndicesMap.Num() <= CollisionEventRegistrations.Num())
		{
			for (TPair<IPhysicsProxyBase*, TArray<int32>> Pair : PhysicsProxyToCollisionIndicesMap)
			{
				IPhysicsProxyBase*  PhysicsProxy0 = Pair.Key;
				const TArray<int32>& CollisionIndices = Pair.Value;

				UPrimitiveComponent* Comp0 = GetOwningComponent<UPrimitiveComponent>(PhysicsProxy0);

				if (Comp0 != nullptr && CollisionEventRegistrations.Contains(Comp0))
				{
					HandleEachCollisionEvent(CollisionIndices, PhysicsProxy0, Comp0, CollisionData, MinDeltaVelocityThreshold);
				}
			}
		}
		else
		{
			// look through all the components that someone is interested in, and see if they had a collision
			// note that we only need to care about the interaction from the POV of the registered component,
			// since if anyone wants notifications for the other component it hit, it's also registered and we'll get to that elsewhere in the list
			for (UPrimitiveComponent* const Comp0 : CollisionEventRegistrations)
			{
				const TArray<IPhysicsProxyBase*>* PhysicsProxyArray = GetOwnedPhysicsProxies(Comp0);

				if (PhysicsProxyArray)
				{
					for (IPhysicsProxyBase* PhysicsProxy0 : *PhysicsProxyArray)
					{
						TArray<int32> const* const CollisionIndices = PhysicsProxyToCollisionIndicesMap.Find(PhysicsProxy0);
						if (CollisionIndices)
						{
							HandleEachCollisionEvent(*CollisionIndices, PhysicsProxy0, Comp0, CollisionData, MinDeltaVelocityThreshold);
						}
					}
				}
			}
		}
	}

	// Tell the world and actors about the collisions
	DispatchPendingCollisionNotifies();

	HandleGlobalCollisionEvent(CollisionData);

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
		if (NotifyInfo.bCallEvent0)
		{
			if (AActor* Actor = NotifyInfo.Info0.Actor.Get())
			{
				QUICK_SCOPE_CYCLE_COUNTER(STAT_DispatchPhysicsCollisionHit);
				SCOPE_CYCLE_UOBJECT(NotifyActor, Actor);
				Actor->DispatchPhysicsCollisionHit(NotifyInfo.Info0, NotifyInfo.Info1, NotifyInfo.RigidCollisionData);
			}
		}
	}
	PendingCollisionNotifies.Reset();
}

void FPhysScene_Chaos::HandleGlobalCollisionEvent(Chaos::FCollisionDataArray const& CollisionData)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_HandleGlobalCollisionEvents);
	TArray<FCollisionChaosEvent> CollisionEvents;
	CollisionEvents.Reserve(CollisionData.Num());

	// If iterating by proxy, then need to process the duplicate collision, 
	// so iterating by collision data should be the faster when using only globals
	for (const Chaos::FCollidingData& CollisionItem : CollisionData)
	{
		UPrimitiveComponent* BodyPrimitive1 = GetOwningComponent<UPrimitiveComponent>(CollisionItem.Proxy1);
		UPrimitiveComponent* BodyPrimitive2 = GetOwningComponent<UPrimitiveComponent>(CollisionItem.Proxy2);

		if (GlobalCollisionEventRegistrations.Contains(BodyPrimitive1) || GlobalCollisionEventRegistrations.Contains(BodyPrimitive2))
		{
			FCollisionChaosEvent& CollisionEvent = CollisionEvents.Emplace_GetRef(CollisionItem);
			CollisionEvent.Body1.Component = BodyPrimitive1;
			CollisionEvent.Body2.Component = BodyPrimitive2;

			const Chaos::FChaosPhysicsMaterial* InternalMat1 = CollisionItem.Mat1.Get();
			const Chaos::FChaosPhysicsMaterial* InternalMat2 = CollisionItem.Mat2.Get();
			CollisionEvent.Body1.PhysMaterial = InternalMat1 ? FPhysicsUserData::Get<UPhysicalMaterial>(InternalMat1->UserData) : nullptr;
			CollisionEvent.Body2.PhysMaterial = InternalMat2 ? FPhysicsUserData::Get<UPhysicalMaterial>(InternalMat2->UserData) : nullptr;
			
			if (BodyPrimitive1 && CollisionItem.Proxy1)
			{
				const FBodyInstance* BodyInst1 = GetBodyInstanceFromProxyAndShape(CollisionItem.Proxy1, CollisionItem.ShapeIndex1);
				if (BodyInst1 != nullptr)
				{
					CollisionEvent.Body1.BodyIndex = BodyInst1->InstanceBodyIndex;
					if (UBodySetupCore* BodySetup1 = BodyInst1->BodySetup.Get())
					{
						CollisionEvent.Body1.BoneName = BodySetup1->BoneName;
					}
					else
					{
						CollisionEvent.Body1.BoneName = NAME_None;
					}
				}
				else if (CollisionItem.Proxy1->GetType() == EPhysicsProxyType::ClusterUnionProxy)
				{
					CollisionEvent.Body1.BodyIndex = CollisionItem.ShapeIndex1;
				}
			}

			if (BodyPrimitive2 && CollisionItem.Proxy2)
			{
				const FBodyInstance* BodyInst2 = GetBodyInstanceFromProxyAndShape(CollisionItem.Proxy2, CollisionItem.ShapeIndex2);
				if (BodyInst2 != nullptr)
				{
					CollisionEvent.Body2.BodyIndex = BodyInst2->InstanceBodyIndex;
					if (UBodySetupCore* BodySetup2 = BodyInst2->BodySetup.Get())
					{
						CollisionEvent.Body2.BoneName = BodySetup2->BoneName;
					}
					else
					{
						CollisionEvent.Body2.BoneName = NAME_None;
					}
				}
				else if (CollisionItem.Proxy2->GetType() == EPhysicsProxyType::ClusterUnionProxy)
				{
					CollisionEvent.Body2.BodyIndex = CollisionItem.ShapeIndex2;
				}
			}
		}
	}
	if (CollisionEvents.Num() > 0)
	{
		ChaosEventRelay->DispatchPhysicsCollisionEvents(CollisionEvents);
	}
}

void FPhysScene_Chaos::HandleBreakingEvents(const Chaos::FBreakingEventData& Event)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_HandleGlobalBreakingEvents);

	if (Event.BreakingData.bHasGlobalEvent && LastBreakEventDispatchTime < Event.BreakingData.TimeCreated)
	{
		LastBreakEventDispatchTime = Event.BreakingData.TimeCreated;
		Chaos::FBreakingDataArray const& BreakingDataArray = Event.BreakingData.AllBreakingsArray;
		TArray<FChaosBreakEvent> PendingBreakEvents;
		for (const Chaos::FBreakingData& BreakingData: BreakingDataArray)
		{
			if ((BreakingData.EmitterFlag & Chaos::EventEmitterFlag::GlobalDispatcher) && BreakingData.Proxy)
			{
				UPrimitiveComponent* Comp = GetOwningComponent<UPrimitiveComponent>(BreakingData.Proxy);
				FChaosBreakEvent& BreakEvent = PendingBreakEvents.Emplace_GetRef(BreakingData);
				BreakEvent.Component = Comp;
			}
		}
		if (PendingBreakEvents.Num() > 0)
		{
			ChaosEventRelay->DispatchPhysicsBreakEvents(PendingBreakEvents);
		}
	}
}

void FPhysScene_Chaos::HandleRemovalEvents(const Chaos::FRemovalEventData& Event)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_HandleGlobalRemovalEvents);
	
	const float RemovalDataTimestamp = Event.RemovalData.TimeCreated;
	if (RemovalDataTimestamp > LastRemovalEventDispatchTime)
	{
		LastRemovalEventDispatchTime = RemovalDataTimestamp;
		TArray<FChaosRemovalEvent> PendingRemovalEvents;
		Chaos::FRemovalDataArray const& RemovalData = Event.RemovalData.AllRemovalArray;
		for (Chaos::FRemovalData const& RemovalDataItem : RemovalData)
		{
			if (RemovalDataItem.Proxy)
			{
				UPrimitiveComponent* Comp = GetOwningComponent<UPrimitiveComponent>(RemovalDataItem.Proxy);
				if (GlobalRemovalEventRegistrations.Contains(Comp))
				{
					FChaosRemovalEvent& RemovalEvent = PendingRemovalEvents.AddZeroed_GetRef();
					RemovalEvent.Component = Comp;
					RemovalEvent.Location = RemovalDataItem.Location;
					RemovalEvent.Mass = RemovalDataItem.Mass;
				}
			}
		}
		if (PendingRemovalEvents.Num() > 0)
		{
			ChaosEventRelay->DispatchPhysicsRemovalEvents(PendingRemovalEvents);
		}
	}
}

void FPhysScene_Chaos::HandleCrumblingEvents(const Chaos::FCrumblingEventData& Event)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_HandleGlobalCrumblingEvents);

	const float CrumblingDataTimestamp = Event.CrumblingData.TimeCreated;
	if (Event.CrumblingData.bHasGlobalEvent && CrumblingDataTimestamp > LastCrumblingEventDispatchTime)
	{
		LastCrumblingEventDispatchTime = CrumblingDataTimestamp;
		TArray<FChaosCrumblingEvent> PendingCrumblingEvent;
		for (const Chaos::FCrumblingData& CrumblingDataItem : Event.CrumblingData.AllCrumblingsArray)
		{
			if ((CrumblingDataItem.EmitterFlag & Chaos::EventEmitterFlag::GlobalDispatcher) && CrumblingDataItem.Proxy)
			{
				FChaosCrumblingEvent& CrumblingEvent = PendingCrumblingEvent.AddZeroed_GetRef();
				CrumblingEvent.Component = GetOwningComponent<UPrimitiveComponent>(CrumblingDataItem.Proxy);
				CrumblingEvent.Location = CrumblingDataItem.Location;
				CrumblingEvent.Orientation = CrumblingDataItem.Orientation;
				CrumblingEvent.LinearVelocity = CrumblingDataItem.LinearVelocity;
				CrumblingEvent.AngularVelocity = CrumblingDataItem.AngularVelocity;
				CrumblingEvent.Mass = static_cast<float>(CrumblingDataItem.Mass);
				CrumblingEvent.LocalBounds = FBox(CrumblingDataItem.LocalBounds.Min(), CrumblingDataItem.LocalBounds.Max());
				CrumblingEvent.Children = CrumblingDataItem.Children;
			}
		}
		if (PendingCrumblingEvent.Num() > 0)
		{
			ChaosEventRelay->DispatchPhysicsCrumblingEvents(PendingCrumblingEvent);
		}
	}
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
		PhysicsProxyToComponentMap.Add(InObject, ObjectPtrWrap(Component));

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

// FReplicationCacheData constructor needs to be in .cpp due to UPrimitiveComponent being forward declared in the header which TWeakObjectPtr doesn't handle
FPhysScene_Chaos::FReplicationCacheData::FReplicationCacheData(UPrimitiveComponent* InRootComponent, Chaos::FReal InAccessTime)
	: RootComponent(InRootComponent)
	, AccessTime(InAccessTime)
	, bValidStateCached(false)
{}

const FRigidBodyState* FPhysScene_Chaos::GetStateFromReplicationCache(UPrimitiveComponent* RootComponent, int& ServerFrame)
{
	if (!GetSolver()->GetRewindCallback())
	{
		// We only populate replication cache through the RewindCallback
		ServerFrame = GetSolver()->GetCurrentFrame();
		return nullptr;
	}

	ServerFrame = ReplicationCache.ServerFrame;

	const FObjectKey Key(RootComponent);
	if (!ReplicationCache.Map.Contains(Key))
	{
		RegisterForReplicationCache(RootComponent);
	}

	FRigidBodyState* ReplicationState = nullptr;
	if (FReplicationCacheData* ReplicationData = ReplicationCache.Map.Find(Key))
	{
		if (ReplicationData->IsCached())
		{
			ReplicationData->SetAccessTime(GetSolver()->GetSolverTime());
			ReplicationState = &ReplicationData->GetState();
		}
	}
	return ReplicationState;
}

void FPhysScene_Chaos::RegisterForReplicationCache(UPrimitiveComponent* RootComponent)
{
	const FObjectKey Key(RootComponent);
	ReplicationCache.Map.Add(Key, FReplicationCacheData(RootComponent, GetSolver()->GetSolverTime()));
}

void FPhysScene_Chaos::PopulateReplicationCache(const int32 PhysicsStep)
{
	auto ReplicationCacheHelper = [this](auto& Handle, FReplicationCacheData& ReplicationData, bool& StateWasCached)
	{
		// If the component reference has lingered in the replication cache for too long without being accessed, remove it and stop caching data.
		const Chaos::FReal CacheLingerTime = GetSolver()->GetSolverTime() - ReplicationData.GetAccessTime();
		if (CacheLingerTime > GReplicationCacheLingerForNSeconds)
		{
			StateWasCached = false;
		}
		else
		{
			FRigidBodyState& ReplicationState = ReplicationData.GetState();
			ReplicationState.Position = Handle->GetX();
			ReplicationState.Quaternion = Handle->GetR();
			ReplicationState.LinVel = Handle->GetV();
			ReplicationState.AngVel = Handle->GetW();
			ReplicationState.Flags = Handle->ObjectState() == Chaos::EObjectStateType::Sleeping ? ERigidBodyFlags::Sleeping : 0;
			StateWasCached = true;
		}
		ReplicationData.SetIsCached(StateWasCached);
	};
	
	ReplicationCache.ServerFrame = PhysicsStep;
	bool StateWasCached;
	for (auto It = ReplicationCache.Map.CreateIterator(); It; ++It)
	{
		StateWasCached = false;
		FReplicationCacheData& ReplicationData = It.Value();
		UPrimitiveComponent* RootComponent = ReplicationData.GetRootComponent();
		if (RootComponent)
		{
			if (FBodyInstanceAsyncPhysicsTickHandle BIHandle = RootComponent->GetBodyInstanceAsyncPhysicsTickHandle())
			{
				ReplicationCacheHelper(BIHandle, ReplicationData, StateWasCached);
			}
			else if (Chaos::FPhysicsObjectHandle PhysicsObject = RootComponent->GetPhysicsObjectByName(NAME_None))
			{
				Chaos::FReadPhysicsObjectInterface_Internal Interface = Chaos::FPhysicsObjectInternalInterface::GetRead();
				if (Chaos::FPBDRigidParticleHandle* POHandle = Interface.GetRigidParticle(PhysicsObject))
				{
					ReplicationCacheHelper(POHandle, ReplicationData, StateWasCached);
				}
			}
		}

		if (!StateWasCached)
		{
			// Deregister actor from ReplicationCache
			It.RemoveCurrent();
		}
	}
}

void FPhysScene_Chaos::RemoveFromComponentMaps(IPhysicsProxyBase* InObject)
{
	auto* const Component = PhysicsProxyToComponentMap.Find(InObject);
	if (Component)
	{
		TArray<IPhysicsProxyBase*>* ProxyArray = ComponentToPhysicsProxyMap.Find(*Component);
		if (ProxyArray)
		{
			ProxyArray->RemoveSingleSwap(InObject, EAllowShrinking::No);
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
	FPhysicsInterface::AddForce_AssumesLocked(BodyInstance->GetPhysicsActorHandle(), Force, bAllowSubstepping, bAccelChange);
}

void FPhysScene_Chaos::AddForceAtPosition_AssumesLocked(FBodyInstance* BodyInstance, const FVector& Force, const FVector& Position, bool bAllowSubstepping, bool bIsLocalForce /*= false*/)
{
	FPhysicsInterface::AddForceAtPosition_AssumesLocked(BodyInstance->GetPhysicsActorHandle(), Force, Position, bAllowSubstepping, bIsLocalForce);
}

void FPhysScene_Chaos::AddRadialForceToBody_AssumesLocked(FBodyInstance* BodyInstance, const FVector& Origin, const float Radius, const float Strength, const uint8 Falloff, bool bAccelChange, bool bAllowSubstepping)
{
	FPhysicsInterface::AddRadialForce_AssumesLocked(BodyInstance->GetPhysicsActorHandle(), Origin, Radius, Strength, Falloff, bAccelChange, bAllowSubstepping);
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
	FPhysicsInterface::AddTorque_AssumesLocked(BodyInstance->GetPhysicsActorHandle(), Torque, bAllowSubstepping, bAccelChange);
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
	FPhysicsInterface::SetKinematicTarget_AssumesLocked(BodyInstance->GetPhysicsActorHandle(), TargetTM);
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
			if (USkeletalMeshComponent* SkelComp = DeferredKinematicUpdateSkelMeshes.Last().Key.Get())
			{
				SkelComp->DeferredKinematicUpdateIndex = DeferredKinematicUpdateIndex;
			}
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
		for (const TPair<TWeakObjectPtr<USkeletalMeshComponent>, FDeferredKinematicUpdateInfo>& DeferredKinematicUpdate : DeferredKinematicUpdateSkelMeshes)
		{
			SkeletalMeshStartIndexArray.Add(TotalBodies);

			if (USkeletalMeshComponent* SkelComp = DeferredKinematicUpdate.Key.Get())
			{
				if (!SkelComp->bEnablePerPolyCollision)
				{
					TotalBodies += SkelComp->Bodies.Num();
				}
			}
		}

		// Actors pool is spare, initialize to nullptr.
		TeleportActorsPool.AddZeroed(TotalBodies);
		ProxiesToDirty.Reserve(TotalBodies);
	}

	// Gather proxies that need to be dirtied before paralell loop, and update any per poly collision skeletal meshes.
	{
		for (const TPair<TWeakObjectPtr<USkeletalMeshComponent>, FDeferredKinematicUpdateInfo>& DeferredKinematicUpdate : DeferredKinematicUpdateSkelMeshes)
		{
			USkeletalMeshComponent* SkelComp = DeferredKinematicUpdate.Key.Get();
			if (SkelComp == nullptr)
			{
				continue;
			}

			const FDeferredKinematicUpdateInfo& Info = DeferredKinematicUpdate.Value;

			if (!SkelComp->bEnablePerPolyCollision)
			{
				const int32 NumBodies = SkelComp->Bodies.Num();
				for (int32 i = 0; i < NumBodies; i++)
				{
					FBodyInstance* BodyInst = SkelComp->Bodies[i];
					if (GKinematicDeferralCheckValidBodies && (BodyInst == nullptr || !BodyInst->IsValidBodyInstance()))
					{
						if (GKinematicDeferralLogInvalidBodies)
						{
							UE_LOG(LogChaos, Warning, TEXT("\n"
								"Invalid FBodyInstance in FPhysScene_Chaos::UpdateKinematicsOnDeferredSkelMesh - Gathering Proxies\n"
								"SkeletalMesh: %s\n"
								"OwningActor: %s"),
								*SkelComp->GetName(),
								*(SkelComp->GetOwner() ? SkelComp->GetOwner()->GetName() : FString("None")));
						}
						continue;
					}

					FPhysicsActorHandle& ActorHandle = BodyInst->ActorHandle;
					if (GKinematicDeferralCheckValidBodies && (ActorHandle == nullptr || ActorHandle->GetSyncTimestamp() == nullptr || ActorHandle->GetMarkedDeleted()))
					{
						if (GKinematicDeferralLogInvalidBodies)
						{
							UE_LOG(LogChaos, Warning, TEXT("\n"
								"Invalid FPhysicsActorHandle in FPhysScene_Chaos::UpdateKinematicsOnDeferredSkelMesh - Gathering Proxies\n"
								"SkeletalMesh: %s\n"
								"OwningActor: %s"),
								*SkelComp->GetName(),
								*(SkelComp->GetOwner() ? SkelComp->GetOwner()->GetName() : FString("None")));
						}
						continue;
					}

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
			const TPair<TWeakObjectPtr<USkeletalMeshComponent>, FDeferredKinematicUpdateInfo>& DeferredKinematicUpdate = DeferredKinematicUpdateSkelMeshes[Index];
			USkeletalMeshComponent* SkelComp = DeferredKinematicUpdate.Key.Get();
			if (SkelComp == nullptr)
			{
				return;
			}

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
					if (GKinematicDeferralCheckValidBodies && (BodyInst == nullptr || !BodyInst->IsValidBodyInstance()))
					{
						if (GKinematicDeferralLogInvalidBodies)
						{
							UE_LOG(LogChaos, Warning, TEXT("\n"
								"Invalid FBodyInstance in FPhysScene_Chaos::UpdateKinematicsOnDeferredSkelMesh - Add to Teleport Pool\n"
								"SkeletalMesh: %s\n"
								"OwningActor: %s"),
								*SkelComp->GetName(),
								*(SkelComp->GetOwner() ? SkelComp->GetOwner()->GetName() : FString("None")));
						}
						continue;
					}

					FPhysicsActorHandle& ActorHandle = BodyInst->ActorHandle;
					if (GKinematicDeferralCheckValidBodies && (ActorHandle == nullptr || ActorHandle->GetSyncTimestamp() == nullptr || ActorHandle->GetMarkedDeleted()))
					{
						if (GKinematicDeferralLogInvalidBodies)
						{
							UE_LOG(LogChaos, Warning, TEXT("\n"
								"Invalid FPhysicsActorHandle in FPhysScene_Chaos::UpdateKinematicsOnDeferredSkelMesh - Add to Teleport Pool\n"
								"SkeletalMesh: %s\n"
								"OwningActor: %s"),
								*SkelComp->GetName(),
								*(SkelComp->GetOwner() ? SkelComp->GetOwner()->GetName() : FString("None")));
						}
						continue;
					}

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

	// If there are pending deletions that share an AABBTree node with an actor
	// that is to be updated in TeleportActorsPool, then we must remove it before
	// trying to update the node.
	//
	// If the deleted particle has had its memory cleared or overwritten, then it
	// may have invalid object bounds, so when the tree attempts to update the bounds
	// of the parent node, NaNs or other invalid numbers may appear in the tree
	// (aka, FORT-564602)
	// Update: This fix was speculative and not needed anymore. The Gamethread Acceleration structure deletes should be up to date at this point, so no need to waste performance here.
	// Setting GKinematicDeferralUpdateExternalAccelerationStructure to false TODO: remove option entirely after a release
	if (GKinematicDeferralUpdateExternalAccelerationStructure)
	{
		CopySolverAccelerationStructure();
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

DECLARE_CYCLE_STAT(TEXT("FPhysScene_Chaos::OnSyncBodies-FSingleParticlePhysicsProxy"), STAT_SyncBodiesSingleParticlePhysicsProxy, STATGROUP_Chaos);
DECLARE_CYCLE_STAT(TEXT("FPhysScene_Chaos::OnSyncBodies-FJointConstraintPhysicsProxy"), STAT_SyncBodiesJointConstraintPhysicsProxy, STATGROUP_Chaos);
DECLARE_CYCLE_STAT(TEXT("FPhysScene_Chaos::OnSyncBodies-FGeometryCollectionPhysicsProxy"), STAT_SyncBodiesGeometryCollectionPhysicsProxy, STATGROUP_Chaos);
DECLARE_CYCLE_STAT(TEXT("FPhysScene_Chaos::OnSyncBodies-FClusterUnionPhysicsProxy"), STAT_SyncBodiesClusterUnionPhysicsProxy, STATGROUP_Chaos);

static void UpdateAccelerationStructureFromGeometryCollectionProxy(FGeometryCollectionPhysicsProxy& Proxy, Chaos::ISpatialAcceleration<Chaos::FAccelerationStructureHandle, Chaos::FReal, 3>& SpatialAcceleration)
{
	SCOPE_CYCLE_COUNTER(STAT_SyncBodiesGeometryCollectionPhysicsProxy);

	const bool bIsParentProxyNull = (Proxy.GetParentProxy() == nullptr);

	auto AreAllShapesQueryEnabled = [](Chaos::FPBDRigidParticle& Particle) -> bool
	{
		for (const TUniquePtr<Chaos::FPerShapeData>& ShapeData : Particle.ShapesArray())
		{
			if (!ShapeData->GetCollisionData().bQueryCollision)
			{
				return false;
			}
		}
		return true;
	};

	auto GetParticleWorldBounds = [](Chaos::FPBDRigidParticle& Particle) -> Chaos::FAABB3
	{
		const Chaos::FImplicitObjectRef Geometry = Particle.GetGeometry();
		ensure(Geometry != nullptr);

		if ((Geometry != nullptr) && Geometry->HasBoundingBox())
		{
			const Chaos::FRigidTransform3 ParticleWorldTransform(Particle.X(), Particle.R());
			return Geometry->CalculateTransformedBounds(ParticleWorldTransform);
		}

		return Chaos::FAABB3(Particle.X(), Particle.X());
	};

	const TArray<TUniquePtr<Chaos::FPBDRigidParticle>>& UnorderedGTParticles = Proxy.GetUnorderedParticles_External();
	if (bIsParentProxyNull)
	{
		for (const TUniquePtr<Chaos::FPBDRigidParticle>& GTParticle : UnorderedGTParticles)
		{
			if (GTParticle)
			{
				const Chaos::FSpatialAccelerationIdx SpatialIndex = GTParticle->SpatialIdx();

				// It's possible to be an enabled particle and not qualify for query collisions if the GC particle has been replication abandoned. 
				// Furthermore, it's possible for a particle to be enabled on the game thread but not actually enabled on the physics thread. A particle
				// with an internal cluster parent (e.g. a cluster union) could get into this state where the geometry collection physics proxy will not
				// transfer the disabled state from the PT to the GT because the particle has an internal cluster for a parent. We can detect this case by making
				// sure that the proxy of the GC does not have a parent proxy (only happens with a cluster union currently).
				if (!GTParticle->Disabled() && AreAllShapesQueryEnabled(*GTParticle))
				{
					const Chaos::FAccelerationStructureHandle AccelerationHandleForUpdate(GTParticle.Get());
					const Chaos::FAABB3 WorldBounds = GetParticleWorldBounds(*GTParticle);
					SpatialAcceleration.UpdateElementIn(AccelerationHandleForUpdate, WorldBounds, true /* bHasBounds */, SpatialIndex);
				}
				else
				{
					// Perf optimization : We only use this FAccelerationStructureHandle for remove from the structure so precomputing the prefiltering data is not necessary 
					const Chaos::FAccelerationStructureHandle AccelerationHandleForRemove(GTParticle.Get(), false /*bUsePrefiltering*/);
					SpatialAcceleration.RemoveElementFrom(AccelerationHandleForRemove, SpatialIndex);
				}
			}
		}
	}
	else
	{
		// if we have a parent proxy ( like attached to a cluster union) then none of the handles should be in the acceleration structure 
		// todo : right now we are sending all the particles, but we should be able to get away with active + direct children that could reduce the overall cost 
		for (const TUniquePtr<Chaos::FPBDRigidParticle>& GTParticle : UnorderedGTParticles)
		{
			if (GTParticle)
			{
				// Perf optimization : We only use this FAccelerationStructureHandle for remove from the structure so precomputing the prefiltering data is not necessary 
				const Chaos::FAccelerationStructureHandle AccelerationHandleForRemove(GTParticle.Get(), false /*bUsePrefiltering*/);
				SpatialAcceleration.RemoveElementFrom(AccelerationHandleForRemove, GTParticle->SpatialIdx());
			}
		}
	}
}

void FPhysScene_Chaos::OnSyncBodies(Chaos::FPhysicsSolverBase* Solver)
{
	using namespace Chaos;

	struct FDispatcher
	{
		FPhysScene_Chaos* Outer;
		FPhysScene* PhysScene;
		Chaos::FPhysicsSolverBase* Solver;
		TArray<FPhysScenePendingComponentTransform_Chaos> PendingTransforms;

		void operator()(FSingleParticlePhysicsProxy* Proxy)
		{
			SCOPE_CYCLE_COUNTER(STAT_SyncBodiesSingleParticlePhysicsProxy);
			FPBDRigidParticle* DirtyParticle = Proxy->GetRigidParticleUnsafe();

			if (FBodyInstance* BodyInstance = ChaosInterface::GetUserData(*(Proxy->GetParticle_LowLevel())))
			{
				if (BodyInstance->OwnerComponent.IsValid())
				{
					// Skip kinematics, unless they've been flagged to update following simulation
					if (!BodyInstance->bUpdateKinematicFromSimulation && BodyInstance->IsInstanceSimulatingPhysics() == false)
					{
						return;
					}
					UPrimitiveComponent* OwnerComponent = BodyInstance->OwnerComponent.Get();
					if (OwnerComponent != nullptr)
					{
						bool bPendingMove = false;
						FRigidTransform3 NewTransform(DirtyParticle->X(), DirtyParticle->R());

						if (!NewTransform.EqualsNoScale(OwnerComponent->GetComponentTransform()))
						{
							if (BodyInstance->InstanceBodyIndex == INDEX_NONE)
							{

								bPendingMove = true;
								const FVector MoveBy = NewTransform.GetLocation() - OwnerComponent->GetComponentTransform().GetLocation();
								const FQuat NewRotation = NewTransform.GetRotation();
								PendingTransforms.Add(FPhysScenePendingComponentTransform_Chaos(OwnerComponent, MoveBy, NewRotation, Proxy->GetWakeEvent()));

							}

							PhysScene->UpdateActorInAccelerationStructure(BodyInstance->ActorHandle);
						}

						if (Proxy->GetWakeEvent() != Chaos::EWakeEventEntry::None && !bPendingMove)
						{
							PendingTransforms.Add(FPhysScenePendingComponentTransform_Chaos(OwnerComponent, Proxy->GetWakeEvent()));
						}
						Proxy->ClearEvents();
					}
				}
			}
		}

		void operator()(FJointConstraintPhysicsProxy* Proxy)
		{
			SCOPE_CYCLE_COUNTER(STAT_SyncBodiesJointConstraintPhysicsProxy);
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
		}

		void operator()(FGeometryCollectionPhysicsProxy* Proxy)
		{
			if (Proxy && Outer->GetSpacialAcceleration())
			{
				UpdateAccelerationStructureFromGeometryCollectionProxy(*Proxy, *Outer->GetSpacialAcceleration());
			}
		}

		void operator()(FClusterUnionPhysicsProxy* Proxy)
		{
			SCOPE_CYCLE_COUNTER(STAT_SyncBodiesClusterUnionPhysicsProxy);
			FLockedWritePhysicsObjectExternalInterface Interface = FPhysicsObjectExternalInterface::LockWrite({});
			Chaos::FPhysicsObjectHandle Handle = Proxy->GetPhysicsObjectHandle();

			// Cluster unions should always have their owner be the cluster union component.
			FClusterUnionPhysicsProxy::FExternalParticle* DirtyParticle = Proxy->GetParticle_External();

			// Use the scene's GetOwningComponent rather than UserData since it's guaranteed to be safe and correspond to the GT state of the component.
			// i.e. if we destroy the component's physics state and then call sync bodies (not entirely impossible), we don't try to add the cluster union
			// back into the acceleration structure (since destroying the physics state already took it out). In that case, we could potentially end up with
			// a deleted cluster union particle in the SQ.
			if (UClusterUnionComponent* ParentComponent = Outer->GetOwningComponent<UClusterUnionComponent>(Proxy))
			{
				const FRigidTransform3 NewTransform(DirtyParticle->X(), DirtyParticle->R());

				bool bHasMoved = false;
				if (!NewTransform.EqualsNoScale(ParentComponent->GetComponentTransform()))
				{
					bHasMoved = true;
					const FVector MoveBy = NewTransform.GetLocation() - ParentComponent->GetComponentTransform().GetLocation();
					PendingTransforms.Add(FPhysScenePendingComponentTransform_Chaos(ParentComponent, MoveBy, NewTransform.GetRotation(), DirtyParticle->GetWakeEvent()));
				}

				if (DirtyParticle->GetWakeEvent() != Chaos::EWakeEventEntry::None && !bHasMoved)
				{
					PendingTransforms.Add(FPhysScenePendingComponentTransform_Chaos(ParentComponent, DirtyParticle->GetWakeEvent()));
				}

				if (bHasMoved || !bGClusterUnionSyncBodiesMoveNewComponents)
				{
					ParentComponent->SyncClusterUnionFromProxy(NewTransform, nullptr);
				}
				else
				{
					// We must to call MoveComponent on any newly added component. The Cluster Union will take care of
					// that but only if it moved. If it did not move we need to handle it manually
					TArray<TTuple<UPrimitiveComponent*, FTransform>> NewComponents;
					ParentComponent->SyncClusterUnionFromProxy(NewTransform, &NewComponents);

					for (TTuple<UPrimitiveComponent*, FTransform>& NewComponentTuple : NewComponents)
					{
						UPrimitiveComponent* NewComponent = NewComponentTuple.Get<0>();
						const FTransform& NewComponentTransform = NewComponentTuple.Get<1>();
						const FVector NewComponentCurrentLocation = NewComponent->GetComponentLocation();
						const FVector NewComponentDelta = NewComponentTransform.GetLocation() - NewComponentCurrentLocation;

						PendingTransforms.Add(FPhysScenePendingComponentTransform_Chaos(NewComponent, NewComponentDelta, NewComponentTransform.GetRotation(), DirtyParticle->GetWakeEvent()));
					}
				}

				// make sure we have at least a child to be added to the acceleration structure 
				// this avoid the invalid bounds to cause the particle to be added to the global acceleration structure array
				bool bShouldBeInSQ = false;

				if (FImplicitObjectRef GeometryRef = DirtyParticle->GetGeometry())
				{
					if (FImplicitObjectUnion* Union = GeometryRef->AsA<FImplicitObjectUnion>())
					{
						bShouldBeInSQ = (Union->GetNumRootObjects() > 0);
					}
				}

				if (bShouldBeInSQ)
				{
					Interface->AddToSpatialAcceleration({ &Handle, 1 }, Outer->GetSpacialAcceleration());
				}
				else
				{
					Interface->RemoveFromSpatialAcceleration({ &Handle, 1 }, Outer->GetSpacialAcceleration());
				}

				DirtyParticle->ClearEvents();
			}
		}
	};

	FDispatcher Dispatcher;
	FPhysicsCommand::ExecuteWrite(this, [this, Solver, &Dispatcher](FPhysScene* PhysScene)
	{
		Dispatcher.Outer = this;
		Dispatcher.PhysScene = PhysScene;
		Dispatcher.Solver = Solver;
		Solver->PullPhysicsStateForEachDirtyProxy_External(Dispatcher);
	});

	for (const FPhysScenePendingComponentTransform_Chaos& ComponentTransform : Dispatcher.PendingTransforms)
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
						const FBodyInstance* BI = ChaosInterface::GetUserData(Info.Particle);
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

void FPhysScene_Chaos::EnqueueAsyncPhysicsCommand(int32 PhysicsStep, UObject* OwningObject, const TFunction<void()>& Command, const bool bEnableResim)
{
	EnableAsyncPhysicsTickCallback();
	AsyncPhysicsTickCallback->PendingCommands.Add({ PhysicsStep, TWeakObjectPtr<UObject>(OwningObject), Command, bEnableResim });
}

TSharedPtr<IPhysicsReplicationFactory> FPhysScene_Chaos::PhysicsReplicationFactory;

