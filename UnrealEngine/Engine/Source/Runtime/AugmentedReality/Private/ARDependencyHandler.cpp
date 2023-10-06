// Copyright Epic Games, Inc. All Rights Reserved.

#include "ARDependencyHandler.h"
#include "Features/IModularFeatures.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ARDependencyHandler)


UARDependencyHandler* UARDependencyHandler::GetARDependencyHandler()
{
	auto& ModularFeatures = IModularFeatures::Get();
	const auto FeatureName = GetModularFeatureName();
	if (ModularFeatures.GetModularFeatureImplementationCount(FeatureName))
	{
		return static_cast<UARDependencyHandler*>(ModularFeatures.GetModularFeatureImplementation(FeatureName, 0));
	}
	return nullptr;
}

