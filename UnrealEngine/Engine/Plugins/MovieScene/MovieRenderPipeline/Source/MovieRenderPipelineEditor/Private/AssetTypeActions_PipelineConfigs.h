// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/IToolkitHost.h"
#include "AssetTypeActions_Base.h"
#include "MoviePipelineMasterConfig.h"
#include "MoviePipelineShotConfig.h"
#include "MoviePipelineQueue.h"

class FAssetTypeActions_PipelineMasterConfig : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_PipelineMasterConfig", "Movie Pipeline Master Config"); }
	virtual FColor GetTypeColor() const override { return FColor(78, 40, 165); }
	virtual UClass* GetSupportedClass() const override { return UMoviePipelineMasterConfig::StaticClass(); }
	virtual void OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>() ) override;
	virtual bool CanFilter() override { return false; }
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Animation; }
};

class FAssetTypeActions_PipelineShotConfig : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_PipelineShotConfig", "Movie Pipeline Shot Config"); }
	virtual FColor GetTypeColor() const override { return FColor(78, 40, 165); }
	virtual UClass* GetSupportedClass() const override { return UMoviePipelineShotConfig::StaticClass(); }
	virtual void OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>() ) override;
	virtual bool CanFilter() override { return false; }
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Animation; }
};


class FAssetTypeActions_PipelineQueue : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_PipelineQueue", "Movie Pipeline Queue"); }
	virtual FColor GetTypeColor() const override { return FColor(78, 40, 165); }
	virtual UClass* GetSupportedClass() const override { return UMoviePipelineQueue::StaticClass(); }
	virtual void OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>() ) override;
	virtual bool CanFilter() override { return false; }
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Animation; }
};