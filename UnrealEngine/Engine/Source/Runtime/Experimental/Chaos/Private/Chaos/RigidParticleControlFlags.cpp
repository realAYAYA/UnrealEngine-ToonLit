// Copyright Epic Games, Inc. All Rights Reserved.
#include "Chaos/RigidParticleControlFlags.h"

#include "Chaos/ChaosArchive.h"

#include "Containers/UnrealString.h"

namespace Chaos
{
	FString FRigidParticleControlFlags::ToString() const
	{
		return FString::Printf(TEXT("0x%x"), Bits);
	}

	FChaosArchive& operator<<(FChaosArchive& Ar, FRigidParticleControlFlags& Flags)
	{
		Ar << Flags.Bits;
		return Ar;
	}
}