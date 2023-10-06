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
#include "Engine/Texture2DArray.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Modules/ModuleManager.h"
#include "MuCO/CustomizableInstanceLODManagement.h"
#include "MuCO/CustomizableInstancePrivateData.h"
#include "MuCO/CustomizableObjectExtension.h"
#include "MuCO/CustomizableObjectMipDataProvider.h"
#include "MuCO/CustomizableObjectPrivate.h"
#include "MuCO/CustomizableSkeletalComponent.h"
#include "MuCO/DefaultImageProvider.h"
#include "MuCO/ICustomizableObjectModule.h"
#include "MuCO/UnrealConversionUtils.h"
#include "MuCO/UnrealPortabilityHelpers.h"
#include "MuR/Model.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Rendering/Texture2DResource.h"
#include "RenderingThread.h"
#include "SkeletalMergingLibrary.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CustomizableObjectInstance)

#if WITH_EDITOR
#include "Logging/MessageLog.h"
#include "MessageLogModule.h"
#endif

UTexture2D* UCustomizableInstancePrivateData::CreateTexture()
{
	UTexture2D* NewTexture = NewObject<UTexture2D>(
		GetTransientPackage(),
		NAME_None,
		RF_Transient
		);
	UCustomizableObjectSystem::GetInstance()->GetPrivate()->CreatedTexture(NewTexture);
	NewTexture->SetPlatformData( nullptr );

	return NewTexture;
}


mu::FResourceID UCustomizableInstancePrivateData::GetLastMeshId(int32 ComponentIndex, int32 LODIndex) const
{
	const FCustomizableInstanceComponentData* ComponentData = GetComponentData(ComponentIndex);
	check(ComponentData);
	check(ComponentData->LastMeshIdPerLOD.IsValidIndex(LODIndex));
		
	return ComponentsData[ComponentIndex].LastMeshIdPerLOD[LODIndex];
}


void UCustomizableInstancePrivateData::SetLastMeshId(int32 ComponentIndex, int32 LODIndex, mu::FResourceID MeshId)
{
	FCustomizableInstanceComponentData* ComponentData = GetComponentData(ComponentIndex);
	check(ComponentData);
	check(ComponentData->LastMeshIdPerLOD.IsValidIndex(LODIndex));

	ComponentData->LastMeshIdPerLOD[LODIndex] = MeshId;
}


void UCustomizableInstancePrivateData::InitLastUpdateData(const TSharedPtr<FMutableOperationData>& OperationData)
{
	LastUpdateData.LODs.Init(FInstanceGeneratedData::FLOD(), OperationData->NumLODsAvailable);
	for (int32 LODIndex = OperationData->CurrentMinLOD; LODIndex <= OperationData->CurrentMaxLOD; ++LODIndex)
	{
		LastUpdateData.LODs[LODIndex].FirstComponent = OperationData->InstanceUpdateData.LODs[LODIndex].FirstComponent;
		LastUpdateData.LODs[LODIndex].ComponentCount = OperationData->InstanceUpdateData.LODs[LODIndex].ComponentCount;
	}

	const int32 NumComponents = OperationData->InstanceUpdateData.Components.Num();
	LastUpdateData.Components.Init(FInstanceGeneratedData::FComponent(), NumComponents);
	for (int32 ComponentIndex = 0; ComponentIndex < NumComponents; ++ComponentIndex)
	{
		FInstanceGeneratedData::FComponent& LastUpdateDataComponent = LastUpdateData.Components[ComponentIndex];
		const FInstanceUpdateData::FComponent& UpdateComponent = OperationData->InstanceUpdateData.Components[ComponentIndex];
		
		LastUpdateDataComponent.ComponentId = UpdateComponent.Id;
		LastUpdateDataComponent.bGenerated = UpdateComponent.bGenerated;

		LastUpdateDataComponent.MeshID = UpdateComponent.MeshID;

		LastUpdateDataComponent.FirstSurface = UpdateComponent.FirstSurface;
		LastUpdateDataComponent.SurfaceCount = UpdateComponent.SurfaceCount;
	}

	const int32 NumSurfaces = OperationData->InstanceUpdateData.Surfaces.Num();
	LastUpdateData.SurfaceIds.Init(0, NumSurfaces);
	for (int32 SurfaceIndex = 0; SurfaceIndex < NumSurfaces; ++SurfaceIndex)
{
		LastUpdateData.SurfaceIds[SurfaceIndex] = OperationData->InstanceUpdateData.Surfaces[SurfaceIndex].SurfaceId;
	}
}


void UCustomizableInstancePrivateData::InvalidateGeneratedData()
{
	for (FCustomizableInstanceComponentData& ComponentData : ComponentsData)
	{
		ComponentData.LastMeshIdPerLOD.Init(MAX_uint64, NumLODsAvailable);
	}

	LastUpdateData.Clear();
}


FCustomizableInstanceComponentData* UCustomizableInstancePrivateData::GetComponentData(int32 ComponentIndex)
{
	return ComponentsData.FindByPredicate([&ComponentIndex](FCustomizableInstanceComponentData& C) { return C.ComponentIndex == ComponentIndex; });
}


const FCustomizableInstanceComponentData* UCustomizableInstancePrivateData::GetComponentData(int32 ComponentIndex) const
{
	return ComponentsData.FindByPredicate([&ComponentIndex](FCustomizableInstanceComponentData& C) { return C.ComponentIndex == ComponentIndex; });
}


UCustomizableObjectInstance::UCustomizableObjectInstance()
{
	PrivateData = CreateDefaultSubobject<UCustomizableInstancePrivateData>(FName("PrivateData"), true);
}


FCustomizableObjectInstanceDescriptor& UCustomizableObjectInstance::GetDescriptor()
{
	return Descriptor;	
}


const FCustomizableObjectInstanceDescriptor& UCustomizableObjectInstance::GetDescriptor() const
{
	return const_cast<UCustomizableObjectInstance*>(this)->GetDescriptor();
}


void UCustomizableObjectInstance::SetDescriptor(const FCustomizableObjectInstanceDescriptor& InDescriptor)
{
	const bool bInvalidatePreviousData = GetCustomizableObject() != InDescriptor.CustomizableObject;
	Descriptor = InDescriptor;
	PrivateData->ReloadParameters(this, bInvalidatePreviousData);
}


void UCustomizableInstancePrivateData::PrepareForUpdate(const TSharedPtr<FMutableOperationData>& OperationData)
{
	// Prepare LastUpdateData to allow reuse of meshes and surfaces
	InitLastUpdateData(OperationData);

	TArray<FInstanceUpdateData::FLOD>& LODs = OperationData->InstanceUpdateData.LODs;
	TArray<FInstanceUpdateData::FComponent>& Components = OperationData->InstanceUpdateData.Components;

	// Clear, reinit or create ComponentData for each component 
	TSet<uint16> ComponentIds;
	for (const FInstanceUpdateData::FLOD& LOD : LODs)
	{
		check(LOD.FirstComponent + LOD.ComponentCount <= Components.Num());

		for (uint16 ComponentIndex = 0; ComponentIndex < LOD.ComponentCount; ++ComponentIndex)
		{
			const FInstanceUpdateData::FComponent& Component = Components[ComponentIndex];
			ComponentIds.Add(Component.Id);

			if (FCustomizableInstanceComponentData* ComponentData = GetComponentData(Component.Id))
			{
				if (!Component.bGenerated || Component.bReuseMesh)
				{
					continue;
				}

				ComponentData->AnimSlotToBP.Empty();

#if WITH_EDITORONLY_DATA
				ComponentData->MeshPartPaths.Empty();
#endif
				ComponentData->Skeletons.Skeleton = nullptr;
				ComponentData->Skeletons.SkeletonIds.Empty();
				ComponentData->Skeletons.SkeletonsToMerge.Empty();
				ComponentData->PhysicsAssets.PhysicsAssetToLoad.Empty();
				ComponentData->PhysicsAssets.PhysicsAssetsToMerge.Empty();
				ComponentData->ClothingPhysicsAssetsToStream.Empty();

				continue;
			}

			FCustomizableInstanceComponentData& NewComponentData = ComponentsData.AddDefaulted_GetRef();
			NewComponentData.ComponentIndex = Component.Id;
			NewComponentData.LastMeshIdPerLOD.Init(MAX_uint64, NumLODsAvailable);
		}
	}

	// Check if a component have been removed.
	if (ComponentsData.Num() != ComponentIds.Num())
	{
		for (uint16 ComponentIndex = 0; ComponentIndex < ComponentsData.Num();)
		{
			FCustomizableInstanceComponentData& ComponentData = ComponentsData[ComponentIndex];

			if (!ComponentIds.Find(ComponentData.ComponentIndex))
			{
				ComponentsData.RemoveSingleSwap(ComponentData);
				continue;
			}

			ComponentIndex++;
		}
	}
}


#if WITH_EDITOR

void UCustomizableObjectInstance::PreEditChange(FProperty* PropertyAboutToChange)
{
	UObject::PreEditChange(PropertyAboutToChange);
	
	const FName PropertyName = PropertyAboutToChange ? PropertyAboutToChange->GetFName() : NAME_None;

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UCustomizableObjectInstance, TextureParameterDeclarations))
	{
		UDefaultImageProvider& DefaultImageProvider = UCustomizableObjectSystem::GetInstance()->GetOrCreateDefaultImageProvider();
		
		for (TObjectPtr<UTexture2D> Texture : TextureParameterDeclarations)
		{
			DefaultImageProvider.Remove(Texture);
		}
	}
}


void UCustomizableObjectInstance::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	bEditorPropertyChanged = true;

	const FName PropertyName = PropertyChangedEvent.MemberProperty ? PropertyChangedEvent.MemberProperty->GetFName() : NAME_None;

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UCustomizableObjectInstance, TextureParameterDeclarations))
	{
		UDefaultImageProvider& DefaultImageProvider = UCustomizableObjectSystem::GetInstance()->GetOrCreateDefaultImageProvider();
		
		for (TObjectPtr<UTexture2D> Texture : TextureParameterDeclarations)
		{
			DefaultImageProvider.Add(Texture);
		}

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
	BeginDestroyDelegate.Broadcast(this);
	BeginDestroyNativeDelegate.Broadcast(this);

	// Release the Live Instance ID if there it hadn't been released before
	DestroyLiveUpdateInstance();

	if (PrivateData)
	{
		if (PrivateData->StreamingHandle.IsValid() && PrivateData->StreamingHandle->IsActive())
		{
			PrivateData->StreamingHandle->CancelHandle();
		}
		PrivateData->StreamingHandle = nullptr;
	}
	
	ReleaseMutableResources(true);
	
	Super::BeginDestroy();
}


void UCustomizableObjectInstance::DestroyLiveUpdateInstance()
{
	if (PrivateData && PrivateData->LiveUpdateModeInstanceID)
	{
		// If FCustomizableObjectSystemPrivate::SSystem is nullptr it means it has already been destroyed, no point in registering an instanceID release
		// since the Mutable system has already been destroyed. Just checking UCustomizableObjectSystem::GetInstance() will try to recreate the system when
		// everything is shutting down, so it's better to check FCustomizableObjectSystemPrivate::SSystem first here
		if (FCustomizableObjectSystemPrivate::SSystem && UCustomizableObjectSystem::GetInstance() && UCustomizableObjectSystem::GetInstance()->GetPrivate())
		{
			UCustomizableObjectSystem::GetInstance()->GetPrivate()->InitInstanceIDRelease(PrivateData->LiveUpdateModeInstanceID);
			PrivateData->LiveUpdateModeInstanceID = 0;
		}
	}
}


void UCustomizableObjectInstance::ReleaseMutableResources(bool bCalledFromBeginDestroy)
{
	PrivateData->GeneratedMaterials.Empty();

	if (UCustomizableObjectSystem::IsCreated()) // Need to check this because the object might be destroyed after the CustomizableObjectSystem at shutdown
	{
		FCustomizableObjectSystemPrivate* CustomizableObjectSystem = UCustomizableObjectSystem::GetInstance()->GetPrivate();
		// Get the cache of resources of all live instances of this object
		FMutableResourceCache& Cache = CustomizableObjectSystem->GetObjectCache(GetCustomizableObject());

		for (FGeneratedTexture& Texture : PrivateData->GeneratedTextures)
		{
			if (CustomizableObjectSystem->RemoveTextureReference(Texture.Key))
			{
				// Do not release textures when called from BeginDestroy, it would produce a texture artifact in the
				// instance's remaining sk meshes and GC is being performed anyway so it will free the textures if needed
				if (!bCalledFromBeginDestroy && CustomizableObjectSystem->bReleaseTexturesImmediately)
				{
					UCustomizableInstancePrivateData::ReleaseMutableTexture(Texture.Key, Cast<UTexture2D>(Texture.Texture), Cache);
				}
			}
		}

		// Remove all references to cached Texture Parameters. Only if we're destroying the COI.
		if (bCalledFromBeginDestroy)
		{
			FUnrealMutableImageProvider* ImageProvider = UCustomizableObjectSystem::GetInstance()->GetPrivateChecked()->GetImageProviderChecked();

			for (const FName& TextureParameter : PrivateData->UpdateTextureParameters)
			{
				ImageProvider->UnCacheImage(TextureParameter, false);
			}
		}
	}

	PrivateData->GeneratedTextures.Empty();
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
			FloatParameters_DEPRECATED.Last().Uid = IntParameters_DEPRECATED[IntParametersToMove[i]].Uid;
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
	// Make sure mutable has been initialised.
	UCustomizableObjectSystem::GetInstance(); 

	Super::PostLoad();

	Descriptor.ReloadParameters();
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


bool UCustomizableObjectInstance::IsParamMultidimensional(const FString& ParamName) const
{
	return GetCustomizableObject()->IsParameterMultidimensional(ParamName);
}


// Only safe to call if the Mutable texture ref count system returns 0 and absolutely sure nobody holds a reference to the texture
void UCustomizableInstancePrivateData::ReleaseMutableTexture(const FMutableImageCacheKey& MutableTextureKey, UTexture2D* Texture, FMutableResourceCache& Cache)
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


void UCustomizableInstancePrivateData::InstanceUpdateFlags(const UCustomizableObjectInstance& Public)
{
	const UCustomizableObject* CustomizableObject = Public.GetCustomizableObject();
	
	FirstLODAvailable = CustomizableObject->LODSettings.FirstLODAvailable;
	NumLODsAvailable = CustomizableObject->GetNumLODs();

	if (CustomizableObject->LODSettings.bLODStreamingEnabled || !UCustomizableObjectSystem::GetInstance()->IsProgressiveMipStreamingEnabled())
	{
		SetCOInstanceFlags(LODsStreamingEnabled);
		ClearCOInstanceFlags(ForceGenerateAllLODs);
		NumMaxLODsToStream = CustomizableObject->LODSettings.NumLODsToStream;
	}
	else
	{
		SetCOInstanceFlags(ForceGenerateAllLODs);
		ClearCOInstanceFlags(LODsStreamingEnabled);
		NumMaxLODsToStream = 0;
	}
}


void UCustomizableInstancePrivateData::ReloadParameters(UCustomizableObjectInstance* Public, bool bInvalidatePreviousData)
{
	const UCustomizableObject* CustomizableObject = Public->GetCustomizableObject();
	if (!CustomizableObject)
	{
		// May happen when deleting assets in editor, while they are still open.
		return;
	}

	if (!CustomizableObject->IsCompiled())
	{	
		return;
	}

	InstanceUpdateFlags(*Public); // TODO Move somewhere else.

#if WITH_EDITOR
	if(!Public->ProjectorAlphaChange)
#endif
	{
		ProjectorStates.Reset();
	}
	

	if (bInvalidatePreviousData)
	{
		InvalidateGeneratedData();

#if WITH_EDITOR
		// Clear the Generated flag to trigger a new update after changing or compiling the CO,
		ClearCOInstanceFlags(Generated);
#endif
	}

	Public->GetDescriptor().ReloadParameters();
}


void UCustomizableObjectInstance::SetObject(UCustomizableObject* InObject)
{
	Descriptor.SetCustomizableObject(*InObject);
	PrivateData->ReloadParameters(this, true);
	//SetRequestedLODs(Descriptor.MinLOD, Descriptor.MaxLOD, Descriptor.RequestedLODLevels);
}


UCustomizableObject* UCustomizableObjectInstance::GetCustomizableObject() const
{
	return Descriptor.CustomizableObject;
}


// Marked as deprecated. Will be removed in future versions.
bool UCustomizableObjectInstance::GetBuildParameterDecorations() const
{
	return false;
}


// Marked as deprecated. Will be removed in future versions.
void UCustomizableObjectInstance::SetBuildParameterDecorations(const bool Value)
{
}


bool UCustomizableObjectInstance::GetBuildParameterRelevancy() const
{
#if WITH_EDITOR
	return true;
#endif
	return Descriptor.bBuildParameterRelevancy;
}


void UCustomizableObjectInstance::SetBuildParameterRelevancy(bool Value)
{
	Descriptor.SetBuildParameterRelevancy(Value);
}


int32 UCustomizableObjectInstance::GetState() const
{
	return Descriptor.GetState();
}


void UCustomizableObjectInstance::SetState(const int32 InState)
{
	const int32 OldState = GetState();
	
	Descriptor.SetState(InState);

	if (OldState != InState)
	{
		PrivateData->ProjectorStates.Reset();

		// State may change texture properties, so invalidate the texture reuse cache
		PrivateData->TextureReuseCache.Empty();
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


void UCustomizableObjectInstance::SetProjectorState(const FString& ParamName, int32 RangeIndex, EProjectorState::Type state)
{
	PrivateData->ProjectorStates.Add(TPair<FString, int32>(ParamName, RangeIndex), state);
	ProjectorStateChangedDelegate.ExecuteIfBound(ParamName);
}


void UCustomizableObjectInstance::ResetProjectorStates()
{
	PrivateData->ProjectorStates.Empty();
}


EProjectorState::Type UCustomizableObjectInstance::GetProjectorState(const FString& ParamName, int32 RangeIndex) const
{
	if (const EProjectorState::Type* state = PrivateData->ProjectorStates.Find(TPair<FString, int32>(ParamName, RangeIndex)))
	{
		return *state;
	}
	else
	{
		return EProjectorState::Hidden;
	}
}


// Marked as deprecated. Will be removed in future versions.
UTexture2D* UCustomizableObjectInstance::GetParameterDescription(const FString& ParamName, int32 DescIndex)
{
	return nullptr;
}


bool UCustomizableObjectInstance::IsParameterRelevant(int32 ParameterIndex) const
{
	// This should have been precalculated in the last update if the appropiate flag in the instance was set.
	return GetPrivate()->RelevantParameters.Contains(ParameterIndex);
}


bool UCustomizableObjectInstance::IsParameterRelevant(const FString& ParamName) const
{
	// This should have been precalculated in the last update if the appropiate flag in the instance was set.
	int32 ParameterIndexInObject = GetCustomizableObject()->FindParameter(ParamName);
	return GetPrivate()->RelevantParameters.Contains(ParameterIndexInObject);
}


void UCustomizableInstancePrivateData::PostEditChangePropertyWithoutEditor(USkeletalMesh* InSkeletalMesh)
{
	MUTABLE_CPUPROFILER_SCOPE(UCustomizableInstancePrivateData::PostEditChangePropertyWithoutEditor);

	if (!InSkeletalMesh) return;

	FSkeletalMeshRenderData* Resource = InSkeletalMesh->GetResourceForRendering();
	if (!Resource) return;

	{
		MUTABLE_CPUPROFILER_SCOPE(InitResources);
		// reinitialize resource
		InSkeletalMesh->InitResources();
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
	DoUpdateSkeletalMesh( false, false, bIgnoreCloseDist, bForceHighPriority, nullptr, nullptr);
}


void UCustomizableObjectInstance::UpdateSkeletalMeshAsyncResult(FInstanceUpdateDelegate Callback, bool bIgnoreCloseDist, bool bForceHighPriority)
{
	DoUpdateSkeletalMesh(false, false, bIgnoreCloseDist, bForceHighPriority, nullptr, &Callback);
}


EUpdateRequired UCustomizableObjectInstance::IsUpdateRequired(bool bIsCloseDistTick, bool bOnlyUpdateIfNotGenerated, bool bIgnoreCloseDist) const
{
	UCustomizableObjectSystem* System = UCustomizableObjectSystem::GetInstance();
	const UCustomizableInstancePrivateData* const Private = GetPrivate();
	
	UCustomizableObject* CustomizableObject = GetCustomizableObject();
	if (!CanUpdateInstance() || CustomizableObject->IsLocked())
	{
		return EUpdateRequired::NoUpdate;
	}

	const bool bIsGenerated = Private->HasCOInstanceFlags(Generated);
	const int32 NumGeneratedInstancesLimit = System->GetInstanceLODManagement()->GetNumGeneratedInstancesLimitFullLODs();
	const int32 NumGeneratedInstancesLimitLOD1 = System->GetInstanceLODManagement()->GetNumGeneratedInstancesLimitLOD1();
	const int32 NumGeneratedInstancesLimitLOD2 = System->GetInstanceLODManagement()->GetNumGeneratedInstancesLimitLOD2();

	if (!bIsGenerated && // Prevent generating more instances than the limit, but let updates to existing instances run normally
		NumGeneratedInstancesLimit > 0 &&
		System->GetPrivate()->GetCountAllocatedSkeletalMesh() > NumGeneratedInstancesLimit + NumGeneratedInstancesLimitLOD1 + NumGeneratedInstancesLimitLOD2)
	{
		return EUpdateRequired::NoUpdate;
	}

	const bool bShouldUpdateLODs = Private->HasCOInstanceFlags(PendingLODsUpdate);

	const bool bDiscardByDistance = Private->LastMinSquareDistFromComponentToPlayer > FMath::Square(System->GetInstanceLODManagement()->GetOnlyUpdateCloseCustomizableObjectsDist());
	const bool bLODManagementDiscard = System->GetInstanceLODManagement()->IsOnlyUpdateCloseCustomizableObjectsEnabled() &&
			bDiscardByDistance &&
			!bIgnoreCloseDist;
	
	if (Private->HasCOInstanceFlags(DiscardedByNumInstancesLimit) ||
		bLODManagementDiscard)
	{
		if (bIsGenerated && !Private->HasCOInstanceFlags(Updating))
		{
			return EUpdateRequired::Discard;		
		}
		else
		{
			return EUpdateRequired::NoUpdate;
		}
	}

	if (bIsGenerated &&
		!bShouldUpdateLODs &&
		(bOnlyUpdateIfNotGenerated || bIsCloseDistTick))
	{
		return EUpdateRequired::NoUpdate;
	}

	return EUpdateRequired::Update;
}


EQueuePriorityType UCustomizableObjectInstance::GetUpdatePriority(bool bIsCloseDistTick, bool bOnlyUpdateIfNotGenerated, bool bIgnoreCloseDist, bool bForceHighPriority) const
{
	const bool bIsGenerated = GetPrivate()->HasCOInstanceFlags(Generated);
	const bool bShouldUpdateLODs = GetPrivate()->HasCOInstanceFlags(PendingLODsUpdate);
	const bool bIsDowngradeLODUpdate = GetPrivate()->HasCOInstanceFlags(PendingLODsDowngrade);
	const bool bIsPlayerOrNearIt = GetPrivate()->HasCOInstanceFlags(UsedByPlayerOrNearIt);

	EQueuePriorityType Priority = EQueuePriorityType::Low;
	if (bForceHighPriority)
	{
		Priority = EQueuePriorityType::High;
	}
	else if (!bIsGenerated || !HasAnySkeletalMesh())
	{
		Priority = EQueuePriorityType::Med;
	}
	else if (bShouldUpdateLODs && bIsDowngradeLODUpdate)
	{
		Priority = EQueuePriorityType::Med_Low;
	}
	else if (bIsPlayerOrNearIt && bShouldUpdateLODs && !bIsDowngradeLODUpdate)
	{
		Priority = EQueuePriorityType::High;
	}
	else if (bShouldUpdateLODs && !bIsDowngradeLODUpdate)
	{
		Priority = EQueuePriorityType::Med;
	}
	else if (bIsPlayerOrNearIt)
	{
		Priority = EQueuePriorityType::High;
	}

	return Priority;
}


void UCustomizableObjectInstance::DoUpdateSkeletalMesh(bool bIsCloseDistTick, bool bOnlyUpdateIfNotGenerated, bool bIgnoreCloseDist, bool bForceHighPriority, const EUpdateRequired* OptionalUpdateRequired, FInstanceUpdateDelegate* UpdateCallback)
{
	MUTABLE_CPUPROFILER_SCOPE(UCustomizableInstancePrivateData::DoUpdateSkeletalMesh);
	check(IsInGameThread());
	check(!OptionalUpdateRequired || *OptionalUpdateRequired != EUpdateRequired::NoUpdate); // If no update is required this functions must not be called.

	UCustomizableObjectSystem* System = UCustomizableObjectSystem::GetInstance();

	if (!CanUpdateInstance())
	{
		FinishUpdateGlobal(this, EUpdateResult::ErrorDiscarded, UpdateCallback);
		return;
	}
	
	const EUpdateRequired UpdateRequired = OptionalUpdateRequired ? *OptionalUpdateRequired : IsUpdateRequired(bIsCloseDistTick, bOnlyUpdateIfNotGenerated, bIgnoreCloseDist);
	switch (UpdateRequired)
	{
	case EUpdateRequired::NoUpdate:
	{	
		FinishUpdateGlobal(this, EUpdateResult::ErrorDiscarded, UpdateCallback);
		break;
	}		
	case EUpdateRequired::Update:
	{
		EQueuePriorityType Priority = GetUpdatePriority(bIsCloseDistTick, bOnlyUpdateIfNotGenerated, bIgnoreCloseDist, bForceHighPriority);

		const uint32 InstanceId = GetUniqueID();
		const float Distance = FMath::Sqrt(GetPrivate()->LastMinSquareDistFromComponentToPlayer);
		const bool bIsPlayerOrNearIt = GetPrivate()->HasCOInstanceFlags(UsedByPlayerOrNearIt);
		UE_LOG(LogMutable, Verbose, TEXT("Started UpdateSkeletalMesh Async. of Instance %d with priority %d at dist %f bIsPlayerOrNearIt=%d, frame=%d"), InstanceId, static_cast<int32>(Priority), Distance, bIsPlayerOrNearIt, GFrameNumber);				

		UpdateDescriptorRuntimeHash = FDescriptorRuntimeHash(Descriptor);

#if WITH_EDITOR
		if (!UCustomizableObjectSystem::GetInstance()->IsCompilationDisabled())
		{
			if (SkeletalMeshStatus != ESkeletalMeshState::AsyncUpdatePending)
			{
				PreUpdateSkeletalMeshStatus = SkeletalMeshStatus;
			}
		}
#endif

		if (GetPrivate()->HasCOInstanceFlags(PendingLODsUpdate))
		{
			UE_LOG(LogMutable, Verbose, TEXT("LOD change: %d, %d -> %d, %d"), GetCurrentMinLOD(), GetCurrentMaxLOD(), GetMinLODToLoad(), GetMinLODToLoad());
		}
		
		GetPrivate()->SetCOInstanceFlags(Generated); // Will be done in UpdateSkeletalMesh_PostBeginUpdate

		// Do not do work after calling InitUpdateSkeletalMesh. This function can optimize an update and fully complete it before even exiting its scope.
		System->GetPrivate()->InitUpdateSkeletalMesh(*this, Priority, bIsCloseDistTick, UpdateCallback);
		break;
	}

	case EUpdateRequired::Discard:
	{
		System->GetPrivate()->InitDiscardResourcesSkeletalMesh(this);
		
		FinishUpdateGlobal(this, EUpdateResult::ErrorDiscarded, UpdateCallback);
		break;
	}

	default:
		check(false); // Case not implemented.
	}
}


void UCustomizableInstancePrivateData::TickUpdateCloseCustomizableObjects(UCustomizableObjectInstance& Public, FMutableInstanceUpdateMap& InOutRequestedUpdates)
{
	if (!Public.CanUpdateInstance())
	{
		return;
	}

	const EUpdateRequired UpdateRequired = Public.IsUpdateRequired(true, true, false);
	if (UpdateRequired != EUpdateRequired::NoUpdate) // Since this is done in the tick, avoid starting an update that we know for sure that would not be performed. Once started it has some performance implications that we want to avoid.
	{
		if (UpdateRequired == EUpdateRequired::Discard)
		{
			Public.DoUpdateSkeletalMesh(true, true, false, false, &UpdateRequired, nullptr);
			InOutRequestedUpdates.Remove(&Public);
		}
		else if (UpdateRequired == EUpdateRequired::Update)
		{
			EQueuePriorityType Priority = Public.GetUpdatePriority(true, true, false, false);

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


void UCustomizableInstancePrivateData::UpdateInstanceIfNotGenerated(UCustomizableObjectInstance& Public, FMutableInstanceUpdateMap& InOutRequestedUpdates)
{
	if (HasCOInstanceFlags(Generated))
	{
		return;
	}

	if (!Public.CanUpdateInstance())
	{
		return;
	}

	Public.DoUpdateSkeletalMesh(false, true, false, false, nullptr, nullptr);

	EQueuePriorityType Priority = Public.GetUpdatePriority(true, true, false, false);
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


USkeleton* UCustomizableInstancePrivateData::MergeSkeletons(UCustomizableObject& CustomizableObject, const FMutableRefSkeletalMeshData& RefSkeletalMeshData, int32 ComponentIndex)
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
		CustomizableObject.CacheMergedSkeleton(ComponentIndex, ReferencedSkeletons.SkeletonIds, FinalSkeleton);
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

		const TArray<FName>& BoneNames = CustomizableObject.GetBoneNamesArray();
		TMap<FName, int32> BonesInUse;

		const int32 MutablePhysicsBodyCount = MutablePhysics->GetBodyCount();
		BonesInUse.Reserve(MutablePhysicsBodyCount);
		for ( int32 I = 0; I < MutablePhysicsBodyCount; ++I )
		{
			const uint16 BoneNameId = MutablePhysics->GetBodyBoneId(I);
			if (BoneNames.IsValidIndex(BoneNameId))
			{
				FName BoneName = BoneNames[BoneNameId];
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

UPhysicsAsset* UCustomizableInstancePrivateData::GetOrBuildMainPhysicsAsset(
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

	const TArray<FName>& BoneNames = CustomizableObject.GetBoneNamesArray();
	TMap<FName, int32> BonesInUse;

	const int32 MutablePhysicsBodyCount = MutablePhysics->GetBodyCount();
	BonesInUse.Reserve(MutablePhysicsBodyCount);
	for ( int32 I = 0; I < MutablePhysicsBodyCount; ++I )
	{
		const uint16 BoneNameId = MutablePhysics->GetBodyBoneId(I);
		if (BoneNames.IsValidIndex(BoneNameId))
		{
			FName BoneName = BoneNames[BoneNameId];
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


bool UCustomizableInstancePrivateData::DoComponentsNeedUpdate(UCustomizableObjectInstance* Public, const TSharedPtr<FMutableOperationData>& OperationData, TArray<bool>& OutComponentNeedsUpdate, bool& bOutEmptyMesh)
{
	UCustomizableObject* CustomizableObject = Public->GetCustomizableObject();
	check(CustomizableObject);

	const int32 MaxNumLODs = OperationData->InstanceUpdateData.LODs.Num();
	const int32 MaxNumComponents = CustomizableObject->GetComponentCount();

	OutComponentNeedsUpdate.AddDefaulted(MaxNumComponents);

	// Return true if at least one mesh needs to be updated
	bool bUpdateMeshes = false;

	for (int32 ComponentIndex = 0; ComponentIndex < MaxNumComponents; ++ComponentIndex)
	{
		USkeletalMesh* SkeletalMesh = Public->SkeletalMeshes.IsValidIndex(ComponentIndex) ? Public->SkeletalMeshes[ComponentIndex] : nullptr;
		FCustomizableInstanceComponentData* ComponentData = GetComponentData(ComponentIndex);

		// Check if the mesh is/isn't generated and it should/shouldn't
		bool bMeshNeedsUpdate = (SkeletalMesh && !ComponentData) || (!SkeletalMesh && ComponentData);

		if (SkeletalMesh)
		{
			bMeshNeedsUpdate = bMeshNeedsUpdate || (SkeletalMesh->GetLODNum() - 1) != OperationData->CurrentMaxLOD;
		}

		if (ComponentData)
		{
			for (int32 LODIndex = 0; !bMeshNeedsUpdate && LODIndex < MaxNumLODs; ++LODIndex)
			{
				// Check if an LOD is generated and it shouldn't
				const bool LODCanBeGenerated = LODIndex >= OperationData->CurrentMinLOD && LODIndex <= OperationData->CurrentMaxLOD;
				bMeshNeedsUpdate = !LODCanBeGenerated && ComponentData->LastMeshIdPerLOD[LODIndex] != MAX_uint64;
			}
		}

		OutComponentNeedsUpdate[ComponentIndex] = bMeshNeedsUpdate;

		bUpdateMeshes = bUpdateMeshes || bMeshNeedsUpdate;
	}

	bool bFoundEmptyMesh = false;
	bool bFoundEmptyLOD = false;

	bool bFoundNonEmptyMesh = false;

	for (int32 LODIndex = OperationData->CurrentMinLOD; LODIndex <= OperationData->CurrentMaxLOD && LODIndex < MaxNumLODs; ++LODIndex)
	{

		const FInstanceUpdateData::FLOD& LOD = OperationData->InstanceUpdateData.LODs[LODIndex];

		for (int32 ComponentIndex = 0; ComponentIndex < LOD.ComponentCount; ++ComponentIndex)
		{
			const FInstanceUpdateData::FComponent& Component = OperationData->InstanceUpdateData.Components[LOD.FirstComponent + ComponentIndex];

			const mu::FResourceID LastMeshID = GetLastMeshId(Component.Id, LODIndex);

			if (Component.bGenerated)
			{
				if (Component.Mesh)
				{
					OutComponentNeedsUpdate[Component.Id] = OutComponentNeedsUpdate[Component.Id] || LastMeshID != Component.MeshID;

					// Don't build a degenerated mesh if something went wrong
					if (Component.Mesh->GetVertexCount() > 0)
					{
						bFoundNonEmptyMesh = true;
					}
					else if (LOD.ComponentCount == 1)
					{
						bFoundEmptyMesh = true;
					}
				}
				else if (Component.bReuseMesh)
				{
					bFoundNonEmptyMesh = true;
					check(LastMeshID != MAX_uint64);
				}
				else
				{
					OutComponentNeedsUpdate[Component.Id] = true;
				}
			}
			else
			{
				OutComponentNeedsUpdate[Component.Id] = OutComponentNeedsUpdate[Component.Id] || LastMeshID != MAX_uint64;
			}

			bUpdateMeshes = bUpdateMeshes || OutComponentNeedsUpdate[Component.Id];
		}
	}


	if (!bFoundNonEmptyMesh)
	{
		bFoundEmptyLOD = true;
		bUpdateMeshes = true;
		UE_LOG(LogMutable, Warning, TEXT("Building instance: An LOD has no mesh geometry. This cannot be handled by Unreal."));
	}

	bOutEmptyMesh = bFoundEmptyLOD || bFoundEmptyMesh;
	return bUpdateMeshes;
}


bool UCustomizableInstancePrivateData::UpdateSkeletalMesh_PostBeginUpdate0(UCustomizableObjectInstance* Public, const TSharedPtr<FMutableOperationData>& OperationData)
{
	MUTABLE_CPUPROFILER_SCOPE(UCustomizableInstancePrivateData::UpdateSkeletalMesh_PostBeginUpdate0)

	bool bEmptyMesh = false;

	TArray<bool> ComponentNeedsUpdate;
	bool bUpdateMeshes = DoComponentsNeedUpdate(Public, OperationData, ComponentNeedsUpdate, bEmptyMesh);

	// We can not handle empty meshes, clear any generated mesh and return
	if (bEmptyMesh)
	{
		Public->SkeletalMeshStatus = ESkeletalMeshState::UpdateError;
		UE_LOG(LogMutable, Warning, TEXT("Updating skeletal mesh error for CO Instance %s"), *Public->GetName());

		Public->SkeletalMeshes.Reset(); // What about all the references

		SetCOInstanceFlags(Generated);

		InvalidateGeneratedData();

		return false;
	}

	Public->SkeletalMeshStatus = ESkeletalMeshState::Correct;

	// None of the current meshes requires a mesh update. Continue to BuildMaterials
	if (!bUpdateMeshes)
	{
		SetCOInstanceFlags(Generated);
		return true;
	}

	SetCOInstanceFlags(CreatingSkeletalMesh);

	TextureReuseCache.Empty(); // Sections may have changed, so invalidate the texture reuse cache because it's indexed by section

	TArray<TObjectPtr<USkeletalMesh>> OldSkeletalMeshes = Public->SkeletalMeshes;

	bool bSuccess = true;
	bool bHasSkeletalMesh = false;

	UCustomizableObject* CustomizableObject = Public->GetCustomizableObject();
	check(CustomizableObject);

	// Initialize the maximum number of SkeletalMeshes we could possibly have. 
	Public->SkeletalMeshes.Init(nullptr, CustomizableObject->GetComponentCount());

	const int32 NumMutableComponents = OperationData->InstanceUpdateData.Components.Num();
	for (int32 ComponentIndex = 0; ComponentIndex < NumMutableComponents; ++ComponentIndex)
	{
		const FInstanceUpdateData::FComponent& Component = OperationData->InstanceUpdateData.Components[ComponentIndex];

		if (!ComponentNeedsUpdate[Component.Id])
		{
			Public->SkeletalMeshes[Component.Id] = OldSkeletalMeshes[Component.Id];
			bHasSkeletalMesh = true;
			continue;
		}

		if (!Component.bGenerated || (!Component.Mesh && !Component.bReuseMesh) || Component.SurfaceCount == 0)
		{
			continue;
		}

		const FMutableRefSkeletalMeshData* RefSkeletalMeshData = CustomizableObject->GetRefSkeletalMeshData(Component.Id);
		if (!RefSkeletalMeshData)
		{
			bSuccess = false;
			break;
		}

		// Check if we have initialized the component
		if (Public->SkeletalMeshes[Component.Id])
		{
			continue;
		}

		// Create and initialize the SkeletalMesh for this component
		MUTABLE_CPUPROFILER_SCOPE(ConstructMesh);

		Public->SkeletalMeshes[Component.Id] = NewObject<USkeletalMesh>(GetTransientPackage(), NAME_None, RF_Transient);

		USkeletalMesh* SkeletalMesh = Public->SkeletalMeshes[Component.Id];
		check(SkeletalMesh);

		INC_DWORD_STAT(STAT_MutableNumSkeletalMeshes);

		// Set up the default information any mesh from this component will have (LODArrayInfos, RenderData, Mesh settings, etc). 
		InitSkeletalMeshData(OperationData, SkeletalMesh, RefSkeletalMeshData, Component.Id);

		if (Component.Mesh)
		{
			// Construct a new skeleton, fix up ActiveBones and Bonemap arrays and recompute the RefInvMatrices
			bSuccess = BuildSkeletonData(OperationData, *SkeletalMesh, *RefSkeletalMeshData, *CustomizableObject, Component.Id);

			if (!bSuccess)
			{
				break;
			}

			// Build PhysicsAsset merging physics assets coming from SubMeshes of the newly generated Mesh
			if (mu::Ptr<const mu::PhysicsBody> MutablePhysics = Component.Mesh->GetPhysicsBody())
			{
				constexpr bool bDisallowCollisionBetweenAssets = true;
				UPhysicsAsset* PhysicsAssetResult = GetOrBuildMainPhysicsAsset(
					RefSkeletalMeshData->PhysicsAsset.Get(), MutablePhysics.get(), *CustomizableObject, Component.Id, bDisallowCollisionBetweenAssets);

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
				
				const FAnimBpOverridePhysicsAssetsInfo& Info = CustomizableObject->AnimBpOverridePhysiscAssetsInfo[PhysicsBodyExternalId];

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
			BuildMeshSockets(OperationData, SkeletalMesh, RefSkeletalMeshData, Public, Component.Mesh);
		}
		else if (Component.bReuseMesh)
		{
			// Source mesh to reuse data from
			USkeletalMesh* SrcSkeletalMesh = OldSkeletalMeshes[Component.Id];
			check(SrcSkeletalMesh);

			// Set the Skeleton from the previously generated mesh and fix up ActiveBones and Bonemap arrays from new components
			bSuccess = CopySkeletonData(OperationData, SrcSkeletalMesh, SkeletalMesh, *CustomizableObject, Component.Id);

			if (!bSuccess)
			{
				break;
			}

			// Set the PhysicsAsset from the previously generated mesh
			SkeletalMesh->SetPhysicsAsset(SrcSkeletalMesh->GetPhysicsAsset());

			// Copy Sockets from the previously generated mesh
			CopyMeshSockets(SrcSkeletalMesh, SkeletalMesh);
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

				const FCustomizableObjectExtensionData* ReferencedExtensionData = nullptr;
				switch (ExtensionOutput.Data->Origin)
				{
					case mu::ExtensionData::EOrigin::ConstantAlwaysLoaded:
					{
						check(CustomizableObject->AlwaysLoadedExtensionData.IsValidIndex(ExtensionOutput.Data->Index));
						ReferencedExtensionData = &CustomizableObject->AlwaysLoadedExtensionData[ExtensionOutput.Data->Index];
					}
					break;

					case mu::ExtensionData::EOrigin::ConstantStreamed:
					{
						check(CustomizableObject->StreamedExtensionData.IsValidIndex(ExtensionOutput.Data->Index));
						
						const FCustomizableObjectStreamedExtensionData& StreamedData =
							CustomizableObject->StreamedExtensionData[ExtensionOutput.Data->Index];

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

		bHasSkeletalMesh = true;
	}


	if (bSuccess && bHasSkeletalMesh)
	{
		MUTABLE_CPUPROFILER_SCOPE(BuildSkeletalMeshData);

		const int32 NumComponents = Public->SkeletalMeshes.Num();
		for (int32 ComponentIndex = 0; bSuccess && ComponentIndex < NumComponents; ++ComponentIndex)
		{
			if (!ComponentNeedsUpdate[ComponentIndex])
			{
				continue;
			}

			USkeletalMesh* SkeletalMesh = Public->SkeletalMeshes[ComponentIndex];

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

				ensure(SkeletalMesh->GetResourceForRendering()->LODRenderData.Num() > 0);
				ensure(SkeletalMesh->GetLODInfoArray().Num() > 0);
			}
		}
	}

	SetCOInstanceFlags(Generated);
	ClearCOInstanceFlags(CreatingSkeletalMesh); // TODO MTBL-391: Review

	if (!bSuccess)
	{
#if WITH_EDITOR
		Public->SkeletalMeshStatus = Public->PreUpdateSkeletalMeshStatus; // The mesh won't be updated, set the previous SkeletalMeshStatus to avoid losing error notifications
#endif

		Public->SkeletalMeshes.Reset();

		InvalidateGeneratedData();
	}

	return bSuccess;
}


static TAutoConsoleVariable<int32> CVarPendingReleaseSkeletalMeshMode(
	TEXT("mutable.PendingReleaseSkeletalMesh"),
	1,
	TEXT("Whether use pending for release or not \n")
	TEXT(" 0: Use GC\n")
	TEXT(" 1: Use PendingRelease\n")
	TEXT(" Default use"),
	ECVF_SetByConsole);


UCustomizableInstancePrivateData::UCustomizableInstancePrivateData()
{
	MinSquareDistFromComponentToPlayer = FLT_MAX;
	LastMinSquareDistFromComponentToPlayer = FLT_MAX;
	
	NumLODsAvailable = INT32_MAX;
	FirstLODAvailable = 0;

	NumMaxLODsToStream = MAX_MESH_LOD_COUNT;
}


UCustomizableObjectInstance* UCustomizableObjectInstance::Clone()
{
	MUTABLE_CPUPROFILER_SCOPE(UCustomizableObjectInstance::Clone);

	// Default Outer is the transient package.
	UCustomizableObjectInstance* NewInstance = NewObject<UCustomizableObjectInstance>();
	NewInstance->CopyParametersFromInstance(this);

	return NewInstance;
}


UCustomizableObjectInstance* UCustomizableObjectInstance::CloneStatic(UObject* Outer)
{
	UCustomizableObjectInstance* NewInstance = NewObject<UCustomizableObjectInstance>(Outer, GetClass());
	NewInstance->CopyParametersFromInstance(this);
	NewInstance->bShowOnlyRuntimeParameters = false;

	return NewInstance;
}


void UCustomizableObjectInstance::CopyParametersFromInstance(UCustomizableObjectInstance* Instance)
{
	SetDescriptor(Instance->GetDescriptor());
	SkeletalMeshStatus = Instance->SkeletalMeshStatus;
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
	return Descriptor.RemoveValueFromIntRange(ParamName);

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


void UCustomizableObjectInstance::SetTextureParameterSelectedOptionT(const FString& TextureParamName, UTexture2D* TextureValue, const int32 RangeIndex)
{
	Descriptor.SetTextureParameterSelectedOptionT(TextureParamName, TextureValue, RangeIndex);
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


void UCustomizableObjectInstance::SetProjectorPosition(const FString& ProjectorParamName, const FVector3f& Pos, const int32 RangeIndex)
{
	Descriptor.SetProjectorPosition(ProjectorParamName, Pos, RangeIndex);
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
	Descriptor.SetRandomValues(FMath::Rand());
}

void UCustomizableObjectInstance::SetRandomValues(const int32 InRandomizationSeed)
{
	Descriptor.SetRandomValues(InRandomizationSeed);
}


bool UCustomizableObjectInstance::LoadParametersFromProfile(int32 ProfileIndex)
{
	UCustomizableObject* CustomizableObject = GetCustomizableObject();
	if (!CustomizableObject)
	{
		return false;
	}

#if WITH_EDITOR
	if (ProfileIndex < 0 || ProfileIndex >= CustomizableObject->InstancePropertiesProfiles.Num() )
	{
		return false;
	}
	
	// This could be done only when the instance changes.
	MigrateProfileParametersToCurrentInstance(ProfileIndex);

	const FProfileParameterDat& Profile = CustomizableObject->InstancePropertiesProfiles[ProfileIndex];

	Descriptor.BoolParameters = Profile.BoolParameters;
	Descriptor.IntParameters = Profile.IntParameters;
	Descriptor.FloatParameters = Profile.FloatParameters;
	Descriptor.TextureParameters = Profile.TextureParameters;
	Descriptor.ProjectorParameters = Profile.ProjectorParameters;
	Descriptor.VectorParameters = Profile.VectorParameters;
#endif
	return true;

}

bool UCustomizableObjectInstance::SaveParametersToProfile(int32 ProfileIndex)
{
	UCustomizableObject* CustomizableObject = GetCustomizableObject();
	if (!CustomizableObject)
	{
		return false;
	}

#if WITH_EDITOR
	bSelectedProfileDirty = ProfileIndex != SelectedProfileIndex;

	if (ProfileIndex < 0 || ProfileIndex >= CustomizableObject->InstancePropertiesProfiles.Num())
	{
		return false;
	}

	FProfileParameterDat& Profile = CustomizableObject->InstancePropertiesProfiles[ProfileIndex];

	Profile.BoolParameters = Descriptor.BoolParameters;
	Profile.IntParameters = Descriptor.IntParameters;
	Profile.FloatParameters = Descriptor.FloatParameters;
	Profile.TextureParameters = Descriptor.TextureParameters;
	Profile.ProjectorParameters = Descriptor.ProjectorParameters;
	Profile.VectorParameters = Descriptor.VectorParameters;
#endif
	return true;
}

bool UCustomizableObjectInstance::MigrateProfileParametersToCurrentInstance(int32 ProfileIndex)
{
	UCustomizableObject* CustomizableObject = GetCustomizableObject();
	if (!CustomizableObject)
	{
		return false;
	}

#if WITH_EDITOR
	if (ProfileIndex < 0 || ProfileIndex >= CustomizableObject->InstancePropertiesProfiles.Num())
	{
		return false;
	}

	FProfileParameterDat& Profile = CustomizableObject->InstancePropertiesProfiles[ProfileIndex];
	FProfileParameterDat TempProfile;

	TempProfile.ProfileName = Profile.ProfileName;
	TempProfile.BoolParameters = Descriptor.BoolParameters;
	TempProfile.FloatParameters = Descriptor.FloatParameters;
	TempProfile.IntParameters = Descriptor.IntParameters;
	TempProfile.ProjectorParameters = Descriptor.ProjectorParameters;
	TempProfile.TextureParameters = Descriptor.TextureParameters;
	TempProfile.VectorParameters = Descriptor.VectorParameters;
	

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


void UCustomizableObjectInstance::SetSelectedParameterProfileDirty()
{
	UCustomizableObject* CustomizableObject = GetCustomizableObject();
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

bool UCustomizableObjectInstance::IsSelectedParameterProfileDirty() const
{
	
#if WITH_EDITOR
	return bSelectedProfileDirty && SelectedProfileIndex != INDEX_NONE;
#else
	return false;
#endif
}

//int32 UCustomizableObjectInstance::GetIntParameterNumOptions(int32 IntParamIndex)
//{
//	if (CustomizableObject && IntParamIndex>=0 && IntParamIndex<IntParameters.Num())
//	{
//		int32 ObjectParamIndex = CustomizableObject->FindParameter(IntParameters[IntParamIndex].ParameterName);
//		return CustomizableObject->GetIntParameterNumOptions(ObjectParamIndex);
//	}
//
//	return 0;
//}


//void UCustomizableObjectInstance::SetIntParameter(int32 IntParamIndex, int32 i)
//{
//	if (CustomizableObject && IntParamIndex >= 0 && IntParamIndex < IntParameters.Num())
//	{
//		int32 ObjectParamIndex = CustomizableObject->FindParameter(IntParameters[IntParamIndex].ParameterName);
//		int32 PossibleValue = CustomizableObject->GetIntParameterAvailableOptionValue(ObjectParamIndex, i);
//		IntParameters[IntParamIndex].ParameterValue = PossibleValue;
//	}
//}


//const FString & UCustomizableObjectInstance::GetIntParameterAvailableOption(int32 IntParamIndex, int32 K)
//{
//	if (CustomizableObject && IntParamIndex >= 0 && IntParamIndex<IntParameters.Num())
//	{
//		int32 ObjectParamIndex = CustomizableObject->FindParameter(IntParameters[IntParamIndex].ParameterName);
//		if (ObjectParamIndex != INDEX_NONE)
//		{
//			return CustomizableObject->GetIntParameterAvailableOption(ObjectParamIndex,K);
//		}
//	}
//
//	return s_EmptyStringCI;
//}


void UCustomizableInstancePrivateData::DiscardResourcesAndSetReferenceSkeletalMesh(UCustomizableObjectInstance* Public )
{
	if (HasCOInstanceFlags(Generated))
	{
		for (int32 Component = 0; Component < Public->SkeletalMeshes.Num(); ++Component)
		{
			if (Public->SkeletalMeshes[Component] && Public->SkeletalMeshes[Component]->IsValidLowLevel())
			{
				Public->SkeletalMeshes[Component]->ReleaseResources();
				Public->SkeletalMeshes[Component] = nullptr;
			}
		}

		Public->ReleaseMutableResources(false);
	}

	ClearCOInstanceFlags(Generated);
	
	InvalidateGeneratedData();
	
	Public->SkeletalMeshes.Reset();
	Public->DescriptorRuntimeHash = FDescriptorRuntimeHash();

	for (TObjectIterator<UCustomizableSkeletalComponent> It; It; ++It)
	{
		UCustomizableSkeletalComponent* CustomizableSkeletalComponent = *It;

		if (CustomizableSkeletalComponent && CustomizableSkeletalComponent->CustomizableObjectInstance == Public)
		{
			UCustomizableObject* CustomizableObject = Public->GetCustomizableObject();
			bool bReplaceDiscardedWithReferenceMesh = UCustomizableObjectSystem::GetInstance()->GetPrivate()->IsReplaceDiscardedWithReferenceMeshEnabled();
			CustomizableSkeletalComponent->SetSkeletalMesh(CustomizableObject && bReplaceDiscardedWithReferenceMesh ? CustomizableObject->GetRefSkeletalMesh(CustomizableSkeletalComponent->ComponentIndex) : nullptr);
		}
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


void UCustomizableObjectInstance::FinishUpdate(EUpdateResult UpdateResult, const FDescriptorRuntimeHash& InUpdatedHash)
{
	if (UpdateResult == EUpdateResult::Success)
	{
		DescriptorRuntimeHash = InUpdatedHash;
	}

	if (UpdateResult == EUpdateResult::Success)
	{
		// Call Instance updated (this) callbacks.
		UpdatedDelegate.Broadcast(this);
		UpdatedNativeDelegate.Broadcast(this);
	}
}


FDescriptorRuntimeHash UCustomizableObjectInstance::GetDescriptorRuntimeHash() const
{
	return DescriptorRuntimeHash;
}


FDescriptorRuntimeHash UCustomizableObjectInstance::GetUpdateDescriptorRuntimeHash() const
{
	return UpdateDescriptorRuntimeHash;
}


UCustomizableInstancePrivateData* UCustomizableObjectInstance::GetPrivate() const
{ 
	check(PrivateData); // Currently this is initialized in the constructor so we expect it always to exist.
	return PrivateData; 
}


void UCustomizableObjectInstance::CommitMinMaxLOD()
{
	CurrentMinLOD = Descriptor.MinLOD;
	CurrentMaxLOD = Descriptor.MaxLOD;	
}


void CopyTextureProperties(UTexture2D* Texture, const UTexture2D* SourceTexture)
{
	MUTABLE_CPUPROFILER_SCOPE(CopyTextureProperties)
		
	Texture->NeverStream = SourceTexture->NeverStream;

	Texture->SRGB = SourceTexture->SRGB;
	Texture->Filter = SourceTexture->Filter;
	Texture->LODBias = SourceTexture->LODBias;

#if WITH_EDITOR
	Texture->MipGenSettings = SourceTexture->MipGenSettings;
	Texture->CompressionNone = SourceTexture->CompressionNone;
#endif

	Texture->LODGroup = SourceTexture->LODGroup;
	Texture->AddressX = SourceTexture->AddressX;
	Texture->AddressY = SourceTexture->AddressY;
}


// The memory allocated in the function and pointed by the returned pointer is owned by the caller and must be freed. 
// If assigned to a UTexture2D, it will be freed by that UTexture2D
FTexturePlatformData* UCustomizableInstancePrivateData::MutableCreateImagePlatformData(mu::Ptr<const mu::Image> MutableImage, int32 OnlyLOD, uint16 FullSizeX, uint16 FullSizeY)
{
	int32 SizeX = FMath::Max(MutableImage->GetSize()[0], FullSizeX);
	int32 SizeY = FMath::Max(MutableImage->GetSize()[1], FullSizeY);

	if (SizeX <= 0 || SizeY <= 0)
	{
		UE_LOG(LogMutable, Warning, TEXT("Invalid parameters specified for UCustomizableInstancePrivateData::MutableCreateImagePlatformData()"));
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
	FCustomizableObjectSystemPrivate* SystemPrivate = System ? System->GetPrivate() : nullptr;

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

	mu::FImageOperator ImOp = mu::FImageOperator::GetDefault();

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
	const uint8_t* MutableData = MutableImage->GetMipData( FirstLOD );

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

			uint32 SourceDataSize = MutableImage->GetLODDataSize(MipLevelMutable);
			uint32 DestDataSize = (MutableFormat == mu::EImageFormat::IF_RGB_UBYTE)
				? (SourceDataSize/3*4)
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

			MutableData += SourceDataSize;

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



void UCustomizableInstancePrivateData::ConvertImage(UTexture2D* Texture, mu::ImagePtrConst MutableImage, const FMutableModelImageProperties& Props, int OnlyLOD, int32 ExtractChannel )
{
	MUTABLE_CPUPROFILER_SCOPE(UCustomizableInstancePrivateData::ConvertImage);

	SetTexturePropertiesFromMutableImageProps(Texture, Props, false);

	mu::EImageFormat MutableFormat = MutableImage->GetFormat();

	// Extract a single channel, if requested.
	if (ExtractChannel >= 0)
	{
		mu::FImageOperator ImOp = mu::FImageOperator::GetDefault();

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

		for (int LODIndex = 0; LODIndex < Converted->GetLODCount(); ++LODIndex)
		{
			int32 pixelCount = MutableImage->GetLODDataSize(LODIndex)/3;
			const uint8_t* pSource = MutableImage->GetMipData(LODIndex);
			uint8_t* pTarget = Converted->GetMipData(LODIndex);
			for (int32 p = 0; p < pixelCount; ++p)
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
		int32 pixelCount = MutableImage->GetSizeX() * MutableImage->GetSizeY();

		const uint8_t* pSource = MutableImage->GetData();
		uint8_t* pTarget = Converted->GetData();
		for (int32 p = 0; p<pixelCount; ++p)
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


void UCustomizableInstancePrivateData::InitSkeletalMeshData(const TSharedPtr<FMutableOperationData>& OperationData, USkeletalMesh* SkeletalMesh, const FMutableRefSkeletalMeshData* RefSkeletalMeshData, int32 ComponentIndex)
{
	MUTABLE_CPUPROFILER_SCOPE(UCustomizableInstancePrivateData::InitSkeletalMesh);

	check(SkeletalMesh);
	check(RefSkeletalMeshData);

	// Mutable skeletal meshes are generated dynamically in-game and cannot be streamed from disk
	SkeletalMesh->NeverStream = 1;

	SkeletalMesh->SetImportedBounds(RefSkeletalMeshData->Bounds);
	SkeletalMesh->SetPostProcessAnimBlueprint(RefSkeletalMeshData->PostProcessAnimInst.Get());
	SkeletalMesh->SetShadowPhysicsAsset(RefSkeletalMeshData->ShadowPhysicsAsset.Get());

	// Set Min LOD
	SkeletalMesh->SetMinLod(FirstLODAvailable);
	SkeletalMesh->SetQualityLevelMinLod(FirstLODAvailable);

	SkeletalMesh->SetHasBeenSimplified(false);
	SkeletalMesh->SetHasVertexColors(false);

	// Set the default Physics Assets
	SkeletalMesh->SetPhysicsAsset(RefSkeletalMeshData->PhysicsAsset.Get());
	SkeletalMesh->SetEnablePerPolyCollision(RefSkeletalMeshData->Settings.bEnablePerPolyCollision);

	// Asset User Data
	for (const FMutableRefAssetUserData& MutAssetUserData : RefSkeletalMeshData->AssetUserData)
	{
		if (MutAssetUserData.AssetUserData)
		{
			SkeletalMesh->AddAssetUserData(MutAssetUserData.AssetUserData);
		}
	}

	// Allocate resources for rendering and add LOD Info
	{
		MUTABLE_CPUPROFILER_SCOPE(InitSkeletalMesh_AddLODData);
		SkeletalMesh->AllocateResourceForRendering();

		FSkeletalMeshRenderData* RenderData = SkeletalMesh->GetResourceForRendering();
		for (int32 LODIndex = 0; LODIndex <= OperationData->CurrentMaxLOD; ++LODIndex)
		{
			RenderData->LODRenderData.Add(new FSkeletalMeshLODRenderData());

			const FMutableRefLODData& LODData = RefSkeletalMeshData->LODData[LODIndex];

			FSkeletalMeshLODInfo& LODInfo = SkeletalMesh->AddLODInfo();
			LODInfo.ScreenSize = LODData.LODInfo.ScreenSize;
			LODInfo.LODHysteresis = LODData.LODInfo.LODHysteresis;
			LODInfo.bSupportUniformlyDistributedSampling = LODData.LODInfo.bSupportUniformlyDistributedSampling;
			LODInfo.bAllowCPUAccess = LODData.LODInfo.bAllowCPUAccess;

			// Disable LOD simplification when baking instances
			LODInfo.ReductionSettings.NumOfTrianglesPercentage = 1.f;
			LODInfo.ReductionSettings.NumOfVertPercentage = 1.f;

#if WITH_EDITORONLY_DATA
			LODInfo.BuildSettings.bUseFullPrecisionUVs = true;
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


bool UCustomizableInstancePrivateData::BuildSkeletonData(const TSharedPtr<FMutableOperationData>& OperationData, USkeletalMesh& SkeletalMesh, const FMutableRefSkeletalMeshData& RefSkeletalMeshData, UCustomizableObject& CustomizableObject, int32 ComponentIndex)
{
	MUTABLE_CPUPROFILER_SCOPE(UCustomizableInstancePrivateData::BuildSkeletonData);

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

		const TArray<FName>& BoneNames = CustomizableObject.GetBoneNamesArray();

		// Ensure all the required bones are present in the skeleton
		for (int32 BoneIndex = 0; BoneIndex < MutBoneCount; ++BoneIndex)
		{
			const uint16 BoneId = MutSkeletonData.BoneIds[BoneIndex];
			check(BoneNames.IsValidIndex(BoneId));

			const FName BoneName = BoneNames[BoneId]; 
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


bool UCustomizableInstancePrivateData::CopySkeletonData(const TSharedPtr<FMutableOperationData>& OperationData, USkeletalMesh* SrcSkeletalMesh, USkeletalMesh* DestSkeletalMesh, const UCustomizableObject& CustomizableObject, int32 ComponentIndex)
{
	MUTABLE_CPUPROFILER_SCOPE(UCustomizableInstancePrivateData::CopySkeletonData);

	check(SrcSkeletalMesh);
	check(DestSkeletalMesh);

	const TObjectPtr<USkeleton> Skeleton = SrcSkeletalMesh->GetSkeleton();
	if (!Skeleton)
	{
		return false;
	}

	DestSkeletalMesh->SetSkeleton(Skeleton);
	DestSkeletalMesh->SetRefSkeleton(Skeleton->GetReferenceSkeleton());
	DestSkeletalMesh->SetRefBasesInvMatrix(SrcSkeletalMesh->GetRefBasesInvMatrix());

	const FReferenceSkeleton& RefSkeleton = DestSkeletalMesh->GetRefSkeleton();

	// Check that the bones we need are present in the current Skeleton
	FInstanceUpdateData::FSkeletonData& MutSkeletonData = OperationData->InstanceUpdateData.Skeletons[ComponentIndex];
	const int32 MutBoneCount = MutSkeletonData.BoneIds.Num();
	
	TMap<uint16, uint16> BoneToFinalBoneIndexMap;
	BoneToFinalBoneIndexMap.Reserve(MutBoneCount);

	{
		MUTABLE_CPUPROFILER_SCOPE(CopySkeletonData_CheckForMissingBones);

		const TArray<FName>& BoneNames = CustomizableObject.GetBoneNamesArray();

		// Ensure all the required bones are present in the skeleton
		for (int32 BoneIndex = 0; BoneIndex < MutBoneCount; ++BoneIndex)
		{
			const uint16 BoneId = MutSkeletonData.BoneIds[BoneIndex];
			check(BoneNames.IsValidIndex(BoneId));

			const FName BoneName = BoneNames[BoneId];
			check(BoneName != NAME_None);

			const int32 SourceBoneIndex = RefSkeleton.FindRawBoneIndex(BoneName);
			
			if (SourceBoneIndex == INDEX_NONE)
			{
				// Merged skeleton is missing some bones! This happens if one of the skeletons involved in the merge is discarded due to being incompatible with the rest
				// or if the source mesh is not in sync with the skeleton. 
				UE_LOG(LogMutable, Warning, TEXT("Building instance: generated mesh has a bone [%s] not present in the reference mesh [%s]. Failing to generate mesh. "),
					*BoneName.ToString(), *DestSkeletalMesh->GetName());
				return false;
			}

			BoneToFinalBoneIndexMap.Add(BoneId, SourceBoneIndex);
		}
	}

	{
		MUTABLE_CPUPROFILER_SCOPE(CopySkeletonData_FixBoneIndices);

		// Fix up BoneMaps and ActiveBones indices
		for (FInstanceUpdateData::FComponent& Component : OperationData->InstanceUpdateData.Components)
		{
			if (Component.Id != ComponentIndex)
			{
				continue;
			}

			for (uint32 BoneMapIndex = Component.FirstBoneMap; BoneMapIndex < Component.FirstBoneMap + Component.BoneMapCount; ++BoneMapIndex)
			{
				const uint16 BoneId = OperationData->InstanceUpdateData.BoneMaps[BoneMapIndex];
				OperationData->InstanceUpdateData.BoneMaps[BoneMapIndex] = BoneToFinalBoneIndexMap[BoneId];
			}

			TArray<uint16> UniqueActiveBones;
			for (const uint16 BoneId : Component.ActiveBones)
			{
				UniqueActiveBones.AddUnique(BoneToFinalBoneIndexMap[BoneId]);
			}

			Exchange(Component.ActiveBones, UniqueActiveBones);
			Component.ActiveBones.Sort();
		}
	}

	return true;
}


void UCustomizableInstancePrivateData::BuildMeshSockets(const TSharedPtr<FMutableOperationData>& OperationData, USkeletalMesh* SkeletalMesh, const FMutableRefSkeletalMeshData* RefSkeletalMeshData, UCustomizableObjectInstance* Public, mu::MeshPtrConst MutableMesh)
{
	// Build mesh sockets.
	MUTABLE_CPUPROFILER_SCOPE(UCustomizableInstancePrivateData::BuildMeshSockets);

	check(SkeletalMesh);
	check(RefSkeletalMeshData);
	check(Public);

	const uint32 SocketCount = RefSkeletalMeshData->Sockets.Num();

	TArray<TObjectPtr<USkeletalMeshSocket>>& Sockets = SkeletalMesh->GetMeshOnlySocketList();
	Sockets.Empty(SocketCount);
	TMap<FName, TTuple<int32, int32>> SocketMap; // Maps Socket name to Sockets Array index and priority
	
	// Add sockets used by the SkeletalMesh of reference.
	{
		MUTABLE_CPUPROFILER_SCOPE(BuildMeshSockets_RefMeshSockets);
	
		for (uint32 SocketIndex = 0; SocketIndex < SocketCount; ++SocketIndex)
		{
			const FMutableRefSocket& RefSocket = RefSkeletalMeshData->Sockets[SocketIndex];

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

		const UCustomizableObject* CustomizableObject = Public->GetCustomizableObject();
		check(CustomizableObject);

		for (int32 TagIndex = 0; TagIndex < MutableMesh->GetTagCount(); ++TagIndex)
		{
			FString Tag = MutableMesh->GetTag(TagIndex);

			if (Tag.RemoveFromStart("__Socket:"))
			{
				check(Tag.IsNumeric());
				const int32 MutableSocketIndex = FCString::Atoi(*Tag);

				if (CustomizableObject->SocketArray.IsValidIndex(MutableSocketIndex))
				{
					const FMutableRefSocket& MutableSocket = CustomizableObject->SocketArray[MutableSocketIndex];
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
}


void UCustomizableInstancePrivateData::CopyMeshSockets(USkeletalMesh* SrcSkeletalMesh, USkeletalMesh* DestSkeletalMesh)
{
	// Copy mesh sockets.
	MUTABLE_CPUPROFILER_SCOPE(UCustomizableInstancePrivateData::CopyMeshSockets);

	check(SrcSkeletalMesh);
	check(DestSkeletalMesh);

	const TArray<USkeletalMeshSocket*>& SrcSockets = SrcSkeletalMesh->GetMeshOnlySocketList();
	TArray<TObjectPtr<USkeletalMeshSocket>>& DestSockets = DestSkeletalMesh->GetMeshOnlySocketList();
	DestSockets.Empty(SrcSockets.Num());

	for (const USkeletalMeshSocket* SrcSocket : SrcSockets)
	{
		if (SrcSocket)
		{
			USkeletalMeshSocket* NewSocket = NewObject<USkeletalMeshSocket>(DestSkeletalMesh);


			NewSocket->SocketName = SrcSocket->SocketName;
			NewSocket->BoneName = SrcSocket->BoneName;

			NewSocket->RelativeLocation = SrcSocket->RelativeLocation;
			NewSocket->RelativeRotation = SrcSocket->RelativeRotation;
			NewSocket->RelativeScale = SrcSocket->RelativeScale;

			NewSocket->bForceAlwaysAnimated = SrcSocket->bForceAlwaysAnimated;
			DestSockets.Add(NewSocket);

		}
	}
}


void UCustomizableInstancePrivateData::BuildOrCopyElementData(const TSharedPtr<FMutableOperationData>& OperationData, USkeletalMesh* SkeletalMesh, UCustomizableObjectInstance* CustomizableObjectInstance, int32 ComponentIndex)
{
	MUTABLE_CPUPROFILER_SCOPE(UCustomizableInstancePrivateData::BuildOrCopyElementData);

	for (int32 LODIndex = OperationData->CurrentMaxLOD; LODIndex >= FirstLODAvailable; --LODIndex)
	{
		const FInstanceUpdateData::FLOD& LOD = OperationData->InstanceUpdateData.LODs[LODIndex];

		if (!LOD.ComponentCount)
		{
			continue;
		}

		const FInstanceUpdateData::FComponent& Component = OperationData->InstanceUpdateData.Components[LOD.FirstComponent + ComponentIndex];

		if (!Component.bGenerated || Component.bReuseMesh)
		{
			continue;
		}

		for (int32 SurfaceIndex = 0; SurfaceIndex < Component.SurfaceCount; ++SurfaceIndex)
		{
			new(SkeletalMesh->GetResourceForRendering()->LODRenderData[LODIndex].RenderSections) FSkelMeshRenderSection();
		}
	}
}


void UCustomizableInstancePrivateData::BuildOrCopyMorphTargetsData(const TSharedPtr<FMutableOperationData>& OperationData, USkeletalMesh* SkeletalMesh, const USkeletalMesh* LastUpdateSkeletalMesh, UCustomizableObjectInstance* CustomizableObjectInstance, int32 ComponentIndex)
{
	MUTABLE_CPUPROFILER_SCOPE(UCustomizableInstancePrivateData::BuildOrCopyMorphTargetsData);

	if (!SkeletalMesh || !CustomizableObjectInstance)
	{
		return;
	}

	const UCustomizableObject* CustomizableObject = CustomizableObjectInstance->GetCustomizableObject();
	const TArray<FMorphTargetInfo>& ContributingMorphTargetsInfo = CustomizableObject->ContributingMorphTargetsInfo;
	const TArray<FMorphTargetVertexData>& MorphTargetReconstructionData = CustomizableObject->MorphTargetReconstructionData;

	if (!(MorphTargetReconstructionData.Num() && ContributingMorphTargetsInfo.Num()))
	{
		return;
	}

	TArray<int32> SectionMorphTargetVertices;
	SectionMorphTargetVertices.SetNumZeroed(ContributingMorphTargetsInfo.Num());

	SkeletalMesh->GetMorphTargets().Empty(ContributingMorphTargetsInfo.Num());
	for (const FMorphTargetInfo& MorphTargetInfo : ContributingMorphTargetsInfo)
	{
		UMorphTarget* NewMorphTarget = NewObject<UMorphTarget>(SkeletalMesh, MorphTargetInfo.Name);

		NewMorphTarget->BaseSkelMesh = SkeletalMesh;
		NewMorphTarget->GetMorphLODModels().SetNum(MorphTargetInfo.LodNum);
		SkeletalMesh->GetMorphTargets().Add(NewMorphTarget);
	}

	const int32 SkeletalMeshMorphTargetsNum = SkeletalMesh->GetMorphTargets().Num();

	int32 LastValidLODIndex = OperationData->CurrentMaxLOD;
	for (int32 LODIndex = OperationData->CurrentMaxLOD; LODIndex >= FirstLODAvailable; --LODIndex)
	{
		const FInstanceUpdateData::FLOD& LOD = OperationData->InstanceUpdateData.LODs[LODIndex];

		if (LODIndex >= OperationData->CurrentMinLOD && OperationData->InstanceUpdateData.Components[OperationData->InstanceUpdateData.LODs[LODIndex].FirstComponent + ComponentIndex].bGenerated)
		{
			LastValidLODIndex = LODIndex;
		}

		const FInstanceUpdateData::FComponent& Component = OperationData->InstanceUpdateData.Components[OperationData->InstanceUpdateData.LODs[LastValidLODIndex].FirstComponent + ComponentIndex];

		if (Component.Mesh)
		{
			MUTABLE_CPUPROFILER_SCOPE(BuildOrCopyMorphTargetsData_Build);

			const mu::FMeshBufferSet& MeshSet = Component.Mesh->GetVertexBuffers();

			int32 VertexMorphsInfoIndexBufferIndex, VertexMorphsInfoIndexBufferChannel;
			MeshSet.FindChannel(mu::MBS_OTHER, 0, &VertexMorphsInfoIndexBufferIndex, &VertexMorphsInfoIndexBufferChannel);

			int32 VertexMorphsCountBufferIndex, VertexMorphsCountBufferChannel;
			MeshSet.FindChannel(mu::MBS_OTHER, 1, &VertexMorphsCountBufferIndex, &VertexMorphsCountBufferChannel);

			if (VertexMorphsInfoIndexBufferIndex < 0 || VertexMorphsCountBufferIndex < 0)
			{
				continue;
			}

			const int32* const VertexMorphsInfoIndexBuffer = reinterpret_cast<const int32*>(MeshSet.GetBufferData(VertexMorphsInfoIndexBufferIndex));
			TArrayView<const int32> VertexMorphsInfoIndexView(VertexMorphsInfoIndexBuffer, MeshSet.GetElementCount());

			const int32* const VertexMorphsCountBuffer = reinterpret_cast<const int32*>(MeshSet.GetBufferData(VertexMorphsCountBufferIndex));
			TArrayView<const int32> VertexMorphsCountView(VertexMorphsCountBuffer, MeshSet.GetElementCount());

			const int32 SurfaceCount = Component.Mesh->GetSurfaceCount();
			for (int32 Section = 0; Section < SurfaceCount; ++Section)
			{
				// Reset SectionMorphTargets.
				for (auto& Elem : SectionMorphTargetVertices)
				{
					Elem = 0;
				}

				int FirstVertex, VerticesCount, FirstIndex, IndiciesCount;
				Component.Mesh->GetSurface(Section, &FirstVertex, &VerticesCount, &FirstIndex, &IndiciesCount, nullptr, nullptr);

				for (int32 VertexIdx = FirstVertex; VertexIdx < FirstVertex + VerticesCount; ++VertexIdx)
				{
					const int32 MorphCount = VertexMorphsCountView[VertexIdx];
					if (MorphCount <= 0)
					{
						continue;
					}

					TArrayView<const FMorphTargetVertexData> MorphsVertexDataView(&(MorphTargetReconstructionData[VertexMorphsInfoIndexView[VertexIdx]]), MorphCount);
					for (const FMorphTargetVertexData& SourceVertex : MorphsVertexDataView)
					{
						// check(SkeletalMeshMorphTargetsNum <= SourceVertex.MorphIndex);
						if (SkeletalMeshMorphTargetsNum <= SourceVertex.MorphIndex)
						{
							continue;
						}

						FMorphTargetLODModel& DestMorphLODModel = SkeletalMesh->GetMorphTargets()[SourceVertex.MorphIndex]->GetMorphLODModels()[LODIndex];

						DestMorphLODModel.Vertices.Emplace(
							FMorphTargetDelta{ SourceVertex.PositionDelta, SourceVertex.TangentZDelta, static_cast<uint32>(VertexIdx) });

						++SectionMorphTargetVertices[SourceVertex.MorphIndex];

					}
				}

				const int32 SectionMorphTargetsNum = SectionMorphTargetVertices.Num();
				for (int32 MorphIdx = 0; MorphIdx < SectionMorphTargetsNum; ++MorphIdx)
				{
					if (SectionMorphTargetVertices[MorphIdx] > 0)
					{
						FMorphTargetLODModel& MorphTargetLodModel = SkeletalMesh->GetMorphTargets()[MorphIdx]->GetMorphLODModels()[LODIndex];

						MorphTargetLodModel.SectionIndices.Add(Section);
						MorphTargetLodModel.NumVertices = SectionMorphTargetVertices[MorphIdx];
					}
				}
			}
		}
		else
		{
			MUTABLE_CPUPROFILER_SCOPE(BuildOrCopyMorphTargetsData_Copy);

			// Copy data from the last valid LOD generated from either the current mesh or the previous mesh
			const FInstanceUpdateData::FLOD& LastLOD = OperationData->InstanceUpdateData.LODs[LastValidLODIndex];
			const FInstanceUpdateData::FComponent& LastComponent = OperationData->InstanceUpdateData.Components[LastLOD.FirstComponent + ComponentIndex];

			const USkeletalMesh* SrcSkeletalMesh = LastComponent.bReuseMesh ? LastUpdateSkeletalMesh : SkeletalMesh;
			if (!SrcSkeletalMesh)
			{
				check(false);
				return;
			}

			const int32 NumMorphTargets = SrcSkeletalMesh->GetMorphTargets().Num();
			for (int32 MorphTargetIndex = 0; MorphTargetIndex < NumMorphTargets; ++MorphTargetIndex)
			{
				SkeletalMesh->GetMorphTargets()[MorphTargetIndex]->GetMorphLODModels()[LODIndex] = SrcSkeletalMesh->GetMorphTargets()[MorphTargetIndex]->GetMorphLODModels()[LastValidLODIndex];
			}
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


void UCustomizableInstancePrivateData::BuildOrCopyClothingData(const TSharedPtr<FMutableOperationData>&OperationData, USkeletalMesh * SkeletalMesh, const USkeletalMesh* LastUpdateSkeletalMesh, UCustomizableObjectInstance * CustomizableObjectInstance, int32 ComponentIndex)
{
	MUTABLE_CPUPROFILER_SCOPE(UCustomizableInstancePrivateData::BuildOrCopyClothingData);

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

		for (int32 LODIndex = OperationData->CurrentMinLOD; LODIndex <= OperationData->CurrentMaxLOD; ++LODIndex)
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


			if (!Component.Mesh && Component.bReuseMesh)
			{
				// TODO PERE: Implement copying LODRenderDatas with clothing
				unimplemented()
			}


			if (mu::MeshPtrConst MutableMesh = Component.Mesh)
			{
				const mu::FMeshBufferSet& MeshSet = MutableMesh->GetVertexBuffers();

				const mu::FMeshBufferSet& IndicesSet = MutableMesh->GetIndexBuffers();

				// Semantics index may vary depending on whether realtime morph targets are enabled.
				const int32 ClothingDataBufferIndex = [&MeshSet]()
				{
					int32 BufferIndex, Channel;
					MeshSet.FindChannel(mu::MBS_OTHER, 2, &BufferIndex, &Channel);

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
					MutableMesh->GetSurface(Section, &FirstVertex, &VerticesCount, &FirstIndex, &IndicesCount, nullptr, nullptr);

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

					// TODO Pere: Remove once we support the copy of LODRenderDatas with clothing
					LastUpdateData.Components[LOD.FirstComponent + ComponentIndex].bGenerated = false;
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
				DstAssetData.LodMap.Init( INDEX_NONE, OperationData->CurrentMaxLOD + 1 );

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
	TArray<TArray<TArray<int32>>> PhysicsSectionLodsIndicesRemaps;	

	check( SectionsWithCloth.Num() > 0 );
	FSectionWithClothData* MaxSection = Algo::MaxElement(SectionsWithCloth, 
			[](const FSectionWithClothData& A, const FSectionWithClothData& B) { return  A.Section < B.Section; } );
	
	PhysicsSectionLodsIndicesRemaps.SetNum(MaxSection->Section + 1);
		
	for (FSectionWithClothData& SectionLods : SectionsWithCloth)
	{
		const int32 SectionLodNum = PhysicsSectionLodsIndicesRemaps[SectionLods.Section].Num();

		PhysicsSectionLodsIndicesRemaps[SectionLods.Section].SetNum(
				FMath::Max( SectionLodNum, SectionLods.ClothAssetLodIndex + 1 ));
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

			TArray<int32>& IndexMap = PhysicsSectionLodsIndicesRemaps[SectionWithCloth.Section][SectionWithCloth.ClothAssetLodIndex];
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

			const bool bNeedsTransitionUpData = SectionWithCloth.Lod - 1 >= 0;
			if (bNeedsTransitionUpData)
			{
				NewLodData.TransitionUpSkinData.SetNum(SrcLodData.TransitionUpSkinData.Num() ? NewPhysicalMeshVerticesNum : 0);	
				CopyIfUsed(NewLodData.TransitionUpSkinData, SrcLodData.TransitionUpSkinData);
			}

			check(SectionWithCloth.Section < PhysicsSectionLodsIndicesRemaps.Num());
			const bool bNeedsTransitionDownData = SectionWithCloth.Lod + 1 < PhysicsSectionLodsIndicesRemaps[SectionWithCloth.Section].Num();
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
			NewPhysicalMeshIndices.SetNum(TrimAndRemapTriangles(NewPhysicalMeshIndices, SrcPhysicalMeshIndices), false);
		
			const auto TrimAndRemapIndices = [&IndexMap](TArray<uint32>& Dst, const TArray<uint32>& Src) -> int32
			{	
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
				for (int32 Idx = 0; Idx < SrcNumElems; ++Idx)
				{
					const int32 MappedIdx = IndexMap[Src[Idx]];

					if (MappedIdx >= 0)
					{
						Dst[DstNumElems++] = MappedIdx;
					}
				}

				return DstNumElems;
			};

			const TArray<uint32>& SrcSelfCollisionIndices = SrcLodData.PhysicalMeshData.SelfCollisionIndices;
			TArray<uint32>& NewSelfCollisionIndices = NewLodData.PhysicalMeshData.SelfCollisionIndices;
			NewSelfCollisionIndices.SetNum(SrcSelfCollisionIndices.Num());
			NewSelfCollisionIndices.SetNum(TrimAndRemapIndices(NewSelfCollisionIndices, SrcSelfCollisionIndices), false);
						
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
							constexpr bool bAllowShrinking = false; 
							Dst.Tethers.RemoveAt( Dst.Tethers.Num() - 1, 1, bAllowShrinking );
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
				check(SectionWithCloth.Section < PhysicsSectionLodsIndicesRemaps.Num() &&
					  SectionWithCloth.ClothAssetLodIndex + 1 < PhysicsSectionLodsIndicesRemaps[SectionWithCloth.Section].Num() );
				
				TArray<int32>& IndexMap = PhysicsSectionLodsIndicesRemaps[SectionWithCloth.Section][SectionWithCloth.ClothAssetLodIndex + 1];

				if (IndexMap.Num())
				{
					RemapTransitionMeshToMeshVertData( NewLodData.TransitionDownSkinData, IndexMap );
				}
			}
			
			if (NewLodData.TransitionUpSkinData.Num() > 0)
			{
				check( SectionWithCloth.ClothAssetLodIndex - 1 >= 0 )
				
				check(SectionWithCloth.Section < PhysicsSectionLodsIndicesRemaps.Num() &&
					  SectionWithCloth.ClothAssetLodIndex - 1 < PhysicsSectionLodsIndicesRemaps[SectionWithCloth.Section].Num());
				
				TArray<int32>& IndexMap = PhysicsSectionLodsIndicesRemaps[SectionWithCloth.Section][SectionWithCloth.ClothAssetLodIndex - 1];
			
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

			const TArray< TArray<int32> >& SectionIndexRemaps = PhysicsSectionLodsIndicesRemaps[SectionWithCloth.Section];

			if (SectionWithCloth.ClothAssetLodIndex < SectionIndexRemaps.Num() - 1)
			{
				const TArray<int32>& IndexMap = PhysicsSectionLodsIndicesRemaps[SectionWithCloth.Section][SectionWithCloth.ClothAssetLodIndex + 1];
				
				const FMeshPhysicsDesc TransitionDownTarget {  
					NewClothingAssetData.LodData[SectionWithCloth.ClothAssetLodIndex + 1].PhysicalMeshData.Vertices,
					NewClothingAssetData.LodData[SectionWithCloth.ClothAssetLodIndex + 1].PhysicalMeshData.Normals,
					NewClothingAssetData.LodData[SectionWithCloth.ClothAssetLodIndex + 1].PhysicalMeshData.Indices };
					
				RecreateTransitionData( TransitionDownTarget, CurrentPhysicsMesh, IndexMap, NewLodData.TransitionDownSkinData );
			}
			
			if (SectionWithCloth.ClothAssetLodIndex > 0)
			{
				const TArray<int32>& IndexMap = PhysicsSectionLodsIndicesRemaps[SectionWithCloth.Section][SectionWithCloth.ClothAssetLodIndex - 1];
				
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

		for (int32 LODIndex = OperationData->CurrentMinLOD; LODIndex <= OperationData->CurrentMaxLOD; ++LODIndex)
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
					MutableMesh->GetSurface(Section, &FirstVertex, &VerticesCount, &FirstIndex, &IndicesCount, nullptr, nullptr);

					if (VerticesCount == 0 || IndicesCount == 0 )
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
		for (int32 LODIndex = OperationData->CurrentMinLOD; LODIndex <= OperationData->CurrentMaxLOD; ++LODIndex)
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
		
			NewClothingAssets[I] = NewObject<UCustomizableObjectClothingAsset>( SkeletalMesh, NewClothingAssetsData[I].Name );
			
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


bool UCustomizableInstancePrivateData::BuildOrCopyRenderData(const TSharedPtr<FMutableOperationData>& OperationData, USkeletalMesh* SkeletalMesh, const USkeletalMesh* LastUpdateSkeletalMesh, UCustomizableObjectInstance* Public, int32 ComponentIndex)
{
	MUTABLE_CPUPROFILER_SCOPE(UCustomizableInstancePrivateData::BuildOrCopyRenderData)

		UCustomizableObject* CustomizableObject = Public->GetCustomizableObject();
	check(CustomizableObject);

	int32 LastValidLODIndex = OperationData->CurrentMaxLOD;
	for (int32 LODIndex = OperationData->CurrentMaxLOD; LODIndex >= FirstLODAvailable; --LODIndex)
	{
		MUTABLE_CPUPROFILER_SCOPE(BuildOrCopyRenderData_LODLoop);

		const FInstanceUpdateData::FLOD& LOD = OperationData->InstanceUpdateData.LODs[LODIndex];

		bool bCopyComponent = false;
		if (!LOD.ComponentCount)
		{
			// Interrupt the generation if the LOD is empty and it should have been generated.
			if (LODIndex >= OperationData->CurrentMinLOD && LODIndex <= OperationData->CurrentMaxLOD)
			{
				UE_LOG(LogMutable, Warning, TEXT("Building instance: generated mesh [%s] has LOD [%d] with no component.")
					, *SkeletalMesh->GetName()
					, LODIndex);

				// End with failure
				return false;
			}

			// LODIndex is below the CurrentMinLOD copy the data from the LastValidLODIndex
			bCopyComponent = true;
		}

		else
		{
			check(ComponentIndex < LOD.ComponentCount);
			FInstanceUpdateData::FComponent& Component = OperationData->InstanceUpdateData.Components[LOD.FirstComponent + ComponentIndex];

			if (!Component.bGenerated || Component.bReuseMesh)
			{
				bCopyComponent = true;
			}
			else
			{
				LastValidLODIndex = LODIndex;
			}
		}


		if (bCopyComponent)
		{
			// Copy data from the last valid LOD generated from either the current mesh or the previous mesh
			const FInstanceUpdateData::FLOD& LastValidLOD = OperationData->InstanceUpdateData.LODs[LastValidLODIndex];
			const FInstanceUpdateData::FComponent& LastValidComponent = OperationData->InstanceUpdateData.Components[LastValidLOD.FirstComponent + ComponentIndex];

			const USkeletalMesh* SrcSkeletalMesh = LastValidComponent.bReuseMesh ? LastUpdateSkeletalMesh : SkeletalMesh;
			if (!SrcSkeletalMesh)
			{
				check(false);
				return false;
			}

			TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*FString::Printf(TEXT("BuildOrCopyRenderData_CopyData: From LOD %d to LOD %d"), LastValidLODIndex, LODIndex));

			// Render Data will be reused from the previously generated component
			UnrealConversionUtils::CopySkeletalMeshLODRenderData(SrcSkeletalMesh, SkeletalMesh, LastValidLODIndex, LODIndex);
			continue;
		}

		FInstanceUpdateData::FComponent& Component = OperationData->InstanceUpdateData.Components[OperationData->InstanceUpdateData.LODs[LastValidLODIndex].FirstComponent + ComponentIndex];

		// There could be components without a mesh in LODs
		if (!Component.Mesh)
		{
			UE_LOG(LogMutable, Warning, TEXT("Building instance: generated mesh [%s] has LOD [%d] of component [%d] with no mesh.")
				, *SkeletalMesh->GetName()
				, LODIndex
				, Component.Id);

			// End with failure
			return false;
		}

		TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*FString::Printf(TEXT("BuildOrCopyRenderData_BuildData: LOD %d"), LODIndex));

		SetLastMeshId(Component.Id, LODIndex, Component.MeshID);

		UnrealConversionUtils::SetupRenderSections(
			SkeletalMesh,
			Component.Mesh,
			LODIndex,
			OperationData->InstanceUpdateData.BoneMaps,
			Component.FirstBoneMap);

		UnrealConversionUtils::CopyMutableVertexBuffers(
			SkeletalMesh,
			Component.Mesh,
			LODIndex);

		FSkeletalMeshLODRenderData& LODModel = SkeletalMesh->GetResourceForRendering()->LODRenderData[LODIndex];

		if (LODModel.DoesVertexBufferUse16BitBoneIndex() && !UCustomizableObjectSystem::GetInstance()->IsSupport16BitBoneIndexEnabled())
		{
			Public->SkeletalMeshStatus = ESkeletalMeshState::PostUpdateError;

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

		// Update active and required bones
		LODModel.ActiveBoneIndices.Append(Component.ActiveBones);
		LODModel.RequiredBones.Append(Component.ActiveBones);

		const TArray<FMutableSkinWeightProfileInfo>& SkinWeightProfilesInfo = CustomizableObject->SkinWeightProfilesInfo;
		if (SkinWeightProfilesInfo.Num())
		{
			bool bHasSkinWeightProfiles = false;

			const mu::FMeshBufferSet& MutableMeshVertexBuffers = Component.Mesh->GetVertexBuffers();

			const int32 SkinWeightProfilesCount = CustomizableObject->SkinWeightProfilesInfo.Num();
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
					LODModel.SkinWeightProfilesData.Init(&LODModel.SkinWeightVertexBuffer);
					bHasSkinWeightProfiles = true;
				}

				const FMutableSkinWeightProfileInfo& Profile = CustomizableObject->SkinWeightProfilesInfo[ProfileIndex];

				const FSkinWeightProfileInfo* ExistingProfile = SkeletalMesh->GetSkinWeightProfiles().FindByPredicate(
					[&Profile](const FSkinWeightProfileInfo& P) { return P.Name == Profile.Name; });

				if (!ExistingProfile)
				{
					SkeletalMesh->AddSkinWeightProfile({ Profile.Name, Profile.DefaultProfile, Profile.DefaultProfileFromLODIndex });
				}

				UnrealConversionUtils::CopyMutableSkinWeightProfilesBuffers(
					LODModel,
					Profile.Name,
					MutableMeshVertexBuffers,
					BoneIndicesBufferIndex);
			}
		}

		// Copy indices.
		if (!UnrealConversionUtils::CopyMutableIndexBuffers(Component.Mesh, LODModel))
		{
			// End with failure
			return false;
		}

		// Update LOD and streaming data
		const FMutableRefLODRenderData& RefLODRenderData = CustomizableObject->GetRefSkeletalMeshData(Component.Id)->LODData[LODIndex].RenderData;
		LODModel.bIsLODOptional = RefLODRenderData.bIsLODOptional;
		LODModel.bStreamedDataInlined = RefLODRenderData.bStreamedDataInlined;

		// WARNING! BufferSize must be > 0 or all texture mips in this LOD will be requested at once. 
		// USkeletalMesh::IsMaterialUsed checks this size to see if a material is being used. If it fails the textures used by it won't be included 
		// in the map of textures to stream.
		LODModel.BuffersSize = 1;

	}


	return true;
}

static bool bEnableHighPriorityLoading = true;
FAutoConsoleVariableRef CVarMutableHighPriorityLoading(
	TEXT("Mutable.EnableLoadingAssetsWithHighPriority"),
	bEnableHighPriorityLoading,
	TEXT("If enabled, the request to load additional assets will have high priority."));

FGraphEventRef UCustomizableInstancePrivateData::LoadAdditionalAssetsAsync(const TSharedPtr<FMutableOperationData>& OperationData, UCustomizableObjectInstance* Public, FStreamableManager& StreamableManager)
{
	MUTABLE_CPUPROFILER_SCOPE(UCustomizableInstancePrivateData::LoadAdditionalAssetsAsync);

	UCustomizableObject* CustomizableObject = Public->GetCustomizableObject();

	FGraphEventRef Result = nullptr;

	TArray<FSoftObjectPath> AssetsToStream;

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
		uint32 MaterialIndex = Surface.MaterialIndex;

		if (!ObjectToInstanceIndexMap.Contains(MaterialIndex))
		{
			UMaterialInterface* MatInterface = CustomizableObject->GetReferencedMaterialAssetPtr(MaterialIndex).Get();

			ObjectToInstanceIndexMap.Add(MaterialIndex, ReferencedMaterials.Num());
			ReferencedMaterials.Add(MatInterface);

			check(ObjectToInstanceIndexMap.Num() == ReferencedMaterials.Num());

			if (!MatInterface)
			{
				FSoftObjectPath MaterialPath = CustomizableObject->GetReferencedMaterialAssetPtr(MaterialIndex).ToSoftObjectPath();

				if (MaterialPath.IsValid())
				{
					AssetsToStream.Add(MaterialPath);
				}
			}
		}
	}

	// Clear invalid skeletons from the MergedSkeletons cache
	CustomizableObject->UnCacheInvalidSkeletons();

	const int32 ComponentCount = OperationData->InstanceUpdateData.Skeletons.Num();

	// Load Skeletons required by the SubMeshes of the newly generated Mesh, will be merged later
	for (FCustomizableInstanceComponentData& ComponentData : ComponentsData)
	{
		const uint16 ComponentIndex = ComponentData.ComponentIndex;

		FInstanceUpdateData::FSkeletonData* SkeletonData = OperationData->InstanceUpdateData.Skeletons.FindByPredicate(
			[&ComponentIndex](FInstanceUpdateData::FSkeletonData& S) { return S.ComponentIndex == ComponentIndex; });
		check(SkeletonData);

		ComponentData.Skeletons.Skeleton = CustomizableObject->GetCachedMergedSkeleton(ComponentIndex, SkeletonData->SkeletonIds);
		if (ComponentData.Skeletons.Skeleton)
		{
			ComponentData.Skeletons.SkeletonIds.Empty();
			ComponentData.Skeletons.SkeletonsToMerge.Empty();
			continue;
		}

		for (const uint32 SkeletonId : SkeletonData->SkeletonIds)
		{
			TSoftObjectPtr<USkeleton> AssetPtr;
			if (SkeletonId == 0)
			{
				FMutableRefSkeletalMeshData* RefSkeletalMeshData = CustomizableObject->GetRefSkeletalMeshData(ComponentIndex);
				check(RefSkeletalMeshData);

				AssetPtr = RefSkeletalMeshData->Skeleton;
			}
			else
			{
				AssetPtr = CustomizableObject->GetReferencedSkeletonAssetPtr(SkeletonId);
			}

			if (AssetPtr.IsNull())
			{
				continue;
			}

			// Add referenced skeletons to the assets to stream
			ComponentData.Skeletons.SkeletonIds.Add(SkeletonId);

			if (USkeleton* Skeleton = AssetPtr.Get())
			{
				ComponentData.Skeletons.SkeletonsToMerge.Add(Skeleton);
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
		const FInstanceUpdateData::FLOD& LOD = OperationData->InstanceUpdateData.LODs[OperationData->CurrentMinLOD];
		for (int32 ComponentIndex = 0; ComponentIndex < LOD.ComponentCount; ++ComponentIndex)
		{
			const FInstanceUpdateData::FComponent& Component = Components[LOD.FirstComponent + ComponentIndex];
			mu::MeshPtrConst MutableMesh = Component.Mesh;

			if (!MutableMesh)
			{
				continue;
			}

			FCustomizableInstanceComponentData* ComponentData = GetComponentData(Component.Id);

			const bool bReplacePhysicsAssets = HasCOInstanceFlags(ReplacePhysicsAssets);

			for (int32 TagIndex = 0; TagIndex < MutableMesh->GetTagCount(); ++TagIndex)
			{
				FString Tag = MutableMesh->GetTag(TagIndex);
				if (Tag.RemoveFromStart("__PhysicsAsset:"))
				{
					TSoftObjectPtr<UPhysicsAsset>* PhysicsAsset = CustomizableObject->PhysicsAssetsMap.Find(Tag);

					if (!PhysicsAsset->IsNull())
					{
						if (PhysicsAsset->Get())
						{
							ComponentData->PhysicsAssets.PhysicsAssetsToMerge.Add(PhysicsAsset->Get());
						}
						else
						{
							ComponentData->PhysicsAssets.PhysicsAssetToLoad.Add(PhysicsAsset->ToSoftObjectPath().ToString());
							AssetsToStream.Add(PhysicsAsset->ToSoftObjectPath());
						}
					}
				}
				else if (Tag.RemoveFromStart("__ClothPhysicsAsset:"))
				{
					FString AssetIndexString, AssetPath;

					if (Tag.Split(TEXT("_AssetIdx_"), &AssetIndexString, &AssetPath) && AssetIndexString.IsNumeric())
					{

						int32 AssetIndex = FCString::Atoi(*AssetIndexString);

						TSoftObjectPtr<UPhysicsAsset>* PhysicsAssetPtr = CustomizableObject->PhysicsAssetsMap.Find(AssetPath);

						// The entry should always be in the map
						check(PhysicsAssetPtr);
						if (!PhysicsAssetPtr->IsNull())
						{
							if (PhysicsAssetPtr->Get())
							{
								if (ClothingPhysicsAssets.IsValidIndex(AssetIndex))
								{
									ClothingPhysicsAssets[AssetIndex] = PhysicsAssetPtr->Get();
								}
							}
							else
							{
								ComponentData->ClothingPhysicsAssetsToStream.Emplace(AssetIndex, PhysicsAssetPtr->ToSoftObjectPath().ToString());
								AssetsToStream.Add(PhysicsAssetPtr->ToSoftObjectPath());
							}
						}
					}
				}
				if (Tag.RemoveFromStart("__AnimBP:"))
				{
					FString SlotIndexString, AssetPath;

					if (Tag.Split(TEXT("_Slot_"), &SlotIndexString, &AssetPath))
					{
						if (!SlotIndexString.IsEmpty())
						{
							FName SlotIndex = *SlotIndexString;

							TSoftClassPtr<UAnimInstance>* AnimBPAsset = CustomizableObject->AnimBPAssetsMap.Find(AssetPath);

							if (AnimBPAsset && !AnimBPAsset->IsNull())
							{
								if (!ComponentData->AnimSlotToBP.Contains(SlotIndex))
								{
									ComponentData->AnimSlotToBP.Add(SlotIndex, *AnimBPAsset);

									if (AnimBPAsset->Get())
									{
										GatheredAnimBPs.Add(AnimBPAsset->Get());
									}
									else
									{
										AssetsToStream.Add(AnimBPAsset->ToSoftObjectPath());
									}
								}
								else
								{
									// Two submeshes should not have the same animation slot index
									FString ErrorMsg = FString::Printf(TEXT("Two submeshes have the same anim slot index [%s] in a Mutable Instance."), *SlotIndex.ToString());
									UE_LOG(LogMutable, Error, TEXT("%s"), *ErrorMsg);
#if WITH_EDITOR
									FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
									MessageLogModule.RegisterLogListing(FName("Mutable"), FText::FromString(FString("Mutable")));
									FMessageLog MessageLog("Mutable");

									MessageLog.Notify(FText::FromString(ErrorMsg), EMessageSeverity::Error, true);
#endif
								}
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
				AssetsToStream.Add(CustomizableObject->AnimBpOverridePhysiscAssetsInfo[ExternalId].SourceAsset.ToSoftObjectPath());
			}
		}
	}

	if (AssetsToStream.Num() > 0)
	{
		check(!StreamingHandle);

		Result = FGraphEvent::CreateGraphEvent();
		StreamingHandle = StreamableManager.RequestAsyncLoad(AssetsToStream, FStreamableDelegate::CreateUObject(Public, &UCustomizableObjectInstance::AdditionalAssetsAsyncLoaded, Result),
			bEnableHighPriorityLoading ? FStreamableManager::AsyncLoadHighPriority : FStreamableManager::DefaultAsyncLoadPriority);
	}

	return Result;
}

void UCustomizableObjectInstance::AdditionalAssetsAsyncLoaded( FGraphEventRef CompletionEvent )
{
	// TODO: Do we need this separated?
	PrivateData->AdditionalAssetsAsyncLoaded(this);
	CompletionEvent->DispatchSubsequents(); // TODO: we know it is game thread?

	PrivateData->StreamingHandle = nullptr;
}


void UCustomizableInstancePrivateData::AdditionalAssetsAsyncLoaded(UCustomizableObjectInstance* Public)
{
	MUTABLE_CPUPROFILER_SCOPE(UCustomizableInstancePrivateData::AdditionalAssetsAsyncLoaded);

	UCustomizableObject* CustomizableObject = Public->GetCustomizableObject();

	// Loaded Materials
	check(ObjectToInstanceIndexMap.Num() == ReferencedMaterials.Num());

	for (TPair<uint32, uint32> Pair : ObjectToInstanceIndexMap)
	{
		ReferencedMaterials[Pair.Value] = CustomizableObject->GetReferencedMaterialAssetPtr(Pair.Key).Get();
	}

#if WITH_EDITOR
	for (int32 i = 0; i < ReferencedMaterials.Num(); ++i)
	{
		if (!ReferencedMaterials[i])
		{
			const uint32* Key = ObjectToInstanceIndexMap.FindKey(i);
			TSoftObjectPtr<UMaterialInterface> ErrorMaterial = Key != nullptr ? CustomizableObject->GetReferencedMaterialAssetPtr(*Key) : nullptr;

			if (!ErrorMaterial.IsNull())
			{
				FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
				MessageLogModule.RegisterLogListing(FName("Mutable"), FText::FromString(FString("Mutable")));
				FMessageLog MessageLog("Mutable");

				FString ErrorMsg = FString::Printf(TEXT("Mutable couldn't load the material [%s] and won't be rendered. If it has been deleted or renamed, please recompile all the mutable objects that use it."), *ErrorMaterial.GetAssetName());
				UE_LOG(LogMutable, Error, TEXT("%s"), *ErrorMsg);
				MessageLog.Notify(FText::FromString(ErrorMsg), EMessageSeverity::Error, true);
			}
			else
			{
				ensure(false); // Couldn't load the material, and we don't know which material
			}
		}
	}
#endif

	
	for (FCustomizableInstanceComponentData& ComponentData : ComponentsData)
	{
		// Loaded Skeletons
		FReferencedSkeletons& Skeletons = ComponentData.Skeletons;
		for (int32 SkeletonIndex : Skeletons.SkeletonIds)
		{
			Skeletons.SkeletonsToMerge.AddUnique(CustomizableObject->GetReferencedSkeletonAssetPtr(SkeletonIndex).Get());
		}

		// Loaded PhysicsAssets
		FReferencedPhysicsAssets& PhysicsAssets = ComponentData.PhysicsAssets;
		for(const FString& Path : PhysicsAssets.PhysicsAssetToLoad)
		{
			const TSoftObjectPtr<UPhysicsAsset>* PhysicsAssetPtr = CustomizableObject->PhysicsAssetsMap.Find(Path);
			PhysicsAssets.PhysicsAssetsToMerge.Add(PhysicsAssetPtr->Get());

#if WITH_EDITOR
			if (!PhysicsAssetPtr->Get())
			{
				if (!PhysicsAssetPtr->IsNull())
				{
					FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
					MessageLogModule.RegisterLogListing(FName("Mutable"), FText::FromString(FString("Mutable")));
					FMessageLog MessageLog("Mutable");

					FString ErrorMsg = FString::Printf(TEXT("Mutable couldn't load the PhysicsAsset [%s] and won't be merged. If it has been deleted or renamed, please recompile all the mutable objects that use it."), *PhysicsAssetPtr->GetAssetName());
					UE_LOG(LogMutable, Error, TEXT("%s"), *ErrorMsg);
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
		for ( TPair<int32, FString>& AssetToStream : ComponentData.ClothingPhysicsAssetsToStream )
		{
			const int32 AssetIndex = AssetToStream.Key;

			if (ClothingPhysicsAssets.IsValidIndex(AssetIndex))
			{
				const TSoftObjectPtr<UPhysicsAsset>* PhysicsAssetPtr = CustomizableObject->PhysicsAssetsMap.Find(AssetToStream.Value);
				check(PhysicsAssetPtr) // Should always be found.
			
				ClothingPhysicsAssets[AssetIndex] = PhysicsAssetPtr->Get();
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
				FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
				MessageLogModule.RegisterLogListing(FName("Mutable"), FText::FromString(FString("Mutable")));
				FMessageLog MessageLog("Mutable");

				FString ErrorMsg = FString::Printf(TEXT("Mutable couldn't load the AnimBlueprint [%s]. If it has been deleted or renamed, please recompile all the mutable objects that use it."), *SlotAnimBP.Value.GetAssetName());
				UE_LOG(LogMutable, Error, TEXT("%s"), *ErrorMsg);
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
					CustomizableObject->AnimBpOverridePhysiscAssetsInfo[AnimBpPhysicsOverrideIndex].SourceAsset.Get());
		}
		ComponentData.PhysicsAssets.AdditionalPhysicsAssetsToLoad.Empty();
	}
}


void UpdateTextureRegionsMutable(UTexture2D* Texture, int32 MipIndex, uint32 NumMips, const FUpdateTextureRegion2D& Region, uint32 SrcPitch, const FByteBulkData* BulkData)
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
		};

		FUpdateTextureRegionsData* RegionData = new FUpdateTextureRegionsData;

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
				delete RegionData;
			});
	}
}


void UCustomizableInstancePrivateData::ReuseTexture(UTexture2D* Texture)
{
	if (Texture->GetPlatformData())
	{
		uint32 NumMips = Texture->GetPlatformData()->Mips.Num();

		for (uint32 i = 0; i < NumMips; i++)
		{
			FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[i];

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

				UpdateTextureRegionsMutable(Texture, i, NumMips, Region, Mip.SizeX * sizeof(uint8) * 4, &Mip.BulkData);
			}
		}
	}
}


void UCustomizableInstancePrivateData::BuildMaterials(const TSharedPtr<FMutableOperationData>& OperationData, UCustomizableObjectInstance* Public)
{
	MUTABLE_CPUPROFILER_SCOPE(UCustomizableInstancePrivateData::BuildMaterials)

	UCustomizableObject* CustomizableObject = Public->GetCustomizableObject();

	// Find skipped LODs. The following valid LOD will be copied into them. 
	TArray<bool> LODsSkipped;
	LODsSkipped.SetNum(OperationData->NumLODsAvailable);

	TArray<FGeneratedTexture> NewGeneratedTextures;

	GeneratedMaterials.Reset();

	// Prepare the data to store in order to regenerate resources for this instance (usually texture mips).
	TSharedPtr<FMutableUpdateContext> UpdateContext = MakeShared<FMutableUpdateContext>(UCustomizableObjectSystem::GetInstance()->GetPrivate()->MutableSystem,
		CustomizableObject->GetModel(),
		OperationData->MutableParameters,
	    OperationData->State);
	
	const bool bReuseTextures = OperationData->bReuseInstanceTextures;

	const FInstanceUpdateData::FLOD& FirstLOD = OperationData->InstanceUpdateData.LODs[OperationData->CurrentMinLOD];
	const int32 NumComponents = FirstLOD.ComponentCount;

	for (int32 ComponentIndex = 0; ComponentIndex < NumComponents; ++ComponentIndex)
	{
		USkeletalMesh* SkeletalMesh = Public->SkeletalMeshes[ComponentIndex];

		if (!SkeletalMesh)
		{
			continue;
		}

		SkeletalMesh->GetMaterials().Reset();

		// Maps serializations of FMutableMaterialPlaceholder to Created Dynamic Material instances, used to reuse materials across LODs
		TMap<FString, TSharedPtr<FMutableMaterialPlaceholder>> ReuseMaterialCache;

		// Map of SharedSurfaceId to FMutableMaterialPlaceholder serialization key 
		TMap<int32, FString> SharedSurfacesCache;

		MUTABLE_CPUPROFILER_SCOPE(BuildMaterials_LODLoop);

		for (int32 LODIndex = FirstLODAvailable; LODIndex <= OperationData->CurrentMaxLOD; LODIndex++)
		{
			const FInstanceUpdateData::FLOD& LOD = OperationData->InstanceUpdateData.LODs[LODIndex];
			const FInstanceUpdateData::FComponent& Component = OperationData->InstanceUpdateData.Components[LOD.FirstComponent + ComponentIndex];

			if (!Component.bGenerated)
			{
				LODsSkipped[LODIndex] = true;
				continue;
			}

			if (SkeletalMesh->GetLODInfoArray().Num() != 0)
			{
				SkeletalMesh->GetLODInfoArray()[LODIndex].LODMaterialMap.Reset();
			}

			check(Component.bGenerated);

			const FMutableRefSkeletalMeshData* RefSkeletalMeshData = CustomizableObject->GetRefSkeletalMeshData(Component.Id);
			check(RefSkeletalMeshData);

			for (int32 SurfaceIndex = 0; SurfaceIndex < Component.SurfaceCount; ++SurfaceIndex)
			{
				const FInstanceUpdateData::FSurface& Surface = OperationData->InstanceUpdateData.Surfaces[Component.FirstSurface + SurfaceIndex];

				const uint32 ReferencedMaterialIndex = ObjectToInstanceIndexMap[Surface.MaterialIndex];
				UMaterialInterface* MaterialTemplate = ReferencedMaterials[ReferencedMaterialIndex];
				if (!MaterialTemplate)
				{
					UE_LOG(LogMutable, Error, TEXT("Build Materials: Missing referenced template to use as parent material on CustomizableObject [%s]."), *CustomizableObject->GetName());
					continue;
				}

				// Reuse surface between LODs when using AutomaticLODs from mesh.
				if (const FString* SharedSurfaceSerialization = SharedSurfacesCache.Find(Surface.SurfaceId))
				{
					TSharedPtr<FMutableMaterialPlaceholder>* FoundMaterialPlaceholder = ReuseMaterialCache.Find(*SharedSurfaceSerialization);
					check(FoundMaterialPlaceholder);

					const int32 MaterialIndex = (*FoundMaterialPlaceholder)->MatIndex;
					check(MaterialIndex >= 0);

					int32 LODMaterialIndex = SkeletalMesh->GetLODInfoArray()[LODIndex].LODMaterialMap.Add(MaterialIndex);
					SkeletalMesh->GetResourceForRendering()->LODRenderData[LODIndex].RenderSections[SurfaceIndex].MaterialIndex = LODMaterialIndex;
					continue;
				}

				TSharedPtr<FMutableMaterialPlaceholder> MutableMaterialPlaceholderPtr(new FMutableMaterialPlaceholder);
				FMutableMaterialPlaceholder& MutableMaterialPlaceholder = *MutableMaterialPlaceholderPtr;

				{
					MUTABLE_CPUPROFILER_SCOPE(ParamLoop);

					for (int32 VectorIndex = 0; VectorIndex < Surface.VectorCount; ++VectorIndex)
					{
						const FInstanceUpdateData::FVector& Vector = OperationData->InstanceUpdateData.Vectors[Surface.FirstVector + VectorIndex];

						// Decoding Material Layer from Mutable parameter name
						FString EncodingString = "-MutableLayerParam:";

						int32 EncodingPosition = Vector.Name.Find(EncodingString);
						int32 LayerIndex = -1;

						if (EncodingPosition == INDEX_NONE)
						{
							MutableMaterialPlaceholder.AddParam(
								FMutableMaterialPlaceholder::FMutableMaterialPlaceHolderParam(FName(Vector.Name), -1, Vector.Vector));
						}
						else
						{
							//Getting layer index
							int32 LayerPosition = Vector.Name.Len() - (EncodingPosition + EncodingString.Len());
							FString IndexString = Vector.Name.RightChop(Vector.Name.Len() - LayerPosition);
							LayerIndex = FCString::Atof(*IndexString);

							//Getting parameter name
							FString Sufix = EncodingString + FString::FromInt(LayerIndex);
							FString NameCopy = Vector.Name;
							NameCopy.RemoveFromEnd(Sufix);

							MutableMaterialPlaceholder.AddParam(
								FMutableMaterialPlaceholder::FMutableMaterialPlaceHolderParam(FName(NameCopy), LayerIndex, Vector.Vector));
						}
					}

					for (int32 ScalarIndex = 0; ScalarIndex < Surface.ScalarCount; ++ScalarIndex)
					{
						const FInstanceUpdateData::FScalar& Scalar = OperationData->InstanceUpdateData.Scalars[Surface.FirstScalar + ScalarIndex];

						// Decoding Material Layer from Mutable parameter name
						FString EncodingString = "-MutableLayerParam:";

						int32 EncodingPosition = Scalar.Name.Find(EncodingString);
						int32 LayerIndex = -1;

						if (EncodingPosition == INDEX_NONE)
						{
							MutableMaterialPlaceholder.AddParam(
								FMutableMaterialPlaceholder::FMutableMaterialPlaceHolderParam(FName(Scalar.Name), -1, Scalar.Scalar));
						}
						else
						{
							//Getting layer index
							int32 LayerPosition = Scalar.Name.Len() - (EncodingPosition + EncodingString.Len());
							FString IndexString = Scalar.Name.RightChop(Scalar.Name.Len() - LayerPosition);
							LayerIndex = FCString::Atof(*IndexString);

							//Getting parameter name
							FString Sufix = EncodingString + FString::FromInt(LayerIndex);
							FString NameCopy = Scalar.Name;
							NameCopy.RemoveFromEnd(Sufix);

							MutableMaterialPlaceholder.AddParam(
								FMutableMaterialPlaceholder::FMutableMaterialPlaceHolderParam(FName(NameCopy), LayerIndex, Scalar.Scalar));
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
						FString KeyName = Image.Name;
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
						if (MutableImage && MutableImage->IsReference())
						{
							uint32 ReferenceID = MutableImage->GetReferencedTexture();
							if (CustomizableObject->ReferencedPassThroughTextures.IsValidIndex(ReferenceID))
							{
								TSoftObjectPtr<UTexture> Ref = CustomizableObject->ReferencedPassThroughTextures[ReferenceID];

								// \TODO: This will force the load of the reference texture, potentially causing a hich. 
								PassThroughTexture = Ref.Get();
							}
							else
							{
								// internal error.
								UE_LOG(LogMutable, Error, TEXT("Referenced image [%d] was not dtored in the resource array."), ReferenceID);
							}
						}

						// Find the additional information for this image
						int32 ImageKey = FCString::Atoi(*KeyName);
						if (ImageKey >= 0 && ImageKey < CustomizableObject->ImageProperties.Num())
						{
							const FMutableModelImageProperties& Props = CustomizableObject->ImageProperties[ImageKey];

							if (!MutableTexture && !PassThroughTexture && MutableImage)
							{
								TWeakObjectPtr<UTexture2D>* ReusedTexture = bReuseTextures ? TextureReuseCache.Find(TextureReuseCacheRef) : nullptr;

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
										}

										if (MutableTexture->GetPlatformData())
										{
											delete MutableTexture->GetPlatformData();
										}

										MutableTexture->SetPlatformData(PlatformData);
										OperationData->ImageToPlatformDataMap.Remove(Image.ImageID);
									}
									else
									{
										UE_LOG(LogMutable, Error, TEXT("Required image was not generated in the mutable thread, and it is not cached."));
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

											ReuseTexture(MutableTexture);
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
							FString ImageName = Image.Name;
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

				MutableMaterialPlaceholder.ParentMaterial = MaterialTemplate;

				const FString MaterialPlaceholderSerialization = MutableMaterialPlaceholder.GetSerialization();
				check(MaterialPlaceholderSerialization != FString("null"));

				int32 MatIndex = INDEX_NONE;
				
				if (TSharedPtr<FMutableMaterialPlaceholder>* FoundMaterialPlaceholder = ReuseMaterialCache.Find(MaterialPlaceholderSerialization))
				{
					MatIndex = (*FoundMaterialPlaceholder)->MatIndex;
				}
				else // Material not cached, create a new one
				{
					MUTABLE_CPUPROFILER_SCOPE(BuildMaterials_CreateMaterial);

					ReuseMaterialCache.Add(MaterialPlaceholderSerialization, MutableMaterialPlaceholderPtr);

					SharedSurfacesCache.Add(Surface.SurfaceId, MaterialPlaceholderSerialization);

					FGeneratedMaterial Material;
					UMaterialInstanceDynamic* MaterialInstance = nullptr;
					UMaterialInterface* ActualMaterialInterface = MaterialTemplate;

					if (MutableMaterialPlaceholder.Params.Num() != 0)
					{
						MaterialInstance = UMaterialInstanceDynamic::Create(MaterialTemplate, GetTransientPackage());
						ActualMaterialInterface = MaterialInstance;
					}

					if (SkeletalMesh)
					{
						MatIndex = SkeletalMesh->GetMaterials().Num();
						MutableMaterialPlaceholder.MatIndex = MatIndex;

						// Set up SkeletalMaterial data
						FSkeletalMaterial& SkeletalMaterial = SkeletalMesh->GetMaterials().Add_GetRef(ActualMaterialInterface);
						SkeletalMaterial.MaterialSlotName = CustomizableObject->ReferencedMaterialSlotNames[Surface.MaterialIndex];
						SetMeshUVChannelDensity(SkeletalMaterial.UVChannelData, RefSkeletalMeshData->Settings.DefaultUVChannelDensity);
					}

					if (MaterialInstance)
					{
						for (const FMutableMaterialPlaceholder::FMutableMaterialPlaceHolderParam& Param : MutableMaterialPlaceholder.Params)
						{
							switch (Param.Type)
							{
							case FMutableMaterialPlaceholder::EPlaceHolderParamType::Vector:
								if (Param.LayerIndex < 0)
								{
									MaterialInstance->SetVectorParameterValue(Param.ParamName, Param.Vector);
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

					GeneratedMaterials.Add(Material);
				}

				int32 LODMaterialIndex = SkeletalMesh->GetLODInfoArray()[LODIndex].LODMaterialMap.Add(MatIndex);
				SkeletalMesh->GetResourceForRendering()->LODRenderData[LODIndex].RenderSections[SurfaceIndex].MaterialIndex = LODMaterialIndex;

			}
		}

		{
			// Copy data from valid LODs into the skipped ones.
			int32 LastValidLODIndex = OperationData->CurrentMaxLOD;
			for (int32 LODIndex = OperationData->CurrentMaxLOD; LODIndex >= FirstLODAvailable; --LODIndex)
			{
				// Copy information from the LastValidLODIndex
				if (LODsSkipped[LODIndex])
				{
					SkeletalMesh->GetLODInfoArray()[LODIndex].LODMaterialMap = SkeletalMesh->GetLODInfoArray()[LastValidLODIndex].LODMaterialMap;

					TIndirectArray<FSkeletalMeshLODRenderData>& LODRenderData = SkeletalMesh->GetResourceForRendering()->LODRenderData;

					const int32 NumRenderSections = LODRenderData[LODIndex].RenderSections.Num();
					check(NumRenderSections == LODRenderData[LastValidLODIndex].RenderSections.Num());

					if (NumRenderSections == LODRenderData[LastValidLODIndex].RenderSections.Num())
					{
						for (int32 RenderSectionIndex = 0; RenderSectionIndex < NumRenderSections; ++RenderSectionIndex)
						{
							const int32 MaterialIndex = LODRenderData[LastValidLODIndex].RenderSections[RenderSectionIndex].MaterialIndex;
							LODRenderData[LODIndex].RenderSections[RenderSectionIndex].MaterialIndex = MaterialIndex;
						}
					}
				}
				else
				{
					LastValidLODIndex = LODIndex;
				}
			}
		}
	}

	
	{
		MUTABLE_CPUPROFILER_SCOPE(BuildMaterials_Exchange);

		FCustomizableObjectSystemPrivate* CustomizableObjectSystem = UCustomizableObjectSystem::GetInstance()->GetPrivate();
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
	}
}


void UCustomizableInstancePrivateData::ProcessTextureCoverageQueries(const TSharedPtr<FMutableOperationData>& OperationData, UCustomizableObject* CustomizableObject, const FString& ImageKeyName, FTexturePlatformData *PlatformData, UMaterialInterface* Material)
{
	if (OperationData->TextureCoverageQueries_MutableThreadParams.Num() && PlatformData->Mips.Num())
	{
		int32 ImageKey = FCString::Atoi(*ImageKeyName);
		int32 ScannedMipLevel = 0;
		check(PlatformData->Mips.IsValidIndex(ScannedMipLevel));

		if (ImageKey >= 0 && ImageKey < CustomizableObject->ImageProperties.Num())
		{
			const FMutableModelImageProperties& Props = CustomizableObject->ImageProperties[ImageKey];

			FTextureCoverageQueryData* TextureCoverageQueryData = OperationData->TextureCoverageQueries_MutableThreadParams.Find(Props.TextureParameterName);

			if (TextureCoverageQueryData)
			{
				FTexture2DMipMap& ScannedMip = PlatformData->Mips[ScannedMipLevel];
				uint32 TotalTexels = ScannedMip.SizeX * ScannedMip.SizeY;
				uint32 CoveredTexels = 0;
				uint32 MaskedOutCoveredTexels = 0;

				auto MaskOutCache = CustomizableObject->MaskOutCache.Get();

				if (!MaskOutCache)
				{
					UE_LOG(LogMutable, Error, TEXT("The CustomizableObject->MaskOutCache has to be manually loaded by the programmer by calling CustomizableObject->LoadMaskOutCache()."));
				}

				FString* MaskOutTexturePath = MaskOutCache ? MaskOutCache->Materials.Find(Material->GetPathName()) : nullptr;
				FMaskOutTexture* MaskOutTexture = MaskOutTexturePath ? MaskOutCache->Textures.Find(*MaskOutTexturePath) : nullptr;

				// Textures must be uncompressed, the CustomizableInstance's state should be set to one with
				// texture compression disabled for Texture Coverage Queries
				ensure(PlatformData->PixelFormat == PF_R8G8B8A8 || PlatformData->PixelFormat == PF_B8G8R8A8);

				const void* pData = ScannedMip.BulkData.Lock(LOCK_READ_ONLY);
				uint8_t* pDest = (uint8_t*)pData;

				for (size_t px = 0; px < ScannedMip.SizeX; ++px)
				{
					for (size_t py = 0; py < ScannedMip.SizeY; ++py)
					{
						if (pDest[(py * ScannedMip.SizeX + px) * 4 + 3] > 0) // Check if alpha channel of PF_R8G8B8A8 is not zero
						{
							CoveredTexels++;

							if (MaskOutTexture)
							{
								float u = float(px) / ScannedMip.SizeX;
								float v = float(py) / ScannedMip.SizeY;

								size_t pu = FMath::RoundToInt(u * MaskOutTexture->GetSizeX());
								size_t pv = FMath::RoundToInt(v * MaskOutTexture->GetSizeY());

								if (MaskOutTexture->GetTexelReference(pv * MaskOutTexture->GetSizeX() + pu) == 0)
								{
									MaskedOutCoveredTexels++;
								}
							}
						}
					}
				}

				ScannedMip.BulkData.Unlock();

				auto& ResultData = OperationData->TextureCoverageQueries_MutableThreadResults.FindOrAdd(Props.TextureParameterName);
				ResultData.CoveredTexels += CoveredTexels;
				ResultData.MaskedOutCoveredTexels += MaskedOutCoveredTexels;
				ResultData.TotalTexels += TotalTexels;
			}
		}
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


void UCustomizableObjectInstance::AddQueryTextureCoverage(const FString& TextureName, const FString* MaskOutChannelName)
{
	FTextureCoverageQueryData& QueryData = GetPrivate()->TextureCoverageQueries.FindOrAdd(TextureName);

	if (MaskOutChannelName)
	{
		QueryData.MaskOutChannelName = *MaskOutChannelName;
	}
}


void UCustomizableObjectInstance::RemoveQueryTextureCoverage(const FString& TextureName)
{
	GetPrivate()->TextureCoverageQueries.Remove(TextureName);
}


float UCustomizableObjectInstance::GetQueryResultTextureCoverage(const FString& TextureName)
{
	FTextureCoverageQueryData* QueryData = GetPrivate()->TextureCoverageQueries.Find(TextureName);

	return QueryData ? QueryData->GetCoverage() : 0.f;
}


float UCustomizableObjectInstance::GetQueryResultTextureCoverageMasked(const FString& TextureName)
{
	FTextureCoverageQueryData* QueryData = GetPrivate()->TextureCoverageQueries.Find(TextureName);

	return QueryData ? QueryData->GetMaskedOutCoverage() : 0.f;
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


void UCustomizableObjectInstance::SetMinMaxLODToLoad(FMutableInstanceUpdateMap& InOutRequestedUpdates, int32 NewMinLOD, int32 NewMaxLOD, bool bLimitLODUpgrades)
{
	SetRequestedLODs(NewMinLOD, NewMaxLOD, Descriptor.RequestedLODLevels, InOutRequestedUpdates);
}


int32 UCustomizableObjectInstance::GetNumComponents() const
{
	return GetCustomizableObject() ? GetCustomizableObject()->GetComponentCount() : 0;
}


int32 UCustomizableObjectInstance::GetMinLODToLoad() const
{
	return Descriptor.MinLOD;	
}


int32 UCustomizableObjectInstance::GetMaxLODToLoad() const
{
	return Descriptor.MaxLOD;	
}


int32 UCustomizableObjectInstance::GetNumLODsAvailable() const
{
	return GetPrivate()->GetNumLODsAvailable();
}


int32 UCustomizableObjectInstance::GetCurrentMinLOD() const
{
	return CurrentMinLOD;
}


int32 UCustomizableObjectInstance::GetCurrentMaxLOD() const
{
	return CurrentMaxLOD;
}

#if !UE_BUILD_SHIPPING
static bool bIgnoreMinMaxLOD = false;
FAutoConsoleVariableRef CVarMutableIgnoreMinMaxLOD(
	TEXT("Mutable.IgnoreMinMaxLOD"),
	bIgnoreMinMaxLOD,
	TEXT("The limits on the number of LODs to generate will be ignored."));
#endif


void UCustomizableObjectInstance::SetRequestedLODs(int32 InMinLOD, int32 InMaxLOD, const TArray<uint16>& InRequestedLODsPerComponent, 
	                                               FMutableInstanceUpdateMap& InOutRequestedUpdates)
{
	check(PrivateData);
	if (!PrivateData->HasCOInstanceFlags(LODsStreamingEnabled))
	{
		return;
	}

	if (!CanUpdateInstance())
	{
		return;
	}

#if !UE_BUILD_SHIPPING
	// Ignore Min/Max LOD limits. Mainly used for debug
	if (!bIgnoreMinMaxLOD)
	{
		InMinLOD = 0;
		InMaxLOD = MAX_MESH_LOD_COUNT;
	}
#endif

	FMutableUpdateCandidate MutableUpdateCandidate(this);

	// Clamp Min LOD
	InMinLOD = FMath::Min(FMath::Max(InMinLOD, static_cast<int32>(PrivateData->FirstLODAvailable)), PrivateData->FirstLODAvailable + PrivateData->NumMaxLODsToStream);

	// Clamp Max LOD
	InMaxLOD = FMath::Max(FMath::Min3(InMaxLOD, PrivateData->NumLODsAvailable - 1, (int32)MAX_MESH_LOD_COUNT), InMinLOD);

	const bool bMinMaxLODChanged = Descriptor.MaxLOD != InMaxLOD || Descriptor.MinLOD != InMinLOD;

	const bool bIsDowngradeLODUpdate = GetCurrentMinLOD() >= 0 && InMinLOD > GetCurrentMinLOD();
	PrivateData->SetCOInstanceFlags(bIsDowngradeLODUpdate ? PendingLODsDowngrade : ECONone);

	// Save the new LODs
	MutableUpdateCandidate.MinLOD = InMinLOD;
	MutableUpdateCandidate.MaxLOD = InMaxLOD;
	MutableUpdateCandidate.RequestedLODLevels = Descriptor.GetRequestedLODLevels();

	bool bUpdateRequestedLODs = false;
	if (UCustomizableObjectSystem::GetInstance()->IsOnlyGenerateRequestedLODsEnabled())
	{
		const int32 ComponentCount = GetNumComponents();
		MutableUpdateCandidate.RequestedLODLevels.SetNum(ComponentCount);

		const TArray<uint16>& GeneratedLODsPerComponent = DescriptorRuntimeHash.GetRequestedLODs();

		const bool bIgnoreGeneratedLODs = DescriptorRuntimeHash != UpdateDescriptorRuntimeHash || GeneratedLODsPerComponent.Num() != ComponentCount;

		if (bMinMaxLODChanged || bIgnoreGeneratedLODs || Descriptor.GetRequestedLODLevels() != InRequestedLODsPerComponent)
		{
			check(InRequestedLODsPerComponent.Num() == ComponentCount);

			bUpdateRequestedLODs = bIgnoreGeneratedLODs;
			for (int32 ComponentIndex = 0; ComponentIndex < ComponentCount; ++ComponentIndex)
			{
				// Find the first requested LOD. We'll generate [FirstRequestedLOD ... MaxLOD].
				int32 FirstRequestedLOD = MutableUpdateCandidate.MaxLOD;
				for (int32 LODIndex = 0; LODIndex < MutableUpdateCandidate.MaxLOD; ++LODIndex)
				{
					if ((!bIgnoreGeneratedLODs && GeneratedLODsPerComponent[ComponentIndex] & (1 << LODIndex))
						||
						InRequestedLODsPerComponent[ComponentIndex] & (1 << LODIndex))
					{
						// First RequestedLOD that fall within the range
						FirstRequestedLOD = LODIndex >= MutableUpdateCandidate.MinLOD ? LODIndex : MutableUpdateCandidate.MinLOD;
						break;
					}
				}

				// Generate at least the MaxLOD
				int32 RequestedLODs = 1 << MutableUpdateCandidate.MaxLOD;

				// Mark all LODs up until MAX_MESH_LOD_COUNT since MaxLOD can be set to Max_int32
				for (int32 LODIndex = FirstRequestedLOD; LODIndex < MAX_MESH_LOD_COUNT; ++LODIndex)
				{
					RequestedLODs |= (1 << LODIndex);
				}
				
				bUpdateRequestedLODs |= (RequestedLODs != Descriptor.RequestedLODLevels[ComponentIndex]);

				// Save new RequestedLODs
				MutableUpdateCandidate.RequestedLODLevels[ComponentIndex] = RequestedLODs;
			}
		}
	}

	if (bMinMaxLODChanged || bUpdateRequestedLODs)
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
	return SkeletalMeshes.IsValidIndex(ComponentIndex) ? SkeletalMeshes[ComponentIndex] : nullptr;
}


bool UCustomizableObjectInstance::HasAnySkeletalMesh() const
{
	for (int32 Index = 0; Index < SkeletalMeshes.Num(); ++Index)
	{
		if (SkeletalMeshes[Index])
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
		FString ErrorMsg = FString::Printf(TEXT("Tried to access and invalid component index [%d] in a Mutable Instance."), ComponentIndex);
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
		FString ErrorMsg = FString::Printf(TEXT("Tried to access and invalid component index [%d] in a Mutable Instance."), ComponentIndex);
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

const TArray<FAnimInstanceOverridePhysicsAsset>* UCustomizableInstancePrivateData::GetGeneratedPhysicsAssetsForAnimInstance(TSubclassOf<UAnimInstance> AnimInstanceClass) const
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
		FString ErrorMsg = FString::Printf(TEXT("Tried to access and invalid component index [%d] in a Mutable Instance."), ComponentIndex);
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
