// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCO/CustomizableObject.h"

#include "Algo/Copy.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Async/AsyncFileHandle.h"
#include "EdGraph/EdGraph.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimInstance.h"
#include "Animation/Skeleton.h"
#include "Engine/AssetUserData.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Input/Reply.h"
#include "Interfaces/ITargetPlatform.h"
#include "Materials/MaterialInterface.h"
#include "Misc/DataValidation.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "MuCO/CustomizableObjectInstance.h"
#include "MuCO/CustomizableObjectInstancePrivate.h"
#include "MuCO/CustomizableObjectPrivate.h"
#include "MuCO/CustomizableObjectSystem.h"
#include "MuCO/CustomizableObjectSystemPrivate.h"
#include "MuCO/CustomizableObjectUIData.h"
#include "MuCO/ICustomizableObjectModule.h"
#include "MuCO/MutableProjectorTypeUtils.h"
#include "MuCO/UnrealMutableModelDiskStreamer.h"
#include "MuCO/UnrealPortabilityHelpers.h"
#include "MuR/Model.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "UObject/AssetRegistryTagsContext.h"
#include "UObject/ObjectSaveContext.h"
#include "MuCO/ICustomizableObjectEditorModule.h"
#include "MuCO/CustomizableObjectSystemPrivate.h"

#if WITH_EDITOR
#include "Editor.h"
#endif


#include UE_INLINE_GENERATED_CPP_BY_NAME(CustomizableObject)

#define LOCTEXT_NAMESPACE "CustomizableObject"

DEFINE_LOG_CATEGORY(LogMutable);

#if WITH_EDITORONLY_DATA

namespace UE::Mutable::Private
{

template <typename T>
T* MoveOldObjectAndCreateNew(UClass* Class, UObject* InOuter)
{
	FName ObjectFName = Class->GetFName();
	FString ObjectNameStr = ObjectFName.ToString();
	UObject* Existing = FindObject<UAssetUserData>(InOuter, *ObjectNameStr);
	if (Existing)
	{
		// Move the old object out of the way
		Existing->Rename(nullptr /* Rename will pick a free name*/, GetTransientPackage(), REN_DontCreateRedirectors);
	}
	return NewObject<T>(InOuter, Class, *ObjectNameStr);
}

}

#endif

//-------------------------------------------------------------------------------------------------

UCustomizableObject::UCustomizableObject()
	: UObject()
{
	Private = CreateDefaultSubobject<UCustomizableObjectPrivate>(FName("Private"));

#if WITH_EDITORONLY_DATA
	const FString CVarName = TEXT("r.SkeletalMesh.MinLodQualityLevel");
	const FString ScalabilitySectionName = TEXT("ViewDistanceQuality");
	LODSettings.MinQualityLevelLOD.SetQualityLevelCVarForCooking(*CVarName, *ScalabilitySectionName);
#endif
}


#if WITH_EDITOR
bool UCustomizableObject::IsEditorOnly() const
{
	return bIsChildObject;
}


void UCustomizableObjectPrivate::UpdateVersionId()
{
	GetPublic()->VersionId = FGuid::NewGuid();
}


FGuid UCustomizableObjectPrivate::GetVersionId() const
{
	return GetPublic()->VersionId;
}


void UCustomizableObject::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS;
	Super::GetAssetRegistryTags(OutTags);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS;
}

void UCustomizableObject::GetAssetRegistryTags(FAssetRegistryTagsContext Context) const
{
	int32 isRoot = 0;

	FCustomizableObjectCompilerBase* Compiler = UCustomizableObjectSystem::GetInstance()->GetNewCompiler();
	if (Compiler)
	{
		isRoot = Compiler->IsRootObject(this) ? 1 : 0;
		delete Compiler;
	}

	Context.AddTag(FAssetRegistryTag("IsRoot", FString::FromInt(isRoot), FAssetRegistryTag::TT_Numerical));
	Super::GetAssetRegistryTags(Context);
}


void UCustomizableObject::PreSave(FObjectPreSaveContext ObjectSaveContext)
{
	Super::PreSave(ObjectSaveContext);

	// Update the derived child object flag
	if (GetPrivate()->TryUpdateIsChildObject())
	{
		if (bIsChildObject)
		{
			GetPackage()->SetPackageFlags(PKG_EditorOnly);
		}
		else
		{
			GetPackage()->ClearPackageFlags(PKG_EditorOnly);
		}
	}

	if (ObjectSaveContext.IsCooking() && !bIsChildObject)
	{
		const ITargetPlatform* TargetPlatform = ObjectSaveContext.GetTargetPlatform();
		// Load cached data before saving
		if (GetPrivate()->TryLoadCompiledCookDataForPlatform(TargetPlatform))
		{
			// Create an export object to manage the streamable data
			if (!BulkData)
			{
				BulkData = UE::Mutable::Private::MoveOldObjectAndCreateNew<UCustomizableObjectBulk>(
					UCustomizableObjectBulk::StaticClass(), this);
			}
			BulkData->Mark(OBJECTMARK_TagExp);

			// Split streamable data into smaller chunks and fix up the CO HashToStreamableBlock's FileIndex and Offset
			BulkData->PrepareBulkData(this, ObjectSaveContext.GetTargetPlatform());
		}
		else
		{
			UE_LOG(LogMutable, Warning, TEXT("Cook: Customizable Object [%s] is missing [%s] platform data."), *GetName(),
				*ObjectSaveContext.GetTargetPlatform()->PlatformName());
			
			GetPrivate()->ClearCompiledData(true);
		}
	}
}


void UCustomizableObject::PostSaveRoot(FObjectPostSaveRootContext ObjectSaveContext)
{
	MUTABLE_CPUPROFILER_SCOPE(UCustomizableObject::PostSaveRoot);

	Super::PostSaveRoot(ObjectSaveContext);

	if (ObjectSaveContext.IsCooking())
	{
		// Free cached data after saving;
		const ITargetPlatform* TargetPlatform = ObjectSaveContext.GetTargetPlatform();
		GetPrivate()->CachedPlatformsData.Remove(TargetPlatform->PlatformName());
	}
}


bool UCustomizableObjectPrivate::TryUpdateIsChildObject()
{
	FCustomizableObjectCompilerBase* Compiler = UCustomizableObjectSystem::GetInstance()->GetNewCompiler();
	if (Compiler)
	{
		GetPublic()->bIsChildObject = !Compiler->IsRootObject(GetPublic());
		delete Compiler;
		return true;
	}
	else
	{
		return false;
	}
}


bool UCustomizableObject::IsChildObject() const
{
	return bIsChildObject;
}


void UCustomizableObjectPrivate::SetIsChildObject(bool bIsChildObject)
{
	GetPublic()->bIsChildObject = bIsChildObject;
}


#if WITH_EDITOR
bool UCustomizableObjectPrivate::TryLoadCompiledCookDataForPlatform(const ITargetPlatform* TargetPlatform)
{
	const FMutableCachedPlatformData* PlatformData = CachedPlatformsData.Find(TargetPlatform->PlatformName());
	if (!PlatformData)
	{
		return false;
	}

	FMemoryReaderView MemoryReader(PlatformData->ModelData);
	LoadCompiledData(MemoryReader, TargetPlatform, true);
	return true;
}
#endif

#endif // End WITH_EDITOR


void UCustomizableObject::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	if (ReferenceSkeletalMesh_DEPRECATED)
	{
		ReferenceSkeletalMeshes.Add(ReferenceSkeletalMesh_DEPRECATED);
		ReferenceSkeletalMesh_DEPRECATED = nullptr;
	}

	// Register to dirty delegate so we update derived data version ID each time that the package is marked as dirty.
	if (UPackage* Package = GetOutermost())
	{
		Package->PackageMarkedDirtyEvent.AddWeakLambda(this, [this](UPackage* Pkg, bool bWasDirty)
			{
				if (GetPackage() == Pkg)
				{
					GetPrivate()->UpdateVersionId();
				}
			});
	}

	const int32 CustomizableObjectCustomVersion = GetLinkerCustomVersion(FCustomizableObjectCustomVersion::GUID);

	if (!IsRunningCookCommandlet())
	{
		GetPrivate()->Status.NextState(FCustomizableObjectStatusTypes::EState::Loading);

		ITargetPlatformManagerModule& TargetPlatformManager = GetTargetPlatformManagerRef();
		const ITargetPlatform* RunningPlatform = TargetPlatformManager.GetRunningTargetPlatform();

		const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		if (AssetRegistryModule.Get().IsLoadingAssets())
		{
			AssetRegistryModule.Get().OnFilesLoaded().AddUObject(GetPrivate(), &UCustomizableObjectPrivate::LoadCompiledDataFromDisk);
		}
		else
		{
			GetPrivate()->LoadCompiledDataFromDisk();
		}
	}
#endif
}


bool UCustomizableObjectPrivate::IsLocked() const
{
	return bLocked;
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
			GetPrivate()->SaveEmbeddedData(Ar_Asset);
		}
	}
	else
	{
		// Can't remove this or saved customizable objects will fail to load
		int64 InternalVersion = UCustomizableObjectPrivate::CurrentSupportedVersion;
		Ar_Asset << InternalVersion;
	}
#else
	if (Ar_Asset.IsLoading())
	{
		GetPrivate()->LoadEmbeddedData(Ar_Asset);
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


void UCustomizableObject::BeginCacheForCookedPlatformData(const ITargetPlatform* TargetPlatform)
{
	if (!TargetPlatform)
	{
		return;
	}

	if (Private->CachedPlatformNames.Find(TargetPlatform->PlatformName()) == INDEX_NONE)
	{
		// Compile and save in the CachedPlatformsData map
		GetPrivate()->CompileForTargetPlatform(TargetPlatform);
		GetPrivate()->CachedPlatformNames.Add(TargetPlatform->PlatformName());
	}
}


bool UCustomizableObject::IsCachedCookedPlatformDataLoaded( const ITargetPlatform* TargetPlatform ) 
{
	return !TargetPlatform || Private->CachedPlatformNames.Find(TargetPlatform->PlatformName()) != INDEX_NONE;
}


FGuid GenerateIdentifier(const UCustomizableObject& CustomizableObject)
{
	// Generate the Identifier using the path and name of the asset
	uint32 FullPathHash = GetTypeHash(CustomizableObject.GetFullName());
	uint32 OutermostHash = GetTypeHash(GetNameSafe(CustomizableObject.GetOutermost()));
	uint32 OuterHash = GetTypeHash(CustomizableObject.GetName());
	return FGuid(0, FullPathHash, OutermostHash, OuterHash);
}


void UCustomizableObjectPrivate::ClearCompiledData(bool bIsCooking)
{
	GetModelResources(bIsCooking) = FModelResources();

	GetPublic()->ClothMeshToMeshVertData.Empty();
	GetPublic()->ContributingClothingAssetsData.Empty();
	GetPublic()->ClothSharedConfigsData.Empty();

#if WITH_EDITORONLY_DATA
	CustomizableObjectPathMap.Empty();
	GroupNodeMap.Empty();
	ParticipatingObjects.Empty();
#endif

	GetPublic()->BulkData = nullptr;
}


void SerializeStreamedResources(FArchive& Ar, UObject* Object, TArray<FCustomizableObjectStreamedResourceData>& StreamedResources, bool bIsCooking)
{
	if (Ar.IsSaving())
	{
		int32 NumStreamedResources = StreamedResources.Num();
		Ar << NumStreamedResources;

		for (const FCustomizableObjectStreamedResourceData& ResourceData : StreamedResources)
		{
			const FCustomizableObjectResourceData& Data = ResourceData.GetLoadedData();
			uint32 ResourceDataType = (uint32)Data.Type;
			Ar << ResourceDataType;

			switch (Data.Type)
			{
			case ECOResourceDataType::AssetUserData:
			{
				const FCustomizableObjectAssetUserData* AssetUserData = Data.Data.GetPtr<FCustomizableObjectAssetUserData>();
				FString AssetUserDataPath;

				if (AssetUserData->AssetUserDataEditor)
				{
					AssetUserDataPath = TSoftObjectPtr<UAssetUserData>(AssetUserData->AssetUserDataEditor).ToString();
				}

				Ar << AssetUserDataPath;
				break;
			}
			default:
				check(false);
				break;
			}
		}
	}
	else 
	{
		const FString CustomizableObjectName = GetNameSafe(Object) + TEXT("_");

		int32 NumStreamedResources = 0;
		Ar << NumStreamedResources;

		StreamedResources.SetNum(NumStreamedResources);

		for (int32 ResourceIndex = 0; ResourceIndex < NumStreamedResources; ++ResourceIndex)
		{
			// Override existing containers
			UCustomizableObjectResourceDataContainer* Container = StreamedResources[ResourceIndex].GetPath().Get();

			// Create a new container if none.
			if (!Container)
			{
				// Generate a deterministic name to help with deterministic cooking
				const FString ContainerName = CustomizableObjectName + FString::Printf(TEXT("SR_%d"), ResourceIndex);

				UCustomizableObjectResourceDataContainer* ExistingContainer = FindObject<UCustomizableObjectResourceDataContainer>(Object, *ContainerName);
				Container = ExistingContainer ? ExistingContainer : NewObject<UCustomizableObjectResourceDataContainer>(
					Object,
					FName(*ContainerName),
					RF_Public);

				StreamedResources[ResourceIndex] = { Container };
			}

			check(Container);
			uint32 Type = 0;
			Ar << Type;
			
			Container->Data.Type = (ECOResourceDataType)Type;
			switch (Container->Data.Type)
			{
				case ECOResourceDataType::AssetUserData:
				{
					FString AssetUserDataPath;
					Ar << AssetUserDataPath;
					
					FCustomizableObjectAssetUserData ResourceData;

					TSoftObjectPtr<UAssetUserData> SoftAssetUserData(AssetUserDataPath);
					ResourceData.AssetUserDataEditor = !SoftAssetUserData.IsNull() ? SoftAssetUserData.LoadSynchronous() : nullptr;

					if (!ResourceData.AssetUserDataEditor)
					{
						UE_LOG(LogMutable, Warning, TEXT("Failed to load streamed resource of type AssetUserData. Resource name: [%s]"), *AssetUserDataPath);
					}

					if (bIsCooking)
					{
						// Rename the asset user data for duplicate
						const FString AssetName = CustomizableObjectName + GetNameSafe(ResourceData.AssetUserDataEditor);
						
						// Find or duplicate the AUD replacing the outer
						ResourceData.AssetUserData = FindObject<UAssetUserData>(Container, *AssetName);
						if (!ResourceData.AssetUserData)
						{
							// AUD may be private objects within meshes. Duplicate changing the outer to avoid including meshes into the builds.
							ResourceData.AssetUserData = DuplicateObject<UAssetUserData>(ResourceData.AssetUserDataEditor, Container, FName(*AssetName));
						}
					}

					Container->Data.Data = FInstancedStruct::Make(ResourceData);
					break;
				}
				default:
					check(false);
					break;
			}
		}
	}
}


void UCustomizableObjectPrivate::SaveCompiledData(FArchive& MemoryWriter, bool bIsCooking)
{
	int32 InternalVersion = UCustomizableObjectPrivate::CurrentSupportedVersion;
	MutableCompiledDataStreamHeader Header(InternalVersion, GetVersionId());
	MemoryWriter << Header;

	FModelResources& LocalModelResources = GetModelResources(false);

	MemoryWriter << LocalModelResources.ReferenceSkeletalMeshesData;

	SerializeStreamedResources(MemoryWriter, GetPublic(), GetPublic()->StreamedResourceData, bIsCooking);

	int32 NumReferencedMaterials = LocalModelResources.Materials.Num();
	MemoryWriter << NumReferencedMaterials;

	for (const TSoftObjectPtr<UMaterialInterface>& Material : LocalModelResources.Materials)
	{
		FString StringRef = Material.ToString();
		MemoryWriter << StringRef;
	}

	int32 NumReferencedSkeletons = LocalModelResources.Skeletons.Num();
	MemoryWriter << NumReferencedSkeletons;

	for (const TSoftObjectPtr<USkeleton>& Skeleton : LocalModelResources.Skeletons)
	{
		FString StringRef = Skeleton.ToString();
		MemoryWriter << StringRef;
	}

	int32 NumPassthroughTextures = LocalModelResources.PassThroughTextures.Num();
	MemoryWriter << NumPassthroughTextures;

	for (const TSoftObjectPtr<UTexture>& PassthroughTexture : LocalModelResources.PassThroughTextures)
	{
		FString StringRef = PassthroughTexture.ToString();
		MemoryWriter << StringRef;
	}

	int32 NumPhysicsAssets = LocalModelResources.PhysicsAssets.Num();
	MemoryWriter << NumPhysicsAssets;

	for (const TSoftObjectPtr<UPhysicsAsset>& PhysicsAsset : LocalModelResources.PhysicsAssets)
	{
		FString StringRef = PhysicsAsset.ToString();
		MemoryWriter << StringRef;
	}

	int32 NumAnimBps = LocalModelResources.AnimBPs.Num();
	MemoryWriter << NumAnimBps;

	for (const TSoftClassPtr<UAnimInstance>& AnimBp : LocalModelResources.AnimBPs)
	{
		FString StringRef = AnimBp.ToString();
		MemoryWriter << StringRef;
	}

	MemoryWriter << LocalModelResources.AnimBpOverridePhysiscAssetsInfo;

	MemoryWriter << LocalModelResources.MaterialSlotNames;
	MemoryWriter << LocalModelResources.BoneNames;
	MemoryWriter << LocalModelResources.SocketArray;

	MemoryWriter << LocalModelResources.SkinWeightProfilesInfo;

	MemoryWriter << LocalModelResources.ImageProperties;
	MemoryWriter << LocalModelResources.ParameterUIDataMap;
	MemoryWriter << LocalModelResources.StateUIDataMap;

	MemoryWriter << LocalModelResources.RealTimeMorphTargetNames;
	MemoryWriter << LocalModelResources.RealTimeMorphStreamableBlocks;

	MemoryWriter << LocalModelResources.HashToStreamableBlock;

	MemoryWriter << GetPublic()->ClothMeshToMeshVertData;
	MemoryWriter << GetPublic()->ContributingClothingAssetsData;
	MemoryWriter << GetPublic()->ClothSharedConfigsData; 

	MemoryWriter << LocalModelResources.NumComponents;
	MemoryWriter << LocalModelResources.NumLODs;
	MemoryWriter << LocalModelResources.NumLODsToStream;
	MemoryWriter << LocalModelResources.FirstLODAvailable;

	// Editor Only data
	MemoryWriter << bDisableTextureStreaming;
	MemoryWriter << bIsCompiledWithoutOptimization;
	MemoryWriter << CustomizableObjectPathMap;
	MemoryWriter << GroupNodeMap;
	MemoryWriter << ParticipatingObjects;

	if (!bIsCooking)
	{
		MemoryWriter << LocalModelResources.EditorOnlyMorphTargetReconstructionData;
	}
}

void UCustomizableObjectPrivate::LoadCompiledData(FArchive& MemoryReader, const ITargetPlatform* InTargetPlatform, bool bIsCooking)
{
	TSharedPtr<mu::Model, ESPMode::ThreadSafe> LoadedModel;
	ClearCompiledData(bIsCooking);

	MutableCompiledDataStreamHeader Header;
	MemoryReader << Header;

	if (UCustomizableObjectPrivate::CurrentSupportedVersion == Header.InternalVersion)
	{
		// Make sure mutable has been initialised.
		UCustomizableObjectSystem::GetInstance();

		FModelResources& LocalModelResource = GetModelResources(bIsCooking);
		LocalModelResource = FModelResources();

		MemoryReader << LocalModelResource.ReferenceSkeletalMeshesData;

		SerializeStreamedResources(MemoryReader, GetPublic(), GetPublic()->StreamedResourceData, bIsCooking);

		// Initialize resources. 
		for(FMutableRefSkeletalMeshData& ReferenceSkeletalMeshData : LocalModelResource.ReferenceSkeletalMeshesData)
		{
			ReferenceSkeletalMeshData.InitResources(GetPublic(), InTargetPlatform);
		}

		int32 NumReferencedMaterials = 0;
		MemoryReader << NumReferencedMaterials;

		for (int32 i = 0; i < NumReferencedMaterials; ++i)
		{
			FString StringRef;
			MemoryReader << StringRef;

			LocalModelResource.Materials.Add(TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(StringRef)));
		}

		int32 NumReferencedSkeletons = 0;
		MemoryReader << NumReferencedSkeletons;

		for (int32 SkeletonIndex = 0; SkeletonIndex < NumReferencedSkeletons; ++SkeletonIndex)
		{
			FString StringRef;
			MemoryReader << StringRef;

			LocalModelResource.Skeletons.Add(TSoftObjectPtr<USkeleton>(FSoftObjectPath(StringRef)));
		}

		int32 NumPassthroughTextures = 0;
		MemoryReader << NumPassthroughTextures;

		for (int32 Index = 0; Index < NumPassthroughTextures; ++Index)
		{
			FString StringRef;
			MemoryReader << StringRef;

			LocalModelResource.PassThroughTextures.Add(TSoftObjectPtr<UTexture>(FSoftObjectPath(StringRef)));
		}

		int32 NumPhysicsAssets = 0;
		MemoryReader << NumPhysicsAssets;

		for (int32 i = 0; i < NumPhysicsAssets; ++i)
		{
			FString StringRef;
			MemoryReader << StringRef;

			LocalModelResource.PhysicsAssets.Add(TSoftObjectPtr<UPhysicsAsset>(FSoftObjectPath(StringRef)));
		}


		int32 NumAnimBps = 0;
		MemoryReader << NumAnimBps;

		for (int32 Index = 0; Index < NumAnimBps; ++Index)
		{
			FString StringRef;
			MemoryReader << StringRef;

			LocalModelResource.AnimBPs.Add(TSoftClassPtr<UAnimInstance>(StringRef));
		}

		MemoryReader << LocalModelResource.AnimBpOverridePhysiscAssetsInfo;

		MemoryReader << LocalModelResource.MaterialSlotNames;
		MemoryReader << LocalModelResource.BoneNames;
		MemoryReader << LocalModelResource.SocketArray;

		MemoryReader << LocalModelResource.SkinWeightProfilesInfo;

		MemoryReader << LocalModelResource.ImageProperties;
		MemoryReader << LocalModelResource.ParameterUIDataMap;
		MemoryReader << LocalModelResource.StateUIDataMap;

		MemoryReader << LocalModelResource.RealTimeMorphTargetNames;
		MemoryReader << LocalModelResource.RealTimeMorphStreamableBlocks;

		MemoryReader << LocalModelResource.HashToStreamableBlock;

		MemoryReader << GetPublic()->ClothMeshToMeshVertData;
		MemoryReader << GetPublic()->ContributingClothingAssetsData;
		MemoryReader << GetPublic()->ClothSharedConfigsData;

		MemoryReader << LocalModelResource.NumComponents;
		MemoryReader << LocalModelResource.NumLODs;
		MemoryReader << LocalModelResource.NumLODsToStream;
		MemoryReader << LocalModelResource.FirstLODAvailable;

		bool bInvalidateModel = false;

		// Editor Only data
		{
			MemoryReader << bDisableTextureStreaming;
			MemoryReader << bIsCompiledWithoutOptimization;
			MemoryReader << CustomizableObjectPathMap;
			MemoryReader << GroupNodeMap;
			MemoryReader << ParticipatingObjects;

			if (!bIsCooking)
			{
				MemoryReader << LocalModelResource.EditorOnlyMorphTargetReconstructionData;
				
				DirtyParticipatingObjects.Empty();

				TArray<FName> OutOfDatePackages;
				bInvalidateModel = IsCompilationOutOfDate(&OutOfDatePackages);

				if (bInvalidateModel)
				{
					UE_LOG(LogMutable, Display, TEXT("Invalidating compiled data due to changes in %s."), *OutOfDatePackages[0].ToString());
				}
			}
		}

		bool bModelSerialized = false;
		MemoryReader << bModelSerialized;

		if (bModelSerialized && !bInvalidateModel)
		{
			UnrealMutableInputStream Stream(MemoryReader);
			mu::InputArchive Arch(&Stream);
			LoadedModel = mu::Model::StaticUnserialise(Arch);
		}
	}
	
	UpdateParameterPropertiesFromModel(LoadedModel);
	SetModel(LoadedModel, GenerateIdentifier(*GetPublic()));
}


void UCustomizableObjectPrivate::LoadCompiledDataFromDisk()
{
	ITargetPlatformManagerModule& TargetPlatformManager = GetTargetPlatformManagerRef();
	const ITargetPlatform* RunningPlatform = TargetPlatformManager.GetRunningTargetPlatform();
	check(RunningPlatform);

	// Compose Folder Name
	const FString FolderPath = GetCompiledDataFolderPath();

	// Compose File Names
	const FString ModelFileName = FolderPath + GetCompiledDataFileName(true, RunningPlatform);
	const FString StreamableFileName = FolderPath + GetCompiledDataFileName(false, RunningPlatform);

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

		if (CompiledDataHeader.InternalVersion == UCustomizableObjectPrivate::CurrentSupportedVersion
			&&
			CompiledDataHeader.InternalVersion == StreamableDataHeader.InternalVersion 
			&&
			CompiledDataHeader.VersionId == StreamableDataHeader.VersionId)
		{
			if (IsRunningGame() || CompiledDataHeader.VersionId == GetVersionId())
			{ 
				int64 CompiledDataSize = CompiledDataFileHandle->Size() - HeaderSize;
				TArray64<uint8> CompiledDataBytes;
				CompiledDataBytes.SetNumUninitialized(CompiledDataSize);

				CompiledDataFileHandle->Seek(HeaderSize);
				CompiledDataFileHandle->Read(CompiledDataBytes.GetData(), CompiledDataSize);

				FMemoryReaderView MemoryReader(CompiledDataBytes);
				LoadCompiledData(MemoryReader, RunningPlatform);
			}
		}

		delete CompiledDataFileHandle;
		delete StreamableDataFileHandle;
	}

	if (!GetModel()) // Not failed to load the model
	{
		Status.NextState(FCustomizableObjectStatusTypes::EState::NoModel);
	}
}


void UCustomizableObjectPrivate::CachePlatformData(const ITargetPlatform* InTargetPlatform, TArray64<uint8>& InModelBytes, TArray64<uint8>& InBulkBytes, TArray64<uint8>& InMorphBytes)
{
	MUTABLE_CPUPROFILER_SCOPE(CachePlatformData)

	FString PlatformName = InTargetPlatform ? InTargetPlatform->PlatformName() : FPlatformProperties::PlatformName();

	check(!CachedPlatformsData.Find(PlatformName));

	FMutableCachedPlatformData& Data = CachedPlatformsData.Add(PlatformName);

	// Cache CO data and mu::Model
	FMemoryWriter64 MemoryWriter(Data.ModelData);
	SaveCompiledData(MemoryWriter, true);
	Data.ModelData.Append(InModelBytes);

	// Cache streamable bulk data
	Data.StreamableData = MoveTemp(InBulkBytes);
	Data.MorphData = MoveTemp(InMorphBytes);
}


void UCustomizableObjectPrivate::CompileForTargetPlatform(const ITargetPlatform* TargetPlatform)
{
	if (!TargetPlatform)
	{
		return;
	}

	FCustomizableObjectCompilerBase* Compiler = UCustomizableObjectSystem::GetInstance()->GetNewCompiler();

	const bool bIsRootObject = Compiler->IsRootObject(GetPublic());
	if (bIsRootObject)
	{
		FCompilationOptions Options;
		Options.OptimizationLevel = UE_MUTABLE_MAX_OPTIMIZATION;	// max optimization when packaging.
		Options.TextureCompression = ECustomizableObjectTextureCompression::HighQuality;
		Options.bIsCooking = true;
		Options.TargetPlatform = TargetPlatform;
		Options.CustomizableObjectNumBoneInfluences = ICustomizableObjectModule::Get().GetNumBoneInfluences();

		Compiler->Compile(*GetPublic(), Options, false);
	}

	delete Compiler;
}


bool UCustomizableObject::ConditionalAutoCompile()
{
	check(IsInGameThread());

	// Don't compile objects being compiled
	if (GetPrivate()->IsLocked())
	{
		return false;
	}

	// Don't compile compiled objects
	if (IsCompiled())
	{
		return true;
	}

	// Model has not loaded yet
	if (GetPrivate()->Status.Get() == FCustomizableObjectStatusTypes::EState::Loading)
	{
		return false;
	}

	UCustomizableObjectSystem* System = UCustomizableObjectSystem::GetInstance();
	if (!System || !System->IsValidLowLevel() || System->HasAnyFlags(RF_BeginDestroyed))
	{
		return false;
	}

	// Don't re-compile objects if they failed to compile. 
	if (GetPrivate()->CompilationState == ECustomizableObjectCompilationState::Failed)
	{
		return false;
	}

	// By default, don't compile in a commandlet.
	// Notice that the cook is also a commandlet. Do not add a warning/error, otherwise we could end up invalidating the cook for no reason.
	if (IsRunningCookCommandlet() || (IsRunningCommandlet() && !System->IsAutoCompileCommandletEnabled()))
	{
		return false;
	}

	// Don't compile if we're running game or if Mutable or AutoCompile is disabled.
	if (IsRunningGame() || !System->IsActive() || !System->IsAutoCompileEnabled())
	{
		System->AddUncompiledCOWarning(*this);
		return false;
	}

	// Sync/Async compilation
	if (System->IsAutoCompilationSync())
	{
		FCustomizableObjectCompilerBase* Compiler = UCustomizableObjectSystem::GetInstance()->GetNewCompiler();
		Compiler->Compile(*this, this->CompileOptions, false);
		delete Compiler;
	}
	else
	{
		const FAssetRegistryModule& AssetRegistryModule = FModuleManager::GetModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		const FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(UE_MUTABLE_OBJECTPATH(GetPathName()));
		System->RecompileCustomizableObjectAsync(AssetData, this);
	}

	return IsCompiled();
}


FReply UCustomizableObjectPrivate::AddNewParameterProfile(FString Name, UCustomizableObjectInstance& CustomInstance)
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
		FProfileParameterDat* Found = GetPublic()->InstancePropertiesProfiles.FindByPredicate(
			[&ProfileName](const FProfileParameterDat& Profile) { return Profile.ProfileName == ProfileName; });

		bUniqueNameFound = static_cast<bool>(!Found);
		if (Found)
		{
			ProfileName = Name + FString::FromInt(Suffix);
			++Suffix;
		}
	}

	int32 ProfileIndex = GetPublic()->InstancePropertiesProfiles.Emplace();

	GetPublic()->InstancePropertiesProfiles[ProfileIndex].ProfileName = ProfileName;
	CustomInstance.GetPrivate()->SaveParametersToProfile(ProfileIndex);

	Modify();

	return FReply::Handled();
}


FString UCustomizableObjectPrivate::GetCompiledDataFolderPath() const
{	
	return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() + TEXT("MutableStreamedDataEditor/"));
}


FString UCustomizableObjectPrivate::GetCompiledDataFileName(bool bIsModel, const ITargetPlatform* InTargetPlatform, bool bIsDiskStreamer)
{
	const FString PlatformName = InTargetPlatform ? InTargetPlatform->PlatformName() : FPlatformProperties::PlatformName();
	const FString FileIdentifier = bIsDiskStreamer ? Identifier.ToString() : GenerateIdentifier(*GetPublic()).ToString();
	const FString Extension = bIsModel ? TEXT("_M.mut") : TEXT("_S.mut");
	return PlatformName + FileIdentifier + Extension;
}


FString UCustomizableObject::GetDesc()
{
	int32 States = GetStateCount();
	int32 Params = GetParameterCount();
	return FString::Printf(TEXT("%d States, %d Parameters"), States, Params);
}


void UCustomizableObjectPrivate::SaveEmbeddedData(FArchive& Ar)
{
	UE_LOG(LogMutable, Verbose, TEXT("Saving embedded data for Customizable Object [%s] now at position %d."), *GetName(), int(Ar.Tell()));

	int32 InternalVersion = GetModel() ? CurrentSupportedVersion : -1;
	Ar << InternalVersion;

	if (GetModel())
	{	
		{
			Ar << GetPublic()->ClothMeshToMeshVertData;
			Ar << GetPublic()->ContributingClothingAssetsData;
			Ar << GetPublic()->ClothSharedConfigsData;
		}

		// Serialise the entire model, but unload the streamable data first.
		{
			GetModel()->UnloadExternalData();

			UnrealMutableOutputStream Stream(Ar);
			mu::OutputArchive Arch(&Stream);
			mu::Model::Serialise(GetModel().Get(), Arch);
		}

		UE_LOG(LogMutable, Verbose, TEXT("Saved embedded data for Customizable Object [%s] now at position %d."), *GetName(), int(Ar.Tell()));
	}
}

#endif // End WITH_EDITOR 

void UCustomizableObjectPrivate::LoadEmbeddedData(FArchive& Ar)
{
	MUTABLE_CPUPROFILER_SCOPE(UCustomizableObject::LoadEmbeddedData)

	int32 InternalVersion;
	Ar << InternalVersion;

	// If this fails, something went wrong with the packaging: we have data that belongs
	// to a different version than the code.
	check(CurrentSupportedVersion == InternalVersion);

	if(CurrentSupportedVersion == InternalVersion)
	{	
		{
			Ar << GetPublic()->ClothMeshToMeshVertData;
			Ar << GetPublic()->ContributingClothingAssetsData;
			Ar << GetPublic()->ClothSharedConfigsData;
		}
		
		// Load model
		UnrealMutableInputStream Stream(Ar);
		mu::InputArchive Arch(&Stream);
		TSharedPtr<mu::Model, ESPMode::ThreadSafe> Model = mu::Model::StaticUnserialise(Arch);

		// Create parameter properties
		UpdateParameterPropertiesFromModel(Model);

		SetModel(Model, FGuid());
	}
}


UCustomizableObjectPrivate* UCustomizableObject::GetPrivate() const
{
	check(Private);
	return Private;
}


bool UCustomizableObject::IsCompiled() const
{
#if WITH_EDITOR
	const bool bIsCompiled = Private->GetModel() != nullptr && Private->GetModel()->IsValid();
#else
	const bool bIsCompiled = Private->GetModel() != nullptr;
#endif

	return bIsCompiled;
}


void UCustomizableObjectPrivate::AddUncompiledCOWarning(const FString& AdditionalLoggingInfo)
{
	// Send a warning (on-screen notification, log error, and in-editor notification)
	UCustomizableObjectSystem* System = UCustomizableObjectSystem::GetInstance();
	if (!System || !System->IsValidLowLevel() || System->HasAnyFlags(RF_BeginDestroyed))
	{
		return;
	}

	System->AddUncompiledCOWarning(*GetPublic(), &AdditionalLoggingInfo);
}


USkeletalMesh* UCustomizableObject::GetRefSkeletalMesh(int32 ComponentIndex) const
{
#if WITH_EDITORONLY_DATA
	if (ReferenceSkeletalMeshes.IsValidIndex(ComponentIndex))
	{
		return ReferenceSkeletalMeshes[ComponentIndex];
	}
#else
	const FModelResources& ModelResources = Private->GetModelResources();
	if (ModelResources.ReferenceSkeletalMeshesData.IsValidIndex(ComponentIndex))
	{
		// Can be nullptr if RefSkeletalMeshes are not loaded yet.
		return ModelResources.ReferenceSkeletalMeshesData[ComponentIndex].SkeletalMesh;
	}
#endif
	return nullptr;
}


int32 UCustomizableObject::FindState( const FString& Name ) const
{
	int32 Result = -1;
	if (Private->GetModel())
	{
		Result = Private->GetModel()->FindState(Name);
	}

	return Result;
}


int32 UCustomizableObject::GetStateCount() const
{
	int32 Result = 0;

	if (Private->GetModel())
	{
		Result = Private->GetModel()->GetStateCount();
	}

	return Result;
}


FString UCustomizableObject::GetStateName(int32 StateIndex) const
{
	return GetPrivate()->GetStateName(StateIndex);
}


FString UCustomizableObjectPrivate::GetStateName(int32 StateIndex) const
{
	FString Result;

	if (GetModel())
	{
		Result = GetModel()->GetStateName(StateIndex);
	}

	return Result;
}


int32 UCustomizableObject::GetStateParameterCount( int32 StateIndex ) const
{
	int32 Result = 0;

	if (Private->GetModel())
	{
		Result = Private->GetModel()->GetStateParameterCount(StateIndex);
	}

	return Result;
}

int32 UCustomizableObject::GetStateParameterIndex(int32 StateIndex, int32 ParameterIndex) const
{
	int32 Result = 0;

	if (Private->GetModel())
	{
		Result = Private->GetModel()->GetStateParameterIndex(StateIndex, ParameterIndex);
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
void UCustomizableObjectPrivate::PostCompile()
{
	PostCompileDelegate.Broadcast();
}
#endif


const UCustomizableObjectBulk* UCustomizableObjectPrivate::GetStreamableBulkData() const
{
	return GetPublic()->BulkData;
}


UCustomizableObject* UCustomizableObjectPrivate::GetPublic() const
{
	UCustomizableObject* Public = StaticCast<UCustomizableObject*>(GetOuter());
	check(Public);

	return Public;
}

#if WITH_EDITORONLY_DATA
FPostCompileDelegate& UCustomizableObject::GetPostCompileDelegate() const
{
	return GetPrivate()->PostCompileDelegate;
}
#endif


UCustomizableObjectInstance* UCustomizableObject::CreateInstance()
{
	MUTABLE_CPUPROFILER_SCOPE(UCustomizableObject::CreateInstance)

	UCustomizableObjectInstance* PreviewInstance = NewObject<UCustomizableObjectInstance>(GetTransientPackage(), NAME_None, RF_Transient);
	PreviewInstance->SetObject(this);
	PreviewInstance->GetPrivate()->bShowOnlyRuntimeParameters = false;

	UE_LOG(LogMutable, Verbose, TEXT("Created Customizable Object Instance."));

	return PreviewInstance;
}


int32 UCustomizableObject::GetNumLODs() const
{
	if (IsCompiled())
	{
		return GetPrivate()->GetModelResources().NumLODs;
	}

	return 0;
}

int32 UCustomizableObject::GetComponentCount() const
{
	if (IsCompiled())
	{
		return GetPrivate()->GetModelResources().NumComponents;
	}

	return 0;
}

int32 UCustomizableObject::GetParameterCount() const
{
	return GetPrivate()->ParameterProperties.Num();
}


EMutableParameterType UCustomizableObject::GetParameterType(int32 ParamIndex) const
{
	return GetPrivate()->GetParameterType(ParamIndex);
}


EMutableParameterType UCustomizableObjectPrivate::GetParameterType(int32 ParamIndex) const
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
	const int* IndexPtr = GetPrivate()->ParameterPropertiesLookupTable.Find(Name);
	int Index = IndexPtr ? *IndexPtr : INDEX_NONE;

	if (GetPrivate()->ParameterProperties.IsValidIndex(Index))
	{
		return GetPrivate()->ParameterProperties[Index].Type;
	}

	UE_LOG(LogMutable, Warning, TEXT("Name '%s' does not exist in ParameterProperties lookup table at GetParameterTypeByName at CO %s."), *Name, *GetName());

	for (int32 ParamIndex = 0; ParamIndex < GetPrivate()->ParameterProperties.Num(); ++ParamIndex)
	{
		if (GetPrivate()->ParameterProperties[ParamIndex].Name == Name)
		{
			return GetPrivate()->ParameterProperties[ParamIndex].Type;
		}
	}

	UE_LOG(LogMutable, Warning, TEXT("Name '%s' does not exist in ParameterProperties at GetParameterTypeByName at CO %s."), *Name, *GetName());

	return EMutableParameterType::None;
}


static const FString s_EmptyString;

const FString & UCustomizableObject::GetParameterName(int32 ParamIndex) const
{
	if (ParamIndex >= 0 && ParamIndex < GetPrivate()->ParameterProperties.Num())
	{
		return GetPrivate()->ParameterProperties[ParamIndex].Name;
	}
	else
	{
		UE_LOG(LogMutable, Warning, TEXT("Index [%d] out of ParameterProperties bounds at GetParameterName at CO %s."), ParamIndex, *GetName());
	}

	return s_EmptyString;
}


void UCustomizableObjectPrivate::UpdateParameterPropertiesFromModel(const TSharedPtr<mu::Model>& Model)
{
	if (Model)
	{
		mu::ParametersPtr MutableParameters = mu::Model::NewParameters(Model);
		int paramCount = MutableParameters->GetCount();

		ParameterProperties.Reset(paramCount);
		ParameterPropertiesLookupTable.Empty(paramCount);
		for (int paramIndex = 0; paramIndex<paramCount; ++paramIndex)
		{
			FMutableModelParameterProperties Data;

			Data.Name = MutableParameters->GetName(paramIndex);
			Data.Type = EMutableParameterType::None;

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
		ParameterProperties.Empty();
		ParameterPropertiesLookupTable.Empty();
	}
}


int32 UCustomizableObject::GetParameterDescriptionCount(const FString& ParamName) const
{
	return 0;
}


int32 UCustomizableObject::GetIntParameterNumOptions(int32 ParamIndex) const
{
	if (ParamIndex >= 0 && ParamIndex < GetPrivate()->ParameterProperties.Num())
	{
		return GetPrivate()->ParameterProperties[ParamIndex].PossibleValues.Num();
	}
	else
	{
		UE_LOG(LogMutable, Warning, TEXT("Index [%d] out of ParameterProperties bounds at GetIntParameterNumOptions at CO %s."), ParamIndex, *GetName());
	}

	return 0;
}


const FString& UCustomizableObject::GetIntParameterAvailableOption(int32 ParamIndex, int32 K) const
{
	if (ParamIndex >= 0 && ParamIndex < GetPrivate()->ParameterProperties.Num())
	{
		if (K >= 0 && K < GetIntParameterNumOptions(ParamIndex))
		{
			return GetPrivate()->ParameterProperties[ParamIndex].PossibleValues[K].Name;
		}
		else
		{
			UE_LOG(LogMutable, Warning, TEXT("Index [%d] out of IntParameterNumOptions bounds at GetIntParameterAvailableOption at CO %s."), K, *GetName());
		}
	}
	else
	{
		UE_LOG(LogMutable, Warning, TEXT("Index [%d] out of ParameterProperties bounds at GetIntParameterAvailableOption at CO %s."), ParamIndex, *GetName());
	}

	return s_EmptyString;
}


int32 UCustomizableObject::FindParameter(const FString& Name) const
{
	return GetPrivate()->FindParameter(Name);
}


int32 UCustomizableObjectPrivate::FindParameter(const FString& Name) const
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
	return GetPrivate()->FindIntParameterValue(ParamIndex, Value);
}


int32 UCustomizableObjectPrivate::FindIntParameterValue(int32 ParamIndex, const FString& Value) const
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
			UE_LOG(LogMutable, Warning, TEXT("No possible values for parameter with index [%d] at FindIntParameterValue at CO %s."), ParamIndex, *GetName());
		}
	}
	else
	{
		UE_LOG(LogMutable, Warning, TEXT("Index [%d] out of ParameterProperties bounds at FindIntParameterValue at CO %s."), ParamIndex, *GetName());
	}
	return MinValueIndex;
}


FString UCustomizableObject::FindIntParameterValueName(int32 ParamIndex, int32 ParamValue) const
{
	if (ParamIndex >= 0 && ParamIndex < GetPrivate()->ParameterProperties.Num())
	{
		const TArray<FMutableModelParameterValue> & PossibleValues = GetPrivate()->ParameterProperties[ParamIndex].PossibleValues;

		const int32 MinValueIndex = !PossibleValues.IsEmpty() ? PossibleValues[0].Value : 0;
		ParamValue = ParamValue - MinValueIndex;

		if (PossibleValues.IsValidIndex(ParamValue))
		{
			return PossibleValues[ParamValue].Name;
		}
	}
	else
	{
		UE_LOG(LogMutable, Warning, TEXT("Index [%d] out of ParameterProperties bounds at FindIntParameterValueName at CO %s."), ParamIndex, *GetName());
	}

	return FString();
}


FParameterUIData UCustomizableObject::GetParameterUIMetadata(const FString& ParamName) const
{
	const FParameterUIData* ParameterUIData = Private->GetModelResources().ParameterUIDataMap.Find(ParamName);

	return ParameterUIData ? *ParameterUIData : FParameterUIData();
}


FParameterUIData UCustomizableObject::GetParameterUIMetadataFromIndex(int32 ParamIndex) const
{
	return GetParameterUIMetadata(GetParameterName(ParamIndex));
}


FParameterUIData UCustomizableObject::GetStateUIMetadata(const FString& StateName) const
{
	return GetPrivate()->GetStateUIMetadata(StateName);
}


FParameterUIData UCustomizableObjectPrivate::GetStateUIMetadata(const FString& StateName) const
{
	const FParameterUIData* StateUIData = GetModelResources().StateUIDataMap.Find(StateName);

	return StateUIData ? *StateUIData : FParameterUIData();
}


FParameterUIData UCustomizableObject::GetStateUIMetadataFromIndex(int32 StateIndex) const
{
	return GetPrivate()->GetStateUIMetadataFromIndex(StateIndex);
}


FParameterUIData UCustomizableObjectPrivate::GetStateUIMetadataFromIndex(int32 StateIndex) const
{
	return GetStateUIMetadata(GetStateName(StateIndex));
}


float UCustomizableObject::GetFloatParameterDefaultValue(const FString& InParameterName) const
{
	const int32 ParameterIndex = FindParameter(InParameterName);
	if (ParameterIndex == INDEX_NONE)
	{
		UE_LOG(LogMutable, Error, TEXT("Tried to access the default value of the nonexistent float parameter [%s] in the CustomizableObject [%s]."), *InParameterName, *GetName());
		return FCustomizableObjectFloatParameterValue::DEFAULT_PARAMETER_VALUE;
	}

	const TSharedPtr<mu::Model> Model = GetPrivate()->GetModel();
	if (!Model)
	{
		checkNoEntry();
		return FCustomizableObjectFloatParameterValue::DEFAULT_PARAMETER_VALUE;
	}

	return Model->GetFloatDefaultValue(ParameterIndex);
}


int32 UCustomizableObject::GetIntParameterDefaultValue(const FString& InParameterName) const
{
	const int32 ParameterIndex = FindParameter(InParameterName);
	if (ParameterIndex == INDEX_NONE)
	{
		UE_LOG(LogMutable, Error, TEXT("Tried to access the default value of the nonexistent integer parameter [%s] in the CustomizableObject [%s]."), *InParameterName, *GetName());
		return FCustomizableObjectIntParameterValue::DEFAULT_PARAMETER_VALUE;
	}

	const TSharedPtr<mu::Model> Model = GetPrivate()->GetModel();
	if (!Model)
	{
		checkNoEntry();
		return FCustomizableObjectIntParameterValue::DEFAULT_PARAMETER_VALUE;
	}
	
	return Model->GetIntDefaultValue(ParameterIndex);
}


bool UCustomizableObject::GetBoolParameterDefaultValue(const FString& InParameterName) const
{
	const int32 ParameterIndex = FindParameter(InParameterName);
	if (ParameterIndex == INDEX_NONE)
	{
		UE_LOG(LogMutable, Error, TEXT("Tried to access the default value of the nonexistent boolean parameter [%s] in the CustomizableObject [%s]."), *InParameterName, *GetName());
		return FCustomizableObjectBoolParameterValue::DEFAULT_PARAMETER_VALUE;
	}

	const TSharedPtr<mu::Model> Model = GetPrivate()->GetModel();
	if (!Model)
	{
		checkNoEntry();
		return FCustomizableObjectBoolParameterValue::DEFAULT_PARAMETER_VALUE;
	}
	
	return Model->GetBoolDefaultValue(ParameterIndex);
}


FLinearColor UCustomizableObject::GetColorParameterDefaultValue(const FString& InParameterName) const
{
	const int32 ParameterIndex = FindParameter(InParameterName);
	if (ParameterIndex == INDEX_NONE)
	{
		UE_LOG(LogMutable, Error, TEXT("Tried to access the default value of the nonexistent color parameter [%s] in the CustomizableObject [%s]."), *InParameterName, *GetName());
		return FCustomizableObjectVectorParameterValue::DEFAULT_PARAMETER_VALUE;
	}

	const TSharedPtr<mu::Model> Model = GetPrivate()->GetModel();
	if (!Model)
	{
		checkNoEntry();
		return FCustomizableObjectVectorParameterValue::DEFAULT_PARAMETER_VALUE;
	}
	
	FLinearColor Value;
	Model->GetColourDefaultValue(ParameterIndex, &Value.R, &Value.G, &Value.B, &Value.A);

	return Value;
}


void UCustomizableObject::GetProjectorParameterDefaultValue(const FString& InParameterName, FVector3f& OutPos,
	FVector3f& OutDirection, FVector3f& OutUp, FVector3f& OutScale, float& OutAngle,
	ECustomizableObjectProjectorType& OutType) const
{
	const FCustomizableObjectProjector Projector = GetProjectorParameterDefaultValue(InParameterName);
		
	OutType = Projector.ProjectionType;
	OutPos = Projector.Position;
	OutDirection = Projector.Direction;
	OutUp = Projector.Up;
	OutScale = Projector.Scale;
	OutAngle = Projector.Angle;
}


FCustomizableObjectProjector UCustomizableObject::GetProjectorParameterDefaultValue(const FString& InParameterName) const 
{
	const int32 ParameterIndex = FindParameter(InParameterName);
	if (ParameterIndex == INDEX_NONE)
	{
		UE_LOG(LogMutable, Error, TEXT("Tried to access the default value of the nonexistent projector [%s] in the CustomizableObject [%s]."), *InParameterName, *GetName());
		return FCustomizableObjectProjectorParameterValue::DEFAULT_PARAMETER_VALUE;
	}

	const TSharedPtr<mu::Model> Model = GetPrivate()->GetModel();
	if (!Model)
	{
		checkNoEntry();
		return FCustomizableObjectProjectorParameterValue::DEFAULT_PARAMETER_VALUE;
	}

	FCustomizableObjectProjector Value;
	mu::PROJECTOR_TYPE Type;
	Model->GetProjectorDefaultValue(ParameterIndex, &Type, &Value.Position, &Value.Direction, &Value.Up, &Value.Scale, &Value.Angle);
	Value.ProjectionType = ProjectorUtils::GetEquivalentProjectorType(Type);
	
	return Value;
}


FName UCustomizableObject::GetTextureParameterDefaultValue(const FString& InParameterName) const
{
	const int32 ParameterIndex = FindParameter(InParameterName);
	if (ParameterIndex == INDEX_NONE)
	{
		UE_LOG(LogMutable, Error, TEXT("Tried to access the default value of the nonexistent texture parameter [%s] in the CustomizableObject [%s]."), *InParameterName, *GetName());
		return FCustomizableObjectTextureParameterValue::DEFAULT_PARAMETER_VALUE;
	}

	const TSharedPtr<mu::Model> Model = GetPrivate()->GetModel();
	if (!Model)
	{
		checkNoEntry();
		return FCustomizableObjectTextureParameterValue::DEFAULT_PARAMETER_VALUE;
	}
	
	return Model->GetImageDefaultValue(ParameterIndex);
}


bool UCustomizableObject::IsParameterMultidimensional(const FString& InParameterName) const
{
	const int32 ParameterIndex = FindParameter(InParameterName);
	if (ParameterIndex == INDEX_NONE)
	{
		UE_LOG(LogMutable, Error, TEXT("Tried to access the default value of the nonexistent parameter [%s] in the CustomizableObject [%s]."), *InParameterName, *GetName());
		return false;
	}

	return IsParameterMultidimensional(ParameterIndex);
}


bool UCustomizableObject::IsParameterMultidimensional(const int32& InParamIndex) const
{
	check(InParamIndex != INDEX_NONE);
	if (Private->GetModel())
	{
		return Private->GetModel()->IsParameterMultidimensional(InParamIndex);
	}

	return false;
}


void UCustomizableObjectPrivate::ApplyStateForcedValuesToParameters( int32 State, mu::Parameters* Parameters)
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
				UE_LOG(LogMutable, Warning, TEXT("Forced parameter type not supported."));
				break;
			}
			}
		}
	}

}


void UCustomizableObjectPrivate::GetLowPriorityTextureNames(TArray<FString>& OutTextureNames)
{
	OutTextureNames.Reset(GetPublic()->LowPriorityTextures.Num());

	if (!GetPublic()->LowPriorityTextures.IsEmpty())
	{
		const FModelResources& LocalModelResources = GetModelResources();
		const int32 ImageCount = LocalModelResources.ImageProperties.Num();
		for (int32 ImageIndex = 0; ImageIndex < ImageCount; ++ImageIndex)
		{
			if (GetPublic()->LowPriorityTextures.Find(FName(LocalModelResources.ImageProperties[ImageIndex].TextureParameterName)) != INDEX_NONE)
			{
				OutTextureNames.Add(FString::FromInt(ImageIndex));
			}
		}
	}
}


int32 UCustomizableObjectPrivate::GetMinLODIndex() const
{
	int32 MinLODIdx = 0;

	if (GEngine && GEngine->UseSkeletalMeshMinLODPerQualityLevels)
	{
		if (UCustomizableObjectSystem::GetInstance() != nullptr)
		{
			MinLODIdx = GetPublic()->LODSettings.MinQualityLevelLOD.GetValue(UCustomizableObjectSystem::GetInstance()->GetSkeletalMeshMinLODQualityLevel());
		}
	}
	else
	{
		MinLODIdx = GetPublic()->LODSettings.MinLOD.GetValue();
	}

	return FMath::Max(MinLODIdx, static_cast<int32>(GetModelResources().FirstLODAvailable));
}


//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

USkeletalMesh* FMeshCache::Get(const TArray<mu::FResourceID>& Key)
{
	const TWeakObjectPtr<USkeletalMesh>* Result = GeneratedMeshes.Find(Key);
	return Result ? Result->Get() : nullptr;
}


void FMeshCache::Add(const TArray<mu::FResourceID>& Key, USkeletalMesh* Value)
{
	if (!Value)
	{
		return;
	}
	
	GeneratedMeshes.Add(Key, Value);

	// Remove invalid SkeletalMeshes from the cache.
	for (auto MeshIterator = GeneratedMeshes.CreateIterator(); MeshIterator; ++MeshIterator)
	{
		if (MeshIterator.Value().IsStale())
		{
			MeshIterator.RemoveCurrent();
		}
	}	
}


USkeleton* FSkeletonCache::Get(const TArray<uint16>& Key)
{
	const TWeakObjectPtr<USkeleton>* Result = MergedSkeletons.Find(Key);
	return Result ? Result->Get() : nullptr;
}


void FSkeletonCache::Add(const TArray<uint16>& Key, USkeleton* Value)
{
	if (!Value)
	{
		return;
	}

	MergedSkeletons.Add(Key, Value);

	// Remove invalid SkeletalMeshes from the cache.
	for (auto SkeletonIterator = MergedSkeletons.CreateIterator(); SkeletonIterator; ++SkeletonIterator)
	{
		if (SkeletonIterator.Value().IsStale())
		{
			SkeletonIterator.RemoveCurrent();
		}
	}
}


UCustomizableObjectPrivate::UCustomizableObjectPrivate()
{
#if WITH_EDITOR
	UPackage::PackageMarkedDirtyEvent.AddUObject(this, &UCustomizableObjectPrivate::OnParticipatingObjectDirty);
#endif
}


void UCustomizableObjectPrivate::SetModel(const TSharedPtr<mu::Model, ESPMode::ThreadSafe>& Model, const FGuid Id)
{
	if (MutableModel == Model
#if WITH_EDITOR
		&& Identifier == Id
#endif
		)
	{
		return;
	}
	
#if WITH_EDITOR
	if (MutableModel)
	{
		MutableModel->Invalidate();
	}

	Identifier = Id;
#endif
	
	MutableModel = Model;

	using EState = FCustomizableObjectStatus::EState;
	Status.NextState(Model ? EState::ModelLoaded : EState::NoModel);
}


const TSharedPtr<mu::Model, ESPMode::ThreadSafe>& UCustomizableObjectPrivate::GetModel()
{
	return MutableModel;
}


TSharedPtr<const mu::Model, ESPMode::ThreadSafe> UCustomizableObjectPrivate::GetModel() const
{
	return MutableModel;
}


const FModelResources& UCustomizableObjectPrivate::GetModelResources() const
{
#if WITH_EDITORONLY_DATA
	return ModelResourcesEditor;
#else
	return ModelResources;
#endif
}


#if WITH_EDITORONLY_DATA
FModelResources& UCustomizableObjectPrivate::GetModelResources(bool bIsCooking)
{
	return bIsCooking ? ModelResources : ModelResourcesEditor;
}
#endif


#if WITH_EDITOR
bool UCustomizableObjectPrivate::IsCompilationOutOfDate(TArray<FName>* OutOfDatePackages) const
{
	if (const ICustomizableObjectEditorModule* Module = ICustomizableObjectEditorModule::Get())
	{
		return Module->IsCompilationOutOfDate(*GetPublic(), OutOfDatePackages);
	}

	return false;		
}


void UCustomizableObjectPrivate::OnParticipatingObjectDirty(UPackage* Package, bool)
{
	if (ParticipatingObjects.Contains(Package->GetFName()))
	{
		DirtyParticipatingObjects.AddUnique(Package->GetFName());		
	}
}
#endif


TArray<FString>& UCustomizableObjectPrivate::GetCustomizableObjectClassTags()
{
	return GetPublic()->CustomizableObjectClassTags;
}


TArray<FString>& UCustomizableObjectPrivate::GetPopulationClassTags()
{
	return GetPublic()->PopulationClassTags;
}


TMap<FString, FParameterTags>& UCustomizableObjectPrivate::GetCustomizableObjectParametersTags()
{
	return GetPublic()->CustomizableObjectParametersTags;
}


#if WITH_EDITORONLY_DATA
TArray<FProfileParameterDat>& UCustomizableObjectPrivate::GetInstancePropertiesProfiles()
{
	return GetPublic()->InstancePropertiesProfiles;
}
#endif


TArray<FCustomizableObjectResourceData>& UCustomizableObjectPrivate::GetAlwaysLoadedExtensionData()
{
	return GetPublic()->AlwaysLoadedExtensionData;
}


TArray<FCustomizableObjectStreamedResourceData>& UCustomizableObjectPrivate::GetStreamedExtensionData()
{
	return GetPublic()->StreamedExtensionData;
}


TArray<FCustomizableObjectStreamedResourceData>& UCustomizableObjectPrivate::GetStreamedResourceData()
{
	return GetPublic()->StreamedResourceData;
}


//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

ECustomizableObjectCompilationState FCustomizableObjectCompilerBase::GetCompilationState() const
{
	return ECustomizableObjectCompilationState::None;
}


void UCustomizableObjectBulk::PostLoad()
{
	UObject::PostLoad();

	const FString OutermostName = GetOutermost()->GetName();
	const FString PackageFilename = FPackageName::LongPackageNameToFilename(OutermostName);
	BulkFilePrefix = PackageFilename;
}

TUniquePtr<IAsyncReadFileHandle> UCustomizableObjectBulk::OpenFileAsyncRead(uint32 FileId) const
{
	check(IsInGameThread());

	FString FilePath = FString::Printf(TEXT("%s-%08x.mut"), *BulkFilePrefix, FileId);

	IAsyncReadFileHandle* Result = FPlatformFileManager::Get().GetPlatformFile().OpenAsyncRead(*FilePath);
	
	// Result being null does not mean the file does not exist. A request has to be made. Let the callee deal with it.
	//UE_CLOG(!Result, LogMutable, Warning, TEXT("CustomizableObjectBulkData: Failed to open file [%s]."), *FilePath);

	return TUniquePtr<IAsyncReadFileHandle>(Result);
}

#if WITH_EDITOR

void UCustomizableObjectBulk::CookAdditionalFilesOverride(const TCHAR* PackageFilename,
	const ITargetPlatform* TargetPlatform,
	TFunctionRef<void(const TCHAR* Filename, void* Data, int64 Size)> WriteAdditionalFile)
{
	// Don't save streamed data on server builds since it won't be used anyway.
	if (TargetPlatform->IsServerOnly())
	{
		return;
	}
	
	check(CustomizableObject);
	
	FMutableCachedPlatformData* PlatformData = CustomizableObject->GetPrivate()->CachedPlatformsData.Find(TargetPlatform->PlatformName());
	check(PlatformData);

	const int32 NumBulkDataFiles = BulkDataFiles.Num();
	for(int32 FileIndex = 0; FileIndex < NumBulkDataFiles; ++FileIndex)
	{
		const FFile& CurrentFile = BulkDataFiles[FileIndex];

		const uint8* SourceData = nullptr;
		if (CurrentFile.DataType == EDataType::Model)
		{
			SourceData = PlatformData->StreamableData.GetData();
		}
		else if (CurrentFile.DataType == EDataType::RealTimeMorph)
		{
			SourceData = PlatformData->MorphData.GetData();
		}
		else
		{
			checkf(false, TEXT("Unknown file DataType found."));
			continue;
		}

		int64 FileSize = 0;
		for (const FBlock& Block : CurrentFile.Blocks)
		{
			FileSize += Block.Size;
		}

		// Generate the bulk data file in memory
		TArray64<uint8> FileBulkData;
		FileBulkData.SetNum(FileSize);
		uint8* FileData = FileBulkData.GetData();

		for (const FBlock& Block : CurrentFile.Blocks)
		{
			FMemory::Memcpy(FileData, SourceData + Block.Offset, Block.Size);
			FileData += Block.Size;
		}

		// Path to the asset
		const FString CookedFilePath = FPaths::GetPath(PackageFilename);

		FString CookedBulkFileName = FString::Printf(TEXT("%s/%s-%08x.mut"), *CookedFilePath, *CustomizableObject->GetName(), CurrentFile.Id);
		WriteAdditionalFile(*CookedBulkFileName, FileBulkData.GetData(), FileBulkData.Num());
	}
}

void UCustomizableObjectBulk::PrepareBulkData(UCustomizableObject* InOuter, const ITargetPlatform* TargetPlatform)
{
	CustomizableObject = InOuter;
	check(CustomizableObject);

	BulkDataFiles.Empty();

	TSharedPtr<const mu::Model, ESPMode::ThreadSafe> Model = CustomizableObject->GetPrivate()->GetModel();
	if (!Model)
	{
		return;
	}

	FModelResources& ModelResources = CustomizableObject->GetPrivate()->GetModelResources(true);

	uint64 TargetBulkDataFileBytes = CustomizableObject->CompileOptions.PackagedDataBytesLimit;
	const uint64 MaxChunkSize = UCustomizableObjectSystem::GetInstance()->GetMaxChunkSizeForPlatform(TargetPlatform);
	TargetBulkDataFileBytes = FMath::Min(TargetBulkDataFileBytes, MaxChunkSize);


	// TODO:
	// To avoid influence of the order of the streamed data (their index), classify it recursively based on hash values
	// until the tree leaves have either a single block, or a sum of blocks below the desired file size.
	struct FClassifyNode
	{
		TArray<FBlock> Blocks;

		//TSharedPtr<FClassifyNode> Child0;
		//TSharedPtr<FClassifyNode> Child1;
		//uint32 Depth=0;
	};

	FClassifyNode RootNode;
	// Create blocks data, filtering out the ones that are too big and will go on its own file in any case.
	{
		const int32 NumBlocks = Model->GetRomCount();
		RootNode.Blocks.Reserve(NumBlocks);
		
		uint64 SourceOffset = 0;
		for (int32 BlockIndex = 0; BlockIndex < NumBlocks; ++BlockIndex)
		{
			uint32 BlockId = Model->GetRomId(BlockIndex);
			const FMutableStreamableBlock& StreamableBlock = ModelResources.HashToStreamableBlock[BlockId];
			const uint32 BlockSize = StreamableBlock.Size;

			FBlock CurrentBlock = { EDataType::Model, BlockId, BlockSize, SourceOffset };
			if (BlockSize > TargetBulkDataFileBytes)
			{
				// It will go to its own file
				BulkDataFiles.Add(FFile{EDataType::Model, 0, {CurrentBlock}});
			}
			else
			{
				// It may merge with other small blocks
				RootNode.Blocks.Add(CurrentBlock);
			}

			SourceOffset += BlockSize;
		}
	}

	// TODO: This should create a new classification branch when the tree is implemented.
	// For now append after Model roms. 
	{
		uint64 SourceOffset = 0;
		
		constexpr bool bGetCooked = false;
		const TArray<FMutableStreamableBlock>& RealTimeMorphTargetsBlocks = 
					CustomizableObject->GetPrivate()->GetModelResources(bGetCooked).RealTimeMorphStreamableBlocks;

		const int32 NumBlocks = RealTimeMorphTargetsBlocks.Num();
		for (int32 BlockIndex = 0; BlockIndex < NumBlocks; ++BlockIndex)
		{
			const FMutableStreamableBlock& StreamableBlock = RealTimeMorphTargetsBlocks[BlockIndex];
			const uint32 BlockSize = StreamableBlock.Size;
			
			check(SourceOffset == StreamableBlock.Offset);
			FBlock CurrentBlock = { EDataType::RealTimeMorph, BlockIndex, BlockSize, SourceOffset };
			
			if (BlockSize > TargetBulkDataFileBytes)
			{
				// It will go to its own file
				BulkDataFiles.Add(FFile{ EDataType::RealTimeMorph, 0, {CurrentBlock} });
			}
			else
			{
				// It may merge with other small blocks
				RootNode.Blocks.Add(CurrentBlock);
			}

			SourceOffset += BlockSize;
		}
	}

	// Temp: Group by order in the array
	for (int32 BlockIndex = 0; BlockIndex < RootNode.Blocks.Num(); )
	{
		int32 CurrentFileSize = 0;
		
		FFile CurrentFile;
		CurrentFile.DataType = RootNode.Blocks[BlockIndex].DataType;

		while(BlockIndex < RootNode.Blocks.Num())
		{
			FBlock CurrentBlock = RootNode.Blocks[BlockIndex];

			// Next file?
			// Store different data types in different files. Blocks should be sorted by DataType
			// so data is properly packeted
			if (CurrentFile.DataType != CurrentBlock.DataType)
			{
				break;
			}
			
			if (CurrentFileSize > 0 && CurrentFileSize + CurrentBlock.Size > TargetBulkDataFileBytes)
			{
				break;
			}

			// Add the block to the current file
			CurrentFile.Blocks.Add(CurrentBlock);
			CurrentFileSize += CurrentBlock.Size;

			// Next block
			++BlockIndex;			
		}

		BulkDataFiles.Add(MoveTemp(CurrentFile));
	}

	// Create the file list
	for (int32 FileIndex = 0; FileIndex < BulkDataFiles.Num(); ++FileIndex)
	{
		// Generate the id for this file
		FFile& CurrentFile = BulkDataFiles[FileIndex];
		uint32 FileId = 0;
		for (int32 FileBlockIndex = 0; FileBlockIndex < CurrentFile.Blocks.Num(); ++FileBlockIndex)
		{
			FBlock ThisBlock = CurrentFile.Blocks[FileBlockIndex];
			FileId = (FileBlockIndex == 0) ? ThisBlock.Id : HashCombine(HashCombine(FileId, ThisBlock.Id), uint32(CurrentFile.DataType));
		}

		// Ensure the FileId is unique
		bool bUnique = false;
		while (!bUnique)
		{
			bUnique = true;
			for (int32 PreviousFileIndex = 0; PreviousFileIndex < FileIndex; ++PreviousFileIndex)
			{
				if (BulkDataFiles[PreviousFileIndex].Id == FileId)
				{
					bUnique = false;
					++FileId;
					break;
				}
			}
		}

		// Set it to the editor-only file descriptor
		CurrentFile.Id = FileId;

		if (CurrentFile.DataType == EDataType::Model)
		{
			// Set it to all streamable blocks
			uint32 OffsetInFile = 0;
			for (int32 FileBlockIndex = 0; FileBlockIndex < CurrentFile.Blocks.Num(); ++FileBlockIndex)
			{
				FBlock ThisBlock = CurrentFile.Blocks[FileBlockIndex];

				FMutableStreamableBlock& StreamableBlock = ModelResources.HashToStreamableBlock[ThisBlock.Id];
				check(StreamableBlock.Size == ThisBlock.Size);
				StreamableBlock.FileId = FileId;
				StreamableBlock.Offset = OffsetInFile;
				OffsetInFile += ThisBlock.Size;
			}
		}
		else if (CurrentFile.DataType == EDataType::RealTimeMorph)
		{
			constexpr bool bGetCooked = true;
			TArray<FMutableStreamableBlock>& MorphBlocks = 
					CustomizableObject->GetPrivate()->GetModelResources(bGetCooked).RealTimeMorphStreamableBlocks;
			// Set it to all streamable blocks
			uint32 OffsetInFile = 0;
			for (int32 FileBlockIndex = 0; FileBlockIndex < CurrentFile.Blocks.Num(); ++FileBlockIndex)
			{
				FBlock ThisBlock = CurrentFile.Blocks[FileBlockIndex];

				FMutableStreamableBlock& StreamableBlock = MorphBlocks[ThisBlock.Id];
				check(StreamableBlock.Size == ThisBlock.Size);
				StreamableBlock.FileId = FileId;
				StreamableBlock.Offset = OffsetInFile;
				OffsetInFile += ThisBlock.Size;
			}
		}
		else
		{
			UE_LOG(LogMutable, Error, TEXT("Unknown DataType found while fixing streaming block files ids."));
			check(false);
		}
	}
}

#endif // WITH_EDITOR

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
bool FAnimBpOverridePhysicsAssetsInfo::operator==(const FAnimBpOverridePhysicsAssetsInfo& Rhs) const
{
	return AnimInstanceClass == Rhs.AnimInstanceClass &&
		SourceAsset == Rhs.SourceAsset &&
		PropertyIndex == Rhs.PropertyIndex;
}


bool FMutableModelImageProperties::operator!=(const FMutableModelImageProperties& Other) const
{
	return
		TextureParameterName != Other.TextureParameterName ||
		Filter != Other.Filter ||
		SRGB != Other.SRGB ||
		FlipGreenChannel != Other.FlipGreenChannel ||
		IsPassThrough != Other.IsPassThrough ||
		LODBias != Other.LODBias ||
		LODGroup != Other.LODGroup ||
		AddressX != Other.AddressX ||
		AddressY != Other.AddressY;
}


bool FMutableRefSocket::operator==(const FMutableRefSocket& Other) const
{
	if (
		SocketName == Other.SocketName &&
		BoneName == Other.BoneName &&
		RelativeLocation == Other.RelativeLocation &&
		RelativeRotation == Other.RelativeRotation &&
		RelativeScale == Other.RelativeScale &&
		bForceAlwaysAnimated == Other.bForceAlwaysAnimated &&
		Priority == Other.Priority)
	{
		return true;
	}

	return false;
}


bool FMutableSkinWeightProfileInfo::operator==(const FMutableSkinWeightProfileInfo& Other) const
{
	return Name == Other.Name;
}


#if WITH_EDITORONLY_DATA
FArchive& operator<<(FArchive& Ar, FMutableModelImageProperties& ImageProps)
{
	Ar << ImageProps.TextureParameterName;
	Ar << ImageProps.Filter;

	// Bitfields don't serialize automatically with FArchive
	if (Ar.IsLoading())
	{
		int32 Aux = 0;
		Ar << Aux;
		ImageProps.SRGB = Aux;

		Aux = 0;
		Ar << Aux;
		ImageProps.FlipGreenChannel = Aux;

		Aux = 0;
		Ar << Aux;
		ImageProps.IsPassThrough = Aux;
	}
	else
	{
		int32 Aux = ImageProps.SRGB;
		Ar << Aux;

		Aux = ImageProps.FlipGreenChannel;
		Ar << Aux;

		Aux = ImageProps.IsPassThrough;
		Ar << Aux;
	}

	Ar << ImageProps.LODBias;
	Ar << ImageProps.LODGroup;

	Ar << ImageProps.AddressX;
	Ar << ImageProps.AddressY;

	return Ar;
}


FArchive& operator<<(FArchive& Ar, FAnimBpOverridePhysicsAssetsInfo& Info)
{
	FString AnimInstanceClassPathString;
	FString PhysicsAssetPathString;

	if (Ar.IsLoading())
	{
		Ar << AnimInstanceClassPathString;
		Ar << PhysicsAssetPathString;
		Ar << Info.PropertyIndex;

		Info.AnimInstanceClass = TSoftClassPtr<UAnimInstance>(AnimInstanceClassPathString);
		Info.SourceAsset = TSoftObjectPtr<UPhysicsAsset>(PhysicsAssetPathString);
	}

	if (Ar.IsSaving())
	{
		AnimInstanceClassPathString = Info.AnimInstanceClass.ToString();
		PhysicsAssetPathString = Info.SourceAsset.ToString();

		Ar << AnimInstanceClassPathString;
		Ar << PhysicsAssetPathString;
		Ar << Info.PropertyIndex;
	}

	return Ar;
}


FArchive& operator<<(FArchive& Ar, FMutableRefSocket& Data)
{
	Ar << Data.SocketName;
	Ar << Data.BoneName;
	Ar << Data.RelativeLocation;
	Ar << Data.RelativeRotation;
	Ar << Data.RelativeScale;
	Ar << Data.bForceAlwaysAnimated;
	Ar << Data.Priority;

	return Ar;
}


FArchive& operator<<(FArchive& Ar, FMutableRefLODRenderData& Data)
{
	Ar << Data.bIsLODOptional;
	Ar << Data.bStreamedDataInlined;

	return Ar;
}


FArchive& operator<<(FArchive& Ar, FMutableRefLODInfo& Data)
{
	Ar << Data.ScreenSize;
	Ar << Data.LODHysteresis;
	Ar << Data.bSupportUniformlyDistributedSampling;
	Ar << Data.bAllowCPUAccess;

	return Ar;
}


FArchive& operator<<(FArchive& Ar, FMutableRefLODData& Data)
{
	Ar << Data.LODInfo;
	Ar << Data.RenderData;

	return Ar;
}


FArchive& operator<<(FArchive& Ar, FMutableRefSkeletalMeshSettings& Data)
{
	Ar << Data.bEnablePerPolyCollision;
	Ar << Data.DefaultUVChannelDensity;

	return Ar;
}


FArchive& operator<<(FArchive& Ar, FMutableRefAssetUserData& Data)
{
	Ar << Data.AssetUserDataIndex;

	return Ar;
}


FArchive& operator<<(FArchive& Ar, FMutableSkinWeightProfileInfo& Info)
{
	Ar << Info.Name;
	Ar << Info.DefaultProfile;
	Ar << Info.DefaultProfileFromLODIndex;

	return Ar;
}


//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void FMutableRefSkeletalMeshData::InitResources(UCustomizableObject* InOuter, const ITargetPlatform* InTargetPlatform)
{
	check(InOuter);

	const bool bHasServer = InTargetPlatform ? !InTargetPlatform->IsClientOnly() : false;
	if (InOuter->bEnableUseRefSkeletalMeshAsPlaceholder || bHasServer)
	{
		SkeletalMesh = TSoftObjectPtr<USkeletalMesh>(SoftSkeletalMesh).LoadSynchronous();
	}

	// Initialize AssetUserData
	for (FMutableRefAssetUserData& Data : AssetUserData)
	{
		if (!InOuter->GetPrivate()->GetStreamedResourceData().IsValidIndex(Data.AssetUserDataIndex))
		{
			check(false);
			continue;
		}

		FCustomizableObjectStreamedResourceData& StreamedResource = InOuter->GetPrivate()->GetStreamedResourceData()[Data.AssetUserDataIndex];
		Data.AssetUserData = StreamedResource.GetPath().LoadSynchronous();
		check(Data.AssetUserData);
		check(Data.AssetUserData->Data.Type == ECOResourceDataType::AssetUserData);
	}
}


FArchive& operator<<(FArchive& Ar, FMutableRefSkeletalMeshData& Data)
{
	Ar << Data.LODData;
	Ar << Data.Sockets;
	Ar << Data.Bounds;
	Ar << Data.Settings;

	if (Ar.IsSaving())
	{
		FString AssetPath = Data.SoftSkeletalMesh.ToString();
		Ar << AssetPath;

		AssetPath = TSoftObjectPtr<USkeleton>(Data.Skeleton).ToString();
		Ar << AssetPath;

		AssetPath = TSoftObjectPtr<UPhysicsAsset>(Data.PhysicsAsset).ToString();
		Ar << AssetPath;

		AssetPath = Data.PostProcessAnimInst.ToString();
		Ar << AssetPath;

		AssetPath = TSoftObjectPtr<UPhysicsAsset>(Data.ShadowPhysicsAsset).ToString();
		Ar << AssetPath;

	}
	else
	{
		FString SkeletalMeshAssetPath;
		Ar << SkeletalMeshAssetPath;
		Data.SoftSkeletalMesh = SkeletalMeshAssetPath;

		FString SkeletonAssetPath;
		Ar << SkeletonAssetPath;
		Data.Skeleton = TSoftObjectPtr<USkeleton>(SkeletonAssetPath).LoadSynchronous();

		FString PhysicsAssetPath;
		Ar << PhysicsAssetPath;
		Data.PhysicsAsset = TSoftObjectPtr<UPhysicsAsset>(PhysicsAssetPath).LoadSynchronous();

		FString PostProcessAnimInstAssetPath;
		Ar << PostProcessAnimInstAssetPath;
		Data.PostProcessAnimInst = TSoftClassPtr<UAnimInstance>(PostProcessAnimInstAssetPath).LoadSynchronous();

		FString ShadowPhysicsAssetPath;
		Ar << ShadowPhysicsAssetPath;
		Data.ShadowPhysicsAsset = TSoftObjectPtr<UPhysicsAsset>(ShadowPhysicsAssetPath).LoadSynchronous();
	}

	Ar << Data.AssetUserData;

	return Ar;
}

#endif // WITH_EDITORONLY_DATA

#undef LOCTEXT_NAMESPACE
