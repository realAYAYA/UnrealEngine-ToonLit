// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Builders/GLTFTaskBuilder.h"
#include "Builders/GLTFMemoryArchive.h"

class GLTFEXPORTER_API FGLTFFileBuilder : public FGLTFTaskBuilder
{
public:

	FGLTFFileBuilder(const FString& FileName, const UGLTFExportOptions* ExportOptions = nullptr);

	FString AddExternalFile(const FString& URI, const TSharedPtr<FGLTFMemoryArchive>& Archive = MakeShared<FGLTFMemoryArchive>());

	void GetExternalFiles(TArray<FString>& OutFilePaths, const FString& DirPath = TEXT("")) const;

	const TMap<FString, TSharedPtr<FGLTFMemoryArchive>>& GetExternalArchives() const;

	bool WriteExternalFiles(const FString& DirPath, uint32 WriteFlags = 0);

private:

	TMap<FString, TSharedPtr<FGLTFMemoryArchive>> ExternalArchives;

	FString GetUniqueURI(const FString& URI) const;

protected:

	bool SaveToFile(const FString& FilePath, const TArray64<uint8>& FileData, uint32 WriteFlags);
};
