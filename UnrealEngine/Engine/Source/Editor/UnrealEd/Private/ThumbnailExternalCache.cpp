// Copyright Epic Games, Inc. All Rights Reserved.

#include "ThumbnailExternalCache.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "AssetThumbnail.h"
#include "Misc/ObjectThumbnail.h"
#include "ObjectTools.h"
#include "Serialization/Archive.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/ScopedSlowTask.h"
#include "Interfaces/IPluginManager.h"
#include "ImageUtils.h"
#include "Hash/CityHash.h"
#include "Async/Async.h"
#include "Async/ParallelFor.h"

#define LOCTEXT_NAMESPACE "ThumbnailExternalCache"

DEFINE_LOG_CATEGORY_STATIC(LogThumbnailExternalCache, Log, All);

namespace ThumbnailExternalCache
{
	const int64 LatestVersion = 0;
	const uint64 ExpectedHeaderId = 0x424d5548545f4555; // "UE_THUMB"
	const FString ThumbnailImageFormatName(TEXT(""));

	void ResizeThumbnailImage(FObjectThumbnail& Thumbnail, const int32 NewWidth, const int32 NewHeight)
	{
		TArray<uint8> DestData;
		DestData.AddUninitialized(NewWidth * NewHeight * sizeof(FColor));

		// Force decompress if needed
		Thumbnail.GetUncompressedImageData();
		TArray<uint8>& UncompressedImageData = Thumbnail.AccessImageData();

		const bool bLinearSpace = false;
		const bool bForceOpaqueOutput = false;
		const TArrayView<FColor> SrcDataView(reinterpret_cast<FColor*>(UncompressedImageData.GetData()), UncompressedImageData.Num() / sizeof(FColor));
		const TArrayView<FColor> DestDataView(reinterpret_cast<FColor*>(DestData.GetData()), DestData.Num() / sizeof(FColor));
		FImageUtils::ImageResize(Thumbnail.GetImageWidth(), Thumbnail.GetImageHeight(), SrcDataView, NewWidth, NewHeight, DestDataView, bLinearSpace, bForceOpaqueOutput);

		UncompressedImageData = MoveTemp(DestData);
		Thumbnail.SetImageSize(NewWidth, NewHeight);

		// Invalidate compressed data so it will be recompressed
		Thumbnail.AccessCompressedImageData().Reset();
	}

	// Return true if was resized
	bool ResizeThumbnailIfNeeded(FObjectThumbnail& Thumbnail, const int32 MaxImageSize)
	{
		const int32 Width = Thumbnail.GetImageWidth();
		const int32 Height = Thumbnail.GetImageHeight();

		// Resize if larger than maximum size
		if (Width > MaxImageSize || Height > MaxImageSize)
		{
			const double ShrinkModifier = (double)FMath::Max<int32>(Width, Height) / (double)MaxImageSize;
			const int32 NewWidth = int32((double)Width / ShrinkModifier);
			const int32 NewHeight = int32((double)Height / ShrinkModifier);
			ResizeThumbnailImage(Thumbnail, NewWidth, NewHeight);

			return true;
		}

		return false;
	}
}

struct FThumbnailDeduplicateKey
{
	FThumbnailDeduplicateKey() {}
	FThumbnailDeduplicateKey(uint64 InHash, int32 InNumBytes) : Hash(InHash), NumBytes(InNumBytes) {}

	uint64 Hash = 0;
	int32 NumBytes = 0;

	bool operator ==( const FThumbnailDeduplicateKey& Other ) const
	{
		return Hash == Other.Hash && NumBytes == Other.NumBytes;
	}
};

inline uint32 GetTypeHash(const FThumbnailDeduplicateKey& InValue)
{
	return GetTypeHash(InValue.Hash);
}

struct FPackageThumbnailRecord
{
	FName Name;
	int64 Offset = 0;
};

class FSaveThumbnailCacheTask
{
public:
	FObjectThumbnail ObjectThumbnail;
	FName Name;
	uint64 CompressedBytesHash = 0;

	void Compress(const FThumbnailExternalCacheSettings& InSettings);
};

class FSaveThumbnailCache
{
public:
	FSaveThumbnailCache();
	~FSaveThumbnailCache();

	void Save(FArchive& Ar, const TArrayView<FAssetData> InAssetDatas, const FThumbnailExternalCacheSettings& InSettings);
};

FSaveThumbnailCache::FSaveThumbnailCache()
{
}

FSaveThumbnailCache::~FSaveThumbnailCache()
{
}

void FSaveThumbnailCache::Save(FArchive& Ar, const TArrayView<FAssetData> InAssetDatas, const FThumbnailExternalCacheSettings& InSettings)
{
	// Reduce peak memory to support larger asset counts by loading then saving in batches
	int32 TaskBatchSize = 100000;

	const double TimeStart = FPlatformTime::Seconds();

	const int32 NumAssetDatas = InAssetDatas.Num();

	UE_LOG(LogThumbnailExternalCache, Log, TEXT("Saving thumbnails for %d assets to %s"), NumAssetDatas, *Ar.GetArchiveName());

	FText StatusText = LOCTEXT("SaveStatus", "Saving Thumbnails: {0}");
	FScopedSlowTask SlowTask( (float)NumAssetDatas / (float)TaskBatchSize, FText::Format(StatusText, FText::AsNumber(NumAssetDatas)));
	SlowTask.MakeDialog(/*bShowCancelButton*/ false);

	TArray<FPackageThumbnailRecord> PackageThumbnailRecords;
	PackageThumbnailRecords.Reset();
	PackageThumbnailRecords.Reserve(NumAssetDatas);

	TMap<FThumbnailDeduplicateKey, int64> DeduplicateMap;	
	DeduplicateMap.Reset();
	DeduplicateMap.Reserve(NumAssetDatas);
	int32 NumDuplicates = 0;
	int64 DuplicateBytesSaved = 0;

	int64 TotalCompressedBytes = 0;

	{
		FThumbnailExternalCache::FThumbnailExternalCacheHeader Header;
		Header.HeaderId = ThumbnailExternalCache::ExpectedHeaderId;
		Header.Version = ThumbnailExternalCache::LatestVersion;
		Header.Flags = 0;
		Header.ImageFormatName = ThumbnailExternalCache::ThumbnailImageFormatName;
		Header.Serialize(Ar);
	}
	const int64 ThumbnailTableOffsetPos = Ar.Tell() - sizeof(int64);

	double LoadTime = 0.0;
	double SaveTime = 0.0;

	for (int32 StartIndex = 0; StartIndex < InAssetDatas.Num(); StartIndex += TaskBatchSize)
	{
		const int32 EndIndex = FMath::Min<int32>(StartIndex + TaskBatchSize, InAssetDatas.Num());
		const TArrayView<FAssetData> AssetsForSegment(&InAssetDatas[StartIndex], EndIndex - StartIndex);

		TArray<FSaveThumbnailCacheTask> Tasks;
		Tasks.AddDefaulted(AssetsForSegment.Num());

		const double LoadTimeStart = FPlatformTime::Seconds();

		// Load then recompress if needed. Thread for improved load performance.
		ParallelFor(AssetsForSegment.Num(), [AssetsForSegment, &Tasks, &InSettings](int32 Index)
		{
			FSaveThumbnailCacheTask& Task = Tasks[Index];
			if (ThumbnailTools::LoadThumbnailFromPackage(AssetsForSegment[Index], Task.ObjectThumbnail) && !Task.ObjectThumbnail.IsEmpty())
			{
				{
					FNameBuilder ObjectFullNameBuilder;
					AssetsForSegment[Index].GetFullName(ObjectFullNameBuilder);
					Task.Name = FName(ObjectFullNameBuilder);
				}

				Task.Compress(InSettings);
			}
		});

		const double SaveTimeStart = FPlatformTime::Seconds();

		// Save compressed image data
		for (FSaveThumbnailCacheTask& Task : Tasks)
		{
			// Skip empty
			if (Task.ObjectThumbnail.IsEmpty())
			{
				continue;
			}

			// Add table of contents entry
			FPackageThumbnailRecord& PackageThumbnailRecord = PackageThumbnailRecords.AddDefaulted_GetRef();
			PackageThumbnailRecord.Name = Task.Name;

			FThumbnailDeduplicateKey DeduplicateKey(Task.CompressedBytesHash, Task.ObjectThumbnail.GetCompressedDataSize());
			if (const int64* ExistingOffset = DeduplicateMap.Find(DeduplicateKey))
			{
				// Reference existing compressed image data
				PackageThumbnailRecord.Offset = *ExistingOffset;
				DuplicateBytesSaved += DeduplicateKey.NumBytes;
				++NumDuplicates;
			}
			else
			{
				// Save compressed image data
				PackageThumbnailRecord.Offset = Ar.Tell();
				Task.ObjectThumbnail.Serialize(Ar);
				DeduplicateMap.Add(DeduplicateKey, PackageThumbnailRecord.Offset);
				TotalCompressedBytes += DeduplicateKey.NumBytes;
			}

			// Free memory
			Task.ObjectThumbnail.AccessCompressedImageData().Empty();
		}

		SaveTime += FPlatformTime::Seconds() - SaveTimeStart;
		LoadTime += SaveTimeStart - LoadTimeStart;

		SlowTask.EnterProgressFrame((float)AssetsForSegment.Num() / (float)TaskBatchSize, FText::Format(StatusText, FText::AsNumber(NumAssetDatas - EndIndex)));
		if (SlowTask.ShouldCancel())
		{
			PackageThumbnailRecords.Reset();
			break;
		}
	}

	// Save table of contents
	int64 NewThumbnailTableOffset = Ar.Tell();

	int64 NumThumbnails = PackageThumbnailRecords.Num();
	Ar << NumThumbnails;
	{
		FString ThumbnailNameString;
		int64 Index = 0;
		for (FPackageThumbnailRecord& PackageThumbnailRecord : PackageThumbnailRecords)
		{
			ThumbnailNameString.Reset();
			PackageThumbnailRecord.Name.AppendString(ThumbnailNameString);
			UE_LOG(LogThumbnailExternalCache, Verbose, TEXT("\t[%d] %s"), Index++, *ThumbnailNameString);
			Ar << ThumbnailNameString;
			Ar << PackageThumbnailRecord.Offset;
		}
	}

	// Modify top of archive to know where table of contents is located
	Ar.Seek(ThumbnailTableOffsetPos);
	Ar << NewThumbnailTableOffset;

	UE_LOG(LogThumbnailExternalCache, Log, TEXT("Load Time: %f secs, Save Time: %f secs, Total Time: %f secs"), LoadTime, SaveTime, FPlatformTime::Seconds() - TimeStart);
	UE_LOG(LogThumbnailExternalCache, Log, TEXT("Thumbnails: %d, %f MB"), PackageThumbnailRecords.Num(), (TotalCompressedBytes / (1024.0 * 1024.0)));
	UE_LOG(LogThumbnailExternalCache, Log, TEXT("Duplicates: %d, %f MB"), NumDuplicates, (DuplicateBytesSaved / (1024.0 * 1024.0)));
}

void FSaveThumbnailCacheTask::Compress(const FThumbnailExternalCacheSettings& InSettings)
{
	ThumbnailExternalCache::ResizeThumbnailIfNeeded(ObjectThumbnail, InSettings.MaxImageSize);

	if (ObjectThumbnail.GetCompressedDataSize() > 0)
	{
		if (InSettings.bRecompressLossless)
		{
			// See if compressor would change
			FThumbnailCompressionInterface* SourceCompressor = ObjectThumbnail.GetCompressor();
			FThumbnailCompressionInterface* DestCompressor = ObjectThumbnail.ChooseNewCompressor();
			if (SourceCompressor != DestCompressor && SourceCompressor && DestCompressor)
			{
				// Do not recompress lossy images because they are already likely small and artifacts in the image would increase
				if (SourceCompressor->IsLosslessCompression())
				{
					// Force decompress if needed so we can compress again
					ObjectThumbnail.GetUncompressedImageData();

					// Delete existing compressed image data and compress again
					ObjectThumbnail.CompressImageData();
				}
			}
		}
	}
	else
	{
		ObjectThumbnail.CompressImageData();
	}

	CompressedBytesHash = CityHash64(reinterpret_cast<const char*>(ObjectThumbnail.AccessCompressedImageData().GetData()), ObjectThumbnail.GetCompressedDataSize());

	// Release uncompressed image memory
	ObjectThumbnail.AccessImageData().Empty();
}

FThumbnailExternalCache::FThumbnailExternalCache()
{
}

FThumbnailExternalCache::~FThumbnailExternalCache()
{
	Cleanup();
}

FThumbnailExternalCache& FThumbnailExternalCache::Get()
{
	static FThumbnailExternalCache ThumbnailExternalCache;
	return ThumbnailExternalCache;
}

const FString& FThumbnailExternalCache::GetCachedEditorThumbnailsFilename()
{
	static const FString Filename = TEXT("CachedEditorThumbnails.bin");
	return Filename;
}

void FThumbnailExternalCache::Init()
{
	if (!bHasInit)
	{
		bHasInit = true;

		// Load file for project
		LoadCacheFileIndex(FPaths::ProjectDir() / FThumbnailExternalCache::GetCachedEditorThumbnailsFilename());

		// Load any thumbnail files for content plugins
		TArray<TSharedRef<IPlugin>> ContentPlugins = IPluginManager::Get().GetEnabledPluginsWithContent();
		for (const TSharedRef<IPlugin>& ContentPlugin : ContentPlugins)
		{
			LoadCacheFileIndexForPlugin(ContentPlugin);
		}

		// Look for cache file when a new path is mounted
		FPackageName::OnContentPathMounted().AddRaw(this, &FThumbnailExternalCache::OnContentPathMounted);

		// Unload cache file when path is unmounted
		FPackageName::OnContentPathDismounted().AddRaw(this, &FThumbnailExternalCache::OnContentPathDismounted);
	}
}

void FThumbnailExternalCache::Cleanup()
{
	if (bHasInit)
	{
		FPackageName::OnContentPathMounted().RemoveAll(this);
		FPackageName::OnContentPathDismounted().RemoveAll(this);
	}
}

bool FThumbnailExternalCache::LoadThumbnailsFromExternalCache(const TSet<FName>& InObjectFullNames, FThumbnailMap& InOutThumbnails)
{
	if (bIsSavingCache)
	{
		return false;
	}

	Init();

	if (CacheFiles.Num() == 0)
	{
		return false;
	}

	static const FString BlueprintGeneratedClassPrefix = TEXT("/Script/Engine.BlueprintGeneratedClass ");

	int32 NumLoaded = 0;
	for (const FName ObjectFullName : InObjectFullNames)
	{
		FName ThumbnailName = ObjectFullName;

		FNameBuilder NameBuilder(ObjectFullName);
		FStringView NameView(NameBuilder);

		// BlueprintGeneratedClass assets can be displayed in content browser but thumbnails are usually not saved to package file for them
		if (NameView.StartsWith(BlueprintGeneratedClassPrefix) && NameView.EndsWith(TEXT("_C")))
		{
			// Look for the thumbnail of the Blueprint version of this object instead
			FNameBuilder ModifiedNameBuilder;
			ModifiedNameBuilder.Append(TEXT("/Script/Engine.Blueprint "));
			FStringView ViewToAppend = NameView;
			ViewToAppend.RightChopInline(BlueprintGeneratedClassPrefix.Len());
			ViewToAppend.LeftChopInline(2);
			ModifiedNameBuilder.Append(ViewToAppend);
			ThumbnailName = FName(ModifiedNameBuilder.ToView());
		}

		for (TPair<FString, TSharedPtr<FThumbnailCacheFile>>& It : CacheFiles)
		{
			TSharedPtr<FThumbnailCacheFile>& ThumbnailCacheFile = It.Value;
			if (FThumbnailEntry* Found = ThumbnailCacheFile->NameToEntry.Find(ThumbnailName))
			{
				if (ThumbnailCacheFile->bUnableToOpenFile == false)
				{
					if (TUniquePtr<FArchive> FileReader = TUniquePtr<FArchive>(IFileManager::Get().CreateFileReader(*ThumbnailCacheFile->Filename)))
					{
						FileReader->Seek(Found->Offset);

						if (ensure(!FileReader->IsError()))
						{
							FObjectThumbnail ObjectThumbnail;
							(*FileReader) << ObjectThumbnail;
							
							InOutThumbnails.Add(ObjectFullName, ObjectThumbnail);
							++NumLoaded;
						}
					}
					else
					{
						// Avoid retrying if file no longer exists
						ThumbnailCacheFile->bUnableToOpenFile = true;
					}
				}
			}
		}
	}

	return NumLoaded > 0;
}

void FThumbnailExternalCache::SortAssetDatas(TArray<FAssetData>& AssetDatas)
{
	Algo::SortBy(AssetDatas, [](const FAssetData& Data) { return Data.PackageName; }, FNameLexicalLess());
}

bool FThumbnailExternalCache::SaveExternalCache(const FString& InFilename, const TArrayView<FAssetData> InAssetDatas, const FThumbnailExternalCacheSettings& InSettings)
{
	bIsSavingCache = true;
	if (TUniquePtr<FArchive> FileWriter = TUniquePtr<FArchive>(IFileManager::Get().CreateFileWriter(*InFilename)))
	{
		SaveExternalCache(*FileWriter, InAssetDatas, InSettings);
		bIsSavingCache = false;
		return true;
	}

	bIsSavingCache = false;

	return false;
}

void FThumbnailExternalCache::SaveExternalCache(FArchive& Ar, const TArrayView<FAssetData> InAssetDatas, const FThumbnailExternalCacheSettings& InSettings)
{
	bIsSavingCache = true;
	FSaveThumbnailCache SaveJob;
	SaveJob.Save(Ar, InAssetDatas, InSettings);
	bIsSavingCache = false;
}

void FThumbnailExternalCache::OnContentPathMounted(const FString& InAssetPath, const FString& InFilesystemPath)
{
	if (TSharedPtr<IPlugin> FoundPlugin = IPluginManager::Get().FindPluginFromPath(InAssetPath))
	{
		LoadCacheFileIndexForPlugin(FoundPlugin);
	}
}

void FThumbnailExternalCache::OnContentPathDismounted(const FString& InAssetPath, const FString& InFilesystemPath)
{
	if (TSharedPtr<IPlugin> FoundPlugin = IPluginManager::Get().FindPluginFromPath(InAssetPath))
	{
		if (FoundPlugin->CanContainContent())
		{
			const FString Filename = FoundPlugin->GetBaseDir() / FThumbnailExternalCache::GetCachedEditorThumbnailsFilename();
			CacheFiles.Remove(Filename);
		}
	}
}

void FThumbnailExternalCache::LoadCacheFileIndexForPlugin(const TSharedPtr<IPlugin> InPlugin)
{
	if (InPlugin && InPlugin->CanContainContent())
	{
		const FString Filename = InPlugin->GetBaseDir() / FThumbnailExternalCache::GetCachedEditorThumbnailsFilename();
		if (IFileManager::Get().FileExists(*Filename))
		{
			LoadCacheFileIndex(Filename);
		}
	}
}

bool FThumbnailExternalCache::LoadCacheFileIndex(const FString& Filename)
{
	// Stop if attempt to load already made
	if (CacheFiles.Contains(Filename))
	{
		return true;
	}

	// Track file
	TSharedPtr<FThumbnailCacheFile> ThumbnailCacheFile = MakeShared<FThumbnailCacheFile>();
	ThumbnailCacheFile->Filename = Filename;
	ThumbnailCacheFile->bUnableToOpenFile = true;
	CacheFiles.Add(Filename, ThumbnailCacheFile);

	// Attempt load index of file
	if (TUniquePtr<FArchive> FileReader = TUniquePtr<FArchive>(IFileManager::Get().CreateFileReader(*Filename)))
	{
		if (LoadCacheFileIndex(*FileReader, ThumbnailCacheFile))
		{
			ThumbnailCacheFile->bUnableToOpenFile = false;
			return true;
		}
	}

	return false;
}

bool FThumbnailExternalCache::LoadCacheFileIndex(FArchive& Ar, const TSharedPtr<FThumbnailCacheFile>& CacheFile)
{
	FThumbnailExternalCacheHeader& Header = CacheFile->Header;
	Header.Serialize(Ar);

	if (Header.HeaderId != ThumbnailExternalCache::ExpectedHeaderId)
	{
		return false;
	}

	if (Header.Version != 0)
	{
		return false;
	}

	Ar.Seek(Header.ThumbnailTableOffset);

	int64 NumPackages = 0;
	Ar << NumPackages;

	CacheFile->NameToEntry.Reserve(IntCastChecked<int32>(NumPackages));

	FString PackageNameString;
	for (int64 i=0; i < NumPackages; ++i)
	{
		PackageNameString.Reset();
		Ar << PackageNameString;

		FThumbnailEntry NewEntry;
		Ar << NewEntry.Offset;

		CacheFile->NameToEntry.Add(FName(PackageNameString), NewEntry);
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
