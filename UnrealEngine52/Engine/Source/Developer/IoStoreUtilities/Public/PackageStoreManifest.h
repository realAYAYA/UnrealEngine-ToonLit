// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/ContainerAllocationPolicies.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "Experimental/ZenServerInterface.h"
#include "HAL/CriticalSection.h"
#include "HAL/Platform.h"
#include "HAL/PlatformCrt.h"
#include "IO/IoDispatcher.h"
#include "Templates/UniquePtr.h"
#include "UObject/NameTypes.h"

class FCbFieldView;
class FCbWriter;

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

	IOSTOREUTILITIES_API void SetTrackPackageData(bool bInTrackpackageData);
	IOSTOREUTILITIES_API void WritePackage(FCbWriter& Writer, FName PackageName);
	IOSTOREUTILITIES_API bool TryReadPackage(FCbFieldView Field, FName PackageName);


private:
	FPackageInfo* GetPackageInfo_NoLock(FName PackageName);

	mutable FCriticalSection CriticalSection;
	FString CookedOutputPath;
	TMap<FName, FPackageInfo> PackageInfoByNameMap;

	TMap<FIoChunkId, FString> FileNameByChunkIdMap;
	TUniquePtr<FZenServerInfo> ZenServerInfo;

	/**
	 * Transient data used during MPCook to find all Filenames used by a package. Used to replicate all data related to
	 * the package to the CookDirector.
	 */
	TMap<FName, TArray<TPair<FString, FIoChunkId>>> PackageFileChunkIds;
	bool bTrackPackageData = false;
};
