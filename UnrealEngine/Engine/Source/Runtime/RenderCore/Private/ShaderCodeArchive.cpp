// Copyright Epic Games, Inc. All Rights Reserved.

#include "ShaderCodeArchive.h"

#include "Async/ParallelFor.h"
#include "Compression/OodleDataCompression.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "Misc/Compression.h"
#include "Misc/FileHelper.h"
#include "Misc/MemStack.h"
#include "Misc/ScopeRWLock.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "RHI.h"
#include "RenderUtils.h"
#include "RHICommandList.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "ShaderCodeLibrary.h"
#include "ShaderCore.h"
#include "Stats/Stats.h"

#if WITH_EDITOR
#include "Misc/Optional.h"
#include "Misc/StringBuilder.h"
#include "Serialization/CompactBinarySerialization.h"
#include "Serialization/CompactBinaryWriter.h"
#include "Templates/Greater.h"
#endif

#if UE_SCA_VISUALIZE_SHADER_USAGE
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"
#endif // UE_SCA_VISUALIZE_SHADER_USAGE

int32 GShaderCodeLibraryAsyncLoadingPriority = int32(AIOP_Normal);
static FAutoConsoleVariableRef CVarShaderCodeLibraryAsyncLoadingPriority(
	TEXT("r.ShaderCodeLibrary.DefaultAsyncIOPriority"),
	GShaderCodeLibraryAsyncLoadingPriority,
	TEXT(""),
	ECVF_Default
);

int32 GShaderCodeLibraryAsyncLoadingAllowDontCache = 0;
static FAutoConsoleVariableRef CVarShaderCodeLibraryAsyncLoadingAllowDontCache(
	TEXT("r.ShaderCodeLibrary.AsyncIOAllowDontCache"),
	GShaderCodeLibraryAsyncLoadingAllowDontCache,
	TEXT(""),
	ECVF_Default
);

int32 GShaderCodeLibraryVisualizeShaderUsage = 0;
static FAutoConsoleVariableRef CVarShaderCodeLibraryVisualizeShaderUsage(
	TEXT("r.ShaderCodeLibrary.VisualizeShaderUsage"),
	GShaderCodeLibraryVisualizeShaderUsage,
	TEXT("If 1, a bitmap with the used shaders (for each shader library chunk) will be saved at the exit. Works in standalone games only."),
	ECVF_RenderThreadSafe | ECVF_ReadOnly
);

int32 GShaderCodeLibraryMaxShaderGroupSize = 1024 * 1024;	// decompressing 1MB of shaders takes about 0.1ms on PC (TR 3970x, Oodle Mermaid6).
static FAutoConsoleVariableRef CVarShaderCodeLibraryMaxShaderGroupSize(
	TEXT("r.ShaderCodeLibrary.MaxShaderGroupSize"),
	GShaderCodeLibraryMaxShaderGroupSize,
	TEXT("Max (uncompressed) size of a group of shaders to be compressed/decompressed together.")
	TEXT("If a group exceeds it, it will be evenly split into subgroups which strive to not exceed it. However, if a shader group is down to one shader and still exceeds the limit, the limit will be ignored."),
	ECVF_RenderThreadSafe | ECVF_ReadOnly
);

float GShaderCodeLibraryMaxShaderPreloadWaitTime = 0.001f;
static FAutoConsoleVariableRef CVarShaderCodeLibraryMaxShaderPreloadWaitTime(
	TEXT("r.ShaderCodeLibrary.MaxShaderPreloadWaitTime"),
	GShaderCodeLibraryMaxShaderPreloadWaitTime,
	TEXT("If we wait on shader preloads longer than this amount of seconds, we will log it as a warning."),
	ECVF_RenderThreadSafe | ECVF_ReadOnly
);

#if RHI_RAYTRACING	// this function is only needed to check if we need to avoid excluding raytracing shaders
namespace
{
	bool IsCreateShadersOnLoadEnabled()
	{
		static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.CreateShadersOnLoad"));
		return CVar && CVar->GetInt() != 0;
	}
}
#endif // RHI_RAYTRACING

int32 FSerializedShaderArchive::FindShaderMapWithKey(const FSHAHash& Hash, uint32 Key) const
{
	for (uint32 Index = ShaderMapHashTable.First(Key); ShaderMapHashTable.IsValid(Index); Index = ShaderMapHashTable.Next(Index))
	{
		if (ShaderMapHashes[Index] == Hash)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

int32 FSerializedShaderArchive::FindShaderMap(const FSHAHash& Hash) const
{
	const uint32 Key = GetTypeHash(Hash);
	return FindShaderMapWithKey(Hash, Key);
}

bool FSerializedShaderArchive::FindOrAddShaderMap(const FSHAHash& Hash, int32& OutIndex, const FShaderMapAssetPaths* AssociatedAssets)
{
	const uint32 Key = GetTypeHash(Hash);
	int32 Index = FindShaderMapWithKey(Hash, Key);
	bool bAdded = Index == INDEX_NONE;
	if (bAdded)
	{
		Index = ShaderMapHashes.Add(Hash);
		ShaderMapEntries.AddDefaulted();
		check(ShaderMapEntries.Num() == ShaderMapHashes.Num());
		ShaderMapHashTable.Add(Key, Index);
	}
#if WITH_EDITOR
	if (AssociatedAssets && AssociatedAssets->Num())
	{
		ShaderCodeToAssets.FindOrAdd(Hash).Append(*AssociatedAssets);
	}
#endif


	OutIndex = Index;
	return bAdded;
}

int32 FSerializedShaderArchive::FindShaderWithKey(const FSHAHash& Hash, uint32 Key) const
{
	for (uint32 Index = ShaderHashTable.First(Key); ShaderHashTable.IsValid(Index); Index = ShaderHashTable.Next(Index))
	{
		if (ShaderHashes[Index] == Hash)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

int32 FSerializedShaderArchive::FindShader(const FSHAHash& Hash) const
{
	const uint32 Key = GetTypeHash(Hash);
	return FindShaderWithKey(Hash, Key);
}

bool FSerializedShaderArchive::FindOrAddShader(const FSHAHash& Hash, int32& OutIndex)
{
	const uint32 Key = GetTypeHash(Hash);
	OutIndex = FindShaderWithKey(Hash, Key);
	if (OutIndex == INDEX_NONE)
	{
		OutIndex = ShaderHashes.Add(Hash);
		ShaderEntries.AddDefaulted();
		check(ShaderEntries.Num() == ShaderHashes.Num());
		ShaderHashTable.Add(Key, OutIndex);
		return true;
	}

	return false;
}

void FSerializedShaderArchive::RemoveLastAddedShader()
{
	check(!ShaderEntries.IsEmpty() && ShaderEntries.Num() == ShaderHashes.Num());
	int32 ShaderIndex = ShaderEntries.Num() - 1;
	const uint32 Key = GetTypeHash(ShaderHashes[ShaderIndex]);
	ShaderHashTable.Remove(Key, ShaderIndex);
	ShaderHashes.RemoveAt(ShaderIndex);
	ShaderEntries.RemoveAt(ShaderIndex);
}

#if WITH_EDITOR
FCbWriter& operator<<(FCbWriter& Writer, const FSerializedShaderArchive& Archive)
{
	TArray64<uint8> SerializedBytes;
	{
		FMemoryWriter64 SerializeArchive(SerializedBytes);
		const_cast<FSerializedShaderArchive&>(Archive).Serialize(SerializeArchive);
	}

	Writer.BeginObject();
	{
		Writer << "SerializedBytes";
		Writer.AddBinary(FMemoryView(SerializedBytes.GetData(), SerializedBytes.Num()));
		SerializedBytes.Empty();

		// Serialize is meant for runtime fields only, so copy the editor-only fields separately
		Writer.BeginArray("ShaderCodeToAssets");
		for (const TPair<FSHAHash, FShaderMapAssetPaths>& Pair : Archive.ShaderCodeToAssets)
		{
			Writer << Pair.Key;
			Writer.BeginArray();
			for (FName AssetName : Pair.Value)
			{
				Writer << AssetName;
			}
			Writer.EndArray();
		}
		Writer.EndArray();
	}
	Writer.EndObject();
	return Writer;
}

bool LoadFromCompactBinary(FCbFieldView Field, FSerializedShaderArchive& OutArchive)
{
	FMemoryView SerializedBytes = Field["SerializedBytes"].AsBinaryView();
	{
		FMemoryReaderView SerializeArchive(SerializedBytes);
		OutArchive.Serialize(SerializeArchive);
		if (SerializeArchive.IsError())
		{
			OutArchive = FSerializedShaderArchive();
			return false;
		}
	}

	FCbFieldView ShaderCodeToAssetsField = Field["ShaderCodeToAssets"];
	// Map size is array size divided by two because pairs are written as successive elements
	int32 NumShaderCodeToAssets = ShaderCodeToAssetsField.AsArrayView().Num()/2;
	bool bOk = !ShaderCodeToAssetsField.HasError();
	OutArchive.ShaderCodeToAssets.Empty(NumShaderCodeToAssets);
	FCbFieldViewIterator It = ShaderCodeToAssetsField.CreateViewIterator();
	while (It)
	{
		FSHAHash ShaderMapHash;
		if (!LoadFromCompactBinary(*It++, ShaderMapHash))
		{
			bOk = false;
			continue;
		}
		FShaderMapAssetPaths& Paths = OutArchive.ShaderCodeToAssets.FindOrAdd(ShaderMapHash);
		FCbFieldView AssetNameArrayField = *It++;
		Paths.Reserve((*It).AsArrayView().Num());
		bOk = (!AssetNameArrayField.HasError()) & bOk;
		for (FCbFieldView AssetNameField : AssetNameArrayField)
		{
			FName AssetName;
			bOk = LoadFromCompactBinary(AssetNameField, AssetName) & bOk;
			Paths.Add(AssetName);
		}
	}

	return bOk;
}
#endif


#if UE_SCA_VISUALIZE_SHADER_USAGE
void FShaderUsageVisualizer::Initialize(const int32 InNumShaders)
{
	FScopeLock Lock(&VisualizeLock);
	NumShaders = InNumShaders;
}

void FShaderUsageVisualizer::SaveShaderUsageBitmap(const FString& Name, EShaderPlatform ShaderPlatform)
{
	if (GShaderCodeLibraryVisualizeShaderUsage)
	{
		if (NumShaders)
		{
			if (IImageWrapperModule* ImageWrapperModule = FModuleManager::Get().GetModulePtr<IImageWrapperModule>(TEXT("ImageWrapper")))
			{
				if (TSharedPtr<IImageWrapper> PNGImageWrapper = ImageWrapperModule->CreateImageWrapper(EImageFormat::PNG))
				{
					UE_LOG(LogShaderLibrary, Log, TEXT("Creating shader usage bitmap for archive %s (NumShaders: %d, preloaded %d, created %d)"), *Name, NumShaders, PreloadedShaders.Num(), CreatedShaders.Num());

					// find a value close to sqrt(NumShaders)
					int32 ImageDimension = static_cast<int32>(FMath::Sqrt(static_cast<float>(NumShaders))) + 1;
					TArray<FColor> ShaderUsageBitmap;
					ShaderUsageBitmap.Reserve(ImageDimension * ImageDimension);

					// map legend:
					FColor UnusedShaderColor(128, 128, 128);	// unused shaders - this is the majority of the bitmap content
					FColor PreloadedShaderColor(192, 192, 192);	// preloaded shaders - including those that weren't explicitly so, but they happened to be grouped with shaders we needed
					FColor ExplicitlyPreloadedShaderColor(0, 255, 0);	// shaders we explicitly wanted to preload - they can become the majority under certain circumstances. Pure white can blend with some viewer's background
					FColor PreloadedAndDecompressedShaderColor(0, 0, 255);	// shaders that we wanted to preload and that got decompressed (as part of the creating them or their neighbor in group)
					FColor NotPreloadedButDecompressedShaderColor(255, 0, 0);	// shaders that we decompressed just because they were grouped together with others. We did not want to preload them at all.
					FColor CreatedShaderColor(255, 255, 255);	// created shaders - in practice, always few and far between. Blue is more noticeable on a largely bright background than magenta

					for (int32 Idx = 0; Idx < NumShaders; ++Idx)
					{
						ShaderUsageBitmap.Add(UnusedShaderColor);
					}
					// the rest can be zero/transparent
					ShaderUsageBitmap.AddZeroed(ImageDimension * ImageDimension - NumShaders);
					check(ShaderUsageBitmap.Num() == ImageDimension * ImageDimension);

					{
						// in case this ever gets called runtime
						FScopeLock Lock(&VisualizeLock);

						// fill preloaded ones first
						for (int32 ShaderIdx : PreloadedShaders)
						{
							ShaderUsageBitmap[ShaderIdx] = PreloadedShaderColor;
						}

						// explicitly preloaded shaders
						for (int32 ShaderIdx : ExplicitlyPreloadedShaders)
						{
							ShaderUsageBitmap[ShaderIdx] = ExplicitlyPreloadedShaderColor;
						}

						// fill decompressed ones, but mark up those that we didn't ask to preload differently
						for (int32 ShaderIdx : DecompressedShaders)
						{
							bool bShaderWasRequestedToBePreloaded = ExplicitlyPreloadedShaders.Contains(ShaderIdx);
							ShaderUsageBitmap[ShaderIdx] = bShaderWasRequestedToBePreloaded ? PreloadedAndDecompressedShaderColor : NotPreloadedButDecompressedShaderColor;
						}

						for (int32 ShaderIdx : CreatedShaders)
						{
							ShaderUsageBitmap[ShaderIdx] = CreatedShaderColor;
						}
					}

					bool bSet = PNGImageWrapper->SetRaw(ShaderUsageBitmap.GetData(), ShaderUsageBitmap.Num() * sizeof(FColor),
						ImageDimension, ImageDimension, ERGBFormat::BGRA, 8);

					if (bSet)
					{
						TArray64<uint8> CompressedData = PNGImageWrapper->GetCompressed(100);

						const FString Filename = FString::Printf(TEXT("%s_%s_RuntimeShaderUsage_%s.png"), *Name, *LexToString(ShaderPlatform), *FDateTime::Now().ToString());
						const FString SaveDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Profiling"));
						const FString FilePath = FPaths::Combine(SaveDir, Filename);

						if (!FFileHelper::SaveArrayToFile(CompressedData, *FilePath))
						{
							UE_LOG(LogShaderLibrary, Warning, TEXT("Couldn't write shader usage bitmap %s"), *FilePath);
						}
						else
						{
							UE_LOG(LogShaderLibrary, Log, TEXT("Saved shader usage bitmap %s. Legend: shaders not loaded from disk - dark grey, loaded to RAM explicitly - green, loaded as part of their compressed group - bright grey, decompressed: blue if they were loaded explicitly, red if just because they were a part of the group. Actually created shaders - white"), 
								*FilePath);
						}
					}
					else
					{
						UE_LOG(LogShaderLibrary, Warning, TEXT("Error creating shader usage bitmap for archive %s (NumShaders: %d, preloaded %d, created %d) - cannot create a PNG image"), *Name, NumShaders, PreloadedShaders.Num(), CreatedShaders.Num());
					}
				}
				else
				{
					UE_LOG(LogShaderLibrary, Warning, TEXT("Couldn't create shader usage bitmap for archive %s (NumShaders: %d, preloaded %d, created %d) - cannot create ImageWrapper for PNG format"), *Name, NumShaders, PreloadedShaders.Num(), CreatedShaders.Num());
				}
			}
			else
			{
				UE_LOG(LogShaderLibrary, Warning, TEXT("Couldn't create shader usage bitmap for archive %s (NumShaders: %d, preloaded %d, created %d) - no ImageWrapper module"), *Name, NumShaders, PreloadedShaders.Num(), CreatedShaders.Num());
			}
		}
	}
}
#endif


static FString GetDecompressionFailureExtraMessage()
{
	FString Brand = FPlatformMisc::GetCPUBrand();

	// most common offender is 13900K
	//	also seems to be a problem on 14900K and maybe 12900K
	//	not sure if 700K is affected
	// don't try to filter the brand string, just always log it if we hit a fatal error
	#if 0
	bool Is900K =
	 ( Brand.Find(TEXT("900K")) != INDEX_NONE ) ||
	 ( Brand.Find(TEXT("900-K")) != INDEX_NONE ) ||
	 ( Brand.Find(TEXT("900 K")) != INDEX_NONE );

	if ( ! Is900K )
	{
		return FString();
	}
	#endif

	return FString::Printf(TEXT("The CPU (%s) may be unstable; for details see http://www.radgametools.com/oodleintel.htm"),*Brand);
}

void ShaderCodeArchive::DecompressShaderWithOodle(uint8* OutDecompressedShader, int64 UncompressedSize, const uint8* CompressedShaderCode, int64 CompressedSize)
{
	// Iostore always compresses with Oodle.
	bool bSucceed = FCompression::UncompressMemory(NAME_Oodle, OutDecompressedShader, UncompressedSize, CompressedShaderCode, CompressedSize);
	if (!bSucceed)
	{
		UE_LOG(LogShaderLibrary, Fatal, TEXT("ShaderCodeArchive::DecompressShader(): Could not decompress shader with Oodle. %s"),
			*GetDecompressionFailureExtraMessage());
	}
}

namespace
{
	void DecompressShadergroupWithOodleAndExtraLogging(
			const int32 GroupIndex, const FIoChunkId& GroupHash, FIoStoreShaderGroupEntry& Entry, const int32 ShaderIndex, const uint64 ShaderInGroupIndex, const FSHAHash& ShaderHash, 
			uint8* OutDecompressedShader, int64 UncompressedSize, const uint8* CompressedShaderCode, int64 CompressedSize)
	{
		bool bSucceed = FCompression::UncompressMemory(NAME_Oodle, OutDecompressedShader, UncompressedSize, CompressedShaderCode, CompressedSize);
		if (!bSucceed)
		{
			UE_LOG(LogShaderLibrary, Fatal, TEXT("DecompressShaderWithOodleAndExtraLogging(): Could not decompress shader group with Oodle. Group Index: %d Group IoStoreHash:%s Group NumShaders: %d Shader Index: %d Shader In-group Index: %d Shader Hash: %s. %s"),
				GroupIndex,
				*LexToString(GroupHash),
				Entry.NumShaders,
				ShaderIndex,
				ShaderInGroupIndex,
				*LexToString(ShaderHash),
				*GetDecompressionFailureExtraMessage()
				);
		}
	}
}

bool ShaderCodeArchive::CompressShaderWithOodle(uint8* OutCompressedShader, int64& OutCompressedSize, const uint8* InUncompressedShaderCode, int64 InUncompressedSize, FOodleDataCompression::ECompressor InOodleCompressor, FOodleDataCompression::ECompressionLevel InOodleLevel)
{
	if (OutCompressedShader)
	{
		OutCompressedSize = FOodleDataCompression::Compress(OutCompressedShader, OutCompressedSize, InUncompressedShaderCode, InUncompressedSize, InOodleCompressor, InOodleLevel);
		check(OutCompressedSize != 0);
		return OutCompressedSize != 0;
	}
	else
	{
		// Just requesting an estimate.
		OutCompressedSize = FOodleDataCompression::CompressedBufferSizeNeeded(InUncompressedSize);
		return false;
	}	
}

void FSerializedShaderArchive::DecompressShader(int32 Index, const TArray<TArray<uint8>>& ShaderCode, TArray<uint8>& OutDecompressedShader) const
{
	const FShaderCodeEntry& Entry = ShaderEntries[Index];
	OutDecompressedShader.SetNum(Entry.UncompressedSize, EAllowShrinking::No);
	if (Entry.Size == Entry.UncompressedSize)
	{
		FMemory::Memcpy(OutDecompressedShader.GetData(), ShaderCode[Index].GetData(), Entry.UncompressedSize);
	}
	else
	{
		ShaderCodeArchive::DecompressShaderWithOodle(OutDecompressedShader.GetData(), Entry.UncompressedSize, ShaderCode[Index].GetData(), Entry.Size);
	}
}

void FSerializedShaderArchive::Finalize()
{
	// Set the correct offsets
	{
		uint64 Offset = 0u;
		for (FShaderCodeEntry& Entry : ShaderEntries)
		{
			Entry.Offset = Offset;
			Offset += Entry.Size;
		}
	}

	constexpr int32 MaxByteGapAllowedInAPreload = 1024;
	PreloadEntries.Empty();
	for (FShaderMapEntry& ShaderMapEntry : ShaderMapEntries)
	{
		check(ShaderMapEntry.NumShaders > 0u);
		TArray<FFileCachePreloadEntry> SortedPreloadEntries;
		SortedPreloadEntries.Empty(ShaderMapEntry.NumShaders + 1);
		for (uint32 i = 0; i < ShaderMapEntry.NumShaders; ++i)
		{
			const int32 ShaderIndex = ShaderIndices[ShaderMapEntry.ShaderIndicesOffset + i];
			const FShaderCodeEntry& ShaderEntry = ShaderEntries[ShaderIndex];
			SortedPreloadEntries.Add(FFileCachePreloadEntry(ShaderEntry.Offset, ShaderEntry.Size));
		}
		SortedPreloadEntries.Sort([](const FFileCachePreloadEntry& Lhs, const FFileCachePreloadEntry& Rhs) { return Lhs.Offset < Rhs.Offset; });
		SortedPreloadEntries.Add(FFileCachePreloadEntry(INT64_MAX, 0));

		ShaderMapEntry.FirstPreloadIndex = PreloadEntries.Num();
		FFileCachePreloadEntry CurrentPreloadEntry = SortedPreloadEntries[0];
		for (uint32 PreloadIndex = 1; PreloadIndex <= ShaderMapEntry.NumShaders; ++PreloadIndex)
		{
			const FFileCachePreloadEntry& PreloadEntry = SortedPreloadEntries[PreloadIndex];
			const int64 Gap = PreloadEntry.Offset - CurrentPreloadEntry.Offset - CurrentPreloadEntry.Size;
			checkf(Gap >= 0, TEXT("Overlapping preload entries, [%lld-%lld), [%lld-%lld)"),
				CurrentPreloadEntry.Offset, CurrentPreloadEntry.Offset + CurrentPreloadEntry.Size, PreloadEntry.Offset, PreloadEntry.Offset + PreloadEntry.Size);
			if (Gap > MaxByteGapAllowedInAPreload)
			{
				++ShaderMapEntry.NumPreloadEntries;
				PreloadEntries.Add(CurrentPreloadEntry);
				CurrentPreloadEntry = PreloadEntry;
			}
			else
			{
				CurrentPreloadEntry.Size = PreloadEntry.Offset + PreloadEntry.Size - CurrentPreloadEntry.Offset;
			}
		}
		check(ShaderMapEntry.NumPreloadEntries > 0u);
		check(CurrentPreloadEntry.Size == 0);
	}
}

void FSerializedShaderArchive::Serialize(FArchive& Ar)
{
	Ar << ShaderMapHashes;
	Ar << ShaderHashes;
	Ar << ShaderMapEntries;
	Ar << ShaderEntries;
	Ar << PreloadEntries;
	Ar << ShaderIndices;

	check(ShaderHashes.Num() == ShaderEntries.Num());
	check(ShaderMapHashes.Num() == ShaderMapEntries.Num());

	if (Ar.IsLoading())
	{
		{
			const uint32 HashSize = FMath::Min<uint32>(0x10000, 1u << FMath::CeilLogTwo(ShaderMapHashes.Num()));
			ShaderMapHashTable.Clear(HashSize, ShaderMapHashes.Num());
			for (int32 Index = 0; Index < ShaderMapHashes.Num(); ++Index)
			{
				const uint32 Key = GetTypeHash(ShaderMapHashes[Index]);
				ShaderMapHashTable.Add(Key, Index);
			}
		}
		{
			const uint32 HashSize = FMath::Min<uint32>(0x10000, 1u << FMath::CeilLogTwo(ShaderHashes.Num()));
			ShaderHashTable.Clear(HashSize, ShaderHashes.Num());
			for (int32 Index = 0; Index < ShaderHashes.Num(); ++Index)
			{
				const uint32 Key = GetTypeHash(ShaderHashes[Index]);
				ShaderHashTable.Add(Key, Index);
			}
		}
	}
}

#if WITH_EDITOR
void FSerializedShaderArchive::SaveAssetInfo(FArchive& Ar)
{
	if (Ar.IsSaving())
	{
		FString JsonTcharText;
		{
			TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&JsonTcharText);
			Writer->WriteObjectStart();

			Writer->WriteValue(TEXT("AssetInfoVersion"), static_cast<int32>(EAssetInfoVersion::CurrentVersion));

			TArray<const TPair<FSHAHash, FShaderMapAssetPaths>*> SortedData;
			SortedData.Reserve(ShaderCodeToAssets.Num());
			for (TPair<FSHAHash, FShaderMapAssetPaths>& Pair : ShaderCodeToAssets)
			{
				SortedData.Add(&Pair);
				Pair.Value.Sort(FNameLexicalLess());
			}
			Algo::Sort(SortedData, [](const TPair<FSHAHash, FShaderMapAssetPaths>* const a, const TPair<FSHAHash, FShaderMapAssetPaths>* const b)
				{
					return GetTypeHash(a->Key) < GetTypeHash(b->Key);
				});

			Writer->WriteArrayStart(TEXT("ShaderCodeToAssets"));
			for (const TPair<FSHAHash, FShaderMapAssetPaths>* Pair : SortedData)
			{
				Writer->WriteObjectStart();
				const FSHAHash& Hash = Pair->Key;
				Writer->WriteValue(TEXT("ShaderMapHash"), Hash.ToString());
				const FShaderMapAssetPaths& Assets = Pair->Value;

				Writer->WriteArrayStart(TEXT("Assets"));
				for (FShaderMapAssetPaths::TConstIterator AssetIter(Assets); AssetIter; ++AssetIter)
				{
					Writer->WriteValue((*AssetIter).ToString());
				}
				Writer->WriteArrayEnd();
				Writer->WriteObjectEnd();
			}
			Writer->WriteArrayEnd();

			Writer->WriteObjectEnd();
			Writer->Close();
		}

		FTCHARToUTF8 JsonUtf8(*JsonTcharText);
		Ar.Serialize(const_cast<void *>(reinterpret_cast<const void*>(JsonUtf8.Get())), JsonUtf8.Length() * sizeof(UTF8CHAR));
	}
}

bool FSerializedShaderArchive::LoadAssetInfo(const FString& Filename)
{
	TArray<uint8> FileData;
	if (!FFileHelper::LoadFileToArray(FileData, *Filename))
	{
		return false;
	}

	FString JsonText;
	FFileHelper::BufferToString(JsonText, FileData.GetData(), FileData.Num());

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(JsonText);

	// Attempt to deserialize JSON
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		return false;
	}

	TSharedPtr<FJsonValue> AssetInfoVersion = JsonObject->Values.FindRef(TEXT("AssetInfoVersion"));
	if (!AssetInfoVersion.IsValid())
	{
		UE_LOG(LogShaderLibrary, Warning, TEXT("Rejecting asset info file %s: missing AssetInfoVersion (damaged file?)"), 
			*Filename);
		return false;
	}
	
	const EAssetInfoVersion FileVersion = static_cast<EAssetInfoVersion>(static_cast<int64>(AssetInfoVersion->AsNumber()));
	if (FileVersion != EAssetInfoVersion::CurrentVersion)
	{
		UE_LOG(LogShaderLibrary, Warning, TEXT("Rejecting asset info file %s: expected version %d, got unsupported version %d."),
			*Filename, static_cast<int32>(EAssetInfoVersion::CurrentVersion), static_cast<int32>(FileVersion));
		return false;
	}

	TSharedPtr<FJsonValue> AssetInfoArrayValue = JsonObject->Values.FindRef(TEXT("ShaderCodeToAssets"));
	if (!AssetInfoArrayValue.IsValid())
	{
		UE_LOG(LogShaderLibrary, Warning, TEXT("Rejecting asset info file %s: missing ShaderCodeToAssets array (damaged file?)"),
			*Filename);
		return false;
	}
	
	TArray<TSharedPtr<FJsonValue>> AssetInfoArray = AssetInfoArrayValue->AsArray();
	UE_LOG(LogShaderLibrary, Display, TEXT("Reading asset info file %s: found %d existing mappings"),
		*Filename, AssetInfoArray.Num());

	for (int32 IdxPair = 0, NumPairs = AssetInfoArray.Num(); IdxPair < NumPairs; ++IdxPair)
	{
		TSharedPtr<FJsonObject> Pair = AssetInfoArray[IdxPair]->AsObject();
		if (UNLIKELY(!Pair.IsValid()))
		{
			UE_LOG(LogShaderLibrary, Warning, TEXT("Rejecting asset info file %s: ShaderCodeToAssets array contains unreadable mapping #%d (damaged file?)"),
				*Filename,
				IdxPair
				);
			return false;
		}

		TSharedPtr<FJsonValue> ShaderMapHashJson = Pair->Values.FindRef(TEXT("ShaderMapHash"));
		if (UNLIKELY(!ShaderMapHashJson.IsValid()))
		{
			UE_LOG(LogShaderLibrary, Warning, TEXT("Rejecting asset info file %s: ShaderCodeToAssets array contains unreadable ShaderMapHash for mapping %d (damaged file?)"),
				*Filename,
				IdxPair
				);
			return false;
		}

		FSHAHash ShaderMapHash;
		ShaderMapHash.FromString(ShaderMapHashJson->AsString());

		TSharedPtr<FJsonValue> AssetPathsArrayValue = Pair->Values.FindRef(TEXT("Assets"));
		if (UNLIKELY(!AssetPathsArrayValue.IsValid()))
		{
			UE_LOG(LogShaderLibrary, Warning, TEXT("Rejecting asset info file %s: ShaderCodeToAssets array contains unreadable Assets array for mapping %d (damaged file?)"),
				*Filename,
				IdxPair
			);
			return false;
		}
			
		FShaderMapAssetPaths Paths;
		TArray<TSharedPtr<FJsonValue>> AssetPathsArray = AssetPathsArrayValue->AsArray();
		for (int32 IdxAsset = 0, NumAssets = AssetPathsArray.Num(); IdxAsset < NumAssets; ++IdxAsset)
		{
			Paths.Add(FName(*AssetPathsArray[IdxAsset]->AsString()));
		}

		ShaderCodeToAssets.Add(ShaderMapHash, Paths);
	}

	return true;
}

void FSerializedShaderArchive::CreateAsChunkFrom(const FSerializedShaderArchive& Parent, const TSet<FName>& PackagesInChunk, TArray<int32>& OutShaderCodeEntriesNeeded)
{
	// we should begin with a clean slate
	checkf(ShaderMapHashes.Num() == 0 && ShaderHashes.Num() == 0 && ShaderMapEntries.Num() == 0 && ShaderEntries.Num() == 0 && PreloadEntries.Num() == 0 && ShaderIndices.Num() == 0,
		TEXT("Expecting a new, uninitialized FSerializedShaderArchive instance for creating a chunk."));

	// go through parent's shadermap hashes in the order of their addition
	for (int32 IdxSM = 0, NumSMs = Parent.ShaderMapHashes.Num(); IdxSM < NumSMs; ++IdxSM)
	{
		const FSHAHash& ShaderMapHash = Parent.ShaderMapHashes[IdxSM];
		const FShaderMapAssetPaths* Assets = Parent.ShaderCodeToAssets.Find(ShaderMapHash);
		bool bIncludeSM = false;
		if (UNLIKELY(Assets == nullptr))
		{
			UE_LOG(LogShaderLibrary, Warning, TEXT("Shadermap %s is not associated with any asset. Including it in every chunk"), *ShaderMapHash.ToString());
			bIncludeSM = true;
		}
		else
		{
			// if any asset is in the chunk, include
			for (const FName& Asset : *Assets)
			{
				if (PackagesInChunk.Contains(Asset))
				{
					bIncludeSM = true;
					break;
				}
			}
		}

		if (bIncludeSM)
		{
			// add this shader map
			int32 ShaderMapIndex = INDEX_NONE;
			if (FindOrAddShaderMap(ShaderMapHash, ShaderMapIndex, Assets))
			{
				// if we're in this scope, it means it's a new shadermap for the chunk and we need more information about it from the parent
				int32 ParentShaderMapIndex = IdxSM;
				const FShaderMapEntry& ParentShaderMapDescriptor = Parent.ShaderMapEntries[ParentShaderMapIndex];

				const int32 NumShaders = ParentShaderMapDescriptor.NumShaders;

				FShaderMapEntry& ShaderMapDescriptor = ShaderMapEntries[ShaderMapIndex];
				ShaderMapDescriptor.NumShaders = NumShaders;
				ShaderMapDescriptor.ShaderIndicesOffset = ShaderIndices.AddZeroed(NumShaders);

				// add shader by shader
				for (int32 ShaderIdx = 0; ShaderIdx < NumShaders; ++ShaderIdx)
				{
					int32 ParentShaderIndex = Parent.ShaderIndices[ParentShaderMapDescriptor.ShaderIndicesOffset + ShaderIdx];

					int32 ShaderIndex = INDEX_NONE;
					if (FindOrAddShader(Parent.ShaderHashes[ParentShaderIndex], ShaderIndex))
					{
						// new shader! add it to the mapping of parent shadercode entries to ours. and check the integrity of the mapping
						checkf(OutShaderCodeEntriesNeeded.Num() == ShaderIndex, TEXT("Mapping between the shader indices in a chunk and the whole archive is inconsistent"));
						OutShaderCodeEntriesNeeded.Add(ParentShaderIndex);

						// copy the entry as is
						ShaderEntries[ShaderIndex] = Parent.ShaderEntries[ParentShaderIndex];
					}
					ShaderIndices[ShaderMapDescriptor.ShaderIndicesOffset + ShaderIdx] = ShaderIndex;
				}
			}
		}
	}
}

void FSerializedShaderArchive::CollectStatsAndDebugInfo(FDebugStats& OutDebugStats, FExtendedDebugStats* OutExtendedDebugStats)
{
	// collect the light-weight stats first
	FMemory::Memzero(OutDebugStats);
	OutDebugStats.NumUniqueShaders = ShaderHashes.Num();
	OutDebugStats.NumShaderMaps = ShaderMapHashes.Num();
	int32 TotalShaders = 0;
	int64 TotalShaderSize = 0;
	uint32 MinSMSizeInShaders = UINT_MAX;
	uint32 MaxSMSizeInShaders = 0;
	for (const FShaderMapEntry& SMEntry : ShaderMapEntries)
	{
		MinSMSizeInShaders = FMath::Min(MinSMSizeInShaders, SMEntry.NumShaders);
		MaxSMSizeInShaders = FMath::Max(MaxSMSizeInShaders, SMEntry.NumShaders);
		TotalShaders += SMEntry.NumShaders;

		const int32 ThisSMShaders = SMEntry.NumShaders;
		for (int32 ShaderIdx = 0; ShaderIdx < ThisSMShaders; ++ShaderIdx)
		{
			TotalShaderSize += ShaderEntries[ShaderIndices[SMEntry.ShaderIndicesOffset + ShaderIdx]].Size;
		}
	}
	OutDebugStats.NumShaders = TotalShaders;
	OutDebugStats.ShadersSize = TotalShaderSize;

	// this is moderately expensive, consider moving to ExtendedStats?
	{
		TSet<FName> AllAssets;
		for (TMap<FSHAHash, FShaderMapAssetPaths>::TConstIterator Iter(ShaderCodeToAssets); Iter; ++Iter)
		{
			for (const FName& AssetName : Iter.Value())
			{
				AllAssets.Add(AssetName);
			}
		}
		OutDebugStats.NumAssets = AllAssets.Num();
	}

	int64 ActuallySavedShaderSize = 0;
	for (const FShaderCodeEntry& ShaderEntry : ShaderEntries)
	{
		ActuallySavedShaderSize += ShaderEntry.Size;
	}
	OutDebugStats.ShadersUniqueSize = ActuallySavedShaderSize;

	// If OutExtendedDebugStats pointer is passed, we're asked to fill out a heavy-weight stats.
	if (OutExtendedDebugStats)
	{
		// textual rep
		DumpContentsInPlaintext(OutExtendedDebugStats->TextualRepresentation);

		OutExtendedDebugStats->MinNumberOfShadersPerSM = MinSMSizeInShaders;
		OutExtendedDebugStats->MaxNumberofShadersPerSM = MaxSMSizeInShaders;

		// median SM size in shaders
		TArray<int32> ShadersInSM;

		// shader usage
		TMap<int32, int32> ShaderToUsageMap;

		for (const FShaderMapEntry& SMEntry : ShaderMapEntries)
		{
			const int32 ThisSMShaders = SMEntry.NumShaders;
			ShadersInSM.Add(ThisSMShaders);

			for (int32 ShaderIdx = 0; ShaderIdx < ThisSMShaders; ++ShaderIdx)
			{
				int ShaderIndex = ShaderIndices[SMEntry.ShaderIndicesOffset + ShaderIdx];
				int32& Usage = ShaderToUsageMap.FindOrAdd(ShaderIndex, 0);
				++Usage;
			}
		}

		ShadersInSM.Sort();
		OutExtendedDebugStats->MedianNumberOfShadersPerSM = ShadersInSM.Num() ? ShadersInSM[ShadersInSM.Num() / 2] : 0;

		ShaderToUsageMap.ValueSort(TGreater<int32>());
		// add top 10 shaders
		for (const TTuple<int32, int32>& UsagePair : ShaderToUsageMap)
		{
			OutExtendedDebugStats->TopShaderUsages.Add(UsagePair.Value);
			if (OutExtendedDebugStats->TopShaderUsages.Num() >= 10)
			{
				break;
			}
		}

		// calculate per-frequency stats
		FMemory::Memzero(OutExtendedDebugStats->NumShadersPerFrequency);
		FMemory::Memzero(OutExtendedDebugStats->UncompressedSizePerFrequency);
		FMemory::Memzero(OutExtendedDebugStats->CompressedSizePerFrequency);
		for (const FShaderCodeEntry& ShaderEntry : ShaderEntries)
		{
			check(ShaderEntry.Frequency < UE_ARRAY_COUNT(OutExtendedDebugStats->NumShadersPerFrequency));
			++OutExtendedDebugStats->NumShadersPerFrequency[ShaderEntry.Frequency];
			check(ShaderEntry.Frequency < UE_ARRAY_COUNT(OutExtendedDebugStats->UncompressedSizePerFrequency));
			OutExtendedDebugStats->UncompressedSizePerFrequency[ShaderEntry.Frequency] += ShaderEntry.UncompressedSize;
			check(ShaderEntry.Frequency < UE_ARRAY_COUNT(OutExtendedDebugStats->UncompressedSizePerFrequency));
			OutExtendedDebugStats->CompressedSizePerFrequency[ShaderEntry.Frequency] += ShaderEntry.Size;
		}
	}

#if 0 // graph visualization - maybe one day we'll return to this
		// enumerate all shaders first (so they can be identified by people looking them up in other debug output)
		int32 IdxShaderNum = 0;
		for (const FSHAHash& ShaderHash : ShaderHashes)
		{
			FString Numeral = FString::Printf(TEXT("Shd_%d"), IdxShaderNum);
			OutRelationshipGraph->Add(TTuple<FString, FString>(Numeral, FString("Hash_") + ShaderHash.ToString()));
			++IdxShaderNum;
		}

		// add all assets if any
		for (TMap<FName, FSHAHash>::TConstIterator Iter(AssetToShaderCode); Iter; ++Iter)
		{
			int32 SMIndex = FindShaderMap(Iter.Value());			
			OutRelationshipGraph->Add(TTuple<FString, FString>(Iter.Key().ToString(), FString::Printf(TEXT("SM_%d"), SMIndex)));
		}

		// shadermaps to shaders
		int NumSMs = ShaderMapHashes.Num();
		for (int32 IdxSM = 0; IdxSM < NumSMs; ++IdxSM)
		{
			FString SMId = FString::Printf(TEXT("SM_%d"), IdxSM);
			const FShaderMapEntry& SMEntry = ShaderMapEntries[IdxSM];

			const int32 ThisSMShaders = SMEntry.NumShaders;
			for (int32 ShaderIdx = 0; ShaderIdx < ThisSMShaders; ++ShaderIdx)
			{
				FString ReferencedShader = FString::Printf(TEXT("Shd_%d"), ShaderIndices[SMEntry.ShaderIndicesOffset + ShaderIdx]);
				OutRelationshipGraph->Add(TTuple<FString, FString>(SMId, ReferencedShader));
			}
		}
#endif // 0
}

void FSerializedShaderArchive::DumpContentsInPlaintext(FString& OutText) const
{
	TStringBuilder<256> Out;
	Out << TEXT("FSerializedShaderArchive\n{\n");
	{
		Out << TEXT("\tShaderMapHashes\n\t{\n");
		for (int32 IdxMapHash = 0, NumMapHashes = ShaderMapHashes.Num(); IdxMapHash < NumMapHashes; ++IdxMapHash)
		{
			Out << TEXT("\t\t");
			Out << ShaderMapHashes[IdxMapHash].ToString();
			Out << TEXT("\n");
		}
		Out << TEXT("\t}\n");
	}

	{
		Out << TEXT("\tShaderHashes\n\t{\n");
		for (int32 IdxHash = 0, NumHashes = ShaderHashes.Num(); IdxHash < NumHashes; ++IdxHash)
		{
			Out << TEXT("\t\t");
			Out << ShaderHashes[IdxHash].ToString();
			Out << TEXT("\n");
		}
		Out << TEXT("\t}\n");
	}

	{
		Out << TEXT("\tShaderMapEntries\n\t{\n");
		for (int32 IdxEntry = 0, NumEntries = ShaderMapEntries.Num(); IdxEntry < NumEntries; ++IdxEntry)
		{
			Out << TEXT("\t\tFShaderMapEntry\n\t\t{\n");

			Out << TEXT("\t\t\tShaderIndicesOffset : ");
			Out << ShaderMapEntries[IdxEntry].ShaderIndicesOffset;
			Out << TEXT("\n");

			Out << TEXT("\t\t\tNumShaders : ");
			Out << ShaderMapEntries[IdxEntry].NumShaders;
			Out << TEXT("\n");

			Out << TEXT("\t\t\tFirstPreloadIndex : ");
			Out << ShaderMapEntries[IdxEntry].FirstPreloadIndex;
			Out << TEXT("\n");

			Out << TEXT("\t\t\tNumPreloadEntries : ");
			Out << ShaderMapEntries[IdxEntry].NumPreloadEntries;
			Out << TEXT("\n");

			Out << TEXT("\t\t}\n");
		}
		Out << TEXT("\t}\n");
	}

	{
		Out << TEXT("\tShaderEntries\n\t{\n");
		for (int32 IdxEntry = 0, NumEntries = ShaderEntries.Num(); IdxEntry < NumEntries; ++IdxEntry)
		{
			Out << TEXT("\t\tFShaderCodeEntry\n\t\t{\n");

			Out << TEXT("\t\t\tOffset : ");
			Out << ShaderEntries[IdxEntry].Offset;
			Out << TEXT("\n");

			Out << TEXT("\t\t\tSize : ");
			Out << ShaderEntries[IdxEntry].Size;
			Out << TEXT("\n");

			Out << TEXT("\t\t\tUncompressedSize : ");
			Out << ShaderEntries[IdxEntry].UncompressedSize;
			Out << TEXT("\n");

			Out << TEXT("\t\t\tFrequency : ");
			Out << ShaderEntries[IdxEntry].Frequency;
			Out << TEXT("\n");

			Out << TEXT("\t\t}\n");
		}
		Out << TEXT("\t}\n");
	}

	{
		Out << TEXT("\tPreloadEntries\n\t{\n");
		for (int32 IdxEntry = 0, NumEntries = PreloadEntries.Num(); IdxEntry < NumEntries; ++IdxEntry)
		{
			Out << TEXT("\t\tFFileCachePreloadEntry\n\t\t{\n");

			Out << TEXT("\t\t\tOffset : ");
			Out << PreloadEntries[IdxEntry].Offset;
			Out << TEXT("\n");

			Out << TEXT("\t\t\tSize : ");
			Out << PreloadEntries[IdxEntry].Size;
			Out << TEXT("\n");

			Out << TEXT("\t\t}\n");
		}
		Out << TEXT("\t}\n");
	}

	{
		Out << TEXT("\tShaderIndices\n\t{\n");
		// split it by shadermaps
		int32 IdxSMEntry = 0;
		int32 NumShadersLeftInSM = ShaderMapEntries.Num() ? ShaderMapEntries[0].NumShaders : 0;
		bool bNewSM = true;
		for (int32 IdxEntry = 0, NumEntries = ShaderIndices.Num(); IdxEntry < NumEntries; ++IdxEntry)
		{
			if (UNLIKELY(bNewSM))
			{
				Out << TEXT("\t\t");
				bNewSM = false;
			}
			else
			{
				Out << TEXT(", ");
			}
			Out << ShaderIndices[IdxEntry];

			--NumShadersLeftInSM;
			while (NumShadersLeftInSM == 0)
			{
				bNewSM = true;
				++IdxSMEntry;
				if (IdxSMEntry >= ShaderMapEntries.Num())
				{
					break;
				}
				NumShadersLeftInSM = ShaderMapEntries[IdxSMEntry].NumShaders;
			}

			if (bNewSM)
			{
				Out << TEXT("\n");
			}
		}
		Out << TEXT("\t}\n");
	}

	Out << TEXT("}\n");
	OutText = FStringView(Out);
}

#endif // WITH_EDITOR

FShaderCodeArchive* FShaderCodeArchive::Create(EShaderPlatform InPlatform, FArchive& Ar, const FString& InDestFilePath, const FString& InLibraryDir, const FString& InLibraryName)
{
	FShaderCodeArchive* Library = new FShaderCodeArchive(InPlatform, InLibraryDir, InLibraryName);
	Ar << Library->SerializedShaders;
	Library->ShaderPreloads.SetNum(Library->SerializedShaders.GetNumShaders());
	Library->LibraryCodeOffset = Ar.Tell();

	// Open library for async reads
	Library->FileCacheHandle = IFileCacheHandle::CreateFileCacheHandle(*InDestFilePath);

	Library->DebugVisualizer.Initialize(Library->SerializedShaders.ShaderEntries.Num());

	UE_LOG(LogShaderLibrary, Display, TEXT("Using %s for material shader code. Total %d unique shaders."), *InDestFilePath, Library->SerializedShaders.ShaderEntries.Num());

	INC_DWORD_STAT_BY(STAT_Shaders_ShaderResourceMemory, Library->GetSizeBytes());

	return Library;
}

FShaderCodeArchive::FShaderCodeArchive(EShaderPlatform InPlatform, const FString& InLibraryDir, const FString& InLibraryName)
	: FRHIShaderLibrary(InPlatform, InLibraryName)
	, LibraryDir(InLibraryDir)
	, LibraryCodeOffset(0)
	, FileCacheHandle(nullptr)
{
}

FShaderCodeArchive::~FShaderCodeArchive()
{
	DEC_DWORD_STAT_BY(STAT_Shaders_ShaderResourceMemory, GetSizeBytes());
	Teardown();
}

void FShaderCodeArchive::Teardown()
{
	if (FileCacheHandle)
	{
		delete FileCacheHandle;
		FileCacheHandle = nullptr;
	}

	for (int32 ShaderIndex = 0; ShaderIndex < SerializedShaders.GetNumShaders(); ++ShaderIndex)
	{
		FShaderPreloadEntry& ShaderPreloadEntry = ShaderPreloads[ShaderIndex];
		if (ShaderPreloadEntry.Code)
		{
			const FShaderCodeEntry& ShaderEntry = SerializedShaders.ShaderEntries[ShaderIndex];
			FMemory::Free(ShaderPreloadEntry.Code);
			ShaderPreloadEntry.Code = nullptr;
			DEC_DWORD_STAT_BY(STAT_Shaders_ShaderPreloadMemory, ShaderEntry.Size);
		}
	}

	DebugVisualizer.SaveShaderUsageBitmap(GetName(), GetPlatform());
}

void FShaderCodeArchive::OnShaderPreloadFinished(int32 ShaderIndex, const IMemoryReadStreamRef& PreloadData)
{
	const FShaderCodeEntry& ShaderEntry = SerializedShaders.ShaderEntries[ShaderIndex];
	PreloadData->EnsureReadNonBlocking();		// Ensure data is ready before taking the lock
	{
		FWriteScopeLock Lock(ShaderPreloadLock);
		FShaderPreloadEntry& ShaderPreloadEntry = ShaderPreloads[ShaderIndex];
		PreloadData->CopyTo(ShaderPreloadEntry.Code, 0, ShaderEntry.Size);
		ShaderPreloadEntry.PreloadEvent.SafeRelease();
	}
}

struct FPreloadShaderTask
{
	explicit FPreloadShaderTask(FShaderCodeArchive* InArchive, int32 InShaderIndex, const IMemoryReadStreamRef& InData)
		: Archive(InArchive), Data(InData), ShaderIndex(InShaderIndex)
	{}

	FShaderCodeArchive* Archive;
	IMemoryReadStreamRef Data;
	int32 ShaderIndex;

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		Archive->OnShaderPreloadFinished(ShaderIndex, Data);
		Data.SafeRelease();
	}

	FORCEINLINE static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }
	FORCEINLINE ENamedThreads::Type GetDesiredThread() { return ENamedThreads::AnyNormalThreadNormalTask; }
	FORCEINLINE TStatId GetStatId() const { return TStatId(); }
};

bool FShaderCodeArchive::PreloadShader(int32 ShaderIndex, FGraphEventArray& OutCompletionEvents)
{
	LLM_SCOPE(ELLMTag::Shaders);

	FWriteScopeLock Lock(ShaderPreloadLock);

	FShaderPreloadEntry& ShaderPreloadEntry = ShaderPreloads[ShaderIndex];
	checkf(!ShaderPreloadEntry.bNeverToBePreloaded, TEXT("We are preloading a shader that shouldn't be preloaded in this run (e.g. raytracing shader on D3D11)."));
	const uint32 ShaderNumRefs = ShaderPreloadEntry.NumRefs++;
	if (ShaderNumRefs == 0u)
	{
		check(!ShaderPreloadEntry.PreloadEvent);

		const FShaderCodeEntry& ShaderEntry = SerializedShaders.ShaderEntries[ShaderIndex];
		ShaderPreloadEntry.Code = FMemory::Malloc(ShaderEntry.Size);
		ShaderPreloadEntry.FramePreloadStarted = GFrameNumber;
		DebugVisualizer.MarkExplicitlyPreloadedForVisualization(ShaderIndex);

		const EAsyncIOPriorityAndFlags IOPriority = (EAsyncIOPriorityAndFlags)GShaderCodeLibraryAsyncLoadingPriority;

		FGraphEventArray ReadCompletionEvents;

		EAsyncIOPriorityAndFlags DontCache = GShaderCodeLibraryAsyncLoadingAllowDontCache ? AIOP_FLAG_DONTCACHE : AIOP_MIN;
		IMemoryReadStreamRef PreloadData = FileCacheHandle->ReadData(ReadCompletionEvents, LibraryCodeOffset + ShaderEntry.Offset, ShaderEntry.Size, IOPriority | DontCache);
		auto Task = TGraphTask<FPreloadShaderTask>::CreateTask(&ReadCompletionEvents).ConstructAndHold(this, ShaderIndex, MoveTemp(PreloadData));
		ShaderPreloadEntry.PreloadEvent = Task->GetCompletionEvent();
		Task->Unlock();

		INC_DWORD_STAT_BY(STAT_Shaders_ShaderPreloadMemory, ShaderEntry.Size);
	}

	if (ShaderPreloadEntry.PreloadEvent)
	{
		OutCompletionEvents.Add(ShaderPreloadEntry.PreloadEvent);
	}
	return true;
}

bool FShaderCodeArchive::PreloadShaderMap(int32 ShaderMapIndex, FGraphEventArray& OutCompletionEvents)
{
	LLM_SCOPE(ELLMTag::Shaders);

	const FShaderMapEntry& ShaderMapEntry = SerializedShaders.ShaderMapEntries[ShaderMapIndex];
	const EAsyncIOPriorityAndFlags IOPriority = (EAsyncIOPriorityAndFlags)GShaderCodeLibraryAsyncLoadingPriority;
	const uint32 FrameNumber = GFrameNumber;
	uint32 PreloadMemory = 0u;
	
	FWriteScopeLock Lock(ShaderPreloadLock);

	for (uint32 i = 0u; i < ShaderMapEntry.NumShaders; ++i)
	{
		const int32 ShaderIndex = SerializedShaders.ShaderIndices[ShaderMapEntry.ShaderIndicesOffset + i];
		FShaderPreloadEntry& ShaderPreloadEntry = ShaderPreloads[ShaderIndex];
		const FShaderCodeEntry& ShaderEntry = SerializedShaders.ShaderEntries[ShaderIndex];

#if RHI_RAYTRACING
		if (!IsRayTracingAllowed() && !IsCreateShadersOnLoadEnabled() && IsRayTracingShaderFrequency(static_cast<EShaderFrequency>(ShaderEntry.Frequency)))
		{
			ShaderPreloadEntry.bNeverToBePreloaded = 1;
			continue;
		}
#endif

		const uint32 ShaderNumRefs = ShaderPreloadEntry.NumRefs++;
		if (ShaderNumRefs == 0u)
		{
			check(!ShaderPreloadEntry.PreloadEvent);
			ShaderPreloadEntry.Code = FMemory::Malloc(ShaderEntry.Size);
			ShaderPreloadEntry.FramePreloadStarted = FrameNumber;
			DebugVisualizer.MarkExplicitlyPreloadedForVisualization(ShaderIndex);
			PreloadMemory += ShaderEntry.Size;

			FGraphEventArray ReadCompletionEvents;
			EAsyncIOPriorityAndFlags DontCache = GShaderCodeLibraryAsyncLoadingAllowDontCache ? AIOP_FLAG_DONTCACHE : AIOP_MIN;
			IMemoryReadStreamRef PreloadData = FileCacheHandle->ReadData(ReadCompletionEvents, LibraryCodeOffset + ShaderEntry.Offset, ShaderEntry.Size, IOPriority | DontCache);
			auto Task = TGraphTask<FPreloadShaderTask>::CreateTask(&ReadCompletionEvents).ConstructAndHold(this, ShaderIndex, MoveTemp(PreloadData));
			ShaderPreloadEntry.PreloadEvent = Task->GetCompletionEvent();
			Task->Unlock();
			OutCompletionEvents.Add(ShaderPreloadEntry.PreloadEvent);
		}
		else if (ShaderPreloadEntry.PreloadEvent)
		{
			OutCompletionEvents.Add(ShaderPreloadEntry.PreloadEvent);
		}
	}

	INC_DWORD_STAT_BY(STAT_Shaders_ShaderPreloadMemory, PreloadMemory);

	return true;
}

bool FShaderCodeArchive::WaitForPreload(FShaderPreloadEntry& ShaderPreloadEntry)
{
	FGraphEventRef Event;
	{
		FReadScopeLock Lock(ShaderPreloadLock);
		if(ShaderPreloadEntry.NumRefs > 0u)
		{
			Event = ShaderPreloadEntry.PreloadEvent;
		}
		else
		{
			check(!ShaderPreloadEntry.PreloadEvent);
		}
	}

	const bool bNeedToWait = Event && !Event->IsComplete();
	if (bNeedToWait)
	{
		FTaskGraphInterface::Get().WaitUntilTaskCompletes(Event);
	}
	return bNeedToWait;
}

void FShaderCodeArchive::ReleasePreloadedShader(int32 ShaderIndex)
{
	FShaderPreloadEntry& ShaderPreloadEntry = ShaderPreloads[ShaderIndex];
	if (!ShaderPreloadEntry.bNeverToBePreloaded)
	{
		WaitForPreload(ShaderPreloadEntry);

		FWriteScopeLock Lock(ShaderPreloadLock);

		ShaderPreloadEntry.PreloadEvent.SafeRelease();

		const uint32 ShaderNumRefs = ShaderPreloadEntry.NumRefs--;
		check(ShaderPreloadEntry.Code);
		check(ShaderNumRefs > 0u);
		if (ShaderNumRefs == 1u)
		{
			FMemory::Free(ShaderPreloadEntry.Code);
			ShaderPreloadEntry.Code = nullptr;
			const FShaderCodeEntry& ShaderEntry = SerializedShaders.ShaderEntries[ShaderIndex];
			DEC_DWORD_STAT_BY(STAT_Shaders_ShaderPreloadMemory, ShaderEntry.Size);
		}
	}
}

TRefCountPtr<FRHIShader> FShaderCodeArchive::CreateShader(int32 Index)
{
	LLM_SCOPE(ELLMTag::Shaders);
#if STATS
	double TimeFunctionEntered = FPlatformTime::Seconds();
	ON_SCOPE_EXIT
	{
		if (IsInRenderingThread())
		{
			double ShaderCreationTime = FPlatformTime::Seconds() - TimeFunctionEntered;
			INC_FLOAT_STAT_BY(STAT_Shaders_TotalRTShaderInitForRenderingTime, ShaderCreationTime);
		}
	};
#endif

	TRefCountPtr<FRHIShader> Shader;

	FMemStackBase& MemStack = FMemStack::Get();
	FMemMark Mark(MemStack);

	const FShaderCodeEntry& ShaderEntry = SerializedShaders.ShaderEntries[Index];
	FShaderPreloadEntry& ShaderPreloadEntry = ShaderPreloads[Index];
	checkf(!ShaderPreloadEntry.bNeverToBePreloaded, TEXT("We are creating a shader that shouldn't be preloaded in this run (e.g. raytracing shader on D3D11)."));

	FGraphEventArray ReadCompleteEvents;
	PreloadShader(Index, ReadCompleteEvents);

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(BlockingShaderLoad);
		double TimeStarted = FPlatformTime::Seconds();
		const bool bNeededToWait = WaitForPreload(ShaderPreloadEntry);
		if (bNeededToWait)
		{
			const double WaitDuration = FPlatformTime::Seconds() - TimeStarted;
			// only complain if we spent more than 1ms waiting
			if (WaitDuration > GShaderCodeLibraryMaxShaderPreloadWaitTime)
			{
				UE_LOG(LogShaderLibrary, Warning, TEXT("Spent %.2f ms in a blocking wait for shader preload, NumRefs: %d, FramePreloadStarted: %d, CurrentFrame: %d"), WaitDuration * 1000.0, ShaderPreloadEntry.NumRefs, ShaderPreloadEntry.FramePreloadStarted, GFrameNumber);
			}
		}
	}

	const uint8* ShaderCode = (uint8*)ShaderPreloadEntry.Code;
	if (ShaderEntry.UncompressedSize != ShaderEntry.Size)
	{
		uint8* UncompressedCode = reinterpret_cast<uint8*>(MemStack.Alloc(ShaderEntry.UncompressedSize, 16));
		ShaderCodeArchive::DecompressShaderWithOodle(UncompressedCode, ShaderEntry.UncompressedSize, ShaderCode, ShaderEntry.Size);
		ShaderCode = (uint8*)UncompressedCode;
	}

	// detect the breach of contract early
	ensureAlwaysMsgf(IsInRenderingThread() || GRHISupportsMultithreadedShaderCreation, TEXT("More than one thread is creating shaders, but GRHISupportsMultithreadedShaderCreation is false."));

	const auto ShaderCodeView = MakeArrayView(ShaderCode, ShaderEntry.UncompressedSize);
	const FSHAHash& ShaderHash = SerializedShaders.ShaderHashes[Index];
	switch (ShaderEntry.Frequency)
	{
	case SF_Vertex: Shader = RHICreateVertexShader(ShaderCodeView, ShaderHash); CheckShaderCreation(Shader, Index); break;
	case SF_Mesh: Shader = RHICreateMeshShader(ShaderCodeView, ShaderHash); CheckShaderCreation(Shader, Index); break;
	case SF_Amplification: Shader = RHICreateAmplificationShader(ShaderCodeView, ShaderHash); CheckShaderCreation(Shader, Index); break;
	case SF_Pixel: Shader = RHICreatePixelShader(ShaderCodeView, ShaderHash); CheckShaderCreation(Shader, Index); break;
	case SF_Geometry: Shader = RHICreateGeometryShader(ShaderCodeView, ShaderHash); CheckShaderCreation(Shader, Index); break;
	case SF_Compute: Shader = RHICreateComputeShader(ShaderCodeView, ShaderHash); CheckShaderCreation(Shader, Index); break;
	case SF_RayGen: case SF_RayMiss: case SF_RayHitGroup: case SF_RayCallable:
#if RHI_RAYTRACING
		if (GRHISupportsRayTracing && GRHISupportsRayTracingShaders)
		{
			Shader = RHICreateRayTracingShader(ShaderCodeView, ShaderHash, ShaderEntry.GetFrequency());
			CheckShaderCreation(Shader, Index);
		}
#endif // RHI_RAYTRACING
		break;
	default: checkNoEntry(); break;
	}
	DebugVisualizer.MarkCreatedForVisualization(Index);

	// Release the reference we were holding
	ReleasePreloadedShader(Index);

	if (Shader)
	{
		INC_DWORD_STAT(STAT_Shaders_NumShadersUsedForRendering);
		Shader->SetHash(ShaderHash);
	}

	return Shader;
}

FIoChunkId FIoStoreShaderCodeArchive::GetShaderCodeArchiveChunkId(const FString& LibraryName, FName FormatName)
{
	FString Name = FString::Printf(TEXT("%s-%s"), *LibraryName, *FormatName.ToString());
	Name.ToLowerInline();
	uint64 Hash = CityHash64(reinterpret_cast<const char*>(*Name), Name.Len() * sizeof(TCHAR));
	return CreateIoChunkId(Hash, 0, EIoChunkType::ShaderCodeLibrary);
}

FIoChunkId FIoStoreShaderCodeArchive::GetShaderCodeChunkId(const FSHAHash& ShaderHash)
{
	uint8 Data[12];
	FMemory::Memcpy(Data, ShaderHash.Hash, 11);
	*reinterpret_cast<uint8*>(&Data[11]) = static_cast<uint8>(EIoChunkType::ShaderCode);
	FIoChunkId ChunkId;
	ChunkId.Set(Data, 12);
	return ChunkId;
}

void FIoStoreShaderCodeArchive::CreateIoStoreShaderCodeArchiveHeader(const FName& Format, const FSerializedShaderArchive& SerializedShaders, FIoStoreShaderCodeArchiveHeader& OutHeader)
{
	OutHeader.ShaderMapHashes = SerializedShaders.ShaderMapHashes;
	OutHeader.ShaderHashes = SerializedShaders.ShaderHashes;
	// shader group hashes will be populated later

	OutHeader.ShaderMapEntries.Empty(SerializedShaders.ShaderMapEntries.Num());
	for (const FShaderMapEntry& ShaderMapEntry : SerializedShaders.ShaderMapEntries)
	{
		FIoStoreShaderMapEntry& IoStoreShaderMapEntry = OutHeader.ShaderMapEntries.AddDefaulted_GetRef();
		IoStoreShaderMapEntry.ShaderIndicesOffset = ShaderMapEntry.ShaderIndicesOffset;
		IoStoreShaderMapEntry.NumShaders = ShaderMapEntry.NumShaders;
	}

	// indices should be copied before grouping as the groups will append to the array
	OutHeader.ShaderIndices = SerializedShaders.ShaderIndices;

	// shader entries are copied, the remainder of the field will be assigned when splitting into groups
	OutHeader.ShaderEntries.Empty(SerializedShaders.ShaderEntries.Num());
	for (const FShaderCodeEntry& ShaderEntry : SerializedShaders.ShaderEntries)
	{
		FIoStoreShaderCodeEntry& IoStoreShaderEntry = OutHeader.ShaderEntries.AddDefaulted_GetRef();
		IoStoreShaderEntry.Frequency = ShaderEntry.Frequency;
	}

	// Higher level description of the group splitting algo that follows:
	// We compress together shaders that are loaded together (all other strategies, like grouping similar shaders, were found to compress better but not reduce RAM usage).
	// For that, we find for each shader which shadermaps are referencing it (often times it will be just one, but for some simple and shared shaders it can be thousands).
	// We group the shaders by those sets of shadermaps - all shaders referenced by the same shadermap(s) are a candidate for being a single group. Then we potentially split this candidate group
	// into raytracing and non-raytracing groups (so we can avoid preloading RTX shaders run-time if RTX is disabled), and then each of those is potentially split further by size
	// (to avoid too large groups that will take too much time to decompress - this is regulated by r.ShaderCodeLibrary.MaxShaderGroupSize). The results of that process (note, it can still be
	// a single group) is added to the header.
	// Each group's indices, like in case of shadermaps, are stored in ShaderIndices array. Before we append a new group's indices however, we look if we can find an existing range that we can reuse.
	const bool bSeparateRaytracingShaders = (Format == FName("PCD3D_SM5"));

	TArray<TPair<uint32, TArray<int32>>> ShaderToShadermapsArray;
	{
		ShaderToShadermapsArray.AddDefaulted(OutHeader.ShaderEntries.Num());

		{
			FCriticalSection ShaderLocks[1024];
			// for each shader, find all the shadermaps it belongs to
			ParallelFor(OutHeader.ShaderMapEntries.Num(),
				[&ShaderToShadermapsArray, &OutHeader, &ShaderLocks](int32 ShaderMapIndex)
				{
					FIoStoreShaderMapEntry& ShaderMapEntry = OutHeader.ShaderMapEntries[ShaderMapIndex];
					for (int32 ShaderIdxIdx = ShaderMapEntry.ShaderIndicesOffset, StopBeforeIdxIdx = ShaderMapEntry.ShaderIndicesOffset + ShaderMapEntry.NumShaders; ShaderIdxIdx < StopBeforeIdxIdx; ++ShaderIdxIdx)
					{
						int32 ShaderIndex = OutHeader.ShaderIndices[ShaderIdxIdx];
						// add this shadermap as a dependency.
						int32 ShaderLockNumber = ShaderIndex % UE_ARRAY_COUNT(ShaderLocks);
						FScopeLock Locker(&ShaderLocks[ShaderLockNumber]);
						ShaderToShadermapsArray[ShaderIndex].Value.Add(ShaderMapIndex);
					}
				}
			);
		}

		// sort shadermaps entries in shaders
		{
			const int32 kShaderSortedPerThread = 1024;
			const int32 NumThreads = (ShaderToShadermapsArray.Num() / kShaderSortedPerThread) + 1;
			ParallelFor(NumThreads,
				[&ShaderToShadermapsArray, kShaderSortedPerThread](int ThreadIndex)
				{
					int32 StartingShader = ThreadIndex * kShaderSortedPerThread;
					int32 EndingShader = FMath::Min(StartingShader + kShaderSortedPerThread, ShaderToShadermapsArray.Num());
					for (int32 Idx = StartingShader; Idx < EndingShader; ++Idx)
					{
						ShaderToShadermapsArray[Idx].Value.Sort();
					}
				},
				EParallelForFlags::Unbalanced
			);
		}

		// now assigning the indices in the array so we can sort it
		for (int32 Idx = 0, Num = ShaderToShadermapsArray.Num(); Idx < Num; ++Idx)
		{
			// check that no shader is unreferenced
			checkf(!ShaderToShadermapsArray[Idx].Value.IsEmpty(), TEXT("Error converting to IoStore archive: shader (index=%d) is not referenced by any of the shadermaps!"), Idx);
			ShaderToShadermapsArray[Idx].Key = Idx;
		}
	}

	// sort the mapping so the first are shaders that are referenced by a smaller number of shadermaps, then by index for determinism
	Algo::Sort(ShaderToShadermapsArray,
		[](const TPair<uint32, TArray<int32>>& EntryA, const TPair<uint32, TArray<int32>>& EntryB)
		{
			const TArray<int32>& A = EntryA.Value;
			const TArray<int32>& B = EntryB.Value;
			// if the number of shadermaps is the same, we need to sort "alphabetically"
			if (A.Num() == B.Num())
			{
				for (int32 Idx = 0, Num = A.Num(); Idx < Num; ++Idx)
				{
					if (A[Idx] != B[Idx])
					{
						return A[Idx] < B[Idx];
					}
				}

				return EntryA.Key < EntryB.Key;
			}

			return A.Num() < B.Num();
		}
	);

	// get the effective maximum uncompressed group size
	uint32 MaxUncompressedShaderGroupSize = FMath::Max(static_cast<uint32>(GShaderCodeLibraryMaxShaderGroupSize), 1U);	// cannot be lower than 1

	// for statistics
	uint64 Stats_TotalUncompressedMemory = 0;
	uint32 Stats_MinGroupSize = MAX_uint32;
	uint32 Stats_MaxGroupSize = 0;

	// We want to avoid adding group indices to ShaderIndices, however looking them up sequentially is to slow. Store them here for a future ParallelFor lookup.
	TArray<TArray<uint32>> StoredGroupShaderIndices;

	/** Third and last stage of processing the shader group. We actually add it here, and do the book-keeping. */
	auto ProcessShaderGroup_AddNewGroup = [&Format, &OutHeader, &SerializedShaders, &StoredGroupShaderIndices, &Stats_TotalUncompressedMemory, &Stats_MinGroupSize, &Stats_MaxGroupSize](TArray<uint32>& ShaderIndicesInGroup)
	{
		// first, sort the shaders by uncompressed size, as this was found to compress better
		ShaderIndicesInGroup.Sort(
			[&SerializedShaders](const int32 ShaderIndexA, const int32 ShaderIndexB)
			{
				const FShaderCodeEntry& ShaderEntryA = SerializedShaders.ShaderEntries[ShaderIndexA];
				const FShaderCodeEntry& ShaderEntryB = SerializedShaders.ShaderEntries[ShaderIndexB];
				if (ShaderEntryA.UncompressedSize != ShaderEntryB.UncompressedSize)
				{
					return ShaderEntryA.UncompressedSize < ShaderEntryB.UncompressedSize;
				}
				if (ShaderEntryA.Size != ShaderEntryB.Size)
				{
					return ShaderEntryA.Size < ShaderEntryB.Size;
				}
				if (ShaderEntryA.Frequency != ShaderEntryB.Frequency)
				{
					return ShaderEntryA.Frequency < ShaderEntryB.Frequency;
				}
				return ShaderEntryA.Offset < ShaderEntryB.Offset;
			}
		);

		// add a new group entry
		const int32 CurrentGroupIdx = OutHeader.ShaderGroupEntries.Num();
		FIoStoreShaderGroupEntry& GroupEntry = OutHeader.ShaderGroupEntries.AddDefaulted_GetRef();
		StoredGroupShaderIndices.Add(ShaderIndicesInGroup);
		GroupEntry.NumShaders = ShaderIndicesInGroup.Num();
		// ShaderIndicesOffset will be filled later, once we know all the groups (see comment about StoredGroupShaderIndices above).

		// update shader entries both with the group number and their uncompressed offset in the group
		FSHA1 GroupHasher;
		uint32 CurrentGroupSize = 0;
		for (int32 ShaderIdxIdx = 0, NumIdxIdx = ShaderIndicesInGroup.Num(); ShaderIdxIdx < NumIdxIdx; ++ShaderIdxIdx)
		{
			int32 ShaderIndex = ShaderIndicesInGroup[ShaderIdxIdx];
			FIoStoreShaderCodeEntry& IoStoreShaderEntry = OutHeader.ShaderEntries[ShaderIndex];
			IoStoreShaderEntry.ShaderGroupIndex = CurrentGroupIdx;
			IoStoreShaderEntry.UncompressedOffsetInGroup = CurrentGroupSize;

			// group hash is constructed from hashing the shaders in the group.
			GroupHasher.Update(SerializedShaders.ShaderHashes[ShaderIndex].Hash, sizeof(FSHAHash));
			// shader hash as of now excludes optional data, so we cannot rely on it, especially across the shader formats. Make the group hash a bit more robust by including the shader size in it.
			GroupHasher.Update(reinterpret_cast<const uint8*>(&SerializedShaders.ShaderEntries[ShaderIndex].UncompressedSize), sizeof(FShaderCodeEntry::UncompressedSize));

			CurrentGroupSize += SerializedShaders.ShaderEntries[ShaderIndex].UncompressedSize;
		}
		// Shader hashes cannot be used to uniquely identify across shader formats due to aforementioned exclusion of optional data from it.
		// Include the shader format (in a platform-agnostic way) into the group hash to lower the risk of collision of shaders of different formats.
		const TStringConversion<TStringConvert<TCHAR, UTF8CHAR>> Utf8Name(Format.ToString());
		GroupHasher.Update(reinterpret_cast<const uint8*>(Utf8Name.Get()), Utf8Name.Length());
		static_assert(sizeof(uint8) == sizeof(UTF8CHAR), "Unexpected UTF-8 char size.");

		GroupEntry.UncompressedSize = CurrentGroupSize;
		OutHeader.ShaderGroupIoHashes.Add(FIoStoreShaderCodeArchive::GetShaderCodeChunkId(GroupHasher.Finalize()));

		Stats_TotalUncompressedMemory += CurrentGroupSize;
		Stats_MinGroupSize = FMath::Min(Stats_MinGroupSize, CurrentGroupSize);
		Stats_MaxGroupSize = FMath::Max(Stats_MaxGroupSize, CurrentGroupSize);
	};

	/** Second stage of processing shader group. Here we potentially split the group into smaller ones (as equally as possible), striving to meet limit imposed by r.ShaderCodeLibrary.MaxShaderGroupSize */
	auto ProcessShaderGroup_SplitBySize = [&OutHeader, &SerializedShaders, &ProcessShaderGroup_AddNewGroup, &MaxUncompressedShaderGroupSize](TArray<uint32>& CurrentShaderGroup)
	{
		// calculate current group size
		uint32 GroupSize = 0;
		for (uint32 ShaderIdx : CurrentShaderGroup)
		{
			GroupSize += SerializedShaders.ShaderEntries[ShaderIdx].UncompressedSize;
		}
		
		if (LIKELY(GroupSize <= MaxUncompressedShaderGroupSize || CurrentShaderGroup.Num() == 1))
		{
			ProcessShaderGroup_AddNewGroup(CurrentShaderGroup);
		}
		else
		{
			// split the shaders evenly into N new groups (don't allow more new groups than there are shaders)
			int32 NumNewGroups = FMath::Min(static_cast<int32>(GroupSize / MaxUncompressedShaderGroupSize + 1), CurrentShaderGroup.Num());
			checkf(NumNewGroups > 1, TEXT("Off by one error in group count calculation? NumNewGroups=%d, GroupSize=%u, MaxUncompressedShaderGroupSize=%u, CurrentShaderGroup.Num()=%d"), NumNewGroups, GroupSize, MaxUncompressedShaderGroupSize, CurrentShaderGroup.Num());

			TArray<TArray<uint32>> NewGroups;
			TArray<uint32> NewGroupSizes;
			NewGroups.AddDefaulted(NumNewGroups);
			NewGroupSizes.AddZeroed(NumNewGroups);

			// sort the shaders descending, as this is easier to split (greedy algorithm)
			CurrentShaderGroup.Sort(
				[&SerializedShaders](const int32 ShaderIndexA, const int32 ShaderIndexB)
				{
					const FShaderCodeEntry& ShaderEntryA = SerializedShaders.ShaderEntries[ShaderIndexA];
					const FShaderCodeEntry& ShaderEntryB = SerializedShaders.ShaderEntries[ShaderIndexB];
					if (ShaderEntryA.UncompressedSize != ShaderEntryB.UncompressedSize)
					{
						return ShaderEntryA.UncompressedSize > ShaderEntryB.UncompressedSize;
					}
					if (ShaderEntryA.Size != ShaderEntryB.Size)
					{
						return ShaderEntryA.Size > ShaderEntryB.Size;
					}
					if (ShaderEntryA.Frequency != ShaderEntryB.Frequency)
					{
						return ShaderEntryA.Frequency > ShaderEntryB.Frequency;
					}
					return ShaderEntryA.Offset > ShaderEntryB.Offset;
				}
			);

			for (int32 ShaderIdx : CurrentShaderGroup)
			{
				// add the shader to the group of smallest size
				int32 SmallestNewGroupIdx = 0;
				for (int32 IdxNewGroup = 1; IdxNewGroup < NumNewGroups; ++IdxNewGroup)
				{
					if (NewGroupSizes[IdxNewGroup] < NewGroupSizes[SmallestNewGroupIdx])
					{
						SmallestNewGroupIdx = IdxNewGroup;
					}
				}

				NewGroups[SmallestNewGroupIdx].Add(ShaderIdx);
				NewGroupSizes[SmallestNewGroupIdx] += SerializedShaders.ShaderEntries[ShaderIdx].UncompressedSize;
			}
			
#if DO_CHECK // sanity checks
			uint32 NewGroupTotalSize = 0;
			for (uint32 NewGroupSize : NewGroupSizes)
			{
				NewGroupTotalSize += NewGroupSize;
			}
			checkf(NewGroupTotalSize == GroupSize, TEXT("Original shader group was of size %u bytes, which was larger than limit %u, but it was split into %d group of total size %u, which is not %u - sizes must agree"),
				GroupSize,
				MaxUncompressedShaderGroupSize,
				NumNewGroups,
				NewGroupTotalSize, GroupSize
				);
#endif

			for (TArray<uint32>& NewGroup : NewGroups)
			{
				// note there can be empty groups (take a very edge case of MaxUncompressedShaderGroupSize = 2 bytes and a shader group of 1 shader)
				if (!NewGroup.IsEmpty())
				{
					ProcessShaderGroup_AddNewGroup(NewGroup);
				}
			}
		}
	};

	/** First stage of processing a streak of shaders all referenced by the same set of shadermaps. We begin with separating raytracing and non-raytracing shaders, so we can avoid preloading RTX in non-RT runs. */
	auto ProcessShaderGroup_SplitRaytracing = [&bSeparateRaytracingShaders, &OutHeader, &ProcessShaderGroup_SplitBySize](TArray<uint32>& CurrentShaderGroup)
	{
		if (!bSeparateRaytracingShaders)
		{
			ProcessShaderGroup_SplitBySize(CurrentShaderGroup);
		}
		else
		{
			// The streak changed. Create the group, but first, determine if the group needs to be split in two because of the raytracing shaders.
			// We want to isolate them into separate groups so their preload can be skipped if raytracing is off.
			TArray<uint32> RaytracingShaders;
			TArray<uint32> NonraytracingShaders;
			for (int32 ShaderIndex : CurrentShaderGroup)
			{
				if (LIKELY(!IsRayTracingShaderFrequency(static_cast<EShaderFrequency>(OutHeader.ShaderEntries[ShaderIndex].Frequency))))
				{
					NonraytracingShaders.Add(ShaderIndex);
				}
				else
				{
					RaytracingShaders.Add(ShaderIndex);
				}
			}
			check(CurrentShaderGroup.Num() == NonraytracingShaders.Num() + RaytracingShaders.Num());

			if (LIKELY(!NonraytracingShaders.IsEmpty()))
			{
				ProcessShaderGroup_SplitBySize(NonraytracingShaders);
			}
			if (UNLIKELY(!RaytracingShaders.IsEmpty()))
			{
				ProcessShaderGroup_SplitBySize(RaytracingShaders);
			}
		}
	};

	// now split this into streaks of shaders that are referenced by the same set of shadermaps and compress
	TArray<uint32> CurrentShaderGroup;
	TArray<int32> LastShadermapSetSeen;
	for (TPair<uint32, TArray<int32>>& Iter : ShaderToShadermapsArray)
	{
		// if we have have just started the group, we don't check the last seen
		if (UNLIKELY(CurrentShaderGroup.IsEmpty()))
		{
			CurrentShaderGroup.Add(Iter.Key);
			LastShadermapSetSeen = Iter.Value;
		}
		else if (UNLIKELY(LastShadermapSetSeen != Iter.Value))
		{
			ProcessShaderGroup_SplitRaytracing(CurrentShaderGroup);

			// reset the collection, but don't forget to add to it the current element
			CurrentShaderGroup.SetNum(1);
			CurrentShaderGroup[0] = Iter.Key;
			LastShadermapSetSeen = Iter.Value;
		}
		else
		{
			// keep adding to the same group
			CurrentShaderGroup.Add(Iter.Key);
		}
	}
	// add the last group
	if (!CurrentShaderGroup.IsEmpty())
	{
		ProcessShaderGroup_SplitRaytracing(CurrentShaderGroup);
	}

	/** Tries to find whether NewIndices exist as a subsequence in ExistingIndices.Returns - 1 if not found. */
	auto FindSequenceInArray = [](const TArray<uint32>& ExistingIndices, const TArray<uint32>& NewIndices) -> int32
	{
		check(NewIndices.Num() > 0);

		uint32 FirstNew = NewIndices[0];
		int32 NumNew = NewIndices.Num();
		for (int32 IdxExisting = 0, NumExisting = ExistingIndices.Num(); IdxExisting < NumExisting - NumNew + 1; ++IdxExisting)
		{
			if (LIKELY(ExistingIndices[IdxExisting] != FirstNew))
			{
				continue;
			}

			// check the rest
			bool bFoundSequence = true;
			for (int32 IdxNew = 1; IdxNew < NumNew; ++IdxNew)
			{
				checkSlow(IdxExisting + IdxNew < NumExisting);
				if (LIKELY(ExistingIndices[IdxExisting + IdxNew] != NewIndices[IdxNew]))
				{
					bFoundSequence = false;
					break;
				}
			}

			if (UNLIKELY(bFoundSequence))
			{
				return IdxExisting;
			}
		}

		return INDEX_NONE;
	};

	// now, try to see if we can look up group's indices in the existing ShaderIndicesArray to avoid storing them there.
	checkf(StoredGroupShaderIndices.Num() == OutHeader.ShaderGroupEntries.Num(), TEXT("We should have stored shader indices for all groups."));
	ParallelFor(StoredGroupShaderIndices.Num(),
		[&OutHeader, &SerializedShaders, &StoredGroupShaderIndices, &FindSequenceInArray](int ShaderGroupIndex)
		{
			const TArray<uint32>& ShaderIndicesInGroup = StoredGroupShaderIndices[ShaderGroupIndex];
			FIoStoreShaderGroupEntry& GroupEntry = OutHeader.ShaderGroupEntries[ShaderGroupIndex];
			// See if we can find indices in that order somewhere in the ShaderIndices array already, to avoid adding new indices.
			// We are looking in the read-only original array, because there's no sense to look in OutHeader.ShaderIndices - groups don't overlap,
			// so we know that newly added (by some previous group) indices aren't useful for us.
			int32 ExistingOffset = FindSequenceInArray(SerializedShaders.ShaderIndices, ShaderIndicesInGroup);
			if (ExistingOffset != INDEX_NONE)
			{
				GroupEntry.ShaderIndicesOffset = static_cast<uint32>(ExistingOffset);
			}
			else
			{
				GroupEntry.ShaderIndicesOffset = MAX_uint32;
			}
		}
	);

	// Now append all the groups that weren't found to the end of ShaderIndices, slow (we could have done that in above ParallelFor, with a lock), but good for determinism
	int32 Stats_GroupsThatAppendedToShaderIndices = 0;
	for (int32 ShaderGroupIndex = 0, NumGroups = OutHeader.ShaderGroupEntries.Num(); ShaderGroupIndex < NumGroups; ++ShaderGroupIndex)
	{
		FIoStoreShaderGroupEntry& GroupEntry = OutHeader.ShaderGroupEntries[ShaderGroupIndex];

		if (GroupEntry.ShaderIndicesOffset == MAX_uint32)
		{
			const TArray<uint32>& ShaderIndicesInGroup = StoredGroupShaderIndices[ShaderGroupIndex];
			GroupEntry.ShaderIndicesOffset = OutHeader.ShaderIndices.Num();
			OutHeader.ShaderIndices.Append(ShaderIndicesInGroup);
			++Stats_GroupsThatAppendedToShaderIndices;
		}
	}

	checkf(OutHeader.ShaderEntries.Num() == SerializedShaders.ShaderEntries.Num(), TEXT("Error creating IoStoreShaderArchive header - shader entries differ (%d in IoStore, %d original). Bug in grouping logic?"),
		OutHeader.ShaderEntries.Num(), SerializedShaders.ShaderEntries.Num());
	checkf(OutHeader.ShaderGroupIoHashes.Num() == OutHeader.ShaderGroupEntries.Num(), TEXT("Error creating IoStoreShaderArchive header - mismatch between shader group hashes and descriptors (%d descriptors, %d hashes). Bug in grouping logic?"),
		OutHeader.ShaderGroupEntries.Num(), OutHeader.ShaderGroupIoHashes.Num());
	checkf(OutHeader.ShaderGroupEntries.Num() != 0, TEXT("At least one group must have been created"));

	UE_LOG(LogShaderLibrary, Display, TEXT("Created IoStoreShaderArchive header: shaders grouped in %d groups (%d of them didn't need new indices), average uncompressed size %llu bytes, min %u bytes, max %u bytes (r.ShaderCodeLibrary.MaxShaderGroupSize=%u)"),
		OutHeader.ShaderGroupEntries.Num(), OutHeader.ShaderGroupEntries.Num() - Stats_GroupsThatAppendedToShaderIndices, Stats_TotalUncompressedMemory / static_cast<uint64>(OutHeader.ShaderGroupEntries.Num()), Stats_MinGroupSize, Stats_MaxGroupSize, MaxUncompressedShaderGroupSize);
}

FArchive& operator <<(FArchive& Ar, FIoStoreShaderCodeArchiveHeader& Ref)
{
	Ar << Ref.ShaderMapHashes;
	Ar << Ref.ShaderHashes;
	Ar << Ref.ShaderGroupIoHashes;
	Ar << Ref.ShaderMapEntries;
	Ar << Ref.ShaderEntries;
	Ar << Ref.ShaderGroupEntries;
	Ar << Ref.ShaderIndices;
	return Ar;
}

void FIoStoreShaderCodeArchive::SaveIoStoreShaderCodeArchive(const FIoStoreShaderCodeArchiveHeader& Header, FArchive& OutLibraryAr)
{
	uint32 Version = CurrentVersion;
	OutLibraryAr << Version;
	OutLibraryAr << const_cast<FIoStoreShaderCodeArchiveHeader &>(Header);
}

FIoStoreShaderCodeArchive* FIoStoreShaderCodeArchive::Create(EShaderPlatform InPlatform, const FString& InLibraryName, FIoDispatcher& InIoDispatcher)
{
	const FName PlatformName = FName(FDataDrivenShaderPlatformInfo::GetShaderFormat(InPlatform).ToString() + TEXT("-") + FDataDrivenShaderPlatformInfo::GetName(InPlatform).ToString());
	FIoChunkId ChunkId = GetShaderCodeArchiveChunkId(InLibraryName, PlatformName);
	if (InIoDispatcher.DoesChunkExist(ChunkId))
	{
		FIoBatch IoBatch = InIoDispatcher.NewBatch();
		FIoRequest IoRequest = IoBatch.Read(ChunkId, FIoReadOptions(), IoDispatcherPriority_Max);
		FEvent* Event = FPlatformProcess::GetSynchEventFromPool();
		IoBatch.IssueAndTriggerEvent(Event);
		Event->Wait();
		FPlatformProcess::ReturnSynchEventToPool(Event);
		const FIoBuffer& IoBuffer = IoRequest.GetResultOrDie();
		FMemoryReaderView Ar(MakeArrayView(IoBuffer.Data(), IoBuffer.DataSize()));
		uint32 Version = 0;
		Ar << Version;
		if (Version == CurrentVersion)
		{
			FIoStoreShaderCodeArchive* Library = new FIoStoreShaderCodeArchive(InPlatform, InLibraryName, InIoDispatcher);
			Ar << Library->Header;
			{
				const uint32 HashSize = FMath::Min<uint32>(0x10000, 1u << FMath::CeilLogTwo(Library->Header.ShaderMapHashes.Num()));
				Library->ShaderMapHashTable.Clear(HashSize, Library->Header.ShaderMapHashes.Num());
				for (int32 Index = 0, Num = Library->Header.ShaderMapHashes.Num(); Index < Num; ++Index)
				{
					const uint32 Key = GetTypeHash(Library->Header.ShaderMapHashes[Index]);
					Library->ShaderMapHashTable.Add(Key, Index);
				}
			}
			{
				const uint32 HashSize = FMath::Min<uint32>(0x10000, 1u << FMath::CeilLogTwo(Library->Header.ShaderHashes.Num()));
				Library->ShaderHashTable.Clear(HashSize, Library->Header.ShaderHashes.Num());
				for (int32 Index = 0, Num = Library->Header.ShaderHashes.Num(); Index < Num; ++Index)
				{
					const uint32 Key = GetTypeHash(Library->Header.ShaderHashes[Index]);
					Library->ShaderHashTable.Add(Key, Index);
				}
			}

			Library->DebugVisualizer.Initialize(Library->Header.ShaderEntries.Num());

			UE_LOG(LogShaderLibrary, Display, TEXT("Using IoDispatcher for shader code library %s. Total %d unique shaders."), *InLibraryName, Library->Header.ShaderEntries.Num());
			INC_DWORD_STAT_BY(STAT_Shaders_ShaderResourceMemory, Library->GetSizeBytes());
			return Library;
		}
	}
	return nullptr;
}

FIoStoreShaderCodeArchive::FIoStoreShaderCodeArchive(EShaderPlatform InPlatform, const FString& InLibraryName, FIoDispatcher& InIoDispatcher)
	: FRHIShaderLibrary(InPlatform, InLibraryName)
	, IoDispatcher(InIoDispatcher)
{
}

FIoStoreShaderCodeArchive::~FIoStoreShaderCodeArchive()
{
	DEC_DWORD_STAT_BY(STAT_Shaders_ShaderResourceMemory, GetSizeBytes());
	Teardown();
}

void FIoStoreShaderCodeArchive::Teardown()
{
	DebugVisualizer.SaveShaderUsageBitmap(GetName(), GetPlatform());

	FWriteScopeLock Lock(PreloadedShaderGroupsLock);
	for (TMap<int32, FShaderGroupPreloadEntry*>::TIterator Iter(PreloadedShaderGroups); Iter; ++Iter)
	{
		FShaderGroupPreloadEntry* PreloadEntry = Iter.Value();

#if UE_SCA_DEBUG_PRELOADING
		checkf(PreloadEntry->NumRefs == 0, TEXT("Group %d has still %d references on deletion. Group extended debug info: \n%s"), Iter.Key(), PreloadEntry->NumRefs,
			*PreloadEntry->DebugInfo);
#else
		checkf(PreloadEntry->NumRefs == 0, TEXT("Group %d has still %d references on deletion. Group extended debug info: \n%s"), Iter.Key(), PreloadEntry->NumRefs,
			TEXT("Not compiled in (set UE_SCA_DEBUG_PRELOADING to 1 in ShaderCodeArchive.h and recompile the game binary)"));
#endif

		const FIoStoreShaderGroupEntry& GroupEntry = Header.ShaderGroupEntries[Iter.Key()];
		DEC_DWORD_STAT_BY(STAT_Shaders_ShaderPreloadMemory, (GroupEntry.CompressedSize + sizeof(FShaderGroupPreloadEntry)));

		delete PreloadEntry;
	}
	PreloadedShaderGroups.Empty();
}

void FIoStoreShaderCodeArchive::SetupPreloadEntryForLoading(int32 ShaderGroupIndex, FShaderGroupPreloadEntry& PreloadEntry)
{
	PreloadEntry.FramePreloadStarted = GFrameNumber;
	check(!PreloadEntry.PreloadEvent);
	PreloadEntry.PreloadEvent = FGraphEvent::CreateGraphEvent();

#if UE_SCA_VISUALIZE_SHADER_USAGE
	const FIoStoreShaderGroupEntry& GroupEntry = Header.ShaderGroupEntries[ShaderGroupIndex];
	for (uint32 ShaderIdxIdx = GroupEntry.ShaderIndicesOffset, StopBeforeIdxIdx = GroupEntry.ShaderIndicesOffset + GroupEntry.NumShaders; ShaderIdxIdx < StopBeforeIdxIdx; ++ShaderIdxIdx)
	{
		DebugVisualizer.MarkPreloadedForVisualization(Header.ShaderIndices[ShaderIdxIdx]);
	}
#endif // UE_SCA_VISUALIZE_SHADER_USAGE
}

bool FIoStoreShaderCodeArchive::PreloadShaderGroup(int32 ShaderGroupIndex, FGraphEventArray& OutCompletionEvents, 
#if UE_SCA_DEBUG_PRELOADING
	const FString& CallsiteInfo,
#endif
	FCoreDelegates::FAttachShaderReadRequestFunc* AttachShaderReadRequestFuncPtr)
{
	// should be called within LLMTag::Shaders scope
	FWriteScopeLock Lock(PreloadedShaderGroupsLock);
	FShaderGroupPreloadEntry& PreloadEntry = *FindOrAddPreloadEntry(ShaderGroupIndex);
	checkf(!PreloadEntry.bNeverToBePreloaded, TEXT("We are preloading a shader group (index=%d) that shouldn't be preloaded in this run (e.g. raytracing shaders on D3D11)."), ShaderGroupIndex);

	const uint32 NumRefs = PreloadEntry.NumRefs++;
#if UE_SCA_DEBUG_PRELOADING
	FString AppendInfo = FString::Printf(TEXT("PreloadShaderGroup: NumRefs %d -> %d    CallsiteInfo: %s\n"),
		NumRefs, PreloadEntry.NumRefs, *CallsiteInfo
	);
	PreloadEntry.DebugInfo.Append(AppendInfo);
#endif
	if (NumRefs == 0u)
	{
		SetupPreloadEntryForLoading(ShaderGroupIndex, PreloadEntry);

		// only global shaders are going to hit this path, all other shaders will be preloaded with the package
		if (UNLIKELY(AttachShaderReadRequestFuncPtr == nullptr))
		{
			FIoBatch IoBatch = IoDispatcher.NewBatch();
			PreloadEntry.IoRequest = IoBatch.Read(Header.ShaderGroupIoHashes[ShaderGroupIndex], FIoReadOptions(), IoDispatcherPriority_Medium);
			IoBatch.IssueAndDispatchSubsequents(PreloadEntry.PreloadEvent);
		}
		else
		{
			PreloadEntry.IoRequest = (*AttachShaderReadRequestFuncPtr)(Header.ShaderGroupIoHashes[ShaderGroupIndex], PreloadEntry.PreloadEvent);
		}

		INC_DWORD_STAT_BY(STAT_Shaders_ShaderPreloadMemory, (Header.ShaderGroupEntries[ShaderGroupIndex].CompressedSize + sizeof(FShaderGroupPreloadEntry)));
	}

	if (AttachShaderReadRequestFuncPtr == nullptr && PreloadEntry.PreloadEvent && !PreloadEntry.PreloadEvent->IsComplete())
	{
		OutCompletionEvents.Add(PreloadEntry.PreloadEvent);
	}
	return true;
}

void FIoStoreShaderCodeArchive::MarkPreloadEntrySkipped(int32 ShaderGroupIndex
#if UE_SCA_DEBUG_PRELOADING
	, const FString& CallsiteInfo
#endif
)
{
	// should be called within LLMTag::Shaders scope
	FWriteScopeLock Lock(PreloadedShaderGroupsLock);
	FShaderGroupPreloadEntry& PreloadEntry = *FindOrAddPreloadEntry(ShaderGroupIndex);
	const uint32 NumRefs = PreloadEntry.NumRefs++;
#if UE_SCA_DEBUG_PRELOADING
	FString AppendInfo = FString::Printf(TEXT("MarkPreloadEntrySkipped: NumRefs %d -> %d    CallsiteInfo: %s\n"),
		NumRefs, PreloadEntry.NumRefs, *CallsiteInfo
	);
	PreloadEntry.DebugInfo.Append(AppendInfo);
#endif
	if (NumRefs == 0u)
	{
		PreloadEntry.bNeverToBePreloaded = 1;
		INC_DWORD_STAT_BY(STAT_Shaders_ShaderPreloadMemory, sizeof(FShaderGroupPreloadEntry));
	}
}

bool FIoStoreShaderCodeArchive::PreloadShader(int32 ShaderIndex, FGraphEventArray& OutCompletionEvents)
{
	LLM_SCOPE(ELLMTag::Shaders);
	DebugVisualizer.MarkExplicitlyPreloadedForVisualization(ShaderIndex);
	return PreloadShaderGroup(GetGroupIndexForShader(ShaderIndex), OutCompletionEvents
#if UE_SCA_DEBUG_PRELOADING
		, FString::Printf(TEXT("PreloadShader %d"), ShaderIndex)
#endif
	);
}

bool FIoStoreShaderCodeArchive::GroupOnlyContainsRaytracingShaders(int32 ShaderGroupIndex)
{
	const FIoStoreShaderGroupEntry& GroupEntry = Header.ShaderGroupEntries[ShaderGroupIndex];
	for (uint32 ShaderIdxIdx = GroupEntry.ShaderIndicesOffset, StopIdxIdx = GroupEntry.ShaderIndicesOffset + GroupEntry.NumShaders; ShaderIdxIdx < StopIdxIdx; ++ShaderIdxIdx)
	{
		int32 ShaderIndex = Header.ShaderIndices[ShaderIdxIdx];
		if (!IsRayTracingShaderFrequency(static_cast<EShaderFrequency>(Header.ShaderEntries[ShaderIndex].Frequency)))
		{
			return false;
		}
	}

	return true;
}

bool FIoStoreShaderCodeArchive::PreloadShaderMap(int32 ShaderMapIndex, FGraphEventArray& OutCompletionEvents)
{
	LLM_SCOPE(ELLMTag::Shaders);
	const FIoStoreShaderMapEntry& ShaderMapEntry = Header.ShaderMapEntries[ShaderMapIndex];

#if UE_SCA_DEBUG_PRELOADING
	FString Callsite = FString::Printf(TEXT("PreloadShaderMap %d"), ShaderMapIndex);
#endif
	for (uint32 i = 0u; i < ShaderMapEntry.NumShaders; ++i)
	{
		const int32 ShaderIndex = Header.ShaderIndices[ShaderMapEntry.ShaderIndicesOffset + i];
		const int32 ShaderGroupIndex = GetGroupIndexForShader(ShaderIndex);

#if RHI_RAYTRACING
		if (!IsRayTracingAllowed() && !IsCreateShadersOnLoadEnabled() && GroupOnlyContainsRaytracingShaders(ShaderGroupIndex))
		{
			MarkPreloadEntrySkipped(ShaderGroupIndex
#if UE_SCA_DEBUG_PRELOADING
				, Callsite
#endif
			);
			continue;
		}
#endif

		// only shaders we actually want to preload should be marked as such, not just everything in the group
		DebugVisualizer.MarkExplicitlyPreloadedForVisualization(ShaderIndex);
		PreloadShaderGroup(ShaderGroupIndex, OutCompletionEvents
#if UE_SCA_DEBUG_PRELOADING
			, Callsite
#endif
		);
	}

	return true;
}

bool FIoStoreShaderCodeArchive::PreloadShaderMap(int32 ShaderMapIndex, FCoreDelegates::FAttachShaderReadRequestFunc AttachShaderReadRequestFunc)
{
	LLM_SCOPE(ELLMTag::Shaders);
	const FIoStoreShaderMapEntry& ShaderMapEntry = Header.ShaderMapEntries[ShaderMapIndex];

	FGraphEventArray Dummy;
#if UE_SCA_DEBUG_PRELOADING
	FString Callsite = FString::Printf(TEXT("PreloadShaderMap(AttachShaderReadRequestFunc) %d"), ShaderMapIndex);
#endif
	for (uint32 i = 0u; i < ShaderMapEntry.NumShaders; ++i)
	{
		const int32 ShaderIndex = Header.ShaderIndices[ShaderMapEntry.ShaderIndicesOffset + i];
		const int32 ShaderGroupIndex = GetGroupIndexForShader(ShaderIndex);

#if RHI_RAYTRACING
		if (!IsRayTracingAllowed() && !IsCreateShadersOnLoadEnabled() && GroupOnlyContainsRaytracingShaders(ShaderGroupIndex))
		{
			MarkPreloadEntrySkipped(ShaderGroupIndex
#if UE_SCA_DEBUG_PRELOADING
				, Callsite
#endif
			);
			continue;
		}
#endif

		// only shaders we actually want to preload should be marked as such, not just everything in the group
		DebugVisualizer.MarkExplicitlyPreloadedForVisualization(ShaderIndex);
		PreloadShaderGroup(ShaderGroupIndex, Dummy, 
#if UE_SCA_DEBUG_PRELOADING
			Callsite,
#endif
			&AttachShaderReadRequestFunc);
	}

	return true;
}

void FIoStoreShaderCodeArchive::ReleasePreloadEntry(int32 ShaderGroupIndex
#if UE_SCA_DEBUG_PRELOADING
	, const FString& CallsiteInfo
#endif
)
{
	FWriteScopeLock Lock(PreloadedShaderGroupsLock);
	FShaderGroupPreloadEntry** ExistingEntry = PreloadedShaderGroups.Find(ShaderGroupIndex);
	ensureMsgf(ExistingEntry, TEXT("Preload entry for shader group %d should exist if we're asked to release it"), ShaderGroupIndex);
	if (ExistingEntry)
	{
		FShaderGroupPreloadEntry* PreloadEntry = *ExistingEntry;

		const uint32 ShaderNumRefs = PreloadEntry->NumRefs--;
		check(ShaderNumRefs > 0u);

#if UE_SCA_DEBUG_PRELOADING
		FString AppendInfo = FString::Printf(TEXT("ReleasePreloadEntry: NumRefs %d -> %d    CallsiteInfo: %s\n"),
			ShaderNumRefs, PreloadEntry->NumRefs, *CallsiteInfo
			);
		PreloadEntry->DebugInfo.Append(AppendInfo);
#endif

		if (ShaderNumRefs == 1u)
		{
			if (!PreloadEntry->bNeverToBePreloaded)
			{
				PreloadEntry->IoRequest = FIoRequest();
				PreloadEntry->PreloadEvent.SafeRelease();
				const FIoStoreShaderGroupEntry& GroupEntry = Header.ShaderGroupEntries[ShaderGroupIndex];
				DEC_DWORD_STAT_BY(STAT_Shaders_ShaderPreloadMemory, (GroupEntry.CompressedSize + sizeof(FShaderGroupPreloadEntry)));
			}
			else
			{
				DEC_DWORD_STAT_BY(STAT_Shaders_ShaderPreloadMemory, sizeof(FShaderGroupPreloadEntry));
			}

#if UE_SCA_DEBUG_PRELOADING
			if (0)	// use this if you need comparison with all other groups
			{
				UE_LOG(LogInit, Log, TEXT("Group %d has still %d references on deletion. Group extended debug info: \n%s"), ShaderGroupIndex, PreloadEntry->NumRefs,
					*PreloadEntry->DebugInfo);
			}
#endif

			delete PreloadEntry;
			PreloadedShaderGroups.Remove(ShaderGroupIndex);
		}
	}
}

void FIoStoreShaderCodeArchive::ReleasePreloadedShader(int32 ShaderIndex)
{
	ReleasePreloadEntry(GetGroupIndexForShader(ShaderIndex)
#if UE_SCA_DEBUG_PRELOADING
		, FString::Printf(TEXT("ReleasePreloadedShader %d"), ShaderIndex)
#endif
	);
}

int32 FIoStoreShaderCodeArchive::FindShaderMapIndex(const FSHAHash& Hash)
{
	const uint32 Key = GetTypeHash(Hash);
	for (uint32 Index = ShaderMapHashTable.First(Key); ShaderMapHashTable.IsValid(Index); Index = ShaderMapHashTable.Next(Index))
	{
		if (Header.ShaderMapHashes[Index] == Hash)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

int32 FIoStoreShaderCodeArchive::FindShaderIndex(const FSHAHash& Hash)
{
	const uint32 Key = GetTypeHash(Hash);
	for (uint32 Index = ShaderHashTable.First(Key); ShaderHashTable.IsValid(Index); Index = ShaderHashTable.Next(Index))
	{
		if (Header.ShaderHashes[Index] == Hash)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

TRefCountPtr<FRHIShader> FIoStoreShaderCodeArchive::CreateShader(int32 ShaderIndex)
{
	LLM_SCOPE(ELLMTag::Shaders);
#if STATS
	double TimeFunctionEntered = FPlatformTime::Seconds();
	ON_SCOPE_EXIT
	{
		if (IsInRenderingThread())
		{
			double ShaderCreationTime = FPlatformTime::Seconds() - TimeFunctionEntered;
			INC_FLOAT_STAT_BY(STAT_Shaders_TotalRTShaderInitForRenderingTime, ShaderCreationTime);
		}
	};
#endif

	TRACE_CPUPROFILER_EVENT_SCOPE(FIoStoreShaderCodeArchive::CreateShader);

	TRefCountPtr<FRHIShader> Shader;

	const FIoStoreShaderCodeEntry& ShaderEntry = Header.ShaderEntries[ShaderIndex];
	int32 GroupIndex = GetGroupIndexForShader(ShaderIndex);

	// Preload shader group if it wasn't yet. This will also addref it so we can be sure it will exist.
	FGraphEventArray Dummy;
#if UE_SCA_DEBUG_PRELOADING
	FString Callsite = FString::Printf(TEXT("CreateShader %d"), ShaderIndex);
#endif
	PreloadShaderGroup(GroupIndex, Dummy
#if UE_SCA_DEBUG_PRELOADING
		, Callsite
#endif
	);

	FShaderGroupPreloadEntry* PreloadEntryPtr;
	{
		FReadScopeLock Lock(PreloadedShaderGroupsLock);
		PreloadEntryPtr = FindExistingPreloadEntry(GroupIndex);
	}

	// raise the prio if still ongoing
	if (!PreloadEntryPtr->IoRequest.Status().IsCompleted())
	{
		PreloadEntryPtr->IoRequest.UpdatePriority(IoDispatcherPriority_Max);
	}
	FGraphEventRef Event = PreloadEntryPtr->PreloadEvent;

	const bool bNeededToWait = Event.IsValid() && !Event->IsComplete();
	if (bNeededToWait)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(BlockingShaderLoad);

		const double TimeStarted = FPlatformTime::Seconds();
		FTaskGraphInterface::Get().WaitUntilTaskCompletes(Event);
		const double WaitDuration = FPlatformTime::Seconds() - TimeStarted;

		// only complain if we spent more than 1ms waiting
		if (WaitDuration > GShaderCodeLibraryMaxShaderPreloadWaitTime)
		{
			UE_LOG(LogShaderLibrary, Warning, TEXT("Spent %.2f ms in a blocking wait for shader preload, NumRefs: %d, FramePreloadStarted: %d, CurrentFrame: %d"), WaitDuration * 1000.0, PreloadEntryPtr->NumRefs, PreloadEntryPtr->FramePreloadStarted, GFrameNumber);
		}
	}

	const uint8* ShaderCode = PreloadEntryPtr->IoRequest.GetResultOrDie().Data();

	FMemStackBase& MemStack = FMemStack::Get();
	FMemMark Mark(MemStack);
	FIoStoreShaderGroupEntry& GroupEntry = Header.ShaderGroupEntries[GroupIndex];
	uint32 CompressedSize = PreloadEntryPtr->IoRequest.GetResultOrDie().DataSize();
	if (GroupEntry.IsGroupCompressed())
	{
		uint8* UncompressedCode = reinterpret_cast<uint8*>(MemStack.Alloc(GroupEntry.UncompressedSize, 16));
		DecompressShadergroupWithOodleAndExtraLogging(GroupIndex, Header.ShaderGroupIoHashes[GroupIndex], GroupEntry, ShaderIndex, ShaderEntry.ShaderGroupIndex, Header.ShaderHashes[ShaderIndex], UncompressedCode, GroupEntry.UncompressedSize, ShaderCode, CompressedSize);
		ShaderCode = reinterpret_cast<uint8*>(UncompressedCode) + ShaderEntry.UncompressedOffsetInGroup;

#if UE_SCA_VISUALIZE_SHADER_USAGE
		for (uint32 ShaderIdxIdx = GroupEntry.ShaderIndicesOffset, StopBeforeIdxIdx = GroupEntry.ShaderIndicesOffset + GroupEntry.NumShaders; ShaderIdxIdx < StopBeforeIdxIdx; ++ShaderIdxIdx)
		{
			DebugVisualizer.MarkDecompressedForVisualization(Header.ShaderIndices[ShaderIdxIdx]);
		}
#endif // UE_SCA_VISUALIZE_SHADER_USAGE
	}
	else
	{
		ShaderCode += ShaderEntry.UncompressedOffsetInGroup;
	}

	const auto ShaderCodeView = MakeArrayView(ShaderCode, Header.GetShaderUncompressedSize(ShaderIndex));
	const FSHAHash& ShaderHash = Header.ShaderHashes[ShaderIndex];
	switch (ShaderEntry.Frequency)
	{
	case SF_Vertex: Shader = RHICreateVertexShader(ShaderCodeView, ShaderHash); break;
	case SF_Mesh: Shader = RHICreateMeshShader(ShaderCodeView, ShaderHash); break;
	case SF_Amplification: Shader = RHICreateAmplificationShader(ShaderCodeView, ShaderHash); break;
	case SF_Pixel: Shader = RHICreatePixelShader(ShaderCodeView, ShaderHash); break;
	case SF_Geometry: Shader = RHICreateGeometryShader(ShaderCodeView, ShaderHash); break;
	case SF_Compute: Shader = RHICreateComputeShader(ShaderCodeView, ShaderHash); break;
	case SF_RayGen: case SF_RayMiss: case SF_RayHitGroup: case SF_RayCallable:
#if RHI_RAYTRACING
		if (GRHISupportsRayTracing && GRHISupportsRayTracingShaders)
		{
			Shader = RHICreateRayTracingShader(ShaderCodeView, ShaderHash, ShaderEntry.GetFrequency());
		}
#endif // RHI_RAYTRACING
		break;
	default: checkNoEntry(); break;
	}
	DebugVisualizer.MarkCreatedForVisualization(ShaderIndex);

	PreloadEntryPtr = nullptr;
	ReleasePreloadEntry(GroupIndex
#if UE_SCA_DEBUG_PRELOADING
		, Callsite
#endif
	);

	if (Shader)
	{
		INC_DWORD_STAT(STAT_Shaders_NumShadersUsedForRendering);
		Shader->SetHash(ShaderHash);
	}

	return Shader;
}

