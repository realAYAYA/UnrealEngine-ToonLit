// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataWrappers/ChaosVDParticleDataWrapper.h"

// @note: Tracing an scene with 1000 particles moving, Manually serializing the structs is ~20% faster
// than normal UStruct serialization in unversioned mode. As we do this at runtime when tracing in development builds, this is important.
// One of the downside is that this will be more involved to maintain as any versioning needs to be done by hand.

bool FChaosVDFRigidParticleControlFlags::Serialize(FArchive& Ar)
{
	Ar << bGravityEnabled;
	Ar << bCCDEnabled;
	Ar << bOneWayInteractionEnabled;
	Ar << bMaxDepenetrationVelocityOverrideEnabled;
	Ar << bInertiaConditioningEnabled;
	Ar << GravityGroupIndex;

	return true;
}

bool FChaosVDParticlePositionRotation::Serialize(FArchive& Ar)
{
	Ar << MX;
	Ar << MR;
	Ar << bHasValidData;

	return true;
}

bool FChaosVDParticleVelocities::Serialize(FArchive& Ar)
{
	Ar << MV;
	Ar << MW;
	Ar << bHasValidData;

	return true;
}

bool FChaosVDParticleDynamics::Serialize(FArchive& Ar)
{
	Ar << MAcceleration;
	Ar << MAngularAcceleration;
	Ar << MAngularImpulseVelocity;
	Ar << MLinearImpulseVelocity;
	Ar << bHasValidData;

	return true;
}

bool FChaosVDParticleMassProps::Serialize(FArchive& Ar)
{
	Ar << MCenterOfMass;
	Ar << MRotationOfMass;
	Ar << MI;
	Ar << MInvI;
	Ar << MM;
	Ar << MInvM;
	Ar << bHasValidData;

	return true;
}

bool FChaosVDParticleDynamicMisc::Serialize(FArchive& Ar)
{
	Ar << MAngularEtherDrag;
	Ar << MMaxLinearSpeedSq;
	Ar << MMaxAngularSpeedSq;
	Ar << MCollisionGroup;
	Ar << MObjectState;
	Ar << MSleepType;
	Ar << bDisabled;
	Ar << bHasValidData;

	MControlFlags.Serialize(Ar);

	return true;
}

bool FChaosVDParticleDataWrapper::Serialize(FArchive& Ar)
{
	Ar << Type;
	Ar << GeometryHash;

	bHasDebugName = DebugNamePtr.IsValid();
	Ar << bHasDebugName;

	if (bHasDebugName)
	{
		if (Ar.IsLoading())
		{
			Ar << DebugName;
		}
		else
		{
			FString& DebugNameRef = *DebugNamePtr.Get();
			Ar << DebugNameRef;
		}
	}

	Ar << ParticleIndex;
	Ar << SolverID;

	Ar << ParticlePositionRotation;
	Ar << ParticleVelocities;
	Ar << ParticleDynamics;
	Ar << ParticleDynamicsMisc;
	Ar << ParticleMassProps;

	Ar << bHasValidData;

	return true;
}
