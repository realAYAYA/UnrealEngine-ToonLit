// Copyright Epic Games, Inc. All Rights Reserved.

#include "PackageStoreManifest.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/ScopeLock.h"
#include "Misc/Paths.h"

FPackageStoreManifest::FPackageStoreManifest(const FString& InCookedOutputPath)
	: CookedOutputPath(InCookedOutputPath)
{
	FPaths::NormalizeFilename(CookedOutputPath);
}

void FPackageStoreManifest::BeginPackage(FName PackageName)
{
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
	FScopeLock Lock(&CriticalSection);
	FPackageInfo* PackageInfo = GetPackageInfo_NoLock(PackageName);
	check(PackageInfo);
	PackageInfo->ExportBundleChunkIds.Add(ChunkId);
	if (!FileName.IsEmpty())
	{
		FileNameByChunkIdMap.Add(ChunkId, FileName);
	}
}

void FPackageStoreManifest::AddBulkData(FName PackageName, const FString& FileName, const FIoChunkId& ChunkId)
{
	FScopeLock Lock(&CriticalSection);
	FPackageInfo* PackageInfo = GetPackageInfo_NoLock(PackageName);
	check(PackageInfo);
	PackageInfo->BulkDataChunkIds.Add(ChunkId);
	if (!FileName.IsEmpty())
	{
		FileNameByChunkIdMap.Add(ChunkId, FileName);
	}
}

FIoStatus FPackageStoreManifest::Save(const TCHAR* Filename) const
{
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
	for (const auto& KV : FileNameByChunkIdMap)
	{
		Writer->WriteObjectStart();
		FString RelativePath = KV.Value;
		FPaths::MakePathRelativeTo(RelativePath, *CookedOutputPath);
		Writer->WriteValue(TEXT("Path"), RelativePath);
		Writer->WriteValue(TEXT("ChunkId"), ChunkIdToString(KV.Key));
		Writer->WriteObjectEnd();
	}
	Writer->WriteArrayEnd();

	auto WritePackageInfoObject = [Writer, &ChunkIdToString](const FPackageInfo& PackageInfo, const TCHAR* Name = nullptr)
	{
		if (Name)
		{
			Writer->WriteObjectStart(Name);
		}
		else
		{
			Writer->WriteObjectStart();
		}
		Writer->WriteValue(TEXT("Name"), PackageInfo.PackageName.ToString());
		if (!PackageInfo.ExportBundleChunkIds.IsEmpty())
		{
			Writer->WriteArrayStart(TEXT("ExportBundleChunkIds"));
			for (const FIoChunkId& ChunkId : PackageInfo.ExportBundleChunkIds)
			{
				Writer->WriteValue(ChunkIdToString(ChunkId));
			}
			Writer->WriteArrayEnd();
		}
		if (!PackageInfo.BulkDataChunkIds.IsEmpty())
		{
			Writer->WriteArrayStart(TEXT("BulkDataChunkIds"));
			for (const FIoChunkId& ChunkId : PackageInfo.BulkDataChunkIds)
			{
				Writer->WriteValue(ChunkIdToString(ChunkId));
			}
			Writer->WriteArrayEnd();
		}
		Writer->WriteObjectEnd();
	};

	Writer->WriteArrayStart(TEXT("Packages"));
	for (const auto& PackageNameInfoPair : PackageInfoByNameMap)
	{
		const FPackageInfo& PackageInfo = PackageNameInfoPair.Value;
		WritePackageInfoObject(PackageInfo);
	}
	Writer->WriteArrayEnd();

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

