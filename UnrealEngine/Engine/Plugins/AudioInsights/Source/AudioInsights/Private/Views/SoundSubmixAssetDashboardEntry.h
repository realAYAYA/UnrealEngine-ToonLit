// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Views/TableDashboardViewFactory.h"

class USoundSubmix;

namespace UE::Audio::Insights
{
	struct AUDIOINSIGHTS_API FSoundSubmixAssetDashboardEntry : public IObjectDashboardEntry
	{
		virtual ~FSoundSubmixAssetDashboardEntry() = default;

		virtual FText GetDisplayName() const override;
		virtual const UObject* GetObject() const override;
		virtual UObject* GetObject() override;
		virtual bool IsValid() const override;

		::Audio::FDeviceId DeviceId = INDEX_NONE;
		uint64 ComponentId = TNumericLimits<uint64>::Max();
		FString Name;
		TWeakObjectPtr<USoundSubmix> SoundSubmix;
	};
} // namespace UE::Audio::Insights
