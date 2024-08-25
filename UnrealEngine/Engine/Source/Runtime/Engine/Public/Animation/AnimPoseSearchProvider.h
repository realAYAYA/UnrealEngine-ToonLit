// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "CoreMinimal.h"
#include "Features/IModularFeature.h"

struct FAnimationBaseContext;
class UAnimationAsset;

namespace UE::Anim
{

/** Modular feature interface for PoseSearch */
class ENGINE_API IPoseSearchProvider : public IModularFeature
{
public:
	virtual ~IPoseSearchProvider() {}

	static FName GetModularFeatureName();
	static bool IsAvailable();
	static IPoseSearchProvider* Get();
	
	struct FSearchResult
	{
		UObject* SelectedAsset = nullptr;
		float TimeOffsetSeconds = 0.f;
		float Dissimilarity = MAX_flt;
		bool bIsFromContinuingPlaying = false;
	};

	/**
	* Finds a matching pose in the input Object given the current graph context
	* 
	* @param	GraphContext				Graph execution context used to construct a pose search query
	* @param	AssetsToSearch				The assets to search for the pose query
	* @param	PlayingAsset				The currently playing asset, used to bias the score of the eventually found continuing pose
	* @param	PlayingAssetAccumulatedTime	The accumulated time of the currently playing asset
	* 
	* @return	FSearchResult				The search result identifying the asset from AssetsToSearch or PlayingAsset that most closely matches the query
	*/
	virtual FSearchResult Search(const FAnimationBaseContext& GraphContext, TArrayView<const UObject*> AssetsToSearch,
		const UObject* PlayingAsset = nullptr, float PlayingAssetAccumulatedTime = 0.f) const = 0;
};

} // namespace UE::Anim