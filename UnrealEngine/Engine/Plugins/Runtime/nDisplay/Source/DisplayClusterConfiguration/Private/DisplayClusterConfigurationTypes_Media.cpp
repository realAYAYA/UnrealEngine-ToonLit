// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DisplayClusterConfigurationTypes_Media.h"

///////////////////////////////////////////////////
// FDisplayClusterConfigurationMedia

bool FDisplayClusterConfigurationMedia::IsMediaInputAssigned() const
{
	return IsValid(MediaInput.MediaSource);
}

bool FDisplayClusterConfigurationMedia::IsMediaOutputAssigned() const
{
	// Return true if we have at least one media output set
	for (const FDisplayClusterConfigurationMediaOutput& MediaOutputItem : MediaOutputs)
	{
		if (IsValid(MediaOutputItem.MediaOutput))
		{
			return true;
		}
	}

	return false;
}


///////////////////////////////////////////////////
// FDisplayClusterConfigurationMediaICVFX

bool FDisplayClusterConfigurationMediaICVFX::IsMediaInputAssigned(const FString& NodeId) const
{
	return IsValid(GetMediaSource(NodeId));
}

bool FDisplayClusterConfigurationMediaICVFX::IsMediaOutputAssigned(const FString& NodeId) const
{
	const TArray<FDisplayClusterConfigurationMediaOutputGroup> NodeOutputGroups = GetMediaOutputGroups(NodeId);

	for (const FDisplayClusterConfigurationMediaOutputGroup& NodeOutputGroup : NodeOutputGroups)
	{
		if (IsValid(NodeOutputGroup.MediaOutput))
		{
			return true;
		}
	}

	return false;
}


//@note
// The way how media is configured for ICVFX cameras technically allows to have multiple inputs assigned
// to the same camera. Yes, this is something that contradicts to a single input concept. However, it provides
// a very user-friendly GUI. So to follow the single input concept, we always return the first media input found.
UMediaSource* FDisplayClusterConfigurationMediaICVFX::GetMediaSource(const FString& NodeId) const
{
	// Look up for a group that contains node ID specified
	for (const FDisplayClusterConfigurationMediaInputGroup& MediaInputGroup : MediaInputGroups)
	{
		const bool bNodeFound = MediaInputGroup.ClusterNodes.ItemNames.ContainsByPredicate([NodeId](const FString& Item)
			{
				return Item.Equals(NodeId, ESearchCase::IgnoreCase);
			});

		if (bNodeFound && IsValid(MediaInputGroup.MediaSource))
		{
			return MediaInputGroup.MediaSource;
		}
	}

	return nullptr;
}

TArray<FDisplayClusterConfigurationMediaOutputGroup> FDisplayClusterConfigurationMediaICVFX::GetMediaOutputGroups(const FString& NodeId) const
{
	return MediaOutputGroups.FilterByPredicate([NodeId](const FDisplayClusterConfigurationMediaOutputGroup& Item)
		{
			return Item.ClusterNodes.ItemNames.Contains(NodeId);
		});
}
