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
#include "Cooker/CookPackageData.h"
#include "Cooker/CookTypes.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformCrt.h"
#include "HAL/PlatformFile.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformString.h"
#include "Hash/Blake3.h"
#include "IO/IoHash.h"
#include "Interfaces/IPluginManager.h"
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
#include "Serialization/LargeMemoryWriter.h"
#include "Serialization/MemoryWriter.h"
#include "Tasks/Task.h"
#include "Templates/RefCounting.h"
#include "Templates/UnrealTemplate.h"
#include "Trace/Detail/Channel.h"
#include "UObject/ObjectVersion.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

FLooseCookedPackageWriter::FLooseCookedPackageWriter(const FString& InOutputPath,
	const FString& InMetadataDirectoryPath, const ITargetPlatform* InTargetPlatform, FAsyncIODelete& InAsyncIODelete,
	UE::Cook::FPackageDatas& InPackageDatas, const TArray<TSharedRef<IPlugin>>& InPluginsToRemap)
	: OutputPath(InOutputPath)
	, MetadataDirectoryPath(InMetadataDirectoryPath)
	, TargetPlatform(*InTargetPlatform)
	, PackageDatas(InPackageDatas)
	, PackageStoreManifest(InOutputPath)
	, PluginsToRemap(InPluginsToRemap)
	, AsyncIODelete(InAsyncIODelete)
	, bIterateSharedBuild(false)
{
}

FLooseCookedPackageWriter::~FLooseCookedPackageWriter()
{
}

void FLooseCookedPackageWriter::BeginPackage(const FBeginPackageInfo& Info)
{
	Super::BeginPackage(Info);
	PackageStoreManifest.BeginPackage(Info.PackageName);
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
	CollectForSaveExportsBuffers(Record, Context);

	AsyncSaveOutputFiles(Record, Context);
}

void FLooseCookedPackageWriter::CompleteExportsArchiveForDiff(const FPackageInfo& Info, FLargeMemoryWriter& ExportsArchive)
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

void FLooseCookedPackageWriter::AddToExportsSize(int64& ExportsSize)
{
	ExportsSize += sizeof(uint32); // Footer size
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
	
	if (EnumHasAnyFlags(Context.Info.WriteOptions, EWriteOptions::ComputeHash))
	{
		ThisPackageHashes = new FPackageHashes();

		bool bAlreadyExisted = false;
		{
			FScopeLock ConcurrentSaveScopeLock(&ConcurrentSaveLock);
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

	UE::Tasks::Launch(TEXT("HashAndWriteLooseCookedFile"), [OutputFiles = MoveTemp(Context.OutputFiles), WriteOptions = Context.Info.WriteOptions, ThisPackageHashes = MoveTemp(ThisPackageHashes)]()
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
	for (const FPackageWriterRecords::FWritePackage& Package : Record.Packages)
	{
		PackageStoreManifest.AddPackageData(Package.Info.PackageName, Package.Info.LooseFilePath, Package.Info.ChunkId);
	}
	for (const FPackageWriterRecords::FBulkData& BulkData : Record.BulkDatas)
	{
		PackageStoreManifest.AddBulkData(BulkData.Info.PackageName, BulkData.Info.LooseFilePath, BulkData.Info.ChunkId);
	}
}

bool FLooseCookedPackageWriter::GetPreviousCookedBytes(const FPackageInfo& Info, FPreviousCookedBytesData& OutData)
{
	FArchiveStackTrace::FPackageData ExistingPackageData;
	FArchiveStackTrace::LoadPackageIntoMemory(*Info.LooseFilePath, ExistingPackageData, OutData.Data);
	OutData.Size = ExistingPackageData.Size;
	OutData.HeaderSize = ExistingPackageData.HeaderSize;
	OutData.StartOffset = ExistingPackageData.StartOffset;
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

void FLooseCookedPackageWriter::BeginCook()
{
	PackageStoreManifest.Load(*(MetadataDirectoryPath / TEXT("packagestore.manifest")));
	AllPackageHashes.Empty();
}

void FLooseCookedPackageWriter::EndCook()
{
	PackageStoreManifest.Save(*(MetadataDirectoryPath / TEXT("packagestore.manifest")));
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

	UncookedPathToCookedPath.Reset();

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
		RemoveFromDisk.Reserve(UncookedPathToCookedPath.Num());
		for (TPair<FName, FName>& Pair : UncookedPathToCookedPath)
		{
			RemoveFromDisk.Add(Pair.Key);
		}
		for (const TPair<FName, const FAssetPackageData*>& Pair : PreviousState->GetAssetPackageDataMap())
		{
			FName PackageName = Pair.Key;
			FName UncookedFilename = PackageDatas.GetFileNameByPackageName(PackageName);

			bool bNoLongerExistsInEditor = false;
			bool bIsScriptPackage = FPackageName::IsScriptPackage(WriteToString<256>(PackageName));
			if (UncookedFilename.IsNone())
			{
				// Script and generated packages do not exist uncooked
				// Check that the package is not an exception before removing from cooked
				bool bIsCookedOnly = bIsScriptPackage;
				bool bIsMap = false;
				if (!bIsCookedOnly)
				{
					for (const FAssetData* AssetData : PreviousState->GetAssetsByPackageName(PackageName))
					{
						bIsCookedOnly |= !!(AssetData->PackageFlags & PKG_CookGenerated);
						bIsMap |= !!(AssetData->PackageFlags & PKG_ContainsMap);
					}
				}
				bNoLongerExistsInEditor = !bIsCookedOnly;
				UncookedFilename = UE::Cook::FPackageDatas::LookupFileNameOnDisk(PackageName,
					false /* bRequireExists */, bIsMap);

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
				bool bExistsOnDisk = (RemoveFromDisk.Remove(UncookedFilename) == 1); // Remove its RemoveFromDisk entry
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
			RemoveCookedPackagesByUncookedFilename(RemoveFromDisk.Array());
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
	if (UncookedPathToCookedPath.IsEmpty())
	{
		return;
	}

	if (PackageNamesToRemove.Num() > 0)
	{
		// if we are going to clear the cooked packages it is conceivable that we will recook the packages which we just cooked 
		// that means it's also conceivable that we will recook the same package which currently has an outstanding async write request
		UPackage::WaitForAsyncFileWrites();

		auto DeletePackageLambda = [&PackageNamesToRemove, this](int32 PackageIndex)
		{
			const FName PackageName = PackageNamesToRemove[PackageIndex];
			const FName UncookedFileName = PackageDatas.GetFileNameByPackageName(PackageName);
			if (UncookedFileName.IsNone())
			{
				return;
			}
			FName* CookedFileName = UncookedPathToCookedPath.Find(UncookedFileName);
			if (CookedFileName)
			{
				TStringBuilder<256> FilePath;
				CookedFileName->ToString(FilePath);
				IFileManager::Get().Delete(*FilePath, true, true, true);
			}
		};
		ParallelFor(PackageNamesToRemove.Num(), DeletePackageLambda);
	}

	// We no longer have a use for UncookedPathToCookedPath, after the RemoveCookedPackages call at the beginning of the cook.
	UncookedPathToCookedPath.Empty();
}

void FLooseCookedPackageWriter::RemoveCookedPackagesByUncookedFilename(const TArray<FName>& UncookedFileNamesToRemove)
{
	auto DeletePackageLambda = [&UncookedFileNamesToRemove, this](int32 PackageIndex)
	{
		FName UncookedFileName = UncookedFileNamesToRemove[PackageIndex];
		FName* CookedFileName = UncookedPathToCookedPath.Find(UncookedFileName);
		if (CookedFileName)
		{
			TStringBuilder<256> FilePath;
			CookedFileName->ToString(FilePath);
			IFileManager::Get().Delete(*FilePath, true, true, true);
		}
	};
	ParallelFor(UncookedFileNamesToRemove.Num(), DeletePackageLambda);

	for (FName UncookedFilename : UncookedFileNamesToRemove)
	{
		UncookedPathToCookedPath.Remove(UncookedFilename);
	}
}

void FLooseCookedPackageWriter::MarkPackagesUpToDate(TArrayView<const FName> UpToDatePackages)
{
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
				const TCHAR* ExtensionStr = Extension.GetData();
				check(ExtensionStr[Extension.Len()] == '\0'); // IsPackageExtension takes a null-terminated TCHAR; we should have it since GetExtension is from the end of the filename
				if (FPackageName::IsPackageExtension(ExtensionStr))
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
	const FString RelativeRootDir = FPaths::GetRelativePathToRoot();
	const FString RelativeProjectDir = FPaths::ProjectDir();
	FString UncookedFilename;
	UncookedFilename.Reserve(1024);

	for (const FString& CookedFile : CookedFiles)
	{
		const FName CookedFName(*CookedFile);
		const FName UncookedFName = ConvertCookedPathToUncookedPath(
			SandboxRootDir, RelativeRootDir,
			SandboxProjectDir, RelativeProjectDir,
			CookedFile, UncookedFilename);

		UncookedPathToCookedPath.Add(UncookedFName, CookedFName);
	}
}

FName FLooseCookedPackageWriter::ConvertCookedPathToUncookedPath(
	const FString& SandboxRootDir, const FString& RelativeRootDir,
	const FString& SandboxProjectDir, const FString& RelativeProjectDir,
	const FString& CookedPath, FString& OutUncookedPath) const
{
	OutUncookedPath.Reset();

	// Check for remapped plugins' cooked content
	if (PluginsToRemap.Num() > 0 && CookedPath.Contains(REMAPPED_PLUGINS))
	{
		int32 RemappedIndex = CookedPath.Find(REMAPPED_PLUGINS);
		check(RemappedIndex >= 0);
		static uint32 RemappedPluginStrLen = FCString::Strlen(REMAPPED_PLUGINS);
		// Snip everything up through the RemappedPlugins/ off so we can find the plugin it corresponds to
		FString PluginPath = CookedPath.RightChop(RemappedIndex + RemappedPluginStrLen + 1);
		// Find the plugin that owns this content
		for (const TSharedRef<IPlugin>& Plugin : PluginsToRemap)
		{
			if (PluginPath.StartsWith(Plugin->GetName()))
			{
				OutUncookedPath = Plugin->GetContentDir();
				static uint32 ContentStrLen = FCString::Strlen(TEXT("Content/"));
				// Chop off the pluginName/Content since it's part of the full path
				OutUncookedPath /= PluginPath.RightChop(Plugin->GetName().Len() + ContentStrLen);
				break;
			}
		}

		if (OutUncookedPath.Len() > 0)
		{
			return FName(*OutUncookedPath);
		}
		// Otherwise fall through to sandbox handling
	}

	auto BuildUncookedPath =
		[&OutUncookedPath](const FString& CookedPath, const FString& CookedRoot, const FString& UncookedRoot)
	{
		OutUncookedPath.AppendChars(*UncookedRoot, UncookedRoot.Len());
		OutUncookedPath.AppendChars(*CookedPath + CookedRoot.Len(), CookedPath.Len() - CookedRoot.Len());
	};

	if (CookedPath.StartsWith(SandboxRootDir))
	{
		// Optimized CookedPath.StartsWith(SandboxProjectDir) that does not compare all of SandboxRootDir again
		if (CookedPath.Len() >= SandboxProjectDir.Len() &&
			0 == FCString::Strnicmp(
				*CookedPath + SandboxRootDir.Len(),
				*SandboxProjectDir + SandboxRootDir.Len(),
				SandboxProjectDir.Len() - SandboxRootDir.Len()))
		{
			BuildUncookedPath(CookedPath, SandboxProjectDir, RelativeProjectDir);
		}
		else
		{
			BuildUncookedPath(CookedPath, SandboxRootDir, RelativeRootDir);
		}
	}
	else
	{
		FString FullCookedFilename = FPaths::ConvertRelativePathToFull(CookedPath);
		BuildUncookedPath(FullCookedFilename, SandboxRootDir, RelativeRootDir);
	}

	// Convert to a standard filename as required by PackageDatas where this path is used.
	FPaths::MakeStandardFilename(OutUncookedPath);

	return FName(*OutUncookedPath);
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