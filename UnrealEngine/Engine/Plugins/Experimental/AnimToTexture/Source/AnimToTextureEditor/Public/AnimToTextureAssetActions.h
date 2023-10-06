// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetTypeActions_Base.h"
#include "AnimToTextureDataAsset.h"

class FMenuBuilder;
class IToolkitHost;
class UClass;
class UObject;


class FAnimToTextureAssetActions : public FAssetTypeActions_Base
{
public:
	
	// IAssetTypeActions Implementation
	FText GetName() const override;
	FColor GetTypeColor() const override;
	UClass* GetSupportedClass() const override;	
	void GetActions( const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder ) override;
	uint32 GetCategories() override;
	const TArray<FText>& GetSubMenus() const override;
	TSharedPtr<SWidget> GetThumbnailOverlay(const FAssetData& AssetData) const override;

private:
	void RunAnimToTexture(TArray<TWeakObjectPtr<UAnimToTextureDataAsset>> Objects);
	
};
