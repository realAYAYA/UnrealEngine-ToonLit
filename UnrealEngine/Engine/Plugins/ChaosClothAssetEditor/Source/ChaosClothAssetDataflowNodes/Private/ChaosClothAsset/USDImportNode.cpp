// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/USDImportNode.h"
#include "ChaosClothAsset/ClothAsset.h"
#include "ChaosClothAsset/ClothDataflowTools.h"
#include "ChaosClothAsset/CollectionClothFacade.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dataflow/DataflowInputOutput.h"
#include "Dataflow/DataflowGraph.h"
#include "Dataflow/DataflowObject.h"
#include "Engine/StaticMesh.h"
#include "HAL/FileManager.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "StaticMeshAttributes.h"
#include "UObject/Package.h"
#include "UnrealUSDWrapper.h"
#include "USDConversionUtils.h"
#include "USDProjectSettings.h"
#include "USDStageImportContext.h"
#include "USDStageImporter.h"
#include "USDStageImportOptions.h"
#include "USDTypesConversion.h"
#include "USDValueConversion.h"
#include "Chaos/CollectionPropertyFacade.h"
#include "UsdWrappers/UsdPrim.h"
#include "UsdWrappers/UsdRelationship.h"
#include "UsdWrappers/VtValue.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(USDImportNode)

#define LOCTEXT_NAMESPACE "ChaosClothAssetUSDImportNode"

namespace UE::Chaos::ClothAsset::Private
{
	static bool RemoveDegenerateTriangles(
		const TArray<FIntVector3>& TriangleToVertexIndex,
		const TArray<FVector2f>& RestPositions2D,
		const TArray<FVector3f>& DrapedPositions3D,
		TArray<FIntVector3>& OutTriangleToVertexIndex,
		TArray<FVector2f>& OutRestPositions2D,
		TArray<FVector3f>& OutDrapedPositions3D,
		TArray<int32>& OutIndices)  // Old to new vertices lookup
	{
		bool bHasDegenerateTriangles = false;

		check(RestPositions2D.Num() == DrapedPositions3D.Num());
		const int32 VertexCount = RestPositions2D.Num();
		const int32 TriangleCount = TriangleToVertexIndex.Num();

		OutTriangleToVertexIndex.Reset(TriangleCount);

		// Remap[Index] is the index of the first vertex in a group of degenerated triangles to be callapsed.
		// When two groups of collapsed vertices are merged, the group with the greatest Remap[index] value must adopt the one from the other group.
		// For Example:
		// 1. For all i, Remap[i] = i
		// 2. Finds one degenerated triangle (7, 9, 4) with collapsed edges (7, 9), (9, 4), and (7, 4) -> Remap[4] = 4, Remap[7] = 4, and Remap[9] = 4
		// 3. Finds another degenerated triangle (2, 3, 4) with collapsed edges (2, 4) -> Remap[2] = 2, Remap[4] = 2, Remap[7] = 2, and Remap[9] = 2
		TArray<int32> Remap;
		Remap.SetNumUninitialized(VertexCount);

		for (int32 Index = 0; Index < VertexCount; ++Index)
		{
			Remap[Index] = Index;
		}

		int32 OutVertexCount = VertexCount;

		auto RemapAndPropagateIndex = [&Remap, &OutVertexCount](int32 Index0, int32 Index1)
			{
				if (Remap[Index0] != Remap[Index1])
				{
					if (Remap[Index0] > Remap[Index1])  // Always remap from the lowest index to ensure the earlier index is always kept
					{
						Swap(Index0, Index1);
					}
					// Merge groups with this new first index Remap[Index0]
					const int32 PrevRemapIndex = Remap[Index1];
					for (int32 Index = PrevRemapIndex; Index < Remap.Num(); ++Index)  // Only need to start from the first index of the group to merge
					{
						if (Remap[Index] == PrevRemapIndex)
						{
							Remap[Index] = Remap[Index0];
						}
					}
					--OutVertexCount;
				}
			};

		for (int32 TriangleIndex = 0; TriangleIndex < TriangleCount; ++TriangleIndex)
		{
			const int32 Index0 = TriangleToVertexIndex[TriangleIndex][0];
			const int32 Index1 = TriangleToVertexIndex[TriangleIndex][1];
			const int32 Index2 = TriangleToVertexIndex[TriangleIndex][2];

			const FVector3f& P0 = DrapedPositions3D[Index0];
			const FVector3f& P1 = DrapedPositions3D[Index1];
			const FVector3f& P2 = DrapedPositions3D[Index2];
			const FVector3f P0P1 = P1 - P0;
			const FVector3f P0P2 = P2 - P0;

			const float TriNormSizeSquared = (P0P1 ^ P0P2).SizeSquared();
			if (TriNormSizeSquared <= UE_SMALL_NUMBER)
			{
				const FVector3f P1P2 = P2 - P1;

				if (P0P1.SquaredLength() <= UE_SMALL_NUMBER)
				{
					RemapAndPropagateIndex(Index0, Index1);
				}
				if (P0P2.SquaredLength() <= UE_SMALL_NUMBER)
				{
					RemapAndPropagateIndex(Index0, Index2);
				}
				if (P1P2.SquaredLength() <= UE_SMALL_NUMBER)
				{
					RemapAndPropagateIndex(Index1, Index2);
				}
			}
			else
			{
				OutTriangleToVertexIndex.Emplace(TriangleToVertexIndex[TriangleIndex]);
			}
		}

		const int32 OutTriangleCount = OutTriangleToVertexIndex.Num();
		bHasDegenerateTriangles = (TriangleCount != OutTriangleCount);

		UE_CLOG(bHasDegenerateTriangles, LogChaosClothAssetDataflowNodes, Display,
			TEXT("USD import found and removed %d degenerated triangles out of %d source triangles."), TriangleCount - OutTriangleCount, TriangleCount);

		OutRestPositions2D.Reset(OutVertexCount);
		OutDrapedPositions3D.Reset(OutVertexCount);
		OutIndices.Reset(VertexCount);
		int32 OutIndex = -1;

		for (int32 VertexIndex = 0; VertexIndex < VertexCount; ++VertexIndex)
		{
			if (Remap[VertexIndex] == VertexIndex)
			{
				OutRestPositions2D.Add(RestPositions2D[Remap[VertexIndex]]);
				OutDrapedPositions3D.Add(DrapedPositions3D[Remap[VertexIndex]]);
				OutIndices.Add(++OutIndex);
			}
			else
			{
				const int32 OutRemappedIndex = OutIndices[Remap[VertexIndex]];
				OutIndices.Add(OutRemappedIndex);
			}
		}
		ensure(OutIndex + 1 == OutVertexCount);

		for (int32 TriangleIndex = 0; TriangleIndex < OutTriangleCount; ++TriangleIndex)
		{
			int32& Index0 = OutTriangleToVertexIndex[TriangleIndex][0];
			int32& Index1 = OutTriangleToVertexIndex[TriangleIndex][1];
			int32& Index2 = OutTriangleToVertexIndex[TriangleIndex][2];

			Index0 = OutIndices[Index0];
			Index1 = OutIndices[Index1];
			Index2 = OutIndices[Index2];

			checkSlow(Index0 != Index1);
			checkSlow(Index0 != Index2);
			checkSlow(Index1 != Index2);
			checkSlow((OutDrapedPositions3D[Index0] - OutDrapedPositions3D[Index1]).SquaredLength() > UE_SMALL_NUMBER);
			checkSlow((OutDrapedPositions3D[Index0] - OutDrapedPositions3D[Index2]).SquaredLength() > UE_SMALL_NUMBER);
			checkSlow((OutDrapedPositions3D[Index1] - OutDrapedPositions3D[Index2]).SquaredLength() > UE_SMALL_NUMBER);
		}

		return bHasDegenerateTriangles;
	}

	static bool RemoveDuplicateTriangles(TArray<FIntVector3>& TriangleToVertexIndex)
	{
		bool bHasDuplicatedTriangles = false;

		const int32 TriangleCount = TriangleToVertexIndex.Num();

		TSet<FIntVector3> Triangles;
		Triangles.Reserve(TriangleCount);

		TArray<FIntVector3> OutTriangleToVertexIndex;
		OutTriangleToVertexIndex.Reserve(TriangleCount);

		auto GetSortedIndices = [](const FIntVector3& TriangleIndices)->FIntVector3
			{
				const int32 Index0 = TriangleIndices[0];
				const int32 Index1 = TriangleIndices[1];
				const int32 Index2 = TriangleIndices[2];

				return (Index0 < Index1) ?
					(Index1 < Index2) ? FIntVector3(Index0, Index1, Index2) : (Index0 < Index2) ? FIntVector3(Index0, Index2, Index1) : FIntVector3(Index2, Index0, Index1) :
					(Index0 < Index2) ? FIntVector3(Index1, Index0, Index2) : (Index1 < Index2) ? FIntVector3(Index1, Index2, Index0) : FIntVector3(Index2, Index1, Index0);
			};

		for (int32 Index = 0; Index < TriangleCount; ++Index)
		{
			const FIntVector3& TriangleIndices = TriangleToVertexIndex[Index];
			const FIntVector3 TriangleSortedIndices = GetSortedIndices(TriangleIndices);

			bool bIsAlreadyInSet;
			Triangles.FindOrAdd(TriangleSortedIndices, &bIsAlreadyInSet);

			if (bIsAlreadyInSet)
			{
				bHasDuplicatedTriangles = true;
			}
			else
			{
				OutTriangleToVertexIndex.Emplace(TriangleIndices);
			}
		}

		UE_CLOG(bHasDuplicatedTriangles, LogChaosClothAssetDataflowNodes, Display,
			TEXT("USD import found and removed %d duplicated triangles out of %d source triangles."), TriangleCount - OutTriangleToVertexIndex.Num(), TriangleCount);

		TriangleToVertexIndex = MoveTemp(OutTriangleToVertexIndex);

		return bHasDuplicatedTriangles;
	}

	bool RemoveDuplicateStitches(TArray<TArray<FIntVector2>>& SeamStitches)
	{
		bool bHasDuplicateStitches = false;

		const int32 NumSeamStitches = SeamStitches.Num();

		// Calculate the total number of stitches
		int32 NumStitches = 0;
		for (const TArray<FIntVector2>& Stitches : SeamStitches)
		{
			NumStitches += Stitches.Num();
		}

		TSet<FIntVector2> StichSet;
		StichSet.Reserve(NumStitches);

		int32 OutNumStitches = 0;
		TArray<TArray<FIntVector2>> OutSeamStitches;
		OutSeamStitches.Reserve(NumSeamStitches);

		for (const TArray<FIntVector2>& Stitches : SeamStitches)
		{
			TArray<FIntVector2> OutStitches;
			OutStitches.Reserve(Stitches.Num());

			for (const FIntVector2& Stitch : Stitches)
			{
				const FIntVector2 SortedStitch = Stitch[0] < Stitch[1] ?
					FIntVector2(Stitch[0], Stitch[1]) :
					FIntVector2(Stitch[1], Stitch[0]);

				bool bIsAlreadyInSet;
				StichSet.FindOrAdd(SortedStitch, &bIsAlreadyInSet);

				if (bIsAlreadyInSet)
				{
					bHasDuplicateStitches = true;
				}
				else
				{
					OutStitches.Emplace(Stitch);
				}
			}

			if (OutStitches.Num())
			{
				OutSeamStitches.Emplace(OutStitches);
				OutNumStitches += OutStitches.Num();
			}
		}

		UE_CLOG(bHasDuplicateStitches, LogChaosClothAssetDataflowNodes, Display,
			TEXT("USD import found and removed %d duplicated stitches out of %d source stitches."), NumStitches - OutNumStitches, NumStitches);

		SeamStitches = MoveTemp(OutSeamStitches);

		return bHasDuplicateStitches;
	}

	static TArray<FSoftObjectPath> UsdClothOverrideMaterials {
		FSoftObjectPath(TEXT("/ChaosClothAsset/Materials/USDImportMaterial.USDImportMaterial")),
		FSoftObjectPath(TEXT("/ChaosClothAsset/Materials/USDImportTranslucentMaterial.USDImportTranslucentMaterial")),
		FSoftObjectPath(TEXT("/ChaosClothAsset/Materials/USDImportTwoSidedMaterial.USDImportTwoSidedMaterial")),
		FSoftObjectPath(TEXT("/ChaosClothAsset/Materials/USDImportTranslucentTwoSidedMaterial.USDImportTranslucentTwoSidedMaterial")),
	};
	
	static void OverrideUsdImportMaterials(const TArray<FSoftObjectPath>& Materials, TArray<FSoftObjectPath>* SavedValues = nullptr)
	{
		if (UUsdProjectSettings* UsdProjectSettings = GetMutableDefault<UUsdProjectSettings>())
		{
			// Check to see if we should save the existing values
			if (SavedValues)
			{
				SavedValues->Push(UsdProjectSettings->ReferencePreviewSurfaceMaterial);
				SavedValues->Push(UsdProjectSettings->ReferencePreviewSurfaceTranslucentMaterial);
				SavedValues->Push(UsdProjectSettings->ReferencePreviewSurfaceTwoSidedMaterial);
				SavedValues->Push(UsdProjectSettings->ReferencePreviewSurfaceTranslucentTwoSidedMaterial);
			}
			UsdProjectSettings->ReferencePreviewSurfaceMaterial = Materials[0];
			UsdProjectSettings->ReferencePreviewSurfaceTranslucentMaterial = Materials[1];
			UsdProjectSettings->ReferencePreviewSurfaceTwoSidedMaterial = Materials[2];
			UsdProjectSettings->ReferencePreviewSurfaceTranslucentTwoSidedMaterial = Materials[3];
		}
	}
}  // End namespace UE::Chaos::ClothAsset::Private

FChaosClothAssetUSDImportNode::FChaosClothAssetUSDImportNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowTerminalNode(InParam, InGuid)
{
	using namespace UE::Chaos::ClothAsset;

	// Initialize to a valid collection cache
	const TSharedRef<FManagedArrayCollection> ClothCollection = MakeShared<FManagedArrayCollection>(MoveTemp(CollectionCache));
	FCollectionClothFacade ClothFacade(ClothCollection);
	ClothFacade.DefineSchema();
	CollectionCache = MoveTemp(*ClothCollection);

	// Register connections
	RegisterOutputConnection(&Collection);
}

void FChaosClothAssetUSDImportNode::SetAssetValue(TObjectPtr<UObject> Asset, Dataflow::FContext& Context) const
{
	using namespace UE::Chaos::ClothAsset;

	if (UChaosClothAsset* const ClothAsset = Cast<UChaosClothAsset>(Asset.Get()))
	{
PRAGMA_DISABLE_DEPRECATION_WARNINGS  // TODO: Don't use public property, and have Getter/Setter API instead
		if (UDataflow* const DataflowAsset = ClothAsset->DataflowAsset)
PRAGMA_ENABLE_DEPRECATION_WARNINGS
		{
			const TSharedPtr<Dataflow::FGraph, ESPMode::ThreadSafe> Dataflow = DataflowAsset->GetDataflow();
			if (const TSharedPtr<FDataflowNode> BaseNode = Dataflow->FindBaseNode(this->GetGuid()))  // This is basically a safe const_cast
			{
				FChaosClothAssetUSDImportNode* const MutableThis = static_cast<FChaosClothAssetUSDImportNode*>(BaseNode.Get());
				check(MutableThis == this);

				const FString& FilePath = UsdFile.FilePath;
				const FMD5Hash InFileHash = FilePath.IsEmpty() ?
					FMD5Hash() :  // Reset to an empty import
					FPaths::FileExists(FilePath) ?
					FMD5Hash::HashFile(*FilePath) :  // Update cache
					FileHash;  // Keep the current cache

				if (FileHash != InFileHash || UsdFile.bForceReimport)
				{
					MutableThis->FileHash = InFileHash;
					MutableThis->UsdFile.bForceReimport = false;

					const FString AssetPath = Asset->GetPackage()->GetPathName();

					FText ErrorText;
					if (!MutableThis->ImportFromFile(FilePath, AssetPath, ErrorText))
					{
						FClothDataflowTools::LogAndToastWarning(*this,
							LOCTEXT("FailedToExportUsdFileHeadline", "Failed to import USD file from file."),
							FText::Format(LOCTEXT("FailedToExportUsdDetails", "Error while importing USD cloth from file '{0}':\n{1}"), FText::FromString(FilePath), ErrorText));
					}
				}
			}
		}
	}
}

void FChaosClothAssetUSDImportNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	using namespace UE::Chaos::ClothAsset;

	if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		const TSharedRef<FManagedArrayCollection> ClothCollection = MakeShared<FManagedArrayCollection>();

		// Import from cache
		FText ErrorText;
		if (!ImportFromCache(ClothCollection, ErrorText))
		{
			FClothDataflowTools::LogAndToastWarning(*this,
				LOCTEXT("FailedToExportUsdCacheHeadline", "Failed to import USD file from cache."),
				FText::Format(LOCTEXT("FailedToExportUsdCacheDetails", "Error while importing USD cloth from cache '{0}':\n{1}"), FText::FromString(UsdFile.FilePath), ErrorText));
		}

		SetValue(Context, MoveTemp(*ClothCollection), &Collection);
	}
}

void FChaosClothAssetUSDImportNode::Serialize(FArchive& Archive)
{
	using namespace UE::Chaos::ClothAsset;

	::Chaos::FChaosArchive ChaosArchive(Archive);
	CollectionCache.Serialize(ChaosArchive);
	if (Archive.IsLoading())
	{
		// Make sure to always have a valid cloth collection on reload, some new attributes could be missing from the cached collection
		const TSharedRef<FManagedArrayCollection> ClothCollection = MakeShared<FManagedArrayCollection>(MoveTemp(CollectionCache));
		FCollectionClothFacade ClothFacade(ClothCollection);
		if (!ClothFacade.IsValid())
		{
			ClothFacade.DefineSchema();
		}
		CollectionCache = MoveTemp(*ClothCollection);
	}

	Archive << FileHash;
}

bool FChaosClothAssetUSDImportNode::ImportFromFile(const FString& UsdFilePath, const FString& AssetPath, FText& OutErrorText)
{
#if USE_USD_SDK
	using namespace UE::Chaos::ClothAsset;

	// Reset cache
	CollectionCache.Reset();
	PackagePath = FString();

	// Steal the cache's collection and make it a valid cloth collection
	const TSharedRef<FManagedArrayCollection> ClothCollection = MakeShared<FManagedArrayCollection>(MoveTemp(CollectionCache));
	FCollectionClothFacade ClothFacade(ClothCollection);
	ClothFacade.DefineSchema();

	// Make sure to give the collection back to the cache when reaching the end of scope
	ON_SCOPE_EXIT { CollectionCache = MoveTemp(*ClothCollection); };

	// Empty file
	if (UsdFilePath.IsEmpty())
	{
		return true;
	}

	// Start slow task
	constexpr float NumSteps = 4.f;
	FScopedSlowTask SlowTask(NumSteps, LOCTEXT("ImportingUSDFile", "Importing USD file..."));
	SlowTask.MakeDialogDelayed(1.f);

	// Open stage
	constexpr bool bUseStageCache = false;  // Reload from disk, not from cache
	constexpr EUsdInitialLoadSet UsdInitialLoadSet = EUsdInitialLoadSet::LoadAll;  // TODO: Ideally we should only use LoadNone to start with and load what's needed once the Schema is defined

	UE::FUsdStage UsdStage = UnrealUSDWrapper::OpenStage(*UsdFilePath, UsdInitialLoadSet, bUseStageCache);
	if (!UsdStage)
	{
		OutErrorText = LOCTEXT("CantCreateNewStage", "Failed to open the specified USD file.");
		return false;
	}

	// Look for the Mesh prim and set its kind to enable KindsToCollapse
	const UE::FSdfPath MeshPath(UE::FSdfPath(UE::FSdfPath::AbsoluteRootPath()).AppendChild(TEXT("Mesh")));
	if (UE::FUsdPrim MeshPrim = UsdStage.GetPrimAtPath(MeshPath))
	{
		MeshPrim.SetTypeName(TEXT("Xform"));  // TODO: Ideally this two operations need to be done in the exporter
		UsdUtils::SetDefaultKind(MeshPrim, EUsdDefaultKind::Component);

		// Look for the SkelRoot prim and disable it to allow the KindsToCollapse to work
		TArray<UE::FUsdPrim> MeshPrimChildren = MeshPrim.GetChildren();
		for (UE::FUsdPrim& MeshPrimChild : MeshPrimChildren)
		{
			if (MeshPrimChild.GetTypeName() == TEXT("SkelRoot"))
			{
				MeshPrimChild.SetActive(false);
			}
		}
	}

	SlowTask.EnterProgressFrame(1.f);

	// Update import location
	const uint32 UsdPathHash = GetTypeHash(UsdFile.FilePath);
	const FString UsdFileName = SlugStringForValidName(FPaths::GetBaseFilename(UsdFile.FilePath));
	const FString PackageName = FString::Printf(TEXT("%s_%08X"), *UsdFileName, UsdPathHash);
	PackagePath = FPaths::Combine(AssetPath + TEXT("_Import"), PackageName);

	// Get stage infos
	const FUsdStageInfo StageInfo(UsdStage);

	// Import recognised assets
	FUsdStageImportContext ImportContext;

	const TObjectPtr<UUsdStageImportOptions>& ImportOptions = ImportContext.ImportOptions;
	{
		check(ImportOptions);
		// Data to import
		ImportOptions->bImportActors = false;
		ImportOptions->bImportGeometry = true;
		ImportOptions->bImportSkeletalAnimations = false;
		ImportOptions->bImportLevelSequences = false;
		ImportOptions->bImportMaterials = true;
		ImportOptions->bImportGroomAssets = false;
		ImportOptions->bImportOnlyUsedMaterials = true;
		// Prims to import
		ImportOptions->PrimsToImport = TArray<FString>{ TEXT( "/" ) };
		// USD options
		ImportOptions->PurposesToImport = (int32)EUsdPurpose::Proxy;
		ImportOptions->NaniteTriangleThreshold = TNumericLimits<int32>::Max();  // Don't enable Nanite
		ImportOptions->RenderContextToImport = NAME_None;
		ImportOptions->MaterialPurpose = NAME_None;  // *UnrealIdentifiers::MaterialPreviewPurpose ???
		ImportOptions->RootMotionHandling = EUsdRootMotionHandling::NoAdditionalRootMotion;
		ImportOptions->SubdivisionLevel = 0;
		ImportOptions->bOverrideStageOptions = false;
		ImportOptions->bImportAtSpecificTimeCode = false;
		ImportOptions->ImportTimeCode = 0.f;
		// Groom
		ImportOptions->GroomInterpolationSettings = TArray<FHairGroupsInterpolation>();
		// Collision
		ImportOptions->ExistingActorPolicy = EReplaceActorPolicy::Replace;
		ImportOptions->ExistingAssetPolicy = EReplaceAssetPolicy::Replace;
		// Processing
		ImportOptions->bPrimPathFolderStructure = false;
		ImportOptions->KindsToCollapse = (int32)EUsdDefaultKind::Component;
		ImportOptions->bMergeIdenticalMaterialSlots = true;
		ImportOptions->bInterpretLODs = false;
	}

	constexpr bool bIsAutomated = true;
	constexpr bool bIsReimport = false;
	constexpr bool bAllowActorImport = false;

	ImportContext.Stage = UsdStage;  // Set the stage first to prevent re-opening it in the Init function
	ImportContext.Init(TEXT(""), UsdFilePath, PackagePath, RF_NoFlags, bIsAutomated, bIsReimport, bAllowActorImport);

	TArray<FSoftObjectPath> OriginalUsdMaterials;
	// Override the project settings to point the USD importer to cloth specific parent materials.
	// This is because we want the materials to import into UEFN and the default USD ones
	// use operations that are not allowed.
	Private::OverrideUsdImportMaterials(Private::UsdClothOverrideMaterials, &OriginalUsdMaterials);
	
	UUsdStageImporter UsdStageImporter;
	UsdStageImporter.ImportFromFile(ImportContext);
	
	// Restore Original USD Materials
	Private::OverrideUsdImportMaterials(OriginalUsdMaterials);

	SlowTask.EnterProgressFrame(2.f);

	// Import sim mesh into collection cache 
	// TODO: Until we have a schema so that we can use the asset cache and remove the collection cache

	// Retrieve stage infos
	const int AxesOrder[] = { 0, (StageInfo.UpAxis == EUsdUpAxis::ZAxis) ? 1 : 2, (StageInfo.UpAxis == EUsdUpAxis::ZAxis) ? 2 : 1 };
	const int WindingOrder[] = { 0, (StageInfo.UpAxis == EUsdUpAxis::ZAxis) ? 2 : 1, (StageInfo.UpAxis == EUsdUpAxis::ZAxis) ? 1 : 2 };
	const float CentimetersPerUnit = StageInfo.MetersPerUnit * 100.f;

	// Sewings
	TArray<TArray<FIntVector2>> SeamStitches;
	TArray<TArray<FIntVector2>> SeamPatterns;

	const UE::FSdfPath SewingsPath = UE::FSdfPath(UE::FSdfPath::AbsoluteRootPath()).AppendChild(TEXT("SimulationData")).AppendChild(TEXT("Sewings"));
	if (const UE::FUsdPrim SewingsPrim = UsdStage.GetPrimAtPath(SewingsPath))
	{
		const TArray<UE::FUsdPrim> SewingsPrimChildren = SewingsPrim.GetChildren();
		const int32 NumSewingsPrimChildren = SewingsPrimChildren.Num();
		SeamStitches.Reserve(NumSewingsPrimChildren);
		SeamPatterns.Reserve(NumSewingsPrimChildren);

		for (const UE::FUsdPrim& SewingPrim : SewingsPrimChildren)
		{
			const UE::FUsdAttribute PLIndexPairAttr = SewingPrim.GetAttribute(TEXT("PLIndexPair"));
			if (PLIndexPairAttr.HasValue() && PLIndexPairAttr.GetTypeName() == TEXT("int4[]"))
			{
				UE::FVtValue Value;
				PLIndexPairAttr.Get(Value);
				UsdUtils::FConvertedVtValue ConvertedVtValue;
				if (UsdToUnreal::ConvertValue(Value, ConvertedVtValue) && ConvertedVtValue.bIsArrayValued && !ConvertedVtValue.bIsEmpty)
				{
					if (const int32 NumSewings = ConvertedVtValue.Entries.Num())
					{
						TArray<FIntVector2> SewingPatterns;
						TArray<FIntVector2> SewingStitches;
						SewingPatterns.SetNumUninitialized(NumSewings);
						SewingStitches.SetNumUninitialized(NumSewings);

						check(ConvertedVtValue.Entries[0].Num() == 4);
						check(ConvertedVtValue.Entries[0][0].IsType<int32>());

						for (int32 Index = 0; Index < NumSewings; ++Index)
						{
							SewingPatterns[Index] = FIntVector2(
								ConvertedVtValue.Entries[Index][0].Get<int32>(),
								ConvertedVtValue.Entries[Index][2].Get<int32>());
							SewingStitches[Index] = FIntVector2(
								ConvertedVtValue.Entries[Index][1].Get<int32>(),
								ConvertedVtValue.Entries[Index][3].Get<int32>());

							UE_LOG(
								LogChaosClothAssetDataflowNodes,
								VeryVerbose,
								TEXT("Sewing %d:%d-%d:%d"),
								SewingPatterns[Index][0],
								SewingStitches[Index][0],
								SewingPatterns[Index][1],
								SewingStitches[Index][1]);
						}

						SeamPatterns.Emplace(MoveTemp(SewingPatterns));
						SeamStitches.Emplace(MoveTemp(SewingStitches));
					}
				}
			}
		}
	}

	auto FillIntDatas = [](const UE::FUsdPrim& UsdPrim, const FString& DatasName, uint32& IntDatas) 
	{
		const UE::FUsdAttribute IntDatasAttr = UsdPrim.GetAttribute(*DatasName);
		if (IntDatasAttr.HasValue() && IntDatasAttr.GetTypeName() == TEXT("uint"))
		{
			UE::FVtValue Value;
			IntDatasAttr.Get(Value);
			const TOptional<uint32> Optional = UsdUtils::GetUnderlyingValue<uint32>(Value);
			IntDatas = Optional.IsSet() ? Optional.GetValue() : 0;
		}
	};
	auto FillFloatDatas = [](const UE::FUsdPrim& UsdPrim, const FString& DatasName, float& FloatDatas) 
	{
		const UE::FUsdAttribute FloatDatasAttr = UsdPrim.GetAttribute(*DatasName);
		if (FloatDatasAttr.HasValue() && FloatDatasAttr.GetTypeName() == TEXT("float"))
		{
			UE::FVtValue Value;
			FloatDatasAttr.Get(Value);
			const TOptional<float> Optional = UsdUtils::GetUnderlyingValue<float>(Value);
			FloatDatas = Optional.IsSet() ? Optional.GetValue() : 0.0f;
		}
	};
	auto FillVectorDatas = [&AxesOrder](const UE::FUsdPrim& UsdPrim, const FString& DatasName, FVector3f& VectorDatas) 
	{
		const UE::FUsdAttribute VectorDatasAttr = UsdPrim.GetAttribute(*DatasName);
		if (VectorDatasAttr.HasValue() && VectorDatasAttr.GetTypeName() == TEXT("float3"))
		{
			UE::FVtValue Value;
			VectorDatasAttr.Get(Value);
			UsdUtils::FConvertedVtValue ConvertedVtValue;
			if (UsdToUnreal::ConvertValue(Value, ConvertedVtValue) && !ConvertedVtValue.bIsArrayValued && !ConvertedVtValue.bIsEmpty)
			{
				VectorDatas = FVector3f(ConvertedVtValue.Entries[0][AxesOrder[0]].Get<float>(),
				ConvertedVtValue.Entries[0][AxesOrder[1]].Get<float>(),
				ConvertedVtValue.Entries[0][AxesOrder[2]].Get<float>());
			}
		}
	};

	auto FillFloatArrayDatas = [](const UE::FUsdPrim& UsdPrim, const FString& DatasName, TArray<float>& FloatArrayDatas) 
	{
		const UE::FUsdAttribute FloatArrayDatasAttr = UsdPrim.GetAttribute(*DatasName);
		if (FloatArrayDatasAttr.HasValue() && FloatArrayDatasAttr.GetTypeName() == TEXT("float[]"))
		{
			UE::FVtValue Value;
			FloatArrayDatasAttr.Get(Value);
			UsdUtils::FConvertedVtValue ConvertedVtValue;
			if (UsdToUnreal::ConvertValue(Value, ConvertedVtValue) && ConvertedVtValue.bIsArrayValued && !ConvertedVtValue.bIsEmpty)
			{
				if (const int32 NumEntries = ConvertedVtValue.Entries.Num())
				{
					for (int32 Index = 0; Index < NumEntries; ++Index)
					{
						FloatArrayDatas.Add(ConvertedVtValue.Entries[Index][0].Get<float>());
					}
				}
			}
		}
	};

	auto FillIntArrayDatas = [](const UE::FUsdPrim& UsdPrim, const FString& DatasName, TArray<int32>& IntArrayDatas) 
	{
		const UE::FUsdAttribute IntArrayDatasAttr = UsdPrim.GetAttribute(*DatasName);
		if (IntArrayDatasAttr.HasValue() && IntArrayDatasAttr.GetTypeName() == TEXT("int[]"))
		{
			UE::FVtValue Value;
			IntArrayDatasAttr.Get(Value);
			UsdUtils::FConvertedVtValue ConvertedVtValue;
			if (UsdToUnreal::ConvertValue(Value, ConvertedVtValue) && ConvertedVtValue.bIsArrayValued && !ConvertedVtValue.bIsEmpty)
			{
				if (const int32 NumTriangles = ConvertedVtValue.Entries.Num())
				{
					UE_LOG(LogChaosClothAssetDataflowNodes, Log, TEXT("Num Triangles = %d"), NumTriangles);
					for (int32 Index = 0; Index < NumTriangles; ++Index)
					{
						UE_LOG(LogChaosClothAssetDataflowNodes, Log, TEXT("Num Entries[%d] = %d"), Index, ConvertedVtValue.Entries[Index].Num());
						IntArrayDatas.Add(ConvertedVtValue.Entries[Index][0].Get<int32>());
					}
				}
			}
		}
	};

	// Simulation properties
	const UE::FSdfPath SimulationPropertiesPath = UE::FSdfPath(UE::FSdfPath::AbsoluteRootPath()).AppendChild(TEXT("SimulationData")).AppendChild(TEXT("SimulationProperties"));
	if (const UE::FUsdPrim SimulationPropertiesPrim = UsdStage.GetPrimAtPath(SimulationPropertiesPath))
	{
		float AirDamping = 0.1f;
		FillFloatDatas(SimulationPropertiesPrim, TEXT("AirDamping"), AirDamping);

		FVector3f Gravity(0.0f, 0.0f, -9810.0f);
		FillVectorDatas(SimulationPropertiesPrim, TEXT("Gravity"), Gravity);

		float TimeStep = 0.033f;
		FillFloatDatas(SimulationPropertiesPrim, TEXT("TimeStep"), TimeStep);
		
		uint32 SubSteps = 1;
		FillIntDatas(SimulationPropertiesPrim, TEXT("SubStepCount"), SubSteps);

		static constexpr float GravityScaling = 1e-1f; // from mm to cm

		ClothFacade.SetSolverGravity(Gravity * GravityScaling);
		ClothFacade.SetSolverAirDamping(AirDamping);
		ClothFacade.SetSolverTimeStep(TimeStep);
		ClothFacade.SetSolverSubSteps(SubSteps);
	}

	// Fabrics
	TArray<uint32> FabricIds;
	const UE::FSdfPath FabricsPath = UE::FSdfPath(UE::FSdfPath::AbsoluteRootPath()).AppendChild(TEXT("SimulationData")).AppendChild(TEXT("Fabrics"));
	if (const UE::FUsdPrim FabricsPrim = UsdStage.GetPrimAtPath(FabricsPath))
	{
		for (const UE::FUsdPrim& FabricPrim : FabricsPrim.GetChildren())
		{
			float BendingBiasLeft = 0.0f;
			FillFloatDatas(FabricPrim, TEXT("BendingBiasLeft"), BendingBiasLeft);

			float BendingBiasRight= 0.0f;
			FillFloatDatas(FabricPrim, TEXT("BendingBiasRight"), BendingBiasRight);

			float BendingWarp = 0.0f;
			FillFloatDatas(FabricPrim, TEXT("BendingWarp"), BendingWarp);

			float BendingWeft = 0.0f;
			FillFloatDatas(FabricPrim, TEXT("BendingWeft"), BendingWeft);

			float BucklingRatioBiasLeft = 0.0f;
			FillFloatDatas(FabricPrim, TEXT("BucklingRatioBiasLeft"), BucklingRatioBiasLeft);

			float BucklingRatioBiasRight = 0.0f;
			FillFloatDatas(FabricPrim, TEXT("BucklingRatioBiasRight"), BucklingRatioBiasRight);
			
			float BucklingRatioWarp = 0.0f;
			FillFloatDatas(FabricPrim, TEXT("BucklingRatioWarp"), BucklingRatioWarp);

			float BucklingRatioWeft = 0.0f;
			FillFloatDatas(FabricPrim, TEXT("BucklingRatioWeft"), BucklingRatioWeft);

			float BucklingStiffnessBiasLeft = 0.0f;
			FillFloatDatas(FabricPrim, TEXT("BucklingStiffnessBiasLeft"), BucklingStiffnessBiasLeft);

			float BucklingStiffnessBiasRight = 0.0f;
			FillFloatDatas(FabricPrim, TEXT("BucklingStiffnessBiasRight"), BucklingStiffnessBiasRight);

			float BucklingStiffnessWarp = 0.0f;
			FillFloatDatas(FabricPrim, TEXT("BucklingStiffnessWarp"), BucklingStiffnessWarp);

			float BucklingStiffnessWeft = 0.0f;
			FillFloatDatas(FabricPrim, TEXT("BucklingStiffnessWeft"), BucklingStiffnessWeft);

			float Density = 0.0f;
			FillFloatDatas(FabricPrim, TEXT("Density"), Density);

			float Friction = 0.0f;
			FillFloatDatas(FabricPrim, TEXT("Friction"), Friction);

			float Damping = 0.0f;
			FillFloatDatas(FabricPrim, TEXT("InternalDamping"), Damping);

			float Thickness = 0.0f;
			FillFloatDatas(FabricPrim, TEXT("Thickness"), Thickness);

			float ShearLeft = 0.0f;
			FillFloatDatas(FabricPrim, TEXT("ShearLeft"), ShearLeft);

			float ShearRight = 0.0f;
			FillFloatDatas(FabricPrim, TEXT("ShearRight"), ShearRight);

			float StretchWarp = 0.0f;
			FillFloatDatas(FabricPrim, TEXT("StretchWarp"), StretchWarp);

			float StretchWeft = 0.0f;
			FillFloatDatas(FabricPrim, TEXT("StretchWeft"), StretchWeft);

			uint32 FabricId = 0;
			FillIntDatas(FabricPrim, TEXT("FabricId"), FabricId);

			FCollectionClothFabricFacade Fabric = ClothFacade.AddGetFabric();

			static constexpr float BendingScaling = 1e-5f; // from g.mm2/s2 to kg.cm2/s2
			static constexpr float StretchShearScaling = 1e-3f; // from g/s2 to kg/s2
			static constexpr float DensityScaling = 1e+3f; // from g/mm2 to kg/m2
			static constexpr float ThicknessScaling = 1e-1f; // from mm to cm
			
			FCollectionClothFabricFacade::FAnisotropicData BendingStiffness(
				  BendingWeft*BendingScaling, BendingWarp*BendingScaling, 0.5f * (BendingBiasLeft+BendingBiasRight)*BendingScaling);
			
			FCollectionClothFabricFacade::FAnisotropicData StretchStiffness(
				StretchWeft*StretchShearScaling, StretchWarp*StretchShearScaling, 0.5f * (ShearLeft+ShearRight)*StretchShearScaling);

			// Only scalar value used in the solver right now
			const float BucklingRatio = (BucklingRatioWeft + BucklingRatioWarp +
				0.5f * (BucklingRatioBiasLeft + BucklingRatioBiasRight)) / 3.0f;
			
			FCollectionClothFabricFacade::FAnisotropicData BucklingStiffness = 
				BucklingRatio < UE_SMALL_NUMBER ? BendingStiffness : 
				FCollectionClothFabricFacade::FAnisotropicData(
				BendingStiffness.Weft * BucklingStiffnessWeft, BendingStiffness.Warp * BucklingStiffnessWarp,
				BendingStiffness.Bias * 0.5f * (BucklingStiffnessBiasLeft+BucklingStiffnessBiasRight));


			Fabric.Initialize(BendingStiffness, BucklingRatio, BucklingStiffness, StretchStiffness,
				Density * DensityScaling, Friction, Damping, 0.0f, 0, Thickness * ThicknessScaling);

			FabricIds.Add(FabricId);
		}
	}

	// Patterns
	const UE::FSdfPath PatternsPath = UE::FSdfPath(UE::FSdfPath::AbsoluteRootPath()).AppendChild(TEXT("SimulationData")).AppendChild(TEXT("Patterns"));
	if (const UE::FUsdPrim PatternsPrim = UsdStage.GetPrimAtPath(PatternsPath))
	{
		for (const UE::FUsdPrim& PatternPrim : PatternsPrim.GetChildren())
		{
			uint32 TriangleCount = 0;
			FillIntDatas(PatternPrim, TEXT("TriangleCount"), TriangleCount);

			uint32 VertexCount = 0;
			FillIntDatas(PatternPrim, TEXT("VertexCount"), VertexCount);

			uint32 PatternId = 0;
			FillIntDatas(PatternPrim, TEXT("PatternId"), PatternId);

			uint32 FabricIndex= 0;
			if(UE::FUsdRelationship Relationship = PatternPrim.GetRelationship(TEXT("fabric")))
			{
				TArray<UE::FSdfPath> TargetsPath;
				if(Relationship.GetTargets(TargetsPath))
				{
					if (TargetsPath.Num() > 0)
                    {
						UE::FUsdPrim FabricPrim = PatternPrim.GetStage().GetPrimAtPath(TargetsPath[0]);

						uint32 FabricId = 0;
						FillIntDatas(FabricPrim, TEXT("FabricId"), FabricId);

						// The fabric index referenced in the pattern is the index of the fabric in the managed array collection
						FabricIndex = FabricIds.Find(FabricId);
                    }
				}
			}

			UE_LOG(LogChaosClothAssetDataflowNodes,
				Display,
				TEXT("Found SimPattern %s, ID %d: %d triangles, %d vertices"), *PatternPrim.GetName().ToString(), PatternId, TriangleCount, VertexCount);

			TArray<FVector2f> RestPositions2D;
			const UE::FUsdAttribute RestPositions2DAttr = PatternPrim.GetAttribute(TEXT("RestPositions2D"));
			if (RestPositions2DAttr.HasValue() && RestPositions2DAttr.GetTypeName() == TEXT("float2[]"))
			{
				UE::FVtValue Value;
				RestPositions2DAttr.Get(Value);
				UsdUtils::FConvertedVtValue ConvertedVtValue;
				if (UsdToUnreal::ConvertValue(Value, ConvertedVtValue) && ConvertedVtValue.bIsArrayValued && !ConvertedVtValue.bIsEmpty)
				{
					RestPositions2D.SetNum(ConvertedVtValue.Entries.Num());
					check(ConvertedVtValue.Entries[0].Num() == 2);
					check(ConvertedVtValue.Entries[0][0].IsType<float>());

					for (int32 Index = 0; Index < RestPositions2D.Num(); ++Index)
					{
						RestPositions2D[Index] = FVector2f(
							ConvertedVtValue.Entries[Index][0].Get<float>(),
							ConvertedVtValue.Entries[Index][1].Get<float>());

						UE_LOG(LogChaosClothAssetDataflowNodes,
							VeryVerbose,
							TEXT("RestPositions2D %f %f"), RestPositions2D[Index][0], RestPositions2D[Index][1]);
					}
				}
			}
			ensure(RestPositions2D.Num() == (int32)VertexCount);

			TArray<FVector3f> DrapedPositions3D;
			const UE::FUsdAttribute DrapedPositions3DAttr = PatternPrim.GetAttribute(TEXT("DrapedPositions3D"));
			if (DrapedPositions3DAttr.HasValue() && DrapedPositions3DAttr.GetTypeName() == TEXT("float3[]"))
			{
				UE::FVtValue Value;
				DrapedPositions3DAttr.Get(Value);
				UsdUtils::FConvertedVtValue ConvertedVtValue;
				if (UsdToUnreal::ConvertValue(Value, ConvertedVtValue) && ConvertedVtValue.bIsArrayValued && !ConvertedVtValue.bIsEmpty)
				{
					DrapedPositions3D.SetNum(ConvertedVtValue.Entries.Num());
					check(ConvertedVtValue.Entries[0].Num() == 3);
					check(ConvertedVtValue.Entries[0][0].IsType<float>());

					for (int32 Index = 0; Index < DrapedPositions3D.Num(); ++Index)
					{
						DrapedPositions3D[Index] = FVector3f(
							ConvertedVtValue.Entries[Index][AxesOrder[0]].Get<float>(),
							ConvertedVtValue.Entries[Index][AxesOrder[1]].Get<float>(),
							ConvertedVtValue.Entries[Index][AxesOrder[2]].Get<float>()) * CentimetersPerUnit;

						UE_LOG(LogChaosClothAssetDataflowNodes,
							VeryVerbose,
							TEXT("RestPositions2D %f %f %f"), DrapedPositions3D[Index][0], DrapedPositions3D[Index][1], DrapedPositions3D[Index][2]);
					}
				}
			}
			ensure(DrapedPositions3D.Num() == (int32)VertexCount);

			TArray<FIntVector3> TriangleToVertexIndex;
			const UE::FUsdAttribute TriangleToVertexIndexAttr = PatternPrim.GetAttribute(TEXT("TriangleToVertexIndex"));
			if (TriangleToVertexIndexAttr.HasValue() && TriangleToVertexIndexAttr.GetTypeName() == TEXT("int3[]"))
			{
				UE::FVtValue Value;
				TriangleToVertexIndexAttr.Get(Value);
				UsdUtils::FConvertedVtValue ConvertedVtValue;
				if (UsdToUnreal::ConvertValue(Value, ConvertedVtValue) && ConvertedVtValue.bIsArrayValued && !ConvertedVtValue.bIsEmpty)
				{
					TriangleToVertexIndex.SetNum(ConvertedVtValue.Entries.Num());
					check(ConvertedVtValue.Entries[0].Num() == 3);
					check(ConvertedVtValue.Entries[0][0].IsType<int32>());

					for (int32 Index = 0; Index < TriangleToVertexIndex.Num(); ++Index)
					{
						TriangleToVertexIndex[Index] = FIntVector3(
							ConvertedVtValue.Entries[Index][WindingOrder[0]].Get<int32>(),
							ConvertedVtValue.Entries[Index][WindingOrder[1]].Get<int32>(),
							ConvertedVtValue.Entries[Index][WindingOrder[2]].Get<int32>());

						UE_LOG(LogChaosClothAssetDataflowNodes,
							VeryVerbose,
							TEXT("TriangleToVertexIndex %d %d %d"), TriangleToVertexIndex[Index][0], TriangleToVertexIndex[Index][1], TriangleToVertexIndex[Index][2]);
					}
				}
			}
			ensure(TriangleToVertexIndex.Num() == (int32)TriangleCount);

			// Save pattern to the collection cache
			if (TriangleCount && VertexCount)
			{
				// Remove degenerated triangles
				TArray<FIntVector3> OutTriangleToVertexIndex;
				TArray<FVector2f> OutRestPositions2D;
				TArray<FVector3f> OutDrapedPositions3D;
				TArray<int32> OutIndices;

				bool bHasRepairedTriangles = Private::RemoveDegenerateTriangles(
					TriangleToVertexIndex,
					RestPositions2D,
					DrapedPositions3D,
					OutTriangleToVertexIndex,
					OutRestPositions2D,
					OutDrapedPositions3D,
					OutIndices);

				// Remove duplicate triangles
				bHasRepairedTriangles = Private::RemoveDuplicateTriangles(OutTriangleToVertexIndex) || bHasRepairedTriangles;

				// Add the new pattern
				const int32 SimPatternIndex = ClothFacade.AddSimPattern();
				FCollectionClothSimPatternFacade SimPattern = ClothFacade.GetSimPattern(SimPatternIndex);
				SimPattern.Initialize(OutRestPositions2D, OutDrapedPositions3D, OutTriangleToVertexIndex, FabricIndex);

				// Remap this pattern's stitches
				check(SeamPatterns.Num() == SeamStitches.Num());
				for (int32 SeamIndex = 0; SeamIndex < SeamStitches.Num(); ++SeamIndex)
				{
					check(SeamPatterns[SeamIndex].Num() == SeamStitches[SeamIndex].Num());

					for (int32 StitchIndex = 0; StitchIndex < SeamStitches[SeamIndex].Num(); ++StitchIndex)
					{
						FIntVector2& SeamPattern = SeamPatterns[SeamIndex][StitchIndex];
						FIntVector2& SeamStitch = SeamStitches[SeamIndex][StitchIndex];

						for (int32 Side = 0; Side < 2; ++Side)
						{
							if (SeamPattern[Side] == (int32)PatternId)
							{
								SeamPattern[Side] = INDEX_NONE;  // In case two patterns were to be exported with the same id.
								SeamStitch[Side] = OutIndices[SeamStitch[Side]] + SimPattern.GetSimVertices2DOffset();
							}
						}
					}
				}

				// Flag vertices of problem triangles for info
				if (bHasRepairedTriangles)
				{
					// TODO: Make this a feature or remove it?
					const FName WeightMapName(TEXT("_RepairedTriangles"));  // The undescore means this is an internal weight map name
					const TConstArrayView<int32> SimVertex3DLookup = static_cast<FCollectionClothSimPatternConstFacade&>(SimPattern).GetSimVertex3DLookup();
					ClothFacade.AddWeightMap(WeightMapName);
					const TArrayView<float> WeightMap = ClothFacade.GetWeightMap(WeightMapName);

					for (int32 TriangleIndex = 0, OutTriangleIndex = 0; OutTriangleIndex < OutTriangleToVertexIndex.Num(); ++TriangleIndex)
					{
						const int32 Index0 = OutIndices[TriangleToVertexIndex[TriangleIndex][0]];
						const int32 Index1 = OutIndices[TriangleToVertexIndex[TriangleIndex][1]];
						const int32 Index2 = OutIndices[TriangleToVertexIndex[TriangleIndex][2]];

						if (Index0 == OutTriangleToVertexIndex[OutTriangleIndex][0] &&
							Index1 == OutTriangleToVertexIndex[OutTriangleIndex][1] &&
							Index2 == OutTriangleToVertexIndex[OutTriangleIndex][2])
						{
							++OutTriangleIndex;
						}
						else
						{
							WeightMap[SimVertex3DLookup[Index0]] = WeightMap[SimVertex3DLookup[Index1]] = WeightMap[SimVertex3DLookup[Index2]] = 1.f;
						}
					}
				}
			}
		}
	}

	// Triangles
	const UE::FSdfPath TrianglesPath = UE::FSdfPath(UE::FSdfPath::AbsoluteRootPath()).AppendChild(TEXT("SimulationData")).AppendChild(TEXT("Triangles"));
	if (const UE::FUsdPrim TrianglesPrim = UsdStage.GetPrimAtPath(TrianglesPath))
	{
		const int32 NumSimFaces = ClothFacade.GetNumSimFaces();
		
		TArray<float> TrianglesCollisionThickness;
		TrianglesCollisionThickness.Reserve(NumSimFaces);
		FillFloatArrayDatas(TrianglesPrim, TEXT("CollisionThickness"), TrianglesCollisionThickness);

		TArray<int32> TrianglesPatternLayer;
		TrianglesPatternLayer.Reserve(NumSimFaces);
		FillIntArrayDatas(TrianglesPrim, TEXT("Layer"), TrianglesPatternLayer);

		TArray<float> TrianglesPatternPressure;
		TrianglesPatternPressure.Reserve(NumSimFaces);
		FillFloatArrayDatas(TrianglesPrim, TEXT("Pressure"), TrianglesPatternPressure);

		if(TrianglesCollisionThickness.Num() == TrianglesPatternLayer.Num() &&
			TrianglesCollisionThickness.Num() == TrianglesPatternPressure.Num() &&
			TrianglesCollisionThickness.Num() == NumSimFaces)
		{
			// Struct datas that will be used to retrieve the correct fabric index
			struct FFabricPatternDatas
			{
				float PatternPressure;
				int32 PatternLayer;
				float CollisionThickness;
				int32 FabricIndex;
			};
			const int32 NumPatterns = ClothFacade.GetNumSimPatterns();
			TArray<TArray<FFabricPatternDatas>> FabricPatternDatas;
			FabricPatternDatas.SetNum(ClothFacade.GetNumFabrics());
			for(int32 PatternIndex = 0; PatternIndex < NumPatterns; ++PatternIndex)
			{
				FCollectionClothSimPatternFacade PatternFacade = ClothFacade.GetSimPattern(PatternIndex);
				
				const int32 PatternFacesStart = PatternFacade.GetSimFacesOffset();
				const int32 PatternFacesEnd = PatternFacade.GetNumSimFaces() + PatternFacesStart;

				float CollisionThickness = 0.0;
				float PatternPressure = 0.0;
				int32 PatternLayer = (PatternFacade.GetNumSimFaces() > 0) ? TrianglesPatternLayer[PatternFacesStart] : INDEX_NONE;
				bool bHasUniformLayer = true;
				for(int32 PatternFaceIndex = PatternFacesStart; PatternFaceIndex < PatternFacesEnd; ++PatternFaceIndex)
				{
					CollisionThickness += TrianglesCollisionThickness[PatternFaceIndex];
					PatternPressure += TrianglesPatternPressure[PatternFaceIndex];
					bHasUniformLayer = (TrianglesPatternLayer[PatternFaceIndex] != PatternLayer) ? false : bHasUniformLayer;
				}
				PatternLayer = !bHasUniformLayer ? INDEX_NONE : PatternLayer;
				CollisionThickness /= PatternFacade.GetNumSimFaces();
				PatternPressure /= PatternFacade.GetNumSimFaces();

				static constexpr float ThicknessScaling = 1e-1f; // from mm to cm
				CollisionThickness *= ThicknessScaling;

				const int32 FabricIndex = PatternFacade.GetFabricIndex();
				if(FabricIndex >= 0 && FabricIndex < ClothFacade.GetNumFabrics())
				{
					FCollectionClothFabricFacade OldFabricFacade = ClothFacade.GetFabric(FabricIndex);
					if(FabricPatternDatas[FabricIndex].IsEmpty())
					{
						// If empty we update the existing fabric
						OldFabricFacade.Initialize(OldFabricFacade, PatternPressure, PatternLayer, CollisionThickness);
						
						// Store the existing fabric into the array for future potential reuse
						FabricPatternDatas[FabricIndex].Add({PatternPressure, PatternLayer, CollisionThickness, FabricIndex});
					}
					else
					{
						bool bFoundMatchingFabric = false;
						for(FFabricPatternDatas& PatternDatas : FabricPatternDatas[FabricIndex])
						{
							// If the fabric already in use and if the pattern datas are matching, reuse the fabric 
							if((PatternDatas.CollisionThickness == CollisionThickness) && (PatternDatas.PatternLayer == PatternLayer) && (PatternDatas.PatternPressure == PatternPressure))
							{
								bFoundMatchingFabric = true;
								PatternFacade.SetFabricIndex(PatternDatas.FabricIndex);
								break;
							}
						}
						if(!bFoundMatchingFabric)
						{
							FCollectionClothFabricFacade NewFabricFacade = ClothFacade.AddGetFabric();
							NewFabricFacade.Initialize(OldFabricFacade, PatternPressure, PatternLayer, CollisionThickness);

							// Store the new fabric into the array for future potential reuse
							FabricPatternDatas[FabricIndex].Add({PatternPressure, PatternLayer, CollisionThickness, NewFabricFacade.GetElementIndex()});
							PatternFacade.SetFabricIndex(NewFabricFacade.GetElementIndex());
						}
					}
				}
			}
		}
	}

	// Check for duplicate stitches
	Private::RemoveDuplicateStitches(SeamStitches);

	// Add seams
	for (int32 SeamIndex = 0; SeamIndex < SeamStitches.Num(); ++SeamIndex)
	{
		FCollectionClothSeamFacade Seam = ClothFacade.AddGetSeam();
		Seam.Initialize(SeamStitches[SeamIndex]);
	}
	
	SlowTask.EnterProgressFrame(1.f);
	SlowTask.ForceRefresh();
	return true;

#else  // #if USE_USD_SDK

	OutErrorText = LOCTEXT("NoUsdSdk", "The ChaosClothAssetDataflowNodes module has been compiled without the USD SDK enabled.");
	return false;

#endif  // #else #if USE_USD_SDK
}

bool FChaosClothAssetUSDImportNode::ImportFromCache(const TSharedRef<FManagedArrayCollection>& OutClothCollection, FText& OutErrorText) const
{
	using namespace UE::Chaos::ClothAsset;

	// Initialize from collection cache
	// TODO: Until we have a schema so that we can use the asset cache and remove the collection cache
	*OutClothCollection = CollectionCache;

	// Initialize from asset registry
	TArray<FAssetData> AssetData;

	if (!PackagePath.IsEmpty())
	{
		const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

		const UClass* const Class = UStaticMesh::StaticClass();
		constexpr bool bRecursive = true;
		constexpr bool bIncludeOnlyOnDiskAssets = false;
		AssetRegistryModule.Get().GetAssetsByPath(FName(*PackagePath), AssetData, bRecursive, bIncludeOnlyOnDiskAssets);
	}

	for (const FAssetData& AssetDatum : AssetData)
	{
		if (!(AssetDatum.IsUAsset() && AssetDatum.IsTopLevelAsset()))
		{
			continue;
		}

		UE_LOG(LogChaosClothAssetDataflowNodes,
			Verbose,
			TEXT("Imported USD Object %s of type %s, path: %s"),
			*AssetDatum.AssetName.ToString(),
			*AssetDatum.AssetClassPath.ToString(),
			*AssetDatum.GetFullName());

		if (AssetDatum.GetClass() == UStaticMesh::StaticClass())
		{
			UStaticMesh* const StaticMesh = CastChecked<UStaticMesh>(AssetDatum.GetAsset());

			if (StaticMesh->GetNumSourceModels() > 0)  // Only deals with LOD 0 for now
			{
				using namespace UE::Chaos::ClothAsset::Private;

				constexpr int32 LODIndex = 0;
				const FMeshDescription* const MeshDescription = StaticMesh->GetMeshDescription(LODIndex);
				const FMeshBuildSettings& BuildSettings = StaticMesh->GetSourceModel(LODIndex).BuildSettings;
				const TArray<FStaticMaterial>& StaticMaterials = StaticMesh->GetStaticMaterials();

				FSkeletalMeshLODModel SkeletalMeshModel;
				if (FClothDataflowTools::BuildSkeletalMeshModelFromMeshDescription(MeshDescription, BuildSettings, SkeletalMeshModel))
				{
					FStaticMeshConstAttributes MeshAttributes(*MeshDescription);
					TPolygonGroupAttributesConstRef<FName> MaterialSlotNames = MeshAttributes.GetPolygonGroupMaterialSlotNames();
					for (int32 SectionIndex = 0; SectionIndex < SkeletalMeshModel.Sections.Num(); ++SectionIndex)
					{
						// Section MaterialIndex refers to the polygon group index. Look up which material this corresponds with.
						const FName& MaterialSlotName = MaterialSlotNames[SkeletalMeshModel.Sections[SectionIndex].MaterialIndex];
						const int32 MaterialIndex = StaticMesh->GetMaterialIndexFromImportedMaterialSlotName(MaterialSlotName);
						const FString RenderMaterialPathName = StaticMaterials.IsValidIndex(MaterialIndex) && StaticMaterials[MaterialIndex].MaterialInterface ? 
							StaticMaterials[MaterialIndex].MaterialInterface->GetPathName() :
							FString();
						FClothDataflowTools::AddRenderPatternFromSkeletalMeshSection(OutClothCollection, SkeletalMeshModel, SectionIndex, RenderMaterialPathName);
					}
				}
			}
		}
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
