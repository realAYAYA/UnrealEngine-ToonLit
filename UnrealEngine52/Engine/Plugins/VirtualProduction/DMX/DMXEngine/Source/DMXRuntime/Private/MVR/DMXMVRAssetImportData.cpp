// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVR/DMXMVRAssetImportData.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"


#if WITH_EDITOR
void UDMXMVRAssetImportData::SetSourceFile(const FString& InFilePathAndName)
{
	FilePathAndName = InFilePathAndName;

	FAssetImportInfo Info;
	Info.Insert(FAssetImportInfo::FSourceFile(FilePathAndName));
	SourceData = Info;

	if (!FPaths::FileExists(FilePathAndName))
	{
		return;
	}

	RawSourceData.ByteArray.Reset();
	FFileHelper::LoadFileToArray(RawSourceData.ByteArray, *FilePathAndName);
}
#endif // WITH_EDITOR

void UDMXMVRAssetImportData::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	FAssetImportInfo Info;
	Info.Insert(FAssetImportInfo::FSourceFile(FilePathAndName));
	SourceData = Info;
#endif
}
