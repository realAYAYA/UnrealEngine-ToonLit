// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCO/CustomizableObject.h"

#include "Algo/Copy.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Async/AsyncFileHandle.h"
#include "EdGraph/EdGraph.h"
#include "Engine/SkeletalMesh.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Input/Reply.h"
#include "Interfaces/ITargetPlatform.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/DataValidation.h"
#include "MuCO/CustomizableObjectInstance.h"
#include "MuCO/CustomizableObjectPrivate.h"
#include "MuCO/CustomizableObjectSystem.h"
#include "MuCO/ICustomizableObjectModule.h"
#include "MuCO/UnrealMutableModelDiskStreamer.h"
#include "MuR/ModelPrivate.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "UObject/ObjectSaveContext.h"

#if WITH_EDITOR
#include "Editor.h"
#endif


#include UE_INLINE_GENERATED_CPP_BY_NAME(CustomizableObject)

class UMaterialInterface;
class UPhysicsAsset;
class USkeletalMesh;
class USkeleton;

#define LOCTEXT_NAMESPACE "CustomizableObject"

DEFINE_LOG_CATEGORY(LogMutable);

#if WITH_EDITOR
static bool bUsesOnCookStart = false;
#endif
//-------------------------------------------------------------------------------------------------

UCustomizableObject::UCustomizableObject()
	: UObject()
{
	PrivateData = TSharedPtr<FCustomizableObjectPrivateData>( new FCustomizableObjectPrivateData() );

#if WITH_EDITORONLY_DATA
	const FString CVarName = TEXT("r.SkeletalMesh.MinLodQualityLevel");
	const FString ScalabilitySectionName = TEXT("ViewDistanceQuality");
	LODSettings.MinQualityLevelLOD.Init(*CVarName, *ScalabilitySectionName);
#endif
}


#if WITH_EDITOR
bool UCustomizableObject::IsEditorOnly() const
{
	return bIsChildObject;
}


void UCustomizableObject::UpdateVersionId()
{
	VersionId = FGuid::NewGuid();
}


void UCustomizableObject::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	int32 isRoot = 0;
#if WITH_EDITOR
	FCustomizableObjectCompilerBase* Compiler = UCustomizableObjectSystem::GetInstance()->GetNewCompiler();
	if (Compiler)
	{
		isRoot = Compiler->IsRootObject(this) ? 1 : 0;
		delete Compiler;
	}
#endif

	OutTags.Add(FAssetRegistryTag("IsRoot", FString::FromInt(isRoot), FAssetRegistryTag::TT_Numerical));
	Super::GetAssetRegistryTags(OutTags);
}


void UCustomizableObject::PreSave(FObjectPreSaveContext ObjectSaveContext)
{
	Super::PreSave(ObjectSaveContext);

	// Update the derived child object flag
	FCustomizableObjectCompilerBase* Compiler = UCustomizableObjectSystem::GetInstance()->GetNewCompiler();
	if (Compiler)
	{
		bIsChildObject = !Compiler->IsRootObject(this);
		delete Compiler;

		if (bIsChildObject)
		{
			GetPackage()->SetPackageFlags(PKG_EditorOnly);
		}
		else
		{
			GetPackage()->ClearPackageFlags(PKG_EditorOnly);
		}
	}

	if (!Identifier.IsValid())
	{
		Identifier = FGuid::NewGuid();
	}

#if WITH_EDITORONLY_DATA
	if (ObjectSaveContext.IsCooking() && !bIsChildObject)
	{
		if (const FMutableCachedPlatformData* PlatformData = CachedPlatformsData.Find(ObjectSaveContext.GetTargetPlatform()->PlatformName()))
		{
			// Load cached data before saving
			FMemoryReaderView MemoryReader(PlatformData->ModelData);
			LoadCompiledData(MemoryReader, true);

			// Create an export object to manage the streamable data
			BulkData = NewObject<UCustomizableObjectBulk>(this);
			BulkData->Mark(OBJECTMARK_TagExp);

			// Split streamable data into smaller chunks and fix up the CO HashToStreamableBlock's FileIndex and Offset
			BulkData->PrepareBulkData(this, ObjectSaveContext.GetTargetPlatform());
		}
		else
		{
			UE_LOG(LogMutable, Warning, TEXT("Cook: Customizable Object [%s] is missing [%s] platform data."), *GetName(),
				*ObjectSaveContext.GetTargetPlatform()->PlatformName());
			
			ClearCompiledData();
			SetModel(nullptr);
		}
	}
#endif
}


#endif // End WITH_EDITOR


void UCustomizableObject::PostLoad()
{
	Super::PostLoad();

	// Make sure mutable has been initialised.
	UCustomizableObjectSystem::GetInstance();

#if WITH_EDITOR
	if (ReferenceSkeletalMesh_DEPRECATED)
	{
		ReferenceSkeletalMeshes.Add(ReferenceSkeletalMesh_DEPRECATED);
		ReferenceSkeletalMesh_DEPRECATED = nullptr;
	}
#endif
}


bool UCustomizableObject::IsLocked() const
{
	if (PrivateData.IsValid())
	{
		return PrivateData->bLocked;
	}

	return false;
}


void UCustomizableObject::Serialize(FArchive& Ar_Asset)
{
	MUTABLE_CPUPROFILER_SCOPE(UCustomizableObject::Serialize)
	
	Super::Serialize(Ar_Asset);

#if WITH_EDITOR
	if (Ar_Asset.IsCooking())
	{
		if (Ar_Asset.IsSaving())
		{
			UE_LOG(LogMutable, Verbose, TEXT("Serializing cooked data for Customizable Object [%s]."), *GetName());
			SaveEmbeddedData(Ar_Asset);
		}
	}
	else
	{
		// Can't remove this or saved customizable objects will fail to load
		int64 InternalVersion = CurrentSupportedVersion;
		Ar_Asset << InternalVersion;
		
		if (Ar_Asset.IsLoading())
		{
			LoadCompiledDataFromDisk();
		}
	}
#else
	if (Ar_Asset.IsLoading())
	{
		LoadEmbeddedData(Ar_Asset);
	}
#endif
}


#if WITH_EDITOR
void UCustomizableObject::PostRename(UObject * OldOuter, const FName OldName)
{
	Super::PostRename(OldOuter, OldName);

	if (Source)
	{
		Source->PostRename(OldOuter, OldName);
	}
}


void UCustomizableObject::PostDuplicate(EDuplicateMode::Type DuplicateMode)
{
	Super::PostDuplicate(DuplicateMode);

	if (DuplicateMode == EDuplicateMode::Normal)
	{
		// The only place where we can change the identifier
		Identifier = FGuid::NewGuid();

		// Create a new Private Data or it will use the same as the original Customizable Object
		PrivateData = TSharedPtr<FCustomizableObjectPrivateData>(new FCustomizableObjectPrivateData());
	}
}


void UCustomizableObject::BeginCacheForCookedPlatformData(const ITargetPlatform* TargetPlatform)
{
	bool bIsRelevantForThisTarget =
		(Relevancy == ECustomizableObjectRelevancy::All)
		||
		(TargetPlatform && Relevancy == ECustomizableObjectRelevancy::ClientOnly && !TargetPlatform->IsServerOnly());
	
	if (TargetPlatform && bIsRelevantForThisTarget)
	{
		if (PrivateData->CachedPlatformNames.Find(TargetPlatform->PlatformName()) == INDEX_NONE)
		{
			if (!bUsesOnCookStart)
			{
				// Compile and save in the CachedPlatformsData map
				CompileForTargetPlatform(TargetPlatform);
			}
			else
			{
				// Load from Disk
				LoadCompiledDataFromDisk(false, TargetPlatform);

				if (!PrivateData->bModelCompiledForCook)
				{
					LoadReferencedObjects();
					PrivateData->bModelCompiledForCook = true;
				}
			}

			PrivateData->CachedPlatformNames.Add(TargetPlatform->PlatformName());
		}
	}
	else
	{
		ClearCompiledData();
		PrivateData->SetModel(nullptr); // Discard compilation
		if (TargetPlatform)
		{
			PrivateData->CachedPlatformNames.Add(TargetPlatform->PlatformName());
		}
	}
}


bool UCustomizableObject::IsCachedCookedPlatformDataLoaded( const ITargetPlatform* TargetPlatform ) 
{ 
	return PrivateData->CachedPlatformNames.Find(TargetPlatform->PlatformName()) != INDEX_NONE;
}

// TODO COOK: Remove Hack to add new references to the package
void UCustomizableObject::LoadReferencedObjects()
{
	for (TSoftObjectPtr<UMaterialInterface> r : ReferencedMaterials)
	{
		r.LoadSynchronous();
	}

	for(TPair<FString, TSoftObjectPtr<UPhysicsAsset>>& p : PhysicsAssetsMap)
	{
		p.Value.LoadSynchronous();
	}
	
	for (TPair<FString, FParameterUIData>& i : ParameterUIDataMap)
	{
		i.Value.LoadResources();
	}

	for (TPair<FString, FParameterUIData>& s : StateUIDataMap)
	{
		s.Value.LoadResources();
	}
}


void UCustomizableObject::ClearCompiledData()
{
	ReferenceSkeletalMeshesData.Empty();
	ReferencedMaterials.Empty();
	ReferencedMaterialSlotNames.Empty();
	ReferencedSkeletons.Empty();
	ImageProperties.Empty();
	ParameterUIDataMap.Empty();
	StateUIDataMap.Empty();
	PhysicsAssetsMap.Empty();
	ContributingMorphTargetsInfo.Empty();
	MorphTargetReconstructionData.Empty();
	ClothMeshToMeshVertData.Empty();
	ContributingClothingAssetsData.Empty();
	ClothSharedConfigsData.Empty();
	SkinWeightProfilesInfo.Empty();

#if WITH_EDITORONLY_DATA
	CustomizableObjectPathMap.Empty();
	GroupNodeMap.Empty();
#endif

	HashToStreamableBlock.Empty();
	BulkData = nullptr;
}


void UCustomizableObject::UpdateCompiledDataFromModel()
{
	TSharedPtr<mu::Model, ESPMode::ThreadSafe> Model = PrivateData->GetModel();

	// Generate a map that using the resource id tells the offset and size of the resource inside the bulk data
	if(Model)
	{
		uint64_t Offset = 0;
		uint32_t ResourceId = 0;
		uint32_t ResourceSize = 0;

		int32 NumStreamingFiles = Model->GetPrivate()->m_program.m_roms.Num();
		HashToStreamableBlock.Empty(NumStreamingFiles);

		for (size_t FileIndex = 0; FileIndex < NumStreamingFiles; ++FileIndex)
		{
			ResourceId = Model->GetPrivate()->m_program.m_roms[FileIndex].Id;
			ResourceSize = Model->GetPrivate()->m_program.m_roms[FileIndex].Size;

			HashToStreamableBlock.Add(ResourceId, FMutableStreamableBlock({0, Offset, ResourceSize }));
			Offset += ResourceSize;
		}
	}

	// Generate ParameterProperties and IntParameterLookUpTable
	UpdateParameterPropertiesFromModel();
}


void UCustomizableObject::SaveCompiledData(FArchive& MemoryWriter, bool bSkipEditorOnlyData)
{
	int32 InternalVersion = CurrentSupportedVersion;
	MutableCompiledDataStreamHeader Header(InternalVersion, VersionId);
	MemoryWriter << Header;

	MemoryWriter << ReferenceSkeletalMeshesData;
	
	int32 NumReferencedMaterials = ReferencedMaterials.Num();
	MemoryWriter << NumReferencedMaterials;

	for (const TSoftObjectPtr<UMaterialInterface>& Material : ReferencedMaterials)
	{
		FString StringRef = Material.ToSoftObjectPath().ToString();
		MemoryWriter << StringRef;
	}

	int32 NumReferencedMaterialSlotNames = ReferencedMaterialSlotNames.Num();
	MemoryWriter << NumReferencedMaterialSlotNames;

	for (const FName& MaterialSlotName : ReferencedMaterialSlotNames)
	{
		FString StringRef = MaterialSlotName.ToString();
		MemoryWriter << StringRef;
	}

	int32 NumReferencedSkeletons = ReferencedSkeletons.Num();
	MemoryWriter << NumReferencedSkeletons;

	for (const TSoftObjectPtr<USkeleton>& Skeleton : ReferencedSkeletons)
	{
		FString StringRef = Skeleton.ToSoftObjectPath().ToString();
		MemoryWriter << StringRef;
	}

	MemoryWriter << ImageProperties;
	MemoryWriter << ParameterUIDataMap;
	MemoryWriter << StateUIDataMap;

	int32 NumPhysicsAssets = PhysicsAssetsMap.Num();
	MemoryWriter << NumPhysicsAssets;

	for (const TPair<FString, TSoftObjectPtr<class UPhysicsAsset>>& PhysicsAsset : PhysicsAssetsMap)
	{
		FString StringRef = PhysicsAsset.Value.ToSoftObjectPath().ToString();
		MemoryWriter << StringRef;
	}

	MemoryWriter << ContributingMorphTargetsInfo;
	MemoryWriter << MorphTargetReconstructionData;
	
	MemoryWriter << ClothMeshToMeshVertData;
	MemoryWriter << ContributingClothingAssetsData;
	MemoryWriter << ClothSharedConfigsData; 
	
	MemoryWriter << SkinWeightProfilesInfo;

	MemoryWriter << HashToStreamableBlock;

	// All Editor Only data must be serialized here
	if (!bSkipEditorOnlyData)
	{
		MemoryWriter << CustomizableObjectPathMap;
		MemoryWriter << GroupNodeMap;
	}

	MemoryWriter << LODSettings.NumLODsInRoot;
	MemoryWriter << NumMeshComponentsInRoot;

	MemoryWriter << LODSettings.FirstLODAvailable;

	MemoryWriter << LODSettings.NumLODsToStream;
	MemoryWriter << LODSettings.bLODStreamingEnabled;
}

void UCustomizableObject::LoadCompiledData(FArchive& MemoryReader, bool bSkipEditorOnlyData)
{
	PrivateData->SetModel(nullptr);
	ClearCompiledData();

	MutableCompiledDataStreamHeader Header;
	MemoryReader << Header;

	if (CurrentSupportedVersion == Header.InternalVersion)
	{
		// Make sure mutable has been initialised.
		UCustomizableObjectSystem::GetInstance();

		MemoryReader << ReferenceSkeletalMeshesData;
		
		// We can load
		int32 NumReferencedMaterials = 0;
		MemoryReader << NumReferencedMaterials;

		for (int32 i = 0; i < NumReferencedMaterials; ++i)
		{
			FString StringRef;
			MemoryReader << StringRef;

			ReferencedMaterials.Add(TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(StringRef)));
		}

		int32 NumReferencedMaterialSlotNames = 0;
		MemoryReader << NumReferencedMaterialSlotNames;

		for (int32 i = 0; i < NumReferencedMaterialSlotNames; ++i)
		{
			FString StringRef;
			MemoryReader << StringRef;

			ReferencedMaterialSlotNames.Add(FName(*StringRef));
		}

		int32 NumReferencedSkeletons = 0;
		MemoryReader << NumReferencedSkeletons;

		for (int32 SkeletonIndex = 0; SkeletonIndex < NumReferencedSkeletons; ++SkeletonIndex)
		{
			FString StringRef;
			MemoryReader << StringRef;

			ReferencedSkeletons.Add(TSoftObjectPtr<USkeleton>(FSoftObjectPath(StringRef)));
		}

		MemoryReader << ImageProperties;
		MemoryReader << ParameterUIDataMap;
		MemoryReader << StateUIDataMap;

		int32 NumPhysicsAssets = 0;
		MemoryReader << NumPhysicsAssets;

		for (int32 i = 0; i < NumPhysicsAssets; ++i)
		{
			FString StringRef;
			MemoryReader << StringRef;

			PhysicsAssetsMap.Add(StringRef, TSoftObjectPtr<UPhysicsAsset>(FSoftObjectPath(StringRef)));
		}

		MemoryReader << ContributingMorphTargetsInfo;
		MemoryReader << MorphTargetReconstructionData;

		MemoryReader << ClothMeshToMeshVertData;
		MemoryReader << ContributingClothingAssetsData;
		MemoryReader << ClothSharedConfigsData;

		MemoryReader << SkinWeightProfilesInfo;

		MemoryReader << HashToStreamableBlock;

		// All Editor Only data must be loaded here
		if (!bSkipEditorOnlyData)
		{
			MemoryReader << CustomizableObjectPathMap;
			MemoryReader << GroupNodeMap;
		}

		MemoryReader << LODSettings.NumLODsInRoot;
		MemoryReader << NumMeshComponentsInRoot;

		MemoryReader << LODSettings.FirstLODAvailable;

		MemoryReader << LODSettings.NumLODsToStream;
		MemoryReader << LODSettings.bLODStreamingEnabled;

		bool bModelSerialized = false;
		MemoryReader << bModelSerialized;

		if (bModelSerialized)
		{
			UnrealMutableInputStream stream(MemoryReader);
			mu::InputArchive arch(&stream);
			TSharedPtr<mu::Model, ESPMode::ThreadSafe> Model = mu::Model::StaticUnserialise( arch );

			PrivateData->SetModel(Model);
		}
	}

	UpdateParameterPropertiesFromModel();
}

void UCustomizableObject::LoadCompiledDataFromDisk(bool bIsEditorData, const ITargetPlatform* InTargetPlatform)
{
	FString PlatformName = InTargetPlatform ? InTargetPlatform->PlatformName() : FPlatformProperties::PlatformName();

	if (bIsEditorData)
	{
		// Customizable Object outdated
		if (!Identifier.IsValid())
		{
			return;
		}
	}
	else // Loading cooked data
	{
		// If we don't use OnCookStart there's nothing to be loaded. Same case for child objects.
		if (!bUsesOnCookStart || bIsChildObject) return;

		// Platform Data already cached
		if (CachedPlatformsData.Contains(PlatformName)) return;
	}

	// Compose Folder Name
	const FString FolderPath = GetCompiledDataFolderPath(bIsEditorData);

	// Compose File Names
	FString ModelFileName = FolderPath + GetCompiledDataFileName(true, InTargetPlatform);
	FString StreamableFileName = FolderPath + GetCompiledDataFileName(false, InTargetPlatform);

	IFileManager& FileManager = IFileManager::Get();
	if (FileManager.FileExists(*ModelFileName) && FileManager.FileExists(*StreamableFileName))
	{
		// Check CompiledData
		IFileHandle* CompiledDataFileHandle = FPlatformFileManager::Get().GetPlatformFile().OpenRead(*ModelFileName);
		IFileHandle* StreamableDataFileHandle = FPlatformFileManager::Get().GetPlatformFile().OpenRead(*StreamableFileName);

		MutableCompiledDataStreamHeader CompiledDataHeader;
		MutableCompiledDataStreamHeader StreamableDataHeader;

		int32 HeaderSize = sizeof(MutableCompiledDataStreamHeader);
		TArray<uint8> HeaderBytes;
		HeaderBytes.SetNum(HeaderSize);

		{
			CompiledDataFileHandle->Read(HeaderBytes.GetData(), HeaderSize);
			FMemoryReader AuxMemoryReader(HeaderBytes);
			AuxMemoryReader << CompiledDataHeader;
		}
		{
			StreamableDataFileHandle->Read(HeaderBytes.GetData(), HeaderSize);
			FMemoryReader AuxMemoryReader(HeaderBytes);
			AuxMemoryReader << StreamableDataHeader;
		}

		if (CompiledDataHeader.InternalVersion == CurrentSupportedVersion
			&&
			CompiledDataHeader.InternalVersion == StreamableDataHeader.InternalVersion 
			&&
			CompiledDataHeader.VersionId == StreamableDataHeader.VersionId)
		{
			if ( bIsEditorData && (IsRunningGame() || CompiledDataHeader.VersionId == VersionId) )
			{ 
				int64 CompiledDataSize = CompiledDataFileHandle->Size() - HeaderSize;
				TArray64<uint8> CompiledDataBytes;
				CompiledDataBytes.SetNumUninitialized(CompiledDataSize);

				CompiledDataFileHandle->Seek(HeaderSize);
				CompiledDataFileHandle->Read(CompiledDataBytes.GetData(), CompiledDataSize);

				FMemoryReaderView MemoryReader(CompiledDataBytes);
				LoadCompiledData(MemoryReader);
			}
			else if (!bIsEditorData)// Caching Cooked Data
			{
				FMutableCachedPlatformData& CachedData = CachedPlatformsData.Add(PlatformName);
				int64 CompiledDataSize = CompiledDataFileHandle->Size() - HeaderSize;
				int64 StreamableDataSize = StreamableDataFileHandle->Size() - HeaderSize;

				CachedData.ModelData.SetNumUninitialized(CompiledDataSize);
				CachedData.StreamableData.SetNumUninitialized(StreamableDataSize);

				// Change the current read position to exclude the header
				CompiledDataFileHandle->Seek(HeaderSize);
				StreamableDataFileHandle->Seek(HeaderSize);

				// Read Data
				CompiledDataFileHandle->Read(CachedData.ModelData.GetData(), CompiledDataSize);
				StreamableDataFileHandle->Read(CachedData.StreamableData.GetData(), StreamableDataSize);
			}
		}

		delete CompiledDataFileHandle;
		delete StreamableDataFileHandle;
	}
}


void UCustomizableObject::CachePlatformData(const ITargetPlatform* InTargetPlatform, const TArray64<uint8>& InModelBytes, const TArray64< uint8>& InBulkBytes)
{
	FString PlatformName = InTargetPlatform ? InTargetPlatform->PlatformName() : FPlatformProperties::PlatformName();

	check(!CachedPlatformsData.Find(PlatformName));

	FMutableCachedPlatformData& Data = CachedPlatformsData.Add(PlatformName);

	// Cache CO data and mu::Model
	FMemoryWriter64 MemoryWriter(Data.ModelData);
	SaveCompiledData(MemoryWriter, true);
	Data.ModelData.Append(InModelBytes);

	// Cache streamable bulk data
	Data.StreamableData.Reset(InBulkBytes.Num());
	Algo::Copy(InBulkBytes, Data.StreamableData);
}


void UCustomizableObject::CompileForTargetPlatform(const ITargetPlatform* TargetPlatform, bool bIsOnCookStart)
{
	// TEMP COOK: Remove together with OnCookStart
	bUsesOnCookStart = bIsOnCookStart;

	FCustomizableObjectCompilerBase* Compiler = UCustomizableObjectSystem::GetInstance()->GetNewCompiler();

	bool bIsRootObject = Compiler->IsRootObject(this);
	
	bool bIsRelevantForThisTarget =
		(Relevancy == ECustomizableObjectRelevancy::All)
		||
		(TargetPlatform && Relevancy == ECustomizableObjectRelevancy::ClientOnly && !TargetPlatform->IsServerOnly());

	// Discard any older compilation
	PrivateData->SetModel(nullptr);

	if (bIsRootObject && bIsRelevantForThisTarget)
	{

		FCompilationOptions Options;
		Options.OptimizationLevel = 3;	// max optimization when packaging.
		Options.bTextureCompression = true;
		Options.bIsCooking = true;
		Options.bSaveCookedDataToDisk = bUsesOnCookStart;
		Options.TargetPlatform = TargetPlatform;
		Options.bExtraBoneInfluencesEnabled = ICustomizableObjectModule::Get().AreExtraBoneInfluencesEnabled();

		// If this is enabled, there are determinism problems. Disable it for packaging.
		Options.bUseParallelCompilation = false;

		Compiler->Compile(*this, Options, false);
	}

	delete Compiler;
}


bool UCustomizableObject::ConditionalAutoCompile()
{
	check(IsInGameThread());

	// Don't compile compiled objects
	if (IsCompiled())
	{
		return true;
	}

	// Don't compile objects being compiled
	if (IsLocked())
	{
		return false;
	}

	UCustomizableObjectSystem* System = UCustomizableObjectSystem::GetInstance();
	if (!System || !System->IsValidLowLevel() || System->HasAnyFlags(RF_BeginDestroyed))
	{
		return false;
	}

	// Don't compile if we're running game, Compile and/or if AutoCompile is disabled
	if (IsRunningGame() || System->IsCompilationDisabled() || !System->IsAutoCompileEnabled())
	{
		System->AddUncompiledCOWarning(this);
		return false;
	}

	// Discard any older compilation
	PrivateData->SetModel(nullptr);

	// Sync/Async compilation
	if (System->IsAutoCompilationSync())
	{
		FCustomizableObjectCompilerBase* Compiler = UCustomizableObjectSystem::GetInstance()->GetNewCompiler();
		Compiler->Compile(*this, this->CompileOptions, false);
		delete Compiler;
	}
	else
	{
		const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		const FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(FSoftObjectPath(GetPathName()));
		System->RecompileCustomizableObjectAsync(AssetData, this);
	}

	return IsCompiled();
}


FReply UCustomizableObject::AddNewParameterProfile(FString Name, UCustomizableObjectInstance& CustomInstance)
{

	if (Name.IsEmpty())
	{
		Name = "Unnamed_Profile";
	}

	FString ProfileName = Name;
	int32 Suffix = 0;

	bool bUniqueNameFound = false;
	while (!bUniqueNameFound)
	{
		FProfileParameterDat* Found = InstancePropertiesProfiles.FindByPredicate(
			[&ProfileName](const FProfileParameterDat& Profile) { return Profile.ProfileName == ProfileName; });

		bUniqueNameFound = static_cast<bool>(!Found);
		if (Found)
		{
			ProfileName = Name + FString::FromInt(Suffix);
			++Suffix;
		}
	}

	int32 ProfileIndex = InstancePropertiesProfiles.Emplace();

	InstancePropertiesProfiles[ProfileIndex].ProfileName = ProfileName;
	CustomInstance.SaveParametersToProfile(ProfileIndex);

	Modify();

	return FReply::Handled();
}


void UCustomizableObject::InitializeIdentifier()
{
	if (!Identifier.IsValid())
	{
		Identifier = FGuid::NewGuid();
		MarkPackageDirty();
	}
}


FString UCustomizableObject::GetCompiledDataFolderPath(bool bIsEditorData) const
{	
	const FString FolderName = bIsEditorData ? TEXT("MutableStreamedDataEditor/") : TEXT("MutableStreamedData/");
	return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() + FolderName);
}


FString UCustomizableObject::GetCompiledDataFileName(bool bIsModel, const ITargetPlatform* InTargetPlatform) const
{
	const FString PlatformName = InTargetPlatform ? InTargetPlatform->PlatformName() : FPlatformProperties::PlatformName();
	const FString FileIdentifier = Identifier.IsValid() ? Identifier.ToString() : VersionId.ToString();
	const FString Extension = bIsModel ? TEXT("_M.mut") : TEXT("_S.mut");
	return PlatformName + FileIdentifier + Extension;
}


FString UCustomizableObject::GetDesc()
{
	int states = GetStateCount();
	int params = GetParameterCount();
	return FString::Printf(TEXT("%d States, %d Parameters"), states, params);
}


void UCustomizableObject::SaveEmbeddedData(FArchive& Ar)
{
	UE_LOG(LogMutable, Log, TEXT("Saving embedded data for Customizable Object [%s] now at position %d."), *GetName(), int(Ar.Tell()));

	int32 InternalVersion = PrivateData->GetModel() ? CurrentSupportedVersion : -1;
	Ar << InternalVersion;

	if (PrivateData->GetModel())
	{
		// Serialize RefSkeletalMeshesData
		{
			Ar << ReferenceSkeletalMeshesData;
		}
		
		// Serialize morph data
		{
			Ar << ContributingMorphTargetsInfo;
			Ar << MorphTargetReconstructionData;
		}
		
		{
			Ar << ClothMeshToMeshVertData;
			Ar << ContributingClothingAssetsData;
			Ar << ClothSharedConfigsData;
		}

		// Serialise the entire model, but unload the streamable data first.
		{
			PrivateData->GetModel()->UnloadExternalData();

			UnrealMutableOutputStream stream(Ar);
			mu::OutputArchive arch(&stream);
			mu::Model::Serialise(PrivateData->GetModel().Get(), arch);
		}

		UE_LOG(LogMutable, Log, TEXT("Saved embedded data for Customizable Object [%s] now at position %d."), *GetName(), int(Ar.Tell()));
	}
}

#endif // End WITH_EDITOR 

void UCustomizableObject::LoadEmbeddedData(FArchive& Ar)
{
	MUTABLE_CPUPROFILER_SCOPE(UCustomizableObject::LoadEmbeddedData)

	int32 InternalVersion;
	Ar << InternalVersion;

	// If this fails, something went wrong with the packaging: we have data that belongs
	// to a different version than the code.
	check(CurrentSupportedVersion==InternalVersion);

	if(CurrentSupportedVersion == InternalVersion)
	{
		// Load RefSkeletalMeshesData
		{
			Ar << ReferenceSkeletalMeshesData;
		}
	
		// Load morph data
		{
			Ar << ContributingMorphTargetsInfo;
			Ar << MorphTargetReconstructionData;
		}
		
		{
			Ar << ClothMeshToMeshVertData;
			Ar << ContributingClothingAssetsData;
			Ar << ClothSharedConfigsData;
		}
		
		// Load model
		UnrealMutableInputStream stream(Ar);
		mu::InputArchive arch(&stream);
		TSharedPtr<mu::Model, ESPMode::ThreadSafe> Model = mu::Model::StaticUnserialise( arch );
		PrivateData->SetModel( Model );

		// Create parameter properties
		UpdateParameterPropertiesFromModel();
	}
}

bool UCustomizableObject::IsCompiled() const
{
	bool IsCompiled = PrivateData->GetModel() != nullptr;

	return IsCompiled;
}


USkeletalMesh* UCustomizableObject::GetRefSkeletalMesh(int32 ComponentIndex)
{
	if (ReferenceSkeletalMeshes.IsValidIndex(ComponentIndex))
	{
		return ReferenceSkeletalMeshes[ComponentIndex];
	}

	return nullptr;
}


FMutableRefSkeletalMeshData* UCustomizableObject::GetRefSkeletalMeshData(int32 ComponentIndex)
{
	if(ReferenceSkeletalMeshesData.IsValidIndex(ComponentIndex))
	{
		return &ReferenceSkeletalMeshesData[ComponentIndex];
	}

	UE_LOG(LogMutable, Warning, TEXT("UCustomizableObject::GetRefSkeletalMeshData with an invalid Index. "
			"Reference SkeletalMesh data and compiled model may be out of sync. "
			"Try recompiling the CustomizableObject [%s]."), *GetName() );
	
	return nullptr;
}


TSoftObjectPtr<UMaterialInterface> UCustomizableObject::GetReferencedMaterialAssetPtr( uint32 Index )
{
	if (ReferencedMaterials.IsValidIndex(Index))
	{
		return ReferencedMaterials[Index];
	}

	UE_LOG(LogMutable, Warning, TEXT("UCustomizableObject::GetReferencedMaterial with an invalid Index. "
		"Source material data and CustomizableObject data may be out of sync. "
		"Try recompiling and saving the CustomizableObject asset [%s]."), *GetName() );
	return nullptr;
}


TSoftObjectPtr<USkeleton> UCustomizableObject::GetReferencedSkeletonAssetPtr( uint32 Index )
{
	if (ReferencedSkeletons.IsValidIndex(Index))
	{
		return ReferencedSkeletons[Index];
	}

	UE_LOG(LogMutable, Warning, TEXT("UCustomizableObject::GetReferencedSkeleton with an invalid Index. "
		"Skeleton data and CustomizableObject data may be out of sync. "
		"Try recompiling and saving the CustomizableObject asset [%s]."), *GetName());
	return nullptr;
}


int32 UCustomizableObject::FindState( const FString& Name ) const
{
	int32 Result = -1;
	if (PrivateData->GetModel())
	{
		Result = PrivateData->GetModel()->FindState( TCHAR_TO_ANSI(*Name) );
	}

	return Result;
}


int32 UCustomizableObject::GetStateCount() const
{
	int32 Result = 0;

	if (PrivateData->GetModel())
	{
		Result = PrivateData->GetModel()->GetStateCount();
	}

	return Result;
}


FString UCustomizableObject::GetStateName(int32 StateIndex) const
{
	FString Result;

	if (PrivateData->GetModel())
	{
		Result = ANSI_TO_TCHAR( PrivateData->GetModel()->GetStateName(StateIndex) );
	}

	return Result;
}


int32 UCustomizableObject::GetStateParameterCount( int32 StateIndex ) const
{
	int32 Result = 0;

	if (PrivateData->GetModel())
	{
		Result = PrivateData->GetModel()->GetStateParameterCount(StateIndex);
	}

	return Result;
}

int32 UCustomizableObject::GetStateParameterIndex(int32 StateIndex, int32 ParameterIndex) const
{
	int32 Result = 0;

	if (PrivateData->GetModel())
	{
		Result = PrivateData->GetModel()->GetStateParameterIndex(StateIndex, ParameterIndex);
	}

	return Result;
}


int32 UCustomizableObject::GetStateParameterCount(const FString& StateName) const
{
	int32 StateIndex = FindState(StateName);
	
	return GetStateParameterCount(StateIndex);
}


FString UCustomizableObject::GetStateParameterName(const FString& StateName, int32 ParameterIndex) const
{
	int32 StateIndex = FindState(StateName);
	
	return GetStateParameterName(StateIndex, ParameterIndex);
}

FString UCustomizableObject::GetStateParameterName(int32 StateIndex, int32 ParameterIndex) const
{
	return GetParameterName(GetStateParameterIndex(StateIndex, ParameterIndex));
}


#if WITH_EDITORONLY_DATA
void UCustomizableObject::PostCompile()
{
	CompilationGuid = FGuid::NewGuid();

	PostCompileDelegate.Broadcast();
}
#endif


UCustomizableObjectInstance* UCustomizableObject::CreateInstance()
{
	MUTABLE_CPUPROFILER_SCOPE(UCustomizableObject::CreateInstance)

	UCustomizableObjectInstance* PreviewInstance = NewObject<UCustomizableObjectInstance>(GetTransientPackage(), NAME_None, RF_Transient);
	PreviewInstance->SetObject(this);
	PreviewInstance->bShowOnlyRuntimeParameters = false;

	UE_LOG(LogMutable, Log, TEXT("Created Customizable Object Instance."));

	return PreviewInstance;
}


TSharedPtr<mu::Model, ESPMode::ThreadSafe> UCustomizableObject::GetModel() const
{
	return PrivateData->GetModel();
}

#if WITH_EDITOR
void UCustomizableObject::SetModel(TSharedPtr<mu::Model, ESPMode::ThreadSafe> Model)
{
	PrivateData->SetModel(Model);
	
	UpdateCompiledDataFromModel();
}

#endif // End WITH_EDITOR


int32 UCustomizableObject::GetNumLODs() const
{
	return LODSettings.NumLODsInRoot;
}

int32 UCustomizableObject::GetComponentCount() const
{
	if (!IsCompiled())
	{
		UE_LOG(LogMutable, Warning,
		       TEXT(
			       "You are trying to get the component count of a non compiled CO. This will always return 0 as value."
		       ));
		return 0;
	}
	
	return NumMeshComponentsInRoot;
}

int UCustomizableObject::GetParameterCount() const
{
	return ParameterProperties.Num();
}


EMutableParameterType UCustomizableObject::GetParameterType(int32 ParamIndex) const
{
	if (ParamIndex >= 0 && ParamIndex < ParameterProperties.Num())
	{
		return ParameterProperties[ParamIndex].Type;
	}
	else
	{
		UE_LOG(LogMutable, Error, TEXT("Index [%d] out of ParameterProperties bounds at GetParameterType."), ParamIndex);
	}

	return EMutableParameterType::None;
}


EMutableParameterType UCustomizableObject::GetParameterTypeByName(const FString& Name) const
{
	const int* IndexPtr = ParameterPropertiesLookupTable.Find(Name);
	int Index = IndexPtr ? *IndexPtr : INDEX_NONE;

	if (ParameterProperties.IsValidIndex(Index))
	{
		return ParameterProperties[Index].Type;
	}

	UE_LOG(LogMutable, Warning, TEXT("Name '%s' does not exist in ParameterProperties lookup table at GetParameterTypeByName."), *Name);

	for (int32 ParamIndex = 0; ParamIndex < ParameterProperties.Num(); ++ParamIndex)
	{
		if (ParameterProperties[ParamIndex].Name == Name)
		{
			return ParameterProperties[ParamIndex].Type;
		}
	}

	UE_LOG(LogMutable, Warning, TEXT("Name '%s' does not exist in ParameterProperties at GetParameterTypeByName."), *Name);

	return EMutableParameterType::None;
}


static const FString s_EmptyString;

const FString & UCustomizableObject::GetParameterName(int32 ParamIndex) const
{
	if (ParamIndex >= 0 && ParamIndex < ParameterProperties.Num())
	{
		return ParameterProperties[ParamIndex].Name;
	}
	else
	{
		UE_LOG(LogMutable, Warning, TEXT("Index [%d] out of ParameterProperties bounds at GetParameterName."), ParamIndex);
	}

	return s_EmptyString;
}


void UCustomizableObject::UpdateParameterPropertiesFromModel()
{
	if (PrivateData->GetModel())
	{
		mu::ParametersPtr MutableParameters = mu::Model::NewParameters(PrivateData->GetModel());
		int paramCount = MutableParameters->GetCount();

		ParameterProperties.Reset(paramCount);
		ParameterPropertiesLookupTable.Empty(paramCount);
		for (int paramIndex = 0; paramIndex<paramCount; ++paramIndex)
		{
			FMutableModelParameterProperties Data;

			Data.Name = MutableParameters->GetName(paramIndex);
			Data.Type = EMutableParameterType::None;
			Data.ImageDescriptionCount = MutableParameters->GetAdditionalImageCount(paramIndex);

			mu::PARAMETER_TYPE mutableType = MutableParameters->GetType(paramIndex);
			switch (mutableType)
			{
			case mu::PARAMETER_TYPE::T_BOOL:
			{
				Data.Type = EMutableParameterType::Bool;
				break;
			}

			case mu::PARAMETER_TYPE::T_INT:
			{
				Data.Type = EMutableParameterType::Int;

				int ValueCount = MutableParameters->GetIntPossibleValueCount(paramIndex);
				for (int i = 0; i<ValueCount; ++i)
				{
					FMutableModelParameterValue ValueData;
					ValueData.Name = MutableParameters->GetIntPossibleValueName(paramIndex,i);
					ValueData.Value = MutableParameters->GetIntPossibleValue(paramIndex,i);
					Data.PossibleValues.Add(ValueData);
				}
				break;
			}

			case mu::PARAMETER_TYPE::T_FLOAT:
			{
				Data.Type = EMutableParameterType::Float;
				break;
			}

			case mu::PARAMETER_TYPE::T_COLOUR:
			{
				Data.Type = EMutableParameterType::Color;
				break;
			}

			case mu::PARAMETER_TYPE::T_PROJECTOR:
			{
				Data.Type = EMutableParameterType::Projector;
				break;
			}

			case mu::PARAMETER_TYPE::T_IMAGE:
			{
				Data.Type = EMutableParameterType::Texture;
				break;
			}

			default:
				// Unhandled type?
				check(false);
				break;
			}

			ParameterProperties.Add(Data);
			ParameterPropertiesLookupTable.Add(Data.Name, paramIndex);
		}
	}
	else
	{
		ParameterProperties.Reset();
		ParameterPropertiesLookupTable.Reset();
	}
}


int UCustomizableObject::GetParameterDescriptionCount(int32 ParamIndex) const
{
	if (ParamIndex >= 0 && ParamIndex < ParameterProperties.Num())
	{
		return ParameterProperties[ParamIndex].ImageDescriptionCount;
	}
	else
	{
		UE_LOG(LogMutable, Warning, TEXT("Index [%d] out of ParameterProperties bounds at GetParameterDescriptionCount."), ParamIndex);
	}

	return 0;
}


int32 UCustomizableObject::GetParameterDescriptionCount(const FString& ParamName) const
{
	return GetParameterDescriptionCount(FindParameter(ParamName));
}


int32 UCustomizableObject::GetIntParameterNumOptions(int32 ParamIndex) const
{
	if (ParamIndex >= 0 && ParamIndex < ParameterProperties.Num())
	{
		return ParameterProperties[ParamIndex].PossibleValues.Num();
	}
	else
	{
		UE_LOG(LogMutable, Warning, TEXT("Index [%d] out of ParameterProperties bounds at GetIntParameterNumOptions."), ParamIndex);
	}

	return 0;
}


const FString& UCustomizableObject::GetIntParameterAvailableOption(int32 ParamIndex, int32 K) const
{
	if (ParamIndex >= 0 && ParamIndex < ParameterProperties.Num())
	{
		if (K >= 0 && K < GetIntParameterNumOptions(ParamIndex))
		{
			return ParameterProperties[ParamIndex].PossibleValues[K].Name;
		}
		else
		{
			UE_LOG(LogMutable, Warning, TEXT("Index [%d] out of IntParameterNumOptions bounds at GetIntParameterAvailableOption."), K);
		}
	}
	else
	{
		UE_LOG(LogMutable, Warning, TEXT("Index [%d] out of ParameterProperties bounds at GetIntParameterAvailableOption."), ParamIndex);
	}

	return s_EmptyString;
}


int32 UCustomizableObject::FindParameter(const FString& Name) const
{
	const int32 * Found = ParameterPropertiesLookupTable.Find(Name);
	if (Found == nullptr)
	{
		return INDEX_NONE;
	}
	return *Found;
}


int32 UCustomizableObject::FindIntParameterValue(int32 ParamIndex, const FString& Value) const
{
	int32 MinValueIndex = 0;
	if (ParamIndex >= 0 && ParamIndex < ParameterProperties.Num())
	{
		const TArray<FMutableModelParameterValue>& PossibleValues = ParameterProperties[ParamIndex].PossibleValues;
		if (PossibleValues.Num())
		{
			MinValueIndex = PossibleValues[0].Value;

			for (int32 OrderValue = 0; OrderValue < PossibleValues.Num(); ++OrderValue)
			{
				const FString& Name = PossibleValues[OrderValue].Name;

				if (Name == Value)
				{
					int32 CorrectedValue = OrderValue + MinValueIndex;
					check(PossibleValues[OrderValue].Value == CorrectedValue);
					return CorrectedValue;
				}
			}
		}
		else
		{
			UE_LOG(LogMutable, Warning, TEXT("No possible values for parameter with index [%d] at FindIntParameterValue."), ParamIndex);
		}
	}
	else
	{
		UE_LOG(LogMutable, Warning, TEXT("Index [%d] out of ParameterProperties bounds at FindIntParameterValue."), ParamIndex);
	}
	return MinValueIndex;
}


FString UCustomizableObject::FindIntParameterValueName(int32 ParamIndex, int32 ParamValue) const
{
	if (ParamIndex >= 0 && ParamIndex < ParameterProperties.Num())
	{
		const TArray<FMutableModelParameterValue> & PossibleValues = ParameterProperties[ParamIndex].PossibleValues;

		const int32 MinValueIndex = !PossibleValues.IsEmpty() ? PossibleValues[0].Value : 0;
		ParamValue = ParamValue - MinValueIndex;

		if (PossibleValues.IsValidIndex(ParamValue))
		{
			return PossibleValues[ParamValue].Name;
		}
	}
	else
	{
		UE_LOG(LogMutable, Warning, TEXT("Index [%d] out of ParameterProperties bounds at FindIntParameterValueName."), ParamIndex);
	}

	return FString();
}


FParameterUIData UCustomizableObject::GetParameterUIMetadata(const FString& ParamName) const
{
	const FParameterUIData* ParameterUIData = ParameterUIDataMap.Find(ParamName);

	return ParameterUIData ? *ParameterUIData : FParameterUIData();
}


FParameterUIData UCustomizableObject::GetParameterUIMetadataFromIndex(int32 ParamIndex) const
{
	return GetParameterUIMetadata(GetParameterName(ParamIndex));
}


FParameterUIData UCustomizableObject::GetStateUIMetadata(const FString& StateName) const
{
	const FParameterUIData* StateUIData = StateUIDataMap.Find(StateName);

	return StateUIData ? *StateUIData : FParameterUIData();
}


FParameterUIData UCustomizableObject::GetStateUIMetadataFromIndex(int32 StateIndex) const
{
	return GetStateUIMetadata(GetStateName(StateIndex));
}


void UCustomizableObject::LoadMaskOutCache()
{
	MaskOutCache_HardRef = MaskOutCache.LoadSynchronous();
}


void UCustomizableObject::UnloadMaskOutCache()
{
	MaskOutCache_HardRef = nullptr;
}


void UCustomizableObject::ApplyStateForcedValuesToParameters( int32 State, mu::Parameters* Parameters)
{
	FParameterUIData StateMetaData = GetStateUIMetadataFromIndex(State);
	for (const TPair<FString, FString>& ForcedParameter : StateMetaData.ForcedParameterValues)
	{
		int32 ForcedParameterIndex = FindParameter(ForcedParameter.Key);
		if (ForcedParameterIndex == INDEX_NONE)
		{
			continue;
		}

		bool bIsMultidimensional = Parameters->NewRangeIndex(ForcedParameterIndex).get() != nullptr;
		if (!bIsMultidimensional)
		{
			switch (GetParameterType(ForcedParameterIndex))
			{
			case EMutableParameterType::Int:
			{
				FString StringValue = ForcedParameter.Value;
				if (StringValue.IsNumeric())
				{
					Parameters->SetIntValue(ForcedParameterIndex, FCString::Atoi(*StringValue));
				}
				else
				{
					int32 IntParameterIndex = FindIntParameterValue(ForcedParameterIndex, StringValue);
					Parameters->SetIntValue(ForcedParameterIndex, IntParameterIndex);
				}
				break;
			}
			case EMutableParameterType::Bool:
			{
				Parameters->SetBoolValue(ForcedParameterIndex, ForcedParameter.Value.ToBool());
				break;
			}
			default:
			{
				UE_LOG(LogMutable, Log, TEXT("Forced parameter type not supported."));
				break;
			}
			}
		}
	}

}

#if WITH_EDITOR
void UCustomizableObject::PreSaveRoot(FObjectPreSaveRootContext ObjectSaveContext)
{
	UObject::PreSaveRoot(ObjectSaveContext);

	// Tell the validation system on this object that the validation that is going to be next invoked is due to
	// this asset being saved.
	// This value will be set to false by the validation method on this object so subsequent validation attempts
	// get treated as expected.
	bIsValidationTriggeredBySave = true;
}


EDataValidationResult UCustomizableObject::IsDataValid(FDataValidationContext& Context)
{
	// This method seems to be designed to check data errors (like variables with unexpected values).
	// Currently it does not check if the root that we are compiling has already been compiled during another validation.
	
	EDataValidationResult Result = EDataValidationResult::NotValidated;

	// If validation is invoked by the saving of the asset just skip it. It is too expensive.
	if (bIsValidationTriggeredBySave)
    {
		// Reenable the validation for this object after running the saving process and skipping this validation run
		bIsValidationTriggeredBySave = false;
    	return Result;
    }

	// Skip validation when cooking the assets. The validation of the CO is designed to be used explicitly by the user
	// and not during automated operations like saving or cooking or any other automated action.
	if (IsRunningCommandlet())
	{
		return Result;
	}

	UE_LOG(LogMutable,Display,TEXT("Running data validation checks for %s CO."),*this->GetName());

	// Bind the post validation method to the post validation delegate if not bound already to be able to know when the validation
	// operation (for all assets) concludes
	if (!UCustomizableObject::OnPostCOValidationHandle.IsValid())
	{
		UCustomizableObject::OnPostCOValidationHandle = FEditorDelegates::OnPostAssetValidation.AddStatic(OnPostCOsValidation);	
	}
	
	// Request a compiler to be able to locate the root and to compile it
	const TUniquePtr<FCustomizableObjectCompilerBase> Compiler =
		TUniquePtr<FCustomizableObjectCompilerBase>(UCustomizableObjectSystem::GetInstance()->GetNewCompiler());
	
	// Find out witch is the root for this CO (it may be itself but that is OK)
	UCustomizableObject* RootObject = Compiler->GetRootObject(this);
	check (RootObject);
	
	// Check that the object to be compiled has not already been compiled
	if (UCustomizableObject::AlreadyValidatedRootObjects.Contains(RootObject))
	{
		return Result;
	}

	// Root Object not yet tested -> Proceed with the testing
	
	// Collection of configurations to be tested with the located root object
	constexpr int32 MaxBias = 15;
	TArray<FCompilationOptions> CompilationOptionsToTest;
	for (int32 LodBias = 0; LodBias < MaxBias; LodBias++)
	{
		FCompilationOptions ModifiedCompilationOptions = this->CompileOptions;
		ModifiedCompilationOptions.bForceLargeLODBias = true;	
		ModifiedCompilationOptions.DebugBias = LodBias;	

		// Add one configuration object for each bias setting
		CompilationOptionsToTest.Add(ModifiedCompilationOptions);
	}
	
	// Add current configuration to be tested as well.
	CompilationOptionsToTest.Add(this->CompileOptions);

	
	// Caches with all the data produced by the subsequent compilations of the root of this CO
	TArray<FText> CachedValidationErrors;
	TArray<FText> CachedValidationWarnings;
	TArray<ECustomizableObjectCompilationState> CachedCompilationEndStates;
	
	// Iterate over the compilation options that we want to test and perform the compilation
	for	(const FCompilationOptions& Options : CompilationOptionsToTest)
	{
		// Run Sync compilation -> Warning : Potentially long operation -------------
		Compiler->Compile(*RootObject, Options, false);
		// --------------------------------------------------------------------------
		
		// Get compilation errors and warnings
		TArray<FText> CompilationErrors;
		TArray<FText> CompilationWarnings;
		Compiler->GetCompilationMessages(CompilationWarnings, CompilationErrors);
		
		// Cache the messages returned by the compiler
		for ( const FText& FoundError : CompilationErrors)
		{
			// Add message if not already present
			if (!CachedValidationErrors.ContainsByPredicate([&FoundError](const FText& ArrayEntry)
				{ return FoundError.EqualTo(ArrayEntry);}))
			{
				CachedValidationErrors.Add(FoundError);
			}
		}
		for ( const FText& FoundWarning : CompilationWarnings)
		{
			if (!CachedValidationWarnings.ContainsByPredicate([&FoundWarning](const FText& ArrayEntry)
				{ return FoundWarning.EqualTo(ArrayEntry);}))
			{
				CachedValidationWarnings.Add(FoundWarning);
			}
		}
	
		CachedCompilationEndStates.Add(Compiler->GetCompilationState());
	}
	
	// Cache root object to avoid processing it again when processing another CO related with the same root CO
	AlreadyValidatedRootObjects.Add(RootObject);
	
	// Wrapping up : Fill message output caches and determine if the compilation was successful or not

	// Provide the warning and log messages to the context object (so it can later notify the user using the UI)
	for (const FText& ValidationError : CachedValidationErrors)
	{
		Context.AddError(ValidationError);
	}
	for (const FText& ValidationWarning : CachedValidationWarnings)
	{
		Context.AddWarning(ValidationWarning);
	}
	
	// Return informed guess about what the validation state of this object should be

	// If one or more tests failed to ran then the result must be invalid
	if (CachedCompilationEndStates.Contains(ECustomizableObjectCompilationState::Failed))
	{
		// Early CO compilation error (before starting mutable compilation) -> Output is invalid
		Result = EDataValidationResult::Invalid;
		UE_LOG(LogMutable, Error,
			   TEXT("Compilation of %s failed : Check previous log messages to get more information."),
			   *this->GetName())
	}
	// If it contains invalid states then notify about it too:
	// ECustomizableObjectCompilationState::None would mean the resource is locked (and should not be)
	// ECustomizableObjectCompilationState::InProgress should not be possible since we are compiling synchronously.
	else if (CachedCompilationEndStates.Contains(ECustomizableObjectCompilationState::InProgress) ||
		CachedCompilationEndStates.Contains(ECustomizableObjectCompilationState::None))
	{
		checkNoEntry();
	}
	// All compilations completed successfully
	else 
	{
		// If a warning or error was found then this object failed the validation process
		Result = (CachedValidationWarnings.IsEmpty() && CachedValidationErrors.IsEmpty()) ? EDataValidationResult::Valid : EDataValidationResult::Invalid;
	}
	
	return Result;
}

void UCustomizableObject::OnPostCOsValidation()
{
	// Unbound this method from the validation end delegate
	UCustomizableObject::OnPostCOValidationHandle.Reset();

	// Clear collection with the already processed COs once the validation system has completed its operation
	UCustomizableObject::AlreadyValidatedRootObjects.Empty();
}

#endif


FGuid UCustomizableObject::GetCompilationGuid() const
{
	return CompilationGuid;
}


//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

void FCustomizableObjectPrivateData::SetModel(const TSharedPtr<mu::Model, ESPMode::ThreadSafe>& Model)
{
	MutableModel = Model;
}


const TSharedPtr<mu::Model, ESPMode::ThreadSafe>& FCustomizableObjectPrivateData::GetModel()
{
	return MutableModel;
}


TSharedPtr<const mu::Model, ESPMode::ThreadSafe> FCustomizableObjectPrivateData::GetModel() const
{
	return MutableModel;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

void UCustomizableObjectBulk::PostLoad()
{
	UObject::PostLoad();

	const FString PackageFilename = FPackageName::LongPackageNameToFilename(GetOutermost()->GetName(), TEXT(".uasset"));
	FString OuterPathName = FPaths::GetPath(PackageFilename);

	for(FString& FilePath : BulkDataFileNames)
	{
		FilePath = OuterPathName / FilePath;
	}

}


TArray<IAsyncReadFileHandle*> UCustomizableObjectBulk::GetAsyncReadFileHandles() const
{
	TArray<IAsyncReadFileHandle*> ReadFileHandles;
	ReadFileHandles.Reserve(BulkDataFileNames.Num());

	for (const FString& FilePath : BulkDataFileNames)
	{
		IAsyncReadFileHandle* ReadFileHandle = FPlatformFileManager::Get().GetPlatformFile().OpenAsyncRead(*FilePath);
		
		if (!ReadFileHandle)
		{
			UE_LOG(LogMutable, Error, TEXT("Failed to create AsyncReadFileHandle. File Path [%s]."), *FilePath);
			break;
		}

		ReadFileHandles.Add(ReadFileHandle);
	}

	if (ReadFileHandles.Num() != BulkDataFileNames.Num())
	{
		for (IAsyncReadFileHandle* ReadFileHandle : ReadFileHandles)
		{
			delete ReadFileHandle;
		}

		ReadFileHandles.Empty();
	}

	return ReadFileHandles;
}


#if WITH_EDITOR

void UCustomizableObjectBulk::CookAdditionalFilesOverride(const TCHAR* PackageFilename,
	const ITargetPlatform* TargetPlatform,
	TFunctionRef<void(const TCHAR* Filename, void* Data, int64 Size)> WriteAdditionalFile)
{
	check(CustomizableObject);
	
	FMutableCachedPlatformData* PlatformData = CustomizableObject->CachedPlatformsData.Find(TargetPlatform->PlatformName());
	check(PlatformData);

	// Data to serialize in separate files
	uint8* Data = PlatformData->StreamableData.GetData();

	// Path to the asset
	const FString CookedFilePath = FPaths::GetPath(PackageFilename);

	const uint16 NumBulkDataFiles = BulkDataFilesSize.Num();
	for(uint16 FileIndex = 0; FileIndex < NumBulkDataFiles; ++FileIndex)
	{
		const FString CookedBulkFileName = CookedFilePath / BulkDataFileNames[FileIndex];

		const int64 NumBytes = BulkDataFilesSize[FileIndex];
		WriteAdditionalFile(*CookedBulkFileName, (void*)Data, NumBytes);
		Data += NumBytes;
	}
}


void UCustomizableObjectBulk::PrepareBulkData(UCustomizableObject* InOuter, const ITargetPlatform* TargetPlatform)
{
	CustomizableObject = InOuter;
	check(CustomizableObject);

	BulkDataFilesSize.Empty();
	BulkDataFileNames.Empty();
	
	// Split the Streamable data into several separate files and fix up FileIndex and Offset of each StreamableBlock
	if(TSharedPtr<const mu::Model, ESPMode::ThreadSafe> Model = CustomizableObject->GetModel())
	{
		const uint64 MaxChunkSize = UCustomizableObjectSystem::GetInstance()->GetMaxChunkSizeForPlatform(TargetPlatform);

		const FString BulkFileName = CustomizableObject->GetName() + FString(TEXT("_Bulk"));

		uint16 CurrentFileIndex = 0;
		uint64 CurrentChunkSize = 0;

		const int32 NumStreamingFiles = Model->GetPrivate()->m_program.m_roms.Num();
		for (size_t FileIndex = 0; FileIndex < NumStreamingFiles; ++FileIndex)
		{
			const uint32 ResourceId = Model->GetPrivate()->m_program.m_roms[FileIndex].Id;

			FMutableStreamableBlock& StreamableBlock = CustomizableObject->HashToStreamableBlock[ResourceId];

			const uint32 BlockSize = StreamableBlock.Size;
			if(CurrentChunkSize + BlockSize > MaxChunkSize)
			{
				BulkDataFileNames.Add(BulkFileName + FString::Printf(TEXT("%d.mut"), CurrentFileIndex));

				if(CurrentChunkSize == 0)
				{
					BulkDataFilesSize.Add(BlockSize);
					StreamableBlock.FileIndex = CurrentFileIndex++;
					StreamableBlock.Offset = 0;
					continue;
				}

				BulkDataFilesSize.Add(CurrentChunkSize);
			
				CurrentFileIndex++;
				CurrentChunkSize = 0;
			}

			StreamableBlock.FileIndex = CurrentFileIndex;
			StreamableBlock.Offset = CurrentChunkSize;
			CurrentChunkSize += BlockSize;			
		}

		BulkDataFilesSize.Add(CurrentChunkSize);
		BulkDataFileNames.Add(BulkFileName + FString::Printf(TEXT("%d.mut"), CurrentFileIndex));
	}
}

#endif

#undef LOCTEXT_NAMESPACE
