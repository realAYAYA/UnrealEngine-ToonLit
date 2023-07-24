// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DisplayClusterConfigurationTypes_Media.h"


bool FDisplayClusterConfigurationMediaICVFX::IsMediaInputAssigned(const FString& InNodeId) const
{
	return GetMediaSource(InNodeId) != nullptr;
}

bool FDisplayClusterConfigurationMediaICVFX::IsMediaOutputAssigned(const FString& InNodeId) const
{
	return GetMediaOutput(InNodeId) != nullptr;
}

UMediaSource* FDisplayClusterConfigurationMediaICVFX::GetMediaSource(const FString& InNodeId) const
{
	// Look up for a group that contains node ID specified
	for (const FDisplayClusterConfigurationMediaInputGroup& MediaInputGroup : MediaInputGroups)
	{
		const bool bFound = MediaInputGroup.ClusterNodes.ItemNames.ContainsByPredicate([InNodeId](const FString& Item)
			{
				return Item.Equals(InNodeId, ESearchCase::IgnoreCase);
			});

		if (bFound)
		{
			return MediaInputGroup.MediaSource;
		}
	}

	return nullptr;
}

UMediaOutput* FDisplayClusterConfigurationMediaICVFX::GetMediaOutput(const FString& InNodeId) const
{
	// Look up for a group that contains node ID specified
	for (const FDisplayClusterConfigurationMediaOutputGroup& MediaOutputGroup : MediaOutputGroups)
	{
		const bool bFound = MediaOutputGroup.ClusterNodes.ItemNames.ContainsByPredicate([InNodeId](const FString& Item)
			{
				return Item.Equals(InNodeId, ESearchCase::IgnoreCase);
			});

		if (bFound)
		{
			return MediaOutputGroup.MediaOutput;
		}
	}

	return nullptr;
}
