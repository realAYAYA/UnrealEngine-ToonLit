// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cooker/LooseCookedPackageWriter.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryState.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Async/ParallelFor.h"
#include "Containers/ContainersFwd.h"
#include "Containers/Set.h"
#include "Containers/StringView.h"
#include "Cooker/AsyncIODelete.h"
#include "Cooker/CompactBinaryTCP.h"
#include "Cooker/CookPackageData.h"
#include "Cooker/CookSandbox.h"
#include "Cooker/CookTypes.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformCrt.h"
#include "HAL/PlatformFile.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformString.h"
#include "Hash/Blake3.h"
#include "IO/IoHash.h"
#include "Interfaces/ITargetPlatform.h"
#include "Logging/LogCategory.h"
#include "Logging/LogMacros.h"
#include "Misc/App.h"
#include "Misc/AssertionMacros.h"
#include "Misc/CString.h"
#include "Misc/EnumClassFlags.h"
#include "Misc/FileHelper.h"
#include "Misc/Optional.h"
#include "Misc/PackageName.h"
#include "Misc/PackagePath.h"
#include "Misc/PathViews.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Misc/SecureHash.h"
#include "Misc/StringBuilder.h"
#include "PackageStoreOptimizer.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Serialization/Archive.h"
#include "Serialization/ArchiveStackTrace.h"
#include "Serialization/ArrayReader.h"
#include "Serialization/CompactBinaryWriter.h"
#include "Serialization/LargeMemoryWriter.h"
#include "Serialization/MemoryWriter.h"
#include "String/Find.h"
#include "Tasks/Task.h"
#include "Templates/RefCounting.h"
#include "Templates/UnrealTemplate.h"
#include "Trace/Detail/Channel.h"
#include "UObject/ObjectVersion.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

LLM_DEFINE_TAG(Cooker_PackageStoreManifest);

FLooseCookedPackageWriter::FLooseCookedPackageWriter(const FString& InOutputPath,
	const FString& InMetadataDirectoryPath, const ITargetPlatform* InTargetPlatform, FAsyncIODelete& InAsyncIODelete,
	UE::Cook::FCookSandbox& InSandboxFile, FBeginCacheCallback&& InBeginCacheCallback)
	: OutputPath(InOutputPath)
	, MetadataDirectoryPath(InMetadataDirectoryPath)
	, TargetPlatform(*InTargetPlatform)
	, SandboxFile(InSandboxFile)
	, AsyncIODelete(InAsyncIODelete)
	, BeginCacheCallback(MoveTemp(InBeginCacheCallback))
{
}

FLooseCookedPackageWriter::~FLooseCookedPackageWriter()
{
}

void FLooseCookedPackageWriter::BeginPackage(const FBeginPackageInfo& Info)
{
	Super::BeginPackage(Info);
	LLM_SCOPE_BYTAG(Cooker_PackageStoreManifest);
	FScopeLock Lock(&OplogLock);
	FOplogPackageInfo& PackageInfo = Oplog.FindOrAdd(Info.PackageName);
	PackageInfo.PackageName = Info.PackageName;
	PackageInfo.PackageDataChunks.Reset();
	PackageInfo.BulkDataChunks.Reset();
}

void FLooseCookedPackageWriter::CommitPackageInternal(FPackageWriterRecords::FPackage&& BaseRecord,
	const FCommitPackageInfo& Info)
{
	FRecord& Record = static_cast<FRecord&>(BaseRecord);
	if (Info.Status == IPackageWriter::ECommitStatus::Success)
	{
		AsyncSave(Record, Info);
	}
	if (Info.Status != IPackageWriter::ECommitStatus::Canceled)
	{
		UpdateManifest(Record);
	}
}

void FLooseCookedPackageWriter::AsyncSave(FRecord& Record, const FCommitPackageInfo& Info)
{
	FCommitContext Context{ Info };

	// The order of these collection calls is important, both for ExportsBuffers (affects the meaning of offsets
	// to those buffers) and for OutputFiles (affects the calculation of the Hash for the set of PackageData)
	// The order of ExportsBuffers must match CompleteExportsArchiveForDiff.
	CollectForSavePackageData(Record, Context);
	CollectForSaveBulkData(Record, Context);
	CollectForSaveLinkerAdditionalDataRecords(Record, Context);
	CollectForSaveAdditionalFileRecords(Record, Context);
	CollectForSaveExportsFooter(Record, Context);
	CollectForSaveExportsPackageTrailer(Record, Context);
	CollectForSaveExportsBuffers(Record, Context);

	AsyncSaveOutputFiles(Record, Context);
}

void FLooseCookedPackageWriter::CompleteExportsArchiveForDiff(FPackageInfo& Info, FLargeMemoryWriter& ExportsArchive)
{
	FPackageWriterRecords::FPackage& BaseRecord = Records.FindRecordChecked(Info.PackageName);
	FRecord& Record = static_cast<FRecord&>(BaseRecord);
	Record.bCompletedExportsArchiveForDiff = true;

	// Add on all the attachments which are usually added on during Commit. The order must match AsyncSave.
	for (FBulkDataRecord& BulkRecord : Record.BulkDatas)
	{
		if (BulkRecord.Info.BulkDataType == FBulkDataInfo::AppendToExports && BulkRecord.Info.MultiOutputIndex == Info.MultiOutputIndex)
		{
			ExportsArchive.Serialize(const_cast<void*>(BulkRecord.Buffer.GetData()),
				BulkRecord.Buffer.GetSize());
		}
	}
	for (FLinkerAdditionalDataRecord& AdditionalRecord : Record.LinkerAdditionalDatas)
	{
		if (AdditionalRecord.Info.MultiOutputIndex == Info.MultiOutputIndex)
		{
			ExportsArchive.Serialize(const_cast<void*>(AdditionalRecord.Buffer.GetData()),
				AdditionalRecord.Buffer.GetSize());
		}
	}

	uint32 FooterData = PACKAGE_FILE_TAG;
	ExportsArchive << FooterData;

	for (FPackageTrailerRecord& PackageTrailer : Record.PackageTrailers)
	{
		if (PackageTrailer.Info.MultiOutputIndex == Info.MultiOutputIndex)
		{
			ExportsArchive.Serialize(const_cast<void*>(PackageTrailer.Buffer.GetData()),
				PackageTrailer.Buffer.GetSize());
		}
	}
}


void FLooseCookedPackageWriter::CollectForSavePackageData(FRecord& Record, FCommitContext& Context)
{
	Context.ExportsBuffers.AddDefaulted(Record.Packages.Num());
	for (FPackageWriterRecords::FWritePackage& Package : Record.Packages)
	{
		Context.ExportsBuffers[Package.Info.MultiOutputIndex].Add(FExportBuffer{ Package.Buffer, MoveTemp(Package.Regions) });
	}
}

void FLooseCookedPackageWriter::CollectForSaveBulkData(FRecord& Record, FCommitContext& Context)
{
	for (FBulkDataRecord& BulkRecord : Record.BulkDatas)
	{
		if (BulkRecord.Info.BulkDataType == FBulkDataInfo::AppendToExports)
		{
			if (Record.bCompletedExportsArchiveForDiff)
			{
				// Already Added in CompleteExportsArchiveForDiff
				continue;
			}
			Context.ExportsBuffers[BulkRecord.Info.MultiOutputIndex].Add(FExportBuffer{ BulkRecord.Buffer, MoveTemp(BulkRecord.Regions) });
		}
		else
		{
			FWriteFileData& OutputFile = Context.OutputFiles.Emplace_GetRef();
			OutputFile.Filename = BulkRecord.Info.LooseFilePath;
			OutputFile.Buffer = FCompositeBuffer(BulkRecord.Buffer);
			OutputFile.Regions = MoveTemp(BulkRecord.Regions);
			OutputFile.bIsSidecar = true;
			OutputFile.bContributeToHash = BulkRecord.Info.MultiOutputIndex == 0; // Only caculate the main package output hash
			OutputFile.ChunkId = BulkRecord.Info.ChunkId;
		}
	}
}
void FLooseCookedPackageWriter::CollectForSaveLinkerAdditionalDataRecords(FRecord& Record, FCommitContext& Context)
{
	if (Record.bCompletedExportsArchiveForDiff)
	{
		// Already Added in CompleteExportsArchiveForDiff
		return;
	}

	for (FLinkerAdditionalDataRecord& AdditionalRecord : Record.LinkerAdditionalDatas)
	{
		Context.ExportsBuffers[AdditionalRecord.Info.MultiOutputIndex].Add(FExportBuffer{ AdditionalRecord.Buffer, MoveTemp(AdditionalRecord.Regions) });
	}
}

void FLooseCookedPackageWriter::CollectForSaveAdditionalFileRecords(FRecord& Record, FCommitContext& Context)
{
	for (FAdditionalFileRecord& AdditionalRecord : Record.AdditionalFiles)
	{
		FWriteFileData& OutputFile = Context.OutputFiles.Emplace_GetRef();
		OutputFile.Filename = AdditionalRecord.Info.Filename;
		OutputFile.Buffer = FCompositeBuffer(AdditionalRecord.Buffer);
		OutputFile.bIsSidecar = true;
		OutputFile.bContributeToHash = AdditionalRecord.Info.MultiOutputIndex == 0; // Only calculate the main package output hash
		OutputFile.ChunkId = AdditionalRecord.Info.ChunkId;
	}
}

void FLooseCookedPackageWriter::CollectForSaveExportsFooter(FRecord& Record, FCommitContext& Context)
{
	if (Record.bCompletedExportsArchiveForDiff)
	{
		// Already Added in CompleteExportsArchiveForDiff
		return;
	}

	uint32 FooterData = PACKAGE_FILE_TAG;
	FSharedBuffer Buffer = FSharedBuffer::Clone(&FooterData, sizeof(FooterData));
	for (FPackageWriterRecords::FWritePackage& Package : Record.Packages)
	{
		Context.ExportsBuffers[Package.Info.MultiOutputIndex].Add(FExportBuffer{ Buffer, TArray<FFileRegion>() });
	}
}

int64 FLooseCookedPackageWriter::GetExportsFooterSize()
{
	return sizeof(uint32);
}

void FLooseCookedPackageWriter::CollectForSaveExportsPackageTrailer(FRecord& Record, FCommitContext& Context)
{
	if (Record.bCompletedExportsArchiveForDiff)
	{
		// Already Added in CompleteExportsArchiveForDiff
		return;
	}

	for (FPackageTrailerRecord& PackageTrailer : Record.PackageTrailers)
	{
		Context.ExportsBuffers[PackageTrailer.Info.MultiOutputIndex].Add(
			FExportBuffer{ PackageTrailer.Buffer, TArray<FFileRegion>() });
	}
}

void FLooseCookedPackageWriter::CollectForSaveExportsBuffers(FRecord& Record, FCommitContext& Context)
{
	check(Context.ExportsBuffers.Num() == Record.Packages.Num());
	for (FPackageWriterRecords::FWritePackage& Package : Record.Packages)
	{
		TArray<FExportBuffer>& ExportsBuffers = Context.ExportsBuffers[Package.Info.MultiOutputIndex];
		check(ExportsBuffers.Num() > 0);

		// Split the ExportsBuffer into (1) Header and (2) Exports + AllAppendedData
		int64 HeaderSize = Package.Info.HeaderSize;
		FExportBuffer& HeaderAndExportsBuffer = ExportsBuffers[0];
		FSharedBuffer& HeaderAndExportsData = HeaderAndExportsBuffer.Buffer;

		// Header (.uasset/.umap)
		{
			FWriteFileData& OutputFile = Context.OutputFiles.Emplace_GetRef();
			OutputFile.Filename = Package.Info.LooseFilePath;
			OutputFile.Buffer = FCompositeBuffer(
				FSharedBuffer::MakeView(HeaderAndExportsData.GetData(), HeaderSize, HeaderAndExportsData));
			OutputFile.bIsSidecar = false;
			OutputFile.bContributeToHash = Package.Info.MultiOutputIndex == 0; // Only calculate the main package output hash
		}

		// Exports + AllAppendedData (.uexp)
		{
			FWriteFileData& OutputFile = Context.OutputFiles.Emplace_GetRef();
			OutputFile.Filename = FPaths::ChangeExtension(Package.Info.LooseFilePath, LexToString(EPackageExtension::Exports));
			OutputFile.bIsSidecar = false;
			OutputFile.bContributeToHash = Package.Info.MultiOutputIndex == 0; // Only caculate the main package output hash

			int32 NumBuffers = ExportsBuffers.Num();
			TArray<FSharedBuffer> BuffersForComposition;
			BuffersForComposition.Reserve(NumBuffers);

			const uint8* ExportsStart = static_cast<const uint8*>(HeaderAndExportsData.GetData()) + HeaderSize;
			BuffersForComposition.Add(FSharedBuffer::MakeView(ExportsStart, HeaderAndExportsData.GetSize() - HeaderSize,
				HeaderAndExportsData));
			OutputFile.Regions.Append(MoveTemp(HeaderAndExportsBuffer.Regions));

			for (FExportBuffer& ExportsBuffer : TArrayView<FExportBuffer>(ExportsBuffers).Slice(1, NumBuffers - 1))
			{
				BuffersForComposition.Add(ExportsBuffer.Buffer);
				OutputFile.Regions.Append(MoveTemp(ExportsBuffer.Regions));
			}
			OutputFile.Buffer = FCompositeBuffer(BuffersForComposition);

			// Adjust regions so they are relative to the start of the uexp file
			for (FFileRegion& Region : OutputFile.Regions)
			{
				Region.Offset -= HeaderSize;
			}
		}
	}
}

FPackageWriterRecords::FPackage* FLooseCookedPackageWriter::ConstructRecord()
{
	return new FRecord();
}

void FLooseCookedPackageWriter::AsyncSaveOutputFiles(FRecord& Record, FCommitContext& Context)
{
	if (!EnumHasAnyFlags(Context.Info.WriteOptions, EWriteOptions::Write | EWriteOptions::ComputeHash))
	{
		return;
	}

	UE::SavePackageUtilities::IncrementOutstandingAsyncWrites();

	TRefCountPtr<FPackageHashes> ThisPackageHashes;
	TUniquePtr<TPromise<int>> PackageHashesCompletionPromise;
	
	if (EnumHasAnyFlags(Context.Info.WriteOptions, EWriteOptions::ComputeHash))
	{
		ThisPackageHashes = new FPackageHashes();
		if (bProvidePerPackageResults)
		{
			PackageHashesCompletionPromise.Reset(new TPromise<int>());
			ThisPackageHashes->CompletionFuture = PackageHashesCompletionPromise->GetFuture();
		}

		bool bAlreadyExisted = false;
		{
			FScopeLock PackageHashesScopeLock(&PackageHashesLock);
			TRefCountPtr<FPackageHashes>& ExistingPackageHashes = AllPackageHashes.FindOrAdd(Context.Info.PackageName);
			// This calculation of bAlreadyExisted looks weird but we're finding the _refcount_, not the hashes. So if it gets
			// constructed, it's not actually assigned a pointer.
			bAlreadyExisted = ExistingPackageHashes.IsValid();
			ExistingPackageHashes = ThisPackageHashes;
		}
		if (bAlreadyExisted)
		{
			UE_LOG(LogSavePackage, Error, TEXT("FLooseCookedPackageWriter encountered the same package twice in a cook! (%s)"), *Context.Info.PackageName.ToString());
		}
	}

	UE::Tasks::Launch(TEXT("HashAndWriteLooseCookedFile"),
		[OutputFiles = MoveTemp(Context.OutputFiles), WriteOptions = Context.Info.WriteOptions,
		ThisPackageHashes = MoveTemp(ThisPackageHashes), PackageHashesCompletionPromise=MoveTemp(PackageHashesCompletionPromise)]
		() mutable
		{
			FMD5 AccumulatedHash;
			for (const FWriteFileData& OutputFile : OutputFiles)
			{
				OutputFile.HashAndWrite(AccumulatedHash, ThisPackageHashes, WriteOptions);
			}

			if (EnumHasAnyFlags(WriteOptions, EWriteOptions::ComputeHash))
			{
				ThisPackageHashes->PackageHash.Set(AccumulatedHash);
			}

			if (PackageHashesCompletionPromise)
			{
				// Note that setting this Promise might call arbitrary code from anything that subscribed
				// to ThisPackageHashes->CompletionFuture.Then(). So don't call it inside a lock.
				PackageHashesCompletionPromise->SetValue(0);
			}

			// This is used to release the game thread to access the hashes
			UE::SavePackageUtilities::DecrementOutstandingAsyncWrites();
		},
		UE::Tasks::ETaskPriority::BackgroundNormal
	);
}

static void WriteToFile(const FString& Filename, const FCompositeBuffer& Buffer)
{
	IFileManager& FileManager = IFileManager::Get();

	struct FFailureReason
	{
		uint32 LastErrorCode = 0;
		bool bSizeMatchFailed = false;
		int64 ExpectedSize = 0;
		int64 ActualSize = 0;
		bool bArchiveError = false;
	};
	TOptional<FFailureReason> FailureReason;

	for (int32 Tries = 0; Tries < 3; ++Tries)
	{
		FArchive* Ar = FileManager.CreateFileWriter(*Filename);
		if (!Ar)
		{
			if (!FailureReason)
			{
				FailureReason = FFailureReason{ FPlatformMisc::GetLastError(), false };
			}
			continue;
		}

		int64 DataSize = 0;
		for (const FSharedBuffer& Segment : Buffer.GetSegments())
		{
			int64 SegmentSize = static_cast<int64>(Segment.GetSize());
			Ar->Serialize(const_cast<void*>(Segment.GetData()), SegmentSize);
			DataSize += SegmentSize;
		}
		bool bArchiveError = Ar->IsError();
		delete Ar;

		int64 ActualSize = FileManager.FileSize(*Filename);
		if (ActualSize != DataSize)
		{
			if (!FailureReason)
			{
				FailureReason = FFailureReason{ 0, true, DataSize, ActualSize, bArchiveError };
			}
			FileManager.Delete(*Filename);
			continue;
		}
		return;
	}

	FString ReasonText;
	if (FailureReason && FailureReason->bSizeMatchFailed)
	{
		ReasonText = FString::Printf(TEXT("Unexpected file size. Tried to write %" INT64_FMT " but resultant size was %" INT64_FMT ".%s")
			TEXT(" Another operation is modifying the file, or the write operation failed to write completely."),
			FailureReason->ExpectedSize, FailureReason->ActualSize, FailureReason->bArchiveError ? TEXT(" Ar->Serialize failed.") : TEXT(""));
	}
	else if (FailureReason && FailureReason->LastErrorCode != 0)
	{
		TCHAR LastErrorText[1024];
		FPlatformMisc::GetSystemErrorMessage(LastErrorText, UE_ARRAY_COUNT(LastErrorText), FailureReason->LastErrorCode);
		ReasonText = LastErrorText;
	}
	else
	{
		ReasonText = TEXT("Unknown failure reason.");
	}
	UE_LOG(LogSavePackage, Fatal, TEXT("SavePackage Async write %s failed: %s"), *Filename, *ReasonText);
}

void FLooseCookedPackageWriter::FWriteFileData::HashAndWrite(FMD5& AccumulatedHash, const TRefCountPtr<FPackageHashes>& PackageHashes, EWriteOptions WriteOptions) const
{
	//@todo: FH: Should we calculate the hash of both output, currently only the main package output hash is calculated
	if (EnumHasAnyFlags(WriteOptions, EWriteOptions::ComputeHash) && bContributeToHash)
	{
		for (const FSharedBuffer& Segment : Buffer.GetSegments())
		{
			AccumulatedHash.Update(static_cast<const uint8*>(Segment.GetData()), Segment.GetSize());
		}

		if (ChunkId.IsValid())
		{
			FBlake3 ChunkHash;
			for (const FSharedBuffer& Segment : Buffer.GetSegments())
			{
				ChunkHash.Update(static_cast<const uint8*>(Segment.GetData()), Segment.GetSize());
			}
			FIoHash FinalHash(ChunkHash.Finalize());
			PackageHashes->ChunkHashes.Add(ChunkId, FinalHash);
		}
	}

	if ((bIsSidecar && EnumHasAnyFlags(WriteOptions, EWriteOptions::WriteSidecars)) ||
		(!bIsSidecar && EnumHasAnyFlags(WriteOptions, EWriteOptions::WritePackage)))
	{
		const FString* WriteFilename = &Filename;
		FString FilenameBuffer;
		if (EnumHasAnyFlags(WriteOptions, EWriteOptions::SaveForDiff))
		{
			FilenameBuffer = FPaths::Combine(FPaths::GetPath(Filename),
				FPaths::GetBaseFilename(Filename) + TEXT("_ForDiff") + FPaths::GetExtension(Filename, true));
			WriteFilename = &FilenameBuffer;
		}
		WriteToFile(*WriteFilename, Buffer);

		if (Regions.Num() > 0)
		{
			TArray<uint8> Memory;
			FMemoryWriter Ar(Memory);
			FFileRegion::SerializeFileRegions(Ar, const_cast<TArray<FFileRegion>&>(Regions));

			WriteToFile(*WriteFilename + FFileRegion::RegionsFileExtension,
				FCompositeBuffer(FSharedBuffer::MakeView(Memory.GetData(), Memory.Num())));
		}
	}
}

void FLooseCookedPackageWriter::UpdateManifest(FRecord& Record)
{
	LLM_SCOPE_BYTAG(Cooker_PackageStoreManifest);
	FScopeLock Lock(&OplogLock);
	for (const FPackageWriterRecords::FWritePackage& Package : Record.Packages)
	{
		FOplogPackageInfo* PackageInfo = Oplog.Find(Package.Info.PackageName);
		check(PackageInfo);
		FOplogChunkInfo& ChunkInfo = PackageInfo->PackageDataChunks.AddDefaulted_GetRef();
		ChunkInfo.ChunkId = Package.Info.ChunkId;
		FStringView RelativePathView;
		FPathViews::TryMakeChildPathRelativeTo(Package.Info.LooseFilePath, OutputPath, RelativePathView);
		ChunkInfo.RelativeFileName = RelativePathView;

	}
	for (const FPackageWriterRecords::FBulkData& BulkData : Record.BulkDatas)
	{
		FOplogPackageInfo* PackageInfo = Oplog.Find(BulkData.Info.PackageName);
		check(PackageInfo);
		FOplogChunkInfo& ChunkInfo = PackageInfo->BulkDataChunks.AddDefaulted_GetRef();
		ChunkInfo.ChunkId = BulkData.Info.ChunkId;
		FStringView RelativePathView;
		FPathViews::TryMakeChildPathRelativeTo(BulkData.Info.LooseFilePath, OutputPath, RelativePathView);
		ChunkInfo.RelativeFileName = RelativePathView;
	}
}

bool FLooseCookedPackageWriter::GetPreviousCookedBytes(const FPackageInfo& Info, FPreviousCookedBytesData& OutData)
{
	UE::ArchiveStackTrace::FPackageData ExistingPackageData;
	TUniquePtr<uint8, UE::ArchiveStackTrace::FDeleteByFree> Bytes;
	UE::ArchiveStackTrace::LoadPackageIntoMemory(*Info.LooseFilePath, ExistingPackageData, Bytes);
	OutData.Size = ExistingPackageData.Size;
	OutData.HeaderSize = ExistingPackageData.HeaderSize;
	OutData.StartOffset = ExistingPackageData.StartOffset;
	OutData.Data.Reset(Bytes.Release());
	return OutData.Data.IsValid();
}

FDateTime FLooseCookedPackageWriter::GetPreviousCookTime() const
{
	const FString PreviousAssetRegistry = FPaths::Combine(MetadataDirectoryPath, GetDevelopmentAssetRegistryFilename());
	return IFileManager::Get().GetTimeStamp(*PreviousAssetRegistry);
}

void FLooseCookedPackageWriter::Initialize(const FCookInfo& Info)
{
	bIterateSharedBuild = Info.bIterateSharedBuild;
	if (Info.bFullBuild && !Info.bWorkerOnSharedSandbox)
	{
		DeleteSandboxDirectory();
	}
	if (!Info.bWorkerOnSharedSandbox)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(SaveScriptObjects);
		FPackageStoreOptimizer PackageStoreOptimizer;
		PackageStoreOptimizer.Initialize();
		FIoBuffer ScriptObjectsBuffer = PackageStoreOptimizer.CreateScriptObjectsBuffer();
		FFileHelper::SaveArrayToFile(
			MakeArrayView(ScriptObjectsBuffer.Data(), ScriptObjectsBuffer.DataSize()),
			*(MetadataDirectoryPath / TEXT("scriptobjects.bin")));
	}
}

EPackageWriterResult FLooseCookedPackageWriter::BeginCacheForCookedPlatformData(
	FBeginCacheForCookedPlatformDataInfo& Info)
{
	return BeginCacheCallback(Info);
}

void FLooseCookedPackageWriter::WriteOplogEntry(FCbWriter& Writer, const FOplogPackageInfo& PackageInfo)
{
	Writer.BeginObject();

	Writer.BeginObject("packagestoreentry");
	Writer << "packagename" << PackageInfo.PackageName;
	Writer.EndObject();

	Writer.BeginArray("packagedata");
	for (const FOplogChunkInfo& ChunkInfo : PackageInfo.PackageDataChunks)
	{
		Writer.BeginObject();
		Writer << "id" << ChunkInfo.ChunkId;
		Writer << "filename" << ChunkInfo.RelativeFileName;
		Writer.EndObject();
	}
	Writer.EndArray();

	Writer.BeginArray("bulkdata");
	for (const FOplogChunkInfo& ChunkInfo : PackageInfo.BulkDataChunks)
	{
		Writer.BeginObject();
		Writer << "id" << ChunkInfo.ChunkId;
		Writer << "filename" << ChunkInfo.RelativeFileName;
		Writer.EndObject();
	}
	Writer.EndArray();

	Writer.EndObject();
}

bool FLooseCookedPackageWriter::ReadOplogEntry(FOplogPackageInfo& PackageInfo, const FCbFieldView& Field)
{
	FCbObjectView PackageStoreEntryObjectView = Field["packagestoreentry"].AsObjectView();
	if (!PackageStoreEntryObjectView)
	{
		return false;
	}
	PackageInfo.PackageName = FName(PackageStoreEntryObjectView["packagename"].AsString());

	FCbArrayView PackageDataView = Field["packagedata"].AsArrayView();
	PackageInfo.PackageDataChunks.Reset();
	PackageInfo.PackageDataChunks.Reserve(PackageDataView.Num());
	for (const FCbFieldView& ChunkEntry : PackageDataView)
	{
		FOplogChunkInfo& ChunkInfo = PackageInfo.PackageDataChunks.AddDefaulted_GetRef();
		ChunkInfo.ChunkId.Set(ChunkEntry["id"].AsObjectId().GetView());
		ChunkInfo.RelativeFileName = FString(ChunkEntry["filename"].AsString());
	}

	FCbArrayView BulkDataView = Field["bulkdata"].AsArrayView();
	PackageInfo.BulkDataChunks.Reset();
	PackageInfo.BulkDataChunks.Reserve(BulkDataView.Num());
	for (const FCbFieldView& ChunkEntry : BulkDataView)
	{
		FOplogChunkInfo& ChunkInfo = PackageInfo.BulkDataChunks.AddDefaulted_GetRef();
		ChunkInfo.ChunkId.Set(ChunkEntry["id"].AsObjectId().GetView());
		ChunkInfo.RelativeFileName = FString(ChunkEntry["filename"].AsString());
	}

	return true;
}

void FLooseCookedPackageWriter::BeginCook(const FCookInfo& Info)
{
	if (!Info.bWorkerOnSharedSandbox)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(LoadPackageStoreManifest);
		FString PackageStoreManifestFilePath = FPaths::Combine(MetadataDirectoryPath, TEXT("packagestore.manifest"));
		TUniquePtr<FArchive> Ar(IFileManager::Get().CreateFileReader(*PackageStoreManifestFilePath));
		if (Ar)
		{
			FCbField ManifestField = LoadCompactBinary(*Ar);
			FCbField OplogField = ManifestField["oplog"];
			if (OplogField)
			{
				FCbArray EntriesArray = OplogField["entries"].AsArray();
				FScopeLock Lock(&OplogLock);
				Oplog.Reserve(EntriesArray.Num());
				for (const FCbField& EntryField : EntriesArray)
				{
					FOplogPackageInfo PackageInfo;
					ReadOplogEntry(PackageInfo, EntryField);
					Oplog.Add(PackageInfo.PackageName, MoveTemp(PackageInfo));
				}
			}
		}
	}
	else
	{
		bProvidePerPackageResults = true;
	}
	AllPackageHashes.Empty();
}

void FLooseCookedPackageWriter::EndCook(const FCookInfo& Info)
{
	if (!Info.bWorkerOnSharedSandbox)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(SavePackageStoreManifest);
		FScopeLock Lock(&OplogLock);
		// Sort packages for determinism
		TArray<const FOplogPackageInfo*> SortedPackages;
		SortedPackages.Reserve(Oplog.Num());
		for (const auto& KV : Oplog)
		{
			SortedPackages.Add(&KV.Value);
		}
		Algo::Sort(SortedPackages, [](const FOplogPackageInfo* A, const FOplogPackageInfo* B)
			{
				return A->PackageName.LexicalLess(B->PackageName);
			});
		FCbWriter ManifestWriter;
		ManifestWriter.BeginObject();
		ManifestWriter.BeginObject("oplog");
		ManifestWriter.BeginArray("entries");
		for (const FOplogPackageInfo* Package : SortedPackages)
		{
			WriteOplogEntry(ManifestWriter, *Package);
		}
		ManifestWriter.EndArray();
		ManifestWriter.EndObject();
		ManifestWriter.EndObject();

		FString PackageStoreManifestFilePath = FPaths::Combine(MetadataDirectoryPath, TEXT("packagestore.manifest"));
		TUniquePtr<FArchive> Ar(IFileManager::Get().CreateFileWriter(*PackageStoreManifestFilePath));
		if (Ar)
		{
			SaveCompactBinary(*Ar, ManifestWriter.Save());
		}
		else
		{
			UE_LOG(LogSavePackage, Error, TEXT("Failed saving package store manifest file '%s'"), *PackageStoreManifestFilePath);
		}
	}
}

TUniquePtr<FAssetRegistryState> FLooseCookedPackageWriter::LoadPreviousAssetRegistry()
{
	// Report files from the shared build if the option is set
	FString PreviousAssetRegistryFile;
	if (bIterateSharedBuild)
	{
		// clean the sandbox
		DeleteSandboxDirectory();
		PreviousAssetRegistryFile = FPaths::Combine(*FPaths::ProjectSavedDir(), TEXT("SharedIterativeBuild"),
			*TargetPlatform.PlatformName(), TEXT("Metadata"), GetDevelopmentAssetRegistryFilename());
	}
	else
	{
		PreviousAssetRegistryFile = FPaths::Combine(MetadataDirectoryPath, GetDevelopmentAssetRegistryFilename());
	}

	PackageNameToCookedFiles.Reset();

	FArrayReader SerializedAssetData;
	if (!IFileManager::Get().FileExists(*PreviousAssetRegistryFile) || !FFileHelper::LoadFileToArray(SerializedAssetData, *PreviousAssetRegistryFile))
	{ 
		RemoveCookedPackages();
		return TUniquePtr<FAssetRegistryState>();
	}

	TUniquePtr<FAssetRegistryState> PreviousState = MakeUnique<FAssetRegistryState>();
	PreviousState->Load(SerializedAssetData);

	// If we are iterating from a shared build the cooked files do not exist in the local cooked directory;
	// we assume they are packaged in the pak file (which we don't want to extract to confirm) and keep them all.
	if (!bIterateSharedBuild)
	{
		// For regular iteration, remove each SuccessfulSave cooked file from the PreviousState if it no longer exists
		// in the cooked directory. Keep the FailedSave previous cook packages; we don't expect them to exist on disk.
		// Also, remove from the registry and from disk the cooked files that no longer exist in the editor.
		GetAllCookedFiles();
		TSet<FName> RemoveFromRegistry;
		TSet<FName> RemoveFromDisk;
		RemoveFromDisk.Reserve(PackageNameToCookedFiles.Num());
		for (TPair<FName, TArray<FString>>& Pair : PackageNameToCookedFiles)
		{
			RemoveFromDisk.Add(Pair.Key);
		}
		IAssetRegistry& AssetRegistry = IAssetRegistry::GetChecked();
		for (const TPair<FName, const FAssetPackageData*>& Pair : PreviousState->GetAssetPackageDataMap())
		{
			FName PackageName = Pair.Key;
			bool bCurrentPackageExists = AssetRegistry.DoesPackageExistOnDisk(PackageName);

			bool bNoLongerExistsInEditor = false;
			bool bIsScriptPackage = FPackageName::IsScriptPackage(WriteToString<256>(PackageName));
			if (!bCurrentPackageExists)
			{
				// Script and generated packages do not exist uncooked
				// Check that the package is not an exception before removing from cooked
				bool bIsCookedOnly = bIsScriptPackage;
				if (!bIsCookedOnly)
				{
					for (const FAssetData* AssetData : PreviousState->GetAssetsByPackageName(PackageName))
					{
						bIsCookedOnly |= !!(AssetData->PackageFlags & PKG_CookGenerated);
					}
				}
				bNoLongerExistsInEditor = !bIsCookedOnly;
			}
			if (bNoLongerExistsInEditor)
			{
				// Remove package from both disk and registry
				// Keep its RemoveFromDisk entry
				RemoveFromRegistry.Add(PackageName); // Add a RemoveFromRegistry entry
			}
			else
			{
				// Keep package on disk if it exists. Keep package in registry if it exists on disk or was a FailedSave.
				bool bExistsOnDisk = (RemoveFromDisk.Remove(PackageName) == 1); // Remove its RemoveFromDisk entry
				const FAssetPackageData* PackageData = Pair.Value;
				if (!bExistsOnDisk && PackageData->DiskSize >= 0 && !bIsScriptPackage)
				{
					// Add RemoveFromRegistry entry if it didn't exist on disk and is a SuccessfulSave package
					RemoveFromRegistry.Add(PackageName);
				}
			}
		}

		if (RemoveFromRegistry.Num())
		{
			PreviousState->PruneAssetData(TSet<FName>(), RemoveFromRegistry, FAssetRegistrySerializationOptions());
		}
		if (RemoveFromDisk.Num())
		{
			RemoveCookedPackagesByPackageName(RemoveFromDisk.Array(), true /* bRemoveRecords */);
		}
	}

	return PreviousState;
}

FCbObject FLooseCookedPackageWriter::GetOplogAttachment(FName PackageName, FUtf8StringView AttachmentKey)
{
	/** Oplog attachments are not implemented by FLooseCookedPackageWriter */
	return FCbObject();
}

void FLooseCookedPackageWriter::RemoveCookedPackages(TArrayView<const FName> PackageNamesToRemove)
{
	if (PackageNameToCookedFiles.IsEmpty())
	{
		FindAndDeleteCookedFilesForPackages(PackageNamesToRemove);
		return;
	}

	// if we are going to clear the cooked packages it is conceivable that we will recook the packages which we just cooked 
	// that means it's also conceivable that we will recook the same package which currently has an outstanding async write request
	UPackage::WaitForAsyncFileWrites();
	RemoveCookedPackagesByPackageName(PackageNamesToRemove, false /* bRemoveRecords */);
	// PackageNameToCookedFiles is no longer used after the RemoveCookedPackages call at the beginning of the cook.
	PackageNameToCookedFiles.Empty();
}

void FLooseCookedPackageWriter::RemoveCookedPackagesByPackageName(TArrayView<const FName> PackageNamesToRemove, bool bRemoveRecords)
{
	auto DeletePackageLambda = [&PackageNamesToRemove, this](int32 PackageIndex)
	{
		FName PackageName = PackageNamesToRemove[PackageIndex];
		TArray<FString>* CookedFileNames = PackageNameToCookedFiles.Find(PackageName);
		if (CookedFileNames)
		{
			for (const FString& CookedFileName : *CookedFileNames)
			{
				IFileManager::Get().Delete(*CookedFileName, true, true, true);
			}
		}
	};
	ParallelFor(PackageNamesToRemove.Num(), DeletePackageLambda);

	if (bRemoveRecords)
	{
		for (FName PackageName : PackageNamesToRemove)
		{
			PackageNameToCookedFiles.Remove(PackageName);
		}
	}
}

void FLooseCookedPackageWriter::UpdatePackageModificationStatus(FName PackageName,
	bool bIterativelyUnmodified, bool& bInOutShouldIterativelySkip)
{
}

TFuture<FCbObject> FLooseCookedPackageWriter::WriteMPCookMessageForPackage(FName PackageName)
{
	FCbFieldIterator OplogEntryField;
	{
		FOplogPackageInfo PackageInfo;
		bool bValid = false;
		{
			FScopeLock Lock(&OplogLock);
			bValid = Oplog.RemoveAndCopyValue(PackageName, PackageInfo);
		}
		if (bValid)
		{
			check(PackageName == PackageInfo.PackageName);
			FCbWriter OplogEntryWriter;
			WriteOplogEntry(OplogEntryWriter, PackageInfo);
			OplogEntryField = OplogEntryWriter.Save();
		}
	}

	TRefCountPtr<FPackageHashes> PackageHashes;
	{
		FScopeLock PackageHashesScopeLock(&PackageHashesLock);
		AllPackageHashes.RemoveAndCopyValue(PackageName, PackageHashes);
	}

	auto ComposeMessage = [OplogEntryField = MoveTemp(OplogEntryField)](FPackageHashes* PackageHashes)
	{
		FCbWriter Writer;
		Writer.BeginObject();
		if (OplogEntryField.HasValue())
		{
			Writer << "OplogEntry" << OplogEntryField;
		}
		if (PackageHashes)
		{
			Writer << "PackageHash" << PackageHashes->PackageHash;
			Writer << "ChunkHashes" << PackageHashes->ChunkHashes;
		}
		Writer.EndObject();
		return Writer.Save().AsObject();
	};

	if (PackageHashes && PackageHashes->CompletionFuture.IsValid())
	{
		TUniquePtr<TPromise<FCbObject>> Promise(new TPromise<FCbObject>());
		TFuture<FCbObject> ResultFuture = Promise->GetFuture();
		PackageHashes->CompletionFuture.Next(
			[PackageHashes, Promise = MoveTemp(Promise), ComposeMessage = MoveTemp(ComposeMessage)]
			(int)
			{
				Promise->SetValue(ComposeMessage(PackageHashes.GetReference()));
			});
		return ResultFuture;
	}
	else
	{
		TPromise<FCbObject> Promise;
		Promise.SetValue(ComposeMessage(PackageHashes.GetReference()));
		return Promise.GetFuture();
	}
}

bool FLooseCookedPackageWriter::TryReadMPCookMessageForPackage(FName PackageName, FCbObjectView Message)
{
	FOplogPackageInfo PackageInfo;
	FCbFieldView OplogEntryField(Message["OplogEntry"]);
	if (ReadOplogEntry(PackageInfo, OplogEntryField))
	{
		check(PackageName == PackageInfo.PackageName);
		FScopeLock Lock(&OplogLock);
		Oplog.Add(PackageName, MoveTemp(PackageInfo));
	}

	bool bOk = true;
	TRefCountPtr<FPackageHashes> ThisPackageHashes(new FPackageHashes());
	if (LoadFromCompactBinary(Message["PackageHash"], ThisPackageHashes->PackageHash))
	{
		bOk = LoadFromCompactBinary(Message["ChunkHashes"], ThisPackageHashes->ChunkHashes) & bOk;
		if (bOk)
		{
			bool bAlreadyExisted = false;
			{
				FScopeLock PackageHashesScopeLock(&PackageHashesLock);
				TRefCountPtr<FPackageHashes>& ExistingPackageHashes = AllPackageHashes.FindOrAdd(PackageName);
				bAlreadyExisted = ExistingPackageHashes.IsValid();
				ExistingPackageHashes = ThisPackageHashes;
			}
			if (bAlreadyExisted)
			{
				UE_LOG(LogSavePackage, Error, TEXT("FLooseCookedPackageWriter encountered the same package twice in a cook! (%s)"),
					*PackageName.ToString());
			}
		}
	}

	return bOk;
}

void FLooseCookedPackageWriter::RemoveCookedPackages()
{
	DeleteSandboxDirectory();
}

void FLooseCookedPackageWriter::DeleteSandboxDirectory()
{
	// if we are going to clear the cooked packages it is conceivable that we will recook the packages which we just cooked 
	// that means it's also conceivable that we will recook the same package which currently has an outstanding async write request
	UPackage::WaitForAsyncFileWrites();

	FString SandboxDirectory = OutputPath;
	FPaths::NormalizeDirectoryName(SandboxDirectory);

	AsyncIODelete.DeleteDirectory(SandboxDirectory);
}

class FPackageSearchVisitor : public IPlatformFile::FDirectoryVisitor
{
	TArray<FString>& FoundFiles;
public:
	FPackageSearchVisitor(TArray<FString>& InFoundFiles)
		: FoundFiles(InFoundFiles)
	{}
	virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory)
	{
		if (bIsDirectory == false)
		{
			FString Filename(FilenameOrDirectory);
			FStringView Extension(FPathViews::GetExtension(Filename, true /* bIncludeDot */));
			if (!Extension.IsEmpty())
			{
				EPackageExtension ExtensionEnum = FPackagePath::ParseExtension(Extension);
				if (ExtensionEnum != EPackageExtension::Unspecified && ExtensionEnum != EPackageExtension::Custom)
				{
					FoundFiles.Add(Filename);
				}
			}
		}
		return true;
	}
};

void FLooseCookedPackageWriter::GetAllCookedFiles()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FLooseCookedPackageWriter::GetAllCookedFiles);

	const FString& SandboxRootDir = OutputPath;
	TArray<FString> CookedFiles;
	{
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		FPackageSearchVisitor PackageSearch(CookedFiles);
		PlatformFile.IterateDirectoryRecursively(*SandboxRootDir, PackageSearch);
	}

	const FString SandboxProjectDir = FPaths::Combine(OutputPath, FApp::GetProjectName()) + TEXT("/");

	UE::Cook::FCookSandboxConvertCookedPathToPackageNameContext Context;
	Context.SandboxRootDir = SandboxRootDir;
	Context.SandboxProjectDir = SandboxProjectDir;
	SandboxFile.FillContext(Context);

	for (const FString& CookedFile : CookedFiles)
	{
		const FName PackageName = SandboxFile.ConvertCookedPathToPackageName(CookedFile, Context);
		if (!PackageName.IsNone())
		{
			PackageNameToCookedFiles.FindOrAdd(PackageName).Add(CookedFile);
		}
	}
}

void FLooseCookedPackageWriter::FindAndDeleteCookedFilesForPackages(TConstArrayView<FName> PackageNames)
{
	const FString& SandboxRootDir = OutputPath;
	const FString SandboxProjectDir = FPaths::Combine(OutputPath, FApp::GetProjectName()) + TEXT("/");

	UE::Cook::FCookSandboxConvertCookedPathToPackageNameContext Context;
	Context.SandboxRootDir = SandboxRootDir;
	Context.SandboxProjectDir = SandboxProjectDir;
	SandboxFile.FillContext(Context);

	for (FName PackageName : PackageNames)
	{
		FString CookedFileName = SandboxFile.ConvertPackageNameToCookedPath(WriteToString<256>(PackageName), Context);
		if (CookedFileName.IsEmpty())
		{
			continue;
		}
		FStringView ParentDir;
		FStringView BaseName;
		FStringView Extension;
		FPathViews::Split(CookedFileName, ParentDir, BaseName, Extension);

		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		TArray<FString, TInlineAllocator<3>> FilesToRemove;
		PlatformFile.IterateDirectory(*WriteToString<1024>(ParentDir),
			[BaseName, ParentDir, &FilesToRemove](const TCHAR* FoundFullPath, bool bDirectory)
			{
				FStringView FoundParentDir;
				FStringView FoundBaseName;
				FStringView FoundExtension;
				FPathViews::Split(FoundFullPath, FoundParentDir, FoundBaseName, FoundExtension);
				if (FoundBaseName == BaseName)
				{
					if (FoundParentDir.IsEmpty())
					{
						FilesToRemove.Add(FPaths::ConvertRelativePathToFull(FString(ParentDir), FString(FoundFullPath)));
					}
					else
					{
						FilesToRemove.Add(FString(FoundFullPath));
					}
				}
				return true;
			});
		for (const FString& FileName : FilesToRemove)
		{
			PlatformFile.DeleteFile(*FileName);
		}
	}
}

EPackageExtension FLooseCookedPackageWriter::BulkDataTypeToExtension(FBulkDataInfo::EType BulkDataType)
{
	switch (BulkDataType)
	{
	case FBulkDataInfo::AppendToExports:
		return EPackageExtension::Exports;
	case FBulkDataInfo::BulkSegment:
		return EPackageExtension::BulkDataDefault;
	case FBulkDataInfo::Mmap:
		return EPackageExtension::BulkDataMemoryMapped;
	case FBulkDataInfo::Optional:
		return EPackageExtension::BulkDataOptional;
	default:
		checkNoEntry();
		return EPackageExtension::Unspecified;
	}
}