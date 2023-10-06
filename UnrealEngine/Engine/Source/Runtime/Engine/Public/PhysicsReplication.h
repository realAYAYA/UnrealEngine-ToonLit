// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PhysicsReplication.h
	Manage replication of physics bodies
=============================================================================*/

#pragma once

#include "Engine/EngineTypes.h"
#include "Engine/ReplicatedState.h"
#include "PhysicsReplicationInterface.h"
#include "Physics/PhysicsInterfaceDeclares.h"
#include "PhysicsProxy/SingleParticlePhysicsProxyFwd.h"
#include "Chaos/Particles.h"
#include "Chaos/PhysicsObject.h"
#include "Chaos/SimCallbackObject.h"
#include "Physics/PhysicsInterfaceUtils.h"

namespace CharacterMovementCVars
{
	extern ENGINE_API int32 SkipPhysicsReplication;
	extern ENGINE_API float NetPingExtrapolation;
	extern ENGINE_API float NetPingLimit;
	extern ENGINE_API float ErrorPerLinearDifference;
	extern ENGINE_API float ErrorPerAngularDifference;
	extern ENGINE_API float ErrorAccumulationSeconds;
	extern ENGINE_API float ErrorAccumulationDistanceSq;
	extern ENGINE_API float ErrorAccumulationSimilarity;
	extern ENGINE_API float MaxLinearHardSnapDistance;
	extern ENGINE_API float MaxRestoredStateError;
	extern ENGINE_API float PositionLerp;
	extern ENGINE_API float LinearVelocityCoefficient;
	extern ENGINE_API float AngleLerp;
	extern ENGINE_API float AngularVelocityCoefficient;
	extern ENGINE_API int32 AlwaysHardSnap;
	extern ENGINE_API int32 AlwaysResetPhysics;
	extern ENGINE_API int32 ApplyAsyncSleepState;
}

#if !UE_BUILD_SHIPPING
namespace PhysicsReplicationCVars
{
	extern ENGINE_API int32 LogPhysicsReplicationHardSnaps;
}
#endif

class FPhysScene_PhysX;


// -------- Async Flow ------->

struct FPhysicsRepErrorCorrectionData
{
	float LinearVelocityCoefficient;
	float AngularVelocityCoefficient;
	float PositionLerp;
	float AngleLerp;
};

/** Final computed desired state passed into the physics sim */
struct FPhysicsRepAsyncInputData
{
	FRigidBodyState TargetState;
	Chaos::FSingleParticlePhysicsProxy* Proxy; // Used for legacy (BodyInstance) flow
	Chaos::FPhysicsObject* PhysicsObject;
	TOptional<FPhysicsRepErrorCorrectionData> ErrorCorrection;
	EPhysicsReplicationMode RepMode;
	int32 ServerFrame;
	int32 FrameOffset;
	float LatencyOneWay;

	FPhysicsRepAsyncInputData()
		: Proxy(nullptr)
		, PhysicsObject(nullptr)
		, RepMode(EPhysicsReplicationMode::Default)
	{};
};

struct FPhysicsReplicationAsyncInput : public Chaos::FSimCallbackInput
{
	FPhysicsRepErrorCorrectionData ErrorCorrection;
	TArray<FPhysicsRepAsyncInputData> InputData;
	void Reset()
	{
		InputData.Reset();
	}
};


struct FReplicatedPhysicsTargetAsync
{
	FReplicatedPhysicsTargetAsync()
		: AccumulatedErrorSeconds(0.0f)
		, ServerFrame(0)
		, PhysicsObject(nullptr)
	{ }

	/** The target state replicated by server */
	FRigidBodyState TargetState;

	/** Physics sync error accumulation */
	float AccumulatedErrorSeconds;

	/** The amount of simulation ticks this target has been used for */
	int32 TickCount;

	/** ServerFrame this target was replicated on (must be converted to local frame prior to client-side use) */
	int32 ServerFrame;

	/** The frame offset between local client and server */
	int32 FrameOffset;

	/** Index of physics object on component */
	Chaos::FPhysicsObject* PhysicsObject;

	/** The replication mode this PhysicsObject should use */
	EPhysicsReplicationMode RepMode;

	/** Correction values from previous update */
	FVector PrevPosTarget;
	FQuat PrevRotTarget;
	FVector PrevPos;
	FVector PrevLinVel;
	int32 PrevServerFrame;

	/** If this target is waiting for up-to-date data? */
	bool bWaiting;
};

class FPhysicsReplicationAsync : public Chaos::TSimCallbackObject<
	FPhysicsReplicationAsyncInput,
	Chaos::FSimCallbackNoOutput,
	Chaos::ESimCallbackOptions::Presimulate>
{
	virtual FName GetFNameForStatId() const override;
	virtual void OnPreSimulate_Internal() override;
	virtual void ApplyTargetStatesAsync(const float DeltaSeconds, const FPhysicsRepErrorCorrectionData& ErrorCorrection, const TArray<FPhysicsRepAsyncInputData>& TargetStates);

	// Replication functions
	virtual void DefaultReplication_DEPRECATED(Chaos::FRigidBodyHandle_Internal* Handle, const FPhysicsRepAsyncInputData& State, const float DeltaSeconds, const FPhysicsRepErrorCorrectionData& ErrorCorrection);
	virtual bool DefaultReplication(Chaos::FPBDRigidParticleHandle* Handle, FReplicatedPhysicsTargetAsync& Target, const float DeltaSeconds);
	virtual bool PredictiveInterpolation(Chaos::FPBDRigidParticleHandle* Handle, FReplicatedPhysicsTargetAsync& Target, const float DeltaSeconds);
	virtual bool ResimulationReplication(Chaos::FPBDRigidParticleHandle* Handle, FReplicatedPhysicsTargetAsync& Target, const float DeltaSeconds);

private:
	float LatencyOneWay;
	FRigidBodyErrorCorrection ErrorCorrectionDefault;
	TMap<Chaos::FPhysicsObject*, FReplicatedPhysicsTargetAsync> ObjectToTarget;

private:
	void UpdateAsyncTarget(const FPhysicsRepAsyncInputData& Input);
	void UpdateRewindDataTarget(const FPhysicsRepAsyncInputData& Input);

public:
	void Setup(FRigidBodyErrorCorrection ErrorCorrection)
	{
		ErrorCorrectionDefault = ErrorCorrection;
	}
};

// <-------- Async Flow --------



struct FReplicatedPhysicsTarget
{
	FReplicatedPhysicsTarget()
		: ArrivedTimeSeconds(0.0f)
		, AccumulatedErrorSeconds(0.0f)
		, ServerFrame(0)
		, PhysicsObject(nullptr)
	{ }

	/** The target state replicated by server */
	FRigidBodyState TargetState;

	/** The bone name used to find the body */
	FName BoneName;

	/** Client time when target state arrived */
	float ArrivedTimeSeconds;

	/** Physics sync error accumulation */
	float AccumulatedErrorSeconds;

	/** Correction values from previous update */
	FVector PrevPosTarget;
	FVector PrevPos;

	/** ServerFrame this target was replicated on (must be converted to local frame prior to client-side use) */
	int32 ServerFrame;

	/** Index of physics object on component */
	Chaos::FPhysicsObject* PhysicsObject;

	/** The replication mode the target should be used with */
	EPhysicsReplicationMode ReplicationMode;

#if !UE_BUILD_SHIPPING
	FDebugFloatHistory ErrorHistory;
#endif
};

class FPhysicsReplication : public IPhysicsReplication
{
public:
	ENGINE_API FPhysicsReplication(FPhysScene* PhysScene);
	ENGINE_API virtual ~FPhysicsReplication();

	/** Helper method so the Skip Replication CVar can be check elsewhere (including game extensions to this class) */
	static ENGINE_API bool ShouldSkipPhysicsReplication();

	/** Tick and update all body states according to replicated targets */
	ENGINE_API virtual void Tick(float DeltaSeconds) override;

	/** Sets the latest replicated target for a body instance */
	UE_DEPRECATED(5.1, "SetReplicatedTarget now takes the ServerFrame.  Please update calls and overloads.")
	virtual void SetReplicatedTarget(UPrimitiveComponent* Component, FName BoneName, const FRigidBodyState& ReplicatedTarget) { SetReplicatedTarget(Component, BoneName, ReplicatedTarget, 0); }
	ENGINE_API virtual void SetReplicatedTarget(UPrimitiveComponent* Component, FName BoneName, const FRigidBodyState& ReplicatedTarget, int32 ServerFrame) override;
private:
	ENGINE_API void SetReplicatedTarget(Chaos::FPhysicsObject* PhysicsObject, const FRigidBodyState& ReplicatedTarget, int32 ServerFrame, EPhysicsReplicationMode ReplicationMode = EPhysicsReplicationMode::Default);

public:
	/** Remove the replicated target*/
	ENGINE_API virtual void RemoveReplicatedTarget(UPrimitiveComponent* Component) override;

protected:

	/** Update the physics body state given a set of replicated targets */
	ENGINE_API virtual void OnTick(float DeltaSeconds, TMap<TWeakObjectPtr<UPrimitiveComponent>, FReplicatedPhysicsTarget>& ComponentsToTargets);
	virtual void OnTargetRestored(TWeakObjectPtr<UPrimitiveComponent> Component, const FReplicatedPhysicsTarget& Target) {}
	virtual void OnSetReplicatedTarget(UPrimitiveComponent* Component, FName BoneName, const FRigidBodyState& ReplicatedTarget, int32 ServerFrame, FReplicatedPhysicsTarget& Target) {}

	/** Called when a dynamic rigid body receives a physics update */
	ENGINE_API virtual bool ApplyRigidBodyState(float DeltaSeconds, FBodyInstance* BI, FReplicatedPhysicsTarget& PhysicsTarget, const FRigidBodyErrorCorrection& ErrorCorrection, const float PingSecondsOneWay, int32 LocalFrame, int32 NumPredictedFrames);
	ENGINE_API virtual bool ApplyRigidBodyState(float DeltaSeconds, FBodyInstance* BI, FReplicatedPhysicsTarget& PhysicsTarget, const FRigidBodyErrorCorrection& ErrorCorrection, const float PingSecondsOneWay, bool* bDidHardSnap = nullptr); // deprecated path with no localframe/numpredicted

	ENGINE_API UWorld* GetOwningWorld();
	ENGINE_API const UWorld* GetOwningWorld() const;

private:

	/** Get the ping from this machine to the server */
	ENGINE_API float GetLocalPing() const;

	/** Get the ping from  */
	ENGINE_API float GetOwnerPing(const AActor* const Owner, const FReplicatedPhysicsTarget& Target) const;

private:
	TMap<TWeakObjectPtr<UPrimitiveComponent>, FReplicatedPhysicsTarget> ComponentToTargets_DEPRECATED; // This collection is keeping the legacy flow working until fully deprecated in a future release
	TArray<FReplicatedPhysicsTarget> ReplicatedTargetsQueue;
	FPhysScene* PhysScene;

	FPhysicsReplicationAsync* PhysicsReplicationAsync;
	FPhysicsReplicationAsyncInput* AsyncInput;	//async data being written into before we push into callback

	ENGINE_API void PrepareAsyncData_External(const FRigidBodyErrorCorrection& ErrorCorrection);	//prepare async data for writing. Call on external thread (i.e. game thread)
};
