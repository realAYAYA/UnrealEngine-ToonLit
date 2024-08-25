// Copyright Epic Games, Inc. All Rights Reserved.

#include "Views/SoundSubmixAssetDashboardEntry.h"

namespace UE::Audio::Insights
{
	FText FSoundSubmixAssetDashboardEntry::GetDisplayName() const
	{
		return FText::FromString(FSoftObjectPath(Name).GetAssetName());
	}

	const UObject* FSoundSubmixAssetDashboardEntry::GetObject() const
	{
		return FSoftObjectPath(Name).ResolveObject();
	}

	UObject* FSoundSubmixAssetDashboardEntry::GetObject()
	{
		return FSoftObjectPath(Name).ResolveObject();
	}

	bool FSoundSubmixAssetDashboardEntry::IsValid() const
	{
		return true;
	}
} // namespace UE::Audio::Insights
