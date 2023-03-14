// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/Layout/SBox.h"

struct FAssetData;

class SContextualAnimAssetBrowser : public SBox
{
public:
	
	SLATE_BEGIN_ARGS(SContextualAnimAssetBrowser) {}
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs);
	virtual ~SContextualAnimAssetBrowser(){}

private:

	TSharedPtr<SBox> AssetBrowserBox;
	void OnAssetDoubleClicked(const FAssetData& AssetData);
	bool OnShouldFilterAsset(const FAssetData& AssetData);
};