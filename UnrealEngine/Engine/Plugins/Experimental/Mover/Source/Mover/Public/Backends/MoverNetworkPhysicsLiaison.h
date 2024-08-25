// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Chaos/Character/CharacterGroundConstraint.h"
#include "Chaos/GeometryParticlesfwd.h"
#include "Backends/MoverBackendLiaison.h"
#include "MoverComponent.h"
#include "Physics/NetworkPhysicsComponent.h"
#include "PhysicsMover/PhysicsMoverSimulationTypes.h"
#include "MoverNetworkPhysicsLiaison.generated.h"

class UCommonLegacyMovementSettings;


//////////////////////////////////////////////////////////////////////////
// Physics networking

USTRUCT()
struct MOVER_API FNetworkPhysicsMoverInputs : public FNetworkPhysicsData
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FMoverInputCmdContext InputCmdContext;

	/**  Apply the data onto the network physics component */
	virtual void ApplyData(UActorComponent* NetworkComponent) const override;

	/**  Build the data from the network physics component */
	virtual void BuildData(const UActorComponent* NetworkComponent) override;

	/**  Serialize data function that will be used to transfer the struct across the network */
	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);

	/** Interpolate the data in between two inputs data */
	virtual void InterpolateData(const FNetworkPhysicsData& MinData, const FNetworkPhysicsData& MaxData) override;
	
	/** Merge data into this input */
	virtual void MergeData(const FNetworkPhysicsData& FromData) override;

	/** Check input data is valid - Input is send from client to server, no need to make sure it's reasonable */
	virtual void ValidateData(const UActorComponent* NetworkComponent) override;
};

template<>
struct TStructOpsTypeTraits<FNetworkPhysicsMoverInputs> : public TStructOpsTypeTraitsBase2<FNetworkPhysicsMoverInputs>
{
	enum
	{
		WithNetSerializer = true,
	};
};

USTRUCT()
struct MOVER_API FNetworkPhysicsMoverState : public FNetworkPhysicsData
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FMoverSyncState SyncStateContext;

	/**  Apply the data onto the network physics component */
	virtual void ApplyData(UActorComponent* NetworkComponent) const override;

	/**  Build the data from the network physics component */
	virtual void BuildData(const UActorComponent* NetworkComponent) override;

	/**  Serialize data function that will be used to transfer the struct across the network */
	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);

	/** Interpolate the data in between two inputs data */
	virtual void InterpolateData(const FNetworkPhysicsData& MinData, const FNetworkPhysicsData& MaxData) override;
};

template<>
struct TStructOpsTypeTraits<FNetworkPhysicsMoverState> : public TStructOpsTypeTraitsBase2<FNetworkPhysicsMoverState>
{
	enum
	{
		WithNetSerializer = true,
	};
};

struct MOVER_API FNetworkPhysicsMoverTraits
{
	using InputsType = FNetworkPhysicsMoverInputs;
	using StatesType = FNetworkPhysicsMoverState;
};

//////////////////////////////////////////////////////////////////////////
// UMoverNetworkPhysicsLiaisonComponent

/**
 * MoverPhysicsLiaisonComponent: This component acts as a middleman between an actor's Mover component and the chaos physics network prediction system.
 * This class is set on a Mover component as the "back end".
 */
UCLASS()
class MOVER_API UMoverNetworkPhysicsLiaisonComponent : public UActorComponent, public IMoverBackendLiaisonInterface
{
	GENERATED_BODY()

public:
	UMoverNetworkPhysicsLiaisonComponent();

	// IMoverBackendLiaisonInterface
	virtual float GetCurrentSimTimeMs() override;
	virtual int32 GetCurrentSimFrame() override;

	// UObject interface
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void InitializeComponent() override;
	virtual void UninitializeComponent() override;
	virtual bool ShouldCreatePhysicsState() const override;
	virtual bool HasValidPhysicsState() const override;
	virtual void OnCreatePhysicsState() override;
	virtual void OnDestroyPhysicsState() override;
	virtual bool CanCreatePhysics() const;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Required for Network Physics Rewind/Resim data
	void GetCurrentInputData(OUT FMoverInputCmdContext& InputCmd) const;
	void SetCurrentInputData(const FMoverInputCmdContext& InputCmd);
	void GetCurrentStateData(OUT FMoverSyncState& SyncState) const;
	void SetCurrentStateData(const FMoverSyncState& SyncState);
	bool ValidateInputData(FMoverInputCmdContext& InputCmd) const;

	// Used by the manager to uniquely identify the component
	Chaos::FUniqueIdx GetUniqueIdx() const;

	void ProduceInput_External(float DeltaSeconds, OUT FPhysicsMoverAsyncInput& Input);
	void ConsumeOutput_External(const FPhysicsMoverAsyncOutput& Output, const double OutputTimeInSeconds);

	void ProcessInputs_Internal(int32 PhysicsStep, float DeltaTime, const FPhysicsMoverAsyncInput& Input) const;
	void OnPreSimulate_Internal(const FPhysicsMoverSimulationTickParams& TickParams, const FPhysicsMoverAsyncInput& Input, OUT FPhysicsMoverAsyncOutput& SimOutput) const;
	void OnContactModification_Internal(const FPhysicsMoverAsyncInput& Input, Chaos::FCollisionContactModifier& Modifier) const;

protected:
	UFUNCTION()
	void OnComponentPhysicsStateChanged(UPrimitiveComponent* ChangedComponent, EComponentPhysicsStateChange StateChange);

	bool HasValidState() const;

	void DestroyConstraint();
	void SetupConstraint();

	void UpdateConstraintSettings();
	void TeleportParticle(Chaos::FGeometryParticleHandle* Particle, const FVector& Position, const FQuat& Rotation) const;
	void WakeParticleIfSleeping(Chaos::FGeometryParticleHandle* Particle) const;

	int32 GetNetworkPhysicsTickOffset() const;

	// Time step on the physics thread when using async physics
	FMoverTimeStep GetCurrentAsyncMoverTimeStep_Internal() const;

	// Time step on the game thread when using async physics. Uses physics results time
	FMoverTimeStep GetCurrentAsyncMoverTimeStep_External() const;

	// Time step on the game thread when using non-async physics
	FMoverTimeStep GetCurrentMoverTimeStep(float DeltaSeconds) const;

	// These are written to by the network input and state data
	FMoverInputCmdContext NetInputCmd;
	FMoverSyncState NetSyncState;

	TUniquePtr<Chaos::FCharacterGroundConstraint> Constraint;
	TObjectPtr<UMoverComponent> MoverComp;
	TObjectPtr<UCommonLegacyMovementSettings> CommonMovementSettings;

	TObjectPtr<UNetworkPhysicsComponent> NetworkPhysicsComponent;

	// The cached physics state is the latest output from the physics thread
	// This can be different from the cached sync state on the mover component
	// which is interpolated to match the interpolated physics particle
	// Note: Time is physics solver time
	FMoverSyncState CachedLastPhysicsSyncState;
	double CachedLastPhysicsSyncStateOutputTime;
	bool bCachedLastPhysicsSyncStateIsValid = false;

	bool bCachedInputIsValid = false;

	bool bUsingAsyncPhysics = false;
};