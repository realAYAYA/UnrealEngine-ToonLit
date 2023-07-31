// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/EngineTypes.h"
#include "Units/RigUnit.h"
#include "RigUnit_WorldCollision.generated.h"

/**
 * Sweeps a sphere against the world and return the first blocking hit using a specific channel
 */
USTRUCT(meta=(DisplayName="Sphere Trace", Category="Collision", DocumentationPolicy = "Strict", Keywords="Sweep,Raytrace,Collision,Collide,Trace", Varying, NodeColor = "0.2 0.4 0.7", Deprecated = "5.0"))
struct CONTROLRIG_API FRigUnit_SphereTraceWorld : public FRigUnit
{
	GENERATED_BODY()

	FRigUnit_SphereTraceWorld()
	: Start(EForceInit::ForceInitToZero)
	, End(EForceInit::ForceInitToZero)
	, Channel(ECC_Visibility)
	, Radius(5.f)
	, bHit(false)
	, HitLocation(EForceInit::ForceInitToZero)
	, HitNormal(0.f, 0.f, 1.f)
	{

	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	/** Start of the trace in rig / global space */
	UPROPERTY(meta = (Input))
	FVector Start;

	/** End of the trace in rig / global space */
	UPROPERTY(meta = (Input))
	FVector End;

	/** The 'channel' that this trace is in, used to determine which components to hit */
	UPROPERTY(meta = (Input))
	TEnumAsByte<ECollisionChannel> Channel;

	/** Radius of the sphere to use for sweeping / tracing */
	UPROPERTY(meta = (Input, UIMin = "0.0", UIMax = "100.0"))
	float Radius;

	/** Returns true if there was a hit */
	UPROPERTY(meta = (Output))
	bool bHit;

	/** Hit location in rig / global Space */
	UPROPERTY(meta = (Output))
	FVector HitLocation;
	
	/** Hit normal in rig / global Space */
	UPROPERTY(meta = (Output))
	FVector HitNormal;

	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};

/**
 * Sweeps a sphere against the world and return the first blocking hit using a specific channel. Target objects can have different object types, but they need to have the same trace channel set to "block" in their collision response settings. 
 * You can create custom trace channels in Project Setting - Collision. 
 */
USTRUCT(meta = (DisplayName = "Sphere Trace By Trace Channel", Category = "Collision", DocumentationPolicy = "Strict", Keywords = "Sweep,Raytrace,Collision,Collide,Trace", Varying, NodeColor = "0.2 0.4 0.7"))
struct CONTROLRIG_API FRigUnit_SphereTraceByTraceChannel : public FRigUnit
{
	GENERATED_BODY()

	FRigUnit_SphereTraceByTraceChannel()
		: Start(EForceInit::ForceInitToZero)
		, End(EForceInit::ForceInitToZero)
		, TraceChannel(TraceTypeQuery1)
		, Radius(5.f)
		, bHit(false)
		, HitLocation(EForceInit::ForceInitToZero)
		, HitNormal(0.f, 0.f, 1.f)
	{

	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	/** Start of the trace in rig / global space */
	UPROPERTY(meta = (Input))
	FVector Start;

	/** End of the trace in rig / global space */
	UPROPERTY(meta = (Input))
	FVector End;

	/** The 'channel' that this trace is in, used to determine which components to hit */
	UPROPERTY(meta = (Input))
	TEnumAsByte<ETraceTypeQuery> TraceChannel;

	/** Radius of the sphere to use for sweeping / tracing */
	UPROPERTY(meta = (Input, UIMin = "0.0", UIMax = "100.0"))
	float Radius;

	/** Returns true if there was a hit */
	UPROPERTY(meta = (Output))
	bool bHit;

	/** Hit location in rig / global Space */
	UPROPERTY(meta = (Output))
	FVector HitLocation;

	/** Hit normal in rig / global Space */
	UPROPERTY(meta = (Output))
	FVector HitNormal;
};

/**
 * Sweeps a sphere against the world and return the first blocking hit. The trace is filtered by object types only, the collision response settings are ignored.
 * You can create custom object types in Project Setting - Collision
 */
USTRUCT(meta = (DisplayName = "Sphere Trace By Object Types", Category = "Collision", DocumentationPolicy = "Strict", Keywords = "Sweep,Raytrace,Collision,Collide,Trace", Varying, NodeColor = "0.2 0.4 0.7"))
struct CONTROLRIG_API FRigUnit_SphereTraceByObjectTypes : public FRigUnit
{
	GENERATED_BODY()

		FRigUnit_SphereTraceByObjectTypes()
		: Start(EForceInit::ForceInitToZero)
		, End(EForceInit::ForceInitToZero)
		, ObjectTypes()
		, Radius(5.f)
		, bHit(false)
		, HitLocation(EForceInit::ForceInitToZero)
		, HitNormal(0.f, 0.f, 1.f)
	{

	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	/** Start of the trace in rig / global space */
	UPROPERTY(meta = (Input))
	FVector Start;

	/** End of the trace in rig / global space */
	UPROPERTY(meta = (Input))
	FVector End;

	/** The types of objects that this trace can hit */
	UPROPERTY(meta = (Input))
	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;

	/** Radius of the sphere to use for sweeping / tracing */
	UPROPERTY(meta = (Input, UIMin = "0.0", UIMax = "100.0"))
	float Radius;

	/** Returns true if there was a hit */
	UPROPERTY(meta = (Output))
	bool bHit;

	/** Hit location in rig / global Space */
	UPROPERTY(meta = (Output))
	FVector HitLocation;

	/** Hit normal in rig / global Space */
	UPROPERTY(meta = (Output))
	FVector HitNormal;
};
