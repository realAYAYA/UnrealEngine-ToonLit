// Copyright Epic Games, Inc. All Rights Reserved.

#include "Views/AudioBusAssetDashboardEntry.h"

namespace UE::Audio::Insights
{
	FText FAudioBusAssetDashboardEntry::GetDisplayName() const
	{
		return FText::FromString(FSoftObjectPath(Name).GetAssetName());
	}

	const UObject* FAudioBusAssetDashboardEntry::GetObject() const
	{
		return FSoftObjectPath(Name).ResolveObject();
	}

	UObject* FAudioBusAssetDashboardEntry::GetObject()
	{
		return FSoftObjectPath(Name).ResolveObject();
	}

	bool FAudioBusAssetDashboardEntry::IsValid() const
	{
		return true;
	}
} // namespace UE::Audio::Insights
