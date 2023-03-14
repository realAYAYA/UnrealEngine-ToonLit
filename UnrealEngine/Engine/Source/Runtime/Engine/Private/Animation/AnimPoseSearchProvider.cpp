// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimPoseSearchProvider.h"
#include "Features/IModularFeatures.h"


namespace UE { namespace Anim {

const FName IPoseSearchProvider::ModularFeatureName(TEXT("AnimPoseSearch"));

IPoseSearchProvider* IPoseSearchProvider::Get()
{
	IModularFeatures::FScopedLockModularFeatureList ScopedLockModularFeatureList;

	if (IModularFeatures::Get().IsModularFeatureAvailable(ModularFeatureName))
	{
		return &IModularFeatures::Get().GetModularFeature<IPoseSearchProvider>(ModularFeatureName);
	}

	return nullptr;
}

bool IPoseSearchProvider::IsAvailable()
{
	IModularFeatures::FScopedLockModularFeatureList ScopedLockModularFeatureList;
	return IModularFeatures::Get().IsModularFeatureAvailable(ModularFeatureName);
}

}} // namespace UE::Anim