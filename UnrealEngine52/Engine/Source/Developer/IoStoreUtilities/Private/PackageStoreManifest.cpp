// Copyright Epic Games, Inc. All Rights Reserved.

#include "PackageStoreManifest.h"
#include "Serialization/CompactBinaryContainerSerialization.h"
#include "Serialization/CompactBinarySerialization.h"
#include "Serialization/CompactBinaryWriter.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "HAL/LowLevelMemTracker.h"
#include "IO/IoStore.h"
#include "Misc/FileHelper.h"
#include "Misc/ScopeLock.h"
#include "Misc/Paths.h"

LLM_DEFINE_TAG(Cooker_PackageStoreManifest);
FPackageStoreManifest::FPackageStoreManifest(const FString& InCookedOutputPath)
	: CookedOutputPath(InCookedOutputPath)
{
	LLM_SCOPE_BYTAG(Cooker_PackageStoreManifest);
	FPaths::NormalizeFilename(CookedOutputPath);
}

void FPackageStoreManifest::BeginPackage(FName PackageName)
{
	LLM_SCOPE_BYTAG(Cooker_PackageStoreManifest);
	FScopeLock Lock(&CriticalSection);
	FPackageInfo& PackageInfo = PackageInfoByNameMap.FindOrAdd(PackageName);
	PackageInfo.PackageName = PackageName;
	for (const FIoChunkId& ExportBundleChunkId : PackageInfo.ExportBundleChunkIds)
	{
		FileNameByChunkIdMap.Remove(ExportBundleChunkId);
	}
	for (const FIoChunkId& BulkDataChunkId : PackageInfo.BulkDataChunkIds)
	{
		FileNameByChunkIdMap.Remove(BulkDataChunkId);
	}
	PackageInfo.BulkDataChunkIds.Reset();
}

void FPackageStoreManifest::AddPackageData(FName PackageName, const FString& FileName, const FIoChunkId& ChunkId)
{
	LLM_SCOPE_BYTAG(Cooker_PackageStoreManifest);
	FScopeLock Lock(&CriticalSection);
	FPackageInfo* PackageInfo = GetPackageInfo_NoLock(PackageName);
	check(PackageInfo);
	PackageInfo->ExportBundleChunkIds.Add(ChunkId);
	if (!FileName.IsEmpty())
	{
		if (!bTrackPackageData)
		{
			FileNameByChunkIdMap.Add(ChunkId, FileName);
		}
		else
		{
			PackageFileChunkIds.FindOrAdd(PackageName).Emplace(FileName, ChunkId);
		}
	}
}

void FPackageStoreManifest::AddBulkData(FName PackageName, const FString& FileName, const FIoChunkId& ChunkId)
{
	LLM_SCOPE_BYTAG(Cooker_PackageStoreManifest);
	FScopeLock Lock(&CriticalSection);
	FPackageInfo* PackageInfo = GetPackageInfo_NoLock(PackageName);
	check(PackageInfo);
	PackageInfo->BulkDataChunkIds.Add(ChunkId);
	if (!FileName.IsEmpty())
	{
		if (!bTrackPackageData)
		{
			FileNameByChunkIdMap.Add(ChunkId, FileName);
		}
		else
		{
			PackageFileChunkIds.FindOrAdd(PackageName).Emplace(FileName, ChunkId);
		}
	}
}

FIoStatus FPackageStoreManifest::Save(const TCHAR* Filename) const
{
	LLM_SCOPE_BYTAG(Cooker_PackageStoreManifest);
	FScopeLock Lock(&CriticalSection);
	TStringBuilder<64> ChunkIdStringBuilder;
	auto ChunkIdToString = [&ChunkIdStringBuilder](const FIoChunkId& ChunkId)
	{
		ChunkIdStringBuilder.Reset();
		ChunkIdStringBuilder << ChunkId;
		return *ChunkIdStringBuilder;
	};

	FString JsonTcharText;
	TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&JsonTcharText);
	Writer->WriteObjectStart();

	if (ZenServerInfo)
	{
		Writer->WriteObjectStart(TEXT("ZenServer"));
		Writer->WriteObjectStart(TEXT("Settings"));
		ZenServerInfo->Settings.WriteToJson(*Writer);
		Writer->WriteObjectEnd();
		Writer->WriteValue(TEXT("ProjectId"), ZenServerInfo->ProjectId);
		Writer->WriteValue(TEXT("OplogId"), ZenServerInfo->OplogId);
		Writer->WriteObjectEnd();
	}
	
	Writer->WriteArrayStart(TEXT("Files"));

	// Convert FilePaths in ChunkIdMap to RelativePaths from the CookedOutput folder
	// Sort by RelativePath for determinism
	TArray<TPair<const FIoChunkId*, FString>> SortedFileNameByChunkIdMap;
	SortedFileNameByChunkIdMap.Reserve(FileNameByChunkIdMap.Num());
	for (const TPair<FIoChunkId, FString>& KV : FileNameByChunkIdMap)
	{
		FString RelativePath = KV.Value;
		FPaths::MakePathRelativeTo(RelativePath, *CookedOutputPath);
		SortedFileNameByChunkIdMap.Emplace(&KV.Key, MoveTemp(RelativePath));
	}
	SortedFileNameByChunkIdMap.Sort([]
		(const TPair<const FIoChunkId*, FString>& A, const TPair<const FIoChunkId*, FString>& B)
		{
			return A.Value < B.Value;
		});
	for (const TPair<const FIoChunkId*, FString>& KV : SortedFileNameByChunkIdMap)
	{
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("Path"), KV.Value);
		Writer->WriteValue(TEXT("ChunkId"), ChunkIdToString(*KV.Key));
		Writer->WriteObjectEnd();
	}
	SortedFileNameByChunkIdMap.Empty();
	Writer->WriteArrayEnd();

	constexpr int32 ChunkIdStringsBufferSize = 10;
	TArray<FString, TInlineAllocator<ChunkIdStringsBufferSize>> ChunkIdStringsBuffer;
	auto WritePackageInfoObject = [Writer, &ChunkIdStringsBuffer, ChunkIdStringsBufferSize]
	(const FPackageInfo& PackageInfo, const TCHAR* Name = nullptr)
	{
		if (Name)
		{
			Writer->WriteObjectStart(Name);
		}
		else
		{
			Writer->WriteObjectStart();
		}
		auto AllocateChunkIdStrings = [&ChunkIdStringsBuffer, ChunkIdStringsBufferSize](int32 Num)
		{
			if (ChunkIdStringsBuffer.Num() < Num)
			{
				if (ChunkIdStringsBuffer.Max() < Num)
				{
					ChunkIdStringsBuffer.SetNum(Num * 2 + ChunkIdStringsBufferSize, false /* bAllowShrinking */);
				}
				else
				{
					ChunkIdStringsBuffer.SetNum(ChunkIdStringsBuffer.Max(), false /* bAllowShrinking */);
				}
			}
			return TArrayView<FString>(ChunkIdStringsBuffer.GetData(), Num);
		};
		Writer->WriteValue(TEXT("Name"), PackageInfo.PackageName.ToString());
		if (!PackageInfo.ExportBundleChunkIds.IsEmpty())
		{
			// Determinism: Sort ExportBundleChunkIds by string
			TArrayView<FString> ChunkIdStrings = AllocateChunkIdStrings(PackageInfo.ExportBundleChunkIds.Num());
			int32 Index = 0;
			for (const FIoChunkId& ChunkId : PackageInfo.ExportBundleChunkIds)
			{
				ChunkId.ToString(ChunkIdStrings[Index++]);
			}
			Algo::Sort(ChunkIdStrings);

			Writer->WriteArrayStart(TEXT("ExportBundleChunkIds"));
			for (const FString& ChunkIdString : ChunkIdStrings)
			{
				Writer->WriteValue(ChunkIdString);
			}
			Writer->WriteArrayEnd();
		}
		if (!PackageInfo.BulkDataChunkIds.IsEmpty())
		{
			// Determinism: Sort BulkDataChunkIds by string
			TArrayView<FString> ChunkIdStrings = AllocateChunkIdStrings(PackageInfo.BulkDataChunkIds.Num());
			int32 Index = 0;
			for (const FIoChunkId& ChunkId : PackageInfo.BulkDataChunkIds)
			{
				ChunkId.ToString(ChunkIdStrings[Index++]);
			}

			Writer->WriteArrayStart(TEXT("BulkDataChunkIds"));
			for (const FString& ChunkIdString : ChunkIdStrings)
			{
				Writer->WriteValue(ChunkIdString);
			}
			Writer->WriteArrayEnd();
		}
		Writer->WriteObjectEnd();
	};

	Writer->WriteArrayStart(TEXT("Packages"));
	// Sort PackageInfoByNameMap by PackageName
	TArray<TPair<FName, const FPackageInfo*>> SortedPackageInfoByNameMap;
	SortedPackageInfoByNameMap.Reserve(PackageInfoByNameMap.Num());
	for (const TPair<FName, FPackageInfo>& KV : PackageInfoByNameMap)
	{
		SortedPackageInfoByNameMap.Emplace(KV.Key, &KV.Value);
	}
	SortedPackageInfoByNameMap.Sort([]
	(const TPair<FName, const FPackageInfo*>& A, const TPair<FName, const FPackageInfo*>& B)
		{
			return A.Key.LexicalLess(B.Key);
		});
	for (const TPair<FName, const FPackageInfo*>& KV : SortedPackageInfoByNameMap)
	{
		WritePackageInfoObject(*KV.Value);
	}
	Writer->WriteArrayEnd();
	ChunkIdStringsBuffer.Empty();

	Writer->WriteObjectEnd();
	Writer->Close();

	if (!FFileHelper::SaveStringToFile(JsonTcharText, Filename))
	{
		return FIoStatus(EIoErrorCode::FileOpenFailed);
	}

	return FIoStatus::Ok;
}

FIoStatus FPackageStoreManifest::Load(const TCHAR* Filename)
{
	LLM_SCOPE_BYTAG(Cooker_PackageStoreManifest);
	FScopeLock Lock(&CriticalSection);
	PackageInfoByNameMap.Empty();
	FileNameByChunkIdMap.Empty();

	auto ChunkIdFromString = [](const FString& ChunkIdString)
	{
		FStringView ChunkIdStringView(*ChunkIdString, 24);
		uint8 Data[12];
		UE::String::HexToBytes(ChunkIdStringView, Data);
		FIoChunkId ChunkId;
		ChunkId.Set(Data, 12);
		return ChunkId;
	};

	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, Filename))
	{
		return FIoStatus(EIoErrorCode::FileOpenFailed);
	}

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		return FIoStatus(EIoErrorCode::Unknown);
	}

	TSharedPtr<FJsonValue> ZenServerValue = JsonObject->Values.FindRef(TEXT("ZenServer"));
	if (ZenServerValue)
	{
		ZenServerInfo = MakeUnique<FZenServerInfo>();
		TSharedPtr<FJsonObject> ZenServerObject = ZenServerValue->AsObject();

		TSharedPtr<FJsonValue> SettingsValue = ZenServerObject->Values.FindRef(TEXT("Settings"));
		if (SettingsValue)
		{
			TSharedPtr<FJsonObject> SettingsObject = SettingsValue->AsObject();
			ZenServerInfo->Settings.ReadFromJson(*SettingsObject);
		}
		ZenServerInfo->ProjectId = ZenServerObject->Values.FindRef(TEXT("ProjectId"))->AsString();
		ZenServerInfo->OplogId = ZenServerObject->Values.FindRef(TEXT("OplogId"))->AsString();
	}

	TSharedPtr<FJsonValue> FilesArrayValue = JsonObject->Values.FindRef(TEXT("Files"));
	TArray<TSharedPtr<FJsonValue>> FilesArray = FilesArrayValue->AsArray();
	FileNameByChunkIdMap.Reserve(FilesArray.Num());
	for (const TSharedPtr<FJsonValue>& FileValue : FilesArray)
	{
		TSharedPtr<FJsonObject> FileObject = FileValue->AsObject();
		FIoChunkId ChunkId = ChunkIdFromString(FileObject->Values.FindRef(TEXT("ChunkId"))->AsString());
		FString RelativePath = FileObject->Values.FindRef(TEXT("Path"))->AsString();
		FileNameByChunkIdMap.Add(ChunkId, FPaths::Combine(CookedOutputPath, RelativePath));
	}


	auto ReadPackageInfo = [&ChunkIdFromString](TSharedPtr<FJsonObject> PackageObject, FPackageInfo& PackageInfo)
	{
		check(!PackageInfo.PackageName.IsNone());
		
		TSharedPtr<FJsonValue> ExportBundleChunkIdsValue = PackageObject->Values.FindRef(TEXT("ExportBundleChunkIds"));
		if (ExportBundleChunkIdsValue.IsValid())
		{
			TArray<TSharedPtr<FJsonValue>> ExportBundleChunkIdsArray = ExportBundleChunkIdsValue->AsArray();
			PackageInfo.ExportBundleChunkIds.Reserve(ExportBundleChunkIdsArray.Num());
			for (const TSharedPtr<FJsonValue>& ExportBundleChunkIdValue : ExportBundleChunkIdsArray)
			{
				PackageInfo.ExportBundleChunkIds.Add(ChunkIdFromString(ExportBundleChunkIdValue->AsString()));
			}
		}

		TSharedPtr<FJsonValue> BulkDataChunkIdsValue = PackageObject->Values.FindRef(TEXT("BulkDataChunkIds"));
		if (BulkDataChunkIdsValue.IsValid())
		{
			TArray<TSharedPtr<FJsonValue>> BulkDataChunkIdsArray = BulkDataChunkIdsValue->AsArray();
			PackageInfo.BulkDataChunkIds.Reserve(BulkDataChunkIdsArray.Num());
			for (const TSharedPtr<FJsonValue>& BulkDataChunkIdValue : BulkDataChunkIdsArray)
			{
				PackageInfo.BulkDataChunkIds.Add(ChunkIdFromString(BulkDataChunkIdValue->AsString()));
			}
		}
	};

	TArray<TSharedPtr<FJsonValue>> PackagesArray = JsonObject->Values.FindRef(TEXT("Packages"))->AsArray();
	PackageInfoByNameMap.Reserve(PackagesArray.Num());
	for (const TSharedPtr<FJsonValue>& PackageValue : PackagesArray)
	{
		TSharedPtr<FJsonObject> PackageObject = PackageValue->AsObject();
		FName PackageName = FName(PackageObject->Values.FindRef(TEXT("Name"))->AsString());

		FPackageInfo& PackageInfo = PackageInfoByNameMap.FindOrAdd(PackageName);
		PackageInfo.PackageName = PackageName;
		ReadPackageInfo(PackageObject, PackageInfo);
	}

	return FIoStatus::Ok;
}

TArray<FPackageStoreManifest::FFileInfo> FPackageStoreManifest::GetFiles() const
{
	FScopeLock Lock(&CriticalSection);
	TArray<FFileInfo> Files;
	Files.Reserve(FileNameByChunkIdMap.Num());
	for (const auto& KV : FileNameByChunkIdMap)
	{
		Files.Add({ KV.Value, KV.Key });
	}
	return Files;
}

TArray<FPackageStoreManifest::FPackageInfo> FPackageStoreManifest::GetPackages() const
{
	FScopeLock Lock(&CriticalSection);
	TArray<FPackageInfo> Packages;
	PackageInfoByNameMap.GenerateValueArray(Packages);
	return Packages;
}

FPackageStoreManifest::FZenServerInfo& FPackageStoreManifest::EditZenServerInfo()
{
	FScopeLock Lock(&CriticalSection);
	if (!ZenServerInfo)
	{
		ZenServerInfo = MakeUnique<FZenServerInfo>();
	}
	return *ZenServerInfo;
}

const FPackageStoreManifest::FZenServerInfo* FPackageStoreManifest::ReadZenServerInfo() const
{
	FScopeLock Lock(&CriticalSection);
	return ZenServerInfo.Get();
}

FPackageStoreManifest::FPackageInfo* FPackageStoreManifest::GetPackageInfo_NoLock(FName PackageName)
{
	return PackageInfoByNameMap.Find(PackageName);
}

void FPackageStoreManifest::SetTrackPackageData(bool bInTrackPackageData)
{
	FScopeLock Lock(&CriticalSection);
	bTrackPackageData = bInTrackPackageData;
}

void FPackageStoreManifest::WritePackage(FCbWriter& Writer, FName PackageName)
{
	FPackageStoreManifest::FPackageInfo PackageInfo;
	TArray<TPair<FString, FIoChunkId>> FileChunkIds;
	bool bHasPackageInfo;

	{
		FScopeLock Lock(&CriticalSection);
		bHasPackageInfo = PackageInfoByNameMap.RemoveAndCopyValue(PackageName, PackageInfo);
		PackageFileChunkIds.RemoveAndCopyValue(PackageName, FileChunkIds);
	}

	// For a failed package, CommitPackage may have never been called. Send empty values in that case.

	Writer.BeginObject();
	Writer << "ExportBundleChunkIds" << PackageInfo.ExportBundleChunkIds;
	Writer << "BulkDataChunkIds" << PackageInfo.BulkDataChunkIds;
	Writer << "FileChunkIds" << FileChunkIds;
	Writer.EndObject();
}

bool FPackageStoreManifest::TryReadPackage(FCbFieldView Field, FName PackageName)
{
	FPackageInfo PackageInfo;
	TArray<TPair<FString, FIoChunkId>> FileChunkIds;

	bool bOk = true;
	PackageInfo.PackageName = PackageName;
	bOk = LoadFromCompactBinary(Field["ExportBundleChunkIds"], PackageInfo.ExportBundleChunkIds) & bOk;
	bOk = LoadFromCompactBinary(Field["BulkDataChunkIds"], PackageInfo.BulkDataChunkIds) & bOk;
	bOk = LoadFromCompactBinary(Field["FileChunkIds"], FileChunkIds) & bOk;

	{
		FScopeLock Lock(&CriticalSection);
		PackageInfoByNameMap.Add(PackageName, MoveTemp(PackageInfo));
		for (TPair<FString, FIoChunkId>& Pair : FileChunkIds)
		{
			FileNameByChunkIdMap.Add(Pair.Value, Pair.Key);
		}
	}

	return bOk;
}

