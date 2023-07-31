// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbcImporter.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

THIRD_PARTY_INCLUDES_START
#include <Alembic/AbcCoreAbstract/TimeSampling.h>
#include <Alembic/AbcCoreFactory/All.h>
#include <Alembic/AbcCoreOgawa/All.h>
THIRD_PARTY_INCLUDES_END

#include "Misc/Paths.h"
#include "Misc/FeedbackContext.h"
#include "Stats/StatsMisc.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UObjectHash.h"
#include "RawIndexBuffer.h"
#include "Misc/ScopedSlowTask.h"

#include "PackageTools.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshOperations.h"
#include "ObjectTools.h"

#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "Rendering/SkeletalMeshModel.h"

#include "AbcImportUtilities.h"
#include "Utils.h"

#include "MeshUtilities.h"
#include "Materials/Material.h"
#include "Modules/ModuleManager.h"

#include "Async/ParallelFor.h"

#include "EigenHelper.h"

#include "AbcAssetImportData.h"
#include "AbcFile.h"

#include "AnimationUtils.h"
#include "ComponentReregisterContext.h"
#include "GeometryCacheCodecV1.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Editor.h"

#include "UObject/MetaData.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "AbcImporter"

DEFINE_LOG_CATEGORY_STATIC(LogAbcImporter, Verbose, All);

#define OBJECT_TYPE_SWITCH(a, b, c) if (AbcImporterUtilities::IsType<a>(ObjectMetaData)) { \
	a TypedObject = a(b, Alembic::Abc::kWrapExisting); \
	ParseAbcObject<a>(TypedObject, c); bHandled = true; }

#define PRINT_UNIQUE_VERTICES 0

FAbcImporter::FAbcImporter()
	: ImportSettings(nullptr), AbcFile(nullptr)
{

}

FAbcImporter::~FAbcImporter()
{
	delete AbcFile;
}

void FAbcImporter::UpdateAssetImportData(UAbcAssetImportData* AssetImportData)
{
	AssetImportData->TrackNames.Empty();
	const TArray<FAbcPolyMesh*>& PolyMeshes = AbcFile->GetPolyMeshes();
	for (const FAbcPolyMesh* PolyMesh : PolyMeshes)
	{
		if (PolyMesh->bShouldImport)
		{
			AssetImportData->TrackNames.Add(PolyMesh->GetName());
		}
	}

	AssetImportData->SamplingSettings = ImportSettings->SamplingSettings;
	AssetImportData->NormalGenerationSettings = ImportSettings->NormalGenerationSettings;
	AssetImportData->CompressionSettings = ImportSettings->CompressionSettings;
	AssetImportData->StaticMeshSettings = ImportSettings->StaticMeshSettings;
	AssetImportData->GeometryCacheSettings = ImportSettings->GeometryCacheSettings;
	AssetImportData->ConversionSettings = ImportSettings->ConversionSettings;
}

void FAbcImporter::RetrieveAssetImportData(UAbcAssetImportData* AssetImportData)
{
	bool bAnySetForImport = false;

	for (FAbcPolyMesh* PolyMesh : AbcFile->GetPolyMeshes())
	{
		if (AssetImportData->TrackNames.Contains(PolyMesh->GetName()))
		{
			PolyMesh->bShouldImport = true;
			bAnySetForImport = true;
		}		
		else
		{
			PolyMesh->bShouldImport = false;
		}
	}

	// If none were set to import, set all of them to import (probably different scene/setup)
	if (!bAnySetForImport)
	{
		for (FAbcPolyMesh* PolyMesh : AbcFile->GetPolyMeshes())
		{
			PolyMesh->bShouldImport = true;
		}
	}
}

const EAbcImportError FAbcImporter::OpenAbcFileForImport(const FString InFilePath)
{
	AbcFile = new FAbcFile(InFilePath);
	return AbcFile->Open();
}

const EAbcImportError FAbcImporter::ImportTrackData(const int32 InNumThreads, UAbcImportSettings* InImportSettings)
{
	ImportSettings = InImportSettings;
	ImportSettings->NumThreads = InNumThreads;
	EAbcImportError Error = AbcFile->Import(ImportSettings);	

	return Error;
}

template<typename T>
T* FAbcImporter::CreateObjectInstance(UObject*& InParent, const FString& ObjectName, const EObjectFlags Flags)
{
	// Parent package to place new mesh
	UPackage* Package = nullptr;
	FString NewPackageName;

	// Setup package name and create one accordingly
	NewPackageName = FPackageName::GetLongPackagePath(InParent->GetOutermost()->GetName() + TEXT("/") + ObjectName);
	NewPackageName = UPackageTools::SanitizePackageName(NewPackageName);
	Package = CreatePackage(*NewPackageName);

	const FString SanitizedObjectName = ObjectTools::SanitizeObjectName(ObjectName);

	T* ExistingTypedObject = FindObject<T>(Package, *SanitizedObjectName);
	UObject* ExistingObject = FindObject<UObject>(Package, *SanitizedObjectName);

	if (ExistingTypedObject != nullptr)
	{
		ExistingTypedObject->PreEditChange(nullptr);
	}
	else if (ExistingObject != nullptr)
	{
		// Replacing an object.  Here we go!
		// Delete the existing object
		const bool bDeleteSucceeded = ObjectTools::DeleteSingleObject(ExistingObject);

		if (bDeleteSucceeded)
		{
			// Force GC so we can cleanly create a new asset (and not do an 'in place' replacement)
			CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);

			// Create a package for each mesh
			Package = CreatePackage(*NewPackageName);
			InParent = Package;
		}
		else
		{
			// failed to delete
			return nullptr;
		}
	}

	return NewObject<T>(Package, FName(*SanitizedObjectName), Flags | RF_Public);
}

UStaticMesh* FAbcImporter::CreateStaticMeshFromSample(UObject* InParent, const FString& Name, EObjectFlags Flags, const uint32 NumMaterials, const TArray<FString>& FaceSetNames, const FAbcMeshSample* Sample)
{
	UStaticMesh* StaticMesh = CreateObjectInstance<UStaticMesh>(InParent, Name, Flags);

	// Only import data if a valid object was created
	if (StaticMesh)
	{
		// Add the first LOD, we only support one
		int32 LODIndex = 0;
		StaticMesh->AddSourceModel();
		FMeshDescription* MeshDescription = StaticMesh->CreateMeshDescription(LODIndex);
		// Generate a new lighting GUID (so its unique)
		StaticMesh->SetLightingGuid();

		// Set it to use textured lightmaps. Note that Build Lighting will do the error-checking (texcoord index exists for all LODs, etc).
		StaticMesh->SetLightMapResolution(64);
		StaticMesh->SetLightMapCoordinateIndex(1);

		// Material setup, since there isn't much material information in the Alembic file, 
		UMaterial* DefaultMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
		check(DefaultMaterial);

		// Material list
		StaticMesh->GetStaticMaterials().Empty();
		// If there were FaceSets available in the Alembic file use the number of unique face sets as num material entries, otherwise default to one material for the whole mesh
		const uint32 FrameIndex = 0;
		uint32 NumFaceSets = FaceSetNames.Num();

		const bool bCreateMaterial = ImportSettings->MaterialSettings.bCreateMaterials;
		for (uint32 MaterialIndex = 0; MaterialIndex < ((NumMaterials != 0) ? NumMaterials : 1); ++MaterialIndex)
		{
			UMaterialInterface* Material = nullptr;
			if (FaceSetNames.IsValidIndex(MaterialIndex))
			{
				Material = AbcImporterUtilities::RetrieveMaterial(*AbcFile, FaceSetNames[MaterialIndex], InParent, Flags);

				if (Material != UMaterial::GetDefaultMaterial(MD_Surface))
				{
					Material->PostEditChange();
				}
			}

			StaticMesh->GetStaticMaterials().Add((Material != nullptr) ? Material : DefaultMaterial);
		}

		GenerateMeshDescriptionFromSample(Sample, MeshDescription, StaticMesh);

		// Get the first LOD for filling it up with geometry, only support one LOD
		FStaticMeshSourceModel& SrcModel = StaticMesh->GetSourceModel(LODIndex);
		// Set build settings for the static mesh
		SrcModel.BuildSettings.bRecomputeNormals = false;
		SrcModel.BuildSettings.bRecomputeTangents = false;
		SrcModel.BuildSettings.bUseMikkTSpace = false;
		// Generate Lightmaps uvs (no support for importing right now)
		SrcModel.BuildSettings.bGenerateLightmapUVs = ImportSettings->StaticMeshSettings.bGenerateLightmapUVs;
		// Set lightmap UV index to 1 since we currently only import one set of UVs from the Alembic Data file
		SrcModel.BuildSettings.DstLightmapIndex = 1;

		// Store the mesh description
		StaticMesh->CommitMeshDescription(LODIndex);

		//Set the Imported version before calling the build
		StaticMesh->ImportVersion = EImportStaticMeshVersion::LastVersion;

		// Build the static mesh (using the build setting etc.) this generates correct tangents using the extracting smoothing group along with the imported Normals data
		StaticMesh->Build(false);

		// No collision generation for now
		StaticMesh->CreateBodySetup();
	}

	return StaticMesh;
}

const TArray<UStaticMesh*> FAbcImporter::ImportAsStaticMesh(UObject* InParent, EObjectFlags Flags)
{
	checkf(AbcFile->GetNumPolyMeshes() > 0, TEXT("No poly meshes found"));

	TArray<UStaticMesh*> ImportedStaticMeshes;
	const FAbcStaticMeshSettings& StaticMeshSettings = ImportSettings->StaticMeshSettings;

	TFunction<void(int32, FAbcFile*)> Func = [this, &ImportedStaticMeshes, StaticMeshSettings, InParent, Flags](int32 FrameIndex, FAbcFile* InFile)
	{
		const TArray<FAbcPolyMesh*>& PolyMeshes = AbcFile->GetPolyMeshes();
		if (StaticMeshSettings.bMergeMeshes)
		{
			// If merging we merge all the raw mesh structures together and generate a static mesh asset from this
			TArray<FString> MergedFaceSetNames;
			TArray<FAbcMeshSample*> Samples;
			uint32 TotalNumMaterials = 0;

			TArray<const FAbcMeshSample*> SamplesToMerge;
			// Should merge all samples in the Alembic cache to one single static mesh
			for (const FAbcPolyMesh* PolyMesh : PolyMeshes)
			{
				if (PolyMesh->bShouldImport)
				{
					const FAbcMeshSample* Sample = PolyMesh->GetSample(FrameIndex);
					SamplesToMerge.Add(Sample);
					TotalNumMaterials += (Sample->NumMaterials != 0) ? Sample->NumMaterials : 1;

					if (PolyMesh->FaceSetNames.Num() > 0)
					{
						MergedFaceSetNames.Append(PolyMesh->FaceSetNames);
					}
					else
					{
						// Default name
						static const FString DefaultName("NoFaceSetName");
						MergedFaceSetNames.Add(DefaultName);
					}
				}
			}

			// Only merged samples if there are any
			if (SamplesToMerge.Num())
			{
				FAbcMeshSample* MergedSample = AbcImporterUtilities::MergeMeshSamples(SamplesToMerge);


				UStaticMesh* StaticMesh = CreateStaticMeshFromSample(InParent, InParent != GetTransientPackage() ? FPaths::GetBaseFilename(InParent->GetName()) : (FPaths::GetBaseFilename(AbcFile->GetFilePath()) + "_" + FGuid::NewGuid().ToString()), Flags, TotalNumMaterials, MergedFaceSetNames, MergedSample);
				if (StaticMesh)
				{
						ImportedStaticMeshes.Add(StaticMesh);
				}
			}
		}
		else
		{
			for (const FAbcPolyMesh* PolyMesh : PolyMeshes)
			{
				const FAbcMeshSample* Sample = PolyMesh->GetSample(FrameIndex);
				if (PolyMesh->bShouldImport && Sample)
				{
					// Setup static mesh instance
					UStaticMesh* StaticMesh = CreateStaticMeshFromSample(InParent, InParent != GetTransientPackage() ? PolyMesh->GetName() : PolyMesh->GetName() + "_" + FGuid::NewGuid().ToString(), Flags, Sample->NumMaterials, PolyMesh->FaceSetNames, Sample);

					if (StaticMesh)
					{
						ImportedStaticMeshes.Add(StaticMesh);
					}
				}
			}
		}
	};
	

	EFrameReadFlags ReadFlags = ( ImportSettings->StaticMeshSettings.bPropagateMatrixTransformations ? EFrameReadFlags::ApplyMatrix : EFrameReadFlags::None ) | EFrameReadFlags::ForceSingleThreaded;
	AbcFile->ProcessFrames(Func, ReadFlags);
	
	TArray<UObject*> Assets;
	Assets.Append(ImportedStaticMeshes);
	SetMetaData(Assets);

	return ImportedStaticMeshes;
}

UGeometryCache* FAbcImporter::ImportAsGeometryCache(UObject* InParent, EObjectFlags Flags)
{
	// Create a GeometryCache instance 
	UGeometryCache* GeometryCache = CreateObjectInstance<UGeometryCache>(InParent, InParent != GetTransientPackage() ? FPaths::GetBaseFilename(InParent->GetName()) : (FPaths::GetBaseFilename(AbcFile->GetFilePath()) + "_" + FGuid::NewGuid().ToString()), Flags);

	// Only import data if a valid object was created
	if (GeometryCache)
	{
		TArray<TUniquePtr<FComponentReregisterContext>> ReregisterContexts;
		for (TObjectIterator<UGeometryCacheComponent> CacheIt; CacheIt; ++CacheIt)
		{
			if (CacheIt->GetGeometryCache() == GeometryCache)
			{	
				ReregisterContexts.Add(MakeUnique<FComponentReregisterContext>(*CacheIt));
			}
		}
		
		// In case this is a reimport operation
		GeometryCache->ClearForReimporting();

		// Load the default material for later usage
		UMaterial* DefaultMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
		check(DefaultMaterial);
		uint32 MaterialOffset = 0;

		// Add tracks
		const int32 NumPolyMeshes = AbcFile->GetNumPolyMeshes();
		if (NumPolyMeshes != 0)
		{
			TArray<UGeometryCacheTrackStreamable*> Tracks;

			TArray<FAbcPolyMesh*> ImportPolyMeshes;
			TArray<int32> MaterialOffsets;

			const bool bContainsHeterogeneousMeshes = AbcFile->ContainsHeterogeneousMeshes();
			if (ImportSettings->GeometryCacheSettings.bApplyConstantTopologyOptimizations && bContainsHeterogeneousMeshes)
			{
				TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create(EMessageSeverity::Warning, LOCTEXT("HeterogeneousMeshesAndForceSingle", "Unable to enforce constant topology optimizations as the imported tracks contain topology varying data."));
				FAbcImportLogger::AddImportMessage(Message);
			}

			if (ImportSettings->GeometryCacheSettings.bFlattenTracks)
			{
				//UGeometryCacheCodecRaw* Codec = NewObject<UGeometryCacheCodecRaw>(GeometryCache, FName(*FString(TEXT("Flattened_Codec"))), RF_Public);
				UGeometryCacheCodecV1* Codec = NewObject<UGeometryCacheCodecV1>(GeometryCache, FName(*FString(TEXT("Flattened_Codec"))), RF_Public);
				Codec->InitializeEncoder(ImportSettings->GeometryCacheSettings.CompressedPositionPrecision, ImportSettings->GeometryCacheSettings.CompressedTextureCoordinatesNumberOfBits);
				UGeometryCacheTrackStreamable* Track = NewObject<UGeometryCacheTrackStreamable>(GeometryCache, FName(*FString(TEXT("Flattened_Track"))), RF_Public);

				const bool bCalculateMotionVectors = (ImportSettings->GeometryCacheSettings.MotionVectors == EAbcGeometryCacheMotionVectorsImport::CalculateMotionVectorsDuringImport);
				Track->BeginCoding(Codec, ImportSettings->GeometryCacheSettings.bApplyConstantTopologyOptimizations && !bContainsHeterogeneousMeshes, bCalculateMotionVectors, ImportSettings->GeometryCacheSettings.bOptimizeIndexBuffers);
				Tracks.Add(Track);
				
				FScopedSlowTask SlowTask((ImportSettings->SamplingSettings.FrameEnd + 1) - ImportSettings->SamplingSettings.FrameStart, FText::FromString(FString(TEXT("Importing Frames"))));
				SlowTask.MakeDialog(true);

				const TArray<FString>& UniqueFaceSetNames = AbcFile->GetUniqueFaceSetNames();
				const TArray<FAbcPolyMesh*>& PolyMeshes = AbcFile->GetPolyMeshes();
				
				const int32 NumTracks = Tracks.Num();
				int32 PreviousNumVertices = 0;
				TFunction<void(int32, FAbcFile*)> Callback = [this, &Tracks, &SlowTask, &UniqueFaceSetNames, &PolyMeshes, &PreviousNumVertices](int32 FrameIndex, const FAbcFile* InAbcFile)
				{
					const bool bUseVelocitiesAsMotionVectors = (ImportSettings->GeometryCacheSettings.MotionVectors == EAbcGeometryCacheMotionVectorsImport::ImportAbcVelocitiesAsMotionVectors);
					FGeometryCacheMeshData MeshData;
					bool bConstantTopology = true;
					const bool bStoreImportedVertexNumbers = ImportSettings->GeometryCacheSettings.bStoreImportedVertexNumbers;

					AbcImporterUtilities::MergePolyMeshesToMeshData(FrameIndex, ImportSettings->SamplingSettings.FrameStart, AbcFile->GetSecondsPerFrame(), bUseVelocitiesAsMotionVectors,
						PolyMeshes, UniqueFaceSetNames,
						MeshData, PreviousNumVertices, bConstantTopology, bStoreImportedVertexNumbers);
					
					Tracks[0]->AddMeshSample(MeshData, PolyMeshes[0]->GetTimeForFrameIndex(FrameIndex) - InAbcFile->GetImportTimeOffset(), bConstantTopology);
					
					if (IsInGameThread())
					{
						SlowTask.EnterProgressFrame(1.0f);
					}					
				};

				AbcFile->ProcessFrames(Callback, EFrameReadFlags::ApplyMatrix);

				// Now add materials for all the face set names
				for (const FString& FaceSetName : UniqueFaceSetNames)
				{
					UMaterialInterface* Material = AbcImporterUtilities::RetrieveMaterial(*AbcFile, FaceSetName, InParent, Flags);
					GeometryCache->Materials.Add((Material != nullptr) ? Material : DefaultMaterial);		

					if (Material != UMaterial::GetDefaultMaterial(MD_Surface))
					{
						Material->PostEditChange();
					}
				}
			}
			else
			{
				const TArray<FAbcPolyMesh*>& PolyMeshes = AbcFile->GetPolyMeshes();
				for (FAbcPolyMesh* PolyMesh : PolyMeshes)
				{
					if (PolyMesh->bShouldImport)
					{
						FName BaseName = FName(*(PolyMesh->GetName()));
						//UGeometryCacheCodecRaw* Codec = NewObject<UGeometryCacheCodecRaw>(GeometryCache, FName(*(PolyMesh->GetName() + FString(TEXT("_Codec")))), RF_Public);
						FName CodecName = MakeUniqueObjectName(GeometryCache, UGeometryCacheCodecV1::StaticClass(), FName(BaseName.ToString() + FString(TEXT("_Codec"))));
						UGeometryCacheCodecV1* Codec = NewObject<UGeometryCacheCodecV1>(GeometryCache, CodecName, RF_Public);
						Codec->InitializeEncoder(ImportSettings->GeometryCacheSettings.CompressedPositionPrecision, ImportSettings->GeometryCacheSettings.CompressedTextureCoordinatesNumberOfBits);

						FName TrackName = MakeUniqueObjectName(GeometryCache, UGeometryCacheTrackStreamable::StaticClass(), BaseName);
						UGeometryCacheTrackStreamable* Track = NewObject<UGeometryCacheTrackStreamable>(GeometryCache, TrackName, RF_Public);

						const bool bCalculateMotionVectors = (ImportSettings->GeometryCacheSettings.MotionVectors == EAbcGeometryCacheMotionVectorsImport::CalculateMotionVectorsDuringImport);
						Track->BeginCoding(Codec, ImportSettings->GeometryCacheSettings.bApplyConstantTopologyOptimizations && !bContainsHeterogeneousMeshes, bCalculateMotionVectors, ImportSettings->GeometryCacheSettings.bOptimizeIndexBuffers);

						ImportPolyMeshes.Add(PolyMesh);
						Tracks.Add(Track);
						MaterialOffsets.Add(MaterialOffset);

						// Add materials for this Mesh Object
						const uint32 NumMaterials = (PolyMesh->FaceSetNames.Num() > 0) ? PolyMesh->FaceSetNames.Num() : 1;
						for (uint32 MaterialIndex = 0; MaterialIndex < NumMaterials; ++MaterialIndex)
						{
							UMaterialInterface* Material = nullptr;
							if (PolyMesh->FaceSetNames.IsValidIndex(MaterialIndex))
							{
								Material = AbcImporterUtilities::RetrieveMaterial(*AbcFile, PolyMesh->FaceSetNames[MaterialIndex], InParent, Flags);
								if (Material != UMaterial::GetDefaultMaterial(MD_Surface))
								{
									Material->PostEditChange();
								}
							}

							GeometryCache->Materials.Add((Material != nullptr) ? Material : DefaultMaterial);
						}

						MaterialOffset += NumMaterials;
					}
				}

				const int32 NumTracks = Tracks.Num();
				TFunction<void(int32, FAbcFile*)> Callback = [this, NumTracks, &ImportPolyMeshes, &Tracks, &MaterialOffsets](int32 FrameIndex, const FAbcFile* InAbcFile)
				{
					for (int32 TrackIndex = 0; TrackIndex < NumTracks; ++TrackIndex)
					{
						const FAbcPolyMesh* PolyMesh = ImportPolyMeshes[TrackIndex];
						if (PolyMesh->bShouldImport)
						{
							UGeometryCacheTrackStreamable* Track = Tracks[TrackIndex];

							// Generate the mesh data for this sample
							const bool bVisible = PolyMesh->GetVisibility(FrameIndex);
							const float FrameTime = PolyMesh->GetTimeForFrameIndex(FrameIndex) - InAbcFile->GetImportTimeOffset();
							if (bVisible)
							{
								const bool bUseVelocitiesAsMotionVectors = ( ImportSettings->GeometryCacheSettings.MotionVectors == EAbcGeometryCacheMotionVectorsImport::ImportAbcVelocitiesAsMotionVectors );
								const bool bStoreImportedVertexData = ImportSettings->GeometryCacheSettings.bStoreImportedVertexNumbers;
								const FAbcMeshSample* Sample = PolyMesh->GetSample(FrameIndex);
								FGeometryCacheMeshData MeshData;
								AbcImporterUtilities::GeometryCacheDataForMeshSample(MeshData, Sample, MaterialOffsets[TrackIndex], AbcFile->GetSecondsPerFrame(), bUseVelocitiesAsMotionVectors, bStoreImportedVertexData);
								Track->AddMeshSample(MeshData, FrameTime, PolyMesh->bConstantTopology);
							}

							Track->AddVisibilitySample(bVisible, FrameTime);
						}
					}
				};

				AbcFile->ProcessFrames(Callback, EFrameReadFlags::ApplyMatrix);
			}

			TArray<FMatrix> Mats;
			Mats.Add(FMatrix::Identity);
			Mats.Add(FMatrix::Identity);

			for (UGeometryCacheTrackStreamable* Track : Tracks)
			{
				TArray<float> MatTimes;
				MatTimes.Add(0.0f);
				MatTimes.Add(AbcFile->GetImportLength() + AbcFile->GetImportTimeOffset());
				Track->SetMatrixSamples(Mats, MatTimes);

				// Some tracks might not have any mesh samples because they are invisible (can happen with bFlattenTracks=false), so skip them
				if (Track->EndCoding())
				{
					GeometryCache->AddTrack(Track);
				}
			}
		}

		// For alembic, for now, we define the duration of the tracks as the duration of the longer track in the whole file so all tracks loop in union
		float MaxDuration = 0.0f;
		for (auto Track : GeometryCache->Tracks)
		{
			MaxDuration = FMath::Max(MaxDuration, Track->GetDuration());
		}
		for (auto Track : GeometryCache->Tracks)
		{
			Track->SetDuration(MaxDuration);
		}
		// Also store the number of frames in the cache
		GeometryCache->SetFrameStartEnd(ImportSettings->SamplingSettings.FrameStart, ImportSettings->SamplingSettings.FrameEnd);
		
		// Update all geometry cache components, TODO move render-data from component to GeometryCache and allow for DDC population
		for (TObjectIterator<UGeometryCacheComponent> CacheIt; CacheIt; ++CacheIt)
		{
			CacheIt->OnObjectReimported(GeometryCache);
		}

		SetMetaData({GeometryCache});
	}
	
	return GeometryCache;
}

TArray<UObject*> FAbcImporter::ImportAsSkeletalMesh(UObject* InParent, EObjectFlags Flags)
{
	// First compress the animation data
	const bool bRunComparison = false;
	const bool bCompressionResult = CompressAnimationDataUsingPCA(ImportSettings->CompressionSettings, bRunComparison);

	TArray<UObject*> GeneratedObjects;

	if (!bCompressionResult)
	{
		return GeneratedObjects;
	}

	// Create a Skeletal mesh instance 
	
	const FString& ObjectName = InParent != GetTransientPackage() ? FPaths::GetBaseFilename(InParent->GetName()) : (FPaths::GetBaseFilename(AbcFile->GetFilePath()) + "_" + FGuid::NewGuid().ToString());
	const FString SanitizedObjectName = ObjectTools::SanitizeObjectName(ObjectName);

	USkeletalMesh* ExistingSkeletalMesh = FindObject<USkeletalMesh>(InParent, *SanitizedObjectName);	
	FSkinnedMeshComponentRecreateRenderStateContext* RecreateExistingRenderStateContext = ExistingSkeletalMesh ? new FSkinnedMeshComponentRecreateRenderStateContext(ExistingSkeletalMesh, false) : nullptr;
	
	USkeletalMesh* SkeletalMesh = CreateObjectInstance<USkeletalMesh>(InParent, ObjectName, Flags);

	// Only import data if a valid object was created
	if (SkeletalMesh)
	{
		// Touch pre edit change
		SkeletalMesh->PreEditChange(NULL);

		// Retrieve the imported resource structure and allocate a new LOD model
		FSkeletalMeshModel* ImportedModel = SkeletalMesh->GetImportedModel();
		check(ImportedModel->LODModels.Num() == 0);
		ImportedModel->LODModels.Empty();
		ImportedModel->EmptyOriginalReductionSourceMeshData();
		ImportedModel->LODModels.Add(new FSkeletalMeshLODModel());
		SkeletalMesh->ResetLODInfo();

		FSkeletalMeshLODInfo& NewLODInfo = SkeletalMesh->AddLODInfo();
		NewLODInfo.ReductionSettings.NumOfTrianglesPercentage = 1.0f;
		NewLODInfo.ReductionSettings.NumOfVertPercentage = 1.0f;
		NewLODInfo.ReductionSettings.MaxDeviationPercentage = 0.0f;
		FSkeletalMeshLODModel& LODModel = ImportedModel->LODModels[0];

		const FMeshBoneInfo BoneInfo(FName(TEXT("RootBone"), FNAME_Add), TEXT("RootBone_Export"), INDEX_NONE);
		const FTransform BoneTransform;
		{
			FReferenceSkeletonModifier RefSkelModifier(SkeletalMesh->GetRefSkeleton(), SkeletalMesh->GetSkeleton());
			RefSkelModifier.Add(BoneInfo, BoneTransform);
		}


		FAbcMeshSample* MergedMeshSample = new FAbcMeshSample();
		for (const FCompressedAbcData& Data : CompressedMeshData)
		{
			AbcImporterUtilities::AppendMeshSample(MergedMeshSample, Data.AverageSample);
		}
		
		// Forced to 1
		LODModel.NumTexCoords = MergedMeshSample->NumUVSets;
		SkeletalMesh->SetHasVertexColors(true);
		SkeletalMesh->SetVertexColorGuid(FGuid::NewGuid());

		/* Bounding box according to animation */
		SkeletalMesh->SetImportedBounds(AbcFile->GetArchiveBounds().GetBox());

		bool bBuildSuccess = false;
		TArray<int32> MorphTargetVertexRemapping;
		TArray<int32> UsedVertexIndicesForMorphs;
		MergedMeshSample->TangentX.Empty();
		MergedMeshSample->TangentY.Empty();
		bBuildSuccess = BuildSkeletalMesh(LODModel, SkeletalMesh->GetRefSkeleton(), MergedMeshSample, MorphTargetVertexRemapping, UsedVertexIndicesForMorphs);

		if (!bBuildSuccess)
		{
			SkeletalMesh->MarkAsGarbage();
			return GeneratedObjects;
		}

		// Create the skeleton object
		FString SkeletonName = FString::Printf(TEXT("%s_Skeleton"), *SkeletalMesh->GetName());
		USkeleton* Skeleton = CreateObjectInstance<USkeleton>(InParent, SkeletonName, Flags);

		// Merge bones to the selected skeleton
		check(Skeleton->MergeAllBonesToBoneTree(SkeletalMesh));
		Skeleton->MarkPackageDirty();
		if (SkeletalMesh->GetSkeleton() != Skeleton)
		{
			SkeletalMesh->SetSkeleton(Skeleton);
			SkeletalMesh->MarkPackageDirty();
		}

		// Create animation sequence for the skeleton
		UAnimSequence* Sequence = CreateObjectInstance<UAnimSequence>(InParent, FString::Printf(TEXT("%s_Animation"), *SkeletalMesh->GetName()), Flags);
		Sequence->SetSkeleton(Skeleton);

		int32 ObjectIndex = 0;
		uint32 TriangleOffset = 0;
		uint32 WedgeOffset = 0;
		uint32 VertexOffset = 0;

		IAnimationDataController& Controller = Sequence->GetController();

		Controller.OpenBracket(LOCTEXT("ImportAsSkeletalMesh", "Importing Alembic Animation"));

		Controller.SetPlayLength(AbcFile->GetImportLength());
		Controller.SetFrameRate( FFrameRate(AbcFile->GetFramerate(), 1));

		Sequence->ImportFileFramerate = AbcFile->GetFramerate();
		Sequence->ImportResampleFramerate = 1.0f / (float)AbcFile->GetFramerate();

		{
#if WITH_EDITOR
			// When ScopedPostEditChange goes out of scope, it will call SkeletalMesh->PostEditChange()
			// while preventing any call to that within the scope
			FScopedSkeletalMeshPostEditChange ScopedPostEditChange(SkeletalMesh);
#endif

			for (FCompressedAbcData& CompressedData : CompressedMeshData)
			{
				FAbcMeshSample* AverageSample = CompressedData.AverageSample;

				if (CompressedData.BaseSamples.Num() > 0)
				{
					const int32 NumBases = CompressedData.BaseSamples.Num();
					int32 NumUsedBases = 0;

					const int32 NumIndices = CompressedData.AverageSample->Indices.Num();

					for (int32 BaseIndex = 0; BaseIndex < NumBases; ++BaseIndex)
					{
						FAbcMeshSample* BaseSample = CompressedData.BaseSamples[BaseIndex];

						// Create new morph target with name based on object and base index
						UMorphTarget* MorphTarget = NewObject<UMorphTarget>(SkeletalMesh, FName(*FString::Printf(TEXT("Base_%i_%i"), BaseIndex, ObjectIndex)));

						// Setup morph target vertices directly
						TArray<FMorphTargetDelta> MorphDeltas;
						GenerateMorphTargetVertices(BaseSample, MorphDeltas, AverageSample, WedgeOffset, MorphTargetVertexRemapping, UsedVertexIndicesForMorphs, VertexOffset, WedgeOffset);

						const bool bCompareNormals = true;
						MorphTarget->PopulateDeltas(MorphDeltas, 0, LODModel.Sections, bCompareNormals);

						const float PercentageOfVerticesInfluences = ((float)MorphTarget->GetMorphLODModels()[0].Vertices.Num() / (float)NumIndices) * 100.0f;
						if (PercentageOfVerticesInfluences > ImportSettings->CompressionSettings.MinimumNumberOfVertexInfluencePercentage)
						{
							SkeletalMesh->RegisterMorphTarget(MorphTarget);
							MorphTarget->MarkPackageDirty();

							// Set up curves
							const TArray<float>& CurveValues = CompressedData.CurveValues[BaseIndex];
							const TArray<float>& TimeValues = CompressedData.TimeValues[BaseIndex];
							// Morph target stuffies
							FString CurveName = FString::Printf(TEXT("Base_%i_%i"), BaseIndex, ObjectIndex);
							FName ConstCurveName = *CurveName;

							// Sets up the morph target curves with the sample values and time keys
							SetupMorphTargetCurves(Skeleton, ConstCurveName, Sequence, CurveValues, TimeValues, Controller);
						}
						else
						{
							MorphTarget->MarkAsGarbage();
						}
					}
				}

				WedgeOffset += CompressedData.AverageSample->Indices.Num();
				VertexOffset += CompressedData.AverageSample->Vertices.Num();

				const uint32 NumMaterials = CompressedData.MaterialNames.Num();
				for (uint32 MaterialIndex = 0; MaterialIndex < NumMaterials; ++MaterialIndex)
				{
					const FString& MaterialName = CompressedData.MaterialNames[MaterialIndex];
					UMaterialInterface* Material = AbcImporterUtilities::RetrieveMaterial(*AbcFile, MaterialName, InParent, Flags);
					SkeletalMesh->GetMaterials().Add(FSkeletalMaterial(Material, true));
					if (Material != UMaterial::GetDefaultMaterial(MD_Surface))
					{
						Material->PostEditChange();
					}
				}

				++ObjectIndex;
			}

			// Add a track for translating the RootBone by the samples centers
			// Each mesh has the same samples centers so use the first one
			if (SamplesOffsets.IsSet() && CompressedMeshData.Num() > 0 && CompressedMeshData[0].CurveValues.Num() > 0)
			{
				const int32 NumSamples = CompressedMeshData[0].CurveValues[0].Num(); // We might have less bases than we have samples, so use the number of curve values here

				FRawAnimSequenceTrack RootBoneTrack;
				RootBoneTrack.PosKeys.Reserve(NumSamples);
				RootBoneTrack.RotKeys.Reserve(NumSamples);
				RootBoneTrack.ScaleKeys.Reserve(NumSamples);

				for (int32 SampleIndex = 0; SampleIndex < NumSamples; ++SampleIndex)
				{
					const FVector SampleOffset = SamplesOffsets.GetValue()[SampleIndex];
					RootBoneTrack.PosKeys.Add((FVector3f)SampleOffset);

					RootBoneTrack.RotKeys.Add(FQuat4f::Identity);
					RootBoneTrack.ScaleKeys.Add(FVector3f::OneVector);
				}

				const FReferenceSkeleton& RefSkeleton = SkeletalMesh->GetRefSkeleton();
				const TArray<FMeshBoneInfo>& BonesInfo = RefSkeleton.GetRawRefBoneInfo();
				
				Controller.AddBoneTrack(BonesInfo[0].Name);
				Controller.SetBoneTrackKeys(BonesInfo[0].Name, RootBoneTrack.PosKeys, RootBoneTrack.RotKeys, RootBoneTrack.ScaleKeys);
			}

			// Set recompute tangent flag on skeletal mesh sections
			for (FSkelMeshSection& Section : LODModel.Sections)
			{
				Section.bRecomputeTangent = true;
			}

			SkeletalMesh->CalculateInvRefMatrices();
		}

		SkeletalMesh->MarkPackageDirty();

		Controller.UpdateCurveNamesFromSkeleton(Skeleton, ERawCurveTrackTypes::RCT_Float);
		Controller.NotifyPopulated();

		Controller.CloseBracket();

		Sequence->PostEditChange();
		Sequence->SetPreviewMesh(SkeletalMesh);
		Sequence->MarkPackageDirty();

		Skeleton->SetPreviewMesh(SkeletalMesh);
		Skeleton->PostEditChange();

		GeneratedObjects.Add(SkeletalMesh);
		GeneratedObjects.Add(Skeleton);
		GeneratedObjects.Add(Sequence);

		UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
		AssetEditorSubsystem->CloseAllEditorsForAsset(Skeleton);
		AssetEditorSubsystem->CloseAllEditorsForAsset(SkeletalMesh);
		AssetEditorSubsystem->CloseAllEditorsForAsset(Sequence);
	}

	if (RecreateExistingRenderStateContext)
	{
		delete RecreateExistingRenderStateContext;
	}

	SetMetaData(GeneratedObjects);

	return GeneratedObjects;
}

void FAbcImporter::SetupMorphTargetCurves(USkeleton* Skeleton, FName ConstCurveName, UAnimSequence* Sequence, const TArray<float> &CurveValues, const TArray<float>& TimeValues, IAnimationDataController& Controller)
{
	FSmartName NewName;
	Skeleton->AddSmartNameAndModify(USkeleton::AnimCurveMappingName, ConstCurveName, NewName);

	FAnimationCurveIdentifier CurveId(NewName, ERawCurveTrackTypes::RCT_Float);
	Controller.AddCurve(CurveId);

	const FFloatCurve* NewCurve = Sequence->GetDataModel()->FindFloatCurve(CurveId);
	ensure(NewCurve);

	FRichCurve RichCurve;
	for (int32 KeyIndex = 0; KeyIndex < CurveValues.Num(); ++KeyIndex)
	{
		const float CurveValue = CurveValues[KeyIndex];
		const float TimeValue = TimeValues[KeyIndex];

		FKeyHandle NewKeyHandle = RichCurve.AddKey(TimeValue, CurveValue, false);

		ERichCurveInterpMode NewInterpMode = RCIM_Linear;
		ERichCurveTangentMode NewTangentMode = RCTM_Auto;
		ERichCurveTangentWeightMode NewTangentWeightMode = RCTWM_WeightedNone;

		RichCurve.SetKeyInterpMode(NewKeyHandle, NewInterpMode);
		RichCurve.SetKeyTangentMode(NewKeyHandle, NewTangentMode);
		RichCurve.SetKeyTangentWeightMode(NewKeyHandle, NewTangentWeightMode);
	}

	Controller.SetCurveKeys(CurveId, RichCurve.GetConstRefOfKeys());
}

void FAbcImporter::SetMetaData(const TArray<UObject*>& Objects)
{
	const TArray<FAbcFile::FMetaData> ArchiveMetaData = AbcFile->GetArchiveMetaData();
	for (UObject* Object : Objects)
	{
		if (Object)
		{
			for (const FAbcFile::FMetaData& MetaData : ArchiveMetaData)
			{
				Object->GetOutermost()->GetMetaData()->SetValue(Object, *MetaData.Key, *MetaData.Value);
			}
		}
	}
}

const bool FAbcImporter::CompressAnimationDataUsingPCA(const FAbcCompressionSettings& InCompressionSettings, const bool bRunComparison /*= false*/)
{
	const TArray<FAbcPolyMesh*>& PolyMeshes = AbcFile->GetPolyMeshes();
	
	// Split up poly mesh objects into constant and animated objects to process
	TArray<FAbcPolyMesh*> PolyMeshesToCompress;
	TArray<FAbcPolyMesh*> ConstantPolyMeshObjects;
	for (FAbcPolyMesh* PolyMesh : PolyMeshes)
	{
		if (PolyMesh->bShouldImport && PolyMesh->bConstantTopology)
		{
			if (PolyMesh->IsConstant() && PolyMesh->bConstantTransformation)
			{
				ConstantPolyMeshObjects.Add(PolyMesh);
			}
			else if (!PolyMesh->IsConstant() || (InCompressionSettings.bBakeMatrixAnimation && !PolyMesh->bConstantTransformation))
			{
				PolyMeshesToCompress.Add(PolyMesh);
			}
		}
	}

	const bool bEnableSamplesOffsets = (ConstantPolyMeshObjects.Num() == 0); // We can't offset constant meshes since they don't have morph targets

	bool bResult = true;
	const int32 NumPolyMeshesToCompress = PolyMeshesToCompress.Num();
	if (NumPolyMeshesToCompress)
	{
		if (InCompressionSettings.bMergeMeshes)
		{
			// Merged path
			TArray<FVector3f> AverageVertexData;
			TArray<FVector3f> AverageNormalData;

			float MinTime = FLT_MAX;
			float MaxTime = -FLT_MAX;
			int32 NumSamples = 0;

			TArray<uint32> ObjectVertexOffsets;
			TArray<uint32> ObjectIndexOffsets;
			TFunction<void(int32, FAbcFile*)> MergedMeshesFunc =
				[this, PolyMeshesToCompress, &MinTime, &MaxTime, &NumSamples, &ObjectVertexOffsets, &ObjectIndexOffsets, &AverageVertexData, &AverageNormalData, NumPolyMeshesToCompress]
				(int32 FrameIndex, FAbcFile* InFile)
				{
					for (int32 MeshIndex = 0; MeshIndex < NumPolyMeshesToCompress; ++MeshIndex)
					{
						FAbcPolyMesh* PolyMesh = PolyMeshesToCompress[MeshIndex];

						MinTime = FMath::Min(MinTime, (float)PolyMesh->GetTimeForFrameIndex(FrameIndex) - AbcFile->GetImportTimeOffset());
						MaxTime = FMath::Max(MaxTime, (float)PolyMesh->GetTimeForFrameIndex(FrameIndex) - AbcFile->GetImportTimeOffset());

						if (ObjectVertexOffsets.Num() != NumPolyMeshesToCompress)
						{
							ObjectVertexOffsets.Add(AverageVertexData.Num());
							ObjectIndexOffsets.Add(AverageNormalData.Num());
							AverageVertexData.Append(PolyMesh->GetSample(FrameIndex)->Vertices);
							AverageNormalData.Append(PolyMesh->GetSample(FrameIndex)->Normals);
						}
						else
						{
							for (int32 VertexIndex = 0; VertexIndex < PolyMesh->GetSample(FrameIndex)->Vertices.Num(); ++VertexIndex)
							{
								AverageVertexData[VertexIndex + ObjectVertexOffsets[MeshIndex]] += PolyMesh->GetSample(FrameIndex)->Vertices[VertexIndex];
							}

							for (int32 Index = 0; Index < PolyMesh->GetSample(FrameIndex)->Indices.Num(); ++Index)
							{
								AverageNormalData[Index + ObjectIndexOffsets[MeshIndex]] += PolyMesh->GetSample(FrameIndex)->Normals[Index];
							}
						}
					}

					++NumSamples;
				};

			EFrameReadFlags Flags = EFrameReadFlags::PositionAndNormalOnly;
			if (ImportSettings->CompressionSettings.bBakeMatrixAnimation)
			{
				Flags |= EFrameReadFlags::ApplyMatrix;
			}

			{
				// Check the first frame to see if the Alembic can be imported as a skeletal mesh due to memory constraints
				AbcFile->ReadFrame(AbcFile->GetStartFrameIndex(), Flags, 0);
				MergedMeshesFunc(AbcFile->GetStartFrameIndex(), AbcFile);
				AbcFile->CleanupFrameData(0);

				const int32 NumFrames = AbcFile->GetEndFrameIndex() - AbcFile->GetStartFrameIndex() + 1;
				const uint64 NumMatrixElements = uint64(AverageVertexData.Num()) * 3 * NumFrames;
				if (!IntFitsIn<int32>(NumMatrixElements))
				{
					UE_LOG(LogAbcImporter, Error, TEXT("Vertex matrix has too many elements (%llu) because the mesh has too many vertices (%d) and/or the animation has too many frames (%d). Try importing as GeometryCache instead."),
						NumMatrixElements, AverageVertexData.Num(), NumFrames);
					return false;
				}

				const uint64 NumNormalsMatrixElements = uint64(AverageNormalData.Num()) * 3 * NumFrames;
				if (!IntFitsIn<int32>(NumNormalsMatrixElements))
				{
					UE_LOG(LogAbcImporter, Error, TEXT("Normal matrix has too many elements (%llu) because the mesh has too many vertices (%d) and/or the animation has too many frames (%d). Try importing as GeometryCache instead."),
						NumNormalsMatrixElements, AverageNormalData.Num(), NumFrames);
					return false;
				}

				AverageVertexData.Reset();
				AverageNormalData.Reset();
				ObjectVertexOffsets.Reset();
				ObjectIndexOffsets.Reset();

				MinTime = FLT_MAX;
				MaxTime = -FLT_MAX;
				NumSamples = 0;
			}

			AbcFile->ProcessFrames(MergedMeshesFunc, Flags);

			// Average out vertex data
			FBox AverageBoundingBox(ForceInit);
			const float Multiplier = 1.0f / FMath::Max(NumSamples, 1);
			for (FVector3f& Vertex : AverageVertexData)
			{
				Vertex *= Multiplier;
				AverageBoundingBox += (FVector)Vertex;
			}
			const FVector AverageSampleCenter = AverageBoundingBox.GetCenter();

			for (FVector3f& Normal : AverageNormalData)
			{
				Normal = Normal.GetSafeNormal();
			}

			// Allocate compressed mesh data object
			CompressedMeshData.AddDefaulted();
			FCompressedAbcData& CompressedData = CompressedMeshData.Last();

			FAbcMeshSample MergedZeroFrameSample;
			for (FAbcPolyMesh* PolyMesh : PolyMeshesToCompress)
			{
				AbcImporterUtilities::AppendMeshSample(&MergedZeroFrameSample, PolyMesh->GetTransformedFirstSample());

				// QQ FUNCTIONALIZE
				// Add material names from this mesh object
				if (PolyMesh->FaceSetNames.Num() > 0)
				{
					CompressedData.MaterialNames.Append(PolyMesh->FaceSetNames);
				}
				else
				{
					static const FString DefaultName("NoFaceSetName");
					CompressedData.MaterialNames.Add(DefaultName);
				}
			}

			const uint32 NumVertices = AverageVertexData.Num();
			const uint32 NumMatrixRows = NumVertices * 3;
			const uint32 NumIndices = AverageNormalData.Num();
			const uint32 NumNormalsMatrixRows = NumIndices * 3;

			TArray<float> OriginalMatrix;
			OriginalMatrix.AddZeroed(NumMatrixRows * NumSamples);

			TArray<float> OriginalNormalsMatrix;
			OriginalNormalsMatrix.AddZeroed(NumNormalsMatrixRows * NumSamples);

			if (bEnableSamplesOffsets)
			{
				SamplesOffsets.Emplace();
				SamplesOffsets.GetValue().AddZeroed(NumSamples);
			}

			uint32 GenerateMatrixSampleIndex = 0;
			TFunction<void(int32, FAbcFile*)> GenerateMatrixFunc =
				[this, PolyMeshesToCompress, NumPolyMeshesToCompress, &OriginalMatrix, &OriginalNormalsMatrix, &AverageVertexData, &AverageNormalData, &NumSamples,
					&ObjectVertexOffsets, &ObjectIndexOffsets, &GenerateMatrixSampleIndex, &AverageSampleCenter]
				(int32 FrameIndex, FAbcFile* InFile)
				{
					FVector SampleOffset = FVector::ZeroVector;
					if (SamplesOffsets.IsSet())
					{
						FBox BoundingBox(ForceInit);

						// For each object generate the delta frame data for the PCA compression
						for (FAbcPolyMesh* PolyMesh : PolyMeshesToCompress)
						{
							const TArray<FVector3f>& Vertices = PolyMesh->GetSample(FrameIndex)->Vertices;

							for ( const FVector3f& Vertex : Vertices )
							{
								BoundingBox += (FVector)Vertex;
							}
						}

						SampleOffset = BoundingBox.GetCenter() - AverageSampleCenter;
						SamplesOffsets.GetValue()[GenerateMatrixSampleIndex] = SampleOffset;
					}

					// For each object generate the delta frame data for the PCA compression
					for (int32 MeshIndex = 0; MeshIndex < NumPolyMeshesToCompress; ++MeshIndex)
					{
						FAbcPolyMesh* PolyMesh = PolyMeshesToCompress[MeshIndex];
						const TArray<FVector3f>& Vertices = PolyMesh->GetSample(FrameIndex)->Vertices;
						const TArray<FVector3f>& Normals = PolyMesh->GetSample(FrameIndex)->Normals;

						AbcImporterUtilities::GenerateDeltaFrameDataMatrix(Vertices, Normals, AverageVertexData, AverageNormalData, GenerateMatrixSampleIndex,
							ObjectVertexOffsets[MeshIndex], ObjectIndexOffsets[MeshIndex], SampleOffset, OriginalMatrix, OriginalNormalsMatrix);
					}

					++GenerateMatrixSampleIndex;
				};

			AbcFile->ProcessFrames(GenerateMatrixFunc, Flags);

			// Perform compression
			TArray<float> OutU, OutV, OutNormalsU;
			TArrayView<float> BasesMatrix;
			TArrayView<float> NormalsBasesMatrix;
			uint32 NumUsedSingularValues = NumSamples;

			if (InCompressionSettings.BaseCalculationType != EBaseCalculationType::NoCompression)
			{
				const float PercentageOfTotalBases = (InCompressionSettings.BaseCalculationType == EBaseCalculationType::PercentageBased ? InCompressionSettings.PercentageOfTotalBases / 100.0f : 1.0f);
				const int32 NumberOfBases = InCompressionSettings.BaseCalculationType == EBaseCalculationType::FixedNumber ? InCompressionSettings.MaxNumberOfBases : 0;

				NumUsedSingularValues = PerformSVDCompression(OriginalMatrix, OriginalNormalsMatrix, NumSamples, PercentageOfTotalBases, NumberOfBases, OutU, OutNormalsU, OutV);
				BasesMatrix = OutU;
				NormalsBasesMatrix= OutNormalsU;
			}
			else
			{
				OutV.AddDefaulted(NumSamples * NumSamples);

				for (int32 SampleIndex = 0; SampleIndex < NumSamples; ++SampleIndex)
				{
					for (int32 CurveIndex = 0; CurveIndex < NumSamples; ++CurveIndex)
					{
						float Weight = 0.f;

						if ( SampleIndex == CurveIndex )
						{
							Weight = 1.f;
						}

						OutV[SampleIndex + (NumSamples * CurveIndex )] = Weight;
					}
				}

				BasesMatrix = OriginalMatrix;
				NormalsBasesMatrix = OriginalNormalsMatrix;
			}

			// Set up average frame 
			CompressedData.AverageSample = new FAbcMeshSample(MergedZeroFrameSample);
			FMemory::Memcpy(CompressedData.AverageSample->Vertices.GetData(), AverageVertexData.GetData(), AverageVertexData.GetTypeSize() * NumVertices);
			FMemory::Memcpy(CompressedData.AverageSample->Normals.GetData(), AverageNormalData.GetData(), AverageNormalData.GetTypeSize() * NumIndices);

			const float FrameStep = (MaxTime - MinTime) / (float)(NumSamples - 1);
			AbcImporterUtilities::GenerateCompressedMeshData(CompressedData, NumUsedSingularValues, NumSamples, BasesMatrix, NormalsBasesMatrix, OutV, FrameStep, FMath::Max(MinTime, 0.0f));

			if (bRunComparison)
			{
				CompareCompressionResult(OriginalMatrix, NumSamples, NumUsedSingularValues, BasesMatrix, OutV, THRESH_POINTS_ARE_SAME);
				CompareCompressionResult(OriginalNormalsMatrix, NumSamples, NumUsedSingularValues, NormalsBasesMatrix, OutV, THRESH_NORMALS_ARE_SAME);
			}
		}
		else
		{
			TArray<float> MinTimes;
			TArray<float> MaxTimes;
			TArray<TArray<FVector3f>> AverageVertexData;
			TArray<TArray<FVector3f>> AverageNormalData;

			MinTimes.AddZeroed(NumPolyMeshesToCompress);
			MaxTimes.AddZeroed(NumPolyMeshesToCompress);
			AverageVertexData.AddDefaulted(NumPolyMeshesToCompress);
			AverageNormalData.AddDefaulted(NumPolyMeshesToCompress);
			
			
			int32 NumSamples = 0;
			TFunction<void(int32, FAbcFile*)> IndividualMeshesFunc =
				[this, NumPolyMeshesToCompress, &PolyMeshesToCompress, &MinTimes, &MaxTimes, &NumSamples, &AverageVertexData, &AverageNormalData]
				(int32 FrameIndex, FAbcFile* InFile)
			{
				// Each individual object creates a compressed data object
				for (int32 MeshIndex = 0; MeshIndex < NumPolyMeshesToCompress; ++MeshIndex)
				{
					FAbcPolyMesh* PolyMesh = PolyMeshesToCompress[MeshIndex];
					TArray<FVector3f>& AverageVertices = AverageVertexData[MeshIndex];
					TArray<FVector3f>& AverageNormals = AverageNormalData[MeshIndex];

					if (AverageVertices.Num() == 0)
					{
						MinTimes[MeshIndex] = FLT_MAX;
						MaxTimes[MeshIndex] = -FLT_MAX;
						AverageVertices.Append(PolyMesh->GetSample(FrameIndex)->Vertices);
						AverageNormals.Append(PolyMesh->GetSample(FrameIndex)->Normals);
					}
					else
					{
						const TArray<FVector3f>& CurrentVertices = PolyMesh->GetSample(FrameIndex)->Vertices;
						for (int32 VertexIndex = 0; VertexIndex < AverageVertices.Num(); ++VertexIndex)
						{
							AverageVertices[VertexIndex] += CurrentVertices[VertexIndex];
						}

						for (int32 Index = 0; Index < PolyMesh->GetSample(FrameIndex)->Indices.Num(); ++Index)
						{
							AverageNormals[Index] += PolyMesh->GetSample(FrameIndex)->Normals[Index];
						}
					}

					MinTimes[MeshIndex] = FMath::Min(MinTimes[MeshIndex], (float)PolyMesh->GetTimeForFrameIndex(FrameIndex) - AbcFile->GetImportTimeOffset());
					MaxTimes[MeshIndex] = FMath::Max(MaxTimes[MeshIndex], (float)PolyMesh->GetTimeForFrameIndex(FrameIndex) - AbcFile->GetImportTimeOffset());
				}

				for (int32 MeshIndex = 0; MeshIndex < NumPolyMeshesToCompress; ++MeshIndex)
				{
					TArray<FVector3f>& AverageNormals = AverageNormalData[MeshIndex];

					for (FVector3f& AverageNormal : AverageNormals)
					{
						AverageNormal = AverageNormal.GetSafeNormal();
					}
				}

				++NumSamples;
			};

			EFrameReadFlags Flags = EFrameReadFlags::PositionAndNormalOnly;
			if (ImportSettings->CompressionSettings.bBakeMatrixAnimation)
			{
				Flags |= EFrameReadFlags::ApplyMatrix;
			}

			{
				// Check the first frame to see if the Alembic can be imported as a skeletal mesh due to memory constraints
				AbcFile->ReadFrame(AbcFile->GetStartFrameIndex(), Flags, 0);
				IndividualMeshesFunc(AbcFile->GetStartFrameIndex(), AbcFile);
				AbcFile->CleanupFrameData(0);

				const int32 NumFrames = AbcFile->GetEndFrameIndex() - AbcFile->GetStartFrameIndex() + 1;
				for (int32 MeshIndex = 0; MeshIndex < NumPolyMeshesToCompress; ++MeshIndex)
				{
					const uint64 NumMatrixElements = uint64(AverageVertexData[MeshIndex].Num()) * 3 * NumFrames;
					if (!IntFitsIn<int32>(NumMatrixElements))
					{
						UE_LOG(LogAbcImporter, Error, TEXT("Vertex matrix has too many elements (%llu) because the mesh has too many vertices (%d) and/or the animation has too many frames (%d). Try importing as GeometryCache instead."),
						NumMatrixElements, AverageVertexData[MeshIndex].Num(), NumFrames);
						return false;
					}

					const uint64 NumNormalsMatrixElements = uint64(AverageNormalData[MeshIndex].Num()) * 3 * NumFrames;
					if (!IntFitsIn<int32>(NumNormalsMatrixElements))
					{
						UE_LOG(LogAbcImporter, Error, TEXT("Normal matrix has too many elements (%llu) because the mesh has too many vertices (%d) and/or the animation has too many frames (%d). Try importing as GeometryCache instead."),
						NumNormalsMatrixElements, AverageNormalData[MeshIndex].Num(), NumFrames);
						return false;
					}
				}

				MinTimes.Reset();
				MaxTimes.Reset();
				AverageVertexData.Reset();
				AverageNormalData.Reset();

				MinTimes.AddZeroed(NumPolyMeshesToCompress);
				MaxTimes.AddZeroed(NumPolyMeshesToCompress);
				AverageVertexData.AddDefaulted(NumPolyMeshesToCompress);
				AverageNormalData.AddDefaulted(NumPolyMeshesToCompress);

				NumSamples = 0;
			}

			AbcFile->ProcessFrames(IndividualMeshesFunc, Flags);

			// Average out vertex data
			FBox AverageBoundingBox(ForceInit);
			const float Multiplier = 1.0f / FMath::Max(NumSamples, 1);
			for (TArray<FVector3f>& VertexData : AverageVertexData)
			{
				for (FVector3f& Vertex : VertexData)
				{
					Vertex *= Multiplier;
					AverageBoundingBox += (FVector)Vertex;
				}
			}
			const FVector AverageSampleCenter = AverageBoundingBox.GetCenter();

			TArray<TArray<float>> Matrices;
			TArray<TArray<float>> NormalsMatrices;
			for (int32 MeshIndex = 0; MeshIndex < NumPolyMeshesToCompress; ++MeshIndex)
			{
				Matrices.AddDefaulted();
				Matrices[MeshIndex].AddZeroed(AverageVertexData[MeshIndex].Num() * 3 * NumSamples);

				NormalsMatrices.AddDefaulted();
				NormalsMatrices[MeshIndex].AddZeroed(AverageNormalData[MeshIndex].Num() * 3 * NumSamples);
			}

			if (bEnableSamplesOffsets)
			{
				SamplesOffsets.Emplace();
				SamplesOffsets.GetValue().AddDefaulted(NumSamples);
			}

			if (bEnableSamplesOffsets)
			{
				SamplesOffsets.Emplace();
				SamplesOffsets.GetValue().AddDefaulted(NumSamples);
			}

			uint32 GenerateMatrixSampleIndex = 0;
			TFunction<void(int32, FAbcFile*)> GenerateMatrixFunc =
				[this, NumPolyMeshesToCompress, &Matrices, &NormalsMatrices, &GenerateMatrixSampleIndex, &PolyMeshesToCompress, &AverageVertexData, &AverageNormalData, &AverageSampleCenter]
				(int32 FrameIndex, FAbcFile* InFile)
				{
					// Compute on bounding box for the sample, which will include all the meshes
					FBox BoundingBox(ForceInit);

					for (int32 MeshIndex = 0; MeshIndex < NumPolyMeshesToCompress; ++MeshIndex)
					{
						FAbcPolyMesh* PolyMesh = PolyMeshesToCompress[MeshIndex];
						const uint32 NumMatrixRows = AverageVertexData[MeshIndex].Num() * 3;

						const TArray<FVector3f>& Vertices = PolyMesh->GetSample(FrameIndex)->Vertices;

						for ( const FVector3f& Vector : Vertices )
						{
							BoundingBox += (FVector)Vector;
						}
					}

					FVector SampleOffset = FVector::ZeroVector;
					if (SamplesOffsets.IsSet())
					{
						SampleOffset = BoundingBox.GetCenter() - AverageSampleCenter;
						SamplesOffsets.GetValue()[GenerateMatrixSampleIndex] = SampleOffset;
					}

					// For each object generate the delta frame data for the PCA compression
					for (int32 MeshIndex = 0; MeshIndex < NumPolyMeshesToCompress; ++MeshIndex)
					{
						FAbcPolyMesh* PolyMesh = PolyMeshesToCompress[MeshIndex];
						const uint32 NumMatrixRows = AverageVertexData[MeshIndex].Num() * 3;
						const TArray<FVector3f>& CurrentVertices = PolyMesh->GetSample(FrameIndex)->Vertices;
						const TArray<FVector3f>& CurrentNormals = PolyMesh->GetSample(FrameIndex)->Normals;

						const int32 AverageVertexOffset = 0;
						const int32 AverageIndexOffset = 0;

						AbcImporterUtilities::GenerateDeltaFrameDataMatrix(CurrentVertices, CurrentNormals, AverageVertexData[MeshIndex], AverageNormalData[MeshIndex],
							GenerateMatrixSampleIndex, AverageVertexOffset, AverageIndexOffset, SampleOffset, Matrices[MeshIndex], NormalsMatrices[MeshIndex]);
					}

					++GenerateMatrixSampleIndex;
				};

			AbcFile->ProcessFrames(GenerateMatrixFunc, Flags);

			for (int32 MeshIndex = 0; MeshIndex < NumPolyMeshesToCompress; ++MeshIndex)
			{
				// Perform compression
				TArray<float> OutU, OutV, OutNormalsU;
				TArrayView<float> BasesMatrix;
				TArrayView<float> NormalsBasesMatrix;

				const int32 NumVertices = AverageVertexData[MeshIndex].Num();
				const int32 NumIndices = AverageNormalData[MeshIndex].Num();
				const int32 NumMatrixRows = NumVertices * 3;
				uint32 NumUsedSingularValues = NumSamples;

				// Allocate compressed mesh data object
				CompressedMeshData.AddDefaulted();
				FCompressedAbcData& CompressedData = CompressedMeshData.Last();
				CompressedData.AverageSample = new FAbcMeshSample(*PolyMeshesToCompress[MeshIndex]->GetTransformedFirstSample());
				FMemory::Memcpy(CompressedData.AverageSample->Vertices.GetData(), AverageVertexData[MeshIndex].GetData(), AverageVertexData[MeshIndex].GetTypeSize() * NumVertices);
				FMemory::Memcpy(CompressedData.AverageSample->Normals.GetData(), AverageNormalData[MeshIndex].GetData(), AverageNormalData[MeshIndex].GetTypeSize() * NumIndices);

				if ( InCompressionSettings.BaseCalculationType != EBaseCalculationType::NoCompression )
				{
					const float PercentageBases = InCompressionSettings.BaseCalculationType == EBaseCalculationType::PercentageBased ? InCompressionSettings.PercentageOfTotalBases / 100.0f : 1.0f;
					const int32 NumBases = InCompressionSettings.BaseCalculationType == EBaseCalculationType::FixedNumber ? InCompressionSettings.MaxNumberOfBases : 0;

					NumUsedSingularValues = PerformSVDCompression(Matrices[MeshIndex], NormalsMatrices[MeshIndex], NumSamples, PercentageBases, NumBases, OutU, OutNormalsU, OutV);
					BasesMatrix = OutU;
					NormalsBasesMatrix = OutNormalsU;
				}
				else
				{
					OutV.AddDefaulted(NumSamples * NumSamples);

					for (int32 SampleIndex = 0; SampleIndex < NumSamples; ++SampleIndex)
					{
						for (int32 CurveIndex = 0; CurveIndex < NumSamples; ++CurveIndex)
						{
							float Weight = 0.f;

							if ( SampleIndex == CurveIndex )
							{
								Weight = 1.f;
							}

							OutV[SampleIndex + (NumSamples * CurveIndex )] = Weight;
						}
					}

					BasesMatrix = Matrices[MeshIndex];
					NormalsBasesMatrix = NormalsMatrices[MeshIndex];
				}

				const float FrameStep = (MaxTimes[MeshIndex] - MinTimes[MeshIndex]) / (float)(NumSamples - 1);
				AbcImporterUtilities::GenerateCompressedMeshData(CompressedData, NumUsedSingularValues, NumSamples, BasesMatrix, NormalsBasesMatrix, OutV, FrameStep, FMath::Max(MinTimes[MeshIndex], 0.0f));

				// QQ FUNCTIONALIZE
				// Add material names from this mesh object
				if (PolyMeshesToCompress[MeshIndex]->FaceSetNames.Num() > 0)
				{
					CompressedData.MaterialNames.Append(PolyMeshesToCompress[MeshIndex]->FaceSetNames);
				}
				else
				{
					static const FString DefaultName("NoFaceSetName");
					CompressedData.MaterialNames.Add(DefaultName);
				}

				if (bRunComparison)
				{
					CompareCompressionResult(Matrices[MeshIndex], NumSamples, NumUsedSingularValues, BasesMatrix, OutV, THRESH_POINTS_ARE_SAME);
					CompareCompressionResult(NormalsMatrices[MeshIndex], NumSamples, NumUsedSingularValues, NormalsBasesMatrix, OutV, THRESH_NORMALS_ARE_SAME);
				}
			}
		}
	}
	else
	{
		bResult = ConstantPolyMeshObjects.Num() > 0;
		TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create(bResult ? EMessageSeverity::Warning : EMessageSeverity::Error, LOCTEXT("NoMeshesToProcess", "Unable to compress animation data, no meshes (with constant topology) found with Vertex Animation and baked Matrix Animation is turned off."));
		FAbcImportLogger::AddImportMessage(Message);
	}

	// Process the constant meshes by only adding them as average samples (without any bases/morphtargets to add as well)
	for (const FAbcPolyMesh* ConstantPolyMesh : ConstantPolyMeshObjects)
	{
		// Allocate compressed mesh data object
		CompressedMeshData.AddDefaulted();
		FCompressedAbcData& CompressedData = CompressedMeshData.Last();

		if (ImportSettings->CompressionSettings.bBakeMatrixAnimation)
		{
			CompressedData.AverageSample = new FAbcMeshSample(*ConstantPolyMesh->GetTransformedFirstSample());
		}
		else
		{
			CompressedData.AverageSample = new FAbcMeshSample(*ConstantPolyMesh->GetFirstSample());
		}

		// QQ FUNCTIONALIZE
		// Add material names from this mesh object
		if (ConstantPolyMesh->FaceSetNames.Num() > 0)
		{
			CompressedData.MaterialNames.Append(ConstantPolyMesh->FaceSetNames);
		}
		else
		{
			static const FString DefaultName("NoFaceSetName");
			CompressedData.MaterialNames.Add(DefaultName);
		}
	}
		
	return bResult;
}

void FAbcImporter::CompareCompressionResult(const TArray<float>& OriginalMatrix, const uint32 NumSamples, const uint32 NumUsedSingularValues, const TArrayView<float>& OutU, const TArray<float>& OutV, const float Tolerance)
{
	if (NumSamples == 0)
	{
		return;
	}

	const uint32 NumRows = OriginalMatrix.Num() / NumSamples;

	TArray<float> ComparisonMatrix;
	ComparisonMatrix.AddZeroed(OriginalMatrix.Num());
	for (uint32 SampleIndex = 0; SampleIndex < NumSamples; ++SampleIndex)
	{
		const int32 SampleOffset = (SampleIndex * NumRows);
		const int32 CurveOffset = (SampleIndex * NumUsedSingularValues);
		for (uint32 BaseIndex = 0; BaseIndex < NumUsedSingularValues; ++BaseIndex)
		{
			const int32 BaseOffset = (BaseIndex * NumRows);
			for (uint32 RowIndex = 0; RowIndex < NumRows; RowIndex++)
			{
				ComparisonMatrix[RowIndex + SampleOffset] += OutU[RowIndex + BaseOffset] * OutV[BaseIndex + CurveOffset];
			}
		}
	}

	// Compare arrays
	for (int32 i = 0; i < ComparisonMatrix.Num(); ++i)
	{
		ensureMsgf(FMath::IsNearlyEqual(OriginalMatrix[i], ComparisonMatrix[i], Tolerance), TEXT("Difference of %2.10f found"), FMath::Abs(OriginalMatrix[i] - ComparisonMatrix[i]));
	}
}

const int32 FAbcImporter::PerformSVDCompression(const TArray<float>& OriginalMatrix, const TArray<float>& OriginalNormalsMatrix, const uint32 NumSamples, const float InPercentage, const int32 InFixedNumValue,
	TArray<float>& OutU, TArray<float>& OutNormalsU, TArray<float>& OutV)
{
	const int32 NumRows = OriginalMatrix.Num() / NumSamples;

	TArray<float> OutS;
	EigenHelpers::PerformSVD(OriginalMatrix, NumRows, NumSamples, OutU, OutV, OutS);

	// Now we have the new basis data we have to construct the correct morph target data and curves
	const float PercentageBasesUsed = InPercentage;
	const int32 NumNonZeroSingularValues = OutS.Num();
	const int32 NumUsedSingularValues = (InFixedNumValue != 0) ? FMath::Min(InFixedNumValue, (int32)OutS.Num()) : (int32)((float)NumNonZeroSingularValues * PercentageBasesUsed);

	// Pre-multiply the bases with it's singular values
	ParallelFor(NumUsedSingularValues, [&](int32 ValueIndex)
	{
		const float Multiplier = OutS[ValueIndex];
		const int32 ValueOffset = ValueIndex * NumRows;

		for (int32 RowIndex = 0; RowIndex < NumRows; ++RowIndex)
		{
			OutU[ValueOffset + RowIndex] *= Multiplier;
		}
	});

	// Project OriginalNormalsMatrix on OutV to deduce OutNormalsU
	// 
	// OriginalNormalsMatrix * OutV.transpose() = OutNormalsU
	//
	// This takes into account that OutNormalsU should be already scaled by what would be OutNormalsS, just like OutU is scaled by OutS

	const int32 NormalsNumRows = OriginalNormalsMatrix.Num() / NumSamples;

	Eigen::MatrixXf NormalsMatrix;
	EigenHelpers::ConvertArrayToEigenMatrix(OriginalNormalsMatrix, NormalsNumRows, NumSamples, NormalsMatrix);

	const uint32 OutVNumRows = OutV.Num() / NumSamples;

	Eigen::MatrixXf VMatrix;
	EigenHelpers::ConvertArrayToEigenMatrix(OutV, OutVNumRows, NumSamples, VMatrix);

	Eigen::MatrixXf NormalsUMatrix = NormalsMatrix * VMatrix.transpose();

	uint32 OutNumColumns, OutNumRows;
	EigenHelpers::ConvertEigenMatrixToArray(NormalsUMatrix, OutNormalsU, OutNumColumns, OutNumRows);

	UE_LOG(LogAbcImporter, Log, TEXT("Decomposed animation and reconstructed with %i number of bases (full %i, percentage %f, calculated %i)"), NumUsedSingularValues, OutS.Num(), PercentageBasesUsed * 100.0f, NumUsedSingularValues);	
	
	return NumUsedSingularValues;
}

const TArray<UStaticMesh*> FAbcImporter::ReimportAsStaticMesh(UStaticMesh* Mesh)
{
	const FString StaticMeshName = Mesh->GetName();
	const TArray<UStaticMesh*> StaticMeshes = ImportAsStaticMesh(Mesh->GetOuter(), RF_Public | RF_Standalone);

	return StaticMeshes;
}

UGeometryCache* FAbcImporter::ReimportAsGeometryCache(UGeometryCache* GeometryCache)
{
	UGeometryCache* ReimportedCache = ImportAsGeometryCache(GeometryCache->GetOuter(), RF_Public | RF_Standalone);
	return ReimportedCache;
}

TArray<UObject*> FAbcImporter::ReimportAsSkeletalMesh(USkeletalMesh* SkeletalMesh)
{
	TArray<UObject*> ReimportedObjects = ImportAsSkeletalMesh(SkeletalMesh->GetOuter(), RF_Public | RF_Standalone);
	return ReimportedObjects;
}

const TArray<FAbcPolyMesh*>& FAbcImporter::GetPolyMeshes() const
{
	return AbcFile->GetPolyMeshes();
}

const uint32 FAbcImporter::GetStartFrameIndex() const
{
	return (AbcFile != nullptr) ? AbcFile->GetMinFrameIndex() : 0;
}

const uint32 FAbcImporter::GetEndFrameIndex() const
{
	return (AbcFile != nullptr) ? FMath::Max(AbcFile->GetMaxFrameIndex() - 1, 1) : 1;
}

const uint32 FAbcImporter::GetNumMeshTracks() const
{
	return (AbcFile != nullptr) ? AbcFile->GetNumPolyMeshes() : 0;
}

void FAbcImporter::GenerateMeshDescriptionFromSample(const FAbcMeshSample* Sample, FMeshDescription* MeshDescription, UStaticMesh* StaticMesh)
{
	if (MeshDescription == nullptr)
	{
		return;
	}

	FStaticMeshAttributes Attributes(*MeshDescription);

	TVertexAttributesRef<FVector3f> VertexPositions = Attributes.GetVertexPositions();
	TEdgeAttributesRef<bool> EdgeHardnesses = Attributes.GetEdgeHardnesses();
	TPolygonGroupAttributesRef<FName> PolygonGroupImportedMaterialSlotNames = Attributes.GetPolygonGroupMaterialSlotNames();
	TVertexInstanceAttributesRef<FVector3f> VertexInstanceNormals = Attributes.GetVertexInstanceNormals();
	TVertexInstanceAttributesRef<FVector3f> VertexInstanceTangents = Attributes.GetVertexInstanceTangents();
	TVertexInstanceAttributesRef<float> VertexInstanceBinormalSigns = Attributes.GetVertexInstanceBinormalSigns();
	TVertexInstanceAttributesRef<FVector4f> VertexInstanceColors = Attributes.GetVertexInstanceColors();
	TVertexInstanceAttributesRef<FVector2f> VertexInstanceUVs = Attributes.GetVertexInstanceUVs();

	//Speedtree use UVs to store is data
	VertexInstanceUVs.SetNumChannels(Sample->NumUVSets);
	
	for (int32 MatIndex = 0; MatIndex < StaticMesh->GetStaticMaterials().Num(); ++MatIndex)
	{
		const FPolygonGroupID PolygonGroupID = MeshDescription->CreatePolygonGroup();
		PolygonGroupImportedMaterialSlotNames[PolygonGroupID] = StaticMesh->GetStaticMaterials()[MatIndex].ImportedMaterialSlotName;
	}

	// position
	for (int32 VertexIndex = 0; VertexIndex < Sample->Vertices.Num(); ++VertexIndex)
	{
		FVector3f Position = Sample->Vertices[VertexIndex];

		FVertexID VertexID = MeshDescription->CreateVertex();
		VertexPositions[VertexID] = FVector3f(Position);
	}

	uint32 VertexIndices[3];
	uint32 TriangleCount = Sample->Indices.Num() / 3;
	for (uint32 TriangleIndex = 0; TriangleIndex < TriangleCount; ++TriangleIndex)
	{
		const uint32 IndiceIndex0 = TriangleIndex * 3;
		VertexIndices[0] = Sample->Indices[IndiceIndex0];
		VertexIndices[1] = Sample->Indices[IndiceIndex0 + 1];
		VertexIndices[2] = Sample->Indices[IndiceIndex0 + 2];

		// Skip degenerated triangle
		if (VertexIndices[0] == VertexIndices[1] || VertexIndices[1] == VertexIndices[2] || VertexIndices[0] == VertexIndices[2])
		{
			continue;
		}

		TArray<FVertexInstanceID> CornerVertexInstanceIDs;
		CornerVertexInstanceIDs.SetNum(3);
		FVertexID CornerVertexIDs[3];
		for (int32 Corner = 0; Corner < 3; ++Corner)
		{
			uint32 IndiceIndex = IndiceIndex0 + Corner;
			uint32 VertexIndex = VertexIndices[Corner];
			const FVertexID VertexID(VertexIndex);
			const FVertexInstanceID VertexInstanceID = MeshDescription->CreateVertexInstance(VertexID);

			// tangents
			FVector3f TangentX = Sample->TangentX[IndiceIndex];
			FVector3f TangentY = Sample->TangentY[IndiceIndex];
			FVector3f TangentZ = Sample->Normals[IndiceIndex];

			VertexInstanceTangents[VertexInstanceID] = TangentX;
			VertexInstanceNormals[VertexInstanceID] = TangentZ;
			VertexInstanceBinormalSigns[VertexInstanceID] = GetBasisDeterminantSign((FVector)TangentX.GetSafeNormal(), (FVector)TangentY.GetSafeNormal(), (FVector)TangentZ.GetSafeNormal());

			if (Sample->Colors.Num())
			{
				VertexInstanceColors[VertexInstanceID] = FVector4f(Sample->Colors[IndiceIndex]);
			}
			else
			{
				VertexInstanceColors[VertexInstanceID] = FVector4f(FLinearColor::White);
			}

			for (uint32 UVIndex = 0; UVIndex < Sample->NumUVSets; ++UVIndex)
			{
				VertexInstanceUVs.Set(VertexInstanceID, UVIndex, Sample->UVs[UVIndex][IndiceIndex]);
			}
			CornerVertexInstanceIDs[Corner] = VertexInstanceID;
			CornerVertexIDs[Corner] = VertexID;
		}

		const FPolygonGroupID PolygonGroupID(Sample->MaterialIndices[TriangleIndex]);
		// Insert a polygon into the mesh
		MeshDescription->CreatePolygon(PolygonGroupID, CornerVertexInstanceIDs);
	}
	//Set the edge hardness from the smooth group
	FStaticMeshOperations::ConvertSmoothGroupToHardEdges(Sample->SmoothingGroupIndices, *MeshDescription);
}

bool FAbcImporter::BuildSkeletalMesh( FSkeletalMeshLODModel& LODModel, const FReferenceSkeleton& RefSkeleton, FAbcMeshSample* Sample, TArray<int32>& OutMorphTargetVertexRemapping, TArray<int32>& OutUsedVertexIndicesForMorphs)
{
	// Module manager is not thread safe, so need to prefetch before parallelfor
	IMeshUtilities& MeshUtilities = FModuleManager::Get().LoadModuleChecked<IMeshUtilities>("MeshUtilities");

	const bool bComputeNormals = (Sample->Normals.Num() == 0);
	const bool bComputeTangents = (Sample->TangentX.Num() == 0) || (Sample->TangentY.Num() == 0);

	// Compute normals/tangents if needed
	if (bComputeNormals || bComputeTangents)
	{
		uint32 TangentOptions = 0;
		MeshUtilities.CalculateTangents(Sample->Vertices, Sample->Indices, Sample->UVs[0], Sample->SmoothingGroupIndices, TangentOptions, Sample->TangentX, Sample->TangentY, Sample->Normals);
	}

	// Populate faces
	const uint32 NumFaces = Sample->Indices.Num() / 3;
	TArray<SkeletalMeshImportData::FMeshFace> Faces;
	Faces.AddZeroed(NumFaces);

	TArray<FMeshSection> MeshSections;
	MeshSections.AddDefaulted(Sample->NumMaterials);

	// Process all the faces and add to their respective mesh section
	for (uint32 FaceIndex = 0; FaceIndex < NumFaces; ++FaceIndex)
	{
		const uint32 FaceOffset = FaceIndex * 3;
		const int32 MaterialIndex = Sample->MaterialIndices[FaceIndex];

		check(MeshSections.IsValidIndex(MaterialIndex));

		FMeshSection& Section = MeshSections[MaterialIndex];
		Section.MaterialIndex = MaterialIndex;
		Section.NumUVSets = Sample->NumUVSets;
	
		for (uint32 VertexIndex = 0; VertexIndex < 3; ++VertexIndex)
		{
			LODModel.MaxImportVertex = FMath::Max<int32>(LODModel.MaxImportVertex, Sample->Indices[FaceOffset + VertexIndex]);

			Section.OriginalIndices.Add(FaceOffset + VertexIndex);
			Section.Indices.Add(Sample->Indices[FaceOffset + VertexIndex]);
			Section.TangentX.Add((FVector)Sample->TangentX[FaceOffset + VertexIndex]);
			Section.TangentY.Add((FVector)Sample->TangentY[FaceOffset + VertexIndex]);
			Section.TangentZ.Add((FVector)Sample->Normals[FaceOffset + VertexIndex]);

			for (uint32 UVIndex = 0; UVIndex < Sample->NumUVSets; ++UVIndex)
			{
				Section.UVs[UVIndex].Add(FVector2D(Sample->UVs[UVIndex][FaceOffset + VertexIndex]));
			}		
			
			Section.Colors.Add(Sample->Colors[FaceOffset + VertexIndex].ToFColor(false));
		}

		++Section.NumFaces;
	}

	// Sort the vertices by z value
	MeshSections.Sort([](const FMeshSection& A, const FMeshSection& B) { return A.MaterialIndex < B.MaterialIndex; });

	// Create Skeletal mesh LOD sections
	LODModel.Sections.Empty(MeshSections.Num());
	LODModel.NumVertices = 0;
	LODModel.IndexBuffer.Empty();

	TArray<uint32>& RawPointIndices = LODModel.GetRawPointIndices();
	RawPointIndices.Reset();

	TArray< TArray<uint32> > VertexIndexRemap;
	VertexIndexRemap.Empty(MeshSections.Num());

	// Create actual skeletal mesh sections
	for (int32 SectionIndex = 0; SectionIndex < MeshSections.Num(); ++SectionIndex)
	{
		const FMeshSection& SourceSection = MeshSections[SectionIndex];
		FSkelMeshSection& TargetSection = *new(LODModel.Sections) FSkelMeshSection();
		TargetSection.MaterialIndex = (uint16)SourceSection.MaterialIndex;
		TargetSection.NumTriangles = SourceSection.NumFaces;
		TargetSection.BaseVertexIndex = LODModel.NumVertices;

		// Separate the section's vertices into rigid and soft vertices.
		TArray<uint32>& ChunkVertexIndexRemap = *new(VertexIndexRemap)TArray<uint32>();
		ChunkVertexIndexRemap.AddUninitialized(SourceSection.NumFaces * 3);

		TMultiMap<uint32, uint32> FinalVertices;
		TArray<uint32> DuplicateVertexIndices;

		// Reused soft vertex
		FSoftSkinVertex NewVertex;

		uint32 VertexOffset = 0;
		// Generate Soft Skin vertices (used by the skeletal mesh)
		for (uint32 FaceIndex = 0; FaceIndex < SourceSection.NumFaces; ++FaceIndex)
		{
			const uint32 FaceOffset = FaceIndex * 3;

			for (uint32 VertexIndex = 0; VertexIndex < 3; ++VertexIndex)
			{
				const uint32 Index = SourceSection.Indices[FaceOffset + VertexIndex];

				DuplicateVertexIndices.Reset();
				FinalVertices.MultiFind(Index, DuplicateVertexIndices);
				
				// Populate vertex data
				NewVertex.Position = Sample->Vertices[Index];
				NewVertex.TangentX = (FVector3f)SourceSection.TangentX[FaceOffset + VertexIndex];
				NewVertex.TangentY = (FVector3f)SourceSection.TangentY[FaceOffset + VertexIndex];
				NewVertex.TangentZ = (FVector3f)SourceSection.TangentZ[FaceOffset + VertexIndex]; // LWC_TODO: precision loss
				for (uint32 UVIndex = 0; UVIndex < SourceSection.NumUVSets; ++UVIndex)
				{
					NewVertex.UVs[UVIndex] = FVector2f(SourceSection.UVs[UVIndex][FaceOffset + VertexIndex]);
				}
				
				NewVertex.Color = SourceSection.Colors[FaceOffset + VertexIndex];

				// Set up bone influence (only using one bone so maxed out weight)
				FMemory::Memzero(NewVertex.InfluenceBones);
				FMemory::Memzero(NewVertex.InfluenceWeights);
				NewVertex.InfluenceWeights[0] = 255;
				
				int32 FinalVertexIndex = INDEX_NONE;
				if (DuplicateVertexIndices.Num())
				{
					for (const uint32 DuplicateVertexIndex : DuplicateVertexIndices)
					{
						if (AbcImporterUtilities::AreVerticesEqual(TargetSection.SoftVertices[DuplicateVertexIndex], NewVertex))
						{
							// Use the existing vertex
							FinalVertexIndex = DuplicateVertexIndex;
							break;
						}
					}
				}

				if (FinalVertexIndex == INDEX_NONE)
				{
					FinalVertexIndex = TargetSection.SoftVertices.Add(NewVertex);
#if PRINT_UNIQUE_VERTICES
					FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Vert - P(%.2f, %.2f,%.2f) N(%.2f, %.2f,%.2f) TX(%.2f, %.2f,%.2f) TY(%.2f, %.2f,%.2f) UV(%.2f, %.2f)\n"), NewVertex.Position.X, NewVertex.Position.Y, NewVertex.Position.Z, SourceSection.TangentX[FaceOffset + VertexIndex].X, 
						SourceSection.TangentZ[FaceOffset + VertexIndex].X, SourceSection.TangentZ[FaceOffset + VertexIndex].Y, SourceSection.TangentZ[FaceOffset + VertexIndex].Z, SourceSection.TangentX[FaceOffset + VertexIndex].Y, SourceSection.TangentX[FaceOffset + VertexIndex].Z, SourceSection.TangentY[FaceOffset + VertexIndex].X, SourceSection.TangentY[FaceOffset + VertexIndex].Y, SourceSection.TangentY[FaceOffset + VertexIndex].Z, NewVertex.UVs[0].X, NewVertex.UVs[0].Y);
#endif

					FinalVertices.Add(Index, FinalVertexIndex);
					OutUsedVertexIndicesForMorphs.Add(Index);
					OutMorphTargetVertexRemapping.Add(SourceSection.OriginalIndices[FaceOffset + VertexIndex]);
				}

				RawPointIndices.Add(FinalVertexIndex);
				ChunkVertexIndexRemap[VertexOffset] = TargetSection.BaseVertexIndex + FinalVertexIndex;
				++VertexOffset;
			}
		}

		LODModel.NumVertices += TargetSection.SoftVertices.Num();
		TargetSection.NumVertices = TargetSection.SoftVertices.Num();

		// Only need first bone from active bone indices
		TargetSection.BoneMap.Add(0);

		TargetSection.CalcMaxBoneInfluences();
		TargetSection.CalcUse16BitBoneIndex();
	}

	// Only using bone zero
	LODModel.ActiveBoneIndices.Add(0);

	// Finish building the sections.
	for (int32 SectionIndex = 0; SectionIndex < LODModel.Sections.Num(); SectionIndex++)
	{
		FSkelMeshSection& Section = LODModel.Sections[SectionIndex];

		const TArray<uint32>& SectionIndices = MeshSections[SectionIndex].Indices;
		Section.BaseIndex = LODModel.IndexBuffer.Num();
		const int32 NumIndices = SectionIndices.Num();
		const TArray<uint32>& SectionVertexIndexRemap = VertexIndexRemap[SectionIndex];
		for (int32 Index = 0; Index < NumIndices; Index++)
		{
			uint32 VertexIndex = SectionVertexIndexRemap[Index];
			LODModel.IndexBuffer.Add(VertexIndex);
		}
	}

	// Compute the required bones for this model.
	USkeletalMesh::CalculateRequiredBones(LODModel, RefSkeleton, NULL);

	return true;
}

void FAbcImporter::GenerateMorphTargetVertices(FAbcMeshSample* BaseSample, TArray<FMorphTargetDelta> &MorphDeltas, FAbcMeshSample* AverageSample, uint32 WedgeOffset, const TArray<int32>& RemapIndices, const TArray<int32>& UsedVertexIndicesForMorphs, const uint32 VertexOffset, const uint32 IndexOffset)
{
	FMorphTargetDelta MorphVertex;
	const uint32 NumberOfUsedVertices = UsedVertexIndicesForMorphs.Num();	
	for (uint32 VertIndex = 0; VertIndex < NumberOfUsedVertices; ++VertIndex)
	{
		const int32 UsedVertexIndex = UsedVertexIndicesForMorphs[VertIndex] - VertexOffset;
		const uint32 UsedNormalIndex = RemapIndices[VertIndex] - IndexOffset;

		if (UsedVertexIndex >= 0 && UsedVertexIndex < BaseSample->Vertices.Num())
		{
			// Position delta
			MorphVertex.PositionDelta = BaseSample->Vertices[UsedVertexIndex] - AverageSample->Vertices[UsedVertexIndex];
			// Tangent delta
			MorphVertex.TangentZDelta = BaseSample->Normals[UsedNormalIndex] - AverageSample->Normals[UsedNormalIndex];
			// Index of base mesh vert this entry is to modify
			MorphVertex.SourceIdx = VertIndex;
			MorphDeltas.Add(MorphVertex);
		}
	}
}

FCompressedAbcData::~FCompressedAbcData()
{
	delete AverageSample;
	for (FAbcMeshSample* Sample : BaseSamples)
	{
		delete Sample;
	}
}

#undef LOCTEXT_NAMESPACE
