// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZenStoreWriter.h"
#include "ZenStoreHttpClient.h"
#include "ZenFileSystemManifest.h"
#include "PackageStoreOptimizer.h"
#include "Algo/BinarySearch.h"
#include "Algo/IsSorted.h"
#include "Algo/Sort.h"
#include "AssetRegistry/AssetRegistryState.h"
#include "Async/Async.h"
#include "Containers/Queue.h"
#include "Serialization/ArrayReader.h"
#include "Serialization/CompactBinaryPackage.h"
#include "Serialization/CompactBinaryWriter.h"
#include "Serialization/LargeMemoryWriter.h" 
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "IO/IoDispatcher.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/StringBuilder.h"
#include "Interfaces/ITargetPlatform.h"
#include "Misc/Paths.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "UObject/SavePackage.h"

DEFINE_LOG_CATEGORY_STATIC(LogZenStoreWriter, Log, All);

using namespace UE;

// Note that this is destructive - we yank out the buffer memory from the 
// IoBuffer into the FSharedBuffer
FSharedBuffer IoBufferToSharedBuffer(FIoBuffer& InBuffer)
{
	InBuffer.EnsureOwned();
	const uint64 DataSize = InBuffer.DataSize();
	uint8* DataPtr = InBuffer.Release().ValueOrDie();
	return FSharedBuffer{ FSharedBuffer::TakeOwnership(DataPtr, DataSize, FMemory::Free) };
};

FCbObjectId ToObjectId(const FIoChunkId& ChunkId)
{
	return FCbObjectId(MakeMemoryView(ChunkId.GetData(), ChunkId.GetSize()));
}

FMD5Hash IoHashToMD5(const FIoHash& IoHash)
{
	const FIoHash::ByteArray& Bytes = IoHash.GetBytes();
	
	FMD5 MD5Gen;
	MD5Gen.Update(Bytes, sizeof(FIoHash::ByteArray));
	
	FMD5Hash Hash;
	Hash.Set(MD5Gen);

	return Hash;
}

struct FZenStoreWriter::FZenCommitInfo
{
	IPackageWriter::FCommitPackageInfo CommitInfo;
	TUniquePtr<FPendingPackageState> PackageState;
};

class FZenStoreWriter::FCommitQueue
{
public:
	FCommitQueue()
		: NewCommitEvent(EEventMode::AutoReset)
		, QueueEmptyEvent(EEventMode::ManualReset) {}
	
	void Enqueue(FZenCommitInfo&& Commit)
	{
		{
			FScopeLock _(&QueueCriticalSection);
			Queue.Enqueue(MoveTemp(Commit));
			QueueNum++;
		}

		NewCommitEvent->Trigger();
	}

	bool BlockAndDequeue(FZenCommitInfo& OutCommit)
	{
		for (;;)
		{
			{
				FScopeLock _(&QueueCriticalSection);
				if (Queue.Dequeue(OutCommit))
				{
					QueueNum--;
					return true;
				}
				QueueEmptyEvent->Trigger();
			}

			if (bCompleteAdding)
			{
				return false;
			}

			NewCommitEvent->Wait();
		}
	}

	void CompleteAdding()
	{
		bCompleteAdding = true;
		NewCommitEvent->Trigger();
	}

	void ResetAdding()
	{
		bCompleteAdding = false;
	}

	void WaitUntilEmpty()
	{
		bool bWait = false;
		{
			FScopeLock _(&QueueCriticalSection);
			if (QueueNum > 0)
			{
				QueueEmptyEvent->Reset();
				bWait = true;
			}
		}

		if (bWait)
		{
			QueueEmptyEvent->Wait();
		}
	}

private:
	FEventRef					NewCommitEvent;
	FEventRef					QueueEmptyEvent;
	FCriticalSection			QueueCriticalSection;
	TQueue<FZenCommitInfo>		Queue;
	int32						QueueNum = 0;
	TAtomic<bool>				bCompleteAdding{false};
};

TArray<const UTF8CHAR*> FZenStoreWriter::ReservedOplogKeys;

void FZenStoreWriter::StaticInit()
{
	if (ReservedOplogKeys.Num() > 0)
	{
		return;
	}

	ReservedOplogKeys.Append({ UTF8TEXT("files"), UTF8TEXT("key"), UTF8TEXT("package"), UTF8TEXT("packagestoreentry") });
	Algo::Sort(ReservedOplogKeys, [](const UTF8CHAR* A, const UTF8CHAR* B)
		{
			return FUtf8StringView(A).Compare(FUtf8StringView(B), ESearchCase::IgnoreCase) < 0;
		});;
}

FZenStoreWriter::FZenStoreWriter(
	const FString& InOutputPath, 
	const FString& InMetadataDirectoryPath, 
	const ITargetPlatform* InTargetPlatform
)
	: TargetPlatform(*InTargetPlatform)
	, TargetPlatformFName(*InTargetPlatform->PlatformName())
	, OutputPath(InOutputPath)
	, MetadataDirectoryPath(InMetadataDirectoryPath)
	, PackageStoreManifest(InOutputPath)
	, PackageStoreOptimizer(new FPackageStoreOptimizer())
	, CookMode(ICookedPackageWriter::FCookInfo::CookByTheBookMode)
	, bInitialized(false)
{
	StaticInit();

	ProjectId = FApp::GetZenStoreProjectId();
	
	if (FParse::Value(FCommandLine::Get(), TEXT("-ZenStorePlatform="), OplogId) == false)
	{
		OplogId = InTargetPlatform->PlatformName();
	}
	
	HttpClient = MakeUnique<UE::FZenStoreHttpClient>();

#if UE_WITH_ZEN
	IsLocalConnection = HttpClient->GetZenServiceInstance().IsServiceRunningLocally();
#endif

	FString RootDir = FPaths::RootDir();
	FString EngineDir = FPaths::EngineDir();
	FPaths::NormalizeDirectoryName(EngineDir);
	FString ProjectDir = FPaths::ProjectDir();
	FPaths::NormalizeDirectoryName(ProjectDir);
	FString ProjectPath = FPaths::GetProjectFilePath();
	FPaths::NormalizeFilename(ProjectPath);

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	FString AbsServerRoot = PlatformFile.ConvertToAbsolutePathForExternalAppForRead(*RootDir);
	FString AbsEngineDir = PlatformFile.ConvertToAbsolutePathForExternalAppForRead(*EngineDir);
	FString AbsProjectDir = PlatformFile.ConvertToAbsolutePathForExternalAppForRead(*ProjectDir);
	FString ProjectFilePath = PlatformFile.ConvertToAbsolutePathForExternalAppForRead(*ProjectPath);

	HttpClient->TryCreateProject(ProjectId, OplogId, AbsServerRoot, AbsEngineDir, AbsProjectDir, IsLocalConnection ? ProjectFilePath : FStringView());

	PackageStoreOptimizer->Initialize();

	FPackageStoreManifest::FZenServerInfo& ZenServerInfo = PackageStoreManifest.EditZenServerInfo();

#if UE_WITH_ZEN
	const UE::Zen::FZenServiceInstance& ZenServiceInstance = HttpClient->GetZenServiceInstance();
	ZenServerInfo.Settings = ZenServiceInstance.GetServiceSettings();
#endif
	ZenServerInfo.ProjectId = ProjectId;
	ZenServerInfo.OplogId = OplogId;

	ZenFileSystemManifest = MakeUnique<FZenFileSystemManifest>(TargetPlatform, OutputPath);
	
	CommitQueue = MakeUnique<FCommitQueue>();
	
	Compressor = FOodleDataCompression::ECompressor::Mermaid;
	CompressionLevel = FOodleDataCompression::ECompressionLevel::VeryFast;
}

FZenStoreWriter::~FZenStoreWriter()
{
	FScopeLock _(&PackagesCriticalSection);

	if (PendingPackages.Num())
	{
		UE_LOG(LogZenStoreWriter, Warning, TEXT("Pending packages at shutdown!"));
	}
}

void FZenStoreWriter::WritePackageData(const FPackageInfo& Info, FLargeMemoryWriter& ExportsArchive, const TArray<FFileRegion>& FileRegions)
{
	check(Info.ChunkId.IsValid());
	int64 DataSize = ExportsArchive.TotalSize();
	FIoBuffer PackageData(FIoBuffer::AssumeOwnership, ExportsArchive.ReleaseOwnership(), DataSize);

	TRACE_CPUPROFILER_EVENT_SCOPE(FZenStoreWriter::WritePackageData);

	FIoBuffer CookedHeaderBuffer = FIoBuffer(PackageData.Data(), Info.HeaderSize, PackageData);
	FIoBuffer CookedExportsBuffer = FIoBuffer(PackageData.Data() + Info.HeaderSize, PackageData.DataSize() - Info.HeaderSize, PackageData);
	TUniquePtr<FPackageStorePackage> Package{PackageStoreOptimizer->CreatePackageFromCookedHeader(Info.PackageName, CookedHeaderBuffer)};
	PackageStoreOptimizer->FinalizePackage(Package.Get());
	TArray<FFileRegion> FileRegionsCopy(FileRegions);
	for (FFileRegion& Region : FileRegionsCopy)
	{
		// Adjust regions so they are relative to the start of the exports buffer
		Region.Offset -= Info.HeaderSize;
	}
	FIoBuffer PackageBuffer = PackageStoreOptimizer->CreatePackageBuffer(Package.Get(), CookedExportsBuffer, &FileRegionsCopy);
	PackageStoreManifest.AddPackageData(Info.PackageName, Info.LooseFilePath, Info.ChunkId);
	for (FFileRegion& Region : FileRegionsCopy)
	{
		// Adjust regions once more so they are relative to the exports bundle buffer
		Region.Offset -= Package->GetHeaderSize();
	}
	//WriteFileRegions(*FPaths::ChangeExtension(Info.LooseFilePath, FString(".uexp") + FFileRegion::RegionsFileExtension), FileRegionsCopy);

	// Commit to Zen build store

	FCbObjectId ChunkOid = ToObjectId(Info.ChunkId);

	FPendingPackageState& ExistingState = GetPendingPackage(Info.PackageName);

	FPackageDataEntry& Entry = ExistingState.PackageData;

	Entry.CompressedPayload = Async(EAsyncExecution::TaskGraph, [this, PackageBuffer]()
	{ 
		return FCompressedBuffer::Compress(FSharedBuffer::MakeView(PackageBuffer.GetView()), Compressor, CompressionLevel);
	});

	Entry.Info				= Info;
	Entry.ChunkId			= ChunkOid;
	Entry.PackageStoreEntry = PackageStoreOptimizer->CreatePackageStoreEntry(Package.Get(), nullptr); // TODO: Can we separate out the optional segment package store entry and do this when we commit instead?
	Entry.IsValid			= true;

	if (EntryCreatedEvent.IsBound())
	{
		IPackageStoreWriter::FEntryCreatedEventArgs EntryCreatedEventArgs
		{
			TargetPlatformFName,
			Entry.PackageStoreEntry
		};
		EntryCreatedEvent.Broadcast(EntryCreatedEventArgs);
	}
}

void FZenStoreWriter::WriteIoStorePackageData(const FPackageInfo& Info, const FIoBuffer& PackageData, const FPackageStoreEntryResource& PackageStoreEntry, const TArray<FFileRegion>& FileRegions)
{
	check(Info.ChunkId.IsValid());

	TRACE_CPUPROFILER_EVENT_SCOPE(WriteIoStorePackageData);

	PackageStoreManifest.AddPackageData(Info.PackageName, Info.LooseFilePath, Info.ChunkId);
	//WriteFileRegions(*FPaths::ChangeExtension(Info.LooseFilePath, FString(".uexp") + FFileRegion::RegionsFileExtension), FileRegionsCopy);

	FCbObjectId ChunkOid = ToObjectId(Info.ChunkId);

	FPendingPackageState& ExistingState = GetPendingPackage(Info.PackageName);

	FPackageDataEntry& Entry = ExistingState.PackageData;

	PackageData.EnsureOwned();

	Entry.CompressedPayload = Async(EAsyncExecution::TaskGraph, [this, PackageData]()
	{ 
		return FCompressedBuffer::Compress(FSharedBuffer::MakeView(PackageData.GetView()), Compressor, CompressionLevel);
	});

	Entry.Info				= Info;
	Entry.ChunkId			= ChunkOid;
	Entry.PackageStoreEntry = PackageStoreEntry;
	Entry.IsValid			= true;
}

void FZenStoreWriter::WriteBulkData(const FBulkDataInfo& Info, const FIoBuffer& BulkData, const TArray<FFileRegion>& FileRegions)
{
	check(Info.ChunkId.IsValid());

	FCbObjectId ChunkOid = ToObjectId(Info.ChunkId);

	FPendingPackageState& ExistingState = GetPendingPackage(Info.PackageName);

	FBulkDataEntry& BulkEntry = ExistingState.BulkData.AddDefaulted_GetRef(); 

	BulkData.EnsureOwned();

	BulkEntry.CompressedPayload = Async(EAsyncExecution::TaskGraph, [this, BulkData]()
	{ 
		return FCompressedBuffer::Compress(FSharedBuffer::MakeView(BulkData.GetView()), Compressor, CompressionLevel);
	});

	BulkEntry.Info		= Info;
	BulkEntry.ChunkId	= ChunkOid;
	BulkEntry.IsValid	= true;

	PackageStoreManifest.AddBulkData(Info.PackageName, Info.LooseFilePath, Info.ChunkId);

	//	WriteFileRegions(*(Info.LooseFilePath + FFileRegion::RegionsFileExtension), FileRegions);
}

void FZenStoreWriter::WriteAdditionalFile(const FAdditionalFileInfo& Info, const FIoBuffer& FileData)
{
	const FZenFileSystemManifest::FManifestEntry& ManifestEntry = ZenFileSystemManifest->CreateManifestEntry(Info.Filename);
	
	FPendingPackageState& ExistingState = GetPendingPackage(Info.PackageName);

	FFileDataEntry& FileEntry = ExistingState.FileData.AddDefaulted_GetRef();
	
	FileData.EnsureOwned();

	FileEntry.CompressedPayload = Async(EAsyncExecution::TaskGraph, [this, FileData]()
	{ 
		return FCompressedBuffer::Compress(FSharedBuffer::MakeView(FileData.GetView()), Compressor, CompressionLevel);
	});

	FileEntry.Info					= Info;
	FileEntry.Info.ChunkId			= ManifestEntry.FileChunkId;
	FileEntry.ZenManifestServerPath = ManifestEntry.ServerPath;
	FileEntry.ZenManifestClientPath = ManifestEntry.ClientPath;
}

void FZenStoreWriter::WriteLinkerAdditionalData(const FLinkerAdditionalDataInfo& Info, const FIoBuffer& Data, const TArray<FFileRegion>& FileRegions)
{
	// LinkerAdditionalData is not yet implemented in this writer; it is only used for VirtualizedBulkData which is not used in cooked content
	checkNoEntry();
}

void FZenStoreWriter::Initialize(const FCookInfo& Info)
{
	CookMode = Info.CookMode;

	if (!bInitialized)
	{
		if (Info.bFullBuild && !Info.bWorkerOnSharedSandbox)
		{
			UE_LOG(LogZenStoreWriter, Display, TEXT("Deleting %s..."), *OutputPath);
			const bool bRequireExists = false;
			const bool bTree = true;
			IFileManager::Get().DeleteDirectory(*OutputPath, bRequireExists, bTree);
		}

		bool bOplogEstablished = HttpClient->TryCreateOplog(ProjectId, OplogId, Info.bFullBuild);
		UE_CLOG(!bOplogEstablished, LogZenStoreWriter, Fatal, TEXT("Failed to establish oplog on the ZenServer"));

		if (!Info.bFullBuild)
		{
			UE_LOG(LogZenStoreWriter, Display, TEXT("Fetching oplog..."), *ProjectId, *OplogId);

			TFuture<FIoStatus> FutureOplogStatus = HttpClient->GetOplog().Next([this](TIoStatusOr<FCbObject> OplogStatus)
				{
					if (!OplogStatus.IsOk())
					{
						return OplogStatus.Status();
					}

					FCbObject Oplog = OplogStatus.ConsumeValueOrDie();

					if (Oplog["entries"])
					{
						for (FCbField& OplogEntry : Oplog["entries"].AsArray())
						{
							FCbObject OplogObj = OplogEntry.AsObject();

							if (OplogObj["package"])
							{
								FCbObject PackageObj = OplogObj["package"].AsObject();

								const FGuid PkgGuid = PackageObj["guid"].AsUuid();
								const FIoHash PkgHash = PackageObj["data"].AsHash();
								const int64	PkgDiskSize = PackageObj["disksize"].AsUInt64();
								FPackageStoreEntryResource Entry = FPackageStoreEntryResource::FromCbObject(OplogObj["packagestoreentry"].AsObject());
								const FName PackageName = Entry.PackageName;

								const int32 Index = PackageStoreEntries.Num();

								PackageStoreEntries.Add(MoveTemp(Entry));
								FOplogCookInfo& CookInfo = CookedPackagesInfo.Add_GetRef(
									FOplogCookInfo{
										FCookedPackageInfo {PackageName, IoHashToMD5(PkgHash), PkgGuid, PkgDiskSize }
									});
								PackageNameToIndex.Add(PackageName, Index);

								for (FCbFieldView Field : OplogObj)
								{
									FUtf8StringView FieldName = Field.GetName();
									if (IsReservedOplogKey(FieldName))
									{
										continue;
									}
									if (Field.IsHash())
									{
										const UTF8CHAR* AttachmentId = UE::FZenStoreHttpClient::FindOrAddAttachmentId(FieldName);
										CookInfo.Attachments.Add({ AttachmentId, Field.AsHash() });
									}
								}
								CookInfo.Attachments.Shrink();
								check(Algo::IsSorted(CookInfo.Attachments,
									[](const FOplogCookInfo::FAttachment& A, const FOplogCookInfo::FAttachment& B)
									{
										return FUtf8StringView(A.Key).Compare(FUtf8StringView(B.Key), ESearchCase::IgnoreCase) < 0;
									}));
							}
						}
					}

					return FIoStatus::Ok;
				});

			UE_LOG(LogZenStoreWriter, Display, TEXT("Fetching file manifest..."), *ProjectId, *OplogId);

			TIoStatusOr<FCbObject> FileStatus = HttpClient->GetFiles().Get();
			if (FileStatus.IsOk())
			{
				FCbObject FilesObj = FileStatus.ConsumeValueOrDie();
				for (FCbField& FileEntry : FilesObj["files"])
				{
					FCbObject FileObj = FileEntry.AsObject();
					FCbObjectId FileId = FileObj["id"].AsObjectId();
					FString ServerPath = FString(FileObj["serverpath"].AsString());
					FString ClientPath = FString(FileObj["clientpath"].AsString());

					FIoChunkId FileChunkId;
					FileChunkId.Set(FileId.GetView());

					ZenFileSystemManifest->AddManifestEntry(FileChunkId, MoveTemp(ServerPath), MoveTemp(ClientPath));
				}

				UE_LOG(LogZenStoreWriter, Display, TEXT("Fetched '%d' file(s) from oplog '%s/%s'"), ZenFileSystemManifest->NumEntries(), *ProjectId, *OplogId);
			}
			else
			{
				UE_LOG(LogZenStoreWriter, Warning, TEXT("Failed to fetch file(s) from oplog '%s/%s'"), *ProjectId, *OplogId);
			}

			if (FutureOplogStatus.Get().IsOk())
			{
				UE_LOG(LogZenStoreWriter, Display, TEXT("Fetched '%d' packages(s) from oplog '%s/%s'"), PackageStoreEntries.Num(), *ProjectId, *OplogId);
			}
			else
			{
				UE_LOG(LogZenStoreWriter, Warning, TEXT("Failed to fetch oplog '%s/%s'"), *ProjectId, *OplogId);
			}
		}
		bInitialized = true;
	}
	else
	{
		if (Info.bFullBuild)
		{
			RemoveCookedPackages();
		}
	}
}

void FZenStoreWriter::BeginCook()
{
	PackageStoreManifest.Load(*(MetadataDirectoryPath / TEXT("packagestore.manifest")));
	AllPackageHashes.Empty();

	if (CookMode == ICookedPackageWriter::FCookInfo::CookOnTheFlyMode)
	{
		FCbPackage Pkg;
		FCbWriter PackageObj;
		
		PackageObj.BeginObject();
		PackageObj << "key" << "CookOnTheFly";

		const bool bGenerateContainerHeader = false;
		CreateProjectMetaData(Pkg, PackageObj, bGenerateContainerHeader);

		PackageObj.EndObject();
		FCbObject Obj = PackageObj.Save().AsObject();

		Pkg.SetObject(Obj);

		TIoStatusOr<uint64> Status = HttpClient->AppendOp(Pkg);
		UE_CLOG(!Status.IsOk(), LogZenStoreWriter, Fatal, TEXT("Failed to append OpLog"));
	}

	if (FPlatformProcess::SupportsMultithreading())
	{
		CommitQueue->ResetAdding();
		CommitThread = AsyncThread([this]()
		{ 
			for(;;)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(FZenStoreWriter::WaitingOnCooker);
				FZenCommitInfo Commit;
				if (!CommitQueue->BlockAndDequeue(Commit))
				{
					break;
				}
				CommitPackageInternal(MoveTemp(Commit));
			}
		});
	}
}

void FZenStoreWriter::EndCook()
{
	UE_LOG(LogZenStoreWriter, Display, TEXT("Flushing..."));
	CommitQueue->WaitUntilEmpty();
	
	CommitQueue->CompleteAdding();
	
	FCbPackage Pkg;
	FCbWriter PackageObj;
	
	PackageObj.BeginObject();
	PackageObj << "key" << "EndCook";

	const bool bGenerateContainerHeader = true;
	CreateProjectMetaData(Pkg, PackageObj, bGenerateContainerHeader);

	PackageObj.EndObject();
	FCbObject Obj = PackageObj.Save().AsObject();

	Pkg.SetObject(Obj);

	TIoStatusOr<uint64> Status = HttpClient->EndBuildPass(Pkg);
	UE_CLOG(!Status.IsOk(), LogZenStoreWriter, Fatal, TEXT("Failed to append OpLog and end the build pass"));

	PackageStoreManifest.Save(*(MetadataDirectoryPath / TEXT("packagestore.manifest")));

	UE_LOG(LogZenStoreWriter, Display, TEXT("Input:\t%d Packages"), PackageStoreOptimizer->GetTotalPackageCount());
	UE_LOG(LogZenStoreWriter, Display, TEXT("Output:\t%d Export bundles"), PackageStoreOptimizer->GetTotalExportBundleCount());
	UE_LOG(LogZenStoreWriter, Display, TEXT("Output:\t%d Export bundle entries"), PackageStoreOptimizer->GetTotalExportBundleEntryCount());
	UE_LOG(LogZenStoreWriter, Display, TEXT("Output:\t%d Internal export bundle arcs"), PackageStoreOptimizer->GetTotalInternalBundleArcsCount());
	UE_LOG(LogZenStoreWriter, Display, TEXT("Output:\t%d External export bundle arcs"), PackageStoreOptimizer->GetTotalExternalBundleArcsCount());
	UE_LOG(LogZenStoreWriter, Display, TEXT("Output:\t%d Public runtime script objects"), PackageStoreOptimizer->GetTotalScriptObjectCount());
}

void FZenStoreWriter::BeginPackage(const FBeginPackageInfo& Info)
{
	FPendingPackageState& State = AddPendingPackage(Info.PackageName);
	State.PackageName = Info.PackageName;

	PackageStoreManifest.BeginPackage(Info.PackageName);
}

bool FZenStoreWriter::IsReservedOplogKey(FUtf8StringView Key)
{
	int32 Index = Algo::LowerBound(ReservedOplogKeys, Key,
		[](const UTF8CHAR* Existing, FUtf8StringView Key)
		{
			return FUtf8StringView(Existing).Compare(Key, ESearchCase::IgnoreCase) < 0;
		});
	return Index != ReservedOplogKeys.Num() &&
		FUtf8StringView(ReservedOplogKeys[Index]).Equals(Key, ESearchCase::IgnoreCase);
}

void FZenStoreWriter::CommitPackage(FCommitPackageInfo&& Info)
{
	if (Info.Status == ECommitStatus::Canceled)
	{
		RemovePendingPackage(Info.PackageName);
		return;
	}

	UE::SavePackageUtilities::IncrementOutstandingAsyncWrites();

	// If we are computing hashes, we need to allocate where the hashes will go.
	// Access to this is protected by the above IncrementOutstandingAsyncWrites.
	if (EnumHasAnyFlags(Info.WriteOptions, EWriteOptions::ComputeHash))
	{
		FPendingPackageState& ExistingState = GetPendingPackage(Info.PackageName);
		ExistingState.PackageHashes = new FPackageHashes();

		if (Info.Status == ECommitStatus::Success)
		{
			// Only record hashes for successful saves. A single package can be saved unsuccessfully multiple times
			// during a cook if its rejected for being only referenced by editor-only references and we keep finding
			// new references to it.
			TRefCountPtr<FPackageHashes>& ExistingPackageHashes = AllPackageHashes.FindOrAdd(Info.PackageName);
			// This looks weird but we've found the _TRefCountPtr_, not the FPackageHashes. When newly assigned
			// it will be an empty pointer, which is what we want.
			if (ExistingPackageHashes.IsValid())
			{
				UE_LOG(LogZenStoreWriter, Error, TEXT("FZenStoreWriter commiting the same package twice during a cook! (%s)"), *Info.PackageName.ToString());
			}
			ExistingPackageHashes = ExistingState.PackageHashes;
		}
	}

	TUniquePtr<FPendingPackageState> PackageState = RemovePendingPackage(Info.PackageName);
	FZenCommitInfo ZenCommitInfo{ Forward<FCommitPackageInfo>(Info), MoveTemp(PackageState) };
	if (FPlatformProcess::SupportsMultithreading())
	{
		CommitQueue->Enqueue(MoveTemp(ZenCommitInfo));
	}
	else
	{
		CommitPackageInternal(MoveTemp(ZenCommitInfo));
	}
}

void FZenStoreWriter::CommitPackageInternal(FZenCommitInfo&& ZenCommitInfo)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FZenStoreWriter::CommitPackage);
	FCommitPackageInfo& CommitInfo = ZenCommitInfo.CommitInfo;
	
	TUniquePtr<FPendingPackageState> PackageState = MoveTemp(ZenCommitInfo.PackageState);
	checkf(PackageState.IsValid(), TEXT("Trying to commit non-pending package '%s'"), *CommitInfo.PackageName.ToString());

	IPackageStoreWriter::FCommitEventArgs CommitEventArgs;

	CommitEventArgs.PlatformName	= TargetPlatformFName;
	CommitEventArgs.PackageName		= CommitInfo.PackageName;
	CommitEventArgs.EntryIndex		= INDEX_NONE;
	
	const bool bComputeHash			= EnumHasAnyFlags(CommitInfo.WriteOptions, EWriteOptions::ComputeHash);
	const bool bWritePackage		= EnumHasAnyFlags(CommitInfo.WriteOptions, EWriteOptions::Write);

	if (CommitInfo.Status == ECommitStatus::Success && bWritePackage)
	{
		FPackageDataEntry& PkgData = PackageState->PackageData;
		checkf(PkgData.IsValid, TEXT("CommitPackage called with bSucceeded but without first calling WritePackageData"))
		checkf(EnumHasAllFlags(CommitInfo.WriteOptions, EWriteOptions::Write), TEXT("Partial EWriteOptions::Write options are not yet implemented."));
		checkf(!EnumHasAnyFlags(CommitInfo.WriteOptions, EWriteOptions::SaveForDiff), TEXT("-diffonly -savefordiff is not yet implemented."));

		{
			FWriteScopeLock _(EntriesLock);
			CommitEventArgs.EntryIndex = PackageNameToIndex.FindOrAdd(CommitInfo.PackageName, PackageStoreEntries.Num());
			if (CommitEventArgs.EntryIndex == PackageStoreEntries.Num())
			{
				PackageStoreEntries.Emplace();
				CookedPackagesInfo.Emplace();
			}
		}
		
		PackageStoreEntries[CommitEventArgs.EntryIndex] = PkgData.PackageStoreEntry;
		
		FMD5 PkgHashGen;
		FCbPackage OplogEntry;
		
		if (bComputeHash)
		{
			PackageState->PackageHashes->ChunkHashes.Add(PkgData.Info.ChunkId, PkgData.CompressedPayload.Get().GetRawHash());
		}

		FCbAttachment PkgDataAttachment = FCbAttachment(PkgData.CompressedPayload.Get());
		PkgHashGen.Update(PkgDataAttachment.GetHash().GetBytes(), sizeof(FIoHash::ByteArray));
		OplogEntry.AddAttachment(PkgDataAttachment);
		
		// Commit attachments
		FOplogCookInfo& CookInfo = CookedPackagesInfo[CommitEventArgs.EntryIndex];
		CookInfo = FOplogCookInfo
		{
			FCookedPackageInfo
			{ 
				CommitInfo.PackageName,
				IoHashToMD5(PkgDataAttachment.GetHash()),
				CommitInfo.PackageGuid,
				int64(PkgDataAttachment.AsCompressedBinary().GetRawSize())
			}
		};

		CookInfo.bUpToDate = true;

		const int32 NumAttachments = CommitInfo.Attachments.Num();
		TArray<FCbAttachment, TInlineAllocator<2>> CbAttachments;
		
		if (NumAttachments)
		{
			TArray<const FCommitAttachmentInfo*, TInlineAllocator<2>> SortedAttachments;
			SortedAttachments.Reserve(NumAttachments);
			
			for (const FCommitAttachmentInfo& AttachmentInfo : CommitInfo.Attachments)
			{
				SortedAttachments.Add(&AttachmentInfo);
			}

			SortedAttachments.Sort([](const FCommitAttachmentInfo& A, const FCommitAttachmentInfo& B)
			{
				return A.Key.Compare(B.Key, ESearchCase::IgnoreCase) < 0;
			});
			
			CbAttachments.Reserve(NumAttachments);
			CookInfo.Attachments.Reserve(NumAttachments);
			
			for (const FCommitAttachmentInfo* AttachmentInfo : SortedAttachments)
			{
				check(!IsReservedOplogKey(AttachmentInfo->Key));
				const FCbAttachment& CbAttachment = CbAttachments.Add_GetRef(CreateAttachment(AttachmentInfo->Value.GetBuffer().ToShared()));
				OplogEntry.AddAttachment(CbAttachment);
				
				CookInfo.Attachments.Add(FOplogCookInfo::FAttachment
				{
					UE::FZenStoreHttpClient::FindOrAddAttachmentId(AttachmentInfo->Key), CbAttachment.GetHash()
				});
			}
		}

		// Create the oplog entry object
		FCbWriter OplogEntryDesc;
		OplogEntryDesc.BeginObject();
		FString PackageNameKey = CommitInfo.PackageName.ToString();
		PackageNameKey.ToLowerInline();
		OplogEntryDesc << "key" << PackageNameKey;

		// NOTE: The package GUID and disk size are used for legacy iterative cooks when comparing asset registry package data
		OplogEntryDesc.BeginObject("package");
		OplogEntryDesc << "id" << PkgData.ChunkId;
		OplogEntryDesc << "guid" << CommitInfo.PackageGuid;
		OplogEntryDesc << "data" << PkgDataAttachment;
		OplogEntryDesc << "disksize" << PkgDataAttachment.AsCompressedBinary().GetRawSize();
		OplogEntryDesc.EndObject();

		OplogEntryDesc << "packagestoreentry" << PkgData.PackageStoreEntry;
		
		if (PackageState->BulkData.Num())
		{
			OplogEntryDesc.BeginArray("bulkdata");

			for (FBulkDataEntry& Bulk : PackageState->BulkData)
			{
				if (bComputeHash)
				{
					PackageState->PackageHashes->ChunkHashes.Add(Bulk.Info.ChunkId, Bulk.CompressedPayload.Get().GetRawHash());
				}

				FCbAttachment BulkAttachment(Bulk.CompressedPayload.Get());
				PkgHashGen.Update(BulkAttachment.GetHash().GetBytes(), sizeof(FIoHash::ByteArray));
				OplogEntry.AddAttachment(BulkAttachment);

				OplogEntryDesc.BeginObject();
				OplogEntryDesc << "id" << Bulk.ChunkId;
				OplogEntryDesc << "type" << LexToString(Bulk.Info.BulkDataType);
				OplogEntryDesc << "data" << BulkAttachment;
				OplogEntryDesc.EndObject();
			}

			OplogEntryDesc.EndArray();
		}

		if (PackageState->FileData.Num())
		{
			OplogEntryDesc.BeginArray("files");

			for (FFileDataEntry& File : PackageState->FileData)
			{
				if (bComputeHash)
				{
					PackageState->PackageHashes->ChunkHashes.Add(File.Info.ChunkId, File.CompressedPayload.Get().GetRawHash());
				}

				FCbAttachment FileDataAttachment(File.CompressedPayload.Get());
				PkgHashGen.Update(FileDataAttachment.GetHash().GetBytes(), sizeof(FIoHash::ByteArray));
				OplogEntry.AddAttachment(FileDataAttachment);

				OplogEntryDesc.BeginObject();
				OplogEntryDesc << "id" << ToObjectId(File.Info.ChunkId);
				OplogEntryDesc << "data" << FileDataAttachment;
				OplogEntryDesc << "serverpath" << File.ZenManifestServerPath;
				OplogEntryDesc << "clientpath" << File.ZenManifestClientPath;
				OplogEntryDesc.EndObject();

				CommitEventArgs.AdditionalFiles.Add(FAdditionalFileInfo
				{ 
					CommitInfo.PackageName,
					File.ZenManifestClientPath,
					File.Info.ChunkId
				});
			}

			OplogEntryDesc.EndArray();
		}

		if (bComputeHash)
		{
			PackageState->PackageHashes->PackageHash.Set(PkgHashGen);
		}

		for (int32 Index = 0; Index < NumAttachments; ++Index)
		{
			FCbAttachment& CbAttachment = CbAttachments[Index];
			FOplogCookInfo::FAttachment& CookInfoAttachment = CookInfo.Attachments[Index];
			OplogEntryDesc << CookInfoAttachment.Key << CbAttachment;
		}

		OplogEntryDesc.EndObject();
		OplogEntry.SetObject(OplogEntryDesc.Save().AsObject());
		
		TIoStatusOr<uint64> Status = HttpClient->AppendOp(MoveTemp(OplogEntry));
		UE_CLOG(!Status.IsOk(), LogZenStoreWriter, Error, TEXT("Failed to commit oplog entry '%s' to Zen"), *CommitInfo.PackageName.ToString());
	}
	else if (CommitInfo.Status == ECommitStatus::Success && bComputeHash)
	{
		FPackageDataEntry& PkgData = PackageState->PackageData;
		checkf(PkgData.IsValid, TEXT("CommitPackage called with bSucceeded but without first calling WritePackageData"));
		
		FMD5 PkgHashGen;
		
		{
			FCompressedBuffer Payload = PkgData.CompressedPayload.Get();
			FIoHash IoHash = Payload.GetRawHash();
			PackageState->PackageHashes->ChunkHashes.Add(PkgData.Info.ChunkId, IoHash);
			PkgHashGen.Update(IoHash.GetBytes(), sizeof(FIoHash::ByteArray));
		}
		
		for (FBulkDataEntry& Bulk : PackageState->BulkData)
		{
			FCompressedBuffer Payload = Bulk.CompressedPayload.Get();
			FIoHash IoHash = Payload.GetRawHash();
			PackageState->PackageHashes->ChunkHashes.Add(Bulk.Info.ChunkId, IoHash);
			PkgHashGen.Update(IoHash.GetBytes(), sizeof(FIoHash::ByteArray));
		}
		
		for (FFileDataEntry& File : PackageState->FileData)
		{
			FCompressedBuffer Payload = File.CompressedPayload.Get();
			FIoHash IoHash = Payload.GetRawHash();
			PackageState->PackageHashes->ChunkHashes.Add(File.Info.ChunkId, IoHash);
			PkgHashGen.Update(IoHash.GetBytes(), sizeof(FIoHash::ByteArray));
		}
		
		PackageState->PackageHashes->PackageHash.Set(PkgHashGen);
	}

	if (bWritePackage)
	{
		BroadcastCommit(CommitEventArgs);
	}

	UE::SavePackageUtilities::DecrementOutstandingAsyncWrites();
}

void FZenStoreWriter::GetEntries(TFunction<void(TArrayView<const FPackageStoreEntryResource>, TArrayView<const FOplogCookInfo>)>&& Callback)
{
	FReadScopeLock _(EntriesLock);
	Callback(PackageStoreEntries, CookedPackagesInfo);
}

TUniquePtr<FAssetRegistryState> FZenStoreWriter::LoadPreviousAssetRegistry() 
{
	// Load the previous asset registry to return to CookOnTheFlyServer, and set the packages enumerated in both *this and
	// the returned asset registry to the intersection of the oplog and the previous asset registry;
	// to report a package as already cooked we have to have the information from both sources.
	FString PreviousAssetRegistryFile = FPaths::Combine(MetadataDirectoryPath, GetDevelopmentAssetRegistryFilename());
	FArrayReader SerializedAssetData;
	if (!IFileManager::Get().FileExists(*PreviousAssetRegistryFile) ||
		!FFileHelper::LoadFileToArray(SerializedAssetData, *PreviousAssetRegistryFile))
	{
		RemoveCookedPackages();
		return TUniquePtr<FAssetRegistryState>();
	}

	TUniquePtr<FAssetRegistryState> PreviousState = MakeUnique<FAssetRegistryState>();
	PreviousState->Load(SerializedAssetData);

	TSet<FName> RemoveSet;
	const TMap<FName, const FAssetPackageData*>& PreviousStatePackages = PreviousState->GetAssetPackageDataMap(); 
	for (const TPair<FName, const FAssetPackageData*>& Pair : PreviousStatePackages)
	{
		FName PackageName = Pair.Key;
		if (!PackageNameToIndex.Find(PackageName))
		{
			RemoveSet.Add(PackageName);
		}
	}
	if (RemoveSet.Num())
	{
		PreviousState->PruneAssetData(TSet<FName>(), RemoveSet, FAssetRegistrySerializationOptions());
	}

	TArray<FName> RemoveArray;
	for (TPair<FName, int32>& Pair : PackageNameToIndex)
	{
		FName PackageName = Pair.Key;
		if (!PreviousStatePackages.Find(PackageName))
		{
			RemoveArray.Add(PackageName);
		}
	}
	if (RemoveArray.Num())
	{
		RemoveCookedPackages(RemoveArray);
	}

	return PreviousState;
}

FCbObject FZenStoreWriter::GetOplogAttachment(FName PackageName, FUtf8StringView AttachmentKey)
{
	const int32* Idx = PackageNameToIndex.Find(PackageName);
	if (!Idx)
	{
		return FCbObject();
	}

	const UTF8CHAR* AttachmentId = UE::FZenStoreHttpClient::FindAttachmentId(AttachmentKey);
	if (!AttachmentId)
	{
		return FCbObject();
	}
	FUtf8StringView AttachmentIdView(AttachmentId);

	const FOplogCookInfo& CookInfo = CookedPackagesInfo[*Idx];
	int32 AttachmentIndex = Algo::LowerBound(CookInfo.Attachments, AttachmentIdView,
		[](const FOplogCookInfo::FAttachment& Existing, FUtf8StringView AttachmentIdView)
		{
			return FUtf8StringView(Existing.Key).Compare(AttachmentIdView, ESearchCase::IgnoreCase) < 0;
		});
	if (AttachmentIndex == CookInfo.Attachments.Num())
	{
		return FCbObject();
	}
	const FOplogCookInfo::FAttachment& Existing = CookInfo.Attachments[AttachmentIndex];
	if (!FUtf8StringView(Existing.Key).Equals(AttachmentIdView, ESearchCase::IgnoreCase))
	{
		return FCbObject();
	}
	TIoStatusOr<FIoBuffer> BufferResult = HttpClient->ReadOpLogAttachment(WriteToString<48>(Existing.Hash));
	if (!BufferResult.IsOk())
	{
		return FCbObject();
	}
	FIoBuffer Buffer = BufferResult.ValueOrDie();
	if (Buffer.DataSize() == 0)
	{
		return FCbObject();
	}

	FSharedBuffer SharedBuffer = IoBufferToSharedBuffer(Buffer);
	return FCbObject(SharedBuffer);
}

void FZenStoreWriter::RemoveCookedPackages(TArrayView<const FName> PackageNamesToRemove)
{
	TSet<int32> PackageIndicesToKeep;
	for (int32 Idx = 0, Num = PackageStoreEntries.Num(); Idx < Num; ++Idx)
	{
		PackageIndicesToKeep.Add(Idx);
	}
	
	for (const FName& PackageName : PackageNamesToRemove)
	{
		if (const int32* Idx = PackageNameToIndex.Find(PackageName))
		{
			PackageIndicesToKeep.Remove(*Idx);
		}
	}

	const int32 NumPackagesToKeep = PackageIndicesToKeep.Num();
	
	TArray<FPackageStoreEntryResource> PreviousPackageStoreEntries = MoveTemp(PackageStoreEntries);
	TArray<FOplogCookInfo> PreviousCookedPackageInfo = MoveTemp(CookedPackagesInfo);
	PackageNameToIndex.Empty();

	if (NumPackagesToKeep > 0)
	{
		PackageStoreEntries.Reserve(NumPackagesToKeep);
		CookedPackagesInfo.Reserve(NumPackagesToKeep);
		PackageNameToIndex.Reserve(NumPackagesToKeep);

		int32 EntryIndex = 0;
		for (int32 Idx : PackageIndicesToKeep)
		{
			const FName PackageName = PreviousCookedPackageInfo[Idx].CookInfo.PackageName;

			PackageStoreEntries.Add(MoveTemp(PreviousPackageStoreEntries[Idx]));
			CookedPackagesInfo.Add(MoveTemp(PreviousCookedPackageInfo[Idx]));
			PackageNameToIndex.Add(PackageName, EntryIndex++);
		}
	}
}

void FZenStoreWriter::RemoveCookedPackages()
{
	PackageStoreEntries.Empty();
	CookedPackagesInfo.Empty();
	PackageNameToIndex.Empty();
}

void FZenStoreWriter::MarkPackagesUpToDate(TArrayView<const FName> UpToDatePackages)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FZenStoreWriter::MarkPackagesUpToDate);

	IPackageStoreWriter::FMarkUpToDateEventArgs MarkUpToDateEventArgs;

	MarkUpToDateEventArgs.PackageIndexes.Reserve(UpToDatePackages.Num());

	{
		FWriteScopeLock _(EntriesLock);
		for (FName PackageName : UpToDatePackages)
		{
			int32* Index = PackageNameToIndex.Find(PackageName);
			if (!Index)
			{
				if (!FPackageName::IsScriptPackage(WriteToString<128>(PackageName)))
				{
					UE_LOG(LogZenStoreWriter, Warning, TEXT("MarkPackagesUpToDate called with package %s that is not in the oplog."),
						*PackageName.ToString());
				}
				continue;
			}

			MarkUpToDateEventArgs.PackageIndexes.Add(*Index);
			CookedPackagesInfo[*Index].bUpToDate = true;
		}
	}
	if (MarkUpToDateEventArgs.PackageIndexes.Num())
	{
		BroadcastMarkUpToDate(MarkUpToDateEventArgs);
	}
}

void FZenStoreWriter::CreateProjectMetaData(FCbPackage& Pkg, FCbWriter& PackageObj, bool bGenerateContainerHeader)
{
	// File Manifest
	{
		// Only append new file entries to the Oplog
		
		int32 NumEntries = ZenFileSystemManifest->NumEntries();
		const int32 NumNewEntries = ZenFileSystemManifest->Generate();

		if (NumNewEntries > 0)
		{
			TArrayView<FZenFileSystemManifest::FManifestEntry const> Entries = ZenFileSystemManifest->ManifestEntries();
			TArrayView<FZenFileSystemManifest::FManifestEntry const> NewEntries = Entries.Slice(NumEntries, NumNewEntries);
			
			PackageObj.BeginArray("files");

			for (const FZenFileSystemManifest::FManifestEntry& NewEntry : NewEntries)
			{
				FCbObjectId FileOid = ToObjectId(NewEntry.FileChunkId);

				if (IsLocalConnection)
				{
					PackageObj.BeginObject();
					PackageObj << "id" << FileOid;
					PackageObj << "data" << FIoHash::Zero;
					PackageObj << "serverpath" << NewEntry.ServerPath;
					PackageObj << "clientpath" << NewEntry.ClientPath;
					PackageObj.EndObject();
				}
				else
				{
					const FString AbsPath = ZenFileSystemManifest->ServerRootPath() / NewEntry.ServerPath;
					TArray<uint8> FileBuffer;
					FFileHelper::LoadFileToArray(FileBuffer, *AbsPath);
					if (FileBuffer.Num())
					{
						FCbAttachment FileAttachment = CreateAttachment(FIoBuffer(FIoBuffer::Clone, FileBuffer.GetData(), FileBuffer.Num()));

						PackageObj.BeginObject();
						PackageObj << "id" << FileOid;
						PackageObj << "data" << FileAttachment;
						PackageObj << "serverpath" << NewEntry.ServerPath;
						PackageObj << "clientpath" << NewEntry.ClientPath;
						PackageObj.EndObject();

						Pkg.AddAttachment(FileAttachment);
					}
				}
			}

			PackageObj.EndArray();
		}

		FString ManifestPath = FPaths::Combine(MetadataDirectoryPath, TEXT("zenfs.manifest"));
		UE_LOG(LogZenStoreWriter, Display, TEXT("Saving Zen filesystem manifest '%s'"), *ManifestPath);
		ZenFileSystemManifest->Save(*ManifestPath);
	}

	// Metadata section
	{
		PackageObj.BeginArray("meta");

		// Summarize Script Objects
		FIoBuffer ScriptObjectsBuffer = PackageStoreOptimizer->CreateScriptObjectsBuffer();
		FCbObjectId ScriptOid = ToObjectId(CreateIoChunkId(0, 0, EIoChunkType::ScriptObjects));

		FCbAttachment ScriptAttachment = CreateAttachment(ScriptObjectsBuffer); 
		Pkg.AddAttachment(ScriptAttachment);

		PackageObj.BeginObject();
		PackageObj << "id" << ScriptOid;
		PackageObj << "name" << "ScriptObjects";
		PackageObj << "data" << ScriptAttachment;
		PackageObj.EndObject();

		// Generate Container Header
		if (bGenerateContainerHeader)
		{
			FCbObjectId HeaderOid = ToObjectId(CreateIoChunkId(ContainerId.Value(), 0, EIoChunkType::ContainerHeader));
			FIoBuffer HeaderBuffer;

			{
				FIoContainerHeader Header = PackageStoreOptimizer->CreateContainerHeader(ContainerId, PackageStoreEntries);
				FLargeMemoryWriter HeaderAr(0, true);
				HeaderAr << Header;
				int64 DataSize = HeaderAr.TotalSize();
				HeaderBuffer = FIoBuffer(FIoBuffer::AssumeOwnership, HeaderAr.ReleaseOwnership(), DataSize);
			}

			FCbAttachment HeaderAttachment = CreateAttachment(HeaderBuffer);
			Pkg.AddAttachment(HeaderAttachment);
			
			PackageObj.BeginObject();
			PackageObj << "id" << HeaderOid;
			PackageObj << "name" << "ContainerHeader";
			PackageObj << "data" << HeaderAttachment;
			PackageObj.EndObject();
		}

		PackageObj.EndArray();	// End of Meta array
	}
}

void FZenStoreWriter::BroadcastCommit(IPackageStoreWriter::FCommitEventArgs& EventArgs)
{
	FScopeLock CommitEventLock(&CommitEventCriticalSection);
	
	if (CommitEvent.IsBound())
	{
		FReadScopeLock _(EntriesLock);
		EventArgs.Entries = PackageStoreEntries;
		CommitEvent.Broadcast(EventArgs);
	}
}

void FZenStoreWriter::BroadcastMarkUpToDate(IPackageStoreWriter::FMarkUpToDateEventArgs& EventArgs)
{
	FScopeLock CommitEventLock(&CommitEventCriticalSection);

	if (MarkUpToDateEvent.IsBound())
	{
		FReadScopeLock _(EntriesLock);
		EventArgs.PlatformName = TargetPlatformFName;
		EventArgs.Entries = PackageStoreEntries;
		EventArgs.CookInfos = CookedPackagesInfo;
		MarkUpToDateEvent.Broadcast(EventArgs);
	}
}

FCbAttachment FZenStoreWriter::CreateAttachment(FSharedBuffer AttachmentData)
{
	check(AttachmentData.GetSize() > 0);
	FCompressedBuffer CompressedBuffer = FCompressedBuffer::Compress(AttachmentData, Compressor, CompressionLevel);
	check(!CompressedBuffer.IsNull());
	return FCbAttachment(CompressedBuffer);
}

FCbAttachment FZenStoreWriter::CreateAttachment(FIoBuffer AttachmentData)
{
	return CreateAttachment(IoBufferToSharedBuffer(AttachmentData));
}
