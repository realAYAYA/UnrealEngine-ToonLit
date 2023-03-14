// Copyright Epic Games, Inc. All Rights Reserved.

#include "OculusResourceHolder.h"
#include "HeadMountedDisplayTypes.h" // for LogHMD
#include "UObject/ConstructorHelpers.h"
#include "Materials/Material.h"

//////////////////////////////////////////////////////////////////////////
// UOculusResourceManager

UOculusResourceHolder::UOculusResourceHolder(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	static ConstructorHelpers::FObjectFinder<UMaterial> StaticPokeAHoleMaterial(TEXT("/OculusVR/Materials/PokeAHoleMaterial"));

	PokeAHoleMaterial = StaticPokeAHoleMaterial.Object;

	if (!PokeAHoleMaterial)
	{
		UE_LOG(LogHMD, Error, TEXT("Unable to load PokeAHoleMaterial"));
	}
	else
	{
		UE_LOG(LogHMD, Log, TEXT("PokeAHoleMaterial loaded successfully"));
	}
}