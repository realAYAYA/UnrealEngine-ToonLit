// Copyright Epic Games, Inc. All Rights Reserved.

#include "DerivedDataCacheModule.h"

#include "CoreGlobals.h"
#include "DerivedDataBuild.h"
#include "DerivedDataBuildPrivate.h"
#include "DerivedDataCache.h"
#include "DerivedDataCachePrivate.h"
#include "DerivedDataPrivate.h"
#include "HAL/CriticalSection.h"
#include "Misc/ScopeLock.h"
#include "Modules/ModuleManager.h"

namespace UE::DerivedData::Private
{

static FDerivedDataCacheInterface* GDerivedDataLegacyCache;
static ICache* GDerivedDataCache;
static IBuild* GDerivedDataBuild;

PRAGMA_DISABLE_DEPRECATION_WARNINGS

class FDerivedDataCacheModule final : public IDerivedDataCacheModule
{
public:
	FDerivedDataCacheInterface& GetDDC() final
	{
		return **CreateOrGetCache();
	}

	FDerivedDataCacheInterface* const* CreateOrGetCache() final
	{
		CreateCacheOnce();
		check(GDerivedDataLegacyCache);
		return &GDerivedDataLegacyCache;
	}

	FDerivedDataCacheInterface* const* GetCache() final
	{
		return &GDerivedDataLegacyCache;
	}

	void CreateCacheOnce()
	{
		FScopeLock Lock(&CreateLock);
		if (!GDerivedDataCache)
		{
			GDerivedDataCache = CreateCache(&GDerivedDataLegacyCache);
			check(GDerivedDataCache);
		}
	}

	void CreateBuildOnce()
	{
		CreateCacheOnce();
		FScopeLock Lock(&CreateLock);
		if (!GDerivedDataBuild)
		{
			GDerivedDataBuild = CreateBuild(*GDerivedDataCache);
			check(GDerivedDataBuild);
		}
	}

	void StartupModule() final
	{
		// Required to guarantee that SSL shuts down after DDC. Without this, waiting for active
		// cache requests on shutdown can crash when accessing SSL.
		FModuleManager::Get().LoadModuleChecked(TEXT("SSL"));
	}

	void ShutdownModule() final
	{
		delete GDerivedDataBuild;
		GDerivedDataBuild = nullptr;
		delete GDerivedDataCache;
		GDerivedDataCache = nullptr;
		GDerivedDataLegacyCache = nullptr;
	}

private:
	FCriticalSection CreateLock;
};

PRAGMA_ENABLE_DEPRECATION_WARNINGS

static FDerivedDataCacheModule* GetModule()
{
	if (!FPlatformProperties::RequiresCookedData())
	{
		if (IDerivedDataCacheModule* Module = FModuleManager::LoadModulePtr<IDerivedDataCacheModule>("DerivedDataCache"))
		{
			return static_cast<FDerivedDataCacheModule*>(Module);
		}
	}
	return nullptr;
}

} // UE::DerivedData::Private

namespace UE::DerivedData
{

LLM_DEFINE_TAG(DerivedData);
LLM_DEFINE_TAG(DerivedDataBuild, "Build", "DerivedData");
LLM_DEFINE_TAG(DerivedDataCache, "Cache", "DerivedData");

ICache& GetCache()
{
	if (ICache* Cache = Private::GDerivedDataCache)
	{
		return *Cache;
	}
	checkf(IsInGameThread(), TEXT("The derived data cache must be created on the main thread."));
	if (Private::FDerivedDataCacheModule* Module = Private::GetModule())
	{
		Module->CreateCacheOnce();
	}
	ICache* Cache = Private::GDerivedDataCache;
	checkf(Cache, TEXT("Failed to create derived data cache."));
	return *Cache;
}

ICache* TryGetCache()
{
	return Private::GDerivedDataCache;
}

IBuild& GetBuild()
{
	if (IBuild* Build = Private::GDerivedDataBuild)
	{
		return *Build;
	}
	checkf(IsInGameThread(), TEXT("The derived data build system must be created on the main thread."));
	if (Private::FDerivedDataCacheModule* Module = Private::GetModule())
	{
		Module->CreateBuildOnce();
	}
	IBuild* Build = Private::GDerivedDataBuild;
	checkf(Build, TEXT("Failed to create derived data build system."));
	return *Build;
}

} // UE::DerivedData

IMPLEMENT_MODULE(UE::DerivedData::Private::FDerivedDataCacheModule, DerivedDataCache);
