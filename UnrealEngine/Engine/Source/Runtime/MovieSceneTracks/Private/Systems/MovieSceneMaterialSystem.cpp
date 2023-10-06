// Copyright Epic Games, Inc. All Rights Reserved.

#include "Systems/MovieSceneMaterialSystem.h"

namespace UE::MovieScene
{

bool GUseSoftObjectPtrsForPreAnimatedMaterial = true;
FAutoConsoleVariableRef CVarUseSoftObjectPtrsForPreAnimatedMaterial(
	TEXT("Sequencer.UseSoftObjectPtrsForPreAnimatedMaterial"),
	GUseSoftObjectPtrsForPreAnimatedMaterial,
	TEXT("Defines whether to use soft-object-ptrs for referencing pre-animated state (default), or strong TObjectPtrs.\n"),
	ECVF_Default
);

} // namespace UE::MovieScene


UMaterialInterface* FMovieScenePreAnimatedMaterialParameters::GetMaterial() const
{
	if (UE::MovieScene::GUseSoftObjectPtrsForPreAnimatedMaterial)
	{
		UMaterialInterface* Material = SoftPreviousMaterial.Get();
		if (!Material)
		{
			Material = SoftPreviousMaterial.LoadSynchronous();
		}
		return Material;
	}
	else
	{
		return PreviousMaterial;
	}
}

void FMovieScenePreAnimatedMaterialParameters::SetMaterial(UMaterialInterface* InMaterial)
{
	if (UE::MovieScene::GUseSoftObjectPtrsForPreAnimatedMaterial)
	{
		SoftPreviousMaterial = InMaterial;
	}
	else
	{
		PreviousMaterial = InMaterial;
	}
}
