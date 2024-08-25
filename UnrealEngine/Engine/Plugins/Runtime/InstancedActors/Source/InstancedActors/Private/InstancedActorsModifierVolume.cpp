// Copyright Epic Games, Inc. All Rights Reserved.

#include "InstancedActorsModifierVolume.h"
#include "InstancedActorsModifierVolumeComponent.h"


//-----------------------------------------------------------------------------
// AInstancedActorsModifierVolume
//-----------------------------------------------------------------------------
AInstancedActorsModifierVolume::AInstancedActorsModifierVolume(const FObjectInitializer& ObjectInitializer)
{
	ModifierVolumeComponent = CreateDefaultSubobject<UInstancedActorsModifierVolumeComponent>(TEXT("ModifierVolume"));
	ModifierVolumeComponent->Extent = FVector(50.0f);
	ModifierVolumeComponent->Radius = 50.0f;
	RootComponent = ModifierVolumeComponent;
}

//-----------------------------------------------------------------------------
// AInstancedActorsRemovalModifierVolume
//-----------------------------------------------------------------------------
AInstancedActorsRemovalModifierVolume::AInstancedActorsRemovalModifierVolume(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<URemoveInstancesModifierVolumeComponent>(TEXT("ModifierVolume")))
{
	ModifierVolumeComponent->Color = FColor::Red;
}
