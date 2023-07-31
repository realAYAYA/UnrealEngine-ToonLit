// Copyright Epic Games, Inc. All Rights Reserved.

#include "ContentBrowserDataLegacyBridge.h"

namespace ContentBrowserDataLegacyBridge
{

FOnCreateNewAsset& OnCreateNewAsset()
{
	static FOnCreateNewAsset OnCreateNewAssetDelegate;
	return OnCreateNewAssetDelegate;
}

}
