// Copyright Epic Games, Inc. All Rights Reserved.

#include "ContentBrowserAliasDataSourceModule.h"

void FContentBrowserAliasDataSourceModule::StartupModule()
{
	UObject::GetMetaDataTagsForAssetRegistry().Add(UContentBrowserAliasDataSource::AliasTagName);

	AliasDataSource.Reset(NewObject<UContentBrowserAliasDataSource>(GetTransientPackage(), "AliasData"));
	AliasDataSource->Initialize();
}

void FContentBrowserAliasDataSourceModule::ShutdownModule()
{
	AliasDataSource.Reset();
}

IMPLEMENT_MODULE(FContentBrowserAliasDataSourceModule, ContentBrowserAliasDataSource);
