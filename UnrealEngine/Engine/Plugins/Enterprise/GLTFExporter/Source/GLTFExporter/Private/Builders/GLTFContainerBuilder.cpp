// Copyright Epic Games, Inc. All Rights Reserved.

#include "Builders/GLTFContainerBuilder.h"
#include "Builders/GLTFMemoryArchive.h"

FGLTFContainerBuilder::FGLTFContainerBuilder(const FString& FileName, const UGLTFExportOptions* ExportOptions, const TSet<AActor*>& SelectedActors)
	: FGLTFConvertBuilder(FileName, ExportOptions, SelectedActors)
{
}

void FGLTFContainerBuilder::WriteInternalArchive(FArchive& Archive)
{
	ProcessSlowTasks();

	if (bIsGLB)
	{
		WriteGlbArchive(Archive);
	}
	else
	{
		WriteJsonArchive(Archive);
	}

	const TSet<EGLTFJsonExtension> CustomExtensions = GetCustomExtensionsUsed();
	if (CustomExtensions.Num() > 0)
	{
		const FString ExtensionsString = FString::JoinBy(CustomExtensions, TEXT(", "),
			[](EGLTFJsonExtension Extension)
		{
			return FGLTFJsonUtilities::GetValue(Extension);
		});

		LogWarning(FString::Printf(TEXT("Export uses some extensions that may only be supported in Unreal's glTF viewer: %s"), *ExtensionsString));
	}
}

bool FGLTFContainerBuilder::WriteAllFiles(const FString& DirPath, uint32 WriteFlags)
{
	FGLTFMemoryArchive Archive;
	WriteInternalArchive(Archive);

	const FString FilePath = FPaths::Combine(DirPath, FileName);
	if (!SaveToFile(FilePath, Archive, WriteFlags))
	{
		return false;
	}

	return WriteExternalFiles(DirPath, WriteFlags);
}

bool FGLTFContainerBuilder::WriteAllFiles(const FString& DirPath, TArray<FString>& OutFilePaths, uint32 WriteFlags)
{
	if (!WriteAllFiles(DirPath, WriteFlags))
	{
		return false;
	}

	GetAllFiles(OutFilePaths, DirPath);
	return true;
}

void FGLTFContainerBuilder::GetAllFiles(TArray<FString>& OutFilePaths, const FString& DirPath) const
{
	OutFilePaths.Add(FPaths::Combine(DirPath, FileName));
	GetExternalFiles(OutFilePaths, DirPath);
}

void FGLTFContainerBuilder::WriteGlbArchive(FArchive& Archive)
{
	FGLTFMemoryArchive JsonData;
	WriteJsonArchive(JsonData);

	const TArray64<uint8>* BufferData = GetBufferData();
	WriteGlb(Archive, JsonData, BufferData);
}

void FGLTFContainerBuilder::WriteGlb(FArchive& Archive, const TArray64<uint8>& JsonData, const TArray64<uint8>* BinaryData)
{
	constexpr uint32 JsonChunkType = 0x4E4F534A; // "JSON" in ASCII
	constexpr uint32 BinaryChunkType = 0x004E4942; // "BIN" in ASCII
	uint32 FileSize =
		3 * sizeof(uint32) +
		2 * sizeof(uint32) + GetPaddedChunkSize(JsonData.Num());

	if (BinaryData != nullptr)
	{
		FileSize += 2 * sizeof(uint32) + GetPaddedChunkSize(BinaryData->Num());
	}

	WriteHeader(Archive, FileSize);
	WriteChunk(Archive, JsonChunkType, JsonData, 0x20);

	if (BinaryData != nullptr)
	{
		WriteChunk(Archive, BinaryChunkType, *BinaryData, 0x0);
	}
}

void FGLTFContainerBuilder::WriteHeader(FArchive& Archive, uint32 FileSize)
{
	constexpr uint32 FileSignature = 0x46546C67; // "glTF" in ASCII
	constexpr uint32 FileVersion = 2;

	WriteInt(Archive, FileSignature);
	WriteInt(Archive, FileVersion);
	WriteInt(Archive, FileSize);
}

void FGLTFContainerBuilder::WriteChunk(FArchive& Archive, uint32 ChunkType, const TArray64<uint8>& ChunkData, uint8 ChunkTrailingByte)
{
	const uint32 ChunkLength = GetPaddedChunkSize(ChunkData.Num());
	const uint32 ChunkTrailing = GetTrailingChunkSize(ChunkData.Num());

	WriteInt(Archive, ChunkLength);
	WriteInt(Archive, ChunkType);
	WriteData(Archive, ChunkData);
	WriteFill(Archive, ChunkTrailing, ChunkTrailingByte);
}

void FGLTFContainerBuilder::WriteInt(FArchive& Archive, uint32 Value)
{
	Archive.SerializeInt(Value, MAX_uint32);
}

void FGLTFContainerBuilder::WriteData(FArchive& Archive, const TArray64<uint8>& Data)
{
	Archive.Serialize(const_cast<uint8*>(Data.GetData()), Data.Num());
}

void FGLTFContainerBuilder::WriteFill(FArchive& Archive, int32 Size, uint8 Value)
{
	while (--Size >= 0)
	{
		Archive.Serialize(&Value, sizeof(Value));
	}
}

int32 FGLTFContainerBuilder::GetPaddedChunkSize(int32 Size)
{
	return (Size + 3) & ~3;
}

int32 FGLTFContainerBuilder::GetTrailingChunkSize(int32 Size)
{
	return (4 - (Size & 3)) & 3;
}
