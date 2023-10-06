// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/MeshDeformerProvider.h"
#include "Features/IModularFeatures.h"

const FName IMeshDeformerProvider::ModularFeatureName(TEXT("MeshDeformer"));

IMeshDeformerProvider* IMeshDeformerProvider::Get()
{
	if (IsAvailable())
	{
		return &IModularFeatures::Get().GetModularFeature<IMeshDeformerProvider>(ModularFeatureName);
	}
	return nullptr;
}

bool IMeshDeformerProvider::IsAvailable()
{
	return IModularFeatures::Get().IsModularFeatureAvailable(ModularFeatureName);
}
