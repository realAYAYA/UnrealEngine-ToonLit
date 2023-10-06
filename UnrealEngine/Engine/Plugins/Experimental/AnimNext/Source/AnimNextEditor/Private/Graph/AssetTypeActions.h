// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetTypeActions_Base.h"
#include "Graph/AnimNextGraph.h"

namespace UE::AnimNext::Editor
{

class FAssetTypeActions_AnimNextGraph : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_AnimNextGraph", "AnimNext Graph"); }
	virtual FColor GetTypeColor() const override { return FColor(128,128,64); }
	virtual UClass* GetSupportedClass() const override { return UAnimNextGraph::StaticClass(); }
	virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>()) override;
	virtual bool CanFilter() override { return true; }
	virtual const TArray<FText>& GetSubMenus() const override;
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Animation; }
};

}
