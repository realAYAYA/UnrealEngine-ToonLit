// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Features/IModularFeature.h"
#include "Features/IModularFeatures.h"
#include "ARSessionConfig.h"
#include "Serialization/Archive.h"
#include "Containers/UnrealString.h"

class AUGMENTEDREALITY_API IARSessionConfigCookSupport : public IModularFeature
{
public:
	static FName GetModularFeatureName()
	{
		static FName FeatureName = FName(TEXT("ARSessionConfigCookingSupport"));
		return FeatureName;
	}

	virtual void OnSerializeSessionConfig(UARSessionConfig* SessionConfig, FArchive& Ar, TArray<uint8>& SerializedARCandidateImageDatabase) = 0;
};
