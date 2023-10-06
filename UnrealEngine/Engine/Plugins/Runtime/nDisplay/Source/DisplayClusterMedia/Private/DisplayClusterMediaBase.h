// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FRHITexture;


/**
 * Base media class
 */
class FDisplayClusterMediaBase
{
public:
	FDisplayClusterMediaBase(const FString& InMediaId, const FString& InClusterNodeId)
		: MediaId(InMediaId)
		, ClusterNodeId(InClusterNodeId)
	{ }

	virtual ~FDisplayClusterMediaBase() = default;

public:
	const FString& GetMediaId() const
	{
		return MediaId;
	}

	const FString& GetClusterNodeId() const
	{
		return ClusterNodeId;
	}

protected:
	struct FMediaTextureInfo
	{
		FRHITexture* Texture = nullptr;
		FIntRect     Region;
	};

private:
	const FString MediaId;
	const FString ClusterNodeId;
};
