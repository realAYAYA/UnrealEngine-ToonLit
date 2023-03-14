// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "GeometryParticleBuffer.h"

namespace Chaos
{
	
class FKinematicGeometryParticleBuffer : public FGeometryParticleBuffer
{
	using FGeometryParticleBuffer::MDirtyFlags;
	using FGeometryParticleBuffer::Proxy;

public:
	FKinematicGeometryParticleBuffer(const FKinematicGeometryParticleParameters& KinematicParams = FKinematicGeometryParticleParameters())
	: FGeometryParticleBuffer(KinematicParams)
	{
		this->Type = EParticleType::Kinematic;
		KinematicGeometryParticleDefaultConstruct<FReal, 3>(*this, KinematicParams);
	}

	static const FKinematicGeometryParticleBuffer* Cast(const FGeometryParticleBuffer* Buffer)
	{
		return Buffer && Buffer->ObjectType() >= EParticleType::Kinematic ? static_cast<const FKinematicGeometryParticleBuffer*>(Buffer) : nullptr;
	}

	static FKinematicGeometryParticleBuffer* Cast(FGeometryParticleBuffer* Buffer)
	{
		return Buffer && Buffer->ObjectType() >= EParticleType::Kinematic ? static_cast<FKinematicGeometryParticleBuffer*>(Buffer) : nullptr;
	}

	const FVec3& V() const { return MVelocities.Read().V(); }
	void SetV(const FVec3& InV, bool bInvalidate = true);

	const FVec3& W() const { return MVelocities.Read().W(); }
	void SetW(const FVec3& InW, bool bInvalidate = true);

	void SetVelocities(const FParticleVelocities& InVelocities, bool bInvalidate = true)
	{
		MVelocities.Write(InVelocities, bInvalidate, MDirtyFlags, Proxy);
	}

	EObjectStateType ObjectState() const;

private:
	TChaosProperty<FParticleVelocities, EChaosProperty::Velocities> MVelocities;

protected:
	virtual void SyncRemoteDataImp(FDirtyPropertiesManager& Manager, int32 DataIdx, const FDirtyChaosProperties& RemoteData) const
	{
		FGeometryParticleBuffer::SyncRemoteDataImp(Manager, DataIdx, RemoteData);
		MVelocities.SyncRemote(Manager, DataIdx, RemoteData);
	}
};

}