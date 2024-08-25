// Copyright Epic Games, Inc. All Rights Reserved.

#include "VCamSubsystem.h"

#include "VCamComponent.h"

UVCamComponent* UVCamSubsystem::GetVCamComponent() const
{
	return GetTypedOuter<UVCamComponent>();
}
