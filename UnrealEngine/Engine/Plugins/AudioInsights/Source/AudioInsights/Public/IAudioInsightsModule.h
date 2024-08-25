// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Modules/ModuleInterface.h"
#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"

namespace UE::Audio::Insights
{
	class IDashboardViewFactory;
} // namespace UE::Audio::Insights

class AUDIOINSIGHTS_API IAudioInsightsModule : public IModuleInterface
{
public:
	virtual void RegisterDashboardViewFactory(TSharedRef<UE::Audio::Insights::IDashboardViewFactory> InDashboardFactory) = 0;
	virtual void UnregisterDashboardViewFactory(FName InName) = 0;

	virtual ::Audio::FDeviceId GetDeviceId() const = 0;

	static FName GetName()
	{
		const FLazyName ModuleName = "AudioInsights";
		return ModuleName.Resolve();
	}
};
