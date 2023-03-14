// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "UObject/StrongObjectPtr.h"
#include "ContentBrowserAssetDataSource.h"

class FContentBrowserAssetDataSourceModule : public FDefaultModuleImpl
{
public:
	virtual void StartupModule() override
	{
		AssetDataSource.Reset(NewObject<UContentBrowserAssetDataSource>(GetTransientPackage(), "AssetData"));
		AssetDataSource->Initialize();
	}

	virtual void ShutdownModule() override
	{
		AssetDataSource.Reset();
	}

private:
	TStrongObjectPtr<UContentBrowserAssetDataSource> AssetDataSource;
};

IMPLEMENT_MODULE(FContentBrowserAssetDataSourceModule, ContentBrowserAssetDataSource);
