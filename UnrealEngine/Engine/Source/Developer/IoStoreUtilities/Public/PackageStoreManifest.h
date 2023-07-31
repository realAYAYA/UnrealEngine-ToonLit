// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/ContainerAllocationPolicies.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "HAL/CriticalSection.h"
#include "HAL/Platform.h"
#include "HAL/PlatformCrt.h"
#include "IO/IoDispatcher.h"
#include "Templates/UniquePtr.h"
#include "UObject/NameTypes.h"
#include "ZenServerInterface.h"

class FPackageStoreManifest
{
public:
	struct FFileInfo
	{
		FString FileName;
		FIoChunkId ChunkId;
	};

	struct FPackageInfo
	{
		FName PackageName;
		TArray<FIoChunkId, TInlineAllocator<1>> ExportBundleChunkIds;
		TArray<FIoChunkId> BulkDataChunkIds;
	};
	
	struct FZenServerInfo
	{
		UE::Zen::FServiceSettings Settings;
		FString ProjectId;
		FString OplogId;
	};

	IOSTOREUTILITIES_API FPackageStoreManifest(const FString& CookedOutputPath);
	IOSTOREUTILITIES_API ~FPackageStoreManifest() = default;

	IOSTOREUTILITIES_API void BeginPackage(FName PackageName);
	IOSTOREUTILITIES_API void AddPackageData(FName PackageName, const FString& FileName, const FIoChunkId& ChunkId);
	IOSTOREUTILITIES_API void AddBulkData(FName PackageName, const FString& FileName, const FIoChunkId& ChunkId);

	IOSTOREUTILITIES_API FIoStatus Save(const TCHAR* Filename) const;
	IOSTOREUTILITIES_API FIoStatus Load(const TCHAR* Filename);

	IOSTOREUTILITIES_API TArray<FFileInfo> GetFiles() const;
	IOSTOREUTILITIES_API TArray<FPackageInfo> GetPackages() const;

	IOSTOREUTILITIES_API FZenServerInfo& EditZenServerInfo();
	IOSTOREUTILITIES_API const FZenServerInfo* ReadZenServerInfo() const;

private:
	FPackageInfo* GetPackageInfo_NoLock(FName PackageName);

	mutable FCriticalSection CriticalSection;
	FString CookedOutputPath;
	TMap<FName, FPackageInfo> PackageInfoByNameMap;

	TMap<FIoChunkId, FString> FileNameByChunkIdMap;
	TUniquePtr<FZenServerInfo> ZenServerInfo;
};
