// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PhysicsReplication.cpp: Code for updating body instance physics state based on replication
=============================================================================*/ 

#include "PhysicsReplication.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Engine/World.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "PhysicsPublic.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Engine/Player.h"
#include "Physics/PhysicsInterfaceCore.h"
#include "Physics/PhysScene_PhysX.h"
#include "Components/SkeletalMeshComponent.h"
#include "Chaos/ChaosMarshallingManager.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"
#include "PBDRigidsSolver.h"
#include "Chaos/PBDRigidsEvolutionGBF.h"

namespace CharacterMovementCVars
{
	extern int32 NetShowCorrections;
	extern float NetCorrectionLifetime;

	static int32 SkipPhysicsReplication = 0;
	static FAutoConsoleVariableRef CVarSkipPhysicsReplication(TEXT("p.SkipPhysicsReplication"), SkipPhysicsReplication, TEXT(""));

	static float NetPingExtrapolation = -1.0f;
	static FAutoConsoleVariableRef CVarNetPingExtrapolation(TEXT("p.NetPingExtrapolation"), NetPingExtrapolation, TEXT(""));

	static float NetPingLimit = -1.f;
	static FAutoConsoleVariableRef CVarNetPingLimit(TEXT("p.NetPingLimit"), NetPingLimit, TEXT(""));

	static float ErrorPerLinearDifference = -1.0f;
	static FAutoConsoleVariableRef CVarErrorPerLinearDifference(TEXT("p.ErrorPerLinearDifference"), ErrorPerLinearDifference, TEXT(""));

	static float ErrorPerAngularDifference = -1.0f;
	static FAutoConsoleVariableRef CVarErrorPerAngularDifference(TEXT("p.ErrorPerAngularDifference"), ErrorPerAngularDifference, TEXT(""));

	static float ErrorAccumulationSeconds = -1.0f;
	static FAutoConsoleVariableRef CVarErrorAccumulation(TEXT("p.ErrorAccumulationSeconds"), ErrorAccumulationSeconds, TEXT(""));

	static float ErrorAccumulationDistanceSq = -1.0f;
	static FAutoConsoleVariableRef CVarErrorAccumulationDistanceSq(TEXT("p.ErrorAccumulationDistanceSq"), ErrorAccumulationDistanceSq, TEXT(""));

	static float ErrorAccumulationSimilarity = -1.f;
	static FAutoConsoleVariableRef CVarErrorAccumulationSimilarity(TEXT("p.ErrorAccumulationSimilarity"), ErrorAccumulationSimilarity, TEXT(""));

	static float MaxLinearHardSnapDistance = -1.f;
	static FAutoConsoleVariableRef CVarMaxLinearHardSnapDistance(TEXT("p.MaxLinearHardSnapDistance"), MaxLinearHardSnapDistance, TEXT(""));

	static float MaxRestoredStateError = -1.0f;
	static FAutoConsoleVariableRef CVarMaxRestoredStateError(TEXT("p.MaxRestoredStateError"), MaxRestoredStateError, TEXT(""));

	static float PositionLerp = -1.0f;
	static FAutoConsoleVariableRef CVarLinSet(TEXT("p.PositionLerp"), PositionLerp, TEXT(""));

	static float LinearVelocityCoefficient = -1.0f;
	static FAutoConsoleVariableRef CVarLinLerp(TEXT("p.LinearVelocityCoefficient"), LinearVelocityCoefficient, TEXT(""));

	static float AngleLerp = -1.0f;
	static FAutoConsoleVariableRef CVarAngSet(TEXT("p.AngleLerp"), AngleLerp, TEXT(""));

	static float AngularVelocityCoefficient = -1.0f;
	static FAutoConsoleVariableRef CVarAngLerp(TEXT("p.AngularVelocityCoefficient"), AngularVelocityCoefficient, TEXT(""));

	static int32 AlwaysHardSnap = 0;
	static FAutoConsoleVariableRef CVarAlwaysHardSnap(TEXT("p.AlwaysHardSnap"), AlwaysHardSnap, TEXT(""));

	static int32 AlwaysResetPhysics = 0;
	static FAutoConsoleVariableRef CVarAlwaysResetPhysics(TEXT("p.AlwaysResetPhysics"), AlwaysResetPhysics, TEXT(""));

	static int32 ApplyAsyncSleepState = 1;
	static FAutoConsoleVariableRef CVarApplyAsyncSleepState(TEXT("p.ApplyAsyncSleepState"), ApplyAsyncSleepState, TEXT(""));
}

namespace PhysicsReplicationCVars
{
	static int32 SkipSkeletalRepOptimization = 1;
	static FAutoConsoleVariableRef CVarSkipSkeletalRepOptimization(TEXT("p.SkipSkeletalRepOptimization"), SkipSkeletalRepOptimization, TEXT("If true, we don't move the skeletal mesh component during replication. This is ok because the skeletal mesh already polls physx after its results"));
#if !UE_BUILD_SHIPPING
	int32 LogPhysicsReplicationHardSnaps = 0;
	static FAutoConsoleVariableRef CVarLogPhysicsReplicationHardSnaps(TEXT("p.LogPhysicsReplicationHardSnaps"), LogPhysicsReplicationHardSnaps, TEXT(""));
#endif
}

struct FAsyncPhysicsRepCallbackData : public Chaos::FSimCallbackInput
{
	TArray<FAsyncPhysicsDesiredState> Buffer;
	ErrorCorrectionData ErrorCorrection;

	void Reset()
	{
		Buffer.Reset();
	}
};

class FPhysicsReplicationAsyncCallback final : public Chaos::TSimCallbackObject<FAsyncPhysicsRepCallbackData>
{
	virtual void OnPreSimulate_Internal() override
	{
		FPhysicsReplication::ApplyAsyncDesiredState(GetDeltaTime_Internal(), GetConsumerInput_Internal());
	}
};

void ComputeDeltas(const FVector& CurrentPos, const FQuat& CurrentQuat, const FVector& TargetPos, const FQuat& TargetQuat, FVector& OutLinDiff, float& OutLinDiffSize,
	FVector& OutAngDiffAxis, float& OutAngDiff, float& OutAngDiffSize)
{
	OutLinDiff = TargetPos - CurrentPos;
	OutLinDiffSize = OutLinDiff.Size();
	const FQuat InvCurrentQuat = CurrentQuat.Inverse();
	const FQuat DeltaQuat = InvCurrentQuat * TargetQuat;
	DeltaQuat.ToAxisAndAngle(OutAngDiffAxis, OutAngDiff);
	OutAngDiff = FMath::RadiansToDegrees(FMath::UnwindRadians(OutAngDiff));
	OutAngDiffSize = FMath::Abs(OutAngDiff);
}

FPhysicsReplication::~FPhysicsReplication()
{
	if (AsyncCallback)
	{
		if (auto* Solver = PhysScene->GetSolver())
		{
			Solver->UnregisterAndFreeSimCallbackObject_External(AsyncCallback);
		}
	}
}

bool FPhysicsReplication::ApplyRigidBodyState(float DeltaSeconds, FBodyInstance* BI, FReplicatedPhysicsTarget& PhysicsTarget, const FRigidBodyErrorCorrection& ErrorCorrection, const float InPingSecondsOneWay, int32 LocalFrame, int32 NumPredictedFrames)
{
	if (ShouldSkipPhysicsReplication())
	{
		return false;
	}

	if (!BI->IsInstanceSimulatingPhysics())
	{
		return false;
	}

	// LocalFrame the local frame number we should use to access past state in the rewind data
	// NumPredictedFrames is how many frames (steps) ahead "now" is for the client compared to the latest data we've received from the server (use this to determine accurately how far ahead this object should extrapolate from its "physics target"

	Chaos::FPhysicsSolver* Solver = PhysScene->GetSolver();
	Chaos::FRewindData* RewindData = Solver->GetRewindData();

	if (RewindData && LocalFrame > RewindData->GetEarliestFrame_Internal() && LocalFrame < RewindData->CurrentFrame())
	{
		auto Proxy = BI->GetPhysicsActorHandle();
		const auto P = RewindData->GetPastStateAtFrame(*Proxy->GetHandle_LowLevel(), LocalFrame);

#if 0
		// Debugging/test: compare and print out if locations differ
		if (FVector::DistSquared(PhysicsTarget.TargetState.Position, P.X()) > 1.f)
		{
			const int32 EarliestFrame = RewindData->GetEarliestFrame_Internal();
			const int32 CurrentFrame = RewindData->CurrentFrame();

			UE_LOG(LogTemp, Warning, TEXT(""));
			UE_LOG(LogTemp, Warning, TEXT("Location differs"));
			UE_LOG(LogTemp, Warning, TEXT("   Replicated: %s"), *PhysicsTarget.TargetState.Position.ToString());
			UE_LOG(LogTemp, Warning, TEXT("   Historic: %s"), *P.X().ToString());
		}
#endif
	}

	// Call into the old ApplyRigidBodyState function for now,
	// "new leash mode" should replace this.
	// Note that old ApplyRigidBodyState is overridden in other projects, so consider backwards compat path
	float PingSecondsOneWay = RewindData == nullptr ? InPingSecondsOneWay : (Solver->GetLastDt() * NumPredictedFrames) * 0.5f;
	return ApplyRigidBodyState(DeltaSeconds, BI, PhysicsTarget, ErrorCorrection, PingSecondsOneWay);
}

bool FPhysicsReplication::ApplyRigidBodyState(float DeltaSeconds, FBodyInstance* BI, FReplicatedPhysicsTarget& PhysicsTarget, const FRigidBodyErrorCorrection& ErrorCorrection, const float PingSecondsOneWay, bool* bDidHardSnap)
{
	if (ShouldSkipPhysicsReplication())
	{
		return false;
	}

	if (!BI->IsInstanceSimulatingPhysics())
	{
		return false;
	}

	//
	// NOTES:
	//
	// The operation of this method has changed since 4.18.
	//
	// When a new remote physics state is received, this method will
	// be called on tick until the local state is within an adequate
	// tolerance of the new state.
	//
	// The received state is extrapolated based on ping, by some
	// adjustable amount.
	//
	// A correction velocity is added new state's velocity, and assigned
	// to the body. The correction velocity scales with the positional
	// difference, so without the interference of external forces, this
	// will result in an exponentially decaying correction.
	//
	// Generally it is not needed and will interrupt smoothness of
	// the replication, but stronger corrections can be obtained by
	// adjusting position lerping.
	//
	// If progress is not being made towards equilibrium, due to some
	// divergence in physics states between the owning and local sims,
	// an error value is accumulated, representing the amount of time
	// spent in an unresolvable state.
	//
	// Once the error value has exceeded some threshold (0.5 seconds
	// by default), a hard snap to the target physics state is applied.
	//

	bool bRestoredState = true;
	const FRigidBodyState NewState = PhysicsTarget.TargetState;
	const float NewQuatSizeSqr = NewState.Quaternion.SizeSquared();

	// failure cases
	if (!BI->IsInstanceSimulatingPhysics())
	{
		UE_LOG(LogPhysics, Warning, TEXT("Physics replicating on non-simulated body. (%s)"), *BI->GetBodyDebugName());
		return bRestoredState;
	}
	else if (NewQuatSizeSqr < UE_KINDA_SMALL_NUMBER)
	{
		UE_LOG(LogPhysics, Warning, TEXT("Invalid zero quaternion set for body. (%s)"), *BI->GetBodyDebugName());
		return bRestoredState;
	}
	else if (FMath::Abs(NewQuatSizeSqr - 1.f) > UE_KINDA_SMALL_NUMBER)
	{
		UE_LOG(LogPhysics, Warning, TEXT("Quaternion (%f %f %f %f) with non-unit magnitude detected. (%s)"),
			NewState.Quaternion.X, NewState.Quaternion.Y, NewState.Quaternion.Z, NewState.Quaternion.W, *BI->GetBodyDebugName());
		return bRestoredState;
	}

	// Grab configuration variables from engine config or from CVars if overriding is turned on.
	const float NetPingExtrapolation = CharacterMovementCVars::NetPingExtrapolation >= 0.0f ? CharacterMovementCVars::NetPingExtrapolation : ErrorCorrection.PingExtrapolation;
	const float NetPingLimit = CharacterMovementCVars::NetPingLimit > 0.0f ? CharacterMovementCVars::NetPingLimit : ErrorCorrection.PingLimit;
	const float ErrorPerLinearDiff = CharacterMovementCVars::ErrorPerLinearDifference >= 0.0f ? CharacterMovementCVars::ErrorPerLinearDifference : ErrorCorrection.ErrorPerLinearDifference;
	const float ErrorPerAngularDiff = CharacterMovementCVars::ErrorPerAngularDifference >= 0.0f ? CharacterMovementCVars::ErrorPerAngularDifference : ErrorCorrection.ErrorPerAngularDifference;
	const float MaxRestoredStateError = CharacterMovementCVars::MaxRestoredStateError >= 0.0f ? CharacterMovementCVars::MaxRestoredStateError : ErrorCorrection.MaxRestoredStateError;
	const float ErrorAccumulationSeconds = CharacterMovementCVars::ErrorAccumulationSeconds >= 0.0f ? CharacterMovementCVars::ErrorAccumulationSeconds : ErrorCorrection.ErrorAccumulationSeconds;
	const float ErrorAccumulationDistanceSq = CharacterMovementCVars::ErrorAccumulationDistanceSq >= 0.0f ? CharacterMovementCVars::ErrorAccumulationDistanceSq : ErrorCorrection.ErrorAccumulationDistanceSq;
	const float ErrorAccumulationSimilarity = CharacterMovementCVars::ErrorAccumulationSimilarity >= 0.0f ? CharacterMovementCVars::ErrorAccumulationSimilarity : ErrorCorrection.ErrorAccumulationSimilarity;
	const float PositionLerp = CharacterMovementCVars::PositionLerp >= 0.0f ? CharacterMovementCVars::PositionLerp : ErrorCorrection.PositionLerp;
	const float LinearVelocityCoefficient = CharacterMovementCVars::LinearVelocityCoefficient >= 0.0f ? CharacterMovementCVars::LinearVelocityCoefficient : ErrorCorrection.LinearVelocityCoefficient;
	const float AngleLerp = CharacterMovementCVars::AngleLerp >= 0.0f ? CharacterMovementCVars::AngleLerp : ErrorCorrection.AngleLerp;
	const float AngularVelocityCoefficient = CharacterMovementCVars::AngularVelocityCoefficient >= 0.0f ? CharacterMovementCVars::AngularVelocityCoefficient : ErrorCorrection.AngularVelocityCoefficient;
	const float MaxLinearHardSnapDistance = CharacterMovementCVars::MaxLinearHardSnapDistance >= 0.f ? CharacterMovementCVars::MaxLinearHardSnapDistance : ErrorCorrection.MaxLinearHardSnapDistance;

	// Get Current state
	FRigidBodyState CurrentState;
	BI->GetRigidBodyState(CurrentState);

	/////// EXTRAPOLATE APPROXIMATE TARGET VALUES ///////

	// Starting from the last known authoritative position, and
	// extrapolate an approximation using the last known velocity
	// and ping.
	const float PingSeconds = FMath::Clamp(PingSecondsOneWay, 0.f, NetPingLimit);
	const float ExtrapolationDeltaSeconds = PingSeconds * NetPingExtrapolation;
	const FVector ExtrapolationDeltaPos = NewState.LinVel * ExtrapolationDeltaSeconds;
	const FVector_NetQuantize100 TargetPos = NewState.Position + ExtrapolationDeltaPos;
	float NewStateAngVel;
	FVector NewStateAngVelAxis;
	NewState.AngVel.FVector::ToDirectionAndLength(NewStateAngVelAxis, NewStateAngVel);
	NewStateAngVel = FMath::DegreesToRadians(NewStateAngVel);
	const FQuat ExtrapolationDeltaQuaternion = FQuat(NewStateAngVelAxis, NewStateAngVel * ExtrapolationDeltaSeconds);
	FQuat TargetQuat = ExtrapolationDeltaQuaternion * NewState.Quaternion;

	/////// COMPUTE DIFFERENCES ///////
	FVector LinDiff;
	float LinDiffSize;
	FVector AngDiffAxis;
	float AngDiff;
	float AngDiffSize;

	ComputeDeltas(CurrentState.Position, CurrentState.Quaternion, TargetPos, TargetQuat, LinDiff, LinDiffSize, AngDiffAxis, AngDiff, AngDiffSize);

	/////// ACCUMULATE ERROR IF NOT APPROACHING SOLUTION ///////

	// Store sleeping state
	const bool bShouldSleep = (NewState.Flags & ERigidBodyFlags::Sleeping) != 0;
	const bool bWasAwake = BI->IsInstanceAwake();
	const bool bAutoWake = false;

	const float Error = (LinDiffSize * ErrorPerLinearDiff) + (AngDiffSize * ErrorPerAngularDiff);
	bRestoredState = Error < MaxRestoredStateError;
	if (bRestoredState)
	{
		PhysicsTarget.AccumulatedErrorSeconds = 0.0f;
	}
	else
	{
		//
		// The heuristic for error accumulation here is:
		// 1. Did the physics tick from the previous step fail to
		//    move the body towards a resolved position?
		// 2. Was the linear error in the same direction as the
		//    previous frame?
		// 3. Is the linear error large enough to accumulate error?
		//
		// If these conditions are met, then "error" time will accumulate.
		// Once error has accumulated for a certain number of seconds,
		// a hard-snap to the target will be performed.
		//
		// TODO: Rotation while moving linearly can still mess up this
		// heuristic. We need to account for it.
		//

		// Project the change in position from the previous tick onto the
		// linear error from the previous tick. This value roughly represents
		// how much correction was performed over the previous physics tick.
		const float PrevProgress = FVector::DotProduct(
			FVector(CurrentState.Position) - PhysicsTarget.PrevPos,
			(PhysicsTarget.PrevPosTarget - PhysicsTarget.PrevPos).GetSafeNormal());

		// Project the current linear error onto the linear error from the
		// previous tick. This value roughly represents how little the direction
		// of the linear error state has changed, and how big the error is.
		const float PrevSimilarity = FVector::DotProduct(
			TargetPos - FVector(CurrentState.Position),
			PhysicsTarget.PrevPosTarget - PhysicsTarget.PrevPos);

		// If the conditions from the heuristic outlined above are met, accumulate
		// error. Otherwise, reduce it.
		if (PrevProgress < ErrorAccumulationDistanceSq &&
			PrevSimilarity > ErrorAccumulationSimilarity)
		{
			PhysicsTarget.AccumulatedErrorSeconds += DeltaSeconds;
		}
		else
		{
			PhysicsTarget.AccumulatedErrorSeconds = FMath::Max(PhysicsTarget.AccumulatedErrorSeconds - DeltaSeconds, 0.0f);
		}

		// Hard snap if error accumulation or linear error is big enough, and clear the error accumulator.
		const bool bHardSnap =
			LinDiffSize > MaxLinearHardSnapDistance ||
			PhysicsTarget.AccumulatedErrorSeconds > ErrorAccumulationSeconds ||
			CharacterMovementCVars::AlwaysHardSnap;

		const FTransform IdealWorldTM(TargetQuat, TargetPos);

		if (bHardSnap)
		{
#if !UE_BUILD_SHIPPING
			if (PhysicsReplicationCVars::LogPhysicsReplicationHardSnaps && GetOwningWorld())
			{
				UE_LOG(LogTemp, Warning, TEXT("Simulated HARD SNAP - \nCurrent Pos - %s, Target Pos - %s\n CurrentState.LinVel - %s, New Lin Vel - %s\nTarget Extrapolation Delta - %s, Is Replay? - %d, Is Asleep - %d, Prev Progress - %f, Prev Similarity - %f"),
					*CurrentState.Position.ToString(), *TargetPos.ToString(), *CurrentState.LinVel.ToString(), *NewState.LinVel.ToString(),
					*ExtrapolationDeltaPos.ToString(), GetOwningWorld()->IsPlayingReplay(), !BI->IsInstanceAwake(), PrevProgress, PrevSimilarity);
				if (bDidHardSnap)
				{
					*bDidHardSnap = true;
				}
				if (LinDiffSize > MaxLinearHardSnapDistance)
				{
					UE_LOG(LogTemp, Warning, TEXT("Hard snap due to linear difference error"));
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("Hard snap due to accumulated error"))
				}
			}
#endif
			// Too much error so just snap state here and be done with it
			PhysicsTarget.AccumulatedErrorSeconds = 0.0f;
			bRestoredState = true;

			BI->SetBodyTransform(IdealWorldTM, ETeleportType::ResetPhysics, bAutoWake);

			// Set the new velocities
			BI->SetLinearVelocity(NewState.LinVel, false, bAutoWake);
			BI->SetAngularVelocityInRadians(FMath::DegreesToRadians(NewState.AngVel), false, bAutoWake);
		}
		else
		{
			// Small enough error to interpolate
			if (AsyncCallback == nullptr)	//sync case
			{
				const FVector NewLinVel = FVector(NewState.LinVel) + (LinDiff * LinearVelocityCoefficient * DeltaSeconds);
				const FVector NewAngVel = FVector(NewState.AngVel) + (AngDiffAxis * AngDiff * AngularVelocityCoefficient * DeltaSeconds);

				const FVector NewPos = FMath::Lerp(FVector(CurrentState.Position), FVector(TargetPos), PositionLerp);
				const FQuat NewAng = FQuat::Slerp(CurrentState.Quaternion, TargetQuat, AngleLerp);

				BI->SetBodyTransform(FTransform(NewAng, NewPos), ETeleportType::ResetPhysics);
				BI->SetLinearVelocity(NewLinVel, false);
				BI->SetAngularVelocityInRadians(FMath::DegreesToRadians(NewAngVel), false);
			}
			else
			{
				//If async is used, enqueue for callback
				FAsyncPhysicsDesiredState AsyncDesiredState;
				AsyncDesiredState.WorldTM = IdealWorldTM;
				AsyncDesiredState.LinearVelocity = NewState.LinVel;
				AsyncDesiredState.AngularVelocity = NewState.AngVel;
				AsyncDesiredState.Proxy = static_cast<Chaos::FSingleParticlePhysicsProxy*>(BI->GetPhysicsActorHandle());
				AsyncDesiredState.ErrorCorrection = { ErrorCorrection.LinearVelocityCoefficient, ErrorCorrection.AngularVelocityCoefficient, ErrorCorrection.PositionLerp, ErrorCorrection.AngleLerp };
				AsyncDesiredState.bShouldSleep = bShouldSleep;

				CurAsyncData->Buffer.Add(AsyncDesiredState);
			}


		}

		// Should we show the async part?
#if !UE_BUILD_SHIPPING
		if (CharacterMovementCVars::NetShowCorrections != 0)
		{
			PhysicsTarget.ErrorHistory.bAutoAdjustMinMax = false;
			PhysicsTarget.ErrorHistory.MinValue = 0.0f;
			PhysicsTarget.ErrorHistory.MaxValue = 1.0f;
			PhysicsTarget.ErrorHistory.AddSample(PhysicsTarget.AccumulatedErrorSeconds / ErrorAccumulationSeconds);
			if (UWorld* OwningWorld = GetOwningWorld())
			{
				FColor Color = FColor::White;
				DrawDebugDirectionalArrow(OwningWorld, CurrentState.Position, TargetPos, 5.0f, Color, true, CharacterMovementCVars::NetCorrectionLifetime, 0, 1.5f);
				DrawDebugFloatHistory(*OwningWorld, PhysicsTarget.ErrorHistory, CurrentState.Position + FVector(0.0f, 0.0f, 100.0f), FVector2D(100.0f, 50.0f), FColor::White, false, 0, -1);
			}
		}
#endif
	}

	/////// SLEEP UPDATE ///////
	if (bShouldSleep)
	{
		// In the async case, we apply sleep state in ApplyAsyncDesiredState
		if (AsyncCallback == nullptr)
		{
			BI->PutInstanceToSleep();
		}
	}

	PhysicsTarget.PrevPosTarget = TargetPos;
	PhysicsTarget.PrevPos = FVector(CurrentState.Position);

	return bRestoredState;
}

UWorld* FPhysicsReplication::GetOwningWorld()
{
	return PhysScene ? PhysScene->GetOwningWorld() : nullptr;
}

const UWorld* FPhysicsReplication::GetOwningWorld() const
{
	return PhysScene ? PhysScene->GetOwningWorld() : nullptr;
}

float FPhysicsReplication::GetLocalPing() const
{
	if (const UWorld* World = GetOwningWorld())
	{
		if (APlayerController* PlayerController = World->GetFirstPlayerController())
		{
			if (APlayerState* PlayerState = PlayerController->PlayerState)
			{
				return PlayerState->ExactPing;
			}
		}
	}
	return 0.0f;
}

float FPhysicsReplication::GetOwnerPing(const AActor* const Owner, const FReplicatedPhysicsTarget& Target) const
{
	//
	// NOTE: At the moment, we have no real way to objectively access the ping of the
	// authoritative simulation owner to the server, which is what this function
	// claims to return.
	//
	// In order to actually use ping to extrapolate replication, we need to access
	// it with something along the lines of the disabled code below.
	//
#if false
	if (UPlayer* OwningPlayer = OwningActor->GetNetOwningPlayer())
	{
		if (UWorld* World = GetOwningWorld())
		{
			if (APlayerController* PlayerController = OwningPlayer->GetPlayerController(World))
			{
				if (APlayerState* PlayerState = PlayerController->PlayerState)
				{
					return PlayerState->ExactPing;
				}
			}
		}
	}
#endif

	return 0.0f;
}

void FPhysicsReplication::OnTick(float DeltaSeconds, TMap<TWeakObjectPtr<UPrimitiveComponent>, FReplicatedPhysicsTarget>& ComponentsToTargets)
{
	using namespace Chaos;

	int32 LocalFrameOffset = 0;		// LocalFrame = ServerFrame + LocalFrameOffset;
	int32 NumPredictedFrames = 0;	// How many frames "ahead" of the server we are predicting. That is, how many frames are in flight between us and the server.

	if (UWorld* World = GetOwningWorld())
	{
		if (World->GetNetMode() == NM_Client)
		{
			if (APlayerController* PlayerController = World->GetFirstPlayerController())
			{
				Chaos::FPhysicsSolver* Solver = PhysScene->GetSolver();
				check(Solver);

				static IConsoleVariable* EnableNetworkPhysicsCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("np2.EnableNetworkPhysicsPrediction"));
				if (EnableNetworkPhysicsCVar && EnableNetworkPhysicsCVar->GetInt() == 1)
				{
					LocalFrameOffset = PlayerController->GetLocalToServerAsyncPhysicsTickOffset();
				}
				//TODO: as we send physics updates down we need to record latest seen
				//NumPredictedFrames = Solver->GetCurrentFrame() - ClientFrameInfo.LastProcessedInputFrame;
			}
		}
	}

	const FRigidBodyErrorCorrection& PhysicErrorCorrection = UPhysicsSettings::Get()->PhysicErrorCorrection;
	if(AsyncCallback)
	{
		PrepareAsyncData_External(PhysicErrorCorrection);
	}

	// Get the ping between this PC & the server
	const float LocalPing = GetLocalPing();

	for (auto Itr = ComponentsToTargets.CreateIterator(); Itr; ++Itr)
	{
		bool bRemoveItr = false;
		if (UPrimitiveComponent* PrimComp = Itr.Key().Get())
		{
			if (FBodyInstance* BI = PrimComp->GetBodyInstance(Itr.Value().BoneName))
			{
				FReplicatedPhysicsTarget& PhysicsTarget = Itr.Value();
				FRigidBodyState& UpdatedState = PhysicsTarget.TargetState;
				bool bUpdated = false;
				if (AActor* OwningActor = PrimComp->GetOwner())
				{
					const ENetRole OwnerRole = OwningActor->GetLocalRole();
					const bool bIsSimulated = OwnerRole == ROLE_SimulatedProxy;
					const bool bIsReplicatedAutonomous = OwnerRole == ROLE_AutonomousProxy && PrimComp->bReplicatePhysicsToAutonomousProxy;
					if (bIsSimulated || bIsReplicatedAutonomous)
					{
						// Get the ping of this thing's owner. If nobody owns it,
						// then it's server authoritative.
						const float OwnerPing = GetOwnerPing(OwningActor, PhysicsTarget);

						// Get the total ping - this approximates the time since the update was
						// actually generated on the machine that is doing the authoritative sim.
						// NOTE: We divide by 2 to approximate 1-way ping from 2-way ping.
						const float PingSecondsOneWay = (LocalPing + OwnerPing) * 0.5f * 0.001f;

						if (UpdatedState.Flags & ERigidBodyFlags::NeedsUpdate)
						{
							const int32 LocalFrame = PhysicsTarget.ServerFrame + LocalFrameOffset;
							const bool bRestoredState = ApplyRigidBodyState(DeltaSeconds, BI, PhysicsTarget, PhysicErrorCorrection, PingSecondsOneWay, LocalFrame, NumPredictedFrames);
							// Need to update the component to match new position.
							if (PhysicsReplicationCVars::SkipSkeletalRepOptimization == 0 || Cast<USkeletalMeshComponent>(PrimComp) == nullptr)	//simulated skeletal mesh does its own polling of physics results so we don't need to call this as it'll happen at the end of the physics sim
							{
								PrimComp->SyncComponentToRBPhysics();
							}

							if (bRestoredState)
							{
								bRemoveItr = true;
							}
						}
					}
				}
			}
		}

		if (bRemoveItr)
		{
			OnTargetRestored(Itr.Key().Get(), Itr.Value());
			Itr.RemoveCurrent();
		}
	}

	CurAsyncData = nullptr;
}

bool FPhysicsReplication::ShouldSkipPhysicsReplication()
{
	return (CharacterMovementCVars::SkipPhysicsReplication != 0);
}

void FPhysicsReplication::Tick(float DeltaSeconds)
{
	OnTick(DeltaSeconds, ComponentToTargets);
}

FPhysicsReplication::FPhysicsReplication(FPhysScene* InPhysicsScene)
: PhysScene(InPhysicsScene)
{
	using namespace Chaos;
	CurAsyncData = nullptr;
	AsyncCallback = nullptr;
	if (auto* Solver = PhysScene->GetSolver())
	{
		AsyncCallback = Solver->CreateAndRegisterSimCallbackObject_External<FPhysicsReplicationAsyncCallback>();
	}
}

void FPhysicsReplication::PrepareAsyncData_External(const FRigidBodyErrorCorrection& ErrorCorrection)
{
	//todo move this logic into a common function?
	const float PositionLerp = CharacterMovementCVars::PositionLerp >= 0.0f ? CharacterMovementCVars::PositionLerp : ErrorCorrection.PositionLerp;
	const float LinearVelocityCoefficient = CharacterMovementCVars::LinearVelocityCoefficient >= 0.0f ? CharacterMovementCVars::LinearVelocityCoefficient : ErrorCorrection.LinearVelocityCoefficient;
	const float AngleLerp = CharacterMovementCVars::AngleLerp >= 0.0f ? CharacterMovementCVars::AngleLerp : ErrorCorrection.AngleLerp;
	const float AngularVelocityCoefficient = CharacterMovementCVars::AngularVelocityCoefficient >= 0.0f ? CharacterMovementCVars::AngularVelocityCoefficient : ErrorCorrection.AngularVelocityCoefficient;

	CurAsyncData = AsyncCallback->GetProducerInputData_External();
	CurAsyncData->ErrorCorrection.PositionLerp = PositionLerp;
	CurAsyncData->ErrorCorrection.AngleLerp = AngleLerp;
	CurAsyncData->ErrorCorrection.LinearVelocityCoefficient = LinearVelocityCoefficient;
	CurAsyncData->ErrorCorrection.AngularVelocityCoefficient = AngularVelocityCoefficient;
}

void FPhysicsReplication::ApplyAsyncDesiredState(const float DeltaSeconds, const FAsyncPhysicsRepCallbackData* AsyncData)
{
	using namespace Chaos;
	if(AsyncData)
	{
		for (const FAsyncPhysicsDesiredState& State : AsyncData->Buffer)
		{
			float LinearVelocityCoefficient = AsyncData->ErrorCorrection.LinearVelocityCoefficient;
			float AngularVelocityCoefficient = AsyncData->ErrorCorrection.AngularVelocityCoefficient;
			float PositionLerp = AsyncData->ErrorCorrection.PositionLerp;
			float AngleLerp = AsyncData->ErrorCorrection.AngleLerp;
			if (State.ErrorCorrection.IsSet())
			{
				ErrorCorrectionData ECData = State.ErrorCorrection.GetValue();
				LinearVelocityCoefficient = ECData.LinearVelocityCoefficient;
				AngularVelocityCoefficient = ECData.AngularVelocityCoefficient;
				PositionLerp = ECData.PositionLerp;
				AngleLerp = ECData.AngleLerp;
			}
			//Proxy should exist because we are using latest and any pending deletes would have been enqueued after
			Chaos::FSingleParticlePhysicsProxy* Proxy = State.Proxy;
			auto* Handle = Proxy->GetPhysicsThreadAPI();


			if(Handle && Handle->CanTreatAsRigid())
			{
				const FVector TargetPos = State.WorldTM.GetLocation();
				const FQuat TargetQuat = State.WorldTM.GetRotation();

				// Get Current state
				FRigidBodyState CurrentState;
				CurrentState.Position = Handle->X();
				CurrentState.Quaternion = Handle->R();
				CurrentState.AngVel = Handle->W();
				CurrentState.LinVel = Handle->V();

				FVector LinDiff;
				float LinDiffSize;
				FVector AngDiffAxis;
				float AngDiff;
				float AngDiffSize;
				ComputeDeltas(CurrentState.Position, CurrentState.Quaternion, TargetPos, TargetQuat, LinDiff, LinDiffSize, AngDiffAxis, AngDiff, AngDiffSize);

				const FVector NewLinVel = FVector(State.LinearVelocity) + (LinDiff * LinearVelocityCoefficient * DeltaSeconds);
				const FVector NewAngVel = FVector(State.AngularVelocity) + (AngDiffAxis * AngDiff * AngularVelocityCoefficient * DeltaSeconds);

				const FVector NewPos = FMath::Lerp(FVector(CurrentState.Position), TargetPos, PositionLerp);
				const FQuat NewAng = FQuat::Slerp(CurrentState.Quaternion, TargetQuat, AngleLerp);
				
				Handle->SetX(NewPos);
				Handle->SetR(NewAng);
				Handle->SetV(NewLinVel);
				Handle->SetW(FMath::DegreesToRadians(NewAngVel));

				if (State.bShouldSleep)
				{
					// don't allow kinematic to sleeping transition
					if (Handle->ObjectState() != EObjectStateType::Kinematic)
					{
						auto* Solver = Proxy->GetSolver<FPBDRigidsSolver>();
						Solver->GetEvolution()->SetParticleObjectState(Proxy->GetHandle_LowLevel()->CastToRigidParticle(), EObjectStateType::Sleeping);	//todo: move object state into physics thread api
					}
				}
			}
		}
	}
}

void FPhysicsReplication::SetReplicatedTarget(UPrimitiveComponent* Component, FName BoneName, const FRigidBodyState& ReplicatedTarget, int32 ServerFrame)
{
	if (UWorld* OwningWorld = GetOwningWorld())
	{
		//TODO: there's a faster way to compare this
		TWeakObjectPtr<UPrimitiveComponent> TargetKey(Component);
		FReplicatedPhysicsTarget* Target = ComponentToTargets.Find(TargetKey);
		if (!Target)
		{
			// First time we add a target, set it's previous and correction
			// positions to the target position to avoid math with uninitialized
			// memory.
			Target = &ComponentToTargets.Add(TargetKey);
			Target->PrevPos = ReplicatedTarget.Position;
			Target->PrevPosTarget = ReplicatedTarget.Position;
		}

		Target->ServerFrame = ServerFrame;
		Target->TargetState = ReplicatedTarget;
		Target->BoneName = BoneName;
		Target->ArrivedTimeSeconds = OwningWorld->GetTimeSeconds();

		ensure(!Target->PrevPos.ContainsNaN());
		ensure(!Target->PrevPosTarget.ContainsNaN());
		ensure(!Target->TargetState.Position.ContainsNaN());
	}
}

void FPhysicsReplication::RemoveReplicatedTarget(UPrimitiveComponent* Component)
{
	ComponentToTargets.Remove(Component);
}
