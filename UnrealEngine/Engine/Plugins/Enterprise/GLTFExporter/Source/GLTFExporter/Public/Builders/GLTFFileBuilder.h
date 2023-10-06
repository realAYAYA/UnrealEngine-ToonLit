// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Builders/GLTFTaskBuilder.h"
#include "Builders/GLTFMemoryArchive.h"

class GLTFEXPORTER_API FGLTFFileBuilder : public FGLTFTaskBuilder
{
public:

	FGLTFFileBuilder(const FString& FileName, const UGLTFExportOptions* ExportOptions = nullptr);

	FString AddExternalFile(const FString& DesiredURI, const TSharedPtr<FGLTFMemoryArchive>& Archive);

	void GetExternalFiles(TArray<FString>& OutFilePaths, const FString& DirPath = TEXT("")) const;

	const TMap<FString, TSharedPtr<FGLTFMemoryArchive>>& GetExternalArchives() const;

	bool WriteExternalFiles(const FString& DirPath, uint32 WriteFlags = 0);

private:

	TMap<FString, TSharedPtr<FGLTFMemoryArchive>> ExternalArchives;

	FString GetUniqueFileName(const FString& InFileName) const;

	static FString SanitizeFileName(const FString& InFileName);

	static FString EncodeURI(const FString& InFileName);

protected:

	bool SaveToFile(const FString& FilePath, const TArray64<uint8>& FileData, uint32 WriteFlags);
};
