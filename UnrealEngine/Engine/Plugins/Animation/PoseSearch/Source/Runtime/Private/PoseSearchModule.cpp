// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimPoseSearchProvider.h"
#include "Features/IModularFeatures.h"
#include "Modules/ModuleManager.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchLibrary.h"

class FPoseSearchModule final : public IModuleInterface, public UE::Anim::IPoseSearchProvider
{
public:
	
	// IModuleInterface
	virtual void StartupModule() override
	{
		IModularFeatures::Get().RegisterModularFeature(UE::Anim::IPoseSearchProvider::GetModularFeatureName(), this);
	}

	virtual void ShutdownModule() override
	{
		IModularFeatures::Get().UnregisterModularFeature(UE::Anim::IPoseSearchProvider::GetModularFeatureName(), this);
	}

	// IPoseSearchProvider
	virtual UE::Anim::IPoseSearchProvider::FSearchResult Search(const FAnimationBaseContext& GraphContext, TArrayView<const UObject*> AssetsToSearch, const UObject* PlayingAsset, float PlayingAssetAccumulatedTime) const override
	{
		const UE::PoseSearch::FSearchResult SearchResult = UPoseSearchLibrary::MotionMatch(GraphContext, AssetsToSearch, PlayingAsset, PlayingAssetAccumulatedTime);
		UE::Anim::IPoseSearchProvider::FSearchResult ProviderResult;
		if (const UE::PoseSearch::FSearchIndexAsset* SearchIndexAsset = SearchResult.GetSearchIndexAsset())
		{
			if (const FPoseSearchDatabaseAnimationAssetBase* DatabaseAnimationAssetBase = SearchResult.Database->GetAnimationAssetBase(*SearchIndexAsset))
			{
				ProviderResult.SelectedAsset = DatabaseAnimationAssetBase->GetAnimationAsset();
				ProviderResult.Dissimilarity = SearchResult.PoseCost.GetTotalCost();
				ProviderResult.TimeOffsetSeconds = SearchResult.AssetTime;
				ProviderResult.bIsFromContinuingPlaying = SearchResult.bIsContinuingPoseSearch;
			}
		}

		return ProviderResult;
	}
};

IMPLEMENT_MODULE(FPoseSearchModule, PoseSearch);