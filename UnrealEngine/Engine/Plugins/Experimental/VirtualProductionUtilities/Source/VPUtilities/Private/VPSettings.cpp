// Copyright Epic Games, Inc. All Rights Reserved.

#include "VPSettings.h"

const FGameplayTagContainer& UVPSettings::GetRoles() const
{
	static const FGameplayTagContainer EmptyContainer;
	if (ensure(RolesGetter.IsBound()))
	{
		return RolesGetter.Execute();
	}
	
	return EmptyContainer;
}

UVPSettings* UVPSettings::GetVPSettings()
{
	return GetMutableDefault<UVPSettings>();
}
