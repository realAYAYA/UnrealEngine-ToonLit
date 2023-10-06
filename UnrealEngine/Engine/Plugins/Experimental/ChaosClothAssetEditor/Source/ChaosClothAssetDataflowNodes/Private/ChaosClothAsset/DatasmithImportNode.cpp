// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/DatasmithImportNode.h"
#include "ChaosClothAsset/ClothAsset.h"
#include "ChaosClothAsset/CollectionClothFacade.h"
#include "Dataflow/DataflowInputOutput.h"
#include "DatasmithImportContext.h"
#include "DatasmithImportFactory.h"
#include "ExternalSource.h"
#include "ExternalSourceModule.h"
#include "SourceUri.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DatasmithImportNode)

#define LOCTEXT_NAMESPACE "ChaosClothAssetDatasmithImportNode"

FChaosClothAssetDatasmithImportNode::FChaosClothAssetDatasmithImportNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowNode(InParam, InGuid)
{
	RegisterOutputConnection(&Collection);
}

void FChaosClothAssetDatasmithImportNode::Serialize(FArchive& Archive)
{
	using namespace UE::Chaos::ClothAsset;

	::Chaos::FChaosArchive ChaosArchive(Archive);
	ImportCache.Serialize(ChaosArchive);
	if (Archive.IsLoading())
	{
		// Make sure to always have a valid cloth collection on reload, some new attributes could be missing from the cached collection
		const TSharedRef<FManagedArrayCollection> ClothCollection = MakeShared<FManagedArrayCollection>(MoveTemp(ImportCache));
		FCollectionClothFacade ClothFacade(ClothCollection);
		if (!ClothFacade.IsValid())
		{
			ClothFacade.DefineSchema();
		}
		ImportCache = MoveTemp(*ClothCollection);
	}
	Archive << ImportHash;
}

bool FChaosClothAssetDatasmithImportNode::EvaluateImpl(Dataflow::FContext& Context, FManagedArrayCollection& OutCollection) const
{
	using namespace UE::DatasmithImporter;

	const FSourceUri SourceUri = FSourceUri::FromFilePath(ImportFile.FilePath);
	const TSharedPtr<FExternalSource> ExternalSource = IExternalSourceModule::GetOrCreateExternalSource(SourceUri);
	if (!ExternalSource)
	{
		return false;
	}

	constexpr bool bLoadConfig = false; // TODO: Not sure what this does
	const FName LoggerName(TEXT("ImportDatasmithClothNode"));
	const FText LoggerLabel(NSLOCTEXT("ImportDatasmithClothNode", "LoggerLabel", "ImportDatasmithClothNode"));
	FDatasmithImportContext DatasmithImportContext(ExternalSource, bLoadConfig, LoggerName, LoggerLabel);

	const FName PackageName = MakeUniqueObjectName(nullptr, UPackage::StaticClass(), FName(FPaths::Combine(GetTransientPackage()->GetPathName(), ExternalSource->GetSourceName())));
	const TStrongObjectPtr<UPackage> DestinationPackage(CreatePackage(*PackageName.ToString()));
	if (!ensure(DestinationPackage))
	{
		// Failed to create the package to hold this asset for some reason
		return false;
	}

	// Don't create the Actors in the level, just read the Assets
	DatasmithImportContext.Options->BaseOptions.SceneHandling = EDatasmithImportScene::AssetsOnly;

	constexpr EObjectFlags NewObjectFlags = RF_Public | RF_Transactional | RF_Transient | RF_Standalone;
	const TSharedPtr<FJsonObject> ImportSettingsJson;
	constexpr bool bIsSilent = true;

	if (!DatasmithImportContext.Init(DestinationPackage->GetPathName(), NewObjectFlags, GWarn, ImportSettingsJson, bIsSilent))
	{
		return false;
	}

	if (const TSharedPtr<IDatasmithScene> LoadedScene = ExternalSource->TryLoad())
	{
		DatasmithImportContext.InitScene(LoadedScene.ToSharedRef());
	}
	else
	{
		return false;
	}

	bool bUserCancelled = false;
	bool bImportSucceed = DatasmithImportFactoryImpl::ImportDatasmithScene(DatasmithImportContext, bUserCancelled);
	bImportSucceed &= !bUserCancelled;

	if (bImportSucceed && DatasmithImportContext.ImportedClothes.Num() > 0)
	{
		UObject* const ClothObject = DatasmithImportContext.ImportedClothes.CreateIterator()->Value;
		UChaosClothAsset* const DatasmithClothAsset = Cast<UChaosClothAsset>(ClothObject);
		if (ensure(DatasmithClothAsset))
		{
			if (DatasmithClothAsset->GetClothCollections().Num() > 0)
			{
				DatasmithClothAsset->GetClothCollections()[0]->CopyTo(&OutCollection);
				return true;
			}
		}
		DatasmithClothAsset->ClearFlags(RF_Standalone);
	}

	return false;
}

void FChaosClothAssetDatasmithImportNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		const FMD5Hash FileHash = ImportFile.FilePath.IsEmpty() ?
			FMD5Hash() :  // Reset to an empty cloth collection
			FPaths::FileExists(ImportFile.FilePath) ?
				FMD5Hash::HashFile(*ImportFile.FilePath) :  // Update cache to the file content if necessary
				ImportHash;  // Keep the current cached file

		if (FileHash != ImportHash)
		{
			ImportHash = FileHash;

			// The file has changed, reimport
			const bool bSuccess = EvaluateImpl(Context, ImportCache);

			if (!bSuccess)
			{
				using namespace UE::Chaos::ClothAsset;

				// The import has failed, reset the cache to an empty cloth collection
				ImportCache.Reset();

				const TSharedRef<FManagedArrayCollection> ClothCollection = MakeShared<FManagedArrayCollection>(MoveTemp(ImportCache));
				FCollectionClothFacade CollectionClothFacade(ClothCollection);
				CollectionClothFacade.DefineSchema();

				ImportCache = MoveTemp(*ClothCollection);
			}
		}

		SetValue(Context, ImportCache, &Collection);
	}
}

#undef LOCTEXT_NAMESPACE
