// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Views/TableDashboardViewFactory.h"

class UAudioBus;

namespace UE::Audio::Insights
{
	struct AUDIOINSIGHTS_API FAudioBusAssetDashboardEntry : public IObjectDashboardEntry
	{
		virtual ~FAudioBusAssetDashboardEntry() = default;

		virtual FText GetDisplayName() const override;
		virtual const UObject* GetObject() const override;
		virtual UObject* GetObject() override;
		virtual bool IsValid() const override;

		::Audio::FDeviceId DeviceId = INDEX_NONE;
		uint64 ComponentId = TNumericLimits<uint64>::Max();
		FString Name;
		TWeakObjectPtr<UAudioBus> AudioBus;
	};
} // namespace UE::Audio::Insights
