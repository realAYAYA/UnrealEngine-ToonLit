// Copyright Epic Games, Inc. All Rights Reserved.

#include "SteamVRAssetManager.h"
#include "ISteamVRPlugin.h" // STEAMVR_SUPPORTED_PLATFORMS
#include "ProceduralMeshComponent.h"
#include "Tickable.h" // for FTickableGameObject
#include "Engine/Engine.h" // for GEngine
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture2D.h"
#include "UObject/Package.h"
#include "UObject/GCObject.h"
#include "Logging/LogMacros.h"

#if STEAMVR_SUPPORTED_PLATFORMS
	#include "SteamVRHMD.h"
	#include <openvr.h>
#endif 

class FSteamVRHMD;
namespace vr { struct RenderModel_t; }
namespace vr { class IVRRenderModels; }
namespace vr { struct RenderModel_TextureMap_t; }

/* SteamVRDevice_Impl 
 *****************************************************************************/

namespace SteamVRDevice_Impl
{
	static FSteamVRHMD* GetSteamHMD();
	static int32 GetDeviceStringProperty(int32 DeviceIndex, int32 PropertyId, FString& StringPropertyOut);
}

static FSteamVRHMD* SteamVRDevice_Impl::GetSteamHMD()
{
#if STEAMVR_SUPPORTED_PLATFORMS
	if (GEngine->XRSystem.IsValid() && (GEngine->XRSystem->GetSystemName() == FSteamVRHMD::SteamSystemName))
	{
		return static_cast<FSteamVRHMD*>(GEngine->XRSystem.Get());
	}
#endif
	return nullptr;
}

// @TODO: move this to a shared util library
static int32 SteamVRDevice_Impl::GetDeviceStringProperty(int32 DeviceIndex, int32 PropertyId, FString& StringPropertyOut)
{
	int32 ErrorResult = -1;
#if STEAMVR_SUPPORTED_PLATFORMS
	vr::IVRSystem* SteamVRSystem = vr::VRSystem();
	if (SteamVRSystem)
	{
		vr::ETrackedDeviceProperty SteamPropId = (vr::ETrackedDeviceProperty)PropertyId;

		vr::TrackedPropertyError APIError;
		TArray<char> Buffer;
		Buffer.AddUninitialized(vr::k_unMaxPropertyStringSize);

		int Size = SteamVRSystem->GetStringTrackedDeviceProperty(DeviceIndex, SteamPropId, Buffer.GetData(), Buffer.Num(), &APIError);
		if (APIError == vr::TrackedProp_BufferTooSmall)
		{
			Buffer.AddUninitialized(Size - Buffer.Num());
			Size = SteamVRSystem->GetStringTrackedDeviceProperty(DeviceIndex, SteamPropId, Buffer.GetData(), Buffer.Num(), &APIError);
		}

		if (APIError == vr::TrackedProp_Success)
		{
			StringPropertyOut = UTF8_TO_TCHAR(Buffer.GetData());
		}
		else
		{
			StringPropertyOut = UTF8_TO_TCHAR(SteamVRSystem->GetPropErrorNameFromEnum(APIError));
		}
		ErrorResult = (int32)APIError;
	}
#endif 
	return ErrorResult;
}


/* TSteamVRResourceLoader
 *****************************************************************************/
template<typename ResType, typename IDType>
class TSteamVRResourceLoader
{
public:
	TSteamVRResourceLoader() {}

	bool LoadResource_Async(const IDType& ResID, TSharedPtr<ResType>& ResourceOut)
	{
#if STEAMVR_SUPPORTED_PLATFORMS
		vr::IVRRenderModels* VRModelManager = GetSteamVRModelManager();
		if (VRModelManager)
		{
			ResType* RawResourcePtr;
			vr::EVRRenderModelError err = LoadResource_Internal(VRModelManager, ResID, &RawResourcePtr);
			if (err == vr::VRRenderModelError_None)
			{
				ResourceOut = WrapResource(RawResourcePtr);
				return true;
			}
			else
			{
				return err == vr::VRRenderModelError_Loading;
			}
		}
#endif
		return false;
	}

private:
	static vr::IVRRenderModels* GetSteamVRModelManager()
	{
		vr::IVRRenderModels* VRModelManager = nullptr;
#if STEAMVR_SUPPORTED_PLATFORMS
		VRModelManager = vr::VRRenderModels();
#endif
		return VRModelManager;
	}

	// When loading the same resource id multiple times, SteamVR will keep returning the same pointer until it is freed. 
	// We wrap it in a TSharedPtr and save a weak reference to it so we can deallocate it when we no longer have 
	// references to it.	
	TSharedPtr<ResType> WrapResource(ResType* RawResourcePtr)
	{
		if (!RawResourcePtr)
		{
			return TSharedPtr<ResType>();
		}
		else
		{
			// First check if we already have created a shared pointer for this resource
			TWeakPtr<ResType>* Existing = ActiveResources.Find(RawResourcePtr);
			if (Existing)
			{
				// The custom deleter will remove the value from the map before the weak pointer becomes invalid,
				// and it should point to the same object as the naked pointer passed in.
				check(Existing->IsValid() && Existing->HasSameObject(RawResourcePtr));
				UE_LOG(LogSteamVR, Log, TEXT("TSteamVRResourceLoader::WrapResource: Returning existing resource %p"), RawResourcePtr);
				return Existing->Pin();
			}
			else
			{
				UE_LOG(LogSteamVR, Log, TEXT("TSteamVRResourceLoader::WrapResource: Creating new TSharedPtr for resource %p"), RawResourcePtr);
				// Construct a new shared pointer with a custom deleter.
				TSharedPtr<ResType> NewPtr = MakeShareable(RawResourcePtr, [this](ResType* Instance)
				{
					FreeResource(Instance);
				});
				// Save a weak reference so we only construct a single shared pointer for each unique resource.
				ActiveResources.Emplace(RawResourcePtr, NewPtr);
				return NewPtr;
			}
		}
	}

	void FreeResource(ResType* RawResourcePtr)
	{
		vr::IVRRenderModels* VRModelManager = GetSteamVRModelManager();
		if (VRModelManager)
		{
			FreeResource_Internal(VRModelManager, RawResourcePtr);
		}
		else
		{
			UE_LOG(LogSteamVR, Warning, TEXT("FSteamVRModelLoader::FreeResource: Could not get SteamVR model manager when freeing render model instance %p."), RawResourcePtr);
		}
		ActiveResources.Remove(RawResourcePtr);
	}

#if STEAMVR_SUPPORTED_PLATFORMS
	static vr::EVRRenderModelError LoadResource_Internal(vr::IVRRenderModels* VRModelManager, const IDType& ResID, ResType** ResourceOut);
#endif
	static void FreeResource_Internal(vr::IVRRenderModels* VRModelManager, ResType* RawResourcePtr);


	TMap<ResType*, TWeakPtr<ResType>> ActiveResources;
};
typedef TSteamVRResourceLoader<vr::RenderModel_t, FString> FSteamVRModelLoader;
typedef TSteamVRResourceLoader<vr::RenderModel_TextureMap_t, int32> FSteamVRTextureLoader;

/// @cond DOXYGEN_WARNINGS
#if STEAMVR_SUPPORTED_PLATFORMS
template<>
vr::EVRRenderModelError FSteamVRModelLoader::LoadResource_Internal(vr::IVRRenderModels* VRModelManager, const FString& ResourceId, vr::RenderModel_t** RawResourceOut)
{
	return VRModelManager->LoadRenderModel_Async(TCHAR_TO_UTF8(*ResourceId), RawResourceOut);
}
#endif
/// @endcond

template<>
void FSteamVRModelLoader::FreeResource_Internal(vr::IVRRenderModels* VRModelManager, vr::RenderModel_t* RawResourcePtr)
{
#if STEAMVR_SUPPORTED_PLATFORMS
	check(VRModelManager);
	UE_LOG(LogSteamVR, Log, TEXT("FSteamVRModelLoader::FreeResource_Internal: Freeing render model instance %p"), RawResourcePtr);
	VRModelManager->FreeRenderModel(RawResourcePtr);
#endif
}

/// @cond DOXYGEN_WARNINGS
#if STEAMVR_SUPPORTED_PLATFORMS
template<>
vr::EVRRenderModelError FSteamVRTextureLoader::LoadResource_Internal(vr::IVRRenderModels* VRModelManager, const int32& ResourceId, vr::RenderModel_TextureMap_t** RawResourceOut)
{
	return VRModelManager->LoadTexture_Async(ResourceId, RawResourceOut);
}
#endif
/// @endcond

template<>
void FSteamVRTextureLoader::FreeResource_Internal(vr::IVRRenderModels* VRModelManager, vr::RenderModel_TextureMap_t* RawResourcePtr)
{
#if STEAMVR_SUPPORTED_PLATFORMS
	check(VRModelManager);
	UE_LOG(LogSteamVR, Log, TEXT("FSteamVRTextureLoader::FreeResource_Internal: Freeing texture resource %p"), RawResourcePtr);
	VRModelManager->FreeTexture(RawResourcePtr);
#endif
}

/* TSteamVRResource
 *****************************************************************************/

template< typename ResType, typename IDType >
struct TSteamVRResource
{
private:
	typedef TSteamVRResourceLoader<ResType, IDType> FResourceLoader;
	static  FResourceLoader ResourceLoader;

public:
	TSteamVRResource(const IDType& ResID, bool bKickOffLoad = true)
		: ResourceId(ResID)
		, RawResource()
		, bLoadFailed(false)
	{
		if (bKickOffLoad)
		{
			TickAsyncLoad();
		}
	}

	bool IsPending()
	{
		return !RawResource.IsValid() && !bLoadFailed;
	}

	bool IsValid()
	{
		return RawResource.IsValid();
	}

	TSharedPtr<ResType> TickAsyncLoad()
	{
		if (IsPending())
		{
			bLoadFailed = !ResourceLoader.LoadResource_Async(ResourceId, RawResource);
			if (bLoadFailed)
			{
				RawResource.Reset();
			}
		}
		return RawResource;
	}

	operator const ResType*() const { return RawResource.Get(); }
	const ResType* operator->() const { return RawResource.Get(); }

	IDType GetId() const  { return ResourceId;  }

protected:
	IDType				ResourceId;
	TSharedPtr<ResType> RawResource;
	bool				bLoadFailed;
};

template<typename ResType, typename IDType>
TSteamVRResourceLoader<ResType, IDType> TSteamVRResource<ResType, IDType>::ResourceLoader;

/* FSteamVRModel 
 *****************************************************************************/

typedef TSteamVRResource<vr::RenderModel_t, FString> TSteamVRModel;
struct FSteamVRModel : public TSteamVRModel
{
public:
	FSteamVRModel(const FString& ResID, bool bKickOffLoad = true)
		: TSteamVRModel(ResID, bKickOffLoad)
	{}
public:
	bool GetRawMeshData(float UEMeterScale, FSteamVRMeshData& MeshDataOut);
};

struct FSteamVRMeshData
{
	TArray<FVector> VertPositions;
	TArray<int32> Indices;
	TArray<FVector2D> UVs;
	TArray<FVector> Normals;
	TArray<FColor> VertColors;
	TArray<FProcMeshTangent> Tangents;
};

bool FSteamVRModel::GetRawMeshData(float UEMeterScale, FSteamVRMeshData& MeshDataOut)
{
	bool bIsValidData = RawResource.IsValid();
#if STEAMVR_SUPPORTED_PLATFORMS
	if (bIsValidData)
	{
		// Logging to try to get info about a crash where the RawMeshData seems to be bad.
		UE_LOG(LogSteamVR, Log, TEXT("FSteamVRModel::GetRawMeshData ResourceId %s rVertexData 0x%lx unVertexCount %i rIndexData 0x%lx unTriangleCount %i diffuseTextureId %i"), *ResourceId, RawResource->rVertexData, RawResource->unVertexCount, RawResource->rIndexData, RawResource->unTriangleCount, RawResource->diffuseTextureId);

		const uint32 VertCount = RawResource->unVertexCount;
		MeshDataOut.VertPositions.Empty(VertCount);
		MeshDataOut.UVs.Empty(VertCount);
		MeshDataOut.Normals.Empty(VertCount);

		const uint32 TriCount = RawResource->unTriangleCount;
		const uint32 IndxCount = TriCount * 3;
		MeshDataOut.Indices.Empty(IndxCount);

		// @TODO: move this into a shared utility class
		auto SteamVecToFVec = [](const vr::HmdVector3_t SteamVec)
		{
			return FVector(-SteamVec.v[2], SteamVec.v[0], SteamVec.v[1]);
		};

		for (uint32 VertIndex = 0; VertIndex < VertCount; ++VertIndex)
		{
			const vr::RenderModel_Vertex_t& VertData = RawResource->rVertexData[VertIndex];

			const vr::HmdVector3_t& VertPos = VertData.vPosition;
			MeshDataOut.VertPositions.Add(SteamVecToFVec(VertPos) * UEMeterScale);

			const FVector2D VertUv = FVector2D(VertData.rfTextureCoord[0], VertData.rfTextureCoord[1]);
			MeshDataOut.UVs.Add(VertUv);

			MeshDataOut.Normals.Add(SteamVecToFVec(VertData.vNormal));
		}

		for (uint32 Indice = 0; Indice < IndxCount; ++Indice)
		{
			MeshDataOut.Indices.Add((int32)RawResource->rIndexData[Indice]);
		}
	}
#endif
	return bIsValidData;
}

/* FSteamVRTexture 
 *****************************************************************************/

typedef TSteamVRResource<vr::RenderModel_TextureMap_t, int32> TSteamVRTexture;
struct FSteamVRTexture : public TSteamVRTexture
{
public:
	FSteamVRTexture(int32 ResID, bool bKickOffLoad = true)
		: TSteamVRTexture(ResID, bKickOffLoad)
	{}

	int32 GetResourceID() const { return ResourceId; }

public:
	UTexture2D* ConstructUETexture(const FName ObjName)
	{
		UTexture2D* NewTexture = nullptr;

#if STEAMVR_SUPPORTED_PLATFORMS
		if (RawResource.IsValid())
		{
			// Create the texture. Using UTexture2D::CreateTransient which is supported outside of editor builds.
			NewTexture = UTexture2D::CreateTransient(RawResource->unWidth, RawResource->unHeight, EPixelFormat::PF_R8G8B8A8, ObjName);
			if (NewTexture != nullptr)
			{
				FTexture2DMipMap& Mip = NewTexture->GetPlatformData()->Mips[0];
				void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
				FMemory::Memcpy(Data, (void*)RawResource->rubTextureMapData, Mip.BulkData.GetBulkDataSize());
				Mip.BulkData.Unlock();
				NewTexture->GetPlatformData()->SetNumSlices(1);
#if WITH_EDITORONLY_DATA
				NewTexture->CompressionNone = true;
				NewTexture->DeferCompression = false;
				NewTexture->MipGenSettings = TMGS_NoMipmaps;
#endif				
				NewTexture->UpdateResource();
			}
		}
#endif

		return NewTexture;
	}
};

/* FSteamVRAsyncMeshLoader 
 *****************************************************************************/


DECLARE_DELEGATE(FOnSteamVRModelAsyncLoadDone);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnSteamVRSubMeshLoaded, int32, const FSteamVRMeshData&, UTexture2D*);
DECLARE_MULTICAST_DELEGATE(FOnSteamVRModelLoadComplete);

class FSteamVRAsyncMeshLoader : public FTickableGameObject, public FGCObject
{
public:
	FSteamVRAsyncMeshLoader(const float WorldMetersScaleIn);

	/** */
	void SetLoaderFinishedCallback(const FOnSteamVRModelAsyncLoadDone& OnLoaderFinished);
	/** */
	int32 EnqueMeshLoad(const FString& ModelName);
	/** */
	FOnSteamVRSubMeshLoaded& OnSubMeshLoaded();
	/** */
	FOnSteamVRModelLoadComplete& OnLoadComplete();

public:
	//~ FTickableObjectBase interface
	
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;

public:
	//~ FTickableGameObject interface

	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual bool IsTickableInEditor() const override   { return true; }

public:
	//~ FGCObject interface

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("FSteamVRAsyncMeshLoader");
	}

protected:
	/** */
	bool EnqueueTextureLoad(int32 SubMeshIndex, TSharedPtr<vr::RenderModel_t> RenderModel);
	/** */
	void OnLoadComplete(int32 SubMeshIndex);

private:
	int32 PendingLoadCount;
	float WorldMetersScale;
	FOnSteamVRModelAsyncLoadDone LoaderDoneCallback;
	FOnSteamVRSubMeshLoaded SubMeshLoadedDelegate;
	FOnSteamVRModelLoadComplete LoadCompleteDelegate;

	TArray<FSteamVRModel>    EnqueuedMeshes;
	TArray<FSteamVRTexture>  EnqueuedTextures;
	TMap<int32, int32>       PendingTextureLoads;
	TMap<int32, UTexture2D*> ConstructedTextures;
};

FSteamVRAsyncMeshLoader::FSteamVRAsyncMeshLoader(const float WorldMetersScaleIn)
	: PendingLoadCount(0)
	, WorldMetersScale(WorldMetersScaleIn)
{}

void FSteamVRAsyncMeshLoader::SetLoaderFinishedCallback(const FOnSteamVRModelAsyncLoadDone& InLoaderDoneCallback)
{
	LoaderDoneCallback = InLoaderDoneCallback;
}

int32 FSteamVRAsyncMeshLoader::EnqueMeshLoad(const FString& ModelName)
{
	int32 MeshIndex = INDEX_NONE;
	if (!ModelName.IsEmpty())
	{
		++PendingLoadCount;
		MeshIndex = EnqueuedMeshes.Emplace( ModelName );
	}
	return MeshIndex;
}

FOnSteamVRSubMeshLoaded& FSteamVRAsyncMeshLoader::OnSubMeshLoaded()
{
	return SubMeshLoadedDelegate;
}

FOnSteamVRModelLoadComplete& FSteamVRAsyncMeshLoader::OnLoadComplete()
{
	return LoadCompleteDelegate;
}

void FSteamVRAsyncMeshLoader::Tick(float /*DeltaTime*/)
{
	for (int32 SubMeshIndex = 0; SubMeshIndex < EnqueuedMeshes.Num(); ++SubMeshIndex)
	{
		FSteamVRModel& ModelResource = EnqueuedMeshes[SubMeshIndex];
		if (ModelResource.IsPending())
		{
			auto RenderModel = ModelResource.TickAsyncLoad();
			if (!ModelResource.IsPending())
			{
				--PendingLoadCount;

				if (!RenderModel)
				{
					// valid index + missing RenderModel => signifies failure
					OnLoadComplete(SubMeshIndex);
				}
#if STEAMVR_SUPPORTED_PLATFORMS
				// if we've already loaded and converted the texture
				else if (ConstructedTextures.Contains(RenderModel->diffuseTextureId))
				{
					OnLoadComplete(SubMeshIndex);
				}
#endif // STEAMVR_SUPPORTED_PLATFORMS
				else if (!EnqueueTextureLoad(SubMeshIndex, RenderModel))
				{
					// if we fail to load the texture, we'll have to do without it
					OnLoadComplete(SubMeshIndex);
				}
			}
		}
	}

	for (int32 TexIndex = 0; TexIndex < EnqueuedTextures.Num(); ++TexIndex)
	{
		FSteamVRTexture& TextureResource = EnqueuedTextures[TexIndex];
		if (TextureResource.IsPending())
		{
			const bool bLoadSuccess = (TextureResource.TickAsyncLoad() != nullptr);
			if (!TextureResource.IsPending())
			{
				--PendingLoadCount;

				if (bLoadSuccess)
				{
					FName    TextureName  = *FString::Printf(TEXT("T_SteamVR_%d"), TextureResource.GetResourceID());

					UTexture2D* UETexture = FindObjectFast<UTexture2D>(GetTransientPackage(), TextureName, /*ExactClass =*/true);
					if (UETexture == nullptr)
					{
						UETexture = TextureResource.ConstructUETexture(TextureName);
					}
					ConstructedTextures.Add(TextureResource.GetResourceID(), UETexture);
				}

				int32* ModelIndexPtr = PendingTextureLoads.Find(TexIndex);
				if ( ensure(ModelIndexPtr != nullptr && EnqueuedMeshes.IsValidIndex(*ModelIndexPtr)) )
				{
					FSteamVRModel& AssociatedModel = EnqueuedMeshes[*ModelIndexPtr];
					OnLoadComplete(*ModelIndexPtr);
				}	
			}
		}
	}

	if (PendingLoadCount <= 0)
	{
		LoadCompleteDelegate.Broadcast();
		// has to happen last thing, as this will delete this async loader
		LoaderDoneCallback.ExecuteIfBound();
	}
}

bool FSteamVRAsyncMeshLoader::IsTickable() const
{
	return (PendingLoadCount > 0);
}

TStatId FSteamVRAsyncMeshLoader::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FSteamVRAsyncMeshLoader, STATGROUP_Tickables);
}

void FSteamVRAsyncMeshLoader::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObjects(ConstructedTextures);
}

bool FSteamVRAsyncMeshLoader::EnqueueTextureLoad(int32 SubMeshIndex, TSharedPtr<vr::RenderModel_t> RenderModel)
{
	bool bLoadEnqueued = false;
#if STEAMVR_SUPPORTED_PLATFORMS
	if (RenderModel && RenderModel->diffuseTextureId != vr::INVALID_TEXTURE_ID)
	{
		++PendingLoadCount;
		bLoadEnqueued = true;

		// load will be kicked off later on in Tick() loop (no need to do it twice in the same tick)
		int32 TextureIndex = EnqueuedTextures.Emplace( RenderModel->diffuseTextureId, /*bKickOffLoad =*/false );
		PendingTextureLoads.Add(TextureIndex, SubMeshIndex);
	}
#endif
	return bLoadEnqueued;
}

void FSteamVRAsyncMeshLoader::OnLoadComplete(int32 SubMeshIndex)
{
	FSteamVRMeshData RawMeshData;
	UTexture2D* Texture = nullptr;

	if (EnqueuedMeshes.IsValidIndex(SubMeshIndex))
	{
		FSteamVRModel& LoadedModel = EnqueuedMeshes[SubMeshIndex];

		if (LoadedModel.IsValid())
		{
#if STEAMVR_SUPPORTED_PLATFORMS
			// trying to handle an illusive crash where the loaded model data appears to be bad... 
			// technically we can handle when there is no diffuse texture, but it may be indicative 
			// of a larger issue (we expect all steamVR models to be textured)
			const bool bHasMalformData = (LoadedModel->diffuseTextureId == vr::INVALID_TEXTURE_ID);
			UE_CLOG(bHasMalformData, LogSteamVR, Warning, TEXT("Loaded what appears to be malformed model data for SteamVR model (0x%08x): \n"
				"\t %s \n" 
				"\t Vert count: %d \n" 
				"\t Tri  count: %d \n" 
			"Treating as a load failure (no model will be spawned)!"), (const vr::RenderModel_t*)LoadedModel, *LoadedModel.GetId(), LoadedModel->unVertexCount, LoadedModel->unTriangleCount);

			if (!bHasMalformData)
			{
				LoadedModel.GetRawMeshData(WorldMetersScale, RawMeshData);
			}
			// else, skip polling mesh data as there may be a crash with vert/index buffer count mismatch

			UTexture2D** CachedTexturePtr = ConstructedTextures.Find(LoadedModel->diffuseTextureId);
			if (CachedTexturePtr)
			{
				Texture = *CachedTexturePtr;
			}
#endif // STEAMVR_SUPPORTED_PLATFORMS
		}
	}
	SubMeshLoadedDelegate.Broadcast(SubMeshIndex, RawMeshData, Texture);
}


/* FSteamVRAssetManager 
 *****************************************************************************/

FSteamVRAssetManager::FSteamVRAssetManager()
	: DefaultDeviceMat(FString(TEXT("/SteamVR/Materials/M_DefaultDevice.M_DefaultDevice")))
{
	IModularFeatures::Get().RegisterModularFeature(IXRSystemAssets::GetModularFeatureName(), this);
}

FSteamVRAssetManager::~FSteamVRAssetManager()
{
	IModularFeatures::Get().UnregisterModularFeature(IXRSystemAssets::GetModularFeatureName(), this);
}

bool FSteamVRAssetManager::EnumerateRenderableDevices(TArray<int32>& DeviceListOut)
{
	bool bHasActiveVRSystem = false;

#if STEAMVR_SUPPORTED_PLATFORMS
	FSteamVRHMD* SteamHMD = SteamVRDevice_Impl::GetSteamHMD();
	bHasActiveVRSystem = SteamHMD && SteamHMD->IsInitialized();
	
	if (bHasActiveVRSystem)
	{
		DeviceListOut.Empty();

		for (uint32 DeviceIndex = 0; DeviceIndex < vr::k_unMaxTrackedDeviceCount; ++DeviceIndex)
		{
			// Add only devices with a currently valid tracked pose
			if (SteamHMD->IsTracking(DeviceIndex))
			{
				DeviceListOut.Add(DeviceIndex);
			}
		}
	}
#endif
	return bHasActiveVRSystem;
}

int32 FSteamVRAssetManager::GetDeviceId(EControllerHand ControllerHand)
{
	int32 DeviceIndexOut = INDEX_NONE;

#if STEAMVR_SUPPORTED_PLATFORMS
	vr::IVRSystem* SteamVRSystem = vr::VRSystem();
	if (SteamVRSystem)
	{
		vr::ETrackedDeviceClass DesiredDeviceClass = vr::ETrackedDeviceClass::TrackedDeviceClass_Invalid;
		vr::ETrackedControllerRole DesiredControllerRole = vr::ETrackedControllerRole::TrackedControllerRole_Invalid;

		switch (ControllerHand)
		{
		case EControllerHand::Left:
			DesiredControllerRole = vr::TrackedControllerRole_LeftHand;
			DesiredDeviceClass = vr::ETrackedDeviceClass::TrackedDeviceClass_Controller;
			break;
		case EControllerHand::Right:
			DesiredControllerRole = vr::TrackedControllerRole_RightHand;
			DesiredDeviceClass = vr::ETrackedDeviceClass::TrackedDeviceClass_Controller;
			break;
		case EControllerHand::AnyHand:
			DesiredDeviceClass = vr::ETrackedDeviceClass::TrackedDeviceClass_Controller;
			break;

		case EControllerHand::ExternalCamera:
			DesiredDeviceClass = vr::ETrackedDeviceClass::TrackedDeviceClass_TrackingReference;
			break;

		default:
			DesiredDeviceClass = vr::ETrackedDeviceClass::TrackedDeviceClass_GenericTracker;
			break;
		}

		if (DesiredDeviceClass != vr::TrackedDeviceClass_Invalid)
		{
			int32 FallbackIndex = INDEX_NONE;

			for (uint32 DeviceIndex = 0; DeviceIndex < vr::k_unMaxTrackedDeviceCount; ++DeviceIndex)
			{
				const vr::ETrackedDeviceClass DeviceClass = SteamVRSystem->GetTrackedDeviceClass(DeviceIndex);
				if (DeviceClass == DesiredDeviceClass)
				{
					if (DesiredControllerRole != vr::TrackedControllerRole_Invalid)
					{
						// NOTE: GetControllerRoleForTrackedDeviceIndex() only seems to return a valid role if the device is on and being tracked
						const vr::ETrackedControllerRole ControllerRole = SteamVRSystem->GetControllerRoleForTrackedDeviceIndex(DeviceIndex);
						if (ControllerRole == vr::TrackedControllerRole_Invalid && FallbackIndex == INDEX_NONE)
						{
							FallbackIndex = DeviceIndex;
						}
						else if (ControllerRole != DesiredControllerRole)
						{
							continue;
						}
					}

					DeviceIndexOut = DeviceIndex;
					break;
				}
			}

			if (DeviceIndexOut == INDEX_NONE)
			{
				DeviceIndexOut = FallbackIndex;
			}
		}
	}
#endif
	return DeviceIndexOut;
}

UPrimitiveComponent* FSteamVRAssetManager::CreateRenderComponent(const int32 DeviceId, AActor* Owner, EObjectFlags Flags, const bool bForceSynchronous, const FXRComponentLoadComplete& OnLoadComplete)
{
	UPrimitiveComponent* NewRenderComponent = nullptr;

#if STEAMVR_SUPPORTED_PLATFORMS

	FString ModelName;
	if (SteamVRDevice_Impl::GetDeviceStringProperty(DeviceId, vr::Prop_RenderModelName_String, ModelName) == vr::TrackedProp_Success)
	{
		vr::IVRRenderModels* VRModelManager = vr::VRRenderModels();

		if (VRModelManager != nullptr)
		{
			const FString BaseComponentName = FString::Printf(TEXT("%s_%s"), TEXT("SteamVR"), *ModelName);
			const FName ComponentObjName = MakeUniqueObjectName(Owner, UProceduralMeshComponent::StaticClass(), *BaseComponentName);
			UProceduralMeshComponent* ProceduralMesh = NewObject<UProceduralMeshComponent>(Owner, ComponentObjName, Flags);

			float MeterScale = 1.f;
			if (UWorld* World = Owner->GetWorld())
			{
				AWorldSettings* WorldSettings = World->GetWorldSettings();
				if (WorldSettings)
				{
					MeterScale = WorldSettings->WorldToMeters;
				}
			}

			TWeakPtr<FSteamVRAsyncMeshLoader> AssignedMeshLoader;
			if (TSharedPtr<FSteamVRAsyncMeshLoader>* ExistingLoader = ActiveMeshLoaders.Find(ModelName))
			{
				AssignedMeshLoader = *ExistingLoader;
			}
			else
			{
				TSharedPtr<FSteamVRAsyncMeshLoader> NewMeshLoader = MakeShareable(new FSteamVRAsyncMeshLoader(MeterScale));

				FOnSteamVRModelAsyncLoadDone LoadHandler;
				LoadHandler.BindRaw(this, &FSteamVRAssetManager::OnModelFullyLoaded, ModelName);
				NewMeshLoader->SetLoaderFinishedCallback(LoadHandler);

				const auto RawModelNameConvert = TStringConversion<FTCHARToUTF8_Convert>(*ModelName);
				const char* RawModelName = RawModelNameConvert.Get();
				const uint32 SubMeshCount = VRModelManager->GetComponentCount(RawModelName);

				if (SubMeshCount > 0)
				{
					TArray<char> NameBuffer;
					NameBuffer.AddUninitialized(vr::k_unMaxPropertyStringSize);

					for (uint32 SubMeshIndex = 0; SubMeshIndex < SubMeshCount; ++SubMeshIndex)
					{
						uint32 NeededSize = VRModelManager->GetComponentName(RawModelName, SubMeshIndex, NameBuffer.GetData(), NameBuffer.Num());
						if (NeededSize == 0)
						{
							continue;
						}
						else if (NeededSize > (uint32)NameBuffer.Num())
						{
							NameBuffer.AddUninitialized(NeededSize - NameBuffer.Num());
							VRModelManager->GetComponentName(RawModelName, SubMeshIndex, NameBuffer.GetData(), NameBuffer.Num());
						}
					
						FString ComponentName = UTF8_TO_TCHAR(NameBuffer.GetData());
						// arbitrary pieces that are not present on the physical device
						// @TODO: probably useful for something, should figure out their purpose (battery readout? handedness?)
						if (ComponentName == TEXT("status") ||
							ComponentName == TEXT("scroll_wheel") ||
							ComponentName == TEXT("trackpad_scroll_cut") ||
							ComponentName == TEXT("trackpad_touch"))
						{
							continue;
						}

						NeededSize = VRModelManager->GetComponentRenderModelName(RawModelName, TCHAR_TO_UTF8(*ComponentName), NameBuffer.GetData(), NameBuffer.Num());
						if (NeededSize == 0)
						{
							continue;
						}
						else if (NeededSize > (uint32)NameBuffer.Num())
						{
							NameBuffer.AddUninitialized(NeededSize - NameBuffer.Num());
							NeededSize = VRModelManager->GetComponentRenderModelName(RawModelName, TCHAR_TO_UTF8(*ComponentName), NameBuffer.GetData(), NameBuffer.Num());
						}

						FString ComponentModelName = UTF8_TO_TCHAR(NameBuffer.GetData());
						NewMeshLoader->EnqueMeshLoad(ComponentModelName);
					}
				}
				else
				{
					NewMeshLoader->EnqueMeshLoad(ModelName);
				}

				AssignedMeshLoader = NewMeshLoader;
				ActiveMeshLoaders.Add(ModelName, NewMeshLoader);
			}
			
			FAsyncLoadData CallbackPayload;
			CallbackPayload.ComponentPtr = ProceduralMesh;
			CallbackPayload.LoadedModelName = ModelName;

			AssignedMeshLoader.Pin()->OnSubMeshLoaded().AddRaw(this, &FSteamVRAssetManager::OnMeshLoaded, CallbackPayload);
			AssignedMeshLoader.Pin()->OnLoadComplete().AddRaw(this, &FSteamVRAssetManager::OnComponentLoadComplete, CallbackPayload.ComponentPtr, OnLoadComplete);

			NewRenderComponent = ProceduralMesh;

			while (bForceSynchronous && AssignedMeshLoader.IsValid())
			{
				FPlatformProcess::Sleep(0.0f);
				AssignedMeshLoader.Pin()->Tick(0.0f);
			}
		}
		else
		{
			// failure...
			OnLoadComplete.ExecuteIfBound(nullptr);
		}
	}
	else
#endif
	{
		// failure...
		OnLoadComplete.ExecuteIfBound(nullptr);
	}
	return NewRenderComponent;
}

void FSteamVRAssetManager::OnMeshLoaded(int32 SubMeshIndex, const FSteamVRMeshData& MeshData, UTexture2D* DiffuseTex, FAsyncLoadData LoadData)
{
	if (MeshData.VertPositions.Num() > 0 && LoadData.ComponentPtr.IsValid())
	{
		LoadData.ComponentPtr->CreateMeshSection(SubMeshIndex
			, MeshData.VertPositions
			, MeshData.Indices
			, MeshData.Normals
			, MeshData.UVs
			, MeshData.VertColors
			, MeshData.Tangents
			, /*bCreateCollision =*/false);

		if (DiffuseTex != nullptr)
		{
			UMaterial* DefaultMaterial = DefaultDeviceMat.LoadSynchronous();
			if (DefaultMaterial)
			{
				const FName MatName = MakeUniqueObjectName(GetTransientPackage(), UMaterialInstanceDynamic::StaticClass(), *FString::Printf(TEXT("M_%s_SubMesh%d"), *LoadData.ComponentPtr->GetName(), SubMeshIndex));
				UMaterialInstanceDynamic* MeshMaterial = UMaterialInstanceDynamic::Create(DefaultMaterial, LoadData.ComponentPtr.Get(), MatName);

				MeshMaterial->SetTextureParameterValue(TEXT("DiffuseTex"), DiffuseTex);
				LoadData.ComponentPtr->SetMaterial(SubMeshIndex, MeshMaterial);
			}
		}
	}
	else
	{
		UE_CLOG(MeshData.VertPositions.Num() <= 0, LogSteamVR, Warning, TEXT("Loaded empty sub-mesh for SteamVR device model: '%s'"), *LoadData.LoadedModelName);
	}
}

void FSteamVRAssetManager::OnComponentLoadComplete(TWeakObjectPtr<UProceduralMeshComponent> ComponentPtr, FXRComponentLoadComplete LoadCompleteCallback)
{
	LoadCompleteCallback.ExecuteIfBound(ComponentPtr.Get());
}

void FSteamVRAssetManager::OnModelFullyLoaded(FString ModelName)
{
	ActiveMeshLoaders.Remove(ModelName);
}
