// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Containers/Map.h"
#include "Misc/Variant.h"


class IElectraCodecFactory;


/**
 * Interface for the `ElectraCodecFactory` module.
 */
class IElectraCodecFactoryModule : public IModuleInterface
{
public:
	virtual ~IElectraCodecFactoryModule() = default;

	static FName GetModularFeatureName()
	{
		static FName FeatureName = FName(TEXT("ElectraCodecFactory"));
		return FeatureName;
	}

	virtual TSharedPtr<IElectraCodecFactory, ESPMode::ThreadSafe> GetBestFactoryForFormat(const FString& InCodecFormat, bool bInEncoder, const TMap<FString, FVariant>& InOptions) = 0;
};
