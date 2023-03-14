// Copyright Epic Games, Inc. All Rights Reserved.

#include "NetworkPredictionWorldManager.h"
#include "NetworkPredictionCVars.h"
#include "Debug/DebugDrawService.h"
#include "GameFramework/PlayerController.h"
#include "NetworkPredictionTrace.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "PBDRigidsSolver.h"
#include "ChaosSolversModule.h"
#include "RewindData.h"
#include "NetworkPredictionReplicatedManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NetworkPredictionWorldManager)

// Do extra checks to make sure Physics and GameThread (PrimitiveComponent) are in sync at verious points in the rollback process
#define NP_ENSURE_PHYSICS_GT_SYNC !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

UNetworkPredictionWorldManager* UNetworkPredictionWorldManager::ActiveInstance=nullptr;

// -----------------------------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------------------------

UNetworkPredictionWorldManager::UNetworkPredictionWorldManager()
{

}

void UNetworkPredictionWorldManager::Initialize(FSubsystemCollectionBase& Collection)
{
	UWorld* World = GetWorld();
	check(World);

	if (World->WorldType == EWorldType::PIE || World->WorldType == EWorldType::Game)
	{
		PreTickDispatchHandle = FWorldDelegates::OnWorldTickStart.AddUObject(this, &UNetworkPredictionWorldManager::OnWorldPreTick);
		PostTickDispatchHandle = World->OnPostTickDispatch().AddUObject(this, &UNetworkPredictionWorldManager::ReconcileSimulationsPostNetworkUpdate);
		PreWorldActorTickHandle = FWorldDelegates::OnWorldPreActorTick.AddUObject(this, &UNetworkPredictionWorldManager::BeginNewSimulationFrame);

		SyncNetworkPredictionSettings(GetDefault<UNetworkPredictionSettingsObject>());
	}
}

void UNetworkPredictionWorldManager::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		if (PreTickDispatchHandle.IsValid())
		{
			FWorldDelegates::OnWorldTickStart.Remove(PreTickDispatchHandle);
		}
		if (PostTickDispatchHandle.IsValid())
		{
			World->OnPostTickDispatch().Remove(PostTickDispatchHandle);
		}
		if (PreWorldActorTickHandle.IsValid())
		{
			FWorldDelegates::OnWorldPreActorTick.Remove(PreWorldActorTickHandle);
		}
	}
}

void UNetworkPredictionWorldManager::SyncNetworkPredictionSettings(const UNetworkPredictionSettingsObject* SettingsObj)
{
	this->Settings = SettingsObj->Settings;
}

// -----------------------------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------------------------

void UNetworkPredictionWorldManager::OnWorldPreTick(UWorld* InWorld, ELevelTick InLevelTick, float InDeltaSeconds)
{
	if (InWorld != GetWorld())
	{
		return;
	}

	UE_NP_TRACE_WORLD_FRAME_START(InWorld->GetGameInstance(), InDeltaSeconds);

	// Update fixed tick rate, this can be changed via editor settings
	FixedTickState.FixedStepRealTimeMS = (1.f /  GEngine->FixedFrameRate) * 1000.f;
	FixedTickState.FixedStepMS = (int32)FixedTickState.FixedStepRealTimeMS;

	ActiveInstance = this;

	// Instantiate replicated manager on server
	if (!ReplicatedManager && InWorld->GetNetMode() != NM_Client)
	{
		UClass* ReplicatedManagerClass = GetDefault<UNetworkPredictionSettingsObject>()->Settings.ReplicatedManagerClassOverride.Get();
		ReplicatedManager = ReplicatedManagerClass ? InWorld->SpawnActor<ANetworkPredictionReplicatedManager>(ReplicatedManagerClass) : InWorld->SpawnActor<ANetworkPredictionReplicatedManager>();
	}
}

void UNetworkPredictionWorldManager::ReconcileSimulationsPostNetworkUpdate()
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_NP_RECONCILE);

	UWorld* World = GetWorld();
	if (World->GetNetMode() != NM_Client)
	{
		return;
	}

	ActiveInstance = this;
	bLockServices = true;

	// Trace Local->Server offset. We need to trace this so that we can flag reconciles that happened
	// due to this (usually caused by server being starved for input)
	const bool OffsetChanged = (LastFixedTickOffset != FixedTickState.Offset);
	UE_NP_TRACE_FIXED_TICK_OFFSET(FixedTickState.Offset, OffsetChanged);
	LastFixedTickOffset = FixedTickState.Offset;

	// -------------------------------------------------------------------------
	//	Non-rollback reconcile services
	// -------------------------------------------------------------------------
	
	// Don't reconcile FixedTick interpolates until we've started interpolation
	// This makes the service's implementation easier if it can rely on a known
	// ToFrame while reconciling network updates
	if (FixedTickState.Interpolation.ToFrame != INDEX_NONE)
	{
		for (TUniquePtr<IFixedInterpolateService>& Ptr : Services.FixedInterpolate.Array)
		{
			Ptr->Reconcile(&FixedTickState);
		}
	}

	for (TUniquePtr<IIndependentInterpolateService>& Ptr : Services.IndependentInterpolate.Array)
	{
		Ptr->Reconcile(&VariableTickState);
	}

	// -------------------------------------------------------------------------
	//	Fixed Tick rollback
	// -------------------------------------------------------------------------
	if (FixedTickState.PhysicsRewindData)
	{
		// Local PendingFrame = PhysicsFrame - PhysicsOffset
		FixedTickState.PhysicsOffset = FixedTickState.PhysicsRewindData->CurrentFrame() - FixedTickState.PendingFrame;
	}

	// Does anyone need to rollback?
	int32 RollbackFrame = INDEX_NONE;
	for (TUniquePtr<IFixedRollbackService>& Ptr : Services.FixedRollback.Array)
	{
		const int32 ReqFrame = Ptr->QueryRollback(&FixedTickState);
		if (ReqFrame != INDEX_NONE)
		{
			RollbackFrame = (RollbackFrame == INDEX_NONE ? ReqFrame : FMath::Min(RollbackFrame, ReqFrame));
		}
	}

	if (RollbackFrame != INDEX_NONE)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_NP_ROLLBACK);		

		if (RollbackFrame < FixedTickState.PendingFrame)
		{
			// Common case: rollback to previously ticked frame and resimulate			

			const int32 EndFrame = FixedTickState.PendingFrame;
			const int32 NumFrames = EndFrame - RollbackFrame;
			npEnsureSlow(NumFrames > 0);

			int32 PhysicsFrame = INDEX_NONE;
			const bool bDoPhysics = (Physics.Solver != nullptr);

			Chaos::EThreadingModeTemp PreThreading = Chaos::EThreadingModeTemp::SingleThread;

			// ---------------------------------------------------
			//	Rollback physics to local historic state (corrections not injected yet)
			// ---------------------------------------------------
			if (bDoPhysics)
			{
				PhysicsFrame = RollbackFrame + FixedTickState.PhysicsOffset;
				check(false);
				//npEnsure(FixedTickState.PhysicsRewindData->RewindToFrame(PhysicsFrame));

				PreThreading = Physics.Solver->GetThreadingMode();
				Physics.Solver->SetThreadingMode_External(Chaos::EThreadingModeTemp::SingleThread);
			}

			bool bFirstStep = true;

			// Do rollback as necessary
			for (int32 Frame=RollbackFrame; Frame < EndFrame; ++Frame)
			{
				FixedTickState.PendingFrame = Frame;
				FNetSimTimeStep Step = FixedTickState.GetNextTimeStep();
				FServiceTimeStep ServiceStep = FixedTickState.GetNextServiceTimeStep();
			
				const int32 ServerInputFrame = Frame + FixedTickState.Offset;
				UE_NP_TRACE_PUSH_TICK(Step.TotalSimulationTime, FixedTickState.FixedStepMS, Step.Frame);

				// Everyone must apply corrections and flush as necessary before anyone runs the next sim tick
				// bFirstStep will indicate that even if they don't have a correction, they need to rollback their historic state
				for (TUniquePtr<IFixedRollbackService>& Ptr : Services.FixedRollback.Array)
				{
					Ptr->PreStepRollback(Step, ServiceStep, FixedTickState.Offset, bFirstStep);
				}
				EnsurePhysicsGTSync(TEXT("PreStepRollback"));

				// Run Sim ticks
				for (TUniquePtr<IFixedRollbackService>& Ptr : Services.FixedRollback.Array)
				{
					Ptr->StepRollback(Step, ServiceStep);
				}
				EnsurePhysicsGTSync(TEXT("PreResimPhysics"));

				// Advance physics 
				if (bDoPhysics)
				{
					AdvancePhysicsResimFrame(PhysicsFrame);
					for (TUniquePtr<IPhysicsService>& Ptr : Services.FixedPhysics.Array)
					{
						Ptr->PostResimulate(&FixedTickState);
					}
				}

				EnsurePhysicsGTSync(TEXT("PostResimulate"));
				bFirstStep = false;
			}

			FixedTickState.PendingFrame = EndFrame;

			if (bDoPhysics)
			{
				Physics.Solver->SetThreadingMode_External(PreThreading);
			}
		}
		else if (RollbackFrame == FixedTickState.PendingFrame)
		{
			// Correction is at the PendingFrame (frame we haven't ticked yet)
			// For now, just do nothing. We are either in a really bad state of PL or are just starting up
			// As our input frames make the round trip, we'll get some slack and be doing corrections in the above code block
			// (Setting the correction data now most likely is still wrong and not worth the iteration time)
			
			UE_LOG(LogNetworkPrediction, Warning, TEXT("RollbackFrame %d EQUAL PendingFrame %d... Offset: %d"), RollbackFrame, FixedTickState.PendingFrame, FixedTickState.Offset);
		}
		else if (RollbackFrame > FixedTickState.PendingFrame)
		{
			// Most likely we haven't had a confirmed frame yet so our local frame -> server mapping hasn't been set yet
			UE_LOG(LogNetworkPrediction, Warning, TEXT("RollbackFrame %d AHEAD of PendingFrame %d... Offset: %d"), RollbackFrame, FixedTickState.PendingFrame, FixedTickState.Offset);
		}
	}

	// -------------------------------------------------------------------------
	//	Independent Tick rollback
	// -------------------------------------------------------------------------
	for (TUniquePtr<IIndependentRollbackService>& Ptr : Services.IndependentRollback.Array)
	{
		Ptr->Reconcile(&VariableTickState);
	}

	bLockServices = false;
	DeferredServiceConfigDelegate.Broadcast(this);
	DeferredServiceConfigDelegate.Clear();
}

void UNetworkPredictionWorldManager::BeginNewSimulationFrame(UWorld* InWorld, ELevelTick InLevelTick, float DeltaTimeSeconds)
{
	if (InWorld != GetWorld() || !InWorld->HasBegunPlay())
	{
		return;
	}

	ActiveInstance = this;
	bLockServices = true;

	const float fEngineFrameDeltaTimeMS = DeltaTimeSeconds * 1000.f;

	QUICK_SCOPE_CYCLE_COUNTER(STAT_NP_TICK);

	// -------------------------------------------------------------------------
	//	Fixed Tick
	// -------------------------------------------------------------------------
	if (Physics.bUsingPhysics || Services.FixedTick.Array.Num() > 0)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_NP_TICK_FIXED);

		FixedTickState.UnspentTimeMS += fEngineFrameDeltaTimeMS;
		
		// If we are using physics, we can only do one NP sim tick per frame.
		// For accumulation fixed tick to work, physics needs to support it too 
		// see notes in NetworkPredictiontickState.h
		const bool bSingleTick = Physics.bUsingPhysics;

		while ((FixedTickState.UnspentTimeMS + KINDA_SMALL_NUMBER) >= FixedTickState.FixedStepRealTimeMS)
		{
			FixedTickState.UnspentTimeMS -= FixedTickState.FixedStepRealTimeMS;
			if (FMath::IsNearlyZero(FixedTickState.UnspentTimeMS))
			{
				FixedTickState.UnspentTimeMS = 0.f;
			}

			FNetSimTimeStep Step = FixedTickState.GetNextTimeStep();
			FServiceTimeStep ServiceStep = FixedTickState.GetNextServiceTimeStep();
			
			const int32 ServerInputFrame = FixedTickState.PendingFrame + FixedTickState.Offset;

			UE_NP_TRACE_PUSH_INPUT_FRAME(ServerInputFrame);
			for (TUniquePtr<IInputService>& Ptr : Services.FixedInputRemote.Array)
			{
				Ptr->ProduceInput(FixedTickState.FixedStepMS);
			}

			for (TUniquePtr<IInputService>& Ptr : Services.FixedInputLocal.Array)
			{
				Ptr->ProduceInput(FixedTickState.FixedStepMS);
			}

			UE_NP_TRACE_PUSH_TICK(Step.TotalSimulationTime, FixedTickState.FixedStepMS, Step.Frame);
			
			// Should we increment PendingFrame before or after the tick?
			// Before: sims that are spawned during Tick (of other sims) will not be ticked this frame.
			// So we want their seed state/cached pending frame to be set to the next pending frame, not this one.
			FixedTickState.PendingFrame++;

			for (TUniquePtr<ILocalTickService>& Ptr : Services.FixedTick.Array)
			{
				Ptr->Tick(Step, ServiceStep);
			}
			
			if (bSingleTick)
			{
				FixedTickState.UnspentTimeMS = 0.f;
				break;
			}
		}
	}

	// -------------------------------------------------------------------------
	//	Local Independent Tick
	// -------------------------------------------------------------------------
	{
		// Update VariableTickState	
		constexpr int32 MinStepMS = 1;
		constexpr int32 MaxStepMS = 100;

		VariableTickState.UnspentTimeMS += fEngineFrameDeltaTimeMS;
		float fDeltaMS = FMath::FloorToFloat(VariableTickState.UnspentTimeMS);
		VariableTickState.UnspentTimeMS -= fDeltaMS;

		const int32 DeltaSimMS = FMath::Clamp((int32)fDeltaMS, MinStepMS, MaxStepMS);

		FVariableTickState::FFrame& PendingFrameData = VariableTickState.Frames[VariableTickState.PendingFrame];
		PendingFrameData.DeltaMS = DeltaSimMS;

		// Input
		UE_NP_TRACE_PUSH_INPUT_FRAME(VariableTickState.PendingFrame);
		for (TUniquePtr<IInputService>& Ptr : Services.IndependentLocalInput.Array)
		{
			Ptr->ProduceInput(DeltaSimMS);
		}

		// -------------------------------------------------------------------------
		// LocalTick
		// -------------------------------------------------------------------------

		FNetSimTimeStep Step = VariableTickState.GetNextTimeStep(PendingFrameData);
		FServiceTimeStep ServiceStep = VariableTickState.GetNextServiceTimeStep(PendingFrameData);
		UE_NP_TRACE_PUSH_TICK(Step.TotalSimulationTime, Step.StepMS, Step.Frame);

		for (TUniquePtr<ILocalTickService>& Ptr : Services.IndependentLocalTick.Array)
		{
			Ptr->Tick(Step, ServiceStep);
		}	

		// -------------------------------------------------------------------------
		//	Remote Independent Tick
		// -------------------------------------------------------------------------
		for (TUniquePtr<IRemoteIndependentTickService>& Ptr : Services.IndependentRemoteTick.Array)
		{
			Ptr->Tick(DeltaTimeSeconds, &VariableTickState);
		}

		// Increment local PendingFrame and set (next) pending frame's TotalMS
		const int32 EndTotalSimTimeMS = PendingFrameData.TotalMS + PendingFrameData.DeltaMS;
		VariableTickState.Frames[++VariableTickState.PendingFrame].TotalMS = EndTotalSimTimeMS;
	}
	
	// -------------------------------------------------------------------------
	// Interpolation
	// -------------------------------------------------------------------------
	if (Services.FixedInterpolate.Array.Num() > 0)
	{
		const int32 LatestRecvFrame = FixedTickState.Interpolation.LatestRecvFrameAP != INDEX_NONE ? FixedTickState.Interpolation.LatestRecvFrameAP : FixedTickState.Interpolation.LatestRecvFrameSP;
		if (LatestRecvFrame != INDEX_NONE)
		{
			// We want 100ms of buffered time. As long a actors replicate at >= 10hz, this is should be good
			// Its better to keep this simple with a single time rather than trying to coordinate lowest amount of buffered time
			// between all the registered instances in the different ModelDefs
			const int32 DesiredBufferedMS = Settings.FixedTickInterpolationBufferedMS;

			float InterpolateRate = 1.f;
			if (FixedTickState.Interpolation.ToFrame == INDEX_NONE)
			{
				const int32 NumBufferedFrames = LatestRecvFrame;
				const int32 BufferedMS = NumBufferedFrames * FixedTickState.FixedStepMS;

				//UE_LOG(LogTemp, Warning, TEXT("BufferedMS: %d Frames: %d (No ToFrame)"), BufferedMS, NumBufferedFrames);

				if (BufferedMS < DesiredBufferedMS)
				{
					// Not enough time buffered yet to start interpolating
					InterpolateRate = 0.f;
				}
				else
				{
					// Begin interpolation
					const int32 DesiredNumBufferedFrames = (DesiredBufferedMS / FixedTickState.FixedStepMS);
					FixedTickState.Interpolation.ToFrame = LatestRecvFrame - DesiredNumBufferedFrames;

					FixedTickState.Interpolation.PCT = 0.f;
					FixedTickState.Interpolation.AccumulatedTimeMS = 0.f;

					// We need to force a reconcile here since we supress the call until interpolation starts
					for (TUniquePtr<IFixedInterpolateService>& Ptr : Services.FixedInterpolate.Array)
					{
						Ptr->Reconcile(&FixedTickState);
					}
				}
			}
			else
			{
				const int32 NumBufferedFrames = LatestRecvFrame - FixedTickState.Interpolation.ToFrame;
				const int32 BufferedMS = NumBufferedFrames * FixedTickState.FixedStepMS;

				//UE_LOG(LogTemp, Warning, TEXT("BufferedMS: %d Frames: %d"), BufferedMS, NumBufferedFrames);

				if (NumBufferedFrames <= 0)
				{
					InterpolateRate = 0.f;
				}
			}

			int32 AdvanceFrames = 0;
			if (InterpolateRate > 0.f)
			{
				const float fScaledDeltaTimeMS = (InterpolateRate * fEngineFrameDeltaTimeMS);

				FixedTickState.Interpolation.AccumulatedTimeMS += fScaledDeltaTimeMS;
				AdvanceFrames = (int32)FixedTickState.Interpolation.AccumulatedTimeMS / FixedTickState.FixedStepRealTimeMS;
				if (AdvanceFrames > 0)
				{
					FixedTickState.Interpolation.ToFrame += AdvanceFrames;
					FixedTickState.Interpolation.AccumulatedTimeMS -= (AdvanceFrames * FixedTickState.FixedStepRealTimeMS);
				}
				const float RawPCT = FixedTickState.Interpolation.AccumulatedTimeMS / (float)FixedTickState.FixedStepRealTimeMS;
				FixedTickState.Interpolation.PCT = FMath::Clamp<float>(RawPCT, 0.f, 1.f);
				npEnsureMsgf(FixedTickState.Interpolation.PCT >= 0.f && FixedTickState.Interpolation.PCT <= 1.f, TEXT("Interpolation PCT out of range. %f"), FixedTickState.Interpolation.PCT);

				const float PCTms = FixedTickState.Interpolation.PCT * (float)FixedTickState.FixedStepMS;
				FixedTickState.Interpolation.InterpolatedTimeMS = ((FixedTickState.Interpolation.ToFrame-1) * FixedTickState.FixedStepMS) + (int32)PCTms;

				//UE_LOG(LogTemp, Warning, TEXT("[Interpolate] %s Interpolating ToFrame %d. PCT: %.2f. Buffered: %d"), *GetPathName(), FixedTickState.Interpolation.ToFrame, FixedTickState.Interpolation.PCT, FixedTickState.Interpolation.LatestRecvFrame - FixedTickState.Interpolation.ToFrame);

				for (TUniquePtr<IFixedInterpolateService>& Ptr : Services.FixedInterpolate.Array)
				{
					Ptr->FinalizeFrame(DeltaTimeSeconds, &FixedTickState);
				}
			}
		}
	}


	if (Services.IndependentInterpolate.Array.Num() > 0)
	{
		const int32 DesiredBufferedMS = Settings.IndependentTickInterpolationBufferedMS;
		const int32 MaxBufferedMS = Settings.IndependentTickInterpolationMaxBufferedMS;
		
		if (VariableTickState.Interpolation.LatestRecvTimeMS > DesiredBufferedMS)
		{
			float InterpolationRate = 1.f;

			const int32 BufferedMS = VariableTickState.Interpolation.LatestRecvTimeMS - (int32)VariableTickState.Interpolation.fTimeMS;
			if (BufferedMS > MaxBufferedMS)
			{
				UE_LOG(LogNetworkPrediction, Warning, TEXT("Independent Interpolation fell behind. BufferedMS: %d"), BufferedMS);
				VariableTickState.Interpolation.fTimeMS = (float)(VariableTickState.Interpolation.LatestRecvTimeMS - DesiredBufferedMS);
			}
			else if (BufferedMS <= 0)
			{
				UE_LOG(LogNetworkPrediction, Warning, TEXT("Independent Interpolation starved: %d"), BufferedMS);
				InterpolationRate = 0.f;
			}

			if (InterpolationRate > 0.f)
			{
				const float fScaledDeltaTimeMS = (InterpolationRate * fEngineFrameDeltaTimeMS);
				VariableTickState.Interpolation.fTimeMS += fScaledDeltaTimeMS;
			}

			for (TUniquePtr<IIndependentInterpolateService>& Ptr : Services.IndependentInterpolate.Array)
			{
				Ptr->FinalizeFrame(DeltaTimeSeconds, &VariableTickState);
			}
		}
	}


	//-------------------------------------------------------------------------------------------------------------
	// Handle newly spawned services right now, so that they can Finalize/SendRPCs on the very first frame of life
	//-------------------------------------------------------------------------------------------------------------

	bLockServices = false;
	DeferredServiceConfigDelegate.Broadcast(this);
	DeferredServiceConfigDelegate.Clear();

	// -------------------------------------------------------------------------
	//	Finalize
	// -------------------------------------------------------------------------
	const int32 FixedTotalSimTimeMS = FixedTickState.GetTotalSimTimeMS();
	const int32 FixedServerFrame = FixedTickState.PendingFrame + FixedTickState.Offset;
	for (TUniquePtr<IFinalizeService>& Ptr : Services.FixedFinalize.Array)
	{
		Ptr->FinalizeFrame(DeltaTimeSeconds, FixedServerFrame, FixedTotalSimTimeMS, FixedTickState.FixedStepMS);
	}

	const int32 IndependentTotalSimTimeMS = VariableTickState.Frames[VariableTickState.PendingFrame].TotalMS;
	const int32 IndependentFrame = VariableTickState.PendingFrame;
	for (TUniquePtr<IFinalizeService>& Ptr : Services.IndependentLocalFinalize.Array)
	{
		Ptr->FinalizeFrame(DeltaTimeSeconds, IndependentFrame, IndependentTotalSimTimeMS, 0);
	}
	
	for (TUniquePtr<IRemoteFinalizeService>& Ptr : Services.IndependentRemoteFinalize.Array)
	{
		Ptr->FinalizeFrame(DeltaTimeSeconds);
	}

	// -------------------------------------------------------------------------
	// Fixed Physics (just does bookkeeping/tracing, not actual physics sim)
	// -------------------------------------------------------------------------
	for (TUniquePtr<IPhysicsService>& Ptr : Services.FixedPhysics.Array)
	{
		Ptr->PostNetworkPredictionFrame(&FixedTickState);
	}

	// -------------------------------------------------------------------------
	// Call server RPC (common)
	// -------------------------------------------------------------------------

	for (TUniquePtr<IServerRPCService>& Ptr : Services.ServerRPC.Array)
	{
		Ptr->CallServerRPC(DeltaTimeSeconds);
	}
}

void UNetworkPredictionWorldManager::InitPhysicsCapture()
{
	constexpr int32 PhysicsBufferSize = 64; // fixme

	npCheckSlow(Physics.Solver != nullptr);
	npCheckSlow(!Physics.bRecordingEnabled);

	Physics.bRecordingEnabled = true;
	Physics.Solver->EnableRewindCapture(PhysicsBufferSize, false);	//TODO: turn optimizations back on
	FixedTickState.PhysicsRewindData = Physics.Solver->GetRewindData();
}

void UNetworkPredictionWorldManager::AdvancePhysicsResimFrame(int32& PhysicsFrame)
{
	Physics.Solver->AdvanceAndDispatch_External(FixedTickState.PhysicsRewindData->GetDeltaTimeForFrame(PhysicsFrame));
	Physics.Solver->UpdateGameThreadStructures();
	PhysicsFrame++;
}

bool UNetworkPredictionWorldManager::CanPhysicsFixTick() const
{
	npCheckSlow(GEngine);
	return (GEngine->bUseFixedFrameRate) || Settings.bForceEngineFixTickForcePhysics;
}

void UNetworkPredictionWorldManager::SetUsingPhysics()
{
	if (!Physics.bUsingPhysics)
	{
		npCheckSlow(Physics.Solver == nullptr);
		Physics.bUsingPhysics = true;

		Physics.Module = FChaosSolversModule::GetModule();
		Physics.Solver = GetWorld()->GetPhysicsScene()->GetSolver();

		const UObject* ExistingOwner = Physics.Solver->GetOwner();
		if (!ExistingOwner)
		{
			Physics.Solver->SetOwner(GetWorld());
		}
		else
		{
			// We don't care if the solver's owner is the actual world or not, just as long as the owner's GetWorld() matches our GetWorld().
			npEnsureMsgf(ExistingOwner->GetWorld() == GetWorld(), TEXT("Mismatch worlds betwen Solver %s and NP %s"), *GetNameSafe(ExistingOwner->GetWorld()), *GetNameSafe(GetWorld()));
		}

		npCheckSlow(GEngine);
		if (!GEngine->bUseFixedFrameRate)
		{
			npEnsure(Settings.bForceEngineFixTickForcePhysics);
			GEngine->bUseFixedFrameRate = true;
			GEngine->FixedFrameRate = Settings.FixedTickFrameRate;
		}
	}
}

void UNetworkPredictionWorldManager::EnsurePhysicsGTSync(const TCHAR* Context) const
{
#if NP_ENSURE_PHYSICS_GT_SYNC
	for (const TUniquePtr<IPhysicsService>& Ptr : Services.FixedPhysics.Array)
	{
		Ptr->EnsureDataInSync(Context);
	}
#endif
}

ENetworkPredictionTickingPolicy UNetworkPredictionWorldManager::PreferredDefaultTickingPolicy() const
{
	return Settings.PreferredTickingPolicy;
}

