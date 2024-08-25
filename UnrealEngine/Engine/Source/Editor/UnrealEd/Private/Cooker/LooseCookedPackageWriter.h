// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/Map.h"
#include "Containers/StringFwd.h"
#include "Containers/UnrealString.h"
#include "HAL/CriticalSection.h"
#include "HAL/Platform.h"
#include "IO/IoDispatcher.h"
#include "Memory/CompositeBuffer.h"
#include "Memory/SharedBuffer.h"
#include "Misc/DateTime.h"
#include "Misc/PackagePath.h"
#include "Serialization/CompactBinary.h"
#include "Serialization/FileRegions.h"
#include "Serialization/PackageWriter.h"
#include "Serialization/PackageWriterToSharedBuffer.h"
#include "Templates/SharedPointer.h"
#include "Templates/UniquePtr.h"
#include "UObject/NameTypes.h"

class FAsyncIODelete;
class FAssetRegistryState;
class FLargeMemoryWriter;
class FMD5;
class ITargetPlatform;
template <typename ReferencedType> class TRefCountPtr;
namespace UE::Cook { class FCookSandbox; }
namespace UE::Cook { struct FCookSandboxConvertCookedPathToPackageNameContext; }

/** A CookedPackageWriter that saves cooked packages in separate .uasset,.uexp,.ubulk files in the Saved\Cooked\[Platform] directory. */
class FLooseCookedPackageWriter : public TPackageWriterToSharedBuffer<ICookedPackageWriter>
{
public:
	using Super = TPackageWriterToSharedBuffer<ICookedPackageWriter>;

	FLooseCookedPackageWriter(const FString& OutputPath, const FString& MetadataDirectoryPath,
		const ITargetPlatform* TargetPlatform, FAsyncIODelete& InAsyncIODelete,
		UE::Cook::FCookSandbox& InSandboxFile, FBeginCacheCallback&& InBeginCacheCallback);
	~FLooseCookedPackageWriter();

	virtual FCookCapabilities GetCookCapabilities() const override
	{
		FCookCapabilities Result;
		Result.bDiffModeSupported = true;
		return Result;
	}

	virtual void BeginPackage(const FBeginPackageInfo& Info) override;
	virtual int64 GetExportsFooterSize() override;

	virtual FDateTime GetPreviousCookTime() const override;
	virtual void Initialize(const FCookInfo& Info) override;
	virtual void BeginCook(const FCookInfo& Info) override;
	virtual void EndCook(const FCookInfo& Info) override;
	virtual TUniquePtr<FAssetRegistryState> LoadPreviousAssetRegistry() override;
	virtual FCbObject GetOplogAttachment(FName PackageName, FUtf8StringView AttachmentKey) override;
	virtual void RemoveCookedPackages(TArrayView<const FName> PackageNamesToRemove) override;
	virtual void RemoveCookedPackages() override;
	virtual void UpdatePackageModificationStatus(FName PackageName, bool bIterativelyUnmodified,
		bool& bInOutShouldIterativelySkip) override;
	virtual TFuture<FCbObject> WriteMPCookMessageForPackage(FName PackageName) override;
	virtual bool TryReadMPCookMessageForPackage(FName PackageName, FCbObjectView Message) override;
	virtual bool GetPreviousCookedBytes(const FPackageInfo& Info, FPreviousCookedBytesData& OutData) override;
	virtual void CompleteExportsArchiveForDiff(FPackageInfo& Info, FLargeMemoryWriter& ExportsArchive) override;
	virtual EPackageWriterResult BeginCacheForCookedPlatformData(FBeginCacheForCookedPlatformDataInfo& Info) override;

	virtual void CommitPackageInternal(FPackageWriterRecords::FPackage&& BaseRecord,
		const FCommitPackageInfo& Info) override;
	virtual FPackageWriterRecords::FPackage* ConstructRecord() override;

	static EPackageExtension BulkDataTypeToExtension(FBulkDataInfo::EType BulkDataType);

private:

	/** Version of the superclass's per-package record that includes our class-specific data. */
	struct FRecord : public FPackageWriterRecords::FPackage
	{
		bool bCompletedExportsArchiveForDiff = false;
	};

	/** Buffers that are combined into the HeaderAndExports file (which is then split into .uasset + .uexp or .uoasset + .uoexp). */
	struct FExportBuffer
	{
		FSharedBuffer Buffer;
		TArray<FFileRegion> Regions;
	};

	/**
	 * The data needed to asynchronously write one of the files (.uasset, .uexp, .ubulk, any optional and any additional),
	 * without reference back to other data on this writer.
	 */
	struct FWriteFileData
	{
		FString Filename;
		FCompositeBuffer Buffer;
		TArray<FFileRegion> Regions;
		bool bIsSidecar;
		bool bContributeToHash = true;
		FIoChunkId ChunkId = FIoChunkId::InvalidChunkId;

		void HashAndWrite(FMD5& AccumulatedHash, const TRefCountPtr<FPackageHashes>& PackageHashes, EWriteOptions WriteOptions) const;
	};

	/** Stack data for the helper functions of CommitPackageInternal. */
	struct FCommitContext
	{
		const FCommitPackageInfo& Info;
		TArray<TArray<FExportBuffer>> ExportsBuffers;
		TArray<FWriteFileData> OutputFiles;
	};

	struct FOplogChunkInfo
	{
		FString RelativeFileName;
		FIoChunkId ChunkId;
	};

	struct FOplogPackageInfo
	{
		FName PackageName;
		TArray<FOplogChunkInfo, TInlineAllocator<1>> PackageDataChunks;
		TArray<FOplogChunkInfo> BulkDataChunks;
	};

	/* Delete the sandbox directory (asynchronously) in preparation for a clean cook */
	void DeleteSandboxDirectory();
	/** Searches the disk for all the cooked files in the sandbox. Stores results in PackageNameToCookedFiles. */
	void GetAllCookedFiles();
	void FindAndDeleteCookedFilesForPackages(TConstArrayView<FName> PackageNames);

	void RemoveCookedPackagesByPackageName(TArrayView<const FName> PackageNamesToRemove, bool bRemoveRecords);
	void AsyncSave(FRecord& Record, const FCommitPackageInfo& Info);

	void CollectForSavePackageData(FRecord& Record, FCommitContext& Context);
	void CollectForSaveBulkData(FRecord& Record, FCommitContext& Context);
	void CollectForSaveLinkerAdditionalDataRecords(FRecord& Record, FCommitContext& Context);
	void CollectForSaveAdditionalFileRecords(FRecord& Record, FCommitContext& Context);
	void CollectForSaveExportsFooter(FRecord& Record, FCommitContext& Context);
	void CollectForSaveExportsPackageTrailer(FRecord& Record, FCommitContext& Context);
	void CollectForSaveExportsBuffers(FRecord& Record, FCommitContext& Context);
	void AsyncSaveOutputFiles(FRecord& Record, FCommitContext& Context);
	void UpdateManifest(FRecord& Record);
	void WriteOplogEntry(FCbWriter& Writer, const FOplogPackageInfo& PackageInfo);
	bool ReadOplogEntry(FOplogPackageInfo& PackageInfo, const FCbFieldView& Field);

	TMap<FName, TRefCountPtr<FPackageHashes>>& GetPackageHashes() override
	{
		return AllPackageHashes;
	}

	// If EWriteOptions::ComputeHash is not set, the package will not get added to this.
	TMap<FName, TRefCountPtr<FPackageHashes>> AllPackageHashes;

	TMap<FName, TArray<FString>> PackageNameToCookedFiles;
	/** CommitPackage can be called in parallel if using recursive save, so we need a lock for shared containers used during CommitPackage */
	FCriticalSection PackageHashesLock;
	FCriticalSection OplogLock;
	FString OutputPath;
	FString MetadataDirectoryPath;
	const ITargetPlatform& TargetPlatform;
	TMap<FName, FOplogPackageInfo> Oplog;
	UE::Cook::FCookSandbox& SandboxFile;
	FAsyncIODelete& AsyncIODelete;
	FBeginCacheCallback BeginCacheCallback;
	bool bIterateSharedBuild = false;
	bool bProvidePerPackageResults = false;
};
