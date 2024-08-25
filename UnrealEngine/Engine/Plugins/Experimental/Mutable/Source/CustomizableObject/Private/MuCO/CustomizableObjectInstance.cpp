// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCO/CustomizableObjectInstance.h"

#include "Algo/Find.h"
#include "Algo/MaxElement.h"
#include "Animation/AnimClassInterface.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Animation/AnimInstance.h"
#include "Animation/Skeleton.h"
#include "BoneControllers/AnimNode_RigidBody.h"
#include "ClothConfig.h"
#include "ClothingAsset.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Engine/SkeletalMeshLODSettings.h"
#include "Engine/Texture2DArray.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Modules/ModuleManager.h"
#include "Tasks/Task.h"

#include "MuCO/CustomizableObjectSystemPrivate.h"
#include "MuCO/CustomizableObjectSkeletalMesh.h"
#include "MuCO/CustomizableInstanceLODManagement.h"
#include "MuCO/CustomizableObjectInstancePrivate.h"
#include "MuCO/CustomizableObjectExtension.h"
#include "MuCO/CustomizableObjectMipDataProvider.h"
#include "MuCO/CustomizableObjectPrivate.h"
#include "MuCO/CustomizableObjectInstanceUsage.h"
#include "MuCO/CustomizableObjectInstanceAssetUserData.h"
#include "MuCO/DefaultImageProvider.h"
#include "MuCO/ICustomizableObjectModule.h"
#include "MuCO/UnrealConversionUtils.h"
#include "MuCO/UnrealPortabilityHelpers.h"
#include "MuCO/LogBenchmarkUtil.h"
#include "MuR/Model.h"

#include "PhysicsEngine/PhysicsAsset.h"
#include "Rendering/Texture2DResource.h"
#include "RenderingThread.h"
#include "SkeletalMergingLibrary.h"
#include "UnrealMutableImageProvider.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CustomizableObjectInstance)

#if WITH_EDITOR
#include "Logging/MessageLog.h"
#include "MessageLogModule.h"
#include "UnrealEdMisc.h"
#include "Subsystems/AssetEditorSubsystem.h"
#endif


// Struct used by BuildMaterials() to identify common materials between LODs
struct FMutableMaterialPlaceholder
{
	enum class EPlaceHolderParamType { Vector, Scalar, Texture };

	struct FMutableMaterialPlaceHolderParam
	{
		FName ParamName;
		EPlaceHolderParamType Type;
		int32 LayerIndex; // Set to -1 for non-multilayer params
		float Scalar;
		FLinearColor Vector;
		FGeneratedTexture Texture;

		FMutableMaterialPlaceHolderParam(const FName& InParamName, const int32 InLayerIndex, const FLinearColor& InVector)
			: ParamName(InParamName), Type(EPlaceHolderParamType::Vector), LayerIndex(InLayerIndex), Vector(InVector) {}

		FMutableMaterialPlaceHolderParam(const FName& InParamName, const int32 InLayerIndex, const float InScalar)
			: ParamName(InParamName), Type(EPlaceHolderParamType::Scalar), LayerIndex(InLayerIndex), Scalar(InScalar) {}

		FMutableMaterialPlaceHolderParam(const FName& InParamName, const int32 InLayerIndex, const FGeneratedTexture& InTexture)
			: ParamName(InParamName), Type(EPlaceHolderParamType::Texture), LayerIndex(InLayerIndex), Texture(InTexture) {}

		bool operator<(const FMutableMaterialPlaceHolderParam& Other) const
		{
			return Type < Other.Type || ParamName.CompareIndexes(Other.ParamName);
		}

		bool operator==(const FMutableMaterialPlaceHolderParam& Other) const = default;
	};

	uint32 ParentMaterialID = 0;
	int32 MatIndex = -1;

private:
	mutable TArray<FMutableMaterialPlaceHolderParam> Params;

public:
	void AddParam(const FMutableMaterialPlaceHolderParam& NewParam) { Params.Add(NewParam); }
	
	const TArray<FMutableMaterialPlaceHolderParam>& GetParams() const { return Params; }

	bool operator==(const FMutableMaterialPlaceholder& Other) const;

	friend uint32 GetTypeHash(const FMutableMaterialPlaceholder& PlaceHolder);
};


bool FMutableMaterialPlaceholder::operator==(const FMutableMaterialPlaceholder& Other) const
{
	return ParentMaterialID == Other.ParentMaterialID &&
		   Params == Other.Params;
}


// Return a hash of the material and its parameters
uint32 GetTypeHash(const FMutableMaterialPlaceholder& PlaceHolder)
{
	uint32 Hash = GetTypeHash(PlaceHolder.ParentMaterialID);

	// Sort parameters before building the hash.
	PlaceHolder.Params.Sort();

	for (const FMutableMaterialPlaceholder::FMutableMaterialPlaceHolderParam& Param : PlaceHolder.Params)
	{
		uint32 ParamHash = GetTypeHash(Param.ParamName);
		ParamHash = HashCombineFast(ParamHash, (uint32)Param.LayerIndex);
		ParamHash = HashCombineFast(ParamHash, (uint32)Param.Type);

		switch (Param.Type)
		{
		case FMutableMaterialPlaceholder::EPlaceHolderParamType::Vector:
			ParamHash = HashCombineFast(ParamHash, GetTypeHash(Param.Vector));
			break;

		case FMutableMaterialPlaceholder::EPlaceHolderParamType::Scalar:
			ParamHash = HashCombineFast(ParamHash, GetTypeHash(Param.Scalar));
			break;

		case FMutableMaterialPlaceholder::EPlaceHolderParamType::Texture:
			ParamHash = HashCombineFast(ParamHash, Param.Texture.Texture->GetUniqueID());
			break;
		}

		Hash = HashCombineFast(Hash, ParamHash);
	}

	return Hash;
}


UTexture2D* UCustomizableInstancePrivate::CreateTexture()
{
	UTexture2D* NewTexture = NewObject<UTexture2D>(
		GetTransientPackage(),
		NAME_None,
		RF_Transient
		);
	UCustomizableObjectSystem::GetInstance()->GetPrivate()->LogBenchmarkUtil.AddTexture(*NewTexture);
	NewTexture->SetPlatformData( nullptr );

	return NewTexture;
}


mu::FResourceID UCustomizableInstancePrivate::GetLastMeshId(int32 ComponentIndex, int32 LODIndex) const
{
	const FCustomizableInstanceComponentData* ComponentData = GetComponentData(ComponentIndex);

	if (ComponentData && ComponentData->LastMeshIdPerLOD.IsValidIndex(LODIndex))
	{
		return ComponentData->LastMeshIdPerLOD[LODIndex];
	}

	check(ComponentData);
	check(ComponentData->LastMeshIdPerLOD.IsValidIndex(LODIndex));
		
	return MAX_uint64;
}


void UCustomizableInstancePrivate::SetLastMeshId(int32 ComponentIndex, int32 LODIndex, mu::FResourceID MeshId)
{
	FCustomizableInstanceComponentData* ComponentData = GetComponentData(ComponentIndex);
	if (ComponentData && ComponentData->LastMeshIdPerLOD.IsValidIndex(LODIndex))
	{
		ComponentData->LastMeshIdPerLOD[LODIndex] = MeshId;
	}
	else
	{
		check(false);
	}
}


void UCustomizableInstancePrivate::InvalidateGeneratedData()
{
	SkeletalMeshStatus = ESkeletalMeshStatus::NotGenerated;
	SkeletalMeshes.Reset();

	CommittedDescriptor = {};
	CommittedDescriptorHash = {};

	// Init Component Data
	FCustomizableInstanceComponentData TemplateComponentData;
	TemplateComponentData.LastMeshIdPerLOD.Init(MAX_uint64, MAX_MESH_LOD_COUNT);
	ComponentsData.Init(TemplateComponentData, ComponentsData.Num());

	GeneratedMaterials.Empty();
}


void UCustomizableInstancePrivate::InitCustomizableObjectData(const UCustomizableObject* InCustomizableObject)
{
	InvalidateGeneratedData();

	if (!InCustomizableObject || !InCustomizableObject->IsCompiled())
	{
		return;
	}

	// Init LOD Data
	const FModelResources& ModelResources = InCustomizableObject->GetPrivate()->GetModelResources();
	NumLODsAvailable = ModelResources.NumLODs;
	FirstLODAvailable = ModelResources.FirstLODAvailable;
	FirstResidentLOD = FMath::Clamp(ModelResources.NumLODsToStream, FirstLODAvailable, NumLODsAvailable);

	// Init Component Data
	FCustomizableInstanceComponentData TemplateComponentData;
	TemplateComponentData.LastMeshIdPerLOD.Init(MAX_uint64, MAX_MESH_LOD_COUNT);
	ComponentsData.Init(TemplateComponentData, InCustomizableObject->GetComponentCount());
}


FCustomizableInstanceComponentData* UCustomizableInstancePrivate::GetComponentData(int32 ComponentIndex)
{
	return ComponentsData.IsValidIndex(ComponentIndex) ? &ComponentsData[ComponentIndex] : nullptr;
}


const FCustomizableInstanceComponentData* UCustomizableInstancePrivate::GetComponentData(int32 ComponentIndex) const
{
	return ComponentsData.IsValidIndex(ComponentIndex) ? &ComponentsData[ComponentIndex] : nullptr;
}


UCustomizableObjectInstance::UCustomizableObjectInstance()
{
	SetFlags(RF_Transactional);
	
	PrivateData = CreateDefaultSubobject<UCustomizableInstancePrivate>(FName("Private"));
}


const FCustomizableObjectInstanceDescriptor& UCustomizableObjectInstance::GetDescriptor() const
{
	return Descriptor;
}


void UCustomizableObjectInstance::SetDescriptor(const FCustomizableObjectInstanceDescriptor& InDescriptor)
{
	UCustomizableObject* InCustomizableObject = InDescriptor.GetCustomizableObject();
	const bool bCustomizableObjectChanged = Descriptor.GetCustomizableObject() != InCustomizableObject;

#if WITH_EDITOR
	// Bind a lambda to the PostCompileDelegate and unbind from the previous object if any.
	PrivateData->BindObjectDelegates(GetCustomizableObject(), InCustomizableObject);
#endif

	Descriptor = InDescriptor;

	if (bCustomizableObjectChanged)
	{
		PrivateData->InitCustomizableObjectData(InCustomizableObject);
	}
}


void UCustomizableInstancePrivate::PrepareForUpdate(const TSharedRef<FUpdateContextPrivate>& OperationData)
{
	// Clear the ComponentData from previous updates
	for (FCustomizableInstanceComponentData& ComponentData : ComponentsData)
	{
		ComponentData.AnimSlotToBP.Empty();
		ComponentData.AssetUserDataArray.Empty();
		ComponentData.Skeletons.Skeleton = nullptr;
		ComponentData.Skeletons.SkeletonIds.Empty();
		ComponentData.Skeletons.SkeletonsToMerge.Empty();
		ComponentData.PhysicsAssets.PhysicsAssetToLoad.Empty();
		ComponentData.PhysicsAssets.PhysicsAssetsToMerge.Empty();
		ComponentData.ClothingPhysicsAssetsToStream.Empty();
		ComponentData.StreamedResourceIndex.Empty();

#if WITH_EDITORONLY_DATA
		ComponentData.MeshPartPaths.Empty();
#endif
	}
}


#if WITH_EDITOR


void UCustomizableInstancePrivate::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);

	// Invalidate all generated data to avoid modifying resources shared between CO instances.
	InvalidateGeneratedData();

	// Empty after duplicating or ReleasingMutableResources may free textures used by the other CO instance.
	GeneratedTextures.Empty();
}


void UCustomizableInstancePrivate::OnPostCompile()
{
	GetDescriptor().ReloadParameters();
	InitCustomizableObjectData(GetPublic()->GetCustomizableObject());
}


void UCustomizableInstancePrivate::OnObjectStatusChanged(FCustomizableObjectStatus::EState Previous, FCustomizableObjectStatus::EState Next)
{
	if (Previous != Next && Next == FCustomizableObjectStatus::EState::ModelLoaded)
	{
		OnPostCompile();
	}
}


void UCustomizableInstancePrivate::BindObjectDelegates(UCustomizableObject*  CurrentCustomizableObject, UCustomizableObject* NewCustomizableObject)
{
	if (CurrentCustomizableObject == NewCustomizableObject)
	{
		return;
	}

	// Unbind callback from the previous CO
	if (CurrentCustomizableObject)
	{
		CurrentCustomizableObject->GetPrivate()->PostCompileDelegate.RemoveAll(this);
		CurrentCustomizableObject->GetPrivate()->Status.GetOnStateChangedDelegate().RemoveAll(this);
	}

	// Bind callback to the new CO
	if (NewCustomizableObject)
	{
		NewCustomizableObject->GetPrivate()->PostCompileDelegate.AddUObject(this, &UCustomizableInstancePrivate::OnPostCompile);
		NewCustomizableObject->GetPrivate()->Status.GetOnStateChangedDelegate().AddUObject(this, &UCustomizableInstancePrivate::OnObjectStatusChanged);
	}
}


void UCustomizableObjectInstance::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.MemberProperty ? PropertyChangedEvent.MemberProperty->GetFName() : NAME_None;

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UCustomizableObjectInstance, TextureParameterDeclarations))
	{
		UpdateSkeletalMeshAsync(true, true);
	}
}


bool UCustomizableObjectInstance::CanEditChange(const FProperty* InProperty) const
{
	bool bIsMutable = Super::CanEditChange(InProperty);
	if (bIsMutable && InProperty != NULL)
	{
		if (InProperty->GetFName() == TEXT("CustomizationObject"))
		{
			bIsMutable = false;
		}

		if (InProperty->GetFName() == TEXT("ParameterName"))
		{
			bIsMutable = false;
		}
	}

	return bIsMutable;
}

#endif // WITH_EDITOR


bool UCustomizableObjectInstance::IsEditorOnly() const
{
	if (UCustomizableObject* CustomizableObject = GetCustomizableObject())
	{
		return CustomizableObject->IsEditorOnly();
	}
	return false;
}


void UCustomizableObjectInstance::BeginDestroy()
{
	// Release the Live Instance ID if there it hadn't been released before
	DestroyLiveUpdateInstance();

	if (PrivateData)
	{
#if WITH_EDITOR
		// Unbind Object delegates
		PrivateData->BindObjectDelegates(GetCustomizableObject(), nullptr);
#endif

		if (PrivateData->StreamingHandle.IsValid() && PrivateData->StreamingHandle->IsActive())
		{
			PrivateData->StreamingHandle->CancelHandle();
		}

		PrivateData->StreamingHandle = nullptr;

		PrivateData.Get()->ReleaseMutableResources(true, *this);
	}
	
	Super::BeginDestroy();
}


void UCustomizableObjectInstance::DestroyLiveUpdateInstance()
{
	if (PrivateData && PrivateData->LiveUpdateModeInstanceID)
	{
		// If UCustomizableObjectSystemPrivate::SSystem is nullptr it means it has already been destroyed, no point in registering an instanceID release
		// since the Mutable system has already been destroyed. Just checking UCustomizableObjectSystem::GetInstance() will try to recreate the system when
		// everything is shutting down, so it's better to check UCustomizableObjectSystemPrivate::SSystem first here
		if (UCustomizableObjectSystemPrivate::SSystem && UCustomizableObjectSystem::GetInstance() && UCustomizableObjectSystem::GetInstance()->GetPrivate())
		{
			UCustomizableObjectSystem::GetInstance()->GetPrivate()->InitInstanceIDRelease(PrivateData->LiveUpdateModeInstanceID);
			PrivateData->LiveUpdateModeInstanceID = 0;
		}
	}
}


void UCustomizableInstancePrivate::ReleaseMutableResources(bool bCalledFromBeginDestroy, const UCustomizableObjectInstance& Instance)
{
	GeneratedMaterials.Empty();

	if (UCustomizableObjectSystem::IsCreated()) // Need to check this because the object might be destroyed after the CustomizableObjectSystem at shutdown
	{
		UCustomizableObjectSystemPrivate* CustomizableObjectSystem = UCustomizableObjectSystem::GetInstance()->GetPrivate();
		// Get the cache of resources of all live instances of this object
		FMutableResourceCache& Cache = CustomizableObjectSystem->GetObjectCache(Instance.GetCustomizableObject());

		for (FGeneratedTexture& Texture : GeneratedTextures)
		{
			if (CustomizableObjectSystem->RemoveTextureReference(Texture.Key))
			{
				// Do not release textures when called from BeginDestroy, it would produce a texture artifact in the
				// instance's remaining sk meshes and GC is being performed anyway so it will free the textures if needed
				if (!bCalledFromBeginDestroy && CustomizableObjectSystem->bReleaseTexturesImmediately)
				{
					ReleaseMutableTexture(Texture.Key, Cast<UTexture2D>(Texture.Texture), Cache);
				}
			}
		}

		// Remove all references to cached Texture Parameters. Only if we're destroying the COI.
		if (bCalledFromBeginDestroy)
		{
			CustomizableObjectSystem->UnCacheTextureParameters(CommittedDescriptor.GetTextureParameters());
		}
	}

	GeneratedTextures.Empty();
}


bool UCustomizableObjectInstance::IsReadyForFinishDestroy()
{
	//return ReleaseResourcesFence.IsFenceComplete();
	return true;
}


void UCustomizableObjectInstance::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FCustomizableObjectCustomVersion::GUID);

	const int32 CustomizableObjectCustomVersion = GetLinkerCustomVersion(FCustomizableObjectCustomVersion::GUID);

	if (CustomizableObjectCustomVersion < FCustomizableObjectCustomVersion::GroupProjectorIntToScalarIndex)
	{
		TArray<int32> IntParametersToMove;

		// Find the num layer parameters that were int enums
		for (int32 i = 0; i < IntParameters_DEPRECATED.Num(); ++i)
		{
			if (IntParameters_DEPRECATED[i].ParameterName.EndsWith(FMultilayerProjector::NUM_LAYERS_PARAMETER_POSTFIX, ESearchCase::CaseSensitive))
			{
				FString ParameterNamePrefix, Aux;
				const bool bSplit = IntParameters_DEPRECATED[i].ParameterName.Split(FMultilayerProjector::NUM_LAYERS_PARAMETER_POSTFIX, &ParameterNamePrefix, &Aux);
				check(bSplit);

				// Confirm this is actually a multilayer param by finding the corresponding pose param
				for (int32 j = 0; j < IntParameters_DEPRECATED.Num(); ++j)
				{
					if (i != j)
					{
						if (IntParameters_DEPRECATED[j].ParameterName.StartsWith(ParameterNamePrefix, ESearchCase::CaseSensitive) &&
							IntParameters_DEPRECATED[j].ParameterName.EndsWith(FMultilayerProjector::POSE_PARAMETER_POSTFIX, ESearchCase::CaseSensitive))
						{
							IntParametersToMove.Add(i);
							break;
						}
					}
				}
			}
		}

		// Convert them to float params
		for (int32 i = 0; i < IntParametersToMove.Num(); ++i)
		{
			FloatParameters_DEPRECATED.AddDefaulted();
			FloatParameters_DEPRECATED.Last().ParameterName = IntParameters_DEPRECATED[IntParametersToMove[i]].ParameterName;
			FloatParameters_DEPRECATED.Last().ParameterValue = FCString::Atoi(*IntParameters_DEPRECATED[IntParametersToMove[i]].ParameterValueName);
			FloatParameters_DEPRECATED.Last().Id = IntParameters_DEPRECATED[IntParametersToMove[i]].Id;
		}

		// Remove them from the int params in reverse order
		for (int32 i = IntParametersToMove.Num() - 1; i >= 0; --i)
		{
			IntParameters_DEPRECATED.RemoveAt(IntParametersToMove[i]);
		}
	}
	
	if (CustomizableObjectCustomVersion < FCustomizableObjectCustomVersion::CustomizableObjectInstanceDescriptor)
	{
		Descriptor.CustomizableObject = CustomizableObject_DEPRECATED;

		Descriptor.BoolParameters = BoolParameters_DEPRECATED;
		Descriptor.IntParameters = IntParameters_DEPRECATED;
		Descriptor.FloatParameters = FloatParameters_DEPRECATED;
		Descriptor.TextureParameters = TextureParameters_DEPRECATED;
		Descriptor.VectorParameters = VectorParameters_DEPRECATED;
		Descriptor.ProjectorParameters = ProjectorParameters_DEPRECATED;
	}

	if (CustomizableObjectCustomVersion < FCustomizableObjectCustomVersion::DescriptorMultilayerProjectors)
	{
		Descriptor.MultilayerProjectors = MultilayerProjectors_DEPRECATED;
	}
}


void UCustomizableObjectInstance::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	PrivateData->BindObjectDelegates(nullptr, GetCustomizableObject());
#endif

	// Skip the cost of ReloadParameters in the cook commandlet; it will be reloaded during PreSave. For cooked runtime
	// and editor UI, reload on load because it will not otherwise reload unless the CustomizableObject recompiles.
	Descriptor.ReloadParameters();
	PrivateData->InitCustomizableObjectData(GetCustomizableObject());
}


FString UCustomizableObjectInstance::GetDesc()
{
	FString ObjectName = "Missing Object";
	if (UCustomizableObject* CustomizableObject = GetCustomizableObject())
	{
		ObjectName = CustomizableObject->GetName();
	}

	return FString::Printf(TEXT("Instance of [%s]"), *ObjectName);
}


int32 UCustomizableObjectInstance::GetProjectorValueRange(const FString& ParamName) const
{
	return Descriptor.GetProjectorValueRange(ParamName);
}


int32 UCustomizableObjectInstance::GetIntValueRange(const FString& ParamName) const
{
	return Descriptor.GetIntValueRange(ParamName);
}


int32 UCustomizableObjectInstance::GetFloatValueRange(const FString& ParamName) const
{
	return Descriptor.GetFloatValueRange(ParamName);
}


int32 UCustomizableObjectInstance::GetTextureValueRange(const FString& ParamName) const
{
	return Descriptor.GetTextureValueRange(ParamName);
}


// Only safe to call if the Mutable texture ref count system returns 0 and absolutely sure nobody holds a reference to the texture
void UCustomizableInstancePrivate::ReleaseMutableTexture(const FMutableImageCacheKey& MutableTextureKey, UTexture2D* Texture, FMutableResourceCache& Cache)
{
	if (ensure(Texture) && Texture->IsValidLowLevel())
	{
		Texture->ConditionalBeginDestroy();

		for (FTexture2DMipMap& Mip : Texture->GetPlatformData()->Mips)
		{
			Mip.BulkData.RemoveBulkData();
		}
	}

	// Must remove texture from cache since it has been released
	Cache.Images.Remove(MutableTextureKey);
}


void UCustomizableObjectInstance::SetObject(UCustomizableObject* InObject)
{
#if WITH_EDITOR
	// Bind a lambda to the PostCompileDelegate and unbind from the previous object if any.
	PrivateData->BindObjectDelegates(GetCustomizableObject(), InObject);
#endif

	Descriptor.SetCustomizableObject(*InObject);
	PrivateData->InitCustomizableObjectData(InObject);
}


UCustomizableObject* UCustomizableObjectInstance::GetCustomizableObject() const
{
	return Descriptor.CustomizableObject;
}


bool UCustomizableObjectInstance::GetBuildParameterRelevancy() const
{
	return Descriptor.GetBuildParameterRelevancy();
}


void UCustomizableObjectInstance::SetBuildParameterRelevancy(bool Value)
{
	Descriptor.SetBuildParameterRelevancy(Value);
}


int32 UCustomizableInstancePrivate::GetState() const
{
	return GetPublic()->Descriptor.GetState();
}


void UCustomizableInstancePrivate::SetState(const int32 InState)
{
	const int32 OldState = GetState();
	
	GetPublic()->Descriptor.SetState(InState);

	if (OldState != InState)
	{
		// State may change texture properties, so invalidate the texture reuse cache
		TextureReuseCache.Empty();
	}
}


FString UCustomizableObjectInstance::GetCurrentState() const
{
	return Descriptor.GetCurrentState();
}


void UCustomizableObjectInstance::SetCurrentState(const FString& StateName)
{
	Descriptor.SetCurrentState(StateName);
}


bool UCustomizableObjectInstance::IsParameterRelevant(int32 ParameterIndex) const
{
	// This should have been precalculated in the last update if the appropriate flag in the instance was set.
	return GetPrivate()->RelevantParameters.Contains(ParameterIndex);
}


bool UCustomizableObjectInstance::IsParameterRelevant(const FString& ParamName) const
{
	// This should have been precalculated in the last update if the appropriate flag in the instance was set.
	int32 ParameterIndexInObject = GetCustomizableObject()->FindParameter(ParamName);
	return GetPrivate()->RelevantParameters.Contains(ParameterIndexInObject);
}


void UCustomizableInstancePrivate::PostEditChangePropertyWithoutEditor()
{
	MUTABLE_CPUPROFILER_SCOPE(UCustomizableInstancePrivate::PostEditChangePropertyWithoutEditor);

	for (USkeletalMesh* SkeletalMesh : SkeletalMeshes)
	{
		if (SkeletalMesh && SkeletalMesh->GetResourceForRendering())
		{
			MUTABLE_CPUPROFILER_SCOPE(InitResources);

			// reinitialize resources
			SkeletalMesh->InitResources();
		}
	}
}


bool UCustomizableObjectInstance::CanUpdateInstance() const
{
	UCustomizableObject* CustomizableObject = GetCustomizableObject();
	if (!CustomizableObject)
	{
		return false;
	}

#if WITH_EDITOR
	return CustomizableObject->ConditionalAutoCompile();
#else
	return CustomizableObject->IsCompiled();
#endif
}


void UCustomizableObjectInstance::UpdateSkeletalMeshAsync(bool bIgnoreCloseDist, bool bForceHighPriority)
{
	UCustomizableObjectSystemPrivate* SystemPrivate = UCustomizableObjectSystem::GetInstance()->GetPrivate();

	const TSharedRef<FUpdateContextPrivate> Context = MakeShared<FUpdateContextPrivate>(*this);
	Context->bIgnoreCloseDist = bIgnoreCloseDist;
	Context->bForceHighPriority = bForceHighPriority;
	
	SystemPrivate->EnqueueUpdateSkeletalMesh(Context);
}


void UCustomizableObjectInstance::UpdateSkeletalMeshAsyncResult(FInstanceUpdateDelegate Callback, bool bIgnoreCloseDist, bool bForceHighPriority)
{
	UCustomizableObjectSystemPrivate* SystemPrivate = UCustomizableObjectSystem::GetInstance()->GetPrivate();

	const TSharedRef<FUpdateContextPrivate> Context = MakeShared<FUpdateContextPrivate>(*this);
	Context->bIgnoreCloseDist = bIgnoreCloseDist;
	Context->bForceHighPriority = bForceHighPriority;
	Context->UpdateCallback = Callback;
	
	SystemPrivate->EnqueueUpdateSkeletalMesh(Context);
}


void UCustomizableObjectInstance::UpdateSkeletalMeshAsyncResult(FInstanceUpdateNativeDelegate Callback, bool bIgnoreCloseDist, bool bForceHighPriority)
{
	UCustomizableObjectSystemPrivate* SystemPrivate = UCustomizableObjectSystem::GetInstance()->GetPrivate();

	const TSharedRef<FUpdateContextPrivate> Context = MakeShared<FUpdateContextPrivate>(*this);
	Context->bIgnoreCloseDist = bIgnoreCloseDist;
	Context->bForceHighPriority = bForceHighPriority;
	Context->UpdateNativeCallback = Callback;
	
	SystemPrivate->EnqueueUpdateSkeletalMesh(Context);
}

void UCustomizableInstancePrivate::TickUpdateCloseCustomizableObjects(UCustomizableObjectInstance& Public, FMutableInstanceUpdateMap& InOutRequestedUpdates)
{
	if (!Public.CanUpdateInstance())
	{
		return;
	}

	const UCustomizableObjectSystemPrivate* SystemPrivate = UCustomizableObjectSystem::GetInstance()->GetPrivate();	

	const EUpdateRequired UpdateRequired = SystemPrivate->IsUpdateRequired(Public, true, true, false);
	if (UpdateRequired != EUpdateRequired::NoUpdate) // Since this is done in the tick, avoid starting an update that we know for sure that would not be performed. Once started it has some performance implications that we want to avoid.
	{
		if (UpdateRequired == EUpdateRequired::Discard)
		{
			UCustomizableObjectSystem::GetInstance()->GetPrivate()->InitDiscardResourcesSkeletalMesh(&Public);
			InOutRequestedUpdates.Remove(&Public);
		}
		else if (UpdateRequired == EUpdateRequired::Update)
		{
			const EQueuePriorityType Priority = SystemPrivate->GetUpdatePriority(Public, false);

			FMutableUpdateCandidate* UpdateCandidate = InOutRequestedUpdates.Find(&Public);

			if (UpdateCandidate)
			{
				ensure(HasCOInstanceFlags(PendingLODsUpdate | PendingLODsDowngrade));

				UpdateCandidate->Priority = Priority;
				UpdateCandidate->Issue();
			}
			else
			{
				FMutableUpdateCandidate Candidate(&Public);
				Candidate.Priority = Priority;
				Candidate.Issue();
				InOutRequestedUpdates.Add(&Public, Candidate);
			}
		}
		else
		{
			check(false);
		}
	}
	else
	{
		InOutRequestedUpdates.Remove(&Public);
	}

	ClearCOInstanceFlags(PendingLODsUpdate | PendingLODsDowngrade);
}


void UCustomizableInstancePrivate::UpdateInstanceIfNotGenerated(UCustomizableObjectInstance& Public, FMutableInstanceUpdateMap& InOutRequestedUpdates)
{
	if (SkeletalMeshStatus != ESkeletalMeshStatus::NotGenerated)
	{
		return;
	}

	if (!Public.CanUpdateInstance())
	{
		return;
	}

	UCustomizableObjectSystemPrivate* SystemPrivate = UCustomizableObjectSystem::GetInstance()->GetPrivate();

	const TSharedRef<FUpdateContextPrivate> Context = MakeShared<FUpdateContextPrivate>(Public);
	Context->bOnlyUpdateIfNotGenerated = true;
	
	SystemPrivate->EnqueueUpdateSkeletalMesh(Context);

	EQueuePriorityType Priority = SystemPrivate->GetUpdatePriority(Public, false);
	FMutableUpdateCandidate* UpdateCandidate = InOutRequestedUpdates.Find(&Public);

	if (UpdateCandidate)
	{
		UpdateCandidate->Priority = Priority;
		UpdateCandidate->Issue();
	}
	else
	{
		FMutableUpdateCandidate Candidate(&Public);
		Candidate.Priority = Priority;
		Candidate.Issue();
		InOutRequestedUpdates.Add(&Public, Candidate);
	}
}


#if !UE_BUILD_SHIPPING
bool AreSkeletonsCompatible(const TArray<TObjectPtr<USkeleton>>& InSkeletons)
{
	MUTABLE_CPUPROFILER_SCOPE(AreSkeletonsCompatible);
	bool bCompatible = true;

	struct FBoneToMergeInfo
	{
		FBoneToMergeInfo(const uint32 InBonePathHash, const uint32 InSkeletonIndex, const uint32 InParentBoneSkeletonIndex) :
		BonePathHash(InBonePathHash), SkeletonIndex(InSkeletonIndex), ParentBoneSkeletonIndex(InParentBoneSkeletonIndex)
		{}

		uint32 BonePathHash = 0;
		uint32 SkeletonIndex = 0;
		uint32 ParentBoneSkeletonIndex = 0;
	};

	// Accumulated hierarchy hash from parent-bone to root bone
	TMap<FName, FBoneToMergeInfo> BoneNamesToBoneInfo;
	BoneNamesToBoneInfo.Reserve(InSkeletons[0] ? InSkeletons[0]->GetReferenceSkeleton().GetNum() : 0);
	
	for (int32 SkeletonIndex = 0; SkeletonIndex < InSkeletons.Num(); ++SkeletonIndex)
	{
		const TObjectPtr<USkeleton> Skeleton = InSkeletons[SkeletonIndex];
		check(Skeleton);

		const FReferenceSkeleton& ReferenceSkeleton = Skeleton->GetReferenceSkeleton();
		const TArray<FMeshBoneInfo>& Bones = ReferenceSkeleton.GetRawRefBoneInfo();
		const TArray<FTransform>& BonePoses = ReferenceSkeleton.GetRawRefBonePose();

		const int32 NumBones = Bones.Num();
		for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
		{
			const FMeshBoneInfo& Bone = Bones[BoneIndex];

			// Retrieve parent bone name and respective hash, root-bone is assumed to have a parent hash of 0
			const FName ParentName = Bone.ParentIndex != INDEX_NONE ? Bones[Bone.ParentIndex].Name : NAME_None;
			const uint32 ParentHash = Bone.ParentIndex != INDEX_NONE ? GetTypeHash(ParentName) : 0;

			// Look-up the path-hash from root to the parent bone
			const FBoneToMergeInfo* ParentBoneInfo = BoneNamesToBoneInfo.Find(ParentName);
			const uint32 ParentBonePathHash = ParentBoneInfo ? ParentBoneInfo->BonePathHash : 0;
			const uint32 ParentBoneSkeletonIndex = ParentBoneInfo ? ParentBoneInfo->SkeletonIndex : 0;

			// Append parent hash to path to give full path hash to current bone
			const uint32 BonePathHash = HashCombine(ParentBonePathHash, ParentHash);

			// Check if the bone exists in the hierarchy 
			const FBoneToMergeInfo* ExistingBoneInfo = BoneNamesToBoneInfo.Find(Bone.Name);

			// If the hash differs from the existing one it means skeletons are incompatible
			if (!ExistingBoneInfo)
			{
				// Add path hash to current bone
				BoneNamesToBoneInfo.Add(Bone.Name, FBoneToMergeInfo(BonePathHash, SkeletonIndex, ParentBoneSkeletonIndex));
			}
			else if (ExistingBoneInfo->BonePathHash != BonePathHash)
			{
				if (bCompatible)
				{
					// Print the skeletons to merge
					FString Msg = TEXT("Failed to merge skeletons. Skeletons to merge: ");
					for (int32 AuxSkeletonIndex = 0; AuxSkeletonIndex < InSkeletons.Num(); ++AuxSkeletonIndex)
					{
						if (InSkeletons[AuxSkeletonIndex] != nullptr)
						{
							Msg += FString::Printf(TEXT("\n\t- %s"), *InSkeletons[AuxSkeletonIndex].GetName());
						}
					}

					UE_LOG(LogMutable, Error, TEXT("%s"), *Msg);

#if WITH_EDITOR
					FNotificationInfo Info(FText::FromString(TEXT("Mutable: Failed to merge skeletons. Invalid parent chain detected. Please check the output log for more information.")));
					Info.bFireAndForget = true;
					Info.FadeOutDuration = 1.0f;
					Info.ExpireDuration = 10.0f;
					FSlateNotificationManager::Get().AddNotification(Info);
#endif

					bCompatible = false;
				}
				
				// Print the first non compatible bone in the bone chain, since all child bones will be incompatible too.
				if (ExistingBoneInfo->ParentBoneSkeletonIndex != SkeletonIndex)
				{
					// Different skeletons can't be used if they are incompatible with the reference skeleton.
					UE_LOG(LogMutable, Error, TEXT("[%s] parent bone is different in skeletons [%s] and [%s]."),
						*Bone.Name.ToString(),
						*InSkeletons[SkeletonIndex]->GetName(),
						*InSkeletons[ExistingBoneInfo->ParentBoneSkeletonIndex]->GetName());
				}
			}
		}
	}

	return bCompatible;
}
#endif


USkeleton* UCustomizableInstancePrivate::MergeSkeletons(UCustomizableObject& CustomizableObject, const FMutableRefSkeletalMeshData& RefSkeletalMeshData, int32 ComponentIndex)
{
	MUTABLE_CPUPROFILER_SCOPE(BuildSkeletonData_MergeSkeletons);

	FCustomizableInstanceComponentData* ComponentData = GetComponentData(ComponentIndex);
	check(ComponentData);

	FReferencedSkeletons& ReferencedSkeletons = ComponentData->Skeletons;
	
	// Merged skeleton found in the cache
	if (ReferencedSkeletons.Skeleton)
	{
		USkeleton* MergedSkeleton = ReferencedSkeletons.Skeleton;
		ReferencedSkeletons.Skeleton = nullptr;
		return MergedSkeleton;
	}

	// No need to merge skeletons
	if(ReferencedSkeletons.SkeletonsToMerge.Num() == 1)
	{
		const TObjectPtr<USkeleton> RefSkeleton = ReferencedSkeletons.SkeletonsToMerge[0];
		ReferencedSkeletons.SkeletonIds.Empty();
		ReferencedSkeletons.SkeletonsToMerge.Empty();
		return RefSkeleton;
	}

#if !UE_BUILD_SHIPPING
	// Test Skeleton compatibility before attempting the merge to avoid a crash.
	if (!AreSkeletonsCompatible(ReferencedSkeletons.SkeletonsToMerge))
	{
		return nullptr;
	}
#endif

	FSkeletonMergeParams Params;
	Exchange(Params.SkeletonsToMerge, ReferencedSkeletons.SkeletonsToMerge);

	USkeleton* FinalSkeleton = USkeletalMergingLibrary::MergeSkeletons(Params);
	if (!FinalSkeleton)
	{
		FString Msg = FString::Printf(TEXT("MergeSkeletons failed for Customizable Object [%s], Instance [%s]. Skeletons involved: "),
			*CustomizableObject.GetName(),
			*GetOuter()->GetName());
		
		const int32 SkeletonCount = Params.SkeletonsToMerge.Num();
		for (int32 SkeletonIndex = 0; SkeletonIndex < SkeletonCount; ++SkeletonIndex)
		{
			Msg += FString::Printf(TEXT(" [%s]"), *Params.SkeletonsToMerge[SkeletonIndex]->GetName());
		}
		
		UE_LOG(LogMutable, Error, TEXT("%s"), *Msg);
	}
	else
	{
		// Make the final skeleton compatible with all the merged skeletons and their compatible skeletons.
		for (USkeleton* Skeleton : Params.SkeletonsToMerge)
		{
			if (Skeleton)
			{
				FinalSkeleton->AddCompatibleSkeleton(Skeleton);

				const TArray<TSoftObjectPtr<USkeleton>>& CompatibleSkeletons = Skeleton->GetCompatibleSkeletons();
				for (const TSoftObjectPtr<USkeleton>& CompatibleSkeleton : CompatibleSkeletons)
				{
					FinalSkeleton->AddCompatibleSkeletonSoft(CompatibleSkeleton);
				}
			}
		}

		// Add Skeleton to the cache
		CustomizableObject.GetPrivate()->SkeletonCache.Add(ReferencedSkeletons.SkeletonIds, FinalSkeleton);
		ReferencedSkeletons.SkeletonIds.Empty();
	}
	
	return FinalSkeleton;
}

namespace
{
	FKAggregateGeom MakeAggGeomFromMutablePhysics(int32 BodyIndex, const mu::PhysicsBody* MutablePhysicsBody)
	{
		FKAggregateGeom BodyAggGeom;

		auto GetCollisionEnabledFormFlags = [](uint32 Flags) -> ECollisionEnabled::Type
		{
			return ECollisionEnabled::Type(Flags & 0xFF);
		};

		auto GetContributeToMassFromFlags = [](uint32 Flags) -> bool
		{
			return static_cast<bool>((Flags >> 8) & 1);
		};

		const int32 NumSpheres = MutablePhysicsBody->GetSphereCount(BodyIndex);
		TArray<FKSphereElem>& AggSpheres = BodyAggGeom.SphereElems;
		AggSpheres.Empty(NumSpheres);
		for (int32 I = 0; I < NumSpheres; ++I)
		{
			uint32 Flags = MutablePhysicsBody->GetSphereFlags(BodyIndex, I);
			FString Name = MutablePhysicsBody->GetSphereName(BodyIndex, I);

			FVector3f Position;
			float Radius;

			MutablePhysicsBody->GetSphere(BodyIndex, I, Position, Radius);
			FKSphereElem& NewElem = AggSpheres.AddDefaulted_GetRef();
			
			NewElem.Center = FVector(Position);
			NewElem.Radius = Radius;
			NewElem.SetContributeToMass(GetContributeToMassFromFlags(Flags));
			NewElem.SetCollisionEnabled(GetCollisionEnabledFormFlags(Flags));
			NewElem.SetName(FName(*Name));
		}
		
		const int32 NumBoxes = MutablePhysicsBody->GetBoxCount(BodyIndex);
		TArray<FKBoxElem>& AggBoxes = BodyAggGeom.BoxElems;
		AggBoxes.Empty(NumBoxes);
		for (int32 I = 0; I < NumBoxes; ++I)
		{
			uint32 Flags = MutablePhysicsBody->GetBoxFlags(BodyIndex, I);
			FString Name = MutablePhysicsBody->GetBoxName(BodyIndex, I);

			FVector3f Position;
			FQuat4f Orientation
				;
			FVector3f Size;
			MutablePhysicsBody->GetBox(BodyIndex, I, Position, Orientation, Size);

			FKBoxElem& NewElem = AggBoxes.AddDefaulted_GetRef();
			
			NewElem.Center = FVector(Position);
			NewElem.Rotation = FRotator(Orientation.Rotator());
			NewElem.X = Size.X;
			NewElem.Y = Size.Y;
			NewElem.Z = Size.Z;
			NewElem.SetContributeToMass(GetContributeToMassFromFlags(Flags));
			NewElem.SetCollisionEnabled(GetCollisionEnabledFormFlags(Flags));
			NewElem.SetName(FName(*Name));
		}

		//const int32 NumConvexes = MutablePhysicsBody->GetConvexCount( BodyIndex );
		//TArray<FKConvexElem>& AggConvexes = BodyAggGeom.ConvexElems;
		//AggConvexes.Empty();
		//for (int32 I = 0; I < NumConvexes; ++I)
		//{
		//	uint32 Flags = MutablePhysicsBody->GetConvexFlags( BodyIndex, I );
		//	FString Name = MutablePhysicsBody->GetConvexName( BodyIndex, I );

		//	const FVector3f* Vertices;
		//	const int32* Indices;
		//	int32 NumVertices;
		//	int32 NumIndices;
		//	FTransform3f Transform;

		//	MutablePhysicsBody->GetConvex( BodyIndex, I, Vertices, NumVertices, Indices, NumIndices, Transform );
		//	
		//	TArrayView<const FVector3f> VerticesView( Vertices, NumVertices );
		//	TArrayView<const int32> IndicesView( Indices, NumIndices );
		//}

		TArray<FKSphylElem>& AggSphyls = BodyAggGeom.SphylElems;
		const int32 NumSphyls = MutablePhysicsBody->GetSphylCount(BodyIndex);
		AggSphyls.Empty(NumSphyls);

		for (int32 I = 0; I < NumSphyls; ++I)
		{
			uint32 Flags = MutablePhysicsBody->GetSphylFlags(BodyIndex, I);
			FString Name = MutablePhysicsBody->GetSphylName(BodyIndex, I);

			FVector3f Position;
			FQuat4f Orientation;
			float Radius;
			float Length;

			MutablePhysicsBody->GetSphyl(BodyIndex, I, Position, Orientation, Radius, Length);

			FKSphylElem& NewElem = AggSphyls.AddDefaulted_GetRef();
			
			NewElem.Center = FVector(Position);
			NewElem.Rotation = FRotator(Orientation.Rotator());
			NewElem.Radius = Radius;
			NewElem.Length = Length;
			
			NewElem.SetContributeToMass(GetContributeToMassFromFlags(Flags));
			NewElem.SetCollisionEnabled(GetCollisionEnabledFormFlags(Flags));
			NewElem.SetName(FName(*Name));
		}	

		TArray<FKTaperedCapsuleElem>& AggTaperedCapsules = BodyAggGeom.TaperedCapsuleElems;
		const int32 NumTaperedCapsules = MutablePhysicsBody->GetTaperedCapsuleCount(BodyIndex);
		AggTaperedCapsules.Empty(NumTaperedCapsules);

		for (int32 I = 0; I < NumTaperedCapsules; ++I)
		{
			uint32 Flags = MutablePhysicsBody->GetTaperedCapsuleFlags(BodyIndex, I);
			FString Name = MutablePhysicsBody->GetTaperedCapsuleName(BodyIndex, I);

			FVector3f Position;
			FQuat4f Orientation;
			float Radius0;
			float Radius1;
			float Length;

			MutablePhysicsBody->GetTaperedCapsule(BodyIndex, I, Position, Orientation, Radius0, Radius1, Length);

			FKTaperedCapsuleElem& NewElem = AggTaperedCapsules.AddDefaulted_GetRef();
			
			NewElem.Center = FVector(Position);
			NewElem.Rotation = FRotator(Orientation.Rotator());
			NewElem.Radius0 = Radius0;
			NewElem.Radius1 = Radius1;
			NewElem.Length = Length;
			
			NewElem.SetContributeToMass(GetContributeToMassFromFlags(Flags));
			NewElem.SetCollisionEnabled(GetCollisionEnabledFormFlags(Flags));
			NewElem.SetName(FName(*Name));
		}	

		return BodyAggGeom;
	};

	TObjectPtr<UPhysicsAsset> MakePhysicsAssetFromTemplateAndMutableBody(
		TObjectPtr<UPhysicsAsset> TemplateAsset, const mu::PhysicsBody* MutablePhysics, const UCustomizableObject& CustomizableObject) 
	{
		check(TemplateAsset);
		TObjectPtr<UPhysicsAsset> Result = NewObject<UPhysicsAsset>();

		if (!Result)
		{
			return nullptr;
		}

		Result->SolverSettings = TemplateAsset->SolverSettings;
		Result->SolverType = TemplateAsset->SolverType;

		Result->bNotForDedicatedServer = TemplateAsset->bNotForDedicatedServer;

		const FModelResources& ModelResources = CustomizableObject.GetPrivate()->GetModelResources();
		TMap<FName, int32> BonesInUse;

		const int32 MutablePhysicsBodyCount = MutablePhysics->GetBodyCount();
		BonesInUse.Reserve(MutablePhysicsBodyCount);
		for ( int32 I = 0; I < MutablePhysicsBodyCount; ++I )
		{
			const uint16 BoneNameId = MutablePhysics->GetBodyBoneId(I);
			if (ModelResources.BoneNames.IsValidIndex(BoneNameId))
			{
				FName BoneName = ModelResources.BoneNames[BoneNameId];
				BonesInUse.Add(BoneName, I);
			}
		}

		const int32 PhysicsAssetBodySetupNum = TemplateAsset->SkeletalBodySetups.Num();
		bool bTemplateBodyNotUsedFound = false;

		TArray<uint8> UsageMap;
		UsageMap.Init(1, PhysicsAssetBodySetupNum);

		for (int32 BodySetupIndex = 0; BodySetupIndex < PhysicsAssetBodySetupNum; ++BodySetupIndex)
		{
			const TObjectPtr<USkeletalBodySetup>& BodySetup = TemplateAsset->SkeletalBodySetups[BodySetupIndex];

			int32* MutableBodyIndex = BonesInUse.Find(BodySetup->BoneName);
			if (!MutableBodyIndex)
			{
				bTemplateBodyNotUsedFound = true;
				UsageMap[BodySetupIndex] = 0;
				continue;
			}

			TObjectPtr<USkeletalBodySetup> NewBodySetup = NewObject<USkeletalBodySetup>(Result);
			NewBodySetup->BodySetupGuid = FGuid::NewGuid();

			// Copy Body properties 	
			NewBodySetup->BoneName = BodySetup->BoneName;
			NewBodySetup->PhysicsType = BodySetup->PhysicsType;
			NewBodySetup->bConsiderForBounds = BodySetup->bConsiderForBounds;
			NewBodySetup->bMeshCollideAll = BodySetup->bMeshCollideAll;
			NewBodySetup->bDoubleSidedGeometry = BodySetup->bDoubleSidedGeometry;
			NewBodySetup->bGenerateNonMirroredCollision = BodySetup->bGenerateNonMirroredCollision;
			NewBodySetup->bSharedCookedData = BodySetup->bSharedCookedData;
			NewBodySetup->bGenerateMirroredCollision = BodySetup->bGenerateMirroredCollision;
			NewBodySetup->PhysMaterial = BodySetup->PhysMaterial;
			NewBodySetup->CollisionReponse = BodySetup->CollisionReponse;
			NewBodySetup->CollisionTraceFlag = BodySetup->CollisionTraceFlag;
			NewBodySetup->DefaultInstance = BodySetup->DefaultInstance;
			NewBodySetup->WalkableSlopeOverride = BodySetup->WalkableSlopeOverride;
			NewBodySetup->BuildScale3D = BodySetup->BuildScale3D;
			NewBodySetup->bSkipScaleFromAnimation = BodySetup->bSkipScaleFromAnimation;

			// PhysicalAnimationProfiles can't be added with the current UPhysicsAsset API outside the editor.
			// Don't pouplate them for now.	
			//NewBodySetup->PhysicalAnimationData = BodySetup->PhysicalAnimationData;

			NewBodySetup->AggGeom = MakeAggGeomFromMutablePhysics(*MutableBodyIndex, MutablePhysics);

			Result->SkeletalBodySetups.Add(NewBodySetup);
		}

		if (!bTemplateBodyNotUsedFound)
		{
			Result->CollisionDisableTable = TemplateAsset->CollisionDisableTable;
			Result->ConstraintSetup = TemplateAsset->ConstraintSetup;
		}
		else
		{
			// Recreate the collision disable entry
			Result->CollisionDisableTable.Reserve(TemplateAsset->CollisionDisableTable.Num());
			for (const TPair<FRigidBodyIndexPair, bool>& CollisionDisableEntry : TemplateAsset->CollisionDisableTable)
			{
				const bool bIndex0Used = UsageMap[CollisionDisableEntry.Key.Indices[0]] > 0;
				const bool bIndex1Used = UsageMap[CollisionDisableEntry.Key.Indices[1]] > 0;

				if (bIndex0Used && bIndex1Used)
				{
					Result->CollisionDisableTable.Add(CollisionDisableEntry);
				}
			}

			// Only add constraints that are part of the bones used for the mutable physics volumes description.
			for (const TObjectPtr<UPhysicsConstraintTemplate>& Constrain : TemplateAsset->ConstraintSetup)
			{
				const FName BoneA = Constrain->DefaultInstance.ConstraintBone1;
				const FName BoneB = Constrain->DefaultInstance.ConstraintBone2;

				if (BonesInUse.Find(BoneA) && BonesInUse.Find(BoneB))
				{
					Result->ConstraintSetup.Add(Constrain);
				}	
			}
		}
		
		Result->ConstraintSetup = TemplateAsset->ConstraintSetup;
		
		Result->UpdateBodySetupIndexMap();
		Result->UpdateBoundsBodiesArray();



	#if WITH_EDITORONLY_DATA
		Result->ConstraintProfiles = TemplateAsset->ConstraintProfiles;
	#endif

		return Result;
	}
}

UPhysicsAsset* UCustomizableInstancePrivate::GetOrBuildMainPhysicsAsset(
	TObjectPtr<UPhysicsAsset> TemplateAsset,
	const mu::PhysicsBody* MutablePhysics,
	const UCustomizableObject& CustomizableObject,
	int32 ComponentId,
	bool bDisableCollisionsBetweenDifferentAssets)
{

	MUTABLE_CPUPROFILER_SCOPE(MergePhysicsAssets);

	check(MutablePhysics);

	UPhysicsAsset* Result = nullptr;

	FCustomizableInstanceComponentData* ComponentData = GetComponentData(ComponentId);
	check(ComponentData);

	TArray<TObjectPtr<UPhysicsAsset>>& PhysicsAssets = ComponentData->PhysicsAssets.PhysicsAssetsToMerge;

	TArray<TObjectPtr<UPhysicsAsset>> ValidAssets;

	const int32 NumPhysicsAssets = ComponentData->PhysicsAssets.PhysicsAssetsToMerge.Num();
	for (int32 I = 0; I < NumPhysicsAssets; ++I)
	{
		const TObjectPtr<UPhysicsAsset>& PhysicsAsset = ComponentData->PhysicsAssets.PhysicsAssetsToMerge[I];

		if (PhysicsAsset)
		{
			ValidAssets.AddUnique(PhysicsAsset);
		}
	}

	if (!ValidAssets.Num())
	{
		return Result;
	}

	// Just get the referenced asset if no recontrution or merge is needed.
	if (ValidAssets.Num() == 1 && !MutablePhysics->bBodiesModified)
	{
		return ValidAssets[0];
	}

	TemplateAsset = TemplateAsset ? TemplateAsset : ValidAssets[0];
	check(TemplateAsset);

	Result = NewObject<UPhysicsAsset>();

	if (!Result)
	{
		return nullptr;
	}

	Result->SolverSettings = TemplateAsset->SolverSettings;
	Result->SolverType = TemplateAsset->SolverType;

	Result->bNotForDedicatedServer = TemplateAsset->bNotForDedicatedServer;

	const FModelResources& ModelResources = CustomizableObject.GetPrivate()->GetModelResources();
	TMap<FName, int32> BonesInUse;

	const int32 MutablePhysicsBodyCount = MutablePhysics->GetBodyCount();
	BonesInUse.Reserve(MutablePhysicsBodyCount);
	for ( int32 I = 0; I < MutablePhysicsBodyCount; ++I )
	{
		const uint16 BoneNameId = MutablePhysics->GetBodyBoneId(I);
		if (ModelResources.BoneNames.IsValidIndex(BoneNameId))
		{
			FName BoneName = ModelResources.BoneNames[BoneNameId];
			BonesInUse.Add(BoneName, I);
		}
	}

	// Each array is a set of elements that can collide  
	TArray<TArray<int32, TInlineAllocator<8>>> CollisionSets;

	// {SetIndex, ElementInSetIndex, BodyIndex}
	using CollisionSetEntryType = TTuple<int32, int32, int32>;	
	// Map from BodyName/BoneName to set and index in set.
	TMap<FName, CollisionSetEntryType> BodySetupSetMap;
	
	// {SetIndex0, SetIndex1, BodyIndex}
	using MultiSetCollisionEnableType = TTuple<int32, int32, int32>;

	// Only for elements that belong to two or more differnet sets. 
	// Contains in which set the elements belong.
	using MultiSetArrayType = TArray<int32, TInlineAllocator<4>>;
	TMap<int32, MultiSetArrayType> MultiCollisionSets;
	TArray<TArray<int32>> SetsIndexMap;

	CollisionSets.SetNum(ValidAssets.Num());
	SetsIndexMap.SetNum(CollisionSets.Num());

	TMap<FRigidBodyIndexPair, bool> CollisionDisableTable;

	// New body index
	int32 CurrentBodyIndex = 0;
	for (int32 CollisionSetIndex = 0;  CollisionSetIndex < ValidAssets.Num(); ++CollisionSetIndex)
	{
		const int32 PhysicsAssetBodySetupNum = ValidAssets[CollisionSetIndex]->SkeletalBodySetups.Num();
		SetsIndexMap[CollisionSetIndex].Init(-1, PhysicsAssetBodySetupNum);

		for (int32 BodySetupIndex = 0; BodySetupIndex < PhysicsAssetBodySetupNum; ++BodySetupIndex) 
		{
			const TObjectPtr<USkeletalBodySetup>& BodySetup = ValidAssets[CollisionSetIndex]->SkeletalBodySetups[BodySetupIndex];
			
			int32* MutableBodyIndex = BonesInUse.Find(BodySetup->BoneName);
			if (!MutableBodyIndex)
			{
				continue;
			}

			CollisionSetEntryType* Found = BodySetupSetMap.Find(BodySetup->BoneName);

			if (!Found)
			{
				TObjectPtr<USkeletalBodySetup> NewBodySetup = NewObject<USkeletalBodySetup>(Result);
				NewBodySetup->BodySetupGuid = FGuid::NewGuid();
			
				// Copy Body properties 	
				NewBodySetup->BoneName = BodySetup->BoneName;
				NewBodySetup->PhysicsType = BodySetup->PhysicsType;
				NewBodySetup->bConsiderForBounds = BodySetup->bConsiderForBounds;
				NewBodySetup->bMeshCollideAll = BodySetup->bMeshCollideAll;
				NewBodySetup->bDoubleSidedGeometry = BodySetup->bDoubleSidedGeometry;
				NewBodySetup->bGenerateNonMirroredCollision = BodySetup->bGenerateNonMirroredCollision;
				NewBodySetup->bSharedCookedData = BodySetup->bSharedCookedData;
				NewBodySetup->bGenerateMirroredCollision = BodySetup->bGenerateMirroredCollision;
				NewBodySetup->PhysMaterial = BodySetup->PhysMaterial;
				NewBodySetup->CollisionReponse = BodySetup->CollisionReponse;
				NewBodySetup->CollisionTraceFlag = BodySetup->CollisionTraceFlag;
				NewBodySetup->DefaultInstance = BodySetup->DefaultInstance;
				NewBodySetup->WalkableSlopeOverride = BodySetup->WalkableSlopeOverride;
				NewBodySetup->BuildScale3D = BodySetup->BuildScale3D;	
				NewBodySetup->bSkipScaleFromAnimation = BodySetup->bSkipScaleFromAnimation;
				
				// PhysicalAnimationProfiles can't be added with the current UPhysicsAsset API outside the editor.
				// Don't poulate them for now.	
				//NewBodySetup->PhysicalAnimationData = BodySetup->PhysicalAnimationData;

				NewBodySetup->AggGeom = MakeAggGeomFromMutablePhysics(*MutableBodyIndex, MutablePhysics);


				Result->SkeletalBodySetups.Add(NewBodySetup);
				
				int32 IndexInSet = CollisionSets[CollisionSetIndex].Add(CurrentBodyIndex);
				BodySetupSetMap.Add(BodySetup->BoneName, {CollisionSetIndex, IndexInSet, CurrentBodyIndex});
				SetsIndexMap[CollisionSetIndex][IndexInSet] = CurrentBodyIndex;

				++CurrentBodyIndex;
			}
			else
			{
				int32 FoundCollisionSetIndex = Found->Get<0>();
				int32 FoundCollisionSetElemIndex = Found->Get<1>();
				int32 FoundBodyIndex = Found->Get<2>();
				
				// No need to add the body again. Volumes that come form mutable are already merged.
				// here we only need to merge properies.
				// TODO: check if there is other properties worth merging. In case of conflict select the more restrivtive one? 
				Result->SkeletalBodySetups[FoundBodyIndex]->bConsiderForBounds |= BodySetup->bConsiderForBounds;

				// Mark as removed so no indices are invalidated.
				CollisionSets[FoundCollisionSetIndex][FoundCollisionSetElemIndex] = INDEX_NONE;
				// Add Elem to the set but mark it as removed so we have an index for remapping.
				int32 IndexInSet = CollisionSets[CollisionSetIndex].Add(INDEX_NONE);	
				SetsIndexMap[CollisionSetIndex][IndexInSet] = FoundBodyIndex;
				
				MultiSetArrayType& Sets = MultiCollisionSets.FindOrAdd(FoundBodyIndex);

				// The first time there is a collision (MultSet is empty), add the colliding element set
				// as well as the current set.
				if (!Sets.Num())
				{
					Sets.Add(FoundCollisionSetIndex);
				}
				
				Sets.Add(CollisionSetIndex);
			}
		}
	
		// Remap collision indices removing invalid ones.

		CollisionDisableTable.Reserve(CollisionDisableTable.Num() + ValidAssets[CollisionSetIndex]->CollisionDisableTable.Num());
		for (const TPair<FRigidBodyIndexPair, bool>& DisabledCollision : ValidAssets[CollisionSetIndex]->CollisionDisableTable)
		{
			int32 MappedIdx0 = SetsIndexMap[CollisionSetIndex][DisabledCollision.Key.Indices[0]];
			int32 MappedIdx1 = SetsIndexMap[CollisionSetIndex][DisabledCollision.Key.Indices[1]];

			// This will generate correct disables for the case when two shapes from different sets
			// are meged to the same setup. Will introduce repeated pairs, but this is not a problem.

			// Currenly if two bodies / bones have disabled collision in one of the merged assets, the collision
			// will remain disabled even if other merges allow it.   
			if ( MappedIdx0 != INDEX_NONE && MappedIdx1 != INDEX_NONE )
			{
				CollisionDisableTable.Add({MappedIdx0, MappedIdx1}, DisabledCollision.Value);
			}
		}

		// Only add constraints that are part of the bones used for the mutable physics volumes description.
		for (const TObjectPtr<UPhysicsConstraintTemplate>& Constrain : ValidAssets[CollisionSetIndex]->ConstraintSetup)
		{
			FName BoneA = Constrain->DefaultInstance.ConstraintBone1;
			FName BoneB = Constrain->DefaultInstance.ConstraintBone2;

			if (BonesInUse.Find(BoneA) && BonesInUse.Find(BoneB))
			{
				Result->ConstraintSetup.Add(Constrain);
			}
		}

#if WITH_EDITORONLY_DATA
		Result->ConstraintProfiles.Append(ValidAssets[CollisionSetIndex]->ConstraintProfiles);
#endif
	}

	if (bDisableCollisionsBetweenDifferentAssets)
	{
		// Compute collision disable table size upperbound to reduce number of alloactions.
		int32 CollisionDisableTableSize = 0;
		for (int32 S0 = 1; S0 < CollisionSets.Num(); ++S0)
		{
			for (int32 S1 = 0; S1 < S0; ++S1)
			{	
				CollisionDisableTableSize += CollisionSets[S1].Num() * CollisionSets[S0].Num();
			}
		}

		// We already may have elements in the table, but at the moment of 
		// addition we don't know yet the final number of elements.
		// Now a good number of elements will be added and because we know the final number of elements
		// an upperbound to the number of interactions can be computed and reserved. 
		CollisionDisableTable.Reserve(CollisionDisableTableSize);

		// Generate disable collision entry for every element in Set S0 for every element in Set S1 
		// that are not in multiple sets.
		for (int32 S0 = 1; S0 < CollisionSets.Num(); ++S0)
		{
			for (int32 S1 = 0; S1 < S0; ++S1)
			{	
				for (int32 Set0Elem : CollisionSets[S0])
				{
					// Element present in more than one set, will be treated later.
					if (Set0Elem == INDEX_NONE)
					{
						continue;
					}

					for (int32 Set1Elem : CollisionSets[S1])
					{
						// Element present in more than one set, will be treated later.
						if (Set1Elem == INDEX_NONE)
						{
							continue;
						}
						CollisionDisableTable.Add(FRigidBodyIndexPair{Set0Elem, Set1Elem}, false);
					}
				}
			}
		}

		// Process elements that belong to multiple sets that have been merged to the same element.
		for ( const TPair<int32, MultiSetArrayType>& Sets : MultiCollisionSets )
		{
			for (int32 S = 0; S < CollisionSets.Num(); ++S)
			{
				if (!Sets.Value.Contains(S))
				{	
					for (int32 SetElem : CollisionSets[S])
					{
						if (SetElem != INDEX_NONE)
						{
							CollisionDisableTable.Add(FRigidBodyIndexPair{Sets.Key, SetElem}, false);
						}
					}
				}
			}
		}

		CollisionDisableTable.Shrink();
	}

	Result->CollisionDisableTable = MoveTemp(CollisionDisableTable);
	Result->UpdateBodySetupIndexMap();
	Result->UpdateBoundsBodiesArray();

	ComponentData->PhysicsAssets.PhysicsAssetsToMerge.Empty();

	return Result;
}


static float MutableMeshesMinUVChannelDensity = 100.f;
FAutoConsoleVariableRef CVarMutableMeshesMinUVChannelDensity(
	TEXT("Mutable.MinUVChannelDensity"),
	MutableMeshesMinUVChannelDensity,
	TEXT("Min UV density to set on generated meshes. This value will influence the requested texture mip to stream in. Higher values will result in higher quality mips being streamed in earlier."));


void SetMeshUVChannelDensity(FMeshUVChannelInfo& UVChannelInfo, float Density = 0.f)
{
	Density = Density > 0.f ? Density : 150.f;
	Density = FMath::Max(MutableMeshesMinUVChannelDensity, Density);

	UVChannelInfo.bInitialized = true;
	UVChannelInfo.bOverrideDensities = false;

	for (int32 i = 0; i < TEXSTREAM_MAX_NUM_UVCHANNELS; ++i)
	{
		UVChannelInfo.LocalUVDensities[i] = Density;
	}
}


bool UCustomizableInstancePrivate::DoComponentsNeedUpdate(UCustomizableObjectInstance* Public, const TSharedRef<FUpdateContextPrivate>& OperationData, bool& bHasInvalidMesh)
{
	MUTABLE_CPUPROFILER_SCOPE(UCustomizableInstancePrivate::DoComponentsNeedUpdate);

	UCustomizableObject* CustomizableObject = Public->GetCustomizableObject();
	check(CustomizableObject);

	const int32 NumComponents = CustomizableObject->GetComponentCount();

	TArray<bool> ComponentWithMesh;
	ComponentWithMesh.Init(false, NumComponents);

	TArray<mu::FResourceID> MeshIDs;
	MeshIDs.Init(MAX_uint64, NumComponents * MAX_MESH_LOD_COUNT);

	// Gather the Mesh Ids of all components, and validate the integrity of the meshes to generate. 
	for (int32 LODIndex = OperationData->GetMinLOD(); LODIndex < OperationData->NumLODsAvailable; ++LODIndex)
	{
		const FInstanceUpdateData::FLOD& LOD = OperationData->InstanceUpdateData.LODs[LODIndex];

		for (int32 ComponentIndex = 0; ComponentIndex < LOD.ComponentCount; ++ComponentIndex)
		{
			const FInstanceUpdateData::FComponent& Component = OperationData->InstanceUpdateData.Components[LOD.FirstComponent + ComponentIndex];

			if (Component.Id >= NumComponents)
			{
				ensureMsgf(false, TEXT("Mutable: Failed to generate SkeletalMesh. Invalid ComponentIndex. Index [%d]. Valid range [0 .. %d)."), Component.Id, NumComponents);
				bHasInvalidMesh = true;
				return false;
			}

			if (!Component.bGenerated || !Component.Mesh || Component.SurfaceCount == 0) // else
			{
				continue;
			}

			// Unreal does not support empty sections.
			if (Component.Mesh->GetVertexCount() == 0) // else
			{
				UE_LOG(LogMutable, Error, TEXT("Failed to generate SkeletalMesh for CO Instance [%s]. CO [%s] has invalid geometry for LOD [%d] Component [%d]."),
					*Public->GetName(), *CustomizableObject->GetName(),
					LODIndex, ComponentIndex);
				bHasInvalidMesh = true;
				continue;
			}

			ComponentWithMesh[Component.Id] = true;
			MeshIDs[Component.Id * MAX_MESH_LOD_COUNT + LODIndex] = Component.MeshID;
		}
	}

	// Find which components need an update
	OperationData->MeshChanged.AddDefaulted(NumComponents);

	for (int32 ComponentIndex = 0; ComponentIndex < NumComponents; ++ComponentIndex)
	{
		if (OperationData->bUseMeshCache)
		{
			USkeletalMesh* CachedMesh = CustomizableObject->GetPrivate()->MeshCache.Get(OperationData->MeshDescriptors[ComponentIndex]);
			if (CachedMesh)
			{
				const bool bMeshNeedsUpdate = !SkeletalMeshes.IsValidIndex(ComponentIndex) || (SkeletalMeshes[ComponentIndex] != CachedMesh);
				OperationData->MeshChanged[ComponentIndex] = bMeshNeedsUpdate;
				ComponentWithMesh[ComponentIndex] = true;
				continue;
			}
		}

		// Components with mesh must have valid geometry at CurrentMaxLOD
		if (ComponentWithMesh[ComponentIndex] && MeshIDs[ComponentIndex * MAX_MESH_LOD_COUNT + OperationData->NumLODsAvailable - 1] == MAX_uint64)
		{
			UE_LOG(LogMutable, Error, TEXT("Failed to generate SkeletalMesh for CO Instance [%s]. CO [%s] is missing geometry for LOD [%d] Component [%d]."),
				*Public->GetName(), *CustomizableObject->GetName(),
				OperationData->NumLODsAvailable - 1, ComponentIndex);
			bHasInvalidMesh = true;
			continue;
		}

		// Update the component if there is a mesh and it shouldn't, or the other way around.
		const bool bHasSkeletalMesh = SkeletalMeshes.IsValidIndex(ComponentIndex) && SkeletalMeshes[ComponentIndex];
		OperationData->MeshChanged[ComponentIndex] = (ComponentWithMesh[ComponentIndex] != bHasSkeletalMesh);

		const FCustomizableInstanceComponentData* ComponentData = GetComponentData(ComponentIndex);
		if (!ComponentData) // Could be nullptr if the component has not been generated.
		{
			continue;
		}

		// Update if MeshIDs are different
		const int32 ComponentOffset = ComponentIndex * MAX_MESH_LOD_COUNT;
		for (int32 MeshIndex = 0; !OperationData->MeshChanged[ComponentIndex] && MeshIndex < MAX_MESH_LOD_COUNT; ++MeshIndex)
		{
			OperationData->MeshChanged[ComponentIndex] = MeshIDs[ComponentOffset + MeshIndex] != ComponentData->LastMeshIdPerLOD[MeshIndex];
		}
	}

	return !bHasInvalidMesh && OperationData->MeshChanged.Find(true) != INDEX_NONE;
}


bool UCustomizableInstancePrivate::UpdateSkeletalMesh_PostBeginUpdate0(UCustomizableObjectInstance* Public, const TSharedRef<FUpdateContextPrivate>& OperationData)
{
	MUTABLE_CPUPROFILER_SCOPE(UCustomizableInstancePrivate::UpdateSkeletalMesh_PostBeginUpdate0)

	bool bHasInvalidMesh = false;

	bool bUpdateMeshes = DoComponentsNeedUpdate(Public, OperationData, bHasInvalidMesh);

	// We can not handle empty meshes, clear any generated mesh and return
	if (bHasInvalidMesh)
	{
		UE_LOG(LogMutable, Warning, TEXT("Failed to generate SkeletalMesh for CO Instance %s. CO [%s]"), *Public->GetName(), *GetNameSafe(Public->GetCustomizableObject()));

		InvalidateGeneratedData();

		OperationData->UpdateResult = EUpdateResult::Error;

		return false;
	}

	// None of the current meshes requires a mesh update. Continue to BuildMaterials
	if (!bUpdateMeshes)
	{
		return true;
	}

	TextureReuseCache.Empty(); // Sections may have changed, so invalidate the texture reuse cache because it's indexed by section

	TArray<TObjectPtr<USkeletalMesh>> OldSkeletalMeshes = SkeletalMeshes;

	bool bSuccess = true;

	UCustomizableObject* CustomizableObject = Public->GetCustomizableObject();
	check(CustomizableObject);

	const FModelResources& ModelResources = CustomizableObject->GetPrivate()->GetModelResources();

	// Initialize the maximum number of SkeletalMeshes we could possibly have. 
	SkeletalMeshes.Init(nullptr, CustomizableObject->GetComponentCount());

	const int32 NumMutableComponents = OperationData->InstanceUpdateData.Components.Num();
	for (int32 ComponentIndex = 0; ComponentIndex < NumMutableComponents; ++ComponentIndex)
	{
		const FInstanceUpdateData::FComponent& Component = OperationData->InstanceUpdateData.Components[ComponentIndex];

		// If the component doesn't need an update copy the previously generated mesh.
		if (!OperationData->MeshChanged[Component.Id])
		{
			if (OldSkeletalMeshes.IsValidIndex(Component.Id))
			{
				SkeletalMeshes[Component.Id] = OldSkeletalMeshes[Component.Id];
			}

			continue;
		}

		// Check if we have initialized the component
		if (SkeletalMeshes[Component.Id])
		{
			continue;
		}

		if (!ComponentsData.IsValidIndex(Component.Id))
		{
			bSuccess = false;
			ensure(false);
			continue;
		}

		if (OperationData->bUseMeshCache)
		{
			if (USkeletalMesh* CachedMesh = CustomizableObject->GetPrivate()->MeshCache.Get(OperationData->MeshDescriptors[Component.Id]))
			{
				check(OperationData->MeshDescriptors[Component.Id].Num() == MAX_MESH_LOD_COUNT);
				ComponentsData[Component.Id].LastMeshIdPerLOD = OperationData->MeshDescriptors[Component.Id];
				SkeletalMeshes[Component.Id] = CachedMesh;
				continue;
			}
		}

		// Reset last mesh IDs.
		ComponentsData[Component.Id].LastMeshIdPerLOD.Init(MAX_uint64, MAX_MESH_LOD_COUNT);

		if (!Component.bGenerated || !Component.Mesh || Component.SurfaceCount == 0)
		{
			continue;
		}

		if (!ModelResources.ReferenceSkeletalMeshesData.IsValidIndex(Component.Id))
		{
			bSuccess = false;
			break;
		}

		const FMutableRefSkeletalMeshData& RefSkeletalMeshData = ModelResources.ReferenceSkeletalMeshesData[Component.Id];

		// Create and initialize the SkeletalMesh for this component
		MUTABLE_CPUPROFILER_SCOPE(ConstructMesh);

		if (OperationData->bStreamMeshLODs)
		{
			SkeletalMeshes[Component.Id] = UCustomizableObjectSkeletalMesh::CreateSkeletalMesh(OperationData, *Public, Component.Id);
		}
		else
		{
			SkeletalMeshes[Component.Id] = NewObject<USkeletalMesh>(GetTransientPackage(), NAME_None, RF_Transient);
		}

		USkeletalMesh* SkeletalMesh = SkeletalMeshes[Component.Id];
		check(SkeletalMesh);

		// Set up the default information any mesh from this component will have (LODArrayInfos, RenderData, Mesh settings, etc). 
		InitSkeletalMeshData(OperationData, SkeletalMesh, RefSkeletalMeshData, *CustomizableObject, Component.Id);

		if (Component.Mesh)
		{
			// Construct a new skeleton, fix up ActiveBones and Bonemap arrays and recompute the RefInvMatrices
			bSuccess = BuildSkeletonData(OperationData, *SkeletalMesh, RefSkeletalMeshData, *CustomizableObject, Component.Id);

			if (!bSuccess)
			{
				break;
			}

			// Build PhysicsAsset merging physics assets coming from SubMeshes of the newly generated Mesh
			if (mu::Ptr<const mu::PhysicsBody> MutablePhysics = Component.Mesh->GetPhysicsBody())
			{
				constexpr bool bDisallowCollisionBetweenAssets = true;
				UPhysicsAsset* PhysicsAssetResult = GetOrBuildMainPhysicsAsset(
					RefSkeletalMeshData.PhysicsAsset, MutablePhysics.get(), *CustomizableObject, Component.Id, bDisallowCollisionBetweenAssets);

				SkeletalMesh->SetPhysicsAsset(PhysicsAssetResult);

				if (PhysicsAssetResult)
				{
					// We are setting the physics asset mesh preview to the generated skeletal mesh.
					// this is fine if the phyiscs asset is also generated, but not sure about referenced assets.
					// In any case, don't mark the asset as modified.

					// This is only called in editor, no need to add editor guards.	
					constexpr bool bMarkAsDirty = false;
					PhysicsAssetResult->SetPreviewMesh(SkeletalMesh, bMarkAsDirty);
				}
				}

			const int32 NumAdditionalPhysicsNum = Component.Mesh->AdditionalPhysicsBodies.Num();
			for (int32 I = 0; I < NumAdditionalPhysicsNum; ++I)
			{
				mu::Ptr<const mu::PhysicsBody> AdditionalPhysiscBody = Component.Mesh->AdditionalPhysicsBodies[I];
				
				check(AdditionalPhysiscBody);
				if (!AdditionalPhysiscBody->bBodiesModified)
				{
					continue;
				}

				const int32 PhysicsBodyExternalId = Component.Mesh->AdditionalPhysicsBodies[I]->CustomId;
				
				const FAnimBpOverridePhysicsAssetsInfo& Info = ModelResources.AnimBpOverridePhysiscAssetsInfo[PhysicsBodyExternalId];

				// Make sure the AnimInstance class is loaded. It is expected to be already loaded at this point though. 
				UClass* AnimInstanceClassLoaded = Info.AnimInstanceClass.LoadSynchronous();
				TSubclassOf<UAnimInstance> AnimInstanceClass = TSubclassOf<UAnimInstance>(AnimInstanceClassLoaded);
				if (!ensureAlways(AnimInstanceClass))
				{
					continue;
				}

				FAnimBpGeneratedPhysicsAssets& PhysicsAssetsUsedByAnimBp = AnimBpPhysicsAssets.FindOrAdd(AnimInstanceClass);

				TObjectPtr<UPhysicsAsset> PhysicsAssetTemplate = TObjectPtr<UPhysicsAsset>(Info.SourceAsset.Get());

				check(PhysicsAssetTemplate);

				FAnimInstanceOverridePhysicsAsset& Entry =
					PhysicsAssetsUsedByAnimBp.AnimInstancePropertyIndexAndPhysicsAssets.Emplace_GetRef();

				Entry.PropertyIndex = Info.PropertyIndex;
				Entry.PhysicsAsset = MakePhysicsAssetFromTemplateAndMutableBody(PhysicsAssetTemplate, AdditionalPhysiscBody.get(), *CustomizableObject);
			}

			// Add sockets from the SkeletalMesh of reference and from the MutableMesh
			BuildMeshSockets(OperationData, SkeletalMesh, ModelResources, RefSkeletalMeshData, Component.Mesh);
		}
		else
		{
			bSuccess = false;
			break;
		}

		// Collate the extension data on the instance into groups based on the extension that
		// produced it, so that we only need to call OnSkeletalMeshCreated once for each extension.
		{
			TMap<const UCustomizableObjectExtension*, TArray<FInputPinDataContainer>> ExtensionToExtensionData;

			const TArrayView<const FRegisteredObjectNodeInputPin> ExtensionPins = ICustomizableObjectModule::Get().GetAdditionalObjectNodePins();

			for (FInstanceUpdateData::FNamedExtensionData& ExtensionOutput : OperationData->InstanceUpdateData.ExtendedInputPins)
			{
				const FRegisteredObjectNodeInputPin* FoundPin =
					Algo::FindBy(ExtensionPins, ExtensionOutput.Name, &FRegisteredObjectNodeInputPin::GlobalPinName);

				if (!FoundPin)
				{
					// Failed to find the corresponding pin for this output
					// 
					// This may indicate that a plugin has been removed or renamed since the CO was compiled
					UE_LOG(LogMutable, Error, TEXT("Failed to find Object node input pin with name %s"), *ExtensionOutput.Name.ToString());
					continue;
				}

				const UCustomizableObjectExtension* Extension = FoundPin->Extension.Get();
				if (!Extension)
				{
					// Extension is not loaded or not found
					UE_LOG(LogMutable, Error, TEXT("Extension for Object node input pin %s is no longer valid"), *ExtensionOutput.Name.ToString());
					continue;
				}

				if (ExtensionOutput.Data->Origin == mu::ExtensionData::EOrigin::Invalid)
				{
					// Null data was produced
					//
					// This can happen if a node produces an ExtensionData but doesn't initialize it
					UE_LOG(LogMutable, Error, TEXT("Invalid data sent to Object node input pin %s"), *ExtensionOutput.Name.ToString());
					continue;
				}

				TArray<FInputPinDataContainer>& ContainerArray = ExtensionToExtensionData.FindOrAdd(Extension);

				const FCustomizableObjectResourceData* ReferencedExtensionData = nullptr;
				switch (ExtensionOutput.Data->Origin)
				{
					case mu::ExtensionData::EOrigin::ConstantAlwaysLoaded:
					{
						check(CustomizableObject->GetPrivate()->GetAlwaysLoadedExtensionData().IsValidIndex(ExtensionOutput.Data->Index));
						ReferencedExtensionData = &CustomizableObject->GetPrivate()->GetAlwaysLoadedExtensionData()[ExtensionOutput.Data->Index];
					}
					break;

					case mu::ExtensionData::EOrigin::ConstantStreamed:
					{
						check(CustomizableObject->GetPrivate()->GetStreamedExtensionData().IsValidIndex(ExtensionOutput.Data->Index));
						
						const FCustomizableObjectStreamedResourceData& StreamedData =
							CustomizableObject->GetPrivate()->GetStreamedExtensionData()[ExtensionOutput.Data->Index];

						if (!StreamedData.IsLoaded())
						{
							// The data should have been loaded as part of executing the CO program.
							//
							// This could indicate a bug in the streaming logic.
							UE_LOG(LogMutable, Error, TEXT("Customizable Object produced a streamed extension data that is not loaded: %s"),
								*StreamedData.GetPath().ToString());

							continue;
						}

						ReferencedExtensionData = &StreamedData.GetLoadedData();
					}
					break;

					default:
						unimplemented();
				}

				check(ReferencedExtensionData);
		
				ContainerArray.Emplace(FoundPin->InputPin, ReferencedExtensionData->Data);
			}

			// Now that we have an array of extension data for each extension, go through the extensions and
			// give each one its data.
			for (const TPair<const UCustomizableObjectExtension*, TArray<FInputPinDataContainer>>& Pair : ExtensionToExtensionData)
			{
				Pair.Key->OnSkeletalMeshCreated(Pair.Value, ComponentIndex, SkeletalMesh);
			}
		}
	}


	if (bSuccess)
	{
		MUTABLE_CPUPROFILER_SCOPE(BuildSkeletalMeshData);

		const int32 NumComponents = SkeletalMeshes.Num();
		for (int32 ComponentIndex = 0; bSuccess && ComponentIndex < NumComponents; ++ComponentIndex)
		{
			if (OperationData->bUseMeshCache)
			{
				const TArray<mu::FResourceID>& MeshId = OperationData->MeshDescriptors[ComponentIndex];
				if (CustomizableObject->GetPrivate()->MeshCache.Get(MeshId))
				{
					continue;
				}
			}
			
			if (!OperationData->MeshChanged[ComponentIndex])
			{
				continue;
			}

			USkeletalMesh* SkeletalMesh = SkeletalMeshes[ComponentIndex];

			if (!SkeletalMesh)
			{
				continue;
			}

			// Mesh to copy data from if possible. 
			USkeletalMesh* OldSkeletalMesh = OldSkeletalMeshes.IsValidIndex(ComponentIndex) ? OldSkeletalMeshes[ComponentIndex] : nullptr;

			{
				BuildOrCopyElementData(OperationData, SkeletalMesh, Public, ComponentIndex);
				bSuccess = BuildOrCopyRenderData(OperationData, SkeletalMesh, OldSkeletalMesh, Public, ComponentIndex);
			}

			if (bSuccess)
			{
				BuildOrCopyMorphTargetsData(OperationData, SkeletalMesh, OldSkeletalMesh, Public, ComponentIndex);
				BuildOrCopyClothingData(OperationData, SkeletalMesh, OldSkeletalMesh, Public, ComponentIndex);

				FSkeletalMeshRenderData* RenderData = SkeletalMesh->GetResourceForRendering();
				ensure(RenderData && RenderData->LODRenderData.Num() > 0);
				ensure(SkeletalMesh->GetLODInfoArray().Num() > 0);

				for (FSkeletalMeshLODRenderData& LODResource : RenderData->LODRenderData)
				{
					UnrealConversionUtils::UpdateSkeletalMeshLODRenderDataBuffersSize(LODResource);
				}
			}

			if (OperationData->bUseMeshCache)
			{
				const TArray<mu::FResourceID>& MeshId = OperationData->MeshDescriptors[ComponentIndex];
				CustomizableObject->GetPrivate()->MeshCache.Add(MeshId, SkeletalMesh);
			}
		}
	}

	if (!bSuccess)
	{
		InvalidateGeneratedData();
	}

	return bSuccess;
}


UCustomizableObjectInstance* UCustomizableObjectInstance::Clone()
{
	MUTABLE_CPUPROFILER_SCOPE(UCustomizableObjectInstance::Clone);

	// Default Outer is the transient package.
	UCustomizableObjectInstance* NewInstance = NewObject<UCustomizableObjectInstance>();
	check(NewInstance->PrivateData);
	NewInstance->CopyParametersFromInstance(this);

	return NewInstance;
}


UCustomizableObjectInstance* UCustomizableObjectInstance::CloneStatic(UObject* Outer)
{
	UCustomizableObjectInstance* NewInstance = NewObject<UCustomizableObjectInstance>(Outer, GetClass());
	NewInstance->CopyParametersFromInstance(this);
	NewInstance->GetPrivate()->bShowOnlyRuntimeParameters = false;

	return NewInstance;
}


void UCustomizableObjectInstance::CopyParametersFromInstance(UCustomizableObjectInstance* Instance)
{
	SetDescriptor(Instance->GetDescriptor());
}


int32 UCustomizableObjectInstance::AddValueToIntRange(const FString& ParamName)
{
	return Descriptor.AddValueToIntRange(ParamName);
}


int32 UCustomizableObjectInstance::AddValueToFloatRange(const FString& ParamName)
{
	return Descriptor.AddValueToFloatRange(ParamName);
}


int32 UCustomizableObjectInstance::AddValueToProjectorRange(const FString& ParamName)
{
	return Descriptor.AddValueToProjectorRange(ParamName);
}


int32 UCustomizableObjectInstance::RemoveValueFromIntRange(const FString& ParamName)
{
	return Descriptor.RemoveValueFromIntRange(ParamName);
}


int32 UCustomizableObjectInstance::RemoveValueFromIntRange(const FString& ParamName, int32 RangeIndex)
{
	return Descriptor.RemoveValueFromIntRange(ParamName, RangeIndex);

}

int32 UCustomizableObjectInstance::RemoveValueFromFloatRange(const FString& ParamName)
{
	return Descriptor.RemoveValueFromFloatRange(ParamName);
}


int32 UCustomizableObjectInstance::RemoveValueFromFloatRange(const FString& ParamName, const int32 RangeIndex)
{
	return Descriptor.RemoveValueFromFloatRange(ParamName, RangeIndex);
}


int32 UCustomizableObjectInstance::RemoveValueFromProjectorRange(const FString& ParamName)
{
	return Descriptor.RemoveValueFromProjectorRange(ParamName);
}


int32 UCustomizableObjectInstance::RemoveValueFromProjectorRange(const FString& ParamName, const int32 RangeIndex)
{
	return Descriptor.RemoveValueFromProjectorRange(ParamName, RangeIndex);
}


bool UCustomizableObjectInstance::CreateMultiLayerProjector(const FName& ProjectorParamName)
{
	return Descriptor.CreateMultiLayerProjector(ProjectorParamName);
}


void UCustomizableObjectInstance::RemoveMultilayerProjector(const FName& ProjectorParamName)
{
	Descriptor.RemoveMultilayerProjector(ProjectorParamName);
}


int32 UCustomizableObjectInstance::MultilayerProjectorNumLayers(const FName& ProjectorParamName) const
{
	return Descriptor.MultilayerProjectorNumLayers(ProjectorParamName);
}


void UCustomizableObjectInstance::MultilayerProjectorCreateLayer(const FName& ProjectorParamName, int32 Index)
{
	Descriptor.MultilayerProjectorCreateLayer(ProjectorParamName, Index);
}


void UCustomizableObjectInstance::MultilayerProjectorRemoveLayerAt(const FName& ProjectorParamName, int32 Index)
{
	Descriptor.MultilayerProjectorRemoveLayerAt(ProjectorParamName, Index);
}


FMultilayerProjectorLayer UCustomizableObjectInstance::MultilayerProjectorGetLayer(const FName& ProjectorParamName, int32 Index) const
{
	return Descriptor.MultilayerProjectorGetLayer(ProjectorParamName, Index);
}


void UCustomizableObjectInstance::MultilayerProjectorUpdateLayer(const FName& ProjectorParamName, int32 Index, const FMultilayerProjectorLayer& Layer)
{
	Descriptor.MultilayerProjectorUpdateLayer(ProjectorParamName, Index, Layer);
}


TArray<FName> UCustomizableObjectInstance::MultilayerProjectorGetVirtualLayers(const FName& ProjectorParamName) const
{
	return Descriptor.MultilayerProjectorGetVirtualLayers(ProjectorParamName);
}


void UCustomizableObjectInstance::MultilayerProjectorCreateVirtualLayer(const FName& ProjectorParamName, const FName& Id)
{
	Descriptor.MultilayerProjectorCreateVirtualLayer(ProjectorParamName, Id);
}


FMultilayerProjectorVirtualLayer UCustomizableObjectInstance::MultilayerProjectorFindOrCreateVirtualLayer(const FName& ProjectorParamName, const FName& Id)
{
	return Descriptor.MultilayerProjectorFindOrCreateVirtualLayer(ProjectorParamName, Id);
}


void UCustomizableObjectInstance::MultilayerProjectorRemoveVirtualLayer(const FName& ProjectorParamName, const FName& Id)
{
	Descriptor.MultilayerProjectorRemoveVirtualLayer(ProjectorParamName, Id);
}


FMultilayerProjectorVirtualLayer UCustomizableObjectInstance::MultilayerProjectorGetVirtualLayer(const FName& ProjectorParamName, const FName& Id) const
{
	return Descriptor.MultilayerProjectorGetVirtualLayer(ProjectorParamName, Id);
}


void UCustomizableObjectInstance::MultilayerProjectorUpdateVirtualLayer(const FName& ProjectorParamName, const FName& Id, const FMultilayerProjectorVirtualLayer& Layer)
{
	Descriptor.MultilayerProjectorUpdateVirtualLayer(ProjectorParamName, Id, Layer);
}


void UCustomizableObjectInstance::SaveDescriptor(FArchive &Ar, bool bUseCompactDescriptor)
{
	Descriptor.SaveDescriptor(Ar, bUseCompactDescriptor);
}


void UCustomizableObjectInstance::LoadDescriptor(FArchive &Ar)
{
	Descriptor.LoadDescriptor(Ar);
}


const FString& UCustomizableObjectInstance::GetIntParameterSelectedOption(const FString& ParamName, const int32 RangeIndex) const
{
	return Descriptor.GetIntParameterSelectedOption(ParamName, RangeIndex);
}


void UCustomizableObjectInstance::SetIntParameterSelectedOption(int32 IntParamIndex, const FString& SelectedOption, const int32 RangeIndex)
{
	Descriptor.SetIntParameterSelectedOption(IntParamIndex, SelectedOption, RangeIndex);
}


void UCustomizableObjectInstance::SetIntParameterSelectedOption(const FString& ParamName, const FString& SelectedOptionName, const int32 RangeIndex)
{
	Descriptor.SetIntParameterSelectedOption(ParamName, SelectedOptionName, RangeIndex);
}


float UCustomizableObjectInstance::GetFloatParameterSelectedOption(const FString& FloatParamName, const int32 RangeIndex) const
{
	return Descriptor.GetFloatParameterSelectedOption(FloatParamName, RangeIndex);
}


void UCustomizableObjectInstance::SetFloatParameterSelectedOption(const FString& FloatParamName, const float FloatValue, const int32 RangeIndex)
{
	return Descriptor.SetFloatParameterSelectedOption(FloatParamName, FloatValue, RangeIndex);
}


FName UCustomizableObjectInstance::GetTextureParameterSelectedOption(const FString& TextureParamName, const int32 RangeIndex) const
{
	return Descriptor.GetTextureParameterSelectedOption(TextureParamName, RangeIndex);
}


void UCustomizableObjectInstance::SetTextureParameterSelectedOption(const FString& TextureParamName, const FString& TextureValue, const int32 RangeIndex)
{
	Descriptor.SetTextureParameterSelectedOption(TextureParamName, TextureValue, RangeIndex);
}


FLinearColor UCustomizableObjectInstance::GetColorParameterSelectedOption(const FString& ColorParamName) const
{
	return Descriptor.GetColorParameterSelectedOption(ColorParamName);
}


void UCustomizableObjectInstance::SetColorParameterSelectedOption(const FString & ColorParamName, const FLinearColor& ColorValue)
{
	Descriptor.SetColorParameterSelectedOption(ColorParamName, ColorValue);
}


bool UCustomizableObjectInstance::GetBoolParameterSelectedOption(const FString& BoolParamName) const
{
	return Descriptor.GetBoolParameterSelectedOption(BoolParamName);
}


void UCustomizableObjectInstance::SetBoolParameterSelectedOption(const FString& BoolParamName, const bool BoolValue)
{
	Descriptor.SetBoolParameterSelectedOption(BoolParamName, BoolValue);
}


void UCustomizableObjectInstance::SetVectorParameterSelectedOption(const FString& VectorParamName, const FLinearColor& VectorValue)
{
	Descriptor.SetVectorParameterSelectedOption(VectorParamName, VectorValue);
}


void UCustomizableObjectInstance::SetProjectorValue(const FString& ProjectorParamName,
	const FVector& Pos, const FVector& Direction, const FVector& Up, const FVector& Scale,
	const float Angle,
	const int32 RangeIndex)
{
	Descriptor.SetProjectorValue(ProjectorParamName, Pos, Direction, Up, Scale, Angle, RangeIndex);
}


void UCustomizableObjectInstance::SetProjectorPosition(const FString& ProjectorParamName, const FVector& Pos, const int32 RangeIndex)
{
	Descriptor.SetProjectorPosition(ProjectorParamName, Pos, RangeIndex);
}


void UCustomizableObjectInstance::SetProjectorDirection(const FString& ProjectorParamName, const FVector& Direction, int32 RangeIndex)
{
	Descriptor.SetProjectorDirection(ProjectorParamName, Direction, RangeIndex);
}


void UCustomizableObjectInstance::SetProjectorUp(const FString& ProjectorParamName, const FVector& Up, int32 RangeIndex)
{
	Descriptor.SetProjectorUp(ProjectorParamName, Up, RangeIndex);	
}


void UCustomizableObjectInstance::SetProjectorScale(const FString& ProjectorParamName, const FVector& Scale, int32 RangeIndex)
{
	Descriptor.SetProjectorScale(ProjectorParamName, Scale, RangeIndex);	
}


void UCustomizableObjectInstance::SetProjectorAngle(const FString& ProjectorParamName, float Angle, int32 RangeIndex)
{
	Descriptor.SetProjectorAngle(ProjectorParamName, Angle, RangeIndex);
}


void UCustomizableObjectInstance::GetProjectorValue(const FString& ProjectorParamName,
                                                    FVector& OutPos, FVector& OutDir, FVector& OutUp, FVector& OutScale,
                                                    float& OutAngle, ECustomizableObjectProjectorType& OutType,
                                                    const int32 RangeIndex) const
{
	Descriptor.GetProjectorValue(ProjectorParamName, OutPos, OutDir, OutUp, OutScale, OutAngle, OutType, RangeIndex);
}


void UCustomizableObjectInstance::GetProjectorValueF(const FString& ProjectorParamName,
	FVector3f& OutPos, FVector3f& OutDir, FVector3f& OutUp, FVector3f& OutScale,
	float& OutAngle, ECustomizableObjectProjectorType& OutType,
	int32 RangeIndex) const
{
	Descriptor.GetProjectorValueF(ProjectorParamName, OutPos, OutDir, OutUp, OutScale, OutAngle, OutType, RangeIndex);
}


FVector UCustomizableObjectInstance::GetProjectorPosition(const FString& ParamName, const int32 RangeIndex) const
{
	return Descriptor.GetProjectorPosition(ParamName, RangeIndex);
}


FVector UCustomizableObjectInstance::GetProjectorDirection(const FString& ParamName, const int32 RangeIndex) const
{
	return Descriptor.GetProjectorDirection(ParamName, RangeIndex);
}


FVector UCustomizableObjectInstance::GetProjectorUp(const FString& ParamName, const int32 RangeIndex) const
{
	return Descriptor.GetProjectorUp(ParamName, RangeIndex);
}


FVector UCustomizableObjectInstance::GetProjectorScale(const FString& ParamName, const int32 RangeIndex) const
{
	return Descriptor.GetProjectorScale(ParamName, RangeIndex);
}


float UCustomizableObjectInstance::GetProjectorAngle(const FString& ParamName, int32 RangeIndex) const
{
	return Descriptor.GetProjectorAngle(ParamName, RangeIndex);
}


ECustomizableObjectProjectorType UCustomizableObjectInstance::GetProjectorParameterType(const FString& ParamName, const int32 RangeIndex) const
{
	return Descriptor.GetProjectorParameterType(ParamName, RangeIndex);
}


FCustomizableObjectProjector UCustomizableObjectInstance::GetProjector(const FString& ParamName, const int32 RangeIndex) const
{
	return Descriptor.GetProjector(ParamName, RangeIndex);
}


int32 UCustomizableObjectInstance::FindIntParameterNameIndex(const FString& ParamName) const
{
	return Descriptor.FindIntParameterNameIndex(ParamName);
}


int32 UCustomizableObjectInstance::FindFloatParameterNameIndex(const FString& ParamName) const
{
	return Descriptor.FindFloatParameterNameIndex(ParamName);
}


int32 UCustomizableObjectInstance::FindBoolParameterNameIndex(const FString& ParamName) const
{
	return Descriptor.FindBoolParameterNameIndex(ParamName);
}


int32 UCustomizableObjectInstance::FindVectorParameterNameIndex(const FString& ParamName) const
{
	return Descriptor.FindVectorParameterNameIndex(ParamName);
}


int32 UCustomizableObjectInstance::FindProjectorParameterNameIndex(const FString& ParamName) const
{
	return Descriptor.FindProjectorParameterNameIndex(ParamName);
}

void UCustomizableObjectInstance::SetRandomValues()
{
	Descriptor.SetRandomValues();
}

void UCustomizableObjectInstance::SetRandomValuesFromStream(const FRandomStream& InStream)
{
	Descriptor.SetRandomValuesFromStream(InStream);
}


bool UCustomizableInstancePrivate::LoadParametersFromProfile(int32 ProfileIndex)
{
	UCustomizableObject* CustomizableObject = GetPublic()->GetCustomizableObject();
	if (!CustomizableObject)
	{
		return false;
	}

#if WITH_EDITOR
	if (ProfileIndex < 0 || ProfileIndex >= CustomizableObject->GetPrivate()->GetInstancePropertiesProfiles().Num() )
	{
		return false;
	}
	
	// This could be done only when the instance changes.
	MigrateProfileParametersToCurrentInstance(ProfileIndex);

	const FProfileParameterDat& Profile = CustomizableObject->GetPrivate()->GetInstancePropertiesProfiles()[ProfileIndex];

	GetPublic()->Descriptor.BoolParameters = Profile.BoolParameters;
	GetPublic()->Descriptor.IntParameters = Profile.IntParameters;
	GetPublic()->Descriptor.FloatParameters = Profile.FloatParameters;
	GetPublic()->Descriptor.TextureParameters = Profile.TextureParameters;
	GetPublic()->Descriptor.ProjectorParameters = Profile.ProjectorParameters;
	GetPublic()->Descriptor.VectorParameters = Profile.VectorParameters;
#endif
	return true;

}

bool UCustomizableInstancePrivate::SaveParametersToProfile(int32 ProfileIndex)
{
	UCustomizableObject* CustomizableObject = GetPublic()->GetCustomizableObject();
	if (!CustomizableObject)
	{
		return false;
	}

#if WITH_EDITOR
	bSelectedProfileDirty = ProfileIndex != SelectedProfileIndex;

	if (ProfileIndex < 0 || ProfileIndex >= CustomizableObject->GetPrivate()->GetInstancePropertiesProfiles().Num())
	{
		return false;
	}

	FProfileParameterDat& Profile = CustomizableObject->GetPrivate()->GetInstancePropertiesProfiles()[ProfileIndex];

	Profile.BoolParameters = GetPublic()->Descriptor.BoolParameters;
	Profile.IntParameters = GetPublic()->Descriptor.IntParameters;
	Profile.FloatParameters = GetPublic()->Descriptor.FloatParameters;
	Profile.TextureParameters = GetPublic()->Descriptor.TextureParameters;
	Profile.ProjectorParameters = GetPublic()->Descriptor.ProjectorParameters;
	Profile.VectorParameters = GetPublic()->Descriptor.VectorParameters;
#endif
	return true;
}

bool UCustomizableInstancePrivate::MigrateProfileParametersToCurrentInstance(int32 ProfileIndex)
{
	UCustomizableObject* CustomizableObject = GetPublic()->GetCustomizableObject();
	if (!CustomizableObject)
	{
		return false;
	}

#if WITH_EDITOR
	if (ProfileIndex < 0 || ProfileIndex >= CustomizableObject->GetPrivate()->GetInstancePropertiesProfiles().Num())
	{
		return false;
	}

	FProfileParameterDat& Profile = CustomizableObject->GetPrivate()->GetInstancePropertiesProfiles()[ProfileIndex];
	FProfileParameterDat TempProfile;

	TempProfile.ProfileName = Profile.ProfileName;
	TempProfile.BoolParameters = GetPublic()->Descriptor.BoolParameters;
	TempProfile.FloatParameters = GetPublic()->Descriptor.FloatParameters;
	TempProfile.IntParameters = GetPublic()->Descriptor.IntParameters;
	TempProfile.ProjectorParameters = GetPublic()->Descriptor.ProjectorParameters;
	TempProfile.TextureParameters = GetPublic()->Descriptor.TextureParameters;
	TempProfile.VectorParameters = GetPublic()->Descriptor.VectorParameters;
	

	// Populate TempProfile with the parameters found in the profile.
	// Any profile parameter missing will be discarded.
	for (FCustomizableObjectBoolParameterValue& Parameter : TempProfile.BoolParameters)
	{
		using ParamValType = FCustomizableObjectBoolParameterValue;
		ParamValType* Found = Profile.BoolParameters.FindByPredicate(
			[&Parameter](const ParamValType& P) { return P.ParameterName == Parameter.ParameterName; });

		if (Found)
		{
			Parameter.ParameterValue = Found->ParameterValue;
		}
	}

	for (FCustomizableObjectIntParameterValue& Parameter : TempProfile.IntParameters)
	{
		using ParamValType = FCustomizableObjectIntParameterValue;
		ParamValType* Found = Profile.IntParameters.FindByPredicate(
			[&Parameter](const ParamValType& P) { return P.ParameterName == Parameter.ParameterName; });

		if (Found)
		{
			Parameter.ParameterValueName = Found->ParameterValueName;
		}
	}

	for (FCustomizableObjectFloatParameterValue& Parameter : TempProfile.FloatParameters)
	{
		using ParamValType = FCustomizableObjectFloatParameterValue;
		ParamValType* Found = Profile.FloatParameters.FindByPredicate(
			[&Parameter](const ParamValType& P) { return P.ParameterName == Parameter.ParameterName; });

		if (Found)
		{
			Parameter.ParameterValue = Found->ParameterValue;
			Parameter.ParameterRangeValues = Found->ParameterRangeValues;
		}
	}

	for (FCustomizableObjectTextureParameterValue& Parameter : TempProfile.TextureParameters)
	{
		using ParamValType = FCustomizableObjectTextureParameterValue;
		ParamValType* Found = Profile.TextureParameters.FindByPredicate(
			[&Parameter](const ParamValType& P) { return P.ParameterName == Parameter.ParameterName; });

		if (Found)
		{
			Parameter.ParameterValue = Found->ParameterValue;
		}
	}

	for (FCustomizableObjectVectorParameterValue& Parameter : TempProfile.VectorParameters)
	{
		using ParamValType = FCustomizableObjectVectorParameterValue;
		ParamValType* Found = Profile.VectorParameters.FindByPredicate(
			[&Parameter](const ParamValType& P) { return P.ParameterName == Parameter.ParameterName; });

		if (Found)
		{
			Parameter.ParameterValue = Found->ParameterValue;
		}
	}

	for (FCustomizableObjectProjectorParameterValue& Parameter : TempProfile.ProjectorParameters)
	{
		using ParamValType = FCustomizableObjectProjectorParameterValue;
		ParamValType* Found = Profile.ProjectorParameters.FindByPredicate(
			[&Parameter](const ParamValType& P) { return P.ParameterName == Parameter.ParameterName; });

		if (Found)
		{
			Parameter.RangeValues = Found->RangeValues;
			Parameter.Value = Found->Value;
		}
	}

	Profile = TempProfile;

	//CustomizableObject->Modify();
#endif

	return true;
}


UCustomizableObjectInstance* UCustomizableInstancePrivate::GetPublic() const
{
	UCustomizableObjectInstance* Public = StaticCast<UCustomizableObjectInstance*>(GetOuter());
	check(Public);

	return Public;
}


void UCustomizableInstancePrivate::SetSelectedParameterProfileDirty()
{
	UCustomizableObject* CustomizableObject = GetPublic()->GetCustomizableObject();
	if (!CustomizableObject)
	{
		return;
	}

#if WITH_EDITOR
	bSelectedProfileDirty = SelectedProfileIndex != INDEX_NONE;
	
	if (bSelectedProfileDirty)
	{
		CustomizableObject->Modify();
	}
#endif
}

bool UCustomizableInstancePrivate::IsSelectedParameterProfileDirty() const
{
	
#if WITH_EDITOR
	return bSelectedProfileDirty && SelectedProfileIndex != INDEX_NONE;
#else
	return false;
#endif
}


void UCustomizableInstancePrivate::DiscardResources()
{
	UCustomizableObjectInstance* Instance = Cast<UCustomizableObjectInstance>(GetOuter());
	if (!Instance)
	{
		return;
	}

	if (SkeletalMeshStatus == ESkeletalMeshStatus::Success)
	{
		for (int32 Component = 0; Component < SkeletalMeshes.Num(); ++Component)
		{
			if (SkeletalMeshes[Component] && SkeletalMeshes[Component]->IsValidLowLevel())
			{
				SkeletalMeshes[Component]->ReleaseResources();
				SkeletalMeshes[Component] = nullptr;
			}
		}

		ReleaseMutableResources(false, *Instance);
	}
	
	InvalidateGeneratedData();
	
}


void UCustomizableInstancePrivate::SetDefaultSkeletalMesh(bool bSetEmptyMesh) const
{
	UCustomizableObjectInstance* Instance = Cast<UCustomizableObjectInstance>(GetOuter());
	if (!Instance || !Instance->GetCustomizableObject())
	{
		return;
	}

	UCustomizableObject* CustomizableObject = Instance->GetCustomizableObject();
	const FModelResources& ModelResources = CustomizableObject->GetPrivate()->GetModelResources();

	const int32 NumComponents = CustomizableObject->GetComponentCount();

	for (TObjectIterator<UCustomizableObjectInstanceUsage> It; It; ++It)
	{
		UCustomizableObjectInstanceUsage* CustomizableObjectInstanceUsage = *It;
		if (!IsValid(CustomizableObjectInstanceUsage) || CustomizableObjectInstanceUsage->GetCustomizableObjectInstance() != Instance)
		{
			continue;
		}

#if WITH_EDITOR
		if (CustomizableObjectInstanceUsage->IsNetMode(NM_DedicatedServer))
		{
			continue;
		}
#endif
		const int32 ComponentIndex = CustomizableObjectInstanceUsage->GetComponentIndex();
		if (!ModelResources.ReferenceSkeletalMeshesData.IsValidIndex(ComponentIndex))
		{
			continue;
		}

		USkeletalMesh* SkeletalMesh = nullptr;
		
		if (!bSetEmptyMesh)
		{
			// Force load the reference mesh if necessary. 
			SkeletalMesh = ModelResources.ReferenceSkeletalMeshesData[ComponentIndex].SoftSkeletalMesh.LoadSynchronous();
		}
		
		CustomizableObjectInstanceUsage->SetSkeletalMesh(SkeletalMesh);
	}
}


namespace
{
	inline float unpack_uint8(uint8 i)
	{
		float res = i;
		res -= 127.5f;
		res /= 127.5f;
		return res;
	}
}


void SetTexturePropertiesFromMutableImageProps(UTexture2D* Texture, const FMutableModelImageProperties& Props, bool bNeverStream)
{
#if !PLATFORM_DESKTOP
	if (UCustomizableObjectSystem::GetInstance()->GetPrivate()->EnableMutableProgressiveMipStreaming <= 0)
	{
		Texture->NeverStream = true;
	}
	else
	{
		Texture->NeverStream = bNeverStream;
	}

#if !PLATFORM_ANDROID && !PLATFORM_IOS
	Texture->bNotOfflineProcessed = true;
#endif

#else
	Texture->NeverStream = bNeverStream;
#endif

	Texture->SRGB = Props.SRGB;
	Texture->Filter = Props.Filter;
	Texture->LODBias = Props.LODBias;

#if WITH_EDITORONLY_DATA
	Texture->bFlipGreenChannel = Props.FlipGreenChannel;
#endif

	Texture->LODGroup = Props.LODGroup;
	Texture->AddressX = Props.AddressX;
	Texture->AddressY = Props.AddressY;
}


UCustomizableInstancePrivate* UCustomizableObjectInstance::GetPrivate() const
{ 
	check(PrivateData); // Currently this is initialized in the constructor so we expect it always to exist.
	return PrivateData; 
}


bool FMutableUpdateCandidate::HasBeenIssued() const
{
	return bHasBeenIssued;
}


void FMutableUpdateCandidate::Issue()
{
	bHasBeenIssued = true;
}


void FMutableUpdateCandidate::ApplyLODUpdateParamsToInstance(FUpdateContextPrivate& Context)
{
	CustomizableObjectInstance->Descriptor.MinLOD = MinLOD;
	CustomizableObjectInstance->Descriptor.RequestedLODLevels = RequestedLODLevels;

	Context.SetMinLOD(MinLOD);
	Context.SetRequestedLODs(RequestedLODLevels);
}


// The memory allocated in the function and pointed by the returned pointer is owned by the caller and must be freed. 
// If assigned to a UTexture2D, it will be freed by that UTexture2D
FTexturePlatformData* MutableCreateImagePlatformData(mu::Ptr<const mu::Image> MutableImage, int32 OnlyLOD, uint16 FullSizeX, uint16 FullSizeY)
{
	int32 SizeX = FMath::Max(MutableImage->GetSize()[0], FullSizeX);
	int32 SizeY = FMath::Max(MutableImage->GetSize()[1], FullSizeY);

	if (SizeX <= 0 || SizeY <= 0)
	{
		UE_LOG(LogMutable, Warning, TEXT("Invalid parameters specified for UCustomizableInstancePrivate::MutableCreateImagePlatformData()"));
		return nullptr;
	}

	int32 FirstLOD = 0;
	for (int32 l = 0; l < OnlyLOD; ++l)
	{
		if (SizeX <= 4 || SizeY <= 4)
		{
			break;
		}
		SizeX = FMath::Max(SizeX / 2, 1);
		SizeY = FMath::Max(SizeY / 2, 1);
		++FirstLOD;
	}

	int32 MaxSize = FMath::Max(SizeX, SizeY);
	int32 FullLODCount = 1;
	int32 MipsToSkip = 0;
	
	if (OnlyLOD < 0)
	{
		FullLODCount = FMath::CeilLogTwo(MaxSize) + 1;
		MipsToSkip = FullLODCount - MutableImage->GetLODCount();
		check(MipsToSkip >= 0);
	}

	// Reduce final texture size if we surpass the max size we can generate.
	UCustomizableObjectSystem* System = UCustomizableObjectSystem::GetInstance();
	UCustomizableObjectSystemPrivate* SystemPrivate = System ? System->GetPrivate() : nullptr;

	int32 MaxTextureSizeToGenerate = SystemPrivate ? SystemPrivate->MaxTextureSizeToGenerate : 0;

	if (MaxTextureSizeToGenerate > 0)
	{
		// Skip mips only if texture streaming is disabled 
		const bool bIsStreamingEnabled = MipsToSkip > 0;

		// Skip mips if the texture surpasses a certain size
		if (MaxSize > MaxTextureSizeToGenerate && !bIsStreamingEnabled && OnlyLOD < 0)
		{
			// Skip mips until MaxSize is equal or less than MaxTextureSizeToGenerate or there aren't more mips to skip
			while (MaxSize > MaxTextureSizeToGenerate && FirstLOD < (FullLODCount - 1))
			{
				MaxSize = MaxSize >> 1;
				FirstLOD++;
			}

			// Update SizeX and SizeY
			SizeX = SizeX >> FirstLOD;
			SizeY = SizeY >> FirstLOD;
		}
	}

	if (MutableImage->GetLODCount() == 1)
	{
		MipsToSkip = 0;
		FullLODCount = 1;
		FirstLOD = 0;
	}

	int32 EndLOD = OnlyLOD < 0 ? FullLODCount : FirstLOD + 1;
	
	mu::EImageFormat MutableFormat = MutableImage->GetFormat();

	int32 MaxPossibleSize = 0;
		
	if (MaxTextureSizeToGenerate > 0)
	{
		MaxPossibleSize = int32(FMath::Pow(2.f, float(FullLODCount - FirstLOD - 1)));
	}
	else
	{
		MaxPossibleSize = int32(FMath::Pow(2.f, float(FullLODCount - 1)));
	}

	// This could happen with non-power-of-two images.
	//check(SizeX == MaxPossibleSize || SizeY == MaxPossibleSize || FullLODCount == 1);
	if (!(SizeX == MaxPossibleSize || SizeY == MaxPossibleSize || FullLODCount == 1))
	{
		UE_LOG(LogMutable, Warning, TEXT("Building instance: unsupported texture size %d x %d."), SizeX, SizeY);
		//return nullptr;
	}

	mu::FImageOperator ImOp = mu::FImageOperator::GetDefault(mu::FImageOperator::FImagePixelFormatFunc());

	EPixelFormat PlatformFormat = PF_Unknown;
	switch (MutableFormat)
	{
	case mu::EImageFormat::IF_RGB_UBYTE:
		// performance penalty. can happen in states that remove compression.
		PlatformFormat = PF_R8G8B8A8;	
		UE_LOG(LogMutable, Warning, TEXT("Building instance: a texture was generated in a format not supported by the hardware (RGB), this results in an additional conversion, so a performance penalty."));
		break; 

	case mu::EImageFormat::IF_BGRA_UBYTE:			
		// performance penalty. can happen with texture parameter images.
		PlatformFormat = PF_R8G8B8A8;	
		UE_LOG(LogMutable, Warning, TEXT("Building instance: a texture was generated in a format not supported by the hardware (BGRA), this results in an additional conversion, so a performance penalty."));
		break;

	// Good cases:
	case mu::EImageFormat::IF_RGBA_UBYTE:		PlatformFormat = PF_R8G8B8A8;	break;
	case mu::EImageFormat::IF_BC1:				PlatformFormat = PF_DXT1;		break;
	case mu::EImageFormat::IF_BC2:				PlatformFormat = PF_DXT3;		break;
	case mu::EImageFormat::IF_BC3:				PlatformFormat = PF_DXT5;		break;
	case mu::EImageFormat::IF_BC4:				PlatformFormat = PF_BC4;		break;
	case mu::EImageFormat::IF_BC5:				PlatformFormat = PF_BC5;		break;
	case mu::EImageFormat::IF_L_UBYTE:			PlatformFormat = PF_G8;			break;
	case mu::EImageFormat::IF_ASTC_4x4_RGB_LDR:	PlatformFormat = PF_ASTC_4x4;	break;
	case mu::EImageFormat::IF_ASTC_4x4_RGBA_LDR:PlatformFormat = PF_ASTC_4x4;	break;
	case mu::EImageFormat::IF_ASTC_4x4_RG_LDR:	PlatformFormat = PF_ASTC_4x4;	break;
	default:
		// Cannot prepare texture if it's not in the right format, this can happen if mutable is in debug mode or in case of bugs
		UE_LOG(LogMutable, Warning, TEXT("Building instance: a texture was generated in an unsupported format, it will be converted to Unreal with a performance penalty."));

		switch (mu::GetImageFormatData(MutableFormat).Channels)
		{
		case 1:
			PlatformFormat = PF_R8;
			MutableImage = ImOp.ImagePixelFormat(0, MutableImage.get(), mu::EImageFormat::IF_L_UBYTE);
			break;
		case 2:
		case 3:
		case 4:
			PlatformFormat = PF_R8G8B8A8;
			MutableImage = ImOp.ImagePixelFormat(0, MutableImage.get(), mu::EImageFormat::IF_RGBA_UBYTE);
			break;
		default: 
			// Absolutely worst case
			return nullptr;
		}		
	}

	FTexturePlatformData* PlatformData = new FTexturePlatformData();
	PlatformData->SizeX = SizeX;
	PlatformData->SizeY = SizeY;
	PlatformData->PixelFormat = PlatformFormat;

	// Allocate mipmaps.

	if (!FMath::IsPowerOfTwo(SizeX) || !FMath::IsPowerOfTwo(SizeY))
	{
		EndLOD = FirstLOD + 1;
		MipsToSkip = 0;
		FullLODCount = 1;
	}

	for (int32 MipLevelUE = FirstLOD; MipLevelUE < EndLOD; ++MipLevelUE)
	{
		int32 MipLevelMutable = MipLevelUE - MipsToSkip;		

		// Unlike Mutable, UE expects MIPs sizes to be at least the size of the compression block.
		// For example, a 8x8 PF_DXT1 texture will have the following MIPs:
		// Mutable    Unreal Engine
		// 8x8        8x8
		// 4x4        4x4
		// 2x2        4x4
		// 1x1        4x4
		//
		// Notice that even though Mutable reports MIP smaller than the block size, the actual data contains at least a block.
		FTexture2DMipMap* Mip = new FTexture2DMipMap( FMath::Max(SizeX, GPixelFormats[PlatformFormat].BlockSizeX)
													, FMath::Max(SizeY, GPixelFormats[PlatformFormat].BlockSizeY));

		PlatformData->Mips.Add(Mip);
		if(MipLevelUE >= MipsToSkip || OnlyLOD>=0)
		{
			check(MipLevelMutable >= 0);
			check(MipLevelMutable < MutableImage->GetLODCount());

			Mip->BulkData.Lock(LOCK_READ_WRITE);
			Mip->BulkData.ClearBulkDataFlags(BULKDATA_SingleUse);

			const uint8* MutableData = MutableImage->GetLODData(MipLevelMutable);
			const uint32 SourceDataSize = MutableImage->GetLODDataSize(MipLevelMutable);

			uint32 DestDataSize = (MutableFormat == mu::EImageFormat::IF_RGB_UBYTE)
					? (SourceDataSize/3) * 4
					: SourceDataSize;
			void* pData = Mip->BulkData.Realloc(DestDataSize);

			// Special inefficient cases
			if (MutableFormat== mu::EImageFormat::IF_BGRA_UBYTE)
			{
				check(SourceDataSize==DestDataSize);

				MUTABLE_CPUPROFILER_SCOPE(Innefficent_BGRA_Format_Conversion);

				uint8_t* pDest = reinterpret_cast<uint8_t*>(pData);
				for (size_t p = 0; p < SourceDataSize / 4; ++p)
				{
					pDest[p * 4 + 0] = MutableData[p * 4 + 2];
					pDest[p * 4 + 1] = MutableData[p * 4 + 1];
					pDest[p * 4 + 2] = MutableData[p * 4 + 0];
					pDest[p * 4 + 3] = MutableData[p * 4 + 3];
				}
			}

			else if (MutableFormat == mu::EImageFormat::IF_RGB_UBYTE)
			{
				MUTABLE_CPUPROFILER_SCOPE(Innefficent_RGB_Format_Conversion);

				uint8_t* pDest = reinterpret_cast<uint8_t*>(pData);
				for (size_t p = 0; p < SourceDataSize / 3; ++p)
				{
					pDest[p * 4 + 0] = MutableData[p * 3 + 0];
					pDest[p * 4 + 1] = MutableData[p * 3 + 1];
					pDest[p * 4 + 2] = MutableData[p * 3 + 2];
					pDest[p * 4 + 3] = 255;
				}
			}

			// Normal case
			else
			{
				check(SourceDataSize == DestDataSize);
				FMemory::Memcpy(pData, MutableData, SourceDataSize);
			}

			Mip->BulkData.Unlock();
		}
		else
		{
			Mip->BulkData.SetBulkDataFlags(BULKDATA_PayloadInSeperateFile);
			Mip->BulkData.ClearBulkDataFlags(BULKDATA_PayloadAtEndOfFile);
		}

		SizeX /= 2;
		SizeY /= 2;

		SizeX = SizeX > 0 ? SizeX : 1;
		SizeY = SizeY > 0 ? SizeY : 1;
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	// Some consistency checks for dev builds
	int32 BulkDataCount = 0;

	for (int32 i = 0; i < PlatformData->Mips.Num(); ++i)
	{
		if (i > 0)
		{
			check(PlatformData->Mips[i].SizeX == PlatformData->Mips[i - 1].SizeX / 2 || PlatformData->Mips[i].SizeX == GPixelFormats[PlatformFormat].BlockSizeX);
			check(PlatformData->Mips[i].SizeY == PlatformData->Mips[i - 1].SizeY / 2 || PlatformData->Mips[i].SizeY == GPixelFormats[PlatformFormat].BlockSizeY);
		}

		if (PlatformData->Mips[i].BulkData.GetBulkDataSize() > 0)
		{
			BulkDataCount++;
		}
	}

	if (MaxTextureSizeToGenerate > 0)
	{
		check(FullLODCount == 1 || OnlyLOD >= 0 || (BulkDataCount == (MutableImage->GetLODCount() - FirstLOD)));
	}
	else
	{
		check(FullLODCount == 1 || OnlyLOD >= 0 || (BulkDataCount == MutableImage->GetLODCount()));
	}
#endif

	return PlatformData;
}



void ConvertImage(UTexture2D* Texture, mu::Ptr<const mu::Image> MutableImage, const FMutableModelImageProperties& Props, int OnlyLOD, int32 ExtractChannel)
{
	MUTABLE_CPUPROFILER_SCOPE(UCustomizableInstancePrivate::ConvertImage);

	SetTexturePropertiesFromMutableImageProps(Texture, Props, false);

	mu::EImageFormat MutableFormat = MutableImage->GetFormat();

	// Extract a single channel, if requested.
	if (ExtractChannel >= 0)
	{
		mu::FImageOperator ImOp = mu::FImageOperator::GetDefault(mu::FImageOperator::FImagePixelFormatFunc());

		MutableImage = ImOp.ImagePixelFormat( 4, MutableImage.get(), mu::EImageFormat::IF_RGBA_UBYTE );

		uint8_t Channel = uint8_t( FMath::Clamp(ExtractChannel,0,3) );
		MutableImage = ImOp.ImageSwizzle( mu::EImageFormat::IF_L_UBYTE, &MutableImage, &Channel );
		MutableFormat = mu::EImageFormat::IF_L_UBYTE;
	}

	// Hack: This format is unsupported in UE, but it shouldn't happen in production.
	if (MutableFormat == mu::EImageFormat::IF_RGB_UBYTE)
	{
		UE_LOG(LogMutable, Warning, TEXT("Building instance: a texture was generated in RGB format, which is slow to convert to Unreal."));

		// Expand the image.
		mu::ImagePtr Converted = new mu::Image(MutableImage->GetSizeX(), MutableImage->GetSizeY(), MutableImage->GetLODCount(), mu::EImageFormat::IF_RGBA_UBYTE, mu::EInitializationType::NotInitialized);

		for (int32 LODIndex = 0; LODIndex < Converted->GetLODCount(); ++LODIndex)
		{
			int32 PixelCount = MutableImage->GetLODDataSize(LODIndex)/3;
			const uint8* pSource = MutableImage->GetMipData(LODIndex);
			uint8* pTarget = Converted->GetMipData(LODIndex);
			for (int32 p = 0; p < PixelCount; ++p)
			{
				pTarget[4 * p + 0] = pSource[3 * p + 0];
				pTarget[4 * p + 1] = pSource[3 * p + 1];
				pTarget[4 * p + 2] = pSource[3 * p + 2];
				pTarget[4 * p + 3] = 255;
			}
		}

		MutableImage = Converted;
		MutableFormat = mu::EImageFormat::IF_RGBA_UBYTE;
	}
	else if (MutableFormat == mu::EImageFormat::IF_BGRA_UBYTE)
	{
		UE_LOG(LogMutable, Warning, TEXT("Building instance: a texture was generated in BGRA format, which is slow to convert to Unreal."));

		MUTABLE_CPUPROFILER_SCOPE(Swizzle);
		// Swizzle the image.
		// \TODO: Raise a warning?
		mu::ImagePtr Converted = new mu::Image(MutableImage->GetSizeX(), MutableImage->GetSizeY(), 1, mu::EImageFormat::IF_RGBA_UBYTE, mu::EInitializationType::NotInitialized);
		int32 PixelCount = MutableImage->GetSizeX() * MutableImage->GetSizeY();

		const uint8* pSource = MutableImage->GetLODData(0);
		uint8* pTarget = Converted->GetLODData(0);
		for (int32 p = 0; p < PixelCount; ++p)
		{
			pTarget[4 * p + 0] = pSource[4 * p + 2];
			pTarget[4 * p + 1] = pSource[4 * p + 1];
			pTarget[4 * p + 2] = pSource[4 * p + 0];
			pTarget[4 * p + 3] = pSource[4 * p + 3];
		}

		MutableImage = Converted;
		MutableFormat = mu::EImageFormat::IF_RGBA_UBYTE;
	}

	if (OnlyLOD >= 0)
	{
		OnlyLOD = FMath::Min( OnlyLOD, MutableImage->GetLODCount()-1 );
	}

	Texture->SetPlatformData(MutableCreateImagePlatformData(MutableImage,OnlyLOD,0,0) );
}


void UCustomizableInstancePrivate::InitSkeletalMeshData(const TSharedRef<FUpdateContextPrivate>& OperationData, USkeletalMesh* SkeletalMesh, const FMutableRefSkeletalMeshData& RefSkeletalMeshData, const UCustomizableObject& CustomizableObject, int32 ComponentIndex)
{
	MUTABLE_CPUPROFILER_SCOPE(UCustomizableInstancePrivate::InitSkeletalMesh);

	check(SkeletalMesh);

	SkeletalMesh->NeverStream = !OperationData->bStreamMeshLODs;

	SkeletalMesh->SetImportedBounds(RefSkeletalMeshData.Bounds);
	SkeletalMesh->SetPostProcessAnimBlueprint(RefSkeletalMeshData.PostProcessAnimInst.Get());
	SkeletalMesh->SetShadowPhysicsAsset(RefSkeletalMeshData.ShadowPhysicsAsset.Get());

	// Set Min LOD
	SkeletalMesh->SetMinLod(FMath::Max(CustomizableObject.LODSettings.MinLOD.GetDefault(), (int32)FirstLODAvailable));
	SkeletalMesh->SetQualityLevelMinLod(CustomizableObject.LODSettings.MinQualityLevelLOD);

	SkeletalMesh->SetHasBeenSimplified(false);
	SkeletalMesh->SetHasVertexColors(false);

	// Set the default Physics Assets
	SkeletalMesh->SetPhysicsAsset(RefSkeletalMeshData.PhysicsAsset.Get());
	SkeletalMesh->SetEnablePerPolyCollision(RefSkeletalMeshData.Settings.bEnablePerPolyCollision);

	// Asset User Data
	{
		for (const FMutableRefAssetUserData& MutAssetUserData : RefSkeletalMeshData.AssetUserData)
		{
			if (MutAssetUserData.AssetUserData && MutAssetUserData.AssetUserData->Data.Type == ECOResourceDataType::AssetUserData)
			{
				const FCustomizableObjectAssetUserData* DataPtr = MutAssetUserData.AssetUserData->Data.Data.GetPtr<FCustomizableObjectAssetUserData>();
				check(DataPtr);
#if WITH_EDITORONLY_DATA
				SkeletalMesh->AddAssetUserData(DataPtr->AssetUserDataEditor);
#else
				SkeletalMesh->AddAssetUserData(DataPtr->AssetUserData);
#endif
			}
		}

		const FCustomizableInstanceComponentData& ComponentData = ComponentsData[ComponentIndex];
		for (TObjectPtr<UAssetUserData> AssetUserData : ComponentData.AssetUserDataArray)
		{
			SkeletalMesh->AddAssetUserData(AssetUserData);
		}

		//Custom Asset User Data
		if (OperationData->Instance->GetAnimationGameplayTags().Num())
		{
			UCustomizableObjectInstanceUserData* InstanceData = NewObject<UCustomizableObjectInstanceUserData>(SkeletalMesh, NAME_None, RF_Public | RF_Transactional);
			InstanceData->SetAnimationGameplayTags(OperationData->Instance->GetAnimationGameplayTags());
			SkeletalMesh->AddAssetUserData(InstanceData);
		}
	}

	// Allocate resources for rendering and add LOD Info
	{
		MUTABLE_CPUPROFILER_SCOPE(InitSkeletalMesh_AddLODData);
		SkeletalMesh->AllocateResourceForRendering();

		FSkeletalMeshRenderData* RenderData = SkeletalMesh->GetResourceForRendering();
		RenderData->NumInlinedLODs = OperationData->NumLODsAvailable - OperationData->FirstResidentLOD;
		RenderData->NumNonOptionalLODs = OperationData->NumLODsAvailable - OperationData->FirstLODAvailable;
		RenderData->CurrentFirstLODIdx = OperationData->FirstResidentLOD;
		RenderData->LODBiasModifier = 0;

		for (int32 LODIndex = 0; LODIndex < OperationData->NumLODsAvailable; ++LODIndex)
		{
			RenderData->LODRenderData.Add(new FSkeletalMeshLODRenderData());
			
			FSkeletalMeshLODRenderData& LODRenderData = RenderData->LODRenderData[LODIndex];
			LODRenderData.bIsLODOptional = LODIndex < OperationData->FirstLODAvailable;
			LODRenderData.bStreamedDataInlined = LODIndex >= OperationData->FirstResidentLOD;

			const FMutableRefLODData& LODData = RefSkeletalMeshData.LODData[LODIndex];
			FSkeletalMeshLODInfo& LODInfo = SkeletalMesh->AddLODInfo();
			LODInfo.ScreenSize = LODData.LODInfo.ScreenSize;
			LODInfo.LODHysteresis = LODData.LODInfo.LODHysteresis;
			LODInfo.bSupportUniformlyDistributedSampling = LODData.LODInfo.bSupportUniformlyDistributedSampling;
			LODInfo.bAllowCPUAccess = LODData.LODInfo.bAllowCPUAccess;

			// Disable LOD simplification when baking instances
			LODInfo.ReductionSettings.NumOfTrianglesPercentage = 1.f;
			LODInfo.ReductionSettings.NumOfVertPercentage = 1.f;
			LODInfo.ReductionSettings.MaxNumOfTriangles = TNumericLimits<uint32>::Max();
			LODInfo.ReductionSettings.MaxNumOfVerts = TNumericLimits<uint32>::Max();
			LODInfo.ReductionSettings.bRecalcNormals = 0;
			LODInfo.ReductionSettings.WeldingThreshold = TNumericLimits<float>::Min();
			LODInfo.ReductionSettings.bMergeCoincidentVertBones = 0;
			LODInfo.ReductionSettings.bImproveTrianglesForCloth = 0;

#if WITH_EDITORONLY_DATA
			LODInfo.ReductionSettings.MaxNumOfTrianglesPercentage = TNumericLimits<uint32>::Max();
			LODInfo.ReductionSettings.MaxNumOfVertsPercentage = TNumericLimits<uint32>::Max();

			LODInfo.BuildSettings.bRecomputeNormals = false;
			LODInfo.BuildSettings.bRecomputeTangents = false;
			LODInfo.BuildSettings.bUseMikkTSpace = false;
			LODInfo.BuildSettings.bComputeWeightedNormals = false;
			LODInfo.BuildSettings.bRemoveDegenerates = false;
			LODInfo.BuildSettings.bUseHighPrecisionTangentBasis = false;
			LODInfo.BuildSettings.bUseHighPrecisionSkinWeights = false;
			LODInfo.BuildSettings.bUseFullPrecisionUVs = true;
			LODInfo.BuildSettings.bUseBackwardsCompatibleF16TruncUVs = false;
			LODInfo.BuildSettings.ThresholdPosition = TNumericLimits<float>::Min();
			LODInfo.BuildSettings.ThresholdTangentNormal = TNumericLimits<float>::Min();
			LODInfo.BuildSettings.ThresholdUV = TNumericLimits<float>::Min();
			LODInfo.BuildSettings.MorphThresholdPosition = TNumericLimits<float>::Min();
			LODInfo.BuildSettings.BoneInfluenceLimit = 0;
#endif
			LODInfo.LODMaterialMap.SetNumZeroed(1);
		}
	}

	// Set up unreal's default material, will be replaced when building materials
	{
		MUTABLE_CPUPROFILER_SCOPE(InitSkeletalMesh_AddDefaultMaterial);
		UMaterialInterface* UnrealMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
		SkeletalMesh->GetMaterials().SetNum(1);
		SkeletalMesh->GetMaterials()[0] = UnrealMaterial;

		// Default density
		SetMeshUVChannelDensity(SkeletalMesh->GetMaterials()[0].UVChannelData);
	}

}


bool UCustomizableInstancePrivate::BuildSkeletonData(const TSharedRef<FUpdateContextPrivate>& OperationData, USkeletalMesh& SkeletalMesh, const FMutableRefSkeletalMeshData& RefSkeletalMeshData, UCustomizableObject& CustomizableObject, int32 ComponentIndex)
{
	MUTABLE_CPUPROFILER_SCOPE(UCustomizableInstancePrivate::BuildSkeletonData);

	const TObjectPtr<USkeleton> Skeleton = MergeSkeletons(CustomizableObject, RefSkeletalMeshData, ComponentIndex);
	if (!Skeleton)
	{
		return false;
	}

	SkeletalMesh.SetSkeleton(Skeleton);

	SkeletalMesh.SetRefSkeleton(Skeleton->GetReferenceSkeleton());
	FReferenceSkeleton& ReferenceSkeleton = SkeletalMesh.GetRefSkeleton();

	// Check that the bones we need are present in the current Skeleton
	FInstanceUpdateData::FSkeletonData& MutSkeletonData = OperationData->InstanceUpdateData.Skeletons[ComponentIndex];
	const int32 MutBoneCount = MutSkeletonData.BoneIds.Num();
	
	TMap<uint16, uint16> BoneToFinalBoneIndexMap;
	BoneToFinalBoneIndexMap.Reserve(MutBoneCount);

	{
		MUTABLE_CPUPROFILER_SCOPE(BuildSkeletonData_EnsureBonesExist);

		const FModelResources& ModelResources = CustomizableObject.GetPrivate()->GetModelResources();

		// Ensure all the required bones are present in the skeleton
		for (int32 BoneIndex = 0; BoneIndex < MutBoneCount; ++BoneIndex)
		{
			const uint16 BoneId = MutSkeletonData.BoneIds[BoneIndex];
			check(ModelResources.BoneNames.IsValidIndex(BoneId));

			const FName BoneName = ModelResources.BoneNames[BoneId];
			check(BoneName != NAME_None);

			const int32 SourceBoneIndex = ReferenceSkeleton.FindRawBoneIndex(BoneName);
			if (SourceBoneIndex == INDEX_NONE)
			{
				// Merged skeleton is missing some bones! This happens if one of the skeletons involved in the merge is discarded due to being incompatible with the rest
				// or if the source mesh is not in sync with the skeleton. 
				UE_LOG(LogMutable, Warning, TEXT("Building instance: generated mesh has a bone [%s] not present in the reference mesh [%s]. Failing to generate mesh. "),
					*BoneName.ToString(), *SkeletalMesh.GetName());
				return false;
			}

			BoneToFinalBoneIndexMap.Add(BoneId, SourceBoneIndex);
		}
	}

	{
		MUTABLE_CPUPROFILER_SCOPE(BuildSkeletonData_FixBoneIndices);

		// Fix up BoneMaps and ActiveBones indices
		for (FInstanceUpdateData::FComponent& Component : OperationData->InstanceUpdateData.Components)
		{
			if (Component.Id != ComponentIndex)
			{
				continue;
			}

			for (uint32 BoneMapIndex = Component.FirstBoneMap; BoneMapIndex < Component.FirstBoneMap + Component.BoneMapCount; ++BoneMapIndex)
			{
				const int32 BoneId = OperationData->InstanceUpdateData.BoneMaps[BoneMapIndex];
				OperationData->InstanceUpdateData.BoneMaps[BoneMapIndex] = BoneToFinalBoneIndexMap[BoneId];
			}

			for (uint16& BoneId : Component.ActiveBones)
			{
				BoneId = BoneToFinalBoneIndexMap[BoneId];
			}
			Component.ActiveBones.Sort();
		}
	}
	
	{
		MUTABLE_CPUPROFILER_SCOPE(BuildSkeletonData_ApplyPose);
		
		const int32 RefRawBoneCount = ReferenceSkeleton.GetRawBoneNum();

		TArray<FMatrix44f>& RefBasesInvMatrix = SkeletalMesh.GetRefBasesInvMatrix();
		RefBasesInvMatrix.Empty(RefRawBoneCount);

		// Initialize the base matrices
		if (RefRawBoneCount == MutBoneCount)
		{
			RefBasesInvMatrix.AddUninitialized(MutBoneCount);
		}
		else
		{
			// Bad case, some bone poses are missing, calculate the InvRefMatrices to ensure all transforms are there for the second step 
			MUTABLE_CPUPROFILER_SCOPE(BuildSkeletonData_CalcInvRefMatrices0);
			SkeletalMesh.CalculateInvRefMatrices();
		}

		// First step is to update the RefBasesInvMatrix for the bones.
		for (int32 BoneIndex = 0; BoneIndex < MutBoneCount; ++BoneIndex)
		{
			const int32 RefSkelBoneIndex = BoneToFinalBoneIndexMap[MutSkeletonData.BoneIds[BoneIndex]];
			RefBasesInvMatrix[RefSkelBoneIndex] = MutSkeletonData.BoneMatricesWithScale[BoneIndex];
		}

		// The second step is to update the pose transforms in the ref skeleton from the BasesInvMatrix
		FReferenceSkeletonModifier SkeletonModifier(ReferenceSkeleton, Skeleton);
		for (int32 RefSkelBoneIndex = 0; RefSkelBoneIndex < ReferenceSkeleton.GetRawBoneNum(); ++RefSkelBoneIndex)
		{
			int32 ParentBoneIndex = ReferenceSkeleton.GetParentIndex(RefSkelBoneIndex);
			if (ParentBoneIndex >= 0)
			{
				const FTransform3f BonePoseTransform(
						RefBasesInvMatrix[RefSkelBoneIndex].Inverse() * RefBasesInvMatrix[ParentBoneIndex]);

				SkeletonModifier.UpdateRefPoseTransform(RefSkelBoneIndex, (FTransform)BonePoseTransform);
			}
		}

		// Force a CalculateInvRefMatrices
		RefBasesInvMatrix.Empty(RefRawBoneCount);
	}

	{
		MUTABLE_CPUPROFILER_SCOPE(BuildSkeletonData_CalcInvRefMatrices);
		SkeletalMesh.CalculateInvRefMatrices();
	}

	return true;
}


void UCustomizableInstancePrivate::BuildMeshSockets(const TSharedRef<FUpdateContextPrivate>& OperationData, USkeletalMesh* SkeletalMesh, const FModelResources& ModelResources, const FMutableRefSkeletalMeshData& RefSkeletalMeshData, mu::MeshPtrConst MutableMesh)
{
	// Build mesh sockets.
	MUTABLE_CPUPROFILER_SCOPE(UCustomizableInstancePrivate::BuildMeshSockets);
	check(SkeletalMesh);

	const uint32 SocketCount = RefSkeletalMeshData.Sockets.Num();

	TArray<TObjectPtr<USkeletalMeshSocket>>& Sockets = SkeletalMesh->GetMeshOnlySocketList();
	Sockets.Empty(SocketCount);
	TMap<FName, TTuple<int32, int32>> SocketMap; // Maps Socket name to Sockets Array index and priority
	
	// Add sockets used by the SkeletalMesh of reference.
	{
		MUTABLE_CPUPROFILER_SCOPE(BuildMeshSockets_RefMeshSockets);
	
		for (uint32 SocketIndex = 0; SocketIndex < SocketCount; ++SocketIndex)
		{
			const FMutableRefSocket& RefSocket = RefSkeletalMeshData.Sockets[SocketIndex];

			USkeletalMeshSocket* Socket = NewObject<USkeletalMeshSocket>(SkeletalMesh);

			Socket->SocketName = RefSocket.SocketName;
			Socket->BoneName = RefSocket.BoneName;

			Socket->RelativeLocation = RefSocket.RelativeLocation;
			Socket->RelativeRotation = RefSocket.RelativeRotation;
			Socket->RelativeScale = RefSocket.RelativeScale;

			Socket->bForceAlwaysAnimated = RefSocket.bForceAlwaysAnimated;
			const int32 LastIndex = Sockets.Add(Socket);

			SocketMap.Add(Socket->SocketName, TTuple<int32, int32>(LastIndex, RefSocket.Priority));
		}
	}

	// Add or update sockets modified by Mutable.
	if (MutableMesh)
	{
		MUTABLE_CPUPROFILER_SCOPE(BuildMeshSockets_MutableSockets);

		for (int32 TagIndex = 0; TagIndex < MutableMesh->GetTagCount(); ++TagIndex)
		{
			FString Tag = MutableMesh->GetTag(TagIndex);

			if (Tag.RemoveFromStart("__Socket:"))
			{
				check(Tag.IsNumeric());
				const int32 MutableSocketIndex = FCString::Atoi(*Tag);

				if (ModelResources.SocketArray.IsValidIndex(MutableSocketIndex))
				{
					const FMutableRefSocket& MutableSocket = ModelResources.SocketArray[MutableSocketIndex];
					int32 IndexToWriteSocket = -1;

					if (TTuple<int32, int32>* FoundSocket = SocketMap.Find(MutableSocket.SocketName))
					{
						if (FoundSocket->Value < MutableSocket.Priority)
						{
							// Overwrite the existing socket because the new mesh part one is higher priority
							IndexToWriteSocket = FoundSocket->Key;
							FoundSocket->Value = MutableSocket.Priority;
						}
					}
					else
					{
						// New Socket
						USkeletalMeshSocket* Socket = NewObject<USkeletalMeshSocket>(SkeletalMesh);
						IndexToWriteSocket = Sockets.Add(Socket);
						SocketMap.Add(MutableSocket.SocketName, TTuple<int32, int32>(IndexToWriteSocket, MutableSocket.Priority));
					}

					if (IndexToWriteSocket >= 0)
					{
						check(Sockets.IsValidIndex(IndexToWriteSocket));

						USkeletalMeshSocket* SocketToWrite = Sockets[IndexToWriteSocket];

						SocketToWrite->SocketName = MutableSocket.SocketName;
						SocketToWrite->BoneName = MutableSocket.BoneName;

						SocketToWrite->RelativeLocation = MutableSocket.RelativeLocation;
						SocketToWrite->RelativeRotation = MutableSocket.RelativeRotation;
						SocketToWrite->RelativeScale = MutableSocket.RelativeScale;

						SocketToWrite->bForceAlwaysAnimated = MutableSocket.bForceAlwaysAnimated;
					}
				}
			}
		}
	}

#if !WITH_EDITOR
	SkeletalMesh->RebuildSocketMap();
#endif // !WITH_EDITOR
}


void UCustomizableInstancePrivate::BuildOrCopyElementData(const TSharedRef<FUpdateContextPrivate>& OperationData, USkeletalMesh* SkeletalMesh, UCustomizableObjectInstance* CustomizableObjectInstance, int32 ComponentIndex)
{
	MUTABLE_CPUPROFILER_SCOPE(UCustomizableInstancePrivate::BuildOrCopyElementData);

	for (int32 LODIndex = FirstLODAvailable; LODIndex < OperationData->NumLODsAvailable; ++LODIndex)
	{
		const FInstanceUpdateData::FLOD& LOD = OperationData->InstanceUpdateData.LODs[LODIndex];

		if (!LOD.ComponentCount)
		{
			continue;
		}

		const FInstanceUpdateData::FComponent& Component = OperationData->InstanceUpdateData.Components[LOD.FirstComponent + ComponentIndex];

		if (!Component.bGenerated)
		{
			continue;
		}

		for (int32 SurfaceIndex = 0; SurfaceIndex < Component.SurfaceCount; ++SurfaceIndex)
		{
			new(SkeletalMesh->GetResourceForRendering()->LODRenderData[LODIndex].RenderSections) FSkelMeshRenderSection();
		}
	}
}


void UCustomizableInstancePrivate::BuildOrCopyMorphTargetsData(const TSharedRef<FUpdateContextPrivate>& OperationData, USkeletalMesh* SkeletalMesh, const USkeletalMesh* LastUpdateSkeletalMesh, UCustomizableObjectInstance* CustomizableObjectInstance, int32 ComponentIndex)
{
	MUTABLE_CPUPROFILER_SCOPE(UCustomizableInstancePrivate::BuildOrCopyMorphTargetsData);

	if (!SkeletalMesh || !CustomizableObjectInstance)
	{
		return;
	}

	const UCustomizableObject* CustomizableObject = CustomizableObjectInstance->GetCustomizableObject();
	const TArray<FName>& MorphTargetNames = CustomizableObject->GetPrivate()->GetModelResources().RealTimeMorphTargetNames;

	const TMap<uint32, TArray<FMorphTargetVertexData>>& ResourceIdToVertexDataMap =
		OperationData->InstanceUpdateData.MorphTargetsVertexData;

	if (MorphTargetNames.IsEmpty() || ResourceIdToVertexDataMap.IsEmpty())
	{
		return;
	}

	TArray<int32> SectionMorphTargetVertices;

	SectionMorphTargetVertices.SetNumZeroed(MorphTargetNames.Num());

	TArray<uint8> UsedMorphSet;
	UsedMorphSet.SetNum(MorphTargetNames.Num());

	TArray<TArray<FMorphTargetLODModel>> MorphsData;
	MorphsData.SetNum(MorphTargetNames.Num());

	TArray<TObjectPtr<UMorphTarget>>& MorphTargets = SkeletalMesh->GetMorphTargets();

	const int32 FirstGeneratedLOD = FMath::Max((int32)OperationData->GetRequestedLODs()[ComponentIndex], OperationData->GetMinLOD());
	for (int32 LODIndex = FirstGeneratedLOD; LODIndex < OperationData->NumLODsAvailable; ++LODIndex)
	{
		MUTABLE_CPUPROFILER_SCOPE(BuildMorphTargetsData);

		int32 NumNotFoundLoadedMorphsResources = 0;

		const FInstanceUpdateData::FLOD& LOD = OperationData->InstanceUpdateData.LODs[LODIndex];
		const FInstanceUpdateData::FComponent& Component = OperationData->InstanceUpdateData.Components[LOD.FirstComponent + ComponentIndex];
		check(Component.bGenerated);
		check(Component.Mesh);

		const mu::FMeshBufferSet& MeshSet = Component.Mesh->GetVertexBuffers();

		int32 VertexMorphsInfoIndexBufferIndex, VertexMorphsInfoIndexBufferChannel;
		MeshSet.FindChannel(mu::MBS_OTHER, 0, &VertexMorphsInfoIndexBufferIndex, &VertexMorphsInfoIndexBufferChannel);

		int32 VertexMorphsCountBufferIndex, VertexMorphsCountBufferChannel;
		MeshSet.FindChannel(mu::MBS_OTHER, 1, &VertexMorphsCountBufferIndex, &VertexMorphsCountBufferChannel);

		int32 VertexMorphsResourceIdBufferIndex, VertexMorphsResourceIdBufferChannel;
		MeshSet.FindChannel(mu::MBS_OTHER, 2, &VertexMorphsResourceIdBufferIndex, &VertexMorphsResourceIdBufferChannel);

		if (VertexMorphsInfoIndexBufferIndex < 0 || VertexMorphsCountBufferIndex < 0 || VertexMorphsResourceIdBufferIndex < 0)
		{
			continue;
		}

		const int32* const VertexMorphsInfoIndexBuffer = reinterpret_cast<const int32*>(MeshSet.GetBufferData(VertexMorphsInfoIndexBufferIndex));
		TArrayView<const int32> VertexMorphsInfoIndexView(VertexMorphsInfoIndexBuffer, MeshSet.GetElementCount());

		const uint16* const VertexMorphsCountBuffer = reinterpret_cast<const uint16*>(MeshSet.GetBufferData(VertexMorphsCountBufferIndex));
		TArrayView<const uint16> VertexMorphsCountView(VertexMorphsCountBuffer, MeshSet.GetElementCount());

		const uint16* const VertexMorphsResourceIdBuffer = reinterpret_cast<const uint16*>(MeshSet.GetBufferData(VertexMorphsResourceIdBufferIndex));
		TArrayView<const uint16> VertexMorphsResourceIdView(VertexMorphsResourceIdBuffer, MeshSet.GetElementCount());

		const int32 SurfaceCount = Component.Mesh->GetSurfaceCount();
		for (int32 Section = 0; Section < SurfaceCount; ++Section)
		{
			// Reset SectionMorphTargets.
			for (int32& Elem : SectionMorphTargetVertices)
			{
				Elem = 0;
			}

			int32 FirstVertex, VerticesCount, FirstIndex, IndiciesCount;
			Component.Mesh->GetSurface(Section, &FirstVertex, &VerticesCount, &FirstIndex, &IndiciesCount, nullptr, nullptr, nullptr);

			for (int32 VertexIdx = FirstVertex; VertexIdx < FirstVertex + VerticesCount;)
			{
				// Find a span with the same VertexMorphResourceId to amortise the cost of finding 
				// in the loaded resources map. It is expected to find large consecutive mesh sections pointing to
				// the same loaded resource.

				const int32 SpanStart = VertexIdx++;
				const uint16 CurrentResourceId = VertexMorphsResourceIdView[SpanStart];

				// Vertex with no morphs are marked with TNumericLimits<uint16>::Max(), skip vertex if the case.
				if (CurrentResourceId == TNumericLimits<uint16>::Max())
				{
					continue;
				}

				for (; VertexIdx < FirstVertex + VerticesCount; ++VertexIdx)
				{
					const int32 VertexResourceId = VertexMorphsResourceIdView[VertexIdx];
					// we can skip vertices with no morph without breaking the span.
					if (VertexResourceId == TNumericLimits<uint16>::Max())
					{
						continue;
					}

					if (CurrentResourceId != VertexResourceId)
					{
						break;
					}
				}
				const int32 SpanEnd = VertexIdx;

				const TArray<FMorphTargetVertexData>* MorphTargetReconstructionData = ResourceIdToVertexDataMap.Find(CurrentResourceId);

				if (!MorphTargetReconstructionData)
				{
					++NumNotFoundLoadedMorphsResources;
					continue;
				}

				const TArray<FMorphTargetVertexData>& SpanMorphData = *MorphTargetReconstructionData;


				// This assumes the number of vertex in an span will be large compared to the number of
				// morphs. Maybe the allocation could be done in a different pass as an optimization.
				FMemory::Memzero(UsedMorphSet.GetData(), UsedMorphSet.Num());
				for (int32 SpanVertexIdx = SpanStart; SpanVertexIdx < SpanEnd; ++SpanVertexIdx)
				{
					const int32 MorphCount = VertexMorphsCountView[SpanVertexIdx];

					TArrayView<const FMorphTargetVertexData> MorphsVertexDataView = MakeArrayView(
						SpanMorphData.GetData() + VertexMorphsInfoIndexView[SpanVertexIdx],
						MorphCount);

					for (const FMorphTargetVertexData& MorphVertexData : MorphsVertexDataView)
					{
						check((uint32)UsedMorphSet.Num() > MorphVertexData.MorphNameIndex);
						UsedMorphSet[MorphVertexData.MorphNameIndex] = 1;
					}
				}

				const int32 NumMorphs = UsedMorphSet.Num();
				for (int32 MorphIndex = 0; MorphIndex < NumMorphs; ++MorphIndex)
				{
					if (!UsedMorphSet[MorphIndex])
					{
						continue;
					}

					if (MorphsData[MorphIndex].IsEmpty())
					{
						MorphsData[MorphIndex].SetNum(OperationData->NumLODsAvailable);
					}
				}

				for (int32 SpanVertexIdx = SpanStart; SpanVertexIdx < SpanEnd; ++SpanVertexIdx)
				{
					const uint16 MorphCount = VertexMorphsCountView[SpanVertexIdx];
					if (MorphCount == 0)
					{
						continue;
					}

					TArrayView<const FMorphTargetVertexData> MorphsVertexDataView = MakeArrayView(
						SpanMorphData.GetData() + VertexMorphsInfoIndexView[SpanVertexIdx],
						MorphCount);

					for (const FMorphTargetVertexData& SourceVertex : MorphsVertexDataView)
					{
						FMorphTargetLODModel& DestMorphLODModel = MorphsData[SourceVertex.MorphNameIndex][LODIndex];

						DestMorphLODModel.Vertices.Emplace(
							FMorphTargetDelta{ SourceVertex.PositionDelta, SourceVertex.TangentZDelta, static_cast<uint32>(SpanVertexIdx) });

						++SectionMorphTargetVertices[SourceVertex.MorphNameIndex];
					}
				}
			}

			const int32 SectionMorphTargetsNum = SectionMorphTargetVertices.Num();
			for (int32 MorphIdx = 0; MorphIdx < SectionMorphTargetsNum; ++MorphIdx)
			{
				if (SectionMorphTargetVertices[MorphIdx] > 0)
				{
					FMorphTargetLODModel& MorphTargetLodModel = MorphsData[MorphIdx][LODIndex];

					MorphTargetLodModel.SectionIndices.Add(Section);
					MorphTargetLodModel.NumVertices += SectionMorphTargetVertices[MorphIdx];
				}
			}
		}

		if (NumNotFoundLoadedMorphsResources > 0)
		{
			UE_LOG(LogMutable, Warning, TEXT("Needed realtime morph reconstruction data was not loaded properly. Some realtime morphs may not work correctly."));
		}

		// Generate the SkeletalMesh data structures. The previous step could be done in an async task.
		MorphTargets.Empty();
		const int32 NumMorphs = MorphTargetNames.Num();
		for (int32 I = 0; I < NumMorphs; ++I)
		{
			if (MorphsData[I].IsEmpty())
			{
				continue;
			}

			UMorphTarget* NewMorphTarget = NewObject<UMorphTarget>(SkeletalMesh, MorphTargetNames[I]);
			NewMorphTarget->BaseSkelMesh = SkeletalMesh;
			NewMorphTarget->GetMorphLODModels() = MoveTemp(MorphsData[I]);
			MorphTargets.Add(NewMorphTarget);
		}
	}

	// Copy MorphTargets from the FirstGeneratedLOD to the LODs below
	for (int32 LODIndex = OperationData->FirstLODAvailable; LODIndex < FirstGeneratedLOD; ++LODIndex)
	{
		MUTABLE_CPUPROFILER_SCOPE(CopyMorphTargetsData);

		const int32 NumMorphTargets = MorphTargets.Num();
		for (int32 MorphTargetIndex = 0; MorphTargetIndex < NumMorphTargets; ++MorphTargetIndex)
		{
			MorphTargets[MorphTargetIndex]->GetMorphLODModels()[LODIndex] =
				MorphTargets[MorphTargetIndex]->GetMorphLODModels()[FirstGeneratedLOD];
		}
	}

	SkeletalMesh->InitMorphTargets();
}

namespace 
{
	// Only used to be able to create new clothing assets and assign a new guid to them without the factory.
	class UCustomizableObjectClothingAsset : public UClothingAssetCommon
	{
	public:
		void AssignNewGuid()
		{
			AssetGuid = FGuid::NewGuid();
		}
	};

}


void UCustomizableInstancePrivate::BuildOrCopyClothingData(const TSharedRef<FUpdateContextPrivate>&OperationData, USkeletalMesh * SkeletalMesh, const USkeletalMesh* LastUpdateSkeletalMesh, UCustomizableObjectInstance * CustomizableObjectInstance, int32 ComponentIndex)
{
	MUTABLE_CPUPROFILER_SCOPE(UCustomizableInstancePrivate::BuildOrCopyClothingData);

	if (!SkeletalMesh)
	{
		return;
	}

	const UCustomizableObject* CustomizableObject = CustomizableObjectInstance->GetCustomizableObject();
	const TArray<FCustomizableObjectClothingAssetData>& ContributingClothingAssetsData = CustomizableObject->ContributingClothingAssetsData;
	const TArray<FCustomizableObjectClothConfigData>& ClothSharedConfigsData = CustomizableObject->ClothSharedConfigsData;
	const TArray<FCustomizableObjectMeshToMeshVertData>& ClothMeshToMeshVertData = CustomizableObject->ClothMeshToMeshVertData;

	if (!(ContributingClothingAssetsData.Num() && ClothMeshToMeshVertData.Num()))
	{
		return;
	}

	// First we need to discover if any clothing asset is used for the instance. 

	struct FSectionWithClothData
	{
		int32 ClothAssetIndex;
		int32 ClothAssetLodIndex;
		int32 Section;
		int32 Lod;
		int32 BaseVertex;
		TArrayView<const uint16> SectionIndex16View;
		TArrayView<const uint32> SectionIndex32View;
		TArrayView<const int32> ClothingDataIndicesView;
		TArray<FMeshToMeshVertData> MappingData;
	};

	TArray<FSectionWithClothData> SectionsWithCloth;
	SectionsWithCloth.Reserve(32);

	const int32 LODCount = OperationData->InstanceUpdateData.LODs.Num();

	{
		MUTABLE_CPUPROFILER_SCOPE(DiscoverSectionsWithCloth);

		for (int32 LODIndex = OperationData->GetMinLOD(); LODIndex < OperationData->NumLODsAvailable; ++LODIndex)
		{
			const FInstanceUpdateData::FLOD& LOD = OperationData->InstanceUpdateData.LODs[LODIndex];
			if (ComponentIndex >= LOD.ComponentCount)
			{
				continue;
			}

			FInstanceUpdateData::FComponent& Component = OperationData->InstanceUpdateData.Components[LOD.FirstComponent + ComponentIndex];

			if (!Component.bGenerated)
			{
				continue;
			}

			if (mu::MeshPtrConst MutableMesh = Component.Mesh)
			{
				const mu::FMeshBufferSet& MeshSet = MutableMesh->GetVertexBuffers();

				const mu::FMeshBufferSet& IndicesSet = MutableMesh->GetIndexBuffers();

				// Semantics index may vary depending on whether realtime morph targets are enabled.
				const int32 ClothingDataBufferIndex = [&MeshSet]()
				{
					int32 BufferIndex, Channel;
					MeshSet.FindChannel(mu::MBS_OTHER, 3, &BufferIndex, &Channel);

					if (BufferIndex > 0)
					{
						return BufferIndex;
					}

					MeshSet.FindChannel(mu::MBS_OTHER, 0, &BufferIndex, &Channel);

					return BufferIndex;
				}(); // lambda is invoked

				if (ClothingDataBufferIndex < 0)
				{
					continue;
				}

				const int32* const ClothingDataBuffer = reinterpret_cast<const int32*>(MeshSet.GetBufferData(ClothingDataBufferIndex));

				const int32 SurfaceCount = MutableMesh->GetSurfaceCount();
				for (int32 Section = 0; Section < SurfaceCount; ++Section)
				{
					int FirstVertex, VerticesCount, FirstIndex, IndicesCount;
					MutableMesh->GetSurface(Section, &FirstVertex, &VerticesCount, &FirstIndex, &IndicesCount, nullptr, nullptr, nullptr);

					if (VerticesCount == 0 || IndicesCount == 0)
					{
						continue;
					}
					
					// A Section has cloth data on all its vertices or it does not have it in any.
					// It can be determined if this section has clothing data just looking at the 
					// first vertex of the section.
					TArrayView<const int32> ClothingDataView(ClothingDataBuffer + FirstVertex, VerticesCount);

					const int32 IndexCount = MutableMesh->GetIndexBuffers().GetElementCount();

					TArrayView<const uint16> IndicesView16Bits;
					TArrayView<const uint32> IndicesView32Bits;
					
					if (IndexCount)
					{
						 if (MutableMesh->GetIndexBuffers().GetElementSize(0) == 2)
						 {
						 	const uint16* IndexPtr = (const uint16*)MutableMesh->GetIndexBuffers().GetBufferData(0);
							IndicesView16Bits = TArrayView<const uint16>(IndexPtr + FirstIndex, IndicesCount);
						 }
						 else
						 {
						 	const uint32* IndexPtr = (const uint32*)MutableMesh->GetIndexBuffers().GetBufferData(0);
							IndicesView32Bits = TArrayView<const uint32>(IndexPtr + FirstIndex, IndicesCount);
						 }
					}
					
					if (!ClothingDataView.Num())
					{
						continue;
					}

					const int32 ClothDataIndex = ClothingDataView[0];
					if (ClothDataIndex < 0)
					{
						continue;
					}

					const int32 ClothAssetIndex = ClothMeshToMeshVertData[ClothDataIndex].SourceAssetIndex;
					const int32 ClothAssetLodIndex = ClothMeshToMeshVertData[ClothDataIndex].SourceAssetLodIndex;

					// Defensive check, this indicates the clothing data might be stale and needs to be recompiled.
					// Should never happen.
					if (!ensure(ClothAssetIndex >= 0 && ClothAssetIndex < ContributingClothingAssetsData.Num()
						&& ContributingClothingAssetsData[ClothAssetIndex].LodData.Num()))
					{
						continue;
					}

					SectionsWithCloth.Add
							(FSectionWithClothData{ ClothAssetIndex, ClothAssetLodIndex, Section, LODIndex, FirstVertex, IndicesView16Bits, IndicesView32Bits, ClothingDataView, TArray<FMeshToMeshVertData>() });
				}
			}
		}
	}

	if (!SectionsWithCloth.Num())
	{
		return; // Nothing to do.
	}
	
	TArray<FCustomizableObjectClothingAssetData> NewClothingAssetsData;
	NewClothingAssetsData.SetNum( ContributingClothingAssetsData.Num() );

	{

		for (FSectionWithClothData& SectionWithCloth : SectionsWithCloth)
		{
			const FCustomizableObjectClothingAssetData& SrcAssetData = ContributingClothingAssetsData[SectionWithCloth.ClothAssetIndex];
			FCustomizableObjectClothingAssetData& DstAssetData = NewClothingAssetsData[SectionWithCloth.ClothAssetIndex];
		
			// Only initilialize once, multiple sections with cloth could point to the same cloth asset.
			if (!DstAssetData.LodMap.Num())
			{
				DstAssetData.LodMap.Init( INDEX_NONE, OperationData->NumLODsAvailable);

				DstAssetData.LodData.SetNum( SrcAssetData.LodData.Num() );
				DstAssetData.UsedBoneNames = SrcAssetData.UsedBoneNames;
				DstAssetData.UsedBoneIndices = SrcAssetData.UsedBoneIndices;
				DstAssetData.ReferenceBoneIndex = SrcAssetData.ReferenceBoneIndex;
				DstAssetData.Name = SrcAssetData.Name;
			}

			DstAssetData.LodMap[SectionWithCloth.Lod] = SectionWithCloth.ClothAssetLodIndex;
		}
	}

	{
		MUTABLE_CPUPROFILER_SCOPE(CopyMeshToMeshData)
		
		// Copy MeshToMeshData.
		for (FSectionWithClothData& SectionWithCloth : SectionsWithCloth)
		{
			const int32 NumVertices = SectionWithCloth.ClothingDataIndicesView.Num();
			
			TArray<FMeshToMeshVertData>& ClothMappingData = SectionWithCloth.MappingData;
			ClothMappingData.SetNum(NumVertices);

			// Copy mesh to mesh data indexed by the index stored per vertex at compile time. 
			for (int32 VertexIdx = 0; VertexIdx < NumVertices; ++VertexIdx)
			{
				// Possible Optimization: Gather consecutive indices in ClothingDataView and Memcpy the whole range.
				// FMeshToMeshVertData and FCustomizableObjectMeshToMeshVertData have the same memory footprint and
				// bytes in a FCustomizableObjectMeshToMeshVertData form a valid FMeshToMeshVertData (not the other way around).

				static_assert(sizeof(FCustomizableObjectMeshToMeshVertData) == sizeof(FMeshToMeshVertData), "");
				static_assert(TIsTrivial<FCustomizableObjectMeshToMeshVertData>::Value, "");
				static_assert(TIsTrivial<FMeshToMeshVertData>::Value, "");

				const int32 VertexDataIndex = SectionWithCloth.ClothingDataIndicesView[VertexIdx];
				check(VertexDataIndex > 0);

				const FCustomizableObjectMeshToMeshVertData& SrcData = ClothMeshToMeshVertData[VertexDataIndex];

				FMeshToMeshVertData& DstData = ClothMappingData[VertexIdx];
				FMemory::Memcpy(&DstData, &SrcData, sizeof(FMeshToMeshVertData));
			}
		}
	}

	// Indices remaps for {Section, AssetLod}, needed to recreate the lod transition data.
	TMap<int32, TArray<TArray<int32>>> PhysicsSectionLodsIndicesRemaps;

	check( SectionsWithCloth.Num() > 0 );
	FSectionWithClothData* MaxSection = Algo::MaxElement(SectionsWithCloth, 
			[](const FSectionWithClothData& A, const FSectionWithClothData& B) { return  A.ClothAssetIndex < B.ClothAssetIndex; } );
	
	PhysicsSectionLodsIndicesRemaps.Reserve(MaxSection->ClothAssetIndex + 1);
		
	for (FSectionWithClothData& SectionLods : SectionsWithCloth)
	{
		TArray<TArray<int32>>& Value = PhysicsSectionLodsIndicesRemaps.FindOrAdd(SectionLods.ClothAssetIndex, {});
		Value.SetNum(FMath::Max(Value.Num(), SectionLods.ClothAssetLodIndex + 1));
	}

	{
		MUTABLE_CPUPROFILER_SCOPE(RemapPhysicsMesh)

		for (FSectionWithClothData& SectionWithCloth : SectionsWithCloth)
		{
			const FCustomizableObjectClothingAssetData& SrcClothingAssetData = ContributingClothingAssetsData[SectionWithCloth.ClothAssetIndex];
			FCustomizableObjectClothingAssetData& NewClothingAssetData = NewClothingAssetsData[SectionWithCloth.ClothAssetIndex];
					
			const FClothLODDataCommon& SrcLodData = SrcClothingAssetData.LodData[SectionWithCloth.ClothAssetLodIndex];
			FClothLODDataCommon& NewLodData = NewClothingAssetData.LodData[SectionWithCloth.ClothAssetLodIndex];
			
			const int32 PhysicalMeshVerticesNum = SrcLodData.PhysicalMeshData.Vertices.Num();

			if (!PhysicalMeshVerticesNum)
			{
				// Nothing to do.
				continue;
			}
			
			// Vertices not indexed in the mesh to mesh data generated for this section need to be removed.
			TArray<uint8> VertexUtilizationBuffer;
			VertexUtilizationBuffer.Init(0, PhysicalMeshVerticesNum);
		
			// Discover used vertices.
			const int32 SectionVerticesNum = SectionWithCloth.ClothingDataIndicesView.Num();

			TArray<uint8> RenderVertexUtilizationBuffer;
			RenderVertexUtilizationBuffer.Init(0, SectionVerticesNum);

			// Sometimes, at least when a clip morph is applied, vertices are not removed from the section
			// and only the triangles (indices) that form the mesh are modified.

			auto GenerateRenderUtilizationBuffer = [&RenderVertexUtilizationBuffer](const auto& IndicesView, int32 SectionBaseVertex )
			{
				const int32 IndicesCount = IndicesView.Num();
				check(IndicesCount % 3 == 0);
				for ( int32 I = 0; I < IndicesCount; I += 3 )
				{
					RenderVertexUtilizationBuffer[int32(IndicesView[I + 0]) - SectionBaseVertex] = 1;
					RenderVertexUtilizationBuffer[int32(IndicesView[I + 1]) - SectionBaseVertex] = 1;
					RenderVertexUtilizationBuffer[int32(IndicesView[I + 2]) - SectionBaseVertex] = 1;
				}
			};
			
			if (SectionWithCloth.SectionIndex16View.Num())
			{
				GenerateRenderUtilizationBuffer(SectionWithCloth.SectionIndex16View, SectionWithCloth.BaseVertex);
			}
			else
			{
				check(SectionWithCloth.SectionIndex32View.Num());
				GenerateRenderUtilizationBuffer(SectionWithCloth.SectionIndex32View, SectionWithCloth.BaseVertex);
			}

			const TArray<FMeshToMeshVertData>& SectionClothMappingData = SectionWithCloth.MappingData;
			for (int32 Idx = 0; Idx < SectionVerticesNum; ++Idx)
			{
				if (RenderVertexUtilizationBuffer[Idx])
				{
					const uint16* Indices = SectionClothMappingData[Idx].SourceMeshVertIndices;

					VertexUtilizationBuffer[Indices[0]] = 1;
					VertexUtilizationBuffer[Indices[1]] = 1;
					VertexUtilizationBuffer[Indices[2]] = 1;
				}
			}

			TArray<int32>& IndexMap = PhysicsSectionLodsIndicesRemaps[SectionWithCloth.ClothAssetIndex][SectionWithCloth.ClothAssetLodIndex];
			IndexMap.SetNumUninitialized(PhysicalMeshVerticesNum);

			// Compute index remap and number of remaining physics vertices.
			// -1 indicates the vertex has been removed.
			int32 NewPhysicalMeshVerticesNum = 0;
			for (int32 Idx = 0; Idx < PhysicalMeshVerticesNum; ++Idx)
			{
				IndexMap[Idx] = VertexUtilizationBuffer[Idx] ? NewPhysicalMeshVerticesNum++ : -1;
			}

			const bool bHasVerticesRemoved = NewPhysicalMeshVerticesNum < PhysicalMeshVerticesNum;
			if (!bHasVerticesRemoved)
			{
				// If no vertices are removed the IndexMap is no longer needed. The lack of data in the map 
				// can indicates that no vertex has been removed to subsequent operations.
				IndexMap.Reset();
			}
			
			const auto CopyIfUsed = [&VertexUtilizationBuffer, bHasVerticesRemoved](auto& Dst, const auto& Src)
			{	
				const int32 SrcNumElems = Src.Num();

				if (!bHasVerticesRemoved)
				{
					for (int32 Idx = 0; Idx < SrcNumElems; ++Idx)
					{
						Dst[Idx] = Src[Idx];
					}

					return;
				}

				for (int32 Idx = 0, DstNumElems = 0; Idx < SrcNumElems; ++Idx)
				{
					if (VertexUtilizationBuffer[Idx])
					{
						Dst[DstNumElems++] = Src[Idx];
					}
				}
			};

			NewLodData.PhysicalMeshData.MaxBoneWeights = SrcLodData.PhysicalMeshData.MaxBoneWeights;

			NewLodData.PhysicalMeshData.Vertices.SetNum(NewPhysicalMeshVerticesNum);
			NewLodData.PhysicalMeshData.Normals.SetNum(NewPhysicalMeshVerticesNum);
			NewLodData.PhysicalMeshData.BoneData.SetNum(NewPhysicalMeshVerticesNum);
			NewLodData.PhysicalMeshData.InverseMasses.SetNum(NewPhysicalMeshVerticesNum);

			CopyIfUsed(NewLodData.PhysicalMeshData.Vertices, SrcLodData.PhysicalMeshData.Vertices);
			CopyIfUsed(NewLodData.PhysicalMeshData.Normals, SrcLodData.PhysicalMeshData.Normals);
			CopyIfUsed(NewLodData.PhysicalMeshData.BoneData, SrcLodData.PhysicalMeshData.BoneData);
			CopyIfUsed(NewLodData.PhysicalMeshData.InverseMasses, SrcLodData.PhysicalMeshData.InverseMasses);

			const int32 PrevIndex = SectionWithCloth.Lod - 1;
			const bool bNeedsTransitionUpData = NewClothingAssetData.LodMap.IsValidIndex(PrevIndex) && NewClothingAssetData.LodMap[PrevIndex] != INDEX_NONE;
			if (bNeedsTransitionUpData)
			{
				NewLodData.TransitionUpSkinData.SetNum(SrcLodData.TransitionUpSkinData.Num() ? NewPhysicalMeshVerticesNum : 0);	
				CopyIfUsed(NewLodData.TransitionUpSkinData, SrcLodData.TransitionUpSkinData);
			}

			const int32 NextIndex = SectionWithCloth.Lod + 1;
			const bool bNeedsTransitionDownData = NewClothingAssetData.LodMap.IsValidIndex(NextIndex) && NewClothingAssetData.LodMap[NextIndex] != INDEX_NONE;
			if (bNeedsTransitionDownData)
			{
				NewLodData.TransitionDownSkinData.SetNum(SrcLodData.TransitionDownSkinData.Num() ? NewPhysicalMeshVerticesNum : 0);
				CopyIfUsed(NewLodData.TransitionDownSkinData, SrcLodData.TransitionDownSkinData);
			}
			
			const TMap<uint32, FPointWeightMap>& SrcPhysWeightMaps = SrcLodData.PhysicalMeshData.WeightMaps;
			TMap<uint32, FPointWeightMap>& NewPhysWeightMaps = NewLodData.PhysicalMeshData.WeightMaps;

			for (const TPair<uint32, FPointWeightMap>& WeightMap : SrcPhysWeightMaps)
			{
				if (WeightMap.Value.Values.Num() > 0)
				{
					FPointWeightMap& NewWeightMap = NewLodData.PhysicalMeshData.AddWeightMap(WeightMap.Key);
					NewWeightMap.Values.SetNum(NewPhysicalMeshVerticesNum);

					CopyIfUsed(NewWeightMap.Values, WeightMap.Value.Values);
				}
			}
			// Remap render mesh to mesh indices.
			if (bHasVerticesRemoved)
			{
				for (FMeshToMeshVertData& VertClothData : SectionWithCloth.MappingData)
				{
					uint16* Indices = VertClothData.SourceMeshVertIndices;
					Indices[0] = (uint16)IndexMap[Indices[0]];
					Indices[1] = (uint16)IndexMap[Indices[1]];
					Indices[2] = (uint16)IndexMap[Indices[2]];
				}
			}

			// Remap and trim physics mesh vertices and self collision indices. 
			
			// Returns the final size of Dst.
			const auto TrimAndRemapTriangles = [&IndexMap](TArray<uint32>& Dst, const TArray<uint32>& Src) -> int32
			{
				check(Src.Num() % 3 == 0);

				const int32 SrcNumElems = Src.Num();
				if (!IndexMap.Num())
				{
					//for (int32 Idx = 0; Idx < SrcNumElems; ++Idx)
					//{
					//	Dst[Idx] = Src[Idx];
					//}

					FMemory::Memcpy( Dst.GetData(), Src.GetData(), SrcNumElems*sizeof(uint32) );
					return SrcNumElems;
				}

				int32 DstNumElems = 0;
				for (int32 Idx = 0; Idx < SrcNumElems; Idx += 3)
				{
					const int32 Idx0 = IndexMap[Src[Idx + 0]];
					const int32 Idx1 = IndexMap[Src[Idx + 1]];
					const int32 Idx2 = IndexMap[Src[Idx + 2]];

					// triangles are only copied if all vertices are used.
					if (!((Idx0 < 0) | (Idx1 < 0) | (Idx2 < 0)))
					{
						Dst[DstNumElems + 0] = Idx0;
						Dst[DstNumElems + 1] = Idx1;
						Dst[DstNumElems + 2] = Idx2;

						DstNumElems += 3;
					}
				}

				return DstNumElems;
			};

			const TArray<uint32>& SrcPhysicalMeshIndices = SrcLodData.PhysicalMeshData.Indices;
			TArray<uint32>& NewPhysicalMeshIndices = NewLodData.PhysicalMeshData.Indices;
			NewPhysicalMeshIndices.SetNum(SrcPhysicalMeshIndices.Num());
			NewPhysicalMeshIndices.SetNum(TrimAndRemapTriangles(NewPhysicalMeshIndices, SrcPhysicalMeshIndices), EAllowShrinking::No);
		
			const auto TrimAndRemapVertexSet = [&IndexMap](TSet<int32>& Dst, const TSet<int32>& Src)
			{	
				if (!IndexMap.Num())
				{
					Dst = Src;
					return;
				}

				Dst.Reserve(Src.Num());
				for(const int32 SrcIdx : Src)
				{
					const int32 MappedIdx = IndexMap[SrcIdx];

					if (MappedIdx >= 0)
					{
						Dst.Add(MappedIdx);
					}
				}
			};

			const TSet<int32>& SrcSelfCollisionVertexSet = SrcLodData.PhysicalMeshData.SelfCollisionVertexSet;
			TSet<int32>& NewSelfCollisionVertexSet = NewLodData.PhysicalMeshData.SelfCollisionVertexSet;
			TrimAndRemapVertexSet(NewSelfCollisionVertexSet, SrcSelfCollisionVertexSet);
						
			{
				MUTABLE_CPUPROFILER_SCOPE(BuildClothTetherData)

				auto TrimAndRemapTethers = [&IndexMap](FClothTetherData& Dst, const FClothTetherData& Src)
				{
					if (!IndexMap.Num())
					{
						Dst.Tethers = Src.Tethers;
						return;
					}

					Dst.Tethers.Reserve(Src.Tethers.Num());
					for ( const TArray<TTuple<int32, int32, float>>& SrcTetherCluster : Src.Tethers )
					{
						TArray<TTuple<int32, int32, float>>& DstTetherCluster = Dst.Tethers.Emplace_GetRef();
						DstTetherCluster.Reserve(SrcTetherCluster.Num());
						for ( const TTuple<int32, int32, float>& Tether : SrcTetherCluster )
						{
							const int32 Index0 = IndexMap[Tether.Get<0>()];
							const int32 Index1 = IndexMap[Tether.Get<1>()];
							if ((Index0 >= 0) & (Index1 >= 0))
							{
								DstTetherCluster.Emplace(Index0, Index1, Tether.Get<2>());
							}
						}

						if (!DstTetherCluster.Num())
						{
							Dst.Tethers.RemoveAt( Dst.Tethers.Num() - 1, 1, EAllowShrinking::No );
						}
					}
				};

				TrimAndRemapTethers(NewLodData.PhysicalMeshData.GeodesicTethers, SrcLodData.PhysicalMeshData.GeodesicTethers);
				TrimAndRemapTethers(NewLodData.PhysicalMeshData.EuclideanTethers, SrcLodData.PhysicalMeshData.EuclideanTethers);
			}
		}
	}

	// Try to find plausible values for LodTransitionData vertices that have lost the triangle to which are attached.
	{
		MUTABLE_CPUPROFILER_SCOPE(BuildLodTransitionData)
		
		for (const FSectionWithClothData& SectionWithCloth : SectionsWithCloth)
		{

			FCustomizableObjectClothingAssetData& NewClothingAssetData = NewClothingAssetsData[SectionWithCloth.ClothAssetIndex];
			FClothLODDataCommon& NewLodData = NewClothingAssetData.LodData[SectionWithCloth.ClothAssetLodIndex];

			const int32 PhysicalMeshVerticesNum = NewLodData.PhysicalMeshData.Vertices.Num();

			if (!PhysicalMeshVerticesNum)
			{
				// Nothing to do.
				continue;
			}
		
			auto RemapTransitionMeshToMeshVertData = []( TArray<FMeshToMeshVertData>& InOutVertData, TArray<int32>& IndexMap )
			{
				for (FMeshToMeshVertData& VertData : InOutVertData)
				{
					uint16* Indices = VertData.SourceMeshVertIndices;
					Indices[0] = (uint16)IndexMap[Indices[0]];
					Indices[1] = (uint16)IndexMap[Indices[1]];
					Indices[2] = (uint16)IndexMap[Indices[2]];
				}
			};

			if (NewLodData.TransitionDownSkinData.Num() > 0)
			{	
				TArray<int32>& IndexMap = PhysicsSectionLodsIndicesRemaps[SectionWithCloth.ClothAssetIndex][SectionWithCloth.ClothAssetLodIndex + 1];

				if (IndexMap.Num())
				{
					RemapTransitionMeshToMeshVertData( NewLodData.TransitionDownSkinData, IndexMap );
				}
			}
			
			if (NewLodData.TransitionUpSkinData.Num() > 0)
			{	
				TArray<int32>& IndexMap = PhysicsSectionLodsIndicesRemaps[SectionWithCloth.ClothAssetIndex][SectionWithCloth.ClothAssetLodIndex - 1];
				if (IndexMap.Num())
				{
					RemapTransitionMeshToMeshVertData( NewLodData.TransitionUpSkinData, IndexMap );
				}
			}

			struct FMeshPhysicsDesc
			{
				const TArray<FVector3f>& Vertices;
				const TArray<FVector3f>& Normals;
				const TArray<uint32>& Indices;
			};	
		
			auto RebindVertex = [](const FMeshPhysicsDesc& Mesh, const FVector3f& InPosition, const FVector3f& InNormal, FMeshToMeshVertData& Out)
			{
				const FVector3f Normal = InNormal;

				// We don't have the mesh tangent, find something plausible.
				FVector3f Tan0, Tan1;
				Normal.FindBestAxisVectors(Tan0, Tan1);
				const FVector3f Tangent = Tan0;
				
				// Some of the math functions take as argument FVector, we'd want to be FVector3f. 
				// This should be changed once support for the single type in the FMath functions is added. 
				const FVector Position = (FVector)InPosition;
				int32 BestBaseTriangleIdx = INDEX_NONE;
				FVector::FReal BestDistanceSq = TNumericLimits<FVector::FReal>::Max();
				
				const int32 NumIndices = Mesh.Indices.Num();
				check(NumIndices % 3 == 0);

				for (int32 I = 0; I < NumIndices; I += 3)
				{
					const FVector& A = (FVector)Mesh.Vertices[Mesh.Indices[I + 0]];
					const FVector& B = (FVector)Mesh.Vertices[Mesh.Indices[I + 1]];
					const FVector& C = (FVector)Mesh.Vertices[Mesh.Indices[I + 2]];

					FVector ClosestTrianglePoint = FMath::ClosestPointOnTriangleToPoint(Position, (FVector)A, (FVector)B, (FVector)C);

					const FVector::FReal CurrentDistSq = (ClosestTrianglePoint - Position).SizeSquared();
					if (CurrentDistSq < BestDistanceSq)
					{
						BestDistanceSq = CurrentDistSq;
						BestBaseTriangleIdx = I;
					}
				}

				check(BestBaseTriangleIdx >= 0);

				auto ComputeBaryCoordsAndDist = [](const FVector3f& A, const FVector3f& B, const FVector3f& C, const FVector3f& P) -> FVector4f
				{
					FPlane4f TrianglePlane(A, B, C);

					const FVector3f PointOnTriPlane = FVector3f::PointPlaneProject(P, TrianglePlane);
					const FVector3f BaryCoords = (FVector3f)FMath::ComputeBaryCentric2D((FVector)PointOnTriPlane, (FVector)A, (FVector)B, (FVector)C);

					return FVector4f(BaryCoords, TrianglePlane.PlaneDot((FVector3f)P));
				};

				const FVector3f& A = Mesh.Vertices[Mesh.Indices[BestBaseTriangleIdx + 0]];
				const FVector3f& B = Mesh.Vertices[Mesh.Indices[BestBaseTriangleIdx + 1]];
				const FVector3f& C = Mesh.Vertices[Mesh.Indices[BestBaseTriangleIdx + 2]];

				Out.PositionBaryCoordsAndDist = ComputeBaryCoordsAndDist(A, B, C, (FVector3f)Position );
				Out.NormalBaryCoordsAndDist = ComputeBaryCoordsAndDist(A, B, C, (FVector3f)Position + Normal );
				Out.TangentBaryCoordsAndDist = ComputeBaryCoordsAndDist(A, B, C, (FVector3f)Position + Tangent );
				Out.SourceMeshVertIndices[0] = (uint16)Mesh.Indices[BestBaseTriangleIdx + 0];
				Out.SourceMeshVertIndices[1] = (uint16)Mesh.Indices[BestBaseTriangleIdx + 1]; 
				Out.SourceMeshVertIndices[2] = (uint16)Mesh.Indices[BestBaseTriangleIdx + 2];
			};
		
			auto RecreateTransitionData = [&PhysicsSectionLodsIndicesRemaps, &RebindVertex]( 
				const FMeshPhysicsDesc& ToMesh, const FMeshPhysicsDesc& FromMesh, const TArray<int32>& IndexMap, TArray<FMeshToMeshVertData>& InOutTransitionData )
			{
				if (!IndexMap.Num())
				{
					return;
				}

				if (!InOutTransitionData.Num())
				{
					return;
				}

				const int32 TransitionDataNum = InOutTransitionData.Num();
				
				for (int32 I = 0; I < TransitionDataNum; ++I)
				{
					FMeshToMeshVertData& VertData = InOutTransitionData[I];
					uint16* Indices = VertData.SourceMeshVertIndices;

					// If any original indices are missing but the vertex is still alive rebind the vertex.
					// In general, the number of rebinds should be small.

					// Currently, if any index is missing we rebind to the closest triangle but it could be nice to use the remaining indices, 
					// if any, to find the most appropriate triangle to bind to. 
					const bool bNeedsRebind = (Indices[0] == 0xFFFF) | (Indices[1] == 0xFFFF) | (Indices[2] == 0xFFFF);

					if (bNeedsRebind)
					{
						RebindVertex( ToMesh, FromMesh.Vertices[I], FromMesh.Normals[I], VertData );
					}
				}
			};

			const FMeshPhysicsDesc CurrentPhysicsMesh {
				NewClothingAssetData.LodData[SectionWithCloth.ClothAssetLodIndex].PhysicalMeshData.Vertices,
				NewClothingAssetData.LodData[SectionWithCloth.ClothAssetLodIndex].PhysicalMeshData.Normals,
				NewClothingAssetData.LodData[SectionWithCloth.ClothAssetLodIndex].PhysicalMeshData.Indices };

			const TArray<TArray<int32>>& SectionIndexRemaps = PhysicsSectionLodsIndicesRemaps[SectionWithCloth.ClothAssetIndex];
			
			if (SectionWithCloth.ClothAssetLodIndex < SectionIndexRemaps.Num() - 1)
			{
				const TArray<int32>& IndexMap = SectionIndexRemaps[SectionWithCloth.ClothAssetLodIndex + 1];
				
				const FMeshPhysicsDesc TransitionDownTarget {  
					NewClothingAssetData.LodData[SectionWithCloth.ClothAssetLodIndex + 1].PhysicalMeshData.Vertices,
					NewClothingAssetData.LodData[SectionWithCloth.ClothAssetLodIndex + 1].PhysicalMeshData.Normals,
					NewClothingAssetData.LodData[SectionWithCloth.ClothAssetLodIndex + 1].PhysicalMeshData.Indices };
					
				RecreateTransitionData( TransitionDownTarget, CurrentPhysicsMesh, IndexMap, NewLodData.TransitionDownSkinData );
			}
			
			if (SectionWithCloth.ClothAssetLodIndex > 0)
			{
				const TArray<int32>& IndexMap = SectionIndexRemaps[SectionWithCloth.ClothAssetLodIndex - 1];
				
				FMeshPhysicsDesc TransitionUpTarget{  
					NewClothingAssetData.LodData[SectionWithCloth.ClothAssetLodIndex - 1].PhysicalMeshData.Vertices,
					NewClothingAssetData.LodData[SectionWithCloth.ClothAssetLodIndex - 1].PhysicalMeshData.Normals,
					NewClothingAssetData.LodData[SectionWithCloth.ClothAssetLodIndex - 1].PhysicalMeshData.Indices };
		
				RecreateTransitionData( TransitionUpTarget, CurrentPhysicsMesh, IndexMap, NewLodData.TransitionUpSkinData );
			}
		}
	}

	// From here up, could be moved to an async task similar to what is done with the other prepare tasks.  
 
	// Create Clothing Assets.

	// Based on FSkeletalMeshLODModel::GetClothMappingData().
	TArray<TArray<FMeshToMeshVertData>> LodMappingData;
	LodMappingData.SetNum(LODCount);

	TArray<TArray<FClothBufferIndexMapping>> LodClothingIndexMapping;
	LodClothingIndexMapping.SetNum(LODCount);
	{
		int32 NumSectionsWithClothProcessed = 0;

		for (int32 LODIndex = OperationData->GetMinLOD(); LODIndex < OperationData->NumLODsAvailable; ++LODIndex)
		{
			const FInstanceUpdateData::FLOD& LOD = OperationData->InstanceUpdateData.LODs[LODIndex];
			if (ComponentIndex >= LOD.ComponentCount)
			{
				continue;
			}

			if (mu::MeshPtrConst MutableMesh = OperationData->InstanceUpdateData.Components[LOD.FirstComponent+ComponentIndex].Mesh)
			{
				TArray<FMeshToMeshVertData>& MappingData = LodMappingData[LODIndex];
				TArray<FClothBufferIndexMapping>& ClothingIndexMapping = LodClothingIndexMapping[LODIndex];
				ClothingIndexMapping.Reserve(32);

				const int32 SurfaceCount = MutableMesh->GetSurfaceCount();
				for (int32 Section = 0; Section < SurfaceCount; ++Section)
				{
					// Check that is a valid surface.
					int32 FirstVertex, VerticesCount, FirstIndex, IndicesCount;
					MutableMesh->GetSurface(Section, &FirstVertex, &VerticesCount, &FirstIndex, &IndicesCount, nullptr, nullptr, nullptr);

					if (VerticesCount == 0 || IndicesCount == 0)
					{
						continue;
					}					
			
					// An entry is added for all sections.	
					FClothBufferIndexMapping& ClothBufferIndexMapping = ClothingIndexMapping.AddZeroed_GetRef();

					if (NumSectionsWithClothProcessed < SectionsWithCloth.Num())
					{
						const FSectionWithClothData& SectionWithCloth = SectionsWithCloth[NumSectionsWithClothProcessed];
						// Section with cloth are sorted by {LOD, Section}
						if (SectionWithCloth.Lod == LODIndex && SectionWithCloth.Section == Section)
						{	
							ClothBufferIndexMapping.BaseVertexIndex = SectionWithCloth.BaseVertex;
							ClothBufferIndexMapping.MappingOffset = (uint32)MappingData.Num();
							ClothBufferIndexMapping.LODBiasStride = (uint32)SectionWithCloth.MappingData.Num();
						
							MappingData += SectionWithCloth.MappingData; 

							++NumSectionsWithClothProcessed;
						}
					}
				}
			}
		}
	}

	FSkeletalMeshRenderData* RenderResource = SkeletalMesh->GetResourceForRendering();
	{
		MUTABLE_CPUPROFILER_SCOPE(InitClothRenderData)
		// Based on FSkeletalMeshLODModel::GetClothMappingData().
		for (int32 LODIndex = OperationData->GetMinLOD(); LODIndex < OperationData->NumLODsAvailable; ++LODIndex)
		{
			FSkeletalMeshLODRenderData& LODModel = RenderResource->LODRenderData[LODIndex];
	
			if (LodMappingData[LODIndex].Num() > 0)
			{
				LODModel.ClothVertexBuffer.Init(LodMappingData[LODIndex], LodClothingIndexMapping[LODIndex]);
			}
		}
	}

	TArray<UCustomizableObjectClothingAsset*> NewClothingAssets;
	NewClothingAssets.Init(nullptr, ContributingClothingAssetsData.Num());

	{ 
		MUTABLE_CPUPROFILER_SCOPE(CreateClothingAssets)

		auto CreateNewClothConfigFromData = [](UObject* Outer, const FCustomizableObjectClothConfigData& ConfigData) -> UClothConfigCommon* 
		{
			UClass* ClothConfigClass = FindObject<UClass>(nullptr, *ConfigData.ClassPath);
			if (ClothConfigClass)
			{
				UClothConfigCommon* ClothConfig = NewObject<UClothConfigCommon>(Outer, ClothConfigClass);
				if (ClothConfig)
				{
					FMemoryReaderView MemoryReader(ConfigData.ConfigBytes);
					ClothConfig->Serialize(MemoryReader);

					return ClothConfig;
				}
			}

			return nullptr;
		};

		TArray<TTuple<FName, UClothConfigCommon*>> SharedConfigs;
		SharedConfigs.Reserve(ClothSharedConfigsData.Num());
 
		for (const FCustomizableObjectClothConfigData& ConfigData : ClothSharedConfigsData)
		{
			UClothConfigCommon* ClothConfig = CreateNewClothConfigFromData(SkeletalMesh, ConfigData);
			if (ClothConfig)
			{
				SharedConfigs.Emplace(ConfigData.ConfigName, ClothConfig);
			}
		}

		check(NewClothingAssets.Num() == ClothingPhysicsAssets.Num());
		for (int32 I = 0; I < NewClothingAssetsData.Num(); ++I)
		{
			// skip assets not set.
			if (!NewClothingAssetsData[I].LodData.Num())
			{
				continue;
			}
	
			FName UniqueClothingAssetName = FName(FString::Printf(TEXT("%s_%d"), *NewClothingAssetsData[I].Name.ToString(), I));
			NewClothingAssets[I] = NewObject<UCustomizableObjectClothingAsset>(SkeletalMesh, UniqueClothingAssetName);
			
			// The data can be moved to the actual asset since it will not be used anymore.
			NewClothingAssets[I]->LodMap = MoveTemp(NewClothingAssetsData[I].LodMap);
			NewClothingAssets[I]->LodData = MoveTemp(NewClothingAssetsData[I].LodData);
			NewClothingAssets[I]->UsedBoneIndices = MoveTemp(NewClothingAssetsData[I].UsedBoneIndices);
			NewClothingAssets[I]->UsedBoneNames = MoveTemp(NewClothingAssetsData[I].UsedBoneNames);
			NewClothingAssets[I]->ReferenceBoneIndex = NewClothingAssetsData[I].ReferenceBoneIndex;
			NewClothingAssets[I]->AssignNewGuid();
			NewClothingAssets[I]->RefreshBoneMapping(SkeletalMesh);
			NewClothingAssets[I]->CalculateReferenceBoneIndex();	
			NewClothingAssets[I]->PhysicsAsset = ClothingPhysicsAssets[I];

			for (const FCustomizableObjectClothConfigData& ConfigData : ContributingClothingAssetsData[I].ConfigsData)
			{
				UClothConfigCommon* ClothConfig = CreateNewClothConfigFromData(NewClothingAssets[I], ConfigData);
				if (ClothConfig)
				{
					NewClothingAssets[I]->ClothConfigs.Add(ConfigData.ConfigName, ClothConfig);
				}
			}

			for (const TTuple<FName, UClothConfigCommon*>& SharedConfig : SharedConfigs)
			{
				NewClothingAssets[I]->ClothConfigs.Add(SharedConfig);
			}

			SkeletalMesh->GetMeshClothingAssets().AddUnique(NewClothingAssets[I]);
		}	
	}

	for ( FSectionWithClothData& SectionWithCloth : SectionsWithCloth )
	{
		FSkeletalMeshLODRenderData& LODModel = RenderResource->LODRenderData[SectionWithCloth.Lod];
		FSkelMeshRenderSection& SectionData = LODModel.RenderSections[SectionWithCloth.Section];
        
		UClothingAssetCommon* NewClothingAsset = NewClothingAssets[SectionWithCloth.ClothAssetIndex];
		if (!NewClothingAsset)
		{
			continue;
		}
       
		SectionData.ClothMappingDataLODs.AddDefaulted(1);
		SectionData.ClothMappingDataLODs[0] = MoveTemp(SectionWithCloth.MappingData);
		
		SectionData.CorrespondClothAssetIndex = SkeletalMesh->GetClothingAssetIndex(NewClothingAsset);
		SectionData.ClothingData.AssetGuid = NewClothingAsset->GetAssetGuid();
		SectionData.ClothingData.AssetLodIndex = SectionWithCloth.ClothAssetLodIndex;
	}

	SkeletalMesh->SetHasActiveClothingAssets( static_cast<bool>( SectionsWithCloth.Num() ) );
}


bool UCustomizableInstancePrivate::BuildOrCopyRenderData(const TSharedRef<FUpdateContextPrivate>& OperationData, USkeletalMesh* SkeletalMesh, const USkeletalMesh* LastUpdateSkeletalMesh, UCustomizableObjectInstance* Public, int32 ComponentIndex)
{
	MUTABLE_CPUPROFILER_SCOPE(UCustomizableInstancePrivate::BuildOrCopyRenderData);

	FSkeletalMeshRenderData* RenderData = SkeletalMesh->GetResourceForRendering();
	check(RenderData);

	UCustomizableObject* CustomizableObject = Public->GetCustomizableObject();
	check(CustomizableObject);
	const FModelResources& ModelResources = CustomizableObject->GetPrivate()->GetModelResources();

	const int32 FirstGeneratedLOD = FMath::Max((int32)OperationData->GetRequestedLODs()[ComponentIndex], OperationData->GetMinLOD());
	for (int32 LODIndex = FirstGeneratedLOD; LODIndex < OperationData->NumLODsAvailable; ++LODIndex)
	{
		MUTABLE_CPUPROFILER_SCOPE(BuildRenderData);

		const FInstanceUpdateData::FLOD& LOD = OperationData->InstanceUpdateData.LODs[LODIndex];

		if (ComponentIndex >= LOD.ComponentCount)
		{
			// Interrupt the generation if the LOD is empty and it should have been generated.
			UE_LOG(LogMutable, Warning, TEXT("Building instance: generated mesh [%s] has LOD [%d] with no component.")
				, *SkeletalMesh->GetName()
				, LODIndex);

			// End with failure
			return false;
		}

		FInstanceUpdateData::FComponent& Component = OperationData->InstanceUpdateData.Components[LOD.FirstComponent + ComponentIndex];

		// There could be components without a mesh in LODs
		if (!Component.bGenerated || !Component.Mesh || Component.SurfaceCount == 0)
		{
			UE_LOG(LogMutable, Warning, TEXT("Building instance: generated mesh [%s] has LOD [%d] of component [%d] with no mesh.")
				, *SkeletalMesh->GetName()
				, LODIndex
				, Component.Id);

			// End with failure
			return false;
		}

		TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*FString::Printf(TEXT("BuildRenderData: Component %d, LOD %d"), ComponentIndex, LODIndex));

		SetLastMeshId(Component.Id, LODIndex, Component.MeshID);

		FSkeletalMeshLODRenderData& LODResource = RenderData->LODRenderData[LODIndex];

		// Set active and required bones
		LODResource.ActiveBoneIndices.Append(Component.ActiveBones);
		LODResource.RequiredBones.Append(Component.ActiveBones);

		// Set RenderSections
		UnrealConversionUtils::SetupRenderSections(
			LODResource,
			Component.Mesh,
			OperationData->InstanceUpdateData.BoneMaps,
			Component.FirstBoneMap);

		if (LODResource.bStreamedDataInlined) // Non-streamable LOD
		{
			// Copy Vertices
			UnrealConversionUtils::CopyMutableVertexBuffers(
				LODResource,
				Component.Mesh,
				SkeletalMesh->GetLODInfo(LODIndex)->bAllowCPUAccess);

			// Copy indices.
			if (!UnrealConversionUtils::CopyMutableIndexBuffers(LODResource, Component.Mesh))
			{
				// End with failure
				return false;
			}

			// Copy SkinWeightProfiles
			if (!ModelResources.SkinWeightProfilesInfo.IsEmpty())
			{
				bool bHasSkinWeightProfiles = false;

				const mu::FMeshBufferSet& MutableMeshVertexBuffers = Component.Mesh->GetVertexBuffers();

				const int32 SkinWeightProfilesCount = ModelResources.SkinWeightProfilesInfo.Num();
				for (int32 ProfileIndex = 0; ProfileIndex < SkinWeightProfilesCount; ++ProfileIndex)
				{
					const int32 ProfileSemanticsIndex = ProfileIndex + 10;
					int32 BoneIndicesBufferIndex, BoneIndicesBufferChannelIndex;
					MutableMeshVertexBuffers.FindChannel(mu::MBS_BONEINDICES, ProfileSemanticsIndex, &BoneIndicesBufferIndex, &BoneIndicesBufferChannelIndex);

					int32 BoneWeightsBufferIndex, BoneWeightsBufferChannelIndex;
					MutableMeshVertexBuffers.FindChannel(mu::MBS_BONEWEIGHTS, ProfileSemanticsIndex, &BoneWeightsBufferIndex, &BoneWeightsBufferChannelIndex);

					if (BoneIndicesBufferIndex < 0 || BoneIndicesBufferIndex != BoneWeightsBufferIndex)
					{
						continue;
					}

					if (!bHasSkinWeightProfiles)
					{
						LODResource.SkinWeightProfilesData.Init(&LODResource.SkinWeightVertexBuffer);
						bHasSkinWeightProfiles = true;
					}

					const FMutableSkinWeightProfileInfo& Profile = ModelResources.SkinWeightProfilesInfo[ProfileIndex];

					const FSkinWeightProfileInfo* ExistingProfile = SkeletalMesh->GetSkinWeightProfiles().FindByPredicate(
						[&Profile](const FSkinWeightProfileInfo& P) { return P.Name == Profile.Name; });

					if (!ExistingProfile)
					{
						SkeletalMesh->AddSkinWeightProfile({ Profile.Name, Profile.DefaultProfile, Profile.DefaultProfileFromLODIndex });
					}

					UnrealConversionUtils::CopyMutableSkinWeightProfilesBuffers(
						LODResource,
						Profile.Name,
						MutableMeshVertexBuffers,
						BoneIndicesBufferIndex);
				}
			}
		}
		else // Streamable LOD. 
		{
			// Init VertexBuffers for streaming
			UnrealConversionUtils::InitVertexBuffersWithDummyData(
				LODResource,
				Component.Mesh,
				SkeletalMesh->GetLODInfo(LODIndex)->bAllowCPUAccess);

			// Init IndexBuffers for streaming
			UnrealConversionUtils::InitIndexBuffersWithDummyData(LODResource, Component.Mesh);

			// SkinWeightProfilesInfo Not supported yet
		}

		if (LODResource.StaticVertexBuffers.ColorVertexBuffer.GetNumVertices())
		{
			SkeletalMesh->SetHasVertexColors(true);
		}

		if (LODResource.DoesVertexBufferUse16BitBoneIndex() && !UCustomizableObjectSystem::GetInstance()->IsSupport16BitBoneIndexEnabled())
		{
			OperationData->UpdateResult = EUpdateResult::Error16BitBoneIndex;

			const FString Msg = FString::Printf(TEXT("Customizable Object [%s] requires of Skinning - 'Support 16 Bit Bone Index' to be enabled. Please, update the Project Settings."),
				*CustomizableObject->GetName());
			UE_LOG(LogMutable, Error, TEXT("%s"), *Msg);

#if WITH_EDITOR
			FNotificationInfo Info(FText::FromString(Msg));
			Info.bFireAndForget = true;
			Info.FadeOutDuration = 1.0f;
			Info.ExpireDuration = 10.0f;
			FSlateNotificationManager::Get().AddNotification(Info);
#endif
		}
	}

	// Copy LODRenderData from the FirstGeneratedLOD to the LODs below
	for (int32 LODIndex = OperationData->FirstLODAvailable; LODIndex < FirstGeneratedLOD; ++LODIndex)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*FString::Printf(TEXT("CopyRenderData: From LOD %d to LOD %d"), FirstGeneratedLOD, LODIndex));

		// Render Data will be reused from the previously generated component
		FSkeletalMeshLODRenderData& SourceLODResource = RenderData->LODRenderData[FirstGeneratedLOD];
		FSkeletalMeshLODRenderData& LODResource = RenderData->LODRenderData[LODIndex];

		UnrealConversionUtils::CopySkeletalMeshLODRenderData(
			LODResource,
			SourceLODResource,
			*SkeletalMesh,
			SkeletalMesh->GetLODInfo(LODIndex)->bAllowCPUAccess
		);
	}

	return true;
}

static bool bEnableHighPriorityLoading = true;
FAutoConsoleVariableRef CVarMutableHighPriorityLoading(
	TEXT("Mutable.EnableLoadingAssetsWithHighPriority"),
	bEnableHighPriorityLoading,
	TEXT("If enabled, the request to load additional assets will have high priority."));

UE::Tasks::FTask UCustomizableInstancePrivate::LoadAdditionalAssetsAndDataAsync(
		const TSharedRef<FUpdateContextPrivate>& OperationData, FStreamableManager& StreamableManager)
{
	MUTABLE_CPUPROFILER_SCOPE(UCustomizableInstancePrivate::LoadAdditionalAssetsAndDataAsync);

	UCustomizableObject* CustomizableObject = GetPublic()->GetCustomizableObject();

	const FModelResources& ModelResources = CustomizableObject->GetPrivate()->GetModelResources();

	TArray<FSoftObjectPath> AssetsToStream;
	TArray<uint32> RealTimeMorphStreamableBlocksToStream;

	TArray<FInstanceUpdateData::FLOD>& LODs = OperationData->InstanceUpdateData.LODs;
	TArray<FInstanceUpdateData::FComponent>& Components = OperationData->InstanceUpdateData.Components;

	ObjectToInstanceIndexMap.Empty();
	ReferencedMaterials.Empty();

	const int32 NumClothingAssets = CustomizableObject->ContributingClothingAssetsData.Num();
	ClothingPhysicsAssets.Reset(NumClothingAssets);
	ClothingPhysicsAssets.SetNum(NumClothingAssets);

	GatheredAnimBPs.Empty();
	AnimBPGameplayTags.Reset();
	AnimBpPhysicsAssets.Reset();
	
	for (const FInstanceUpdateData::FSurface& Surface : OperationData->InstanceUpdateData.Surfaces)
	{
		const uint32 MaterialIndex = Surface.MaterialIndex;
		if (ObjectToInstanceIndexMap.Contains(MaterialIndex))
		{
			continue;
		}

		TSoftObjectPtr<UMaterialInterface> AssetPtr = ModelResources.Materials.IsValidIndex(MaterialIndex) ? ModelResources.Materials[MaterialIndex] : nullptr;
		UMaterialInterface* LoadedMaterial = AssetPtr.Get();

		const int32 ReferencedMaterialsIndex = ReferencedMaterials.Add(LoadedMaterial);
		ObjectToInstanceIndexMap.Add(MaterialIndex, ReferencedMaterialsIndex);

		if (!LoadedMaterial && !AssetPtr.IsNull())
		{
			AssetsToStream.Add(AssetPtr.ToSoftObjectPath());
		}
	}


	// Load Skeletons required by the SubMeshes of the newly generated Mesh, will be merged later
	for (FInstanceUpdateData::FSkeletonData& SkeletonData : OperationData->InstanceUpdateData.Skeletons)
	{
		FCustomizableInstanceComponentData* ComponentData = GetComponentData(SkeletonData.ComponentIndex);
		if (!ComponentData)
		{
			check(false);
			continue;
		}

		// Reuse merged Skeleton if cached
		ComponentData->Skeletons.Skeleton = CustomizableObject->GetPrivate()->SkeletonCache.Get(SkeletonData.SkeletonIds);
		if (ComponentData->Skeletons.Skeleton)
		{
			ComponentData->Skeletons.SkeletonIds.Empty();
			ComponentData->Skeletons.SkeletonsToMerge.Empty();
			continue;
		}

		// Add Skeletons to merge
		for (const uint32 SkeletonId : SkeletonData.SkeletonIds)
		{
			TSoftObjectPtr<USkeleton> AssetPtr = ModelResources.Skeletons.IsValidIndex(SkeletonId) ? ModelResources.Skeletons[SkeletonId] : nullptr;
			if (AssetPtr.IsNull())
			{
				continue;
			}

			// Add referenced skeletons to the assets to stream
			ComponentData->Skeletons.SkeletonIds.Add(SkeletonId);

			if (USkeleton* Skeleton = AssetPtr.Get())
			{
				ComponentData->Skeletons.SkeletonsToMerge.Add(Skeleton);
			}
			else
			{
				AssetsToStream.Add(AssetPtr.ToSoftObjectPath());
			}
		}
	}


	// Load assets coming from SubMeshes of the newly generated Mesh
	if (OperationData->InstanceUpdateData.LODs.Num())
	{
		const FInstanceUpdateData::FLOD& LOD = OperationData->InstanceUpdateData.LODs[OperationData->GetMinLOD()];
		for (int32 ComponentIndex = 0; ComponentIndex < LOD.ComponentCount; ++ComponentIndex)
		{
			const FInstanceUpdateData::FComponent& Component = Components[LOD.FirstComponent + ComponentIndex];
			mu::MeshPtrConst MutableMesh = Component.Mesh;

			if (!MutableMesh)
			{
				continue;
			}

			FCustomizableInstanceComponentData* ComponentData = GetComponentData(Component.Id);

			const TArray<uint32>& StreamedResources = MutableMesh->GetStreamedResources();

			for (uint32 ResourceId : StreamedResources)
			{
				const TArray<FCustomizableObjectStreamedResourceData>& StreamedResourcesData = CustomizableObject->GetPrivate()->GetStreamedResourceData();
				FCustomizableObjectStreameableResourceId TypedResourceId = BitCast<FCustomizableObjectStreameableResourceId>(ResourceId);	
	
				if (TypedResourceId.Type == (uint8)FCustomizableObjectStreameableResourceId::EType::AssetUserData)
				{
					const uint32 ResourceIndex = TypedResourceId.Id;
					if (!StreamedResourcesData.IsValidIndex(ResourceIndex))
					{
						UE_LOG(LogMutable, Error, TEXT("Invalid streamed resource index. Max Index [%d]. Resource Index [%d]."), StreamedResourcesData.Num(), ResourceIndex);
						continue; 
					}

					const FCustomizableObjectStreamedResourceData& StreamedResource = StreamedResourcesData[ResourceIndex];
					if (!StreamedResource.IsLoaded())
					{
						AssetsToStream.AddUnique(StreamedResource.GetPath().ToSoftObjectPath());
					}

					ComponentData->StreamedResourceIndex.Add(ResourceIndex);
				}
				else if (TypedResourceId.Type == (uint8)FCustomizableObjectStreameableResourceId::EType::RealTimeMorphTarget)
				{
					const TArray<FMutableStreamableBlock>& MorphsStremeableBlocks = ModelResources.RealTimeMorphStreamableBlocks;
					if (MorphsStremeableBlocks.IsValidIndex(TypedResourceId.Id))
					{
						RealTimeMorphStreamableBlocksToStream.AddUnique(TypedResourceId.Id);
					}
					else
					{
						UE_LOG(LogMutable, Error, TEXT("Invalid streamed real time morph target data block [%d] found."), TypedResourceId.Id);
					}
				}
				else
				{
					check(false);
				}
			}

			const bool bReplacePhysicsAssets = HasCOInstanceFlags(ReplacePhysicsAssets);

			for (int32 TagIndex = 0; TagIndex < MutableMesh->GetTagCount(); ++TagIndex)
			{
				FString Tag = MutableMesh->GetTag(TagIndex);
				if (Tag.RemoveFromStart("__PA:"))
				{
					const int32 AssetIndex = FCString::Atoi(*Tag);
					const TSoftObjectPtr<UPhysicsAsset>& PhysicsAsset = ModelResources.PhysicsAssets.IsValidIndex(AssetIndex) ? ModelResources.PhysicsAssets[AssetIndex] : nullptr;

					if (!PhysicsAsset.IsNull())
					{
						if (PhysicsAsset.Get())
						{
							ComponentData->PhysicsAssets.PhysicsAssetsToMerge.Add(PhysicsAsset.Get());
						}
						else
						{
							ComponentData->PhysicsAssets.PhysicsAssetToLoad.Add(AssetIndex);
							AssetsToStream.Add(PhysicsAsset.ToSoftObjectPath());
						}
					}
				}
				else if (Tag.RemoveFromStart("__ClothPA:"))
				{
					FString AssetIndexString, PhysicsAssetIndexString;

					if (Tag.Split(TEXT("_"), &AssetIndexString, &PhysicsAssetIndexString))
					{
						if (AssetIndexString.IsNumeric() && PhysicsAssetIndexString.IsNumeric())
						{
							const int32 AssetIndex = FCString::Atoi(*AssetIndexString);
							const int32 PhysicsAssetIndex = FCString::Atoi(*PhysicsAssetIndexString);

							const TSoftObjectPtr<UPhysicsAsset>& PhysicsAsset = ModelResources.PhysicsAssets.IsValidIndex(PhysicsAssetIndex) 
									? ModelResources.PhysicsAssets[PhysicsAssetIndex] 
									: nullptr;

							// The entry should always be in the map
							if (!PhysicsAsset.IsNull())
							{
								if (PhysicsAsset.Get())
								{
									if (ClothingPhysicsAssets.IsValidIndex(AssetIndex))
									{
										ClothingPhysicsAssets[AssetIndex] = PhysicsAsset.Get();
									}
								}
								else
								{
									ComponentData->ClothingPhysicsAssetsToStream.Emplace(AssetIndex, PhysicsAssetIndex);
									AssetsToStream.Add(PhysicsAsset.ToSoftObjectPath());
								}
							}
						}
					}
				}
				if (Tag.RemoveFromStart("__AnimBP:"))
				{
					FString SlotIndexString, AnimBpIndexString;

					if (Tag.Split(TEXT("_Slot_"), &SlotIndexString, &AnimBpIndexString))
					{
						if (SlotIndexString.IsEmpty() || AnimBpIndexString.IsEmpty())
						{
							continue;
						}

						const int32 AnimBpIndex = FCString::Atoi(*AnimBpIndexString);
						if (!ModelResources.AnimBPs.IsValidIndex(AnimBpIndex))
						{
							continue;
						}

						FName SlotIndex = *SlotIndexString;

						const TSoftClassPtr<UAnimInstance>& AnimBPAsset = ModelResources.AnimBPs[AnimBpIndex];

						if (!AnimBPAsset.IsNull())
						{
							if (!ComponentData->AnimSlotToBP.Contains(SlotIndex))
							{
								ComponentData->AnimSlotToBP.Add(SlotIndex, AnimBPAsset);

								if (AnimBPAsset.Get())
								{
									GatheredAnimBPs.Add(AnimBPAsset.Get());
								}
								else
								{
									AssetsToStream.Add(AnimBPAsset.ToSoftObjectPath());
								}
							}
							else
							{
								// Two submeshes should not have the same animation slot index
								OperationData->UpdateResult = EUpdateResult::Warning;

								FString WarningMessage = FString::Printf(TEXT("Two submeshes have the same anim slot index [%s] in a Mutable Instance."), *SlotIndex.ToString());
								UE_LOG(LogMutable, Warning, TEXT("%s"), *WarningMessage);
#if WITH_EDITOR
								FMessageLog MessageLog("Mutable");
								MessageLog.Notify(FText::FromString(WarningMessage), EMessageSeverity::Warning, true);
#endif
							}
						}
					}
				}
				else if (Tag.RemoveFromStart("__AnimBPTag:"))
				{
					AnimBPGameplayTags.AddTag(FGameplayTag::RequestGameplayTag(*Tag));
				}
#if WITH_EDITORONLY_DATA
				else if (Tag.RemoveFromStart("__MeshPath:"))
				{
					ComponentData->MeshPartPaths.Add(Tag);
				}
#endif
			}

			const int32 AdditionalPhysicsNum = MutableMesh->AdditionalPhysicsBodies.Num();
			for (int32 I = 0; I < AdditionalPhysicsNum; ++I)
			{
				const int32 ExternalId = MutableMesh->AdditionalPhysicsBodies[I]->CustomId;
				
				ComponentData->PhysicsAssets.AdditionalPhysicsAssetsToLoad.Add(ExternalId);
				AssetsToStream.Add(ModelResources.AnimBpOverridePhysiscAssetsInfo[ExternalId].SourceAsset.ToSoftObjectPath());
			}
		}
	}

	for (TSoftObjectPtr<UTexture>& TextureRef : PassThroughTexturesToLoad)
	{
		AssetsToStream.Add(TextureRef.ToSoftObjectPath());
	}

	TArray<UE::Tasks::FTaskEvent> StreamingCompletionEvents;
	if (AssetsToStream.Num() > 0)
	{	
		UE::Tasks::FTaskEvent AssetAsyncLoadCompletionEvent = StreamingCompletionEvents.Emplace_GetRef(TEXT("AssetAsyncLoadCompletionEvent"));

		StreamingHandle = StreamableManager.RequestAsyncLoad(
				AssetsToStream, 
				FStreamableDelegate::CreateUObject(this, &UCustomizableInstancePrivate::AdditionalAssetsAsyncLoaded, AssetAsyncLoadCompletionEvent),
				bEnableHighPriorityLoading ? FStreamableManager::AsyncLoadHighPriority : FStreamableManager::DefaultAsyncLoadPriority);
	}

	
	// File handles will end up owned by the gather task.
	TArray<TUniquePtr<IAsyncReadFileHandle>> OpenFileHandles;
	TArray<UE::Tasks::TTask<TUniquePtr<IAsyncReadRequest>>> ReadRequestTasks;
	
	bool bHasInvalidMesh = false;
	bool bUpdateMeshes = DoComponentsNeedUpdate(GetPublic(), OperationData, bHasInvalidMesh);

	if (RealTimeMorphStreamableBlocksToStream.Num() && bUpdateMeshes)
	{
#if WITH_EDITOR
		// On editor the data is always loaded, load directly form the ModelResources.
		MUTABLE_CPUPROFILER_SCOPE(RealTimeMorphStreamingEditor);
		for (uint32 BlockId : RealTimeMorphStreamableBlocksToStream)
		{	
			const FMutableStreamableBlock& Block = ModelResources.RealTimeMorphStreamableBlocks[BlockId]; 
			
			const TArray<FMorphTargetVertexData>& SourceData = ModelResources.EditorOnlyMorphTargetReconstructionData;
			TArray<FMorphTargetVertexData>& DestData = OperationData->InstanceUpdateData.MorphTargetsVertexData.Emplace(BlockId);
			
			const uint32 NumElems = Block.Size / sizeof(FMorphTargetVertexData);
			const uint32 OffsetInElems = Block.Offset / sizeof(FMorphTargetVertexData);
			DestData.SetNumUninitialized(NumElems);

			check(SourceData.Num()*sizeof(FMorphTargetVertexData) >= Block.Offset + Block.Size);
			FMemory::Memcpy(DestData.GetData(), SourceData.GetData() + OffsetInElems, Block.Size);
		}
#else	
		MUTABLE_CPUPROFILER_SCOPE(RealTimeMorphStreaming);
		struct FBlockReadInfo
		{
			uint64 Offset;
			IAsyncReadFileHandle* FileHandle;
			TArrayView<uint8> AllocatedMemoryView;
			uint32 FileId;
		};

		TArray<FBlockReadInfo> BlockReadInfos;
		BlockReadInfos.Reserve(16);
		
		const UCustomizableObjectBulk* BulkData = CustomizableObject->GetPrivate()->GetStreamableBulkData();
		UE_CLOG(!BulkData, LogMutable, Error, TEXT("BulkData object for CustomizableObject [%s] not found."), 
				*CustomizableObject->GetFName().ToString());
		check(BulkData);

		TArray<uint32> OpenFilesIds;
		const int32 NumMorphBlocks = RealTimeMorphStreamableBlocksToStream.Num();
		for (int32 I = 0; I < NumMorphBlocks; ++I)
		{
			MUTABLE_CPUPROFILER_SCOPE(RealTimeMorphStreamingRequest_Alloc);

			const int32 BlockId = RealTimeMorphStreamableBlocksToStream[I];
			const FMutableStreamableBlock& Block = ModelResources.RealTimeMorphStreamableBlocks[BlockId]; 
		
			TArray<FMorphTargetVertexData>& ReadDestData = OperationData->InstanceUpdateData.MorphTargetsVertexData.FindOrAdd(BlockId);

			// Only request blocks once.
			if (ReadDestData.Num())
			{
				continue;
			}

			check(Block.Size % sizeof(FMorphTargetVertexData) == 0);
			uint32 NumElems = Block.Size / sizeof(FMorphTargetVertexData);

			ReadDestData.SetNumUninitialized(NumElems);

			int32 FileHandleIndex = OpenFilesIds.Find(Block.FileId);
			if (FileHandleIndex == INDEX_NONE && BulkData)
			{
				TUniquePtr<IAsyncReadFileHandle> ReadFileHandle = BulkData->OpenFileAsyncRead(Block.FileId);

				OpenFileHandles.Emplace(MoveTemp(ReadFileHandle));
				FileHandleIndex = OpenFilesIds.Add(Block.FileId);
			}	

			BlockReadInfos.Emplace(FBlockReadInfo
			{ 
				Block.Offset,
				OpenFileHandles[FileHandleIndex].Get(), 
				MakeArrayView(reinterpret_cast<uint8*>(ReadDestData.GetData()), ReadDestData.Num()*sizeof(FMorphTargetVertexData)),
				Block.FileId
			});
		}

		for (const FBlockReadInfo& BlockReadInfo : BlockReadInfos)
		{
			if (!BlockReadInfo.FileHandle)
			{
				continue;
			}	
	
			ReadRequestTasks.Emplace(UE::Tasks::Launch(TEXT("CustomizableObjectInstanceReadRequestTask"),
			[
				OwnedOperationData = OperationData.ToSharedPtr(), // Keep a reference to make sure allocated memory is always alive.
				ReadDataReadyEvent = StreamingCompletionEvents.Emplace_GetRef(TEXT("AsyncReadDataReadyEvent")),
				Block              = BlockReadInfo,
				Priority 		   = bEnableHighPriorityLoading ? AIOP_High : AIOP_Normal
			]() -> TUniquePtr<IAsyncReadRequest>
			{
				MUTABLE_CPUPROFILER_SCOPE(CustomizableInstanceLoadBlocksAsyncRead_Request);
				FAsyncFileCallBack ReadRequestCallBack = 
						[OwnedOperationData, ReadDataReadyEvent, FileId = Block.FileId](bool bWasCancelled, IAsyncReadRequest*) mutable
						{
							if (bWasCancelled)
							{
								UE_LOG(LogMutable, Warning, TEXT("An AsyncReadRequest to file %08x was cancelled. The file may not exist."), FileId);
							}
							ReadDataReadyEvent.Trigger();
						}; 

				return TUniquePtr<IAsyncReadRequest>(Block.FileHandle->ReadRequest(
						Block.Offset, 
						(int64)Block.AllocatedMemoryView.Num(),
						Priority,
						&ReadRequestCallBack,
						Block.AllocatedMemoryView.GetData()));
			}, 
			bEnableHighPriorityLoading ? UE::Tasks::ETaskPriority::High : UE::Tasks::ETaskPriority::Normal));
		}	

#endif

	}

	if (AssetsToStream.Num() > 0 || OpenFileHandles.Num() > 0)
	{
		return UE::Tasks::Launch(TEXT("GatherStreamingRequestsCompletionTask"),
				[
					ReadRequestTasks = MoveTemp(ReadRequestTasks),
					OpenFileHandles  = MoveTemp(OpenFileHandles)
				]() mutable 
				{
					for (UE::Tasks::TTask<TUniquePtr<IAsyncReadRequest>>& ReadRequestTask : ReadRequestTasks)
					{
						// GetResult() may wait for the task to complete, this should not be a problem as this task
						// prerequisites guarantee ReadRequestTask has at least started execution.
						TUniquePtr<IAsyncReadRequest>& ReadRequest = ReadRequestTask.GetResult();
						if (ReadRequest)
						{
							ReadRequest->WaitCompletion();
						}
					}
					
					ReadRequestTasks.Empty();
					OpenFileHandles.Empty();
				},
				StreamingCompletionEvents,
				bEnableHighPriorityLoading ? UE::Tasks::ETaskPriority::High : UE::Tasks::ETaskPriority::Normal);
	}
	else
	{
		check(ReadRequestTasks.Num() == 0);
		return UE::Tasks::MakeCompletedTask<void>();
	}
}

void UCustomizableInstancePrivate::AdditionalAssetsAsyncLoaded(UE::Tasks::FTaskEvent CompletionEvent)
{
	// TODO: Do we need this separated?
	//check(IsInGameThread())
	AdditionalAssetsAsyncLoaded(GetPublic());
	CompletionEvent.Trigger(); // TODO: we know it is game thread?

	StreamingHandle = nullptr;
}


FCustomizableObjectInstanceDescriptor& UCustomizableInstancePrivate::GetDescriptor() const
{
	return GetPublic()->Descriptor;
}

const TArray<TObjectPtr<UMaterialInterface>>* UCustomizableObjectInstance::GetOverrideMaterials(int32 ComponentIndex) const
{
	FCustomizableInstanceComponentData* ComponentData = PrivateData->GetComponentData(ComponentIndex);
	return ComponentData ? &ComponentData->OverrideMaterials : nullptr;
}

void UCustomizableInstancePrivate::AdditionalAssetsAsyncLoaded(UCustomizableObjectInstance* Public)
{
	MUTABLE_CPUPROFILER_SCOPE(UCustomizableInstancePrivate::AdditionalAssetsAsyncLoaded);

	UCustomizableObject* CustomizableObject = Public->GetCustomizableObject();

	const FModelResources& ModelResources = CustomizableObject->GetPrivate()->GetModelResources();

	// Loaded Materials
	check(ObjectToInstanceIndexMap.Num() == ReferencedMaterials.Num());

	for (TPair<uint32, uint32> Pair : ObjectToInstanceIndexMap)
	{
		const TSoftObjectPtr<UMaterialInterface>& AssetPtr = ModelResources.Materials.IsValidIndex(Pair.Key) ? ModelResources.Materials[Pair.Key] : nullptr;
		ReferencedMaterials[Pair.Value] = AssetPtr.Get();

#if WITH_EDITOR
		if (!ReferencedMaterials[Pair.Value])
		{
			if (!AssetPtr.IsNull())
			{
				FString ErrorMsg = FString::Printf(TEXT("Mutable couldn't load the material [%s] and won't be rendered. If it has been deleted or renamed, please recompile all the mutable objects that use it."), *AssetPtr.GetAssetName());
				UE_LOG(LogMutable, Error, TEXT("%s"), *ErrorMsg);

				FMessageLog MessageLog("Mutable");
				MessageLog.Notify(FText::FromString(ErrorMsg), EMessageSeverity::Error, true);
			}
			else
			{
				ensure(false); // Couldn't load the material, and we don't know which material
			}
		}
#endif
	}

	
	TArray<FCustomizableObjectStreamedResourceData>& StreamedResources = CustomizableObject->GetPrivate()->GetStreamedResourceData();

	for (FCustomizableInstanceComponentData& ComponentData : ComponentsData)
	{
		for (int32 ResourceIndex : ComponentData.StreamedResourceIndex)
		{
			FCustomizableObjectStreamedResourceData& Resource = StreamedResources[ResourceIndex];
			if (!Resource.IsLoaded())
			{
				Resource.NotifyLoaded(Resource.GetPath().Get());
			}

			const FCustomizableObjectResourceData& ResourceData = Resource.GetLoadedData();
			switch (ResourceData.Type)
			{
				case ECOResourceDataType::AssetUserData:
				{
					const FCustomizableObjectAssetUserData* AUDResource = ResourceData.Data.GetPtr<FCustomizableObjectAssetUserData>();
#if WITH_EDITORONLY_DATA
					ComponentData.AssetUserDataArray.Add(AUDResource->AssetUserDataEditor);
#else
					ComponentData.AssetUserDataArray.Add(AUDResource->AssetUserData);
#endif
					break;
				}
				default:
					break;
			}
		}

		// Loaded Skeletons
		FReferencedSkeletons& Skeletons = ComponentData.Skeletons;
		for (int32 SkeletonIndex : Skeletons.SkeletonIds)
		{
			const TSoftObjectPtr<USkeleton>& AssetPtr = ModelResources.Skeletons.IsValidIndex(SkeletonIndex) ? ModelResources.Skeletons[SkeletonIndex] : nullptr;
			Skeletons.SkeletonsToMerge.AddUnique(AssetPtr.Get());
		}

		// Loaded PhysicsAssets
		FReferencedPhysicsAssets& PhysicsAssets = ComponentData.PhysicsAssets;
		for(const int32 PhysicsAssetIndex : PhysicsAssets.PhysicsAssetToLoad)
		{
			check(ModelResources.PhysicsAssets.IsValidIndex(PhysicsAssetIndex));
			const TSoftObjectPtr<UPhysicsAsset>& PhysicsAsset = ModelResources.PhysicsAssets[PhysicsAssetIndex];
			PhysicsAssets.PhysicsAssetsToMerge.Add(PhysicsAsset.Get());

#if WITH_EDITOR
			if (!PhysicsAsset.Get())
			{
				if (!PhysicsAsset.IsNull())
				{
					FString ErrorMsg = FString::Printf(TEXT("Mutable couldn't load the PhysicsAsset [%s] and won't be merged. If it has been deleted or renamed, please recompile all the mutable objects that use it."), *PhysicsAsset.GetAssetName());
					UE_LOG(LogMutable, Error, TEXT("%s"), *ErrorMsg);

					FMessageLog MessageLog("Mutable");
					MessageLog.Notify(FText::FromString(ErrorMsg), EMessageSeverity::Error, true);
				}
				else
				{
					ensure(false); // Couldn't load the PhysicsAsset, and we don't know which PhysicsAsset
				}
			}
#endif
		}
		PhysicsAssets.PhysicsAssetToLoad.Empty();
		
		// Loaded Clothing PhysicsAssets 
		for ( TPair<int32, int32>& AssetToStream : ComponentData.ClothingPhysicsAssetsToStream )
		{
			const int32 AssetIndex = AssetToStream.Key;

			if (ClothingPhysicsAssets.IsValidIndex(AssetIndex) && ModelResources.PhysicsAssets.IsValidIndex(AssetToStream.Value))
			{
				const TSoftObjectPtr<UPhysicsAsset>& PhysicsAssetPtr = ModelResources.PhysicsAssets[AssetToStream.Value];
				ClothingPhysicsAssets[AssetIndex] = PhysicsAssetPtr.Get();
			}
		}
		ComponentData.ClothingPhysicsAssetsToStream.Empty();

		// Loaded anim BPs
		for (TPair<FName, TSoftClassPtr<UAnimInstance>>& SlotAnimBP : ComponentData.AnimSlotToBP)
		{
			if (TSubclassOf<UAnimInstance> AnimBP = SlotAnimBP.Value.Get())
			{
				if (!GatheredAnimBPs.Contains(AnimBP))
				{
					GatheredAnimBPs.Add(AnimBP);
				}
			}
#if WITH_EDITOR
			else
			{
				FString ErrorMsg = FString::Printf(TEXT("Mutable couldn't load the AnimBlueprint [%s]. If it has been deleted or renamed, please recompile all the mutable objects that use it."), *SlotAnimBP.Value.GetAssetName());
				UE_LOG(LogMutable, Error, TEXT("%s"), *ErrorMsg);

				FMessageLog MessageLog("Mutable");
				MessageLog.Notify(FText::FromString(ErrorMsg), EMessageSeverity::Error, true);
			}
#endif
		}

		const int32 AdditionalPhysicsNum = ComponentData.PhysicsAssets.AdditionalPhysicsAssetsToLoad.Num();
		ComponentData.PhysicsAssets.AdditionalPhysicsAssets.Reserve(AdditionalPhysicsNum);
		for (int32 I = 0; I < AdditionalPhysicsNum; ++I)
		{
			// Make the loaded assets references strong.
			const int32 AnimBpPhysicsOverrideIndex = ComponentData.PhysicsAssets.AdditionalPhysicsAssetsToLoad[I];
			ComponentData.PhysicsAssets.AdditionalPhysicsAssets.Add( 
				ModelResources.AnimBpOverridePhysiscAssetsInfo[AnimBpPhysicsOverrideIndex].SourceAsset.Get());
		}
		ComponentData.PhysicsAssets.AdditionalPhysicsAssetsToLoad.Empty();
	}

	LoadedPassThroughTexturesPendingSetMaterial.Empty(PassThroughTexturesToLoad.Num());

	for (TSoftObjectPtr<UTexture>& TextureRef : PassThroughTexturesToLoad)
	{
		ensure(TextureRef.IsValid());
		LoadedPassThroughTexturesPendingSetMaterial.Add(TextureRef.Get());
	}

	PassThroughTexturesToLoad.Empty();

	// Only Unload in cooked builds. Unloading them when in the editor will trigger an assert. 
	if (FPlatformProperties::RequiresCookedData())
	{
		for (FCustomizableObjectStreamedResourceData& ResourceData : StreamedResources)
		{
			ResourceData.Unload();
		}
	}
}


void UpdateTextureRegionsMutable(UTexture2D* Texture, int32 MipIndex, uint32 NumMips, const FUpdateTextureRegion2D& Region, uint32 SrcPitch, 
								 const FByteBulkData* BulkData, TSharedRef<FTexturePlatformData, ESPMode::ThreadSafe>& PlatformData)
{
	if (Texture->GetResource())
	{
		struct FUpdateTextureRegionsData
		{
			FTexture2DResource* Texture2DResource;
			int32 MipIndex;
			FUpdateTextureRegion2D Region;
			uint32 SrcPitch;
			uint32 NumMips;
			
			// The Platform Data mips will be automatically deleted when all FUpdateTextureRegionsData that reference it are deleted
			// in the render thread after being used to update the texture
			TSharedRef<FTexturePlatformData, ESPMode::ThreadSafe> PlatformData;

			FUpdateTextureRegionsData(TSharedRef<FTexturePlatformData, ESPMode::ThreadSafe>& InPlatformData) : PlatformData(InPlatformData) {}
		};

		FUpdateTextureRegionsData* RegionData = new FUpdateTextureRegionsData(PlatformData);

		RegionData->Texture2DResource = (FTexture2DResource*)Texture->GetResource();
		RegionData->MipIndex = MipIndex;
		RegionData->Region = Region;
		RegionData->SrcPitch = SrcPitch;
		RegionData->NumMips = NumMips;

		ENQUEUE_RENDER_COMMAND(UpdateTextureRegionsMutable)(
			[RegionData, BulkData](FRHICommandList& CmdList)
			{
				check(int32(RegionData->NumMips) >= RegionData->Texture2DResource->GetCurrentMipCount());
				int32 MipDifference = RegionData->NumMips - RegionData->Texture2DResource->GetCurrentMipCount();
				check(MipDifference >= 0);
				int32 CurrentFirstMip = RegionData->Texture2DResource->GetCurrentFirstMip();
				uint8* SrcData = (uint8*)BulkData->LockReadOnly();

				//uint32 Size = RegionData->SrcPitch / (sizeof(uint8) * 4);
				//UE_LOG(LogMutable, Warning, TEXT("UpdateTextureRegionsMutable MipIndex = %d, FirstMip = %d, size = %d"),
				//	RegionData->MipIndex, CurrentFirstMip, Size);

				//checkf(Size <= RegionData->Texture2DResource->GetSizeX(),
				//	TEXT("UpdateTextureRegionsMutable incorrect size. %d, %d. NumMips=%d"), 
				//	Size, RegionData->Texture2DResource->GetSizeX(), RegionData->Texture2DResource->GetCurrentMipCount());

				if (RegionData->MipIndex >= CurrentFirstMip + MipDifference)
				{
					RHIUpdateTexture2D(
						RegionData->Texture2DResource->GetTexture2DRHI(),
						RegionData->MipIndex - CurrentFirstMip - MipDifference,
						RegionData->Region,
						RegionData->SrcPitch,
						SrcData);
				}

				BulkData->Unlock();
				delete RegionData; // This will implicitly delete the Platform Data if this is the last RegionData referencing it
			});
	}
}


void UCustomizableInstancePrivate::ReuseTexture(UTexture2D* Texture, TSharedRef<FTexturePlatformData, ESPMode::ThreadSafe>& PlatformData)
{
	uint32 NumMips = PlatformData->Mips.Num();

	for (uint32 i = 0; i < NumMips; i++)
	{
		FTexture2DMipMap& Mip = PlatformData->Mips[i];

		if (Mip.BulkData.GetElementCount() > 0)
		{
			FUpdateTextureRegion2D Region;

			Region.DestX = 0;
			Region.DestY = 0;
			Region.SrcX = 0;
			Region.SrcY = 0;
			Region.Width = Mip.SizeX;
			Region.Height = Mip.SizeY;

			check(int32(Region.Width) <= Texture->GetSizeX());
			check(int32(Region.Height) <= Texture->GetSizeY());

			UpdateTextureRegionsMutable(Texture, i, NumMips, Region, 
					                    Mip.SizeX * sizeof(uint8) * 4, &Mip.BulkData, PlatformData);
		}
	}
}


void UCustomizableInstancePrivate::BuildMaterials(const TSharedRef<FUpdateContextPrivate>& OperationData, UCustomizableObjectInstance* Public)
{
	MUTABLE_CPUPROFILER_SCOPE(UCustomizableInstancePrivate::BuildMaterials)

	UCustomizableObject* CustomizableObject = Public->GetCustomizableObject();

	const FModelResources& ModelResources = CustomizableObject->GetPrivate()->GetModelResources();

	TArray<FGeneratedTexture> NewGeneratedTextures;

	// Temp copy to allow reuse of MaterialInstances
	TArray<FGeneratedMaterial> OldGeneratedMaterials;
	Exchange(OldGeneratedMaterials, GeneratedMaterials);
	
	GeneratedMaterials.Reset();

	// Prepare the data to store in order to regenerate resources for this instance (usually texture mips).
	TSharedPtr<FMutableUpdateContext> UpdateContext = MakeShared<FMutableUpdateContext>(
		CustomizableObject->GetPathName(),
		Public->GetPathName(),
		UCustomizableObjectSystem::GetInstance()->GetPrivate()->MutableSystem,
		CustomizableObject->GetPrivate()->GetModel(),
		OperationData->Parameters,
	    OperationData->GetCapturedDescriptor().GetState());

	// Cache the descriptor as a string if we want to later report it using our benchmark utility. 
	if (CVarEnableBenchmark.GetValueOnAnyThread())
	{
		UpdateContext->CapturedDescriptor = OperationData->GetCapturedDescriptor().ToString();
	}
	
	const bool bReuseTextures = OperationData->bReuseInstanceTextures;

	TArray<bool> RecreateRenderStateOnComponent;
	RecreateRenderStateOnComponent.Init(false, OperationData->NumComponents);

	for (int32 ComponentIndex = 0; ComponentIndex < OperationData->NumComponents; ++ComponentIndex)
	{
		if (!SkeletalMeshes.IsValidIndex(ComponentIndex))
		{
			// This could happen if there are no meshes at all.
			continue;
		}

		USkeletalMesh* SkeletalMesh = SkeletalMeshes[ComponentIndex];

		if (!SkeletalMesh)
		{
			continue;
		}

		const bool bReuseMaterials = !OperationData->MeshChanged[ComponentIndex];

		// It is not safe to replace the materials of a SkeletalMesh whose resources are initialized. Use overrides instead.
		const bool bUseOverrideMaterialsOnly = OperationData->bUseMeshCache && SkeletalMesh->GetResourceForRendering()->IsInitialized();

		ComponentsData[ComponentIndex].OverrideMaterials.Reset();

		TArray<FSkeletalMaterial> Materials;

		// Maps serializations of FMutableMaterialPlaceholder to Created Dynamic Material instances, used to reuse materials across LODs
		TSet<FMutableMaterialPlaceholder> ReuseMaterialCache;

		// SurfaceId per MaterialSlotIndex
		TArray<int32> SurfaceIdToMaterialIndex;

		MUTABLE_CPUPROFILER_SCOPE(BuildMaterials_LODLoop);

		const int32 FirstGeneratedLOD = FMath::Max((int32)OperationData->GetRequestedLODs()[ComponentIndex], OperationData->GetMinLOD());
		for (int32 LODIndex = FirstGeneratedLOD; LODIndex < OperationData->NumLODsAvailable; LODIndex++)
		{
			const FInstanceUpdateData::FLOD& LOD = OperationData->InstanceUpdateData.LODs[LODIndex];
			const FInstanceUpdateData::FComponent& Component = OperationData->InstanceUpdateData.Components[LOD.FirstComponent + ComponentIndex];

			if (!Component.bGenerated)
			{
				continue;
			}

			if (SkeletalMesh->GetLODInfoArray().IsValidIndex(LODIndex))
			{
				SkeletalMesh->GetLODInfoArray()[LODIndex].LODMaterialMap.Reset();
			}

			const FMutableRefSkeletalMeshData& RefSkeletalMeshData = ModelResources.ReferenceSkeletalMeshesData[Component.Id];

			for (int32 SurfaceIndex = 0; SurfaceIndex < Component.SurfaceCount; ++SurfaceIndex)
			{
				const FInstanceUpdateData::FSurface& Surface = OperationData->InstanceUpdateData.Surfaces[Component.FirstSurface + SurfaceIndex];

				// Reuse MaterialSlot from the previous LOD.
				if (const int32 MaterialIndex = SurfaceIdToMaterialIndex.Find(Surface.SurfaceId); MaterialIndex != INDEX_NONE)
				{
					const int32 LODMaterialIndex = SkeletalMesh->GetLODInfoArray()[LODIndex].LODMaterialMap.Add(MaterialIndex);
					SkeletalMesh->GetResourceForRendering()->LODRenderData[LODIndex].RenderSections[SurfaceIndex].MaterialIndex = LODMaterialIndex;
					continue;
				}

				const uint32 ReferencedMaterialIndex = ObjectToInstanceIndexMap[Surface.MaterialIndex];
				UMaterialInterface* MaterialTemplate = ReferencedMaterials[ReferencedMaterialIndex];
				if (!MaterialTemplate)
				{
					// Missing MaterialTemplate. Use DefaultMaterial instead. 
					MaterialTemplate = UMaterial::GetDefaultMaterial(MD_Surface);
					check(MaterialTemplate);
					UE_LOG(LogMutable, Error, TEXT("Build Materials: Missing referenced template to use as parent material on CustomizableObject [%s]."), *CustomizableObject->GetName());
				}

				// This section will require a new slot
				SurfaceIdToMaterialIndex.Add(Surface.SurfaceId);

				// Add and set up the material data for this slot
				const int32 MaterialSlotIndex = Materials.Num();
				FSkeletalMaterial& MaterialSlot = Materials.AddDefaulted_GetRef();
				MaterialSlot.MaterialInterface = MaterialTemplate;

				if (ModelResources.MaterialSlotNames.IsValidIndex(Surface.MaterialIndex))
				{
					MaterialSlot.MaterialSlotName = ModelResources.MaterialSlotNames[Surface.MaterialIndex];
				}

				SetMeshUVChannelDensity(MaterialSlot.UVChannelData, RefSkeletalMeshData.Settings.DefaultUVChannelDensity);

				const int32 LODMaterialIndex = SkeletalMesh->GetLODInfoArray()[LODIndex].LODMaterialMap.Add(MaterialSlotIndex);
				SkeletalMesh->GetResourceForRendering()->LODRenderData[LODIndex].RenderSections[SurfaceIndex].MaterialIndex = LODMaterialIndex;

				FMutableMaterialPlaceholder MutableMaterialPlaceholder;
				MutableMaterialPlaceholder.ParentMaterialID = MaterialTemplate->GetUniqueID();
				MutableMaterialPlaceholder.MatIndex = MaterialSlotIndex;

				{
					MUTABLE_CPUPROFILER_SCOPE(ParamLoop);

					for (int32 VectorIndex = 0; VectorIndex < Surface.VectorCount; ++VectorIndex)
					{
						const FInstanceUpdateData::FVector& Vector = OperationData->InstanceUpdateData.Vectors[Surface.FirstVector + VectorIndex];

						// Decoding Material Layer from Mutable parameter name
						FString EncodingString = "-MutableLayerParam:";

						FString VectorName = Vector.Name.ToString();
						int32 EncodingPosition = VectorName.Find(EncodingString);
						int32 LayerIndex = -1;

						if (EncodingPosition == INDEX_NONE)
						{
							MutableMaterialPlaceholder.AddParam(
								FMutableMaterialPlaceholder::FMutableMaterialPlaceHolderParam(FName(Vector.Name), -1, Vector.Vector));
						}
						else
						{
							//Getting layer index
							int32 LayerPosition = VectorName.Len() - (EncodingPosition + EncodingString.Len());
							FString IndexString = VectorName.RightChop(VectorName.Len() - LayerPosition);
							LayerIndex = FCString::Atof(*IndexString);

							//Getting parameter name
							FString Sufix = EncodingString + FString::FromInt(LayerIndex);
							VectorName.RemoveFromEnd(Sufix);

							MutableMaterialPlaceholder.AddParam(
								FMutableMaterialPlaceholder::FMutableMaterialPlaceHolderParam(FName(VectorName), LayerIndex, Vector.Vector));
						}
					}

					for (int32 ScalarIndex = 0; ScalarIndex < Surface.ScalarCount; ++ScalarIndex)
					{
						const FInstanceUpdateData::FScalar& Scalar = OperationData->InstanceUpdateData.Scalars[Surface.FirstScalar + ScalarIndex];

						// Decoding Material Layer from Mutable parameter name
						FString EncodingString = "-MutableLayerParam:";

						FString ScalarName = Scalar.Name.ToString();
						int32 EncodingPosition = ScalarName.Find(EncodingString);
						int32 LayerIndex = -1;

						if (EncodingPosition == INDEX_NONE)
						{
							MutableMaterialPlaceholder.AddParam(
								FMutableMaterialPlaceholder::FMutableMaterialPlaceHolderParam(FName(Scalar.Name), -1, Scalar.Scalar));
						}
						else
						{
							//Getting layer index
							int32 LayerPosition = ScalarName.Len() - (EncodingPosition + EncodingString.Len());
							FString IndexString = ScalarName.RightChop(ScalarName.Len() - LayerPosition);
							LayerIndex = FCString::Atof(*IndexString);

							//Getting parameter name
							FString Sufix = EncodingString + FString::FromInt(LayerIndex);
							ScalarName.RemoveFromEnd(Sufix);

							MutableMaterialPlaceholder.AddParam(
								FMutableMaterialPlaceholder::FMutableMaterialPlaceHolderParam(FName(ScalarName), LayerIndex, Scalar.Scalar));
						}
					}
				}

				{
					MUTABLE_CPUPROFILER_SCOPE(BuildMaterials_ImageLoop);

					// Get the cache of resources of all live instances of this object
					FMutableResourceCache& Cache = UCustomizableObjectSystem::GetInstance()->GetPrivate()->GetObjectCache(CustomizableObject);

					FString CurrentState = Public->GetCurrentState();
					bool bNeverStream = OperationData->bNeverStream;

					check((bNeverStream && OperationData->MipsToSkip == 0) ||
						(!bNeverStream && OperationData->MipsToSkip >= 0));

					for (int32 ImageIndex = 0; ImageIndex < Surface.ImageCount; ++ImageIndex)
					{
						const FInstanceUpdateData::FImage& Image = OperationData->InstanceUpdateData.Images[Surface.FirstImage + ImageIndex];
						FString KeyName = Image.Name.ToString();
						mu::ImagePtrConst MutableImage = Image.Image;

						UTexture2D* MutableTexture = nullptr; // Texture generated by mutable
						UTexture* PassThroughTexture = nullptr; // Texture not generated by mutable

						// \TODO: Change this key to a struct.
						FString TextureReuseCacheRef = bReuseTextures ? FString::Printf(TEXT("%d-%d-%d-%d"), Image.BaseLOD, ComponentIndex, Surface.SurfaceId, ImageIndex) : FString();

						// If the mutable image is null, it must be in the cache
						FMutableImageCacheKey ImageCacheKey = { Image.ImageID, OperationData->MipsToSkip };
						if (!MutableImage)
						{
							TWeakObjectPtr<UTexture2D>* CachedPointerPtr = Cache.Images.Find(ImageCacheKey);
							if (CachedPointerPtr)
							{
								ensure(!CachedPointerPtr->IsStale());
								MutableTexture = CachedPointerPtr->Get();
							}

							check(MutableTexture);
						}

						// Check if the image is a reference to an engine texture
						if (MutableImage && Image.bIsPassThrough)
						{
							check(MutableImage->IsReference());

							uint32 ReferenceID = MutableImage->GetReferencedTexture();
							if (ModelResources.PassThroughTextures.IsValidIndex(ReferenceID))
							{
								TSoftObjectPtr<UTexture> Ref = ModelResources.PassThroughTextures[ReferenceID];

								// The texture should have been loaded by now by LoadAdditionalAssetsAsync()
								PassThroughTexture = Ref.Get();

								if (!PassThroughTexture)
								{
									// The texture should be loaded, something went wrong, possibly a bug in LoadAdditionalAssetsAsync()
									UE_LOG(LogMutable, Error,
										TEXT("Pass-through texture with name %s hasn't been loaded yet in BuildMaterials(). Forcing sync load."),
										*Ref.ToSoftObjectPath().ToString());
									ensure(false);
									PassThroughTexture = Ref.LoadSynchronous();
								}
							}

							if (!PassThroughTexture)
							{
								// Internal error.
								UE_LOG(LogMutable, Error, TEXT("Missing referenced image [%d]."), ReferenceID);
								continue;
							}
						}

						// Find the additional information for this image
						int32 ImageKey = FCString::Atoi(*KeyName);
						if (ImageKey >= 0 && ImageKey < ModelResources.ImageProperties.Num())
						{
							const FMutableModelImageProperties& Props = ModelResources.ImageProperties[ImageKey];

							if (!MutableTexture && !PassThroughTexture && MutableImage)
							{
								TWeakObjectPtr<UTexture2D>* ReusedTexture = bReuseTextures ? TextureReuseCache.Find(TextureReuseCacheRef) : nullptr;
								
								// This shared ptr will hold the reused texture platform data (mips) until the reused texture is updated 
								// and delete it automatically
								TSharedPtr<FTexturePlatformData, ESPMode::ThreadSafe> ReusedTexturePlatformData;

								if (ReusedTexture && (*ReusedTexture).IsValid() && !(*ReusedTexture)->HasAnyFlags(RF_BeginDestroyed))
								{
									// Only uncompressed textures can be reused. This also fixes an issue in the editor where textures supposedly 
									// uncompressed by their state, are still compressed because the CO has not been compiled at maximum settings
									// and the uncompressed setting cannot be applied to them.
									EPixelFormat PixelFormat = (*ReusedTexture)->GetPixelFormat();

									if (PixelFormat == EPixelFormat::PF_R8G8B8A8)
									{
										MutableTexture = (*ReusedTexture).Get();
										check(MutableTexture != nullptr);
									}
									else
									{
										ReusedTexture = nullptr;
										MutableTexture = CreateTexture();

#if WITH_EDITOR
										UE_LOG(LogMutable, Warning,
											TEXT("Tried to reuse an uncompressed texture with name %s. Make sure the selected Mutable state disables texture compression/streaming, that one of the state's runtime parameters affects the texture and that the CO is compiled with max. optimization settings."),
											*MutableTexture->GetName());
#endif
									}
								}
								else
								{
									ReusedTexture = nullptr;
									MutableTexture = CreateTexture();
								}

								if (MutableTexture)
								{
									if (OperationData->ImageToPlatformDataMap.Contains(Image.ImageID))
									{
										SetTexturePropertiesFromMutableImageProps(MutableTexture, Props, bNeverStream);

										FTexturePlatformData* PlatformData = OperationData->ImageToPlatformDataMap[Image.ImageID];

										if (ReusedTexture)
										{
											check(PlatformData->Mips.Num() == MutableTexture->GetPlatformData()->Mips.Num());
											check(PlatformData->Mips[0].SizeX == MutableTexture->GetPlatformData()->Mips[0].SizeX);
											check(PlatformData->Mips[0].SizeY == MutableTexture->GetPlatformData()->Mips[0].SizeY);

											// Now the ReusedTexturePlatformData shared ptr owns the platform data
											ReusedTexturePlatformData = TSharedPtr<FTexturePlatformData, ESPMode::ThreadSafe>(PlatformData);
										}
										else
										{
											// Now the MutableTexture owns the platform data
											MutableTexture->SetPlatformData(PlatformData);
										}

										OperationData->ImageToPlatformDataMap.Remove(Image.ImageID);
									}
									else
									{
										UE_LOG(LogMutable, Error, TEXT("Required image [%s] with ID [%lld] was not generated in the mutable thread, and it is not cached. LOD [%d]. Component [%d]"),
											*Props.TextureParameterName,
											Image.ImageID,
											LODIndex, ComponentIndex);
										continue;
									}

									if (bNeverStream)
									{
										// To prevent LogTexture Error "Loading non-streamed mips from an external bulk file."
										for (int32 i = 0; i < MutableTexture->GetPlatformData()->Mips.Num(); ++i)
										{
											MutableTexture->GetPlatformData()->Mips[i].BulkData.ClearBulkDataFlags(BULKDATA_PayloadInSeperateFile);
										}
									}

									{
										MUTABLE_CPUPROFILER_SCOPE(UpdateResource);
#if !PLATFORM_DESKTOP && !PLATFORM_SWITCH // Switch does this in FTexture2DResource::InitRHI()
										for (int32 i = 0; i < MutableTexture->GetPlatformData()->Mips.Num(); ++i)
										{
											uint32 DataFlags = MutableTexture->GetPlatformData()->Mips[i].BulkData.GetBulkDataFlags();
											MutableTexture->GetPlatformData()->Mips[i].BulkData.SetBulkDataFlags(DataFlags | BULKDATA_SingleUse);
										}
#endif

										if (ReusedTexture)
										{
											// Must remove texture from cache since it will be reused with a different ImageID
											for (TPair<FMutableImageCacheKey, TWeakObjectPtr<UTexture2D>>& CachedTexture : Cache.Images)
											{
												if (CachedTexture.Value == MutableTexture)
												{
													Cache.Images.Remove(CachedTexture.Key);
													break;
												}
											}

											check(ReusedTexturePlatformData.IsValid());

											if (ReusedTexturePlatformData.IsValid())
											{
												TSharedRef<FTexturePlatformData, ESPMode::ThreadSafe> PlatformDataRef = ReusedTexturePlatformData.ToSharedRef();
												ReuseTexture(MutableTexture, PlatformDataRef);
											}
										}
										else if (MutableTexture)
										{	
											//if (!bNeverStream) // No need to check bNeverStream. In that case, the texture won't use 
											// the MutableMipDataProviderFactory anyway and it's needed for detecting Mutable textures elsewhere
											{
												UMutableTextureMipDataProviderFactory* MutableMipDataProviderFactory = Cast<UMutableTextureMipDataProviderFactory>(MutableTexture->GetAssetUserDataOfClass(UMutableTextureMipDataProviderFactory::StaticClass()));
												if (!MutableMipDataProviderFactory)
												{
													MutableMipDataProviderFactory = NewObject<UMutableTextureMipDataProviderFactory>();

													if (MutableMipDataProviderFactory)
													{
														MutableMipDataProviderFactory->CustomizableObjectInstance = Public;
														check(LODIndex < 256 && ComponentIndex < 256 && ImageIndex < 256);
														MutableMipDataProviderFactory->ImageRef.ImageID = Image.ImageID;
														MutableMipDataProviderFactory->ImageRef.SurfaceId = Surface.SurfaceId;
														MutableMipDataProviderFactory->ImageRef.LOD = uint8(Image.BaseLOD);
														MutableMipDataProviderFactory->ImageRef.Component = uint8(ComponentIndex);
														MutableMipDataProviderFactory->ImageRef.Image = uint8(ImageIndex);
														MutableMipDataProviderFactory->ImageRef.BaseMip = uint8(Image.BaseMip);
														MutableMipDataProviderFactory->UpdateContext = UpdateContext;
														MutableTexture->AddAssetUserData(MutableMipDataProviderFactory);
													}
												}
											}

											MutableTexture->UpdateResource();
										}
									}

									Cache.Images.Add(ImageCacheKey, MutableTexture);						
								}
								else
								{
									UE_LOG(LogMutable, Error, TEXT("Texture creation failed."));
								}
							}

							FGeneratedTexture TextureData;
							TextureData.Key = ImageCacheKey;
							TextureData.Name = Props.TextureParameterName;
							TextureData.Texture = MutableTexture ? MutableTexture : PassThroughTexture;
							
							// Only add textures generated by mutable to the cache
							if (MutableTexture)
							{
								NewGeneratedTextures.Add(TextureData);
							}

							// Decoding Material Layer from Mutable parameter name
							FString ImageName = Image.Name.ToString();
							FString EncodingString = "-MutableLayerParam:";

							int32 EncodingPosition = ImageName.Find(EncodingString);
							int32 LayerIndex = -1;

							if (EncodingPosition == INDEX_NONE)
							{
								MutableMaterialPlaceholder.AddParam(
									FMutableMaterialPlaceholder::FMutableMaterialPlaceHolderParam(*Props.TextureParameterName, -1, TextureData));
							}
							else
							{
								//Getting layer index
								int32 LayerPosition = ImageName.Len() - (EncodingPosition + EncodingString.Len());
								FString IndexString = ImageName.RightChop(ImageName.Len() - LayerPosition);
								LayerIndex = FCString::Atof(*IndexString);

								MutableMaterialPlaceholder.AddParam(
									FMutableMaterialPlaceholder::FMutableMaterialPlaceHolderParam(*Props.TextureParameterName, LayerIndex, TextureData));
							}
						}
						else
						{
							// This means the compiled model (maybe coming from derived data) has images that the asset doesn't know about.
							UE_LOG(LogMutable, Error, TEXT("CustomizableObject derived data out of sync with asset for [%s]. Try recompiling it."), *CustomizableObject->GetName());
						}

						if (bReuseTextures)
						{
							if (MutableTexture)
							{
								TextureReuseCache.Add(TextureReuseCacheRef, MutableTexture);
							}
							else
							{
								TextureReuseCache.Remove(TextureReuseCacheRef);
							}
						}
					}
				}

				// Find or create the material for this slot
				UMaterialInterface* MaterialInterface = MaterialSlot.MaterialInterface;

				if (FMutableMaterialPlaceholder* FoundMaterialPlaceholder = ReuseMaterialCache.Find(MutableMaterialPlaceholder))
				{
					MaterialInterface = Materials[FoundMaterialPlaceholder->MatIndex].MaterialInterface;
				}
				else // Material not cached, create a new one
				{
					MUTABLE_CPUPROFILER_SCOPE(BuildMaterials_CreateMaterial);

					ReuseMaterialCache.Add(MutableMaterialPlaceholder);

					FGeneratedMaterial& Material = GeneratedMaterials.AddDefaulted_GetRef();
					Material.SurfaceId = Surface.SurfaceId;
					Material.MaterialIndex = Surface.MaterialIndex;
					Material.MaterialInterface = MaterialInterface;

					UMaterialInstanceDynamic* MaterialInstance = nullptr;
					
					if (const int32 OldMaterialIndex = OldGeneratedMaterials.Find(Material); bReuseMaterials && OldMaterialIndex != INDEX_NONE)
					{
						const FGeneratedMaterial& OldMaterial = OldGeneratedMaterials[OldMaterialIndex];
						MaterialInstance = Cast<UMaterialInstanceDynamic>(OldMaterial.MaterialInterface);
						Material.MaterialInterface = OldMaterial.MaterialInterface;
					}
					
					if (!MaterialInstance && MutableMaterialPlaceholder.GetParams().Num() != 0)
					{
						MaterialInstance = UMaterialInstanceDynamic::Create(MaterialTemplate, GetTransientPackage());
						Material.MaterialInterface = MaterialInstance;
					}

					if (MaterialInstance)
					{
						for (const FMutableMaterialPlaceholder::FMutableMaterialPlaceHolderParam& Param : MutableMaterialPlaceholder.GetParams())
						{
							switch (Param.Type)
							{
							case FMutableMaterialPlaceholder::EPlaceHolderParamType::Vector:
								if (Param.LayerIndex < 0)
								{
									FLinearColor Color = Param.Vector;

									if (FVector4f(Color) == mu::DefaultMutableColorValue)
									{
										FMaterialParameterInfo ParameterInfo(Param.ParamName);
										MaterialTemplate->GetVectorParameterValue(ParameterInfo, Color);
									}

									MaterialInstance->SetVectorParameterValue(Param.ParamName, Color);
								}
								else
								{
									FMaterialParameterInfo ParameterInfo = FMaterialParameterInfo(Param.ParamName, EMaterialParameterAssociation::LayerParameter, Param.LayerIndex);
									MaterialInstance->SetVectorParameterValueByInfo(ParameterInfo, Param.Vector);
								}

								break;

							case FMutableMaterialPlaceholder::EPlaceHolderParamType::Scalar:
								if (Param.LayerIndex < 0)
								{
									MaterialInstance->SetScalarParameterValue(FName(Param.ParamName), Param.Scalar);
								}
								else
								{
									FMaterialParameterInfo ParameterInfo = FMaterialParameterInfo(Param.ParamName, EMaterialParameterAssociation::LayerParameter, Param.LayerIndex);
									MaterialInstance->SetScalarParameterValueByInfo(ParameterInfo, Param.Scalar);
								}

								break;

							case FMutableMaterialPlaceholder::EPlaceHolderParamType::Texture:
								if (Param.LayerIndex < 0)
								{
									MaterialInstance->SetTextureParameterValue(Param.ParamName, Param.Texture.Texture);
								}
								else
								{
									FMaterialParameterInfo ParameterInfo = FMaterialParameterInfo(Param.ParamName, EMaterialParameterAssociation::LayerParameter, Param.LayerIndex);
									MaterialInstance->SetTextureParameterValueByInfo(ParameterInfo, Param.Texture.Texture);
								}

								Material.Textures.Add(Param.Texture);

								break;
							}
						}
					}

					MaterialInterface = Material.MaterialInterface;
				}

				// Assign the material to the slot, and add it to the  OverrideMaterials
				MaterialSlot.MaterialInterface = MaterialInterface;
				ComponentsData[ComponentIndex].OverrideMaterials.Add(MaterialInterface);
			}
		}

		{
			// Copy data from the FirstGeneratedLOD into the LODs below.
			for (int32 LODIndex = OperationData->FirstLODAvailable; LODIndex < FirstGeneratedLOD; ++LODIndex)
			{
				SkeletalMesh->GetLODInfoArray()[LODIndex].LODMaterialMap = SkeletalMesh->GetLODInfoArray()[FirstGeneratedLOD].LODMaterialMap;

				TIndirectArray<FSkeletalMeshLODRenderData>& LODRenderData = SkeletalMesh->GetResourceForRendering()->LODRenderData;

				const int32 NumRenderSections = LODRenderData[LODIndex].RenderSections.Num();
				check(NumRenderSections == LODRenderData[FirstGeneratedLOD].RenderSections.Num());

				if (NumRenderSections == LODRenderData[FirstGeneratedLOD].RenderSections.Num())
				{
					for (int32 RenderSectionIndex = 0; RenderSectionIndex < NumRenderSections; ++RenderSectionIndex)
					{
						const int32 MaterialIndex = LODRenderData[FirstGeneratedLOD].RenderSections[RenderSectionIndex].MaterialIndex;
						LODRenderData[LODIndex].RenderSections[RenderSectionIndex].MaterialIndex = MaterialIndex;
					}
				}
			}
		}

		if (!bUseOverrideMaterialsOnly)
		{
			// Force recreate render state after replacing the materials to avoid a crash in the render pipeline if the old materials are GCed while in use.
			RecreateRenderStateOnComponent[ComponentIndex] = SkeletalMesh->GetResourceForRendering()->IsInitialized() && SkeletalMesh->GetMaterials() != Materials;

			SkeletalMesh->SetMaterials(Materials);

#if WITH_EDITOR
			if (RecreateRenderStateOnComponent[ComponentIndex])
			{
				// Close all open editors for this mesh to invalidate viewports.
				GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->CloseAllEditorsForAsset(SkeletalMesh);
			}
#endif
		}

		// Ensure the number of materials is the same on both sides when using overrides. 
		check(SkeletalMesh->GetMaterials().Num() == Materials.Num());
	}

	// Force recreate render state if the mesh is reused and the materials have changed.
	// TODO: MTBL-1697 Remove after merging ConvertResources and Callbacks.
	if (RecreateRenderStateOnComponent.Find(true) != INDEX_NONE)
	{
		MUTABLE_CPUPROFILER_SCOPE(BuildMaterials_RecreateRenderState);

		for (TObjectIterator<UCustomizableObjectInstanceUsage> It; It; ++It)
		{
			UCustomizableObjectInstanceUsage* CustomizableObjectInstanceUsage = *It;

			if (!IsValid(CustomizableObjectInstanceUsage) || CustomizableObjectInstanceUsage->GetCustomizableObjectInstance() != Public)
			{
				continue;
			}

#if WITH_EDITOR
			if (CustomizableObjectInstanceUsage->IsNetMode(NM_DedicatedServer))
			{
				continue;
			}
#endif

			const int32 ComponentIndex = CustomizableObjectInstanceUsage->GetComponentIndex();
			if (!RecreateRenderStateOnComponent.IsValidIndex(ComponentIndex) || !RecreateRenderStateOnComponent[ComponentIndex])
			{
				continue;
			}

			USkeletalMeshComponent* AttachedParent = CustomizableObjectInstanceUsage->GetAttachParent();
			if (!AttachedParent || AttachedParent->GetSkeletalMeshAsset() != SkeletalMeshes[ComponentIndex])
			{
				continue;
			}

			AttachedParent->RecreateRenderState_Concurrent();
		}
	}

	{
		MUTABLE_CPUPROFILER_SCOPE(BuildMaterials_Exchange);

		UCustomizableObjectSystemPrivate* CustomizableObjectSystem = UCustomizableObjectSystem::GetInstance()->GetPrivate();
		TexturesToRelease.Empty();

		for (const FGeneratedTexture& Texture : NewGeneratedTextures)
		{
			CustomizableObjectSystem->AddTextureReference(Texture.Key);
		}

		for (const FGeneratedTexture& Texture : GeneratedTextures)
		{
			if (CustomizableObjectSystem->RemoveTextureReference(Texture.Key))
			{
				if (CustomizableObjectSystem->bReleaseTexturesImmediately)
				{
					TexturesToRelease.Add(Texture); // Texture count is zero, so prepare to release it
				}
			}
		}

		Exchange(GeneratedTextures, NewGeneratedTextures);

		// All pass-through textures have been set, no need to keep referencing them from the instance
		LoadedPassThroughTexturesPendingSetMaterial.Empty();
	}
}


void UCustomizableObjectInstance::SetReplacePhysicsAssets(bool bReplaceEnabled)
{
	bReplaceEnabled ? GetPrivate()->SetCOInstanceFlags(ReplacePhysicsAssets) : GetPrivate()->ClearCOInstanceFlags(ReplacePhysicsAssets);
}


void UCustomizableObjectInstance::SetReuseInstanceTextures(bool bTextureReuseEnabled)
{
	bTextureReuseEnabled ? GetPrivate()->SetCOInstanceFlags(ReuseTextures) : GetPrivate()->ClearCOInstanceFlags(ReuseTextures);
}


void UCustomizableObjectInstance::SetForceGenerateResidentMips(bool bForceGenerateResidentMips)
{
	bForceGenerateResidentMips ? GetPrivate()->SetCOInstanceFlags(ForceGenerateMipTail) : GetPrivate()->ClearCOInstanceFlags(ForceGenerateMipTail);
}


void UCustomizableObjectInstance::SetIsBeingUsedByComponentInPlay(bool bIsUsedByComponentInPlay)
{
	bIsUsedByComponentInPlay ? GetPrivate()->SetCOInstanceFlags(UsedByComponentInPlay) : GetPrivate()->ClearCOInstanceFlags(UsedByComponentInPlay);
}


bool UCustomizableObjectInstance::GetIsBeingUsedByComponentInPlay() const
{
	return GetPrivate()->HasCOInstanceFlags(UsedByComponentInPlay);
}


void UCustomizableObjectInstance::SetIsDiscardedBecauseOfTooManyInstances(bool bIsDiscarded)
{
	bIsDiscarded ? GetPrivate()->SetCOInstanceFlags(DiscardedByNumInstancesLimit) : GetPrivate()->ClearCOInstanceFlags(DiscardedByNumInstancesLimit);
}


bool UCustomizableObjectInstance::GetIsDiscardedBecauseOfTooManyInstances() const
{
	return GetPrivate()->HasCOInstanceFlags(DiscardedByNumInstancesLimit);
}


void UCustomizableObjectInstance::SetIsPlayerOrNearIt(bool bIsPlayerorNearIt)
{
	bIsPlayerorNearIt ? GetPrivate()->SetCOInstanceFlags(UsedByPlayerOrNearIt) : GetPrivate()->ClearCOInstanceFlags(UsedByPlayerOrNearIt);
}


float UCustomizableObjectInstance::GetMinSquareDistToPlayer() const
{
	return GetPrivate()->MinSquareDistFromComponentToPlayer;
}

void UCustomizableObjectInstance::SetMinSquareDistToPlayer(float NewValue)
{
	GetPrivate()->MinSquareDistFromComponentToPlayer = NewValue;
}


int32 UCustomizableObjectInstance::GetNumComponents() const
{
	return GetCustomizableObject() ? GetCustomizableObject()->GetComponentCount() : 0;
}


int32 UCustomizableObjectInstance::GetMinLODToLoad() const
{
	return Descriptor.MinLOD;	
}


int32 UCustomizableObjectInstance::GetNumLODsAvailable() const
{
	return GetPrivate()->GetNumLODsAvailable();
}


int32 UCustomizableObjectInstance::GetCurrentMinLOD() const
{
	return GetPrivate()->CommittedDescriptor.GetMinLod();
}


#if !UE_BUILD_SHIPPING
static bool bIgnoreMinLOD = false;
FAutoConsoleVariableRef CVarMutableIgnoreMinMaxLOD(
	TEXT("Mutable.IgnoreMinMaxLOD"),
	bIgnoreMinLOD,
	TEXT("The limits on the number of LODs to generate will be ignored."));
#endif


void UCustomizableObjectInstance::SetRequestedLODs(int32 InMinLOD, int32 , const TArray<uint16>& InRequestedLODsPerComponent, 
	                                               FMutableInstanceUpdateMap& InOutRequestedUpdates)
{
	check(PrivateData);

	if (!CanUpdateInstance())
	{
		return;
	}

	if (GetPrivate()->SkeletalMeshStatus == ESkeletalMeshStatus::Error)
	{
		return;
	}

	if (CVarPreserveUserLODsOnFirstGeneration.GetValueOnGameThread() &&
		GetCustomizableObject()->bPreserveUserLODsOnFirstGeneration &&
		GetPrivate()->SkeletalMeshStatus != ESkeletalMeshStatus::Success)
	{
		return;
	}
	
#if !UE_BUILD_SHIPPING
	// Ignore Min LOD limits. Mainly used for debug
	if (bIgnoreMinLOD)
	{
		InMinLOD = 0;
	}
#endif

	FMutableUpdateCandidate MutableUpdateCandidate(this);

	// Clamp Min LOD
	const int32 MinLODIdx = GetCustomizableObject()->GetPrivate()->GetMinLODIndex();
	const int32 MaxLODIdx = PrivateData->NumLODsAvailable - 1;
	InMinLOD = FMath::Clamp(InMinLOD, MinLODIdx, MaxLODIdx);

	const bool bMinLODChanged = Descriptor.MinLOD != InMinLOD;

	PrivateData->SetCOInstanceFlags(InMinLOD > GetCurrentMinLOD() ? PendingLODsDowngrade : ECONone);

	// Save the new LODs
	MutableUpdateCandidate.MinLOD = InMinLOD;
	MutableUpdateCandidate.RequestedLODLevels = Descriptor.GetRequestedLODLevels();

	bool bUpdateRequestedLODs = false;
	if (UCustomizableObjectSystem::GetInstance()->IsOnlyGenerateRequestedLODsEnabled())
	{
		const uint16 FirstNonStreamedLODIndex = FMath::Clamp(PrivateData->FirstResidentLOD, 0, MaxLODIdx);

		const int32 ComponentCount = GetNumComponents();
		if (ComponentCount != MutableUpdateCandidate.RequestedLODLevels.Num())
		{
			MutableUpdateCandidate.RequestedLODLevels.Init(FirstNonStreamedLODIndex, ComponentCount);
		}

		const TArray<uint16>& GeneratedLODsPerComponent = GetPrivate()->CommittedDescriptorHash.RequestedLODsPerComponent;

		const bool bIgnoreGeneratedLODs = GeneratedLODsPerComponent.Num() != ComponentCount;

		if (bMinLODChanged || bIgnoreGeneratedLODs || Descriptor.GetRequestedLODLevels() != InRequestedLODsPerComponent)
		{
			check(InRequestedLODsPerComponent.Num() == ComponentCount);

			bUpdateRequestedLODs = bIgnoreGeneratedLODs;
			for (int32 ComponentIndex = 0; ComponentIndex < ComponentCount; ++ComponentIndex)
			{
				uint16 PredictedLOD = FMath::Min(InRequestedLODsPerComponent[ComponentIndex], FirstNonStreamedLODIndex);

				if (!bIgnoreGeneratedLODs)
				{
					PredictedLOD = FMath::Min(PredictedLOD, GeneratedLODsPerComponent[ComponentIndex]);
				}

				bUpdateRequestedLODs |= (PredictedLOD != MutableUpdateCandidate.RequestedLODLevels[ComponentIndex]);

				// Save new RequestedLODs
				MutableUpdateCandidate.RequestedLODLevels[ComponentIndex] = PredictedLOD;
			}
		}
	}

	if (bMinLODChanged || bUpdateRequestedLODs)
	{
		// TODO: Remove this flag as it will become redundant with the new InOutRequestedUpdates system
		PrivateData->SetCOInstanceFlags(PendingLODsUpdate);

		InOutRequestedUpdates.Add(this, MutableUpdateCandidate);
	}
}


const TArray<uint16>& UCustomizableObjectInstance::GetRequestedLODsPerComponent() const
{
	return Descriptor.RequestedLODLevels;
}


USkeletalMesh* UCustomizableObjectInstance::GetSkeletalMesh(int32 ComponentIndex) const
{
	check(PrivateData);
	return PrivateData->SkeletalMeshes.IsValidIndex(ComponentIndex) ? PrivateData->SkeletalMeshes[ComponentIndex] : nullptr;
}


bool UCustomizableObjectInstance::HasAnySkeletalMesh() const
{
	check(PrivateData);
	for (int32 Index = 0; Index < PrivateData->SkeletalMeshes.Num(); ++Index)
	{
		if (PrivateData->SkeletalMeshes[Index])
		{
			return true;
		}
	}

	return false;
}


TArray<FCustomizableObjectBoolParameterValue>& UCustomizableObjectInstance::GetBoolParameters()
{
	return Descriptor.GetBoolParameters();
}


const TArray<FCustomizableObjectBoolParameterValue>& UCustomizableObjectInstance::GetBoolParameters() const
{
	return Descriptor.GetBoolParameters();
}


TArray<FCustomizableObjectIntParameterValue>& UCustomizableObjectInstance::GetIntParameters()
{
	return Descriptor.GetIntParameters();
}


const TArray<FCustomizableObjectIntParameterValue>& UCustomizableObjectInstance::GetIntParameters() const
{
	return Descriptor.GetIntParameters();
}


TArray<FCustomizableObjectFloatParameterValue>& UCustomizableObjectInstance::GetFloatParameters()
{
	return Descriptor.GetFloatParameters();
}


const TArray<FCustomizableObjectFloatParameterValue>& UCustomizableObjectInstance::GetFloatParameters() const
{
	return Descriptor.GetFloatParameters();
}


TArray<FCustomizableObjectTextureParameterValue>& UCustomizableObjectInstance::GetTextureParameters()
{
	return Descriptor.GetTextureParameters();
}


const TArray<FCustomizableObjectTextureParameterValue>& UCustomizableObjectInstance::GetTextureParameters() const
{
	return Descriptor.GetTextureParameters();
}


TArray<FCustomizableObjectVectorParameterValue>& UCustomizableObjectInstance::GetVectorParameters()
{
	return Descriptor.GetVectorParameters();
}

const TArray<FCustomizableObjectVectorParameterValue>& UCustomizableObjectInstance::GetVectorParameters() const
{
	return Descriptor.GetVectorParameters();
}


TArray<FCustomizableObjectProjectorParameterValue>& UCustomizableObjectInstance::GetProjectorParameters()
{
	return Descriptor.GetProjectorParameters();
}


const TArray<FCustomizableObjectProjectorParameterValue>& UCustomizableObjectInstance::GetProjectorParameters() const
{
	return Descriptor.GetProjectorParameters();
}


bool UCustomizableObjectInstance::HasAnyParameters() const
{
	return Descriptor.HasAnyParameters();	
}


TSubclassOf<UAnimInstance> UCustomizableObjectInstance::GetAnimBP(int32 ComponentIndex, const FName& SlotName) const
{
	FCustomizableInstanceComponentData* ComponentData =	GetPrivate()->GetComponentData(ComponentIndex);
	
	if (!ComponentData)
	{
		FString ErrorMsg = FString::Printf(TEXT("Tried to access an invalid component index [%d] in a Mutable Instance."), ComponentIndex);
		UE_LOG(LogMutable, Error, TEXT("%s"), *ErrorMsg);
#if WITH_EDITOR
		FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
		MessageLogModule.RegisterLogListing(FName("Mutable"), FText::FromString(FString("Mutable")));
		FMessageLog MessageLog("Mutable");

		MessageLog.Notify(FText::FromString(ErrorMsg), EMessageSeverity::Error, true);
#endif

		return nullptr;
	}

	TSoftClassPtr<UAnimInstance>* Result = ComponentData->AnimSlotToBP.Find(SlotName);

	return Result ? Result->Get() : nullptr;
}

const FGameplayTagContainer& UCustomizableObjectInstance::GetAnimationGameplayTags() const
{
	return GetPrivate()->AnimBPGameplayTags;
}

void UCustomizableObjectInstance::ForEachAnimInstance(int32 ComponentIndex, FEachComponentAnimInstanceClassDelegate Delegate) const
{
	// allow us to log out both bad states with one pass
	bool bAnyErrors = false;

	if (!Delegate.IsBound())
	{
		FString ErrorMsg = FString::Printf(TEXT("Attempting to iterate over AnimInstances with an unbound delegate - does nothing!"), ComponentIndex);
		UE_LOG(LogMutable, Warning, TEXT("%s"), *ErrorMsg);
#if WITH_EDITOR
		FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
		MessageLogModule.RegisterLogListing(FName("Mutable"), FText::FromString(FString("Mutable")));
		FMessageLog MessageLog("Mutable");

		MessageLog.Notify(FText::FromString(ErrorMsg), EMessageSeverity::Warning, true);
#endif
		bAnyErrors = true;
	}

	const FCustomizableInstanceComponentData* ComponentData =	GetPrivate()->GetComponentData(ComponentIndex);
	
	if (!ComponentData)
	{
		FString ErrorMsg = FString::Printf(TEXT("Tried to access an invalid component index [%d] in a Mutable Instance."), ComponentIndex);
		UE_LOG(LogMutable, Error, TEXT("%s"), *ErrorMsg);
#if WITH_EDITOR
		FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
		MessageLogModule.RegisterLogListing(FName("Mutable"), FText::FromString(FString("Mutable")));
		FMessageLog MessageLog("Mutable");

		MessageLog.Notify(FText::FromString(ErrorMsg), EMessageSeverity::Error, true);
#endif

		bAnyErrors = true;
	}

	if (bAnyErrors)
	{
		return;
	}

	for (const TPair<FName, TSoftClassPtr<UAnimInstance>>& MapElem : ComponentData->AnimSlotToBP)
	{
		const FName& Index = MapElem.Key;
		const TSoftClassPtr<UAnimInstance>& AnimBP = MapElem.Value;

		// if this _can_ resolve to a real AnimBP
		if (!AnimBP.IsNull())
		{
			// force a load right now - we don't know whether we would have loaded already - this could be called in editor
			const TSubclassOf<UAnimInstance> LiveAnimBP = AnimBP.LoadSynchronous();
			if (LiveAnimBP)
			{
				Delegate.Execute(Index, LiveAnimBP);
			}
		}
	}
}


bool UCustomizableObjectInstance::AnimInstanceNeedsFixup(TSubclassOf<UAnimInstance> AnimInstanceClass) const
{
	return PrivateData->AnimBpPhysicsAssets.Contains(AnimInstanceClass);
}


void UCustomizableObjectInstance::AnimInstanceFixup(UAnimInstance* InAnimInstance) const
{
	if (!InAnimInstance)
	{
		return;
	}

	TSubclassOf<UAnimInstance> AnimInstanceClass = InAnimInstance->GetClass();

	const TArray<FAnimInstanceOverridePhysicsAsset>* AnimInstanceOverridePhysicsAssets = 
			PrivateData->GetGeneratedPhysicsAssetsForAnimInstance(AnimInstanceClass);
	
	if (!AnimInstanceOverridePhysicsAssets)
	{
		return;
	}

	// Swap RigidBody anim nodes override physics assets with mutable generated ones.
	if (UAnimBlueprintGeneratedClass* AnimClass = Cast<UAnimBlueprintGeneratedClass>(AnimInstanceClass))
	{
		bool bPropertyMismatchFound = false;
		const int32 AnimNodePropertiesNum = AnimClass->AnimNodeProperties.Num();

		for (const FAnimInstanceOverridePhysicsAsset& PropIndexAndAsset : *AnimInstanceOverridePhysicsAssets)
		{
			check(PropIndexAndAsset.PropertyIndex >= 0);
			if (PropIndexAndAsset.PropertyIndex >= AnimNodePropertiesNum)
			{
				bPropertyMismatchFound = true;
				continue;
			}

			const int32 AnimNodePropIndex = PropIndexAndAsset.PropertyIndex;

			FStructProperty* StructProperty = AnimClass->AnimNodeProperties[AnimNodePropIndex];

			if (!ensure(StructProperty))
			{
				bPropertyMismatchFound = true;
				continue;
			}

			const bool bIsRigidBodyNode = StructProperty->Struct->IsChildOf(FAnimNode_RigidBody::StaticStruct());

			if (!bIsRigidBodyNode)
			{
				bPropertyMismatchFound = true;
				continue;
			}

			FAnimNode_RigidBody* RbanNode = StructProperty->ContainerPtrToValuePtr<FAnimNode_RigidBody>(InAnimInstance);

			if (!ensure(RbanNode))
			{
				bPropertyMismatchFound = true;
				continue;
			}

			RbanNode->OverridePhysicsAsset = PropIndexAndAsset.PhysicsAsset;
		}
#if WITH_EDITOR
		if (bPropertyMismatchFound)
		{
			UE_LOG(LogMutable, Warning, TEXT("AnimBp %s is not in sync with the data stored in the CO %s. A CO recompilation may be needed."),
				*AnimInstanceClass.Get()->GetName(), 
				*GetCustomizableObject()->GetName());
		}
#endif
	}
}

const TArray<FAnimInstanceOverridePhysicsAsset>* UCustomizableInstancePrivate::GetGeneratedPhysicsAssetsForAnimInstance(TSubclassOf<UAnimInstance> AnimInstanceClass) const
{
	const FAnimBpGeneratedPhysicsAssets* Found = AnimBpPhysicsAssets.Find(AnimInstanceClass);

	if (!Found)
	{
		return nullptr;
	}

	return &Found->AnimInstancePropertyIndexAndPhysicsAssets;
}

void UCustomizableObjectInstance::ForEachAnimInstance(int32 ComponentIndex, FEachComponentAnimInstanceClassNativeDelegate Delegate) const
{
	// allow us to log out both bad states with one pass
	bool bAnyErrors = false;

	if (!Delegate.IsBound())
	{
		FString ErrorMsg = FString::Printf(TEXT("Attempting to iterate over AnimInstances with an unbound delegate - does nothing!"), ComponentIndex);
		UE_LOG(LogMutable, Warning, TEXT("%s"), *ErrorMsg);
#if WITH_EDITOR
		FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
		MessageLogModule.RegisterLogListing(FName("Mutable"), FText::FromString(FString("Mutable")));
		FMessageLog MessageLog("Mutable");

		MessageLog.Notify(FText::FromString(ErrorMsg), EMessageSeverity::Warning, true);
#endif
		bAnyErrors = true;
	}

	const FCustomizableInstanceComponentData* ComponentData =	GetPrivate()->GetComponentData(ComponentIndex);
	
	if (!ComponentData)
	{
		FString ErrorMsg = FString::Printf(TEXT("Tried to access an invalid component index [%d] in a Mutable Instance."), ComponentIndex);
		UE_LOG(LogMutable, Error, TEXT("%s"), *ErrorMsg);
#if WITH_EDITOR
		FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
		MessageLogModule.RegisterLogListing(FName("Mutable"), FText::FromString(FString("Mutable")));
		FMessageLog MessageLog("Mutable");

		MessageLog.Notify(FText::FromString(ErrorMsg), EMessageSeverity::Error, true);
#endif

		bAnyErrors = true;
	}

	if (bAnyErrors)
	{
		return;
	}

	for (const TPair<FName, TSoftClassPtr<UAnimInstance>>& MapElem : ComponentData->AnimSlotToBP)
	{
		const FName& Index = MapElem.Key;
		const TSoftClassPtr<UAnimInstance>& AnimBP = MapElem.Value;

		// if this _can_ resolve to a real AnimBP
		if (!AnimBP.IsNull())
		{
			// force a load right now - we don't know whether we would have loaded already - this could be called in editor
			const TSubclassOf<UAnimInstance> LiveAnimBP = AnimBP.LoadSynchronous();
			if (LiveAnimBP)
			{
				Delegate.Execute(Index, LiveAnimBP);
			}
		}
	}
}


TSet<UAssetUserData*> UCustomizableObjectInstance::GetMergedAssetUserData(int32 ComponentIndex) const
{
	UCustomizableInstancePrivate* PrivateInstanceData = GetPrivate();

	if (PrivateInstanceData && PrivateInstanceData->ComponentsData.IsValidIndex(ComponentIndex))
	{
		TSet<UAssetUserData*> Set;
		
		// Have to convert to UAssetUserData* because BP functions don't support TObjectPtr
		for (const TObjectPtr<UAssetUserData>& Elem : PrivateInstanceData->ComponentsData[ComponentIndex].AssetUserDataArray)
		{
			Set.Add(Elem);
		}

		return Set;
	}
	else
	{
		return TSet<UAssetUserData*>();
	}
}


#if WITH_EDITORONLY_DATA

void CalculateBonesToRemove(const FSkeletalMeshLODRenderData& LODResource, const FReferenceSkeleton& RefSkeleton, TArray<FBoneReference>& OutBonesToRemove)
{
	const int32 NumBones = RefSkeleton.GetNum();
	OutBonesToRemove.Empty(NumBones);

	TArray<bool> RemovedBones;
	RemovedBones.Init(true, NumBones);

	for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
	{
		if (LODResource.RequiredBones.Find((uint16)BoneIndex) != INDEX_NONE)
		{
			RemovedBones[BoneIndex] = false;
			continue;
		}

		const int32 ParentIndex = RefSkeleton.GetParentIndex(BoneIndex);
		if (RemovedBones.IsValidIndex(ParentIndex) && !RemovedBones[ParentIndex])
		{
			OutBonesToRemove.Add(RefSkeleton.GetBoneName(BoneIndex));
		}
	}
}

void UCustomizableInstancePrivate::RegenerateImportedModels()
{
	MUTABLE_CPUPROFILER_SCOPE(RegenerateImportedModels);

	for (USkeletalMesh* SkeletalMesh : SkeletalMeshes)
	{
		if (!SkeletalMesh)
		{
			continue;
		}

		FSkeletalMeshRenderData* RenderData = SkeletalMesh->GetResourceForRendering();
		if (!RenderData || RenderData->IsInitialized())
		{
			continue;
		}

		for (UClothingAssetBase* ClothingAssetBase : SkeletalMesh->GetMeshClothingAssets())
		{
			if (!ClothingAssetBase)
			{
				continue;
			}

			UClothingAssetCommon* ClothAsset = Cast<UClothingAssetCommon>(ClothingAssetBase);

			if (!ClothAsset)
			{
				continue;
			}

			if (!ClothAsset->LodData.Num())
			{
				continue;
			}

			for (FClothLODDataCommon& ClothLodData : ClothAsset->LodData)
			{
				ClothLodData.PointWeightMaps.Empty(16);
				for (TPair<uint32, FPointWeightMap>& WeightMap : ClothLodData.PhysicalMeshData.WeightMaps)
				{
					if (WeightMap.Value.Num())
					{
						FPointWeightMap& PointWeightMap = ClothLodData.PointWeightMaps.AddDefaulted_GetRef();
						PointWeightMap.Initialize(WeightMap.Value, WeightMap.Key);
					}
				}
			}
		}

		FSkeletalMeshModel* ImportedModel = SkeletalMesh->GetImportedModel();
		ImportedModel->bGuidIsHash = false;
		ImportedModel->SkeletalMeshModelGUID = FGuid::NewGuid();

		ImportedModel->LODModels.Empty();

		int32 OriginalIndex = 0;
		for (int32 LODIndex = 0; LODIndex < RenderData->LODRenderData.Num(); ++LODIndex)
		{
			ImportedModel->LODModels.Add(new FSkeletalMeshLODModel());
			FSkeletalMeshLODModel& LODModel = ImportedModel->LODModels[LODIndex];

			FSkeletalMeshLODRenderData& LODRenderData = RenderData->LODRenderData[LODIndex];
			int32 CurrentSectionInitialVertex = 0;

			LODModel.ActiveBoneIndices = LODRenderData.ActiveBoneIndices;
			LODModel.NumTexCoords = LODRenderData.GetNumTexCoords();
			LODModel.RequiredBones = LODRenderData.RequiredBones;
			LODModel.NumVertices = LODRenderData.GetNumVertices();

			// Indices
			if (LODRenderData.MultiSizeIndexContainer.IsIndexBufferValid())
			{
				const int32 NumIndices = LODRenderData.MultiSizeIndexContainer.GetIndexBuffer()->Num();
				LODModel.IndexBuffer.SetNum(NumIndices);
				for (int32 Index = 0; Index < NumIndices; ++Index)
				{
					LODModel.IndexBuffer[Index] = LODRenderData.MultiSizeIndexContainer.GetIndexBuffer()->Get(Index);
				}
			}

			LODModel.Sections.SetNum(LODRenderData.RenderSections.Num());

			for (int SectionIndex = 0; SectionIndex < LODRenderData.RenderSections.Num(); ++SectionIndex)
			{
				const FSkelMeshRenderSection& RenderSection = LODRenderData.RenderSections[SectionIndex];
				FSkelMeshSection& ImportedSection = ImportedModel->LODModels[LODIndex].Sections[SectionIndex];

				ImportedSection.CorrespondClothAssetIndex = RenderSection.CorrespondClothAssetIndex;
				ImportedSection.ClothingData = RenderSection.ClothingData;

				if (RenderSection.ClothMappingDataLODs.Num())
				{
					ImportedSection.ClothMappingDataLODs.SetNum(1);
					ImportedSection.ClothMappingDataLODs[0] = RenderSection.ClothMappingDataLODs[0];
				}

				// Vertices
				ImportedSection.NumVertices = RenderSection.NumVertices;
				ImportedSection.SoftVertices.Empty(RenderSection.NumVertices);
				ImportedSection.SoftVertices.AddUninitialized(RenderSection.NumVertices);
				ImportedSection.bUse16BitBoneIndex = LODRenderData.DoesVertexBufferUse16BitBoneIndex();

				for (uint32 i = 0; i < RenderSection.NumVertices; ++i)
				{
					const FPositionVertex* PosPtr = static_cast<const FPositionVertex*>(LODRenderData.StaticVertexBuffers.PositionVertexBuffer.GetVertexData());
					PosPtr += (CurrentSectionInitialVertex + i);

					check(!LODRenderData.StaticVertexBuffers.StaticMeshVertexBuffer.GetUseHighPrecisionTangentBasis());
					const FPackedNormal* TangentPtr = static_cast<const FPackedNormal*>(LODRenderData.StaticVertexBuffers.StaticMeshVertexBuffer.GetTangentData());
					TangentPtr += ((CurrentSectionInitialVertex + i) * 2);

					check(LODRenderData.StaticVertexBuffers.StaticMeshVertexBuffer.GetUseFullPrecisionUVs());

					using UVsVectorType = typename TDecay<decltype(DeclVal<FSoftSkinVertex>().UVs[0])>::Type;

					const UVsVectorType* TexCoordPosPtr = static_cast<const UVsVectorType*>(LODRenderData.StaticVertexBuffers.StaticMeshVertexBuffer.GetTexCoordData());
					const uint32 NumTexCoords = LODRenderData.StaticVertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords();
					TexCoordPosPtr += ((CurrentSectionInitialVertex + i) * NumTexCoords);

					FSoftSkinVertex& Vertex = ImportedSection.SoftVertices[i];
					for (int32 j = 0; j < RenderSection.MaxBoneInfluences; ++j)
					{
						Vertex.InfluenceBones[j] = LODRenderData.SkinWeightVertexBuffer.GetBoneIndex(CurrentSectionInitialVertex + i, j);
						Vertex.InfluenceWeights[j] = LODRenderData.SkinWeightVertexBuffer.GetBoneWeight(CurrentSectionInitialVertex + i, j);
					}

					for (int32 j = RenderSection.MaxBoneInfluences; j < MAX_TOTAL_INFLUENCES; ++j)
					{
						Vertex.InfluenceBones[j] = 0;
						Vertex.InfluenceWeights[j] = 0;
					}


					Vertex.Color = FColor::White;

					Vertex.Position = PosPtr->Position;

					Vertex.TangentX = TangentPtr[0].ToFVector3f();
					Vertex.TangentZ = TangentPtr[1].ToFVector3f();
					float TangentSign = TangentPtr[1].Vector.W < 0 ? -1.f : 1.f;
					Vertex.TangentY = FVector3f::CrossProduct(Vertex.TangentZ, Vertex.TangentX) * TangentSign;

					Vertex.UVs[0] = TexCoordPosPtr[0];
					Vertex.UVs[1] = NumTexCoords > 1 ? TexCoordPosPtr[1] : UVsVectorType::ZeroVector;
					Vertex.UVs[2] = NumTexCoords > 2 ? TexCoordPosPtr[2] : UVsVectorType::ZeroVector;
					Vertex.UVs[3] = NumTexCoords > 3 ? TexCoordPosPtr[3] : UVsVectorType::ZeroVector;
				}

				CurrentSectionInitialVertex += RenderSection.NumVertices;

				// Triangles
				ImportedSection.NumTriangles = RenderSection.NumTriangles;
				ImportedSection.BaseIndex = RenderSection.BaseIndex;
				ImportedSection.BaseVertexIndex = RenderSection.BaseVertexIndex;
				ImportedSection.BoneMap = RenderSection.BoneMap;

				// Add bones to remove
				CalculateBonesToRemove(LODRenderData, SkeletalMesh->GetRefSkeleton(), SkeletalMesh->GetLODInfo(LODIndex)->BonesToRemove);

				const TArray<int32>& LODMaterialMap = SkeletalMesh->GetLODInfoArray()[LODIndex].LODMaterialMap;

				if (LODMaterialMap.IsValidIndex(RenderSection.MaterialIndex))
				{
					ImportedSection.MaterialIndex = LODMaterialMap[RenderSection.MaterialIndex];
				}
				else
				{
					// The material should have been in the LODMaterialMap
					ensureMsgf(false, TEXT("Unexpected material index in UCustomizableInstancePrivate::RegenerateImportedModel"));

					// Fallback index, may shift materials around sections
					if (SkeletalMesh->GetMaterials().IsValidIndex(RenderSection.MaterialIndex))
					{
						ImportedSection.MaterialIndex = LODMaterialMap[RenderSection.MaterialIndex];
					}
					else
					{
						// The material should have been in the LODMaterialMap
						ensureMsgf(false, TEXT("Unexpected material index in UCustomizableInstancePrivate::RegenerateImportedModel"));

						// Fallback index, may shift materials around sections
						if (SkeletalMesh->GetMaterials().IsValidIndex(RenderSection.MaterialIndex))
						{
							ImportedSection.MaterialIndex = RenderSection.MaterialIndex;
						}
						else
						{
							ImportedSection.MaterialIndex = 0;
						}
					}
				}

				ImportedSection.MaxBoneInfluences = RenderSection.MaxBoneInfluences;
				ImportedSection.OriginalDataSectionIndex = OriginalIndex++;

				FSkelMeshSourceSectionUserData& SectionUserData = LODModel.UserSectionsData.FindOrAdd(ImportedSection.OriginalDataSectionIndex);
				SectionUserData.bCastShadow = RenderSection.bCastShadow;
				SectionUserData.bDisabled = RenderSection.bDisabled;

				SectionUserData.CorrespondClothAssetIndex = RenderSection.CorrespondClothAssetIndex;
				SectionUserData.ClothingData.AssetGuid = RenderSection.ClothingData.AssetGuid;
				SectionUserData.ClothingData.AssetLodIndex = RenderSection.ClothingData.AssetLodIndex;
				
				LODModel.SyncronizeUserSectionsDataArray();

				// DDC keys
				const USkeletalMeshLODSettings* LODSettings = SkeletalMesh->GetLODSettings();
				const bool bValidLODSettings = LODSettings && LODSettings->GetNumberOfSettings() > LODIndex;
				const FSkeletalMeshLODGroupSettings* SkeletalMeshLODGroupSettings = bValidLODSettings ? &LODSettings->GetSettingsForLODLevel(LODIndex) : nullptr;

				FSkeletalMeshLODInfo* LODInfo = SkeletalMesh->GetLODInfo(LODIndex);
				LODInfo->BuildGUID = LODInfo->ComputeDeriveDataCacheKey(SkeletalMeshLODGroupSettings);

				LODModel.BuildStringID = LODModel.GetLODModelDeriveDataKey();
			}
		}
	}
}

#endif
