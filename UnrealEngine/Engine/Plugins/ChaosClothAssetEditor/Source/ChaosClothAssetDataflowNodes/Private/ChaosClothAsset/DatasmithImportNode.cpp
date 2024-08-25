// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/DatasmithImportNode.h"
#include "ChaosClothAsset/ClothAsset.h"
#include "ChaosClothAsset/ClothComponent.h"
#include "ChaosClothAsset/CollectionClothFacade.h"
#include "Dataflow/DataflowInputOutput.h"
#include "Features/IModularFeatures.h"
#include "DatasmithImportContext.h"
#include "DatasmithImportFactory.h"
#include "ExternalSource.h"
#include "ExternalSourceModule.h"
#include "SourceUri.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DatasmithImportNode)

#define LOCTEXT_NAMESPACE "ChaosClothAssetDatasmithImportNode"

UChaosClothAssetDatasmithClothAssetFactory::UChaosClothAssetDatasmithClothAssetFactory() = default;
UChaosClothAssetDatasmithClothAssetFactory::~UChaosClothAssetDatasmithClothAssetFactory() = default;

UObject* UChaosClothAssetDatasmithClothAssetFactory::CreateClothAsset(UObject* Outer, const FName& Name, EObjectFlags Flags) const
{
	return CastChecked<UObject>(NewObject<UChaosClothAsset>(Outer, Name, Flags));
}

UObject* UChaosClothAssetDatasmithClothAssetFactory::DuplicateClothAsset(UObject* ClothAsset, UObject* Outer, const FName& Name) const
{
	return CastChecked<UObject>(DuplicateObject<UChaosClothAsset>(CastChecked<UChaosClothAsset>(ClothAsset), Outer, Name));
}

void UChaosClothAssetDatasmithClothAssetFactory::InitializeClothAsset(UObject* ClothAsset, const FDatasmithCloth& DatasmithCloth) const
{
	using namespace UE::Chaos::ClothAsset;

	UChaosClothAsset* const ChaosClothAsset = CastChecked<UChaosClothAsset>(ClothAsset);

	TArray<TSharedRef<FManagedArrayCollection>>& Collections = ChaosClothAsset->GetClothCollections();
	Collections.Reset(1);
	FCollectionClothFacade Cloth(Collections.Emplace_GetRef(MakeShared<FManagedArrayCollection>()));
	Cloth.DefineSchema();

	for (const FDatasmithClothPattern& Pattern : DatasmithCloth.Patterns)
	{
		if (Pattern.IsValid())
		{
			FCollectionClothSimPatternFacade ClothPattern = Cloth.AddGetSimPattern();
			ClothPattern.Initialize(Pattern.SimPosition, Pattern.SimRestPosition, Pattern.SimTriangleIndices);
		}
	}

	for (const FDatasmithClothSewingInfo& SeamInfo : DatasmithCloth.Sewing)
	{
		const int32 SeamPattern0 = (int32)SeamInfo.Seam0PanelIndex;
		const int32 SeamPattern1 = (int32)SeamInfo.Seam1PanelIndex;

		if (SeamPattern0 >= 0 && SeamPattern0 < Cloth.GetNumSimPatterns() &&
			SeamPattern1 >= 0 && SeamPattern1 < Cloth.GetNumSimPatterns())
		{
			const FCollectionClothSimPatternConstFacade ClothPattern0 = Cloth.GetSimPattern(SeamPattern0);
			const FCollectionClothSimPatternConstFacade ClothPattern1 = Cloth.GetSimPattern(SeamPattern1);

			const int32 ClothPattern0VerticesOffset = ClothPattern0.GetSimVertices2DOffset();
			const int32 ClothPattern1VerticesOffset = ClothPattern1.GetSimVertices2DOffset();

			TArray<FIntVector2> Stitches;
			const uint32 StitchesCount = FMath::Min(SeamInfo.Seam0MeshIndices.Num(), SeamInfo.Seam1MeshIndices.Num());
			Stitches.Reserve(StitchesCount);
			for (uint32 StitchIndex = 0; StitchIndex < StitchesCount; ++StitchIndex)
			{
				Stitches.Emplace(
					(int32)SeamInfo.Seam0MeshIndices[StitchIndex] + ClothPattern0VerticesOffset,
					(int32)SeamInfo.Seam1MeshIndices[StitchIndex] + ClothPattern1VerticesOffset);
			}

			FCollectionClothSeamFacade SeamFacade = Cloth.AddGetSeam();
			SeamFacade.Initialize(Stitches);
		}
	}

	// Set the render mesh to duplicate the sim mesh
	ChaosClothAsset->CopySimMeshToRenderMesh();

	// Set a default skeleton and rebuild the asset
	ChaosClothAsset->SetReferenceSkeleton(nullptr);  // This creates a default reference skeleton, redoes the bindings, and rebuilds the asset
}

UChaosClothAssetDatasmithClothComponentFactory::UChaosClothAssetDatasmithClothComponentFactory() = default;
UChaosClothAssetDatasmithClothComponentFactory::~UChaosClothAssetDatasmithClothComponentFactory() = default;

USceneComponent* UChaosClothAssetDatasmithClothComponentFactory::CreateClothComponent(UObject* Outer) const
{
	return CastChecked<USceneComponent>(NewObject<UChaosClothComponent>(Outer));
}

void UChaosClothAssetDatasmithClothComponentFactory::InitializeClothComponent(USceneComponent* ClothComponent, UObject* ClothAsset, USceneComponent* RootComponent) const
{
	UChaosClothComponent* const ChaosClothComponent = CastChecked<UChaosClothComponent>(ClothComponent);
	ChaosClothComponent->SetClothAsset(Cast<UChaosClothAsset>(ClothAsset));
	ChaosClothComponent->SetupAttachment(RootComponent);
}

namespace UE::Chaos::ClothAsset::Private
{
	/** A modular interface to provide and initialize cloth asset objects. */
	class FDatasmithClothFactoryClassesProvider final: public IDatasmithClothFactoryClassesProvider
	{
	public:
		FDatasmithClothFactoryClassesProvider() = default;
		virtual ~FDatasmithClothFactoryClassesProvider() override = default;

		virtual FName GetName() const override
		{
			static const FName Name = TEXT("ChaosClothAsset");
			return Name;
		}

		virtual TSubclassOf<UDatasmithClothAssetFactory> GetClothAssetFactoryClass() const override
		{
			return TSubclassOf<UDatasmithClothAssetFactory>(UChaosClothAssetDatasmithClothAssetFactory::StaticClass());
		}

		virtual TSubclassOf<UDatasmithClothComponentFactory> GetClothComponentFactoryClass() const override
		{
			return TSubclassOf<UDatasmithClothComponentFactory>(UChaosClothAssetDatasmithClothComponentFactory::StaticClass());
		}
	};

	static FDatasmithClothFactoryClassesProvider DatasmithClothFactoryClassesProvider;
}

void FChaosClothAssetDatasmithImportNode::RegisterModularFeature()
{
	using namespace UE::Chaos::ClothAsset::Private;
	IModularFeatures::Get().RegisterModularFeature(IDatasmithClothFactoryClassesProvider::FeatureName, &DatasmithClothFactoryClassesProvider);
}

void FChaosClothAssetDatasmithImportNode::UnregisterModularFeature()
{
	using namespace UE::Chaos::ClothAsset::Private;
	IModularFeatures::Get().UnregisterModularFeature(IDatasmithClothFactoryClassesProvider::FeatureName, &DatasmithClothFactoryClassesProvider);
}

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
