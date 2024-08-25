// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraDataChannelCommon.h"
#include "Components/SceneComponent.h"

FVector FNiagaraDataChannelSearchParameters::GetLocation()const
{
	return OwningComponent && !bOverrideLocation ? OwningComponent->GetComponentLocation() : Location;
}