// Copyright Epic Games, Inc. All Rights Reserved.

#include "DatasmithAssetImportData.h"

#include "DatasmithContentModule.h"
#include "DatasmithScene.h"

#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/EnterpriseObjectVersion.h"

#if WITH_EDITORONLY_DATA
void FDatasmithImportInfo::GetAssetRegistryTags(TArray<UObject::FAssetRegistryTag>& OutTags) const
{
	using FAssetRegistryTag = UObject::FAssetRegistryTag;

	OutTags.Add(FAssetRegistryTag(TEXT("SourceUri"), SourceUri, FAssetRegistryTag::TT_Hidden));
	OutTags.Add(FAssetRegistryTag(TEXT("SourceHash"), SourceHash, FAssetRegistryTag::TT_Hidden));
}

#if WITH_EDITOR
bool UDatasmithSceneImportData::CanEditChange(const FProperty* InProperty) const
{
	if (!Super::CanEditChange(InProperty))
	{
		return false;
	}

	if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(FDatasmithImportBaseOptions, bIncludeAnimation))
	{
		return BaseOptions.CanIncludeAnimation();
	}

	return true;
}
#endif //WITH_EDITOR

UDatasmithStaticMeshImportData* UDatasmithStaticMeshImportData::GetImportDataForStaticMesh( UStaticMesh* StaticMesh, TOptional< DefaultOptionsPair > DefaultImportOptions )
{
	if( !StaticMesh )
	{
		return nullptr;
	}

	UDatasmithStaticMeshImportData* ImportData = nullptr;

	ImportData = Cast< UDatasmithStaticMeshImportData >( StaticMesh->AssetImportData );
	if ( !ImportData )
	{
		ImportData = NewObject< UDatasmithStaticMeshImportData >( StaticMesh );

		if ( DefaultImportOptions.IsSet() )
		{
			ImportData->ImportOptions = DefaultImportOptions.GetValue().Key;
			ImportData->AssetImportOptions = DefaultImportOptions.GetValue().Value;
		}

		// Try to preserve the source file path if possible
		if ( StaticMesh->AssetImportData != nullptr )
		{
			ImportData->SourceData = StaticMesh->AssetImportData->SourceData;
		}

		StaticMesh->AssetImportData = ImportData;
	}

	return ImportData;
}

UDatasmithStaticMeshCADImportData * UDatasmithStaticMeshCADImportData::GetCADImportDataForStaticMesh(UStaticMesh * StaticMesh, TOptional< DefaultOptionsTuple > DefaultImportOptions)
{
	check( StaticMesh );

	UDatasmithStaticMeshCADImportData* ImportData = Cast< UDatasmithStaticMeshCADImportData >( StaticMesh->AssetImportData );

	if ( !ImportData )
	{
		ImportData = NewObject< UDatasmithStaticMeshCADImportData >( StaticMesh );


		// Try to preserve the source file path if possible
		if ( StaticMesh->AssetImportData != nullptr )
		{
			ImportData->SourceData = StaticMesh->AssetImportData->SourceData;
		}

		StaticMesh->AssetImportData = ImportData;
	}

	if ( DefaultImportOptions.IsSet() )
	{
		DefaultOptionsTuple& Value = DefaultImportOptions.GetValue();

		ImportData->TessellationOptions = Value.Get<0>();
		ImportData->ImportOptions = Value.Get<1>();
		ImportData->AssetImportOptions = Value.Get<2>();
	}

	return ImportData;
}

void UDatasmithStaticMeshCADImportData::SetResourcePath(const FString& FilePath)
{
	check(FPaths::FileExists(FilePath));

	ResourceFilename = FPaths::GetCleanFilename(FilePath);

	static TArray<FString> AuxiliaryExtentions = {
		TEXT(".ext"),
	};

	AuxiliaryFilenames.Empty(AuxiliaryExtentions.Num());
	for (const FString& Ext : AuxiliaryExtentions)
	{
		FString AuxililaryFilePath = FilePath + Ext;
		if (FPaths::FileExists(AuxililaryFilePath))
		{
			AuxiliaryFilenames.Add(FPaths::GetCleanFilename(AuxililaryFilePath));
		}
	}

	// Set ResourcePath as absolute path because CoreTech expects an absolute path
	ResourcePath = FPaths::ConvertRelativePathToFull(FilePath);
}

const FString& UDatasmithStaticMeshCADImportData::GetResourcePath()
{
	check(!ResourcePath.IsEmpty());

	return ResourcePath;
}
#endif // WITH_EDITORONLY_DATA

void UDatasmithStaticMeshCADImportData::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FEnterpriseObjectVersion::GUID);

	// Serialize/Deserialize stripping flag to control serialization of tessellation data
	bool bIsEditorDataIncluded = true;
	if (Ar.CustomVer(FEnterpriseObjectVersion::GUID) >= FEnterpriseObjectVersion::FixSerializationOfBulkAndExtraData)
	{
		FStripDataFlags StripFlags( Ar );
		bIsEditorDataIncluded = !StripFlags.IsEditorDataStripped();
	}

#if WITH_EDITORONLY_DATA
	// Do not bother loading or saving the rest if there is no source file or Editor's data are not included
	if (!bIsEditorDataIncluded || SourceData.SourceFiles.Num() == 0)
	{
		return;
	}

	FString ResourceDirectory;
	TArray<uint8> ByteArray;
	int32 AuxiliaryCount = AuxiliaryFilenames.Num();

	if (Ar.IsSaving())
	{
		ResourceDirectory = FPaths::GetPath(ResourcePath);
		FFileHelper::LoadFileToArray(ByteArray, *ResourcePath);
	}
	else if (Ar.IsLoading())
	{
		FString BaseName = FPaths::GetBaseFilename(SourceData.SourceFiles[0].RelativeFilename);
		ResourceDirectory = FPaths::Combine(IDatasmithContentModule::Get().GetTempDir(), TEXT("ReimportCache"), *BaseName);
		if (!FPaths::DirectoryExists(ResourceDirectory))
		{
			IFileManager::Get().MakeDirectory(*ResourceDirectory);
		}
	}

	Ar << ByteArray;

	if (Ar.IsSaving())
	{
		// Save each auxiliary file content
		for (FString& Filename : AuxiliaryFilenames)
		{
			FString FilePath = FPaths::Combine(ResourceDirectory, Filename);

			ByteArray.Empty();
			FFileHelper::LoadFileToArray(ByteArray, *FilePath);

			Ar << ByteArray;
		}
	}
	else if (Ar.IsLoading())
	{
		ResourcePath = FPaths::ConvertRelativePathToFull(FPaths::Combine(ResourceDirectory, ResourceFilename));
		FFileHelper::SaveArrayToFile(ByteArray, *ResourcePath);

		// Save each auxiliary file content
		for (FString& Filename : AuxiliaryFilenames)
		{
			ByteArray.Empty();
			Ar << ByteArray;

			FString FilePath = FPaths::Combine(ResourceDirectory, Filename);
			FFileHelper::SaveArrayToFile(ByteArray, *FilePath);
		}
	}
#endif // WITH_EDITORONLY_DATA
}

// Helper function required since UObject doesn't have an interface to access AssetImportData
UAssetImportData* Datasmith::GetAssetImportData(UObject* Asset)
{
#if WITH_EDITOR
	if (UStaticMesh* Mesh = Cast<UStaticMesh>(Asset))
	{
		return Mesh->AssetImportData;
	}
	else if (UTexture* Texture = Cast<UTexture>(Asset))
	{
		return Texture->AssetImportData;
	}
	else if (UMaterialInterface* Material = Cast<UMaterialInterface>(Asset))
	{
		return Material->AssetImportData;
	}
#endif // WITH_EDITOR

	return nullptr;
}

