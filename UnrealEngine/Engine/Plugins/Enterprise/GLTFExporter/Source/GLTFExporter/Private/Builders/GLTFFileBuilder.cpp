// Copyright Epic Games, Inc. All Rights Reserved.

#include "Builders/GLTFFileBuilder.h"
#include "Misc/FileHelper.h"

FGLTFFileBuilder::FGLTFFileBuilder(const FString& FileName, const UGLTFExportOptions* ExportOptions)
	: FGLTFTaskBuilder(FileName, ExportOptions)
{
}

FString FGLTFFileBuilder::AddExternalFile(const FString& URI, const TSharedPtr<FGLTFMemoryArchive>& Archive)
{
	const FString UnqiueURI = GetUniqueURI(URI);
	ExternalArchives.Add(UnqiueURI, Archive);
	return UnqiueURI;
}

void FGLTFFileBuilder::GetExternalFiles(TArray<FString>& OutFilePaths, const FString& DirPath) const
{
	for (const TPair<FString, TSharedPtr<FGLTFMemoryArchive>>& ExternalFile : ExternalArchives)
	{
		const FString FilePath = FPaths::Combine(DirPath, *ExternalFile.Key);
		OutFilePaths.Add(FilePath);
	}
}

const TMap<FString, TSharedPtr<FGLTFMemoryArchive>>& FGLTFFileBuilder::GetExternalArchives() const
{
	return ExternalArchives;
}

bool FGLTFFileBuilder::WriteExternalFiles(const FString& DirPath, uint32 WriteFlags)
{
	for (const TPair<FString, TSharedPtr<FGLTFMemoryArchive>>& ExternalFile : ExternalArchives)
	{
		const FString FilePath = FPaths::Combine(DirPath, *ExternalFile.Key);
		const TArray64<uint8>& FileData = *ExternalFile.Value;

		if (!SaveToFile(FilePath, FileData, WriteFlags))
		{
			return false;
		}
	}

	return true;
}

FString FGLTFFileBuilder::GetUniqueURI(const FString& URI) const
{
	if (!ExternalArchives.Contains(URI))
	{
		return URI;
	}

	const FString BaseFilename = FPaths::GetBaseFilename(URI);
	const FString FileExtension = FPaths::GetExtension(URI, true);
	FString UnqiueURI;

	int32 Suffix = 1;
	do
	{
		UnqiueURI = BaseFilename + TEXT('_') + FString::FromInt(Suffix) + FileExtension;
		Suffix++;
	}
	while (ExternalArchives.Contains(UnqiueURI));

	return UnqiueURI;
}

bool FGLTFFileBuilder::SaveToFile(const FString& FilePath, const TArray64<uint8>& FileData, uint32 WriteFlags)
{
	if (!FFileHelper::SaveArrayToFile(FileData, *FilePath, &IFileManager::Get(), WriteFlags))
	{
		LogError(FString::Printf(TEXT("Failed to save file: %s"), *FilePath));
		return false;
	}

	return true;
}
