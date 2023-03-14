// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreTypes.h"

#if PLATFORM_ANDROID

#include <ElectraPlayerPlugin.h>


class FElectraPlayerResourceDelegate : public IElectraPlayerResourceDelegate
{
public:
	FElectraPlayerResourceDelegate(FElectraPlayerPlugin* InOwner) : Owner(InOwner->AsWeak()) {}

	virtual jobject GetCodecSurface() override
	{
		if (auto PinnedOwner = Owner.Pin())
		{
			return (jobject)PinnedOwner->OutputTexturePool->GetCodecSurface();
		}
		return (jobject)nullptr;
	}

private:
	TWeakPtr<FElectraPlayerPlugin, ESPMode::ThreadSafe> Owner;
};


IElectraPlayerResourceDelegate* FElectraPlayerPlugin::PlatformCreatePlayerResourceDelegate()
{
	return new FElectraPlayerResourceDelegate(this);
}

void FElectraPlayerPlugin::PlatformSetupResourceParams(Electra::FParamDict& Params)
{
}

#endif // PLATFORM_ANDROID
