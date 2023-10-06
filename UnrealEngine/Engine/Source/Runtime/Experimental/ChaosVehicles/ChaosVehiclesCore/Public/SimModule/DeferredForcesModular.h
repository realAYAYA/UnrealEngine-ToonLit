// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Chaos/ParticleHandleFwd.h"
#include "GeometryCollection/ManagedArray.h"

enum class EForceFlags : uint32
{
	None = 0, // No flags.

	AllowSubstepping = 1 << 0,
	AccelChange = 1 << 1,
	VelChange = 1 << 2,
	IsLocalForce = 1 << 3,
	LevelSlope = 1 << 4

};
ENUM_CLASS_FLAGS(EForceFlags);	


class CHAOSVEHICLESCORE_API FDeferredForcesModular
{
public:

	struct FApplyForceData
	{
		FApplyForceData(int TransformIndexIn, const FVector& ForceIn, bool bAllowSubsteppingIn, bool bAccelChangeIn, bool bLevelSlope, const FColor& ColorIn)
			: TransformIndex(TransformIndexIn)
			, Force(ForceIn)
			, Flags(EForceFlags::None)
			, DebugColor(ColorIn)
		{
			Flags |= bAllowSubsteppingIn ? EForceFlags::AllowSubstepping : EForceFlags::None;
			Flags |= bAccelChangeIn ? EForceFlags::AccelChange : EForceFlags::None;
			Flags |= bLevelSlope ? EForceFlags::LevelSlope : EForceFlags::None;
		}

		int TransformIndex;
		FVector Force;
		EForceFlags Flags;
		FColor DebugColor;
	};

	struct FApplyForceAtPositionData
	{
		FApplyForceAtPositionData(int TransformIndexIn, const FVector& ForceIn, const FVector& PositionIn, bool bAllowSubsteppingIn, bool bIsLocalForceIn, bool bLevelSlope, const FColor& ColorIn)
			: TransformIndex(TransformIndexIn)
			, Force(ForceIn)
			, Position(PositionIn)
			, Flags(EForceFlags::None)
			, DebugColor(ColorIn)
		{
			Flags |= bAllowSubsteppingIn ? EForceFlags::AllowSubstepping : EForceFlags::None;
			Flags |= bIsLocalForceIn ? EForceFlags::IsLocalForce : EForceFlags::None;
			Flags |= bLevelSlope ? EForceFlags::LevelSlope : EForceFlags::None;
		}

		int TransformIndex;
		FVector Force;
		FVector Position;
		EForceFlags Flags;
		FColor DebugColor;
	};

	struct FAddTorqueInRadiansData
	{
		FAddTorqueInRadiansData(int TransformIndexIn, const FVector& TorqueIn, bool bAllowSubsteppingIn, bool bAccelChangeIn)
			: TransformIndex(TransformIndexIn)
			, Torque(TorqueIn)
			, Flags(EForceFlags::None)
		{
			Flags |= bAllowSubsteppingIn ? EForceFlags::AllowSubstepping : EForceFlags::None;
			Flags |= bAccelChangeIn ? EForceFlags::AccelChange : EForceFlags::None;
		}

		int TransformIndex;
		FVector Torque;
		EForceFlags Flags;
	};

	struct FAddImpulseData
	{
		FAddImpulseData(int TransformIndexIn, const FVector& ImpulseIn, const bool bVelChangeIn)
			: TransformIndex(TransformIndexIn)
			, Impulse(ImpulseIn)
			, Flags(EForceFlags::None)
		{
			Flags |= bVelChangeIn ? EForceFlags::VelChange : EForceFlags::None;
		}

		int TransformIndex;
		FVector Impulse;
		EForceFlags Flags;
	};

	struct FAddImpulseAtPositionData
	{
		FAddImpulseAtPositionData(int TransformIndexIn, const FVector& ImpulseIn, const FVector& PositionIn)
			: TransformIndex(TransformIndexIn)
			, Impulse(ImpulseIn)
			, Position(PositionIn)
		{

		}

		int TransformIndex;
		FVector Impulse;
		FVector Position;
	};

	void Add(const FApplyForceData& ApplyForceDataIn)
	{
		ApplyForceDatas.Add(ApplyForceDataIn);
	}

	void Add(const FApplyForceAtPositionData& ApplyForceAtPositionDataIn)
	{
		ApplyForceAtPositionDatas.Add(ApplyForceAtPositionDataIn);
	}

	void Add(const FAddTorqueInRadiansData& ApplyTorqueDataIn)
	{
		ApplyTorqueDatas.Add(ApplyTorqueDataIn);
	}

	void Add(const FAddImpulseData& ApplyImpulseDataIn)
	{
		ApplyImpulseDatas.Add(ApplyImpulseDataIn);
	}

	void Add(const FAddImpulseAtPositionData& ApplyImpulseAtPositionDataIn)
	{
		ApplyImpulseAtPositionDatas.Add(ApplyImpulseAtPositionDataIn);
	}	

	Chaos::FPBDRigidClusteredParticleHandle* GetParticle(TArray<Chaos::FPBDRigidClusteredParticleHandle*>& Particles
			, TArray<Chaos::FPBDRigidClusteredParticleHandle*>& ClusterParticles
			, int TransformIndex
			, const TManagedArray<FTransform>& Transforms
			, const TManagedArray<FTransform>& CollectionMassToLocal
			, const TManagedArray<int32>& Parent
			, FTransform& TransformOut);

	void Apply(TArray<Chaos::FPBDRigidClusteredParticleHandle*>& Particles
		, TArray<Chaos::FPBDRigidClusteredParticleHandle*>& ClusterParticles
		, const TManagedArray<FTransform>& Transforms
		, const TManagedArray<FTransform>& CollectionMassToLocal
		, const TManagedArray<int32>& Parent);

private:

	void AddForceAtPosition(Chaos::FPBDRigidClusteredParticleHandle* RigidHandle, const FApplyForceAtPositionData& DataIn, const FTransform& OffsetTransform);
	void AddTorque(Chaos::FPBDRigidClusteredParticleHandle* RigidHandle, const FAddTorqueInRadiansData& DataIn, const FTransform& OffsetTransform);
	void AddForce(Chaos::FPBDRigidClusteredParticleHandle* RigidHandle, const FApplyForceData& DataIn, const FTransform& OffsetTransform);

	TArray<FApplyForceData> ApplyForceDatas;
	TArray<FApplyForceAtPositionData> ApplyForceAtPositionDatas;
	TArray<FAddTorqueInRadiansData> ApplyTorqueDatas;
	TArray<FAddImpulseData> ApplyImpulseDatas;
	TArray<FAddImpulseAtPositionData> ApplyImpulseAtPositionDatas;
};
