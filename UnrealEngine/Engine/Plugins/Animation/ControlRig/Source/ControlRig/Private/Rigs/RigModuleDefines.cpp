// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rigs/RigModuleDefines.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigModuleDefines)

bool FRigModuleConnector::operator==(const FRigModuleConnector& Other) const
{
	return Name == Other.Name && GetTypeHash(Settings) == GetTypeHash(Other.Settings);
}
