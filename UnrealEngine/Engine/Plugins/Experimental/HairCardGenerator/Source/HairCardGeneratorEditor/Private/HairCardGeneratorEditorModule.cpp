// Copyright Epic Games, Inc. All Rights Reserved.

#include "IHairCardGeneratorEditor.h"
#include "Engine/Texture2D.h"
#include "Factories/TextureFactory.h"
#include "HairCardGeneratorLog.h"
#include "HairCardGenOptionsWindow.h"
#include "HairCardGeneratorPluginSettings.h"
#include "HairCardGenControllerBase.h"
#include "HairCardGeneratorEditorSettings.h"
#include "HairCardGenCardSubdivider.h"
#include "HairCardGenStrandNCSInterpolator.h"
#include "HairCardGenStrandFilter.h"
#include "HairCardGenSettingsDetail.h"

#include "AssetToolsModule.h"
#include "PropertyEditorModule.h"
#include "AutomatedAssetImportData.h"
#include "GroomAsset.h"
#include "GroomAssetCards.h" // for GetHairTextureLayoutTextureCount
#include "GroomCacheData.h" // for EGroomBasisType, EGroomCurveType
#include "HairDescription.h"
#include "ObjectTools.h"
#include "AssetRegistry/AssetRegistryModule.h" // for GetAssetByObjectPath()
#include "Engine/StaticMesh.h"
#include "HAL/IConsoleManager.h"
#include "HAL/LowLevelMemTracker.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "Misc/ScopedSlowTask.h"
#include "UObject/UObjectHash.h" // for GetObjectsOfClass()
#include "UObject/WeakObjectPtrTemplates.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

DEFINE_LOG_CATEGORY(LogHairCardGenerator)
#define LOCTEXT_NAMESPACE "HairCardGeneratorEd"

LLM_DEFINE_TAG(HairCardGenerator);

/* Used for keeping track of the editor settings for importing generated data
*/
struct FHairCardImportSettings
{
	FString MeshName;
	TArray<FString> TextureNames;
};


/* FHairCardGeneratorHelper Declaration
 *****************************************************************************/

// Singleton class for caching a pointer to the hair card generator
class FHairCardGenController
{
public:
	static TObjectPtr<UHairCardGenControllerBase> GetHairCardGenerator();

private:
	static UGroomAsset* GroomAsset;
	static TWeakObjectPtr<UHairCardGenControllerBase> HairCardGenController;
};

TWeakObjectPtr<UHairCardGenControllerBase> FHairCardGenController::HairCardGenController = nullptr;

TObjectPtr<UHairCardGenControllerBase> FHairCardGenController::GetHairCardGenerator()
{
	if ( HairCardGenController.IsValid() )
		return HairCardGenController.Get();

	// Find the HairCardGenController in Python (or wherever it may be implemented -- could be moved to C++/Blueprints/etc.)
	TArray<UObject*> HairCardGenControllers;
	GetObjectsOfClass(UHairCardGenControllerBase::StaticClass(), HairCardGenControllers);

	if ( HairCardGenControllers.Num() == 0 )
	{
		UE_LOG(LogHairCardGenerator, Error, TEXT("Failed to find an active hair card controller."));
		return nullptr;
	}

	if ( HairCardGenControllers.Num() != 1 )
		UE_LOG(LogHairCardGenerator, Warning, TEXT("More than one active hair-card controller (using first controller found)."))

	HairCardGenController = TWeakObjectPtr<UHairCardGenControllerBase>(static_cast<UHairCardGenControllerBase*>(HairCardGenControllers[0]));
	return HairCardGenController.Get();
}

/* HairCardGeneratorEditor_Impl Declaration
 *****************************************************************************/

// Static namespace with helper functions for commands/generation tasks
namespace HairCardGeneratorEditor_Impl
{
	static TObjectPtr<UGroomAsset> ParseGroomAssetArg(const TArray<FString>& Args, bool bOptional);
	static FHairGroupsCardsSourceDescription* FindCardSourceDescription(TObjectPtr<UGroomAsset> Groom, int32 LODIndex);
	static TObjectPtr<UHairCardGeneratorPluginSettings> CreateGroomSettingsLast(TObjectPtr<UGroomAsset> Groom, const FHairGroupsCardsSourceDescription& SourceDesc);

	static void DbgCmdCacheGroom(const TArray<FString>& Args);

	static bool LoadGroomData(UGroomAsset* GroomAsset, const bool SaveCached);
	static bool BuildHairCardGenGroomData(const int PointsPerStrand, const FHairDescription& InGroomData, FHairCardGen_GroomData& Out);
	static bool GenerateCardsForCardGroup(TObjectPtr<const UHairCardGeneratorPluginSettings> Settings, int index, uint8 GenFlags);
	static bool GenerateCardsGeometry(TObjectPtr<const UHairCardGeneratorPluginSettings> Settings, int index);
	static void GetTextureImportNames(UHairCardGeneratorPluginSettings* Settings, FHairCardImportSettings& ImportSettings);
	static TArray<UObject*> ImportHairCardTextures(const FString& ImportSrc, const FString& ImportDst, const FHairCardImportSettings& ImportSettings);
	static UStaticMesh* CreateMeshInPackage(const FString& DestinationPackage, const FString& BaseFilename, int GroupID);
	static int32 AssignImportedTextures(const TArray<UObject*>& ImportedAssets, const FHairCardImportSettings& ImportSettings, FHairGroupsCardsSourceDescription& Assignee);
	static void CreateFinishNotification(bool bSuccess, const FText& NotificationText);

	static TWeakObjectPtr<UGroomAsset> LastGroom = nullptr;
}


static FAutoConsoleCommand CCmdLoadGroom(
	TEXT("HairCardSolver.CacheGroom"),
	TEXT(""),
	FConsoleCommandWithArgsDelegate::CreateStatic(HairCardGeneratorEditor_Impl::DbgCmdCacheGroom));

/* HairCardGeneratorEditor_Impl Definition
 *****************************************************************************/

// Always assume the first argument is a (sometimes optional) groom argument and get UGroomAsset, if possible
static TObjectPtr<UGroomAsset> HairCardGeneratorEditor_Impl::ParseGroomAssetArg(const TArray<FString>& Args, bool bOptional)
{
	if ( Args.Num() < 1 )
		return nullptr;

	FString AssetPath = Args[0];
	FSoftObjectPath CheckPath(AssetPath);

	if ( !CheckPath.IsValid() )
	{
		UE_CLOG(!bOptional, LogHairCardGenerator, Error, TEXT("The path is not a valid asset path."));
		return nullptr;
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	FAssetData ExistingAsset = AssetRegistryModule.Get().GetAssetByObjectPath(CheckPath);
	if (!ExistingAsset.IsValid())
	{
		UE_CLOG(!bOptional, LogHairCardGenerator, Error, TEXT("The asset path cannot be found in the registry."));
		return nullptr;
	}


	if(ExistingAsset.GetClass() != UGroomAsset::StaticClass())
	{
		UE_CLOG(!bOptional, LogHairCardGenerator, Error, TEXT("The specified asset is not a groom asset."));
		return nullptr;
	}

	return LoadObject<UGroomAsset>(nullptr, *AssetPath);
}

static FHairGroupsCardsSourceDescription* HairCardGeneratorEditor_Impl::FindCardSourceDescription(TObjectPtr<UGroomAsset> Groom, int32 LODIndex)
{
	// Try to load previous settings from a matching (LOD) card source description slot, if compatible
	for ( FHairGroupsCardsSourceDescription& Desc : Groom->GetHairGroupsCards() )
	{
		if ( Desc.LODIndex == LODIndex)
		{
			return &Desc;
		}
	}

	return nullptr;
}

static TObjectPtr<UHairCardGeneratorPluginSettings> HairCardGeneratorEditor_Impl::CreateGroomSettingsLast(TObjectPtr<UGroomAsset> Groom, const FHairGroupsCardsSourceDescription& SourceDesc)
{
	TObjectPtr<UHairCardGeneratorPluginSettings> NewSettings = NewObject<UHairCardGeneratorPluginSettings>(/*Outer =*/Groom);
	NewSettings->SetSource(Groom, SourceDesc.LODIndex, SourceDesc.GroupIndex);
	NewSettings->ResetToDefault();

	// First try to reset from Json file (then fallback to previous settings if they exist)
	if ( NewSettings->ResetFromSettingsJson() )
		return NewSettings;

	NewSettings->ResetFromSourceDescription(SourceDesc);
	return NewSettings;
}

static void HairCardGeneratorEditor_Impl::DbgCmdCacheGroom(const TArray<FString>& Args)
{
	TObjectPtr<UGroomAsset> GroomAsset = HairCardGeneratorEditor_Impl::ParseGroomAssetArg(Args, false);
	if ( !GroomAsset )
		return;

	if ( !HairCardGeneratorEditor_Impl::LoadGroomData(GroomAsset, true) )
	{
		UE_LOG(LogHairCardGenerator, Error, TEXT("Failed to cache groom asset."));
	}
}

static bool HairCardGeneratorEditor_Impl::LoadGroomData(UGroomAsset* NewGroomAsset, bool SaveCached = false)
{
	if (HairCardGeneratorEditor_Impl::LastGroom == NewGroomAsset && !SaveCached)
	{
		return true;
	}
	else
	{
		TObjectPtr<UHairCardGenControllerBase> CardGenController = FHairCardGenController::GetHairCardGenerator();
		if ( !CardGenController )
			return false;

		FHairCardGen_GroomData RestructuredData;
		if ( !HairCardGeneratorEditor_Impl::BuildHairCardGenGroomData(CardGenController->GetPointsPerCurve(), NewGroomAsset->GetHairDescription(), RestructuredData))
			return false;

		FScopedSlowTask SlowTask(0, LOCTEXT("GeneratingHairCards.LoadGroom", "Loading Groom..."));
		SlowTask.MakeDialog(/*bShowCancelButton =*/true);

		FString CachedGroomsPath = FPaths::ProjectIntermediateDir() / TEXT("GroomHairCardGen") / TEXT("CachedGrooms");
		if (!CardGenController->LoadGroomData(RestructuredData, NewGroomAsset->GetName(), CachedGroomsPath, SaveCached))
		{
			return false;
		}
		else
		{
			HairCardGeneratorEditor_Impl::LastGroom = NewGroomAsset;
			return true;
		}
	}
}

static bool HairCardGeneratorEditor_Impl::BuildHairCardGenGroomData(const int PointsPerStrand, const FHairDescription& InGroomData, FHairCardGen_GroomData& Out)
{
	const int NumStrands = InGroomData.GetNumStrands();
	Out.Strands.Reserve(NumStrands);

	TGroomAttributesConstRef<float> GroomHairWidth = InGroomData.GroomAttributes().GetAttributesRef<float>(HairAttribute::Groom::Width);
	const FGroomID GroomID = FGroomID(0);
	const float Groom_DefaultWidth = GroomHairWidth.IsValid() ? GroomHairWidth[GroomID] : 0.f;

	FStrandAttributesRange TraverseStrands(InGroomData, FHairStrandAttribsConstIterator::DefaultIgnore);
	if ( !TraverseStrands.IsValid() )
		return false;

	UE_CLOG(!TraverseStrands.First().HasGroupID(), LogHairCardGenerator, Warning, TEXT("Groom description has no physics group info, may not generate correctly"));

	const FHairStrandAttribsRefProxy& FirstStrand = TraverseStrands.First();
	UE_CLOG((!FirstStrand.HasCurveType() || !FirstStrand.HasBasisType() || FirstStrand.HasKnots()), LogHairCardGenerator, Warning, TEXT("Invalid strand curve specification, treating as linear"));

	EGroomCurveType CurveType = FirstStrand.GetCurveType();
	EGroomBasisType BasisType = FirstStrand.GetBasisType();

	if (CurveType == EGroomCurveType::VariableOrder)
	{
		UE_LOG(LogHairCardGenerator, Warning, TEXT("VariableOrder curves not supported, treating as linear"));

		CurveType = EGroomCurveType::Linear;
		BasisType = EGroomBasisType::NoBasis;
	}

	Out.CurveType = StaticEnum<EGroomCurveType>()->GetNameStringByValue((int64)CurveType);
	Out.BasisType = StaticEnum<EGroomBasisType>()->GetNameStringByValue((int64)BasisType);

	Out.VertexPositions.Reserve(NumStrands * PointsPerStrand * 3);
	Out.VertexWidths.Reserve(NumStrands * PointsPerStrand);

	for ( const FHairStrandAttribsRefProxy& StrandAttribs : TraverseStrands )
	{
		FHairCardGen_StrandData& OutStrandData = Out.Strands.AddDefaulted_GetRef();
		OutStrandData.GroupID = StrandAttribs.HasGroupID() ? StrandAttribs.GetGroupID() : -1;

		const float Strand_DefaultWidth = StrandAttribs.HasStrandWidth() ? StrandAttribs.GetStrandWidth() : Groom_DefaultWidth;

		TArray<FVector> VertexPositions;
		VertexPositions.SetNum(StrandAttribs.NumVerts());
		TArray<float> VertexWidths;
		VertexWidths.SetNum(StrandAttribs.NumVerts());

		for (int StrandVertIndex = 0; StrandVertIndex < StrandAttribs.NumVerts(); ++StrandVertIndex)
		{
			VertexPositions[StrandVertIndex] = FVector(StrandAttribs.GetVertexPosition(StrandVertIndex));
			VertexWidths[StrandVertIndex] = StrandAttribs.HasVertexWidths() ? StrandAttribs.GetVertexWidth(StrandVertIndex) : Strand_DefaultWidth;
		}

		FHairCardGenStrandNSCInterpolator::StrandInterpolationResult InterpolatedStrand = FHairCardGenStrandNSCInterpolator(VertexPositions, VertexWidths).GetInterpolatedStrand(PointsPerStrand);

		Out.VertexPositions.Append(InterpolatedStrand.Positions);
		Out.VertexWidths.Append(InterpolatedStrand.Widths);
	}

	Out.Strands.Shrink();
	Out.VertexPositions.Shrink();
	Out.VertexWidths.Shrink();

	Out.HairlineGroupID = INDEX_NONE;

	return true;
}

static bool HairCardGeneratorEditor_Impl::GenerateCardsGeometry(TObjectPtr<const UHairCardGeneratorPluginSettings> Settings, int index)
{
	TObjectPtr<UHairCardGenControllerBase> CardGenController = FHairCardGenController::GetHairCardGenerator();
	if ( !CardGenController )
	{
		return false;
	}

	FScopedSlowTask SlowTask(10, FText::Format(LOCTEXT("GeneratingHairCards.GeometryOptimization", "Generating cards geometry"), index));

	SlowTask.EnterProgressFrame(1);
	TArray<int> NumCards = CardGenController->SetOptimizations(index);

	if (NumCards.Num() == 0)
	{
		UE_LOG(LogHairCardGenerator, Error, TEXT("Card geometry generation failed for settings group %d. Check the log for details."), index);
		SlowTask.EnterProgressFrame(9);
		return false;
	}

	int TotalNumCards = 0;
	for (int CurrentNumCards : NumCards) TotalNumCards += CurrentNumCards;

	float AvgLength = 0;
	float AvgCurvRadius = 0;

	TArray<MatrixXf> AvgCurves;
	AvgCurves.SetNum(TotalNumCards);

	int CardId = 0;
	for (int Id = 0; Id < NumCards.Num(); Id++)
	{
		for (int Cid = 0; Cid < NumCards[Id]; Cid++)
		{
			const TArray<float> AvgCurvePoints = CardGenController->GetAverageCurve(Id, Cid);

			if (AvgCurvePoints.Num() == 0)
			{
				UE_LOG(LogHairCardGenerator, Error, TEXT("Card geometry generation failed for settings group %d. Check the log for details."), index);
				SlowTask.EnterProgressFrame(9);
				return false;
			}

			AvgCurves[CardId] = FHairCardGenCardSubdivider::TArrayToMatrixXf(AvgCurvePoints);
			AvgLength += FHairCardGenCardSubdivider::GetCurveLength(AvgCurves[CardId]);
			AvgCurvRadius += FHairCardGenCardSubdivider::GetAverageCurvatureRadius(AvgCurves[CardId]);

			CardId++;
		}
	}

	AvgLength /= (float)TotalNumCards;
	AvgCurvRadius /= (float)TotalNumCards;

	auto GetSubdToleranceFromTriCount = [AvgLength, AvgCurvRadius, TotalNumCards](int TriCount)
	{
		const float NumSubdivisions = 0.5 * (float)TriCount / (float)TotalNumCards;
		return FHairCardGenCardSubdivider::GetToleranceFromLengthAndCurvRadius(AvgLength, AvgCurvRadius, NumSubdivisions);
	};

	int const InitialTargetTriCount = Settings->GetFilterGroupSettings(index)->TargetTriangleCount;

	FHairCardGenCardSubdivider Subdivider(
		GetSubdToleranceFromTriCount(InitialTargetTriCount),
		Settings->GetFilterGroupSettings(index)->UseAdaptiveSubdivision,
		Settings->GetFilterGroupSettings(index)->MaxVerticalSegmentsPerCard);

	if (InitialTargetTriCount <= 2 * TotalNumCards)
	{
		Subdivider.SetSubdTolerance(-1.);
	}
	else
	{
		auto CountTriangles = [&Subdivider, &AvgCurves]() 
		{
			int TriCount = 0;
			for (int CardId = 0; CardId < AvgCurves.Num(); CardId++) TriCount += 2 * (Subdivider.GetSubdivisionPoints(AvgCurves[CardId]).Num() / 3 - 1);
			return TriCount;
		};

		const int InitialTriCount = CountTriangles();

		int ShiftedTargetTriCount = 2 * InitialTargetTriCount - InitialTriCount;
		int const MinTargetTriCount = 2.2 * float(TotalNumCards * InitialTargetTriCount) / float(InitialTriCount);
		if (ShiftedTargetTriCount < MinTargetTriCount) ShiftedTargetTriCount = MinTargetTriCount;

		Subdivider.SetSubdTolerance(GetSubdToleranceFromTriCount(ShiftedTargetTriCount));
		const int ShiftedTriCount = CountTriangles();

		if (InitialTriCount != ShiftedTriCount)
		{
			const int CorrectedTargetTriCount = InitialTargetTriCount + int(float(ShiftedTargetTriCount - InitialTargetTriCount) / float(ShiftedTriCount - InitialTriCount) * float(InitialTargetTriCount - InitialTriCount));
			Subdivider.SetSubdTolerance(GetSubdToleranceFromTriCount(CorrectedTargetTriCount));
		}
	}

	CardId = 0;
	for (int Id = 0; Id < NumCards.Num(); Id++)
	{
		for (int Cid = 0; Cid < NumCards[Id]; Cid++)
		{
			TArray<float> SubdividedPoints = Subdivider.GetSubdivisionPoints(AvgCurves[CardId]);
			CardGenController->SetInterpolatedAvgCurve(Id, Cid, SubdividedPoints);

			CardId++;
		}
	}

	SlowTask.EnterProgressFrame(9);
	if (!CardGenController->GenerateCardsGeometry())
	{
		UE_LOG(LogHairCardGenerator, Error, TEXT("Card geometry generation failed for settings group %d. Check the log for details."), index);
		return false;
	}

	return true;
}

static bool HairCardGeneratorEditor_Impl::GenerateCardsForCardGroup(TObjectPtr<const UHairCardGeneratorPluginSettings> Settings, int index, uint8 GenFlags)
{
	TObjectPtr<UHairCardGenControllerBase> CardGenController = FHairCardGenController::GetHairCardGenerator();
	if ( !CardGenController )
	{
		return false;
	}

	FScopedSlowTask SlowTask(10, FText::Format(LOCTEXT("GeneratingHairCards.GenerateGroup", "Generating Hair Cards for settings group {0}..."), index));
	SlowTask.MakeDialog(/*bShowCancelButton =*/true);

	SlowTask.EnterProgressFrame(1);
	if ( UHairCardGeneratorPluginSettings::CheckGenerationFlags(GenFlags, EHairCardGenerationPipeline::StrandClustering) && !Settings->bReduceCardsFromPreviousLOD )
	{
		if ( !CardGenController->GenerateClumps(index) )
		{
			UE_LOG(LogHairCardGenerator, Error, TEXT("Strand clustering failed for settings group %d. Check the log for details."), index);
			SlowTask.EnterProgressFrame(9);
			return false;
		}
	}

	SlowTask.EnterProgressFrame(5);
	if ( UHairCardGeneratorPluginSettings::CheckGenerationFlags(GenFlags, EHairCardGenerationPipeline::GeometryGeneration) )
	{
		if ( !HairCardGeneratorEditor_Impl::GenerateCardsGeometry(Settings, index) )
		{
			SlowTask.EnterProgressFrame(4);
			return false;
		}
	}

	SlowTask.EnterProgressFrame(4);
	if ( UHairCardGeneratorPluginSettings::CheckGenerationFlags(GenFlags, EHairCardGenerationPipeline::TextureClustering) && !Settings->bReduceCardsFromPreviousLOD )
	{
		if (!CardGenController->ClusterTextures(index))
		{
			UE_LOG(LogHairCardGenerator, Error, TEXT("Texture clustering failed for settings group %d. Check the log for details."), index)
			return false;
		}
	}

	return true;
}

static void HairCardGeneratorEditor_Impl::GetTextureImportNames(UHairCardGeneratorPluginSettings* Settings, FHairCardImportSettings& ImportSettings)
{
	uint32 NumTextures = GetHairTextureLayoutTextureCount(Settings->ChannelLayout);
	FString TextureBasename = Settings->GetTextureImportBaseName();

	ImportSettings.TextureNames.SetNum(NumTextures);
	ImportSettings.MeshName = Settings->BaseFilename;

	for (uint32 i = 0; i < NumTextures; ++i)
	{
		ImportSettings.TextureNames[i] = FString::Printf(TEXT("%s_TS%d"), *TextureBasename, i);
	}
}

static TArray<UObject*> HairCardGeneratorEditor_Impl::ImportHairCardTextures(const FString& ImportSrc, const FString& ImportDst, const FHairCardImportSettings& ImportSettings)
{
	// Create a texture factory for importing tangents/attributes with linear colorspace
	UTextureFactory* LinearTextureFactory = NewObject<UTextureFactory>();
	LinearTextureFactory->ColorSpaceMode = ETextureSourceColorSpace::Linear;

	UAutomatedAssetImportData* AutoImportLinearTextures = NewObject<UAutomatedAssetImportData>();
	AutoImportLinearTextures->DestinationPath = ImportDst;
	AutoImportLinearTextures->bReplaceExisting = true;
	AutoImportLinearTextures->bSkipReadOnly = false;
	AutoImportLinearTextures->Factory = LinearTextureFactory;

	IPlatformFile& FileManager = FPlatformFileManager::Get().GetPlatformFile();
	FileManager.IterateDirectory(*ImportSrc, [AutoImportLinearTextures, ImportSettings](const TCHAR* FilenameOrDirectory, const bool bIsDirectory)->bool
		{
			if (!bIsDirectory)
			{
				AutoImportLinearTextures->Filenames.Add(FString(FilenameOrDirectory));
			}
			return true;
		}
	);

	TArray<UObject*> ImportedAssets = FAssetToolsModule::GetModule().Get().ImportAssetsAutomated(AutoImportLinearTextures);
	return ImportedAssets;
}

static UStaticMesh* HairCardGeneratorEditor_Impl::CreateMeshInPackage(const FString& DestinationPackage, const FString& BaseFilename, int GroupID)
{
	FString MeshName = BaseFilename;
	if ( GroupID >= 0 )
	{
		MeshName += FString::Printf(TEXT("_PG%d"), GroupID);
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

	FString PackageDest = ObjectTools::SanitizeInvalidChars(FPaths::Combine(*DestinationPackage, *MeshName), INVALID_LONGPACKAGE_CHARACTERS);
	FString ObjectName = ObjectTools::SanitizeObjectName(MeshName);

	UPackage* PackageOuter = CreatePackage(*PackageDest);
	TObjectPtr<UObject> ExistingMesh = FindObject<UStaticMesh>(PackageOuter, *ObjectName);
	if ( !ExistingMesh )
	{
		UStaticMesh* NewMesh = NewObject<UStaticMesh>(PackageOuter, UStaticMesh::StaticClass(), *ObjectName, RF_Public | RF_Standalone);
		// Add asset path to registry to get folder reference in content browser
		AssetRegistryModule.Get().AssetCreated(NewMesh);

		return NewMesh;
	}

	if ( ExistingMesh->GetClass()->IsChildOf(UStaticMesh::StaticClass()) )
	{
		// Free any RHI resources for existing mesh before we re-create in place.
		ExistingMesh->PreEditChange(nullptr);
		UStaticMesh* NewMesh = NewObject<UStaticMesh>(PackageOuter, UStaticMesh::StaticClass(), *ObjectName, RF_Public | RF_Standalone);
		AssetRegistryModule.Get().AssetCreated(NewMesh);

		return NewMesh;
	}

	UE_LOG(LogHairCardGenerator, Error, TEXT("Non-mesh asset ('%s') already exists in package: '%s'"), *ObjectName, *PackageDest);

	return nullptr;
}

static int32 HairCardGeneratorEditor_Impl::AssignImportedTextures(const TArray<UObject*>& ImportedAssets, const FHairCardImportSettings& ImportSettings, FHairGroupsCardsSourceDescription& Assignee)
{
	int32 AssignedAssets = 0;
	for (UObject* ImportedObject : ImportedAssets)
	{
		TObjectPtr<UTexture2D> TextureImport = Cast<UTexture2D>(ImportedObject);
		if ( TextureImport )
		{
			bool bFound = false;
			int SlotIdx = ImportSettings.TextureNames.Find(TextureImport->GetName());
			if (SlotIdx != INDEX_NONE)
			{
				Assignee.Textures.SetTexture(SlotIdx, TextureImport);
				bFound = true;
				++AssignedAssets;
			}

			UE_CLOG(!bFound, LogHairCardGenerator, Warning, TEXT("Unable to determine which slot imported texture ('%s') is for. Check your config."), *TextureImport->GetName());
		}
		else
		{
			UE_LOG(LogHairCardGenerator, Warning, TEXT("Unused asset imported along with hair card mesh: '%s'"), *ImportedObject->GetName());
		}
	}

	return AssignedAssets;
}

static void HairCardGeneratorEditor_Impl::CreateFinishNotification(bool bSuccess, const FText& NotificationText)
{
	FNotificationInfo Info(NotificationText);

	Info.bFireAndForget = true;
	Info.bUseLargeFont = true;
	Info.bUseSuccessFailIcons = true;

	Info.FadeOutDuration = 4.0f;
	Info.ExpireDuration = Info.FadeOutDuration;

	SNotificationItem::ECompletionState State = (bSuccess) ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail;

	TSharedPtr<SNotificationItem> NotificationPtr = FSlateNotificationManager::Get().AddNotification(Info);
	NotificationPtr->SetCompletionState(State);
}

/* FHairCardGeneratorEditorModule
 *****************************************************************************/

class FHairCardGeneratorEditorModule : public IHairCardGeneratorEditor
{
public:
	//~ Begin IModuleInterface API
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface API

	//~ Begin IHairCardGenerator API
	virtual bool GenerateHairCardsForLOD(UGroomAsset* Groom, FHairGroupsCardsSourceDescription& CardsDesc) override;
	virtual bool IsCompatibleSettings(UHairCardGenerationSettings* OldSettings) override;
	//~ End IHairCardGenerator API
};

void FHairCardGeneratorEditorModule::StartupModule()
{
	LLM_SCOPE_BYTAG(HairCardGenerator)

	HairCardGenerator_Utils::RegisterModularHairCardGenerator(this);

	// Register custom detail panel handler for hair card settings
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomClassLayout(UHairCardGeneratorPluginSettings::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FHairCardSettingsDetailCustomization::MakeInstance));
}

void FHairCardGeneratorEditorModule::ShutdownModule()
{
	HairCardGenerator_Utils::UnregisterModularHairCardGenerator(this);

	// Register custom detail panel handler for hair card settings
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.UnregisterCustomClassLayout(UHairCardGeneratorPluginSettings::StaticClass()->GetFName());
}

bool FHairCardGeneratorEditorModule::GenerateHairCardsForLOD(UGroomAsset* NewGroomAsset, FHairGroupsCardsSourceDescription& CardsDesc)
{
	// Find the HairCardGenController in Python (or wherever it may be implemented -- could be moved to C++/Blueprints/etc.)
	TObjectPtr<UHairCardGenControllerBase> CardGenController = FHairCardGenController::GetHairCardGenerator();
	if ( !CardGenController )
	{
		UE_LOG(LogHairCardGenerator, Error, TEXT("No hair card generator controller found, cannot generate hair cards!"));
		return false;
	}

	UE_CLOG(CardsDesc.GenerationSettings && !IsCompatibleSettings(CardsDesc.GenerationSettings), LogHairCardGenerator, Warning, TEXT("Old hair-card generation settings (from a different generator) will be discarded when you run this generator."));

	TObjectPtr<UHairCardGeneratorPluginSettings>& GenerationSettings = CardGenController->GetGroomSettings(NewGroomAsset, CardsDesc.LODIndex);
	if ( !GenerationSettings )
	{
		GenerationSettings = HairCardGeneratorEditor_Impl::CreateGroomSettingsLast(NewGroomAsset, CardsDesc);
	}
	else
	{
		// Need to update all non-generation UI settings in case they've changed on groom asset
		GenerationSettings->SetGenerateForGroomGroup(CardsDesc.GroupIndex);
		GenerationSettings->PostResetUpdates();
	}

	bool bSuccess = false;
	if ( !HairCardGenWindow_Utils::PromptUserWithHairCardGenDialog(GenerationSettings) )
	{
		return false;
	}

	// Use num groups and out group id to determine mesh name
	int NumPhysGroups = NewGroomAsset->GetHairDescriptionGroups().HairGroups.Num();

	// Check primary settings object merged and all group settings for differences
	uint8 AllGenFlags = GenerationSettings->GetAllPipelineGeneratedDifferences();
	if ( !GenerationSettings->CheckGenerationFlags(AllGenFlags, EHairCardGenerationPipeline::All) && CardsDesc.ImportedMesh != nullptr )
	{
		UE_LOG(LogHairCardGenerator, Display, TEXT("No settings changes, skipping hair card run"));
		HairCardGeneratorEditor_Impl::CreateFinishNotification(true, LOCTEXT("HairCardGen.UpToDate", "Hair card assets are up to date"));
		// FIXME: Currently returning true clears textures unless a reimport is run
		return false;
	}

	// Force reimport if we are going to run any pipeline step
	AllGenFlags |= (uint8)EHairCardGenerationPipeline::ImportUpdate;

	// Check primary settings object (without group settings) used to force reruns for e.g. random seed changes
	uint8 PipelineGenFlags = GenerationSettings->GetPipelineGeneratedDifferences();

	// Write the settings that we will attempt to output to card desc (won't be stored on failure see FGroomRenderingDetails::OnGenerateCardDataUsingPlugin for details)
	CardsDesc.GenerationSettings = GenerationSettings;

	// Need to set the CardsDesc texture layout appropriately in case we succeed
	CardsDesc.Textures.SetLayout(GenerationSettings->ChannelLayout);

	// TODO: On error will force full regen (is this the best we can do?)
	GenerationSettings->ClearPipelineGenerated();

	// Output generation settings to json file in Metadata folder
	GenerationSettings->WriteGenerationSettings();

	if (!HairCardGeneratorEditor_Impl::LoadGroomData(NewGroomAsset))
	{
		UE_LOG(LogHairCardGenerator, Error, TEXT("Failed to load groom asset. See log for details."));
		HairCardGeneratorEditor_Impl::CreateFinishNotification(false, LOCTEXT("HairCardGen.FailedGroomLoad", "Failed to load groom asset. See log for details."));
		return false;
	}

	GenerationSettings->UpdateStrandFilterAssignment();
	if ( !CardGenController->LoadSettings(GenerationSettings) )
	{
		UE_LOG(LogHairCardGenerator, Error, TEXT("Failed to load hair card generation settings. See log for details."));
		HairCardGeneratorEditor_Impl::CreateFinishNotification(false, LOCTEXT("HairCardGen.FailedSettingsLoad", "Failed to load hair card generation settings. See log for details."));
		return false;
	}

	// Run card group generation in loop
	bool bAllGenerated = true;
	for ( int i=0; i < GenerationSettings->GetFilterGroupSettings().Num(); ++i )
	{
		uint8 GroupGenFlags = PipelineGenFlags | GenerationSettings->GetPipelineFilterGroupDifferences(i);

		// Skip if no group update flags required for current group
		if ( !GenerationSettings->CheckGenerationFlags(GroupGenFlags, EHairCardGenerationPipeline::GroupUpdate) )
		{
			UE_LOG(LogHairCardGenerator, Display, TEXT("Setttings group %d already generated. Skipping."), i);
			continue;
		}

		// TODO: On errors this will force full regeneration (because we clear the group gen object)
		GenerationSettings->ClearPipelineGeneratedFilterGroup(i);
		bAllGenerated &= HairCardGeneratorEditor_Impl::GenerateCardsForCardGroup(GenerationSettings, i, GroupGenFlags);

		// TODO: It might be useful to handle user cancellation by breaking vs. errors by continuing here
		if ( !bAllGenerated )
		{
			break;
		}

		GenerationSettings->WritePipelineGeneratedFilterGroup(i);
	}

	if ( !bAllGenerated )
	{
		UE_LOG(LogHairCardGenerator, Error, TEXT("Failed to generate cards for all groups. See log for details."));
		HairCardGeneratorEditor_Impl::CreateFinishNotification(false, LOCTEXT("HairCardGen.FailedGeneration", "Failed to generate cards for all groups. See log for details."));
		return false;
	}

	if ( GenerationSettings->CheckGenerationFlags(AllGenFlags, EHairCardGenerationPipeline::TextureLayout) && !GenerationSettings->bReduceCardsFromPreviousLOD )
	{
		FScopedSlowTask SlowTask(0, LOCTEXT("HairCardGen.BuildingLayout", "Building texture atlas layout..."));
		SlowTask.MakeDialog(/*bShowCancelButton =*/true);
		if ( !CardGenController->GenerateTextureLayout() )
		{
			UE_LOG(LogHairCardGenerator, Error, TEXT("Texture layout failed. See log for details."));
			HairCardGeneratorEditor_Impl::CreateFinishNotification(false, LOCTEXT("HairCardGen.FailedLayout", "Texture layout failed. See log for details."));
			return false;
		}
	}

	if ( GenerationSettings->CheckGenerationFlags(AllGenFlags, EHairCardGenerationPipeline::TextureRendering) && !GenerationSettings->bReduceCardsFromPreviousLOD )
	{
		FScopedSlowTask SlowTask(0, LOCTEXT("HairCardGen.BuildingAtlas", "Exporting textures..."));
		SlowTask.MakeDialog(/*bShowCancelButton =*/true);
		if ( !CardGenController->GenerateTextureAtlases() )
		{
			UE_LOG(LogHairCardGenerator, Error, TEXT("Texture atlas creation failed. See log for details."));
			HairCardGeneratorEditor_Impl::CreateFinishNotification(false, LOCTEXT("HairCardGen.FailedAtlas", "Texture atlas creation failed. See log for details."));
			return false;
		}
	}

	// Import Textures (NOTE: Mesh import no longer supported)
	if ( GenerationSettings->CheckGenerationFlags(AllGenFlags, EHairCardGenerationPipeline::ImportTextures) )
	{
		FHairCardImportSettings ImportSettings;
		HairCardGeneratorEditor_Impl::GetTextureImportNames(GenerationSettings, ImportSettings);

		const FString TexturePath = GenerationSettings->GetTextureImportPath();
		const FString TextureDest = GenerationSettings->GetTextureContentPath();
		UE_LOG(LogHairCardGenerator, Display, TEXT("Importing generated cards from: %s"), *TexturePath);
		TArray<UObject*> ImportedAssets = HairCardGeneratorEditor_Impl::ImportHairCardTextures(TexturePath, TextureDest, ImportSettings);
		if ( ImportedAssets.IsEmpty() )
		{
			UE_LOG(LogHairCardGenerator, Error, TEXT("Failed to import textures. Check '%s' for the files."), *TexturePath);
			HairCardGeneratorEditor_Impl::CreateFinishNotification(false, LOCTEXT("HairCardGen.FailedTextureImport", "Failed to import textures. See log for details."));
			return false;
		}

		const int32 NumAssigned = HairCardGeneratorEditor_Impl::AssignImportedTextures(ImportedAssets, ImportSettings, CardsDesc);
		if ( NumAssigned == 0 )
		{
			UE_LOG(LogHairCardGenerator, Error, TEXT("Unable to assign any of the imported assets to the groom. Verify texture file name format."));
			HairCardGeneratorEditor_Impl::CreateFinishNotification(false, LOCTEXT("HairCardGen.FailedAssignImports", "Unable to assign any of the imported assets to the groom. See log for details."));
			return false;
		}
	}

	// Generate static mesh from cards and add to destination package
	if ( GenerationSettings->CheckGenerationFlags(AllGenFlags, EHairCardGenerationPipeline::GenerateMesh) )
	{
		FScopedSlowTask SlowTask(0, LOCTEXT("GeneratingHairCards.BuildMesh", "Building mesh..."));
		SlowTask.MakeDialog(/*bShowCancelButton =*/true);

		// Don't bother adding _PG# at end of mesh name if there's only one physics group
		int OutGroupId = (NumPhysGroups > 1) ? GenerationSettings->GetGenerateForGroomGroup() : -1;
		TObjectPtr<UStaticMesh> NewMesh = HairCardGeneratorEditor_Impl::CreateMeshInPackage(GenerationSettings->DestinationPath.Path, GenerationSettings->BaseFilename, OutGroupId);
		if ( !NewMesh )
		{
			
			UE_LOG(LogHairCardGenerator, Error, TEXT("Unable to create mesh in destination package."));
			HairCardGeneratorEditor_Impl::CreateFinishNotification(false, LOCTEXT("HairCardGen.FailedObjectCreate", "Unable to create mesh in destination package. See log for details."));
			return false;
		}

		if ( !CardGenController->GenerateMesh(NewMesh) )
		{
			UE_LOG(LogHairCardGenerator, Error, TEXT("Mesh generation failed. See log for details."));
			HairCardGeneratorEditor_Impl::CreateFinishNotification(false, LOCTEXT("HairCardGen.FailedMeshGen", "Mesh generation failed. See log for details."));
			return false;
		}

		CardsDesc.ImportedMesh = NewMesh;
		CardsDesc.UpdateMeshKey();

		NewMesh->PostEditChange();
		NewMesh->GetPackage()->MarkPackageDirty();
	}

	GenerationSettings->WritePipelineGenerated();
	HairCardGeneratorEditor_Impl::CreateFinishNotification(true, LOCTEXT("HairCardGen.GenSuccess", "Hair cards generated successfully."));
	return true;
}

bool FHairCardGeneratorEditorModule::IsCompatibleSettings(UHairCardGenerationSettings* OldSettings)
{
	return UHairCardGeneratorPluginSettings::IsCompatibleSettings(OldSettings);
}

IMPLEMENT_MODULE(/*FDefaultModuleImpl*/FHairCardGeneratorEditorModule, HairCardGeneratorEditor);
#undef LOCTEXT_NAMESPACE
