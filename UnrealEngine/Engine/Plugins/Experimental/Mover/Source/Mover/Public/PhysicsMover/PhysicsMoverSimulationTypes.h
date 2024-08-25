// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Chaos/GeometryParticlesfwd.h"
#include "MoverSimulationTypes.h"
#include "MoveLibrary/FloorQueryUtils.h"
#include "UObject/Interface.h"
#include "UObject/WeakInterfacePtr.h"
#include "PhysicsMoverSimulationTypes.generated.h"

//////////////////////////////////////////////////////////////////////////

namespace Chaos
{
	class FCharacterGroundConstraint;
	class FCharacterGroundConstraintHandle;
	class FCollisionContactModifier;
}

class UPrimitiveComponent;

//////////////////////////////////////////////////////////////////////////
// Debug

struct FPhysicsDrivenMotionDebugParams
{
	float TeleportThreshold = 100.0f;
	float MinStepUpDistance = 5.0f;
	bool EnableMultithreading = true;
	bool DebugDrawGroundQueries = false;
};

#ifndef PHYSICSDRIVENMOTION_DEBUG_DRAW
#define PHYSICSDRIVENMOTION_DEBUG_DRAW (!(UE_BUILD_SHIPPING || UE_BUILD_TEST) || WITH_EDITOR)
#endif

//////////////////////////////////////////////////////////////////////////
// Async update

struct FPhysicsMoverSimulationTickParams
{
	float SimTimeSeconds = 0.0f;
	float DeltaTimeSeconds = 0.0f;
};

struct FPhysicsMoverAsyncInput
{
	bool IsValid() const
	{
		return MoverSimulation.IsValid() && MoverIdx.IsValid();
	}

	// Input is modified during ProcessInputs_Internal
	mutable FMoverInputCmdContext InputCmd;
	mutable FMoverSyncState SyncState;

	TWeakObjectPtr<class UMoverNetworkPhysicsLiaisonComponent> MoverSimulation;
	Chaos::FUniqueIdx MoverIdx;
};

struct FPhysicsMoverAsyncOutput
{
	FMoverSyncState SyncState;
	FFloorCheckResult FloorResult;
	bool bIsValid = false;
};

//////////////////////////////////////////////////////////////////////////
// Movement modes

struct FPhysicsMoverSimulationContactModifierParams
{
	Chaos::FCharacterGroundConstraintHandle* ConstraintHandle;
	UPrimitiveComponent* UpdatedPrimitive;
};

/**
 * UPhysicsCharacterMovementModeInterface: Interface for movement modes that are for physics driven motion
 * A physics driven motion mode needs to update the character ground constraint with the
 * parameters associated with that mode
 */
UINTERFACE(MinimalAPI)
class UPhysicsCharacterMovementModeInterface : public UInterface
{
	GENERATED_BODY()
};

class IPhysicsCharacterMovementModeInterface
{
	GENERATED_BODY()

public:
	// Update the constraint settings on the game thread
	virtual void UpdateConstraintSettings(Chaos::FCharacterGroundConstraint& Constraint) const = 0;

	// Optionally run contact modification on the physics thread
	virtual void OnContactModification_Internal(const FPhysicsMoverSimulationContactModifierParams& Params, Chaos::FCollisionContactModifier& Modifier) const {};
};

//////////////////////////////////////////////////////////////////////////
// FMovementSettingsInput

// Data block containing movement settings inputs that are networked from client to server.
// This is useful if settings changes need to be predicted on the client and synced on the server.
// Also supports rewind/resimulation of settings changes.
USTRUCT(BlueprintType)
struct MOVER_API FMovementSettingsInputs : public FMoverDataStructBase
{
	GENERATED_USTRUCT_BODY()

public:
	// Maximum speed in cm/s
	UPROPERTY(BlueprintReadWrite, Category = Mover)
		float MaxSpeed;

	// Maximum acceleration in cm/s^2
	UPROPERTY(BlueprintReadWrite, Category = Mover)
		float Acceleration;

	FMovementSettingsInputs()
		: MaxSpeed(800.0f)
		, Acceleration(4000.0f)
	{
	}

	virtual ~FMovementSettingsInputs() {}

	// @return newly allocated copy of this FMovementSettingsInputs. Must be overridden by child classes
	virtual FMoverDataStructBase* Clone() const override;

	virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override;

	virtual UScriptStruct* GetScriptStruct() const override { return StaticStruct(); }

	virtual void ToString(FAnsiStringBuilderBase& Out) const override;

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override { Super::AddReferencedObjects(Collector); }
};

template<>
struct TStructOpsTypeTraits< FMovementSettingsInputs > : public TStructOpsTypeTraitsBase2< FMovementSettingsInputs >
{
	enum
	{
		WithNetSerializer = true,
		WithCopy = true
	};
};