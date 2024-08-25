// Copyright Epic Games, Inc. All Rights Reserved.

#include "GroomBindingAsset.h"

#include "EngineUtils.h"
#include "GeometryCache.h"
#include "GeometryCacheMeshData.h"
#include "GroomAsset.h"
#include "GroomBindingBuilder.h"
#include "HAL/LowLevelMemTracker.h"
#include "Serialization/MemoryReader.h"
#include "UObject/AnimObjectVersion.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/DevObjectVersion.h"
#include "Misc/CoreMisc.h"
#include "Interfaces/ITargetPlatformManagerModule.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GroomBindingAsset)

#if WITH_EDITORONLY_DATA
#include "DerivedDataCache.h"
#include "DerivedDataRequestOwner.h"
#include "DerivedDataCacheInterface.h"
#include "Serialization/LargeMemoryReader.h"
#include "Serialization/LargeMemoryWriter.h"
#include "GroomComponent.h"
#endif

LLM_DECLARE_TAG(Groom);

/////////////////////////////////////////////////////////////////////////////////////////

bool IsHairStrandsDDCLogEnable();
uint32 GetAssetNameHash(const FString& In);

////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void InternalSerializeGuides(FArchive& Ar, UObject* Owner, TArray<FHairStrandsRootBulkData>& Datas)
{
	uint32 MeshLODCount = Datas.Num();
	Ar << MeshLODCount;
	if (Ar.IsLoading())
	{
		Datas.SetNum(MeshLODCount);
	}
	for (FHairStrandsRootBulkData& Data : Datas)
	{
		Data.SerializeHeader(Ar, Owner);
		Data.SerializeData(Ar, Owner);
	}
}

static void InternalSerializeStrands(FArchive& Ar, UObject* Owner, TArray<FHairStrandsRootBulkData>& Datas, uint32 Flags, bool bHeader, bool bData)
{
	Ar.UsingCustomVersion(FAnimObjectVersion::GUID);

	const bool bStripped = (Flags & UGroomAsset::CDSF_StrandsStripped);
	if (!bStripped)
	{
		uint32 MeshLODCount = Datas.Num();
		Ar << MeshLODCount;
		if (Ar.IsLoading())
		{
			Datas.SetNum(MeshLODCount);
		}
		for (FHairStrandsRootBulkData& Data : Datas)
		{
			if (bHeader){ Data.SerializeHeader(Ar, Owner); }
			if (bData)	{ Data.SerializeData(Ar, Owner); }
		}
	}
}

static void InternalSerializeCards(FArchive& Ar, UObject* Owner, TArray<TArray<FHairStrandsRootBulkData>>& Datass)
{
	uint32 CardLODCount = Datass.Num();
	Ar << CardLODCount;
	if (Ar.IsLoading())
	{
		Datass.SetNum(CardLODCount);
	}
	for (TArray<FHairStrandsRootBulkData>& Datas : Datass)
	{
		uint32 MeshLODCount = Datas.Num();
		Ar << MeshLODCount;
		if (Ar.IsLoading())
		{
			Datas.SetNum(MeshLODCount);
		}
		for (FHairStrandsRootBulkData& Data : Datas)
		{	
			Data.SerializeHeader(Ar, Owner);
			Data.SerializeData(Ar, Owner);
		}
	}
}

static void InternalSerializePlatformData(FArchive& Ar, UObject* Owner, UGroomBindingAsset::FHairGroupPlatformData& GroupData, uint32 Flags, bool bHeader, bool bData)
{
	Ar.UsingCustomVersion(FAnimObjectVersion::GUID);

	// Guides
	InternalSerializeGuides(Ar, Owner, GroupData.SimRootBulkDatas);

	// Strands
	InternalSerializeStrands(Ar, Owner, GroupData.RenRootBulkDatas, Flags, bHeader, bData);

	// Cards
	InternalSerializeCards(Ar, Owner, GroupData.CardsRootBulkDatas);
}

static void InternalSerializePlatformDatas(FArchive& Ar, UObject* Owner, TArray<UGroomBindingAsset::FHairGroupPlatformData>& GroupDatas, uint32 Flags)
{
	uint32 GroupCount = GroupDatas.Num();
	Ar << Flags;
	Ar << GroupCount;
	if (Ar.IsLoading())
	{
		GroupDatas.SetNum(GroupCount);
	}
	for (uint32 GroupIt = 0; GroupIt < GroupCount; ++GroupIt)
	{
		InternalSerializePlatformData(Ar, Owner, GroupDatas[GroupIt], Flags, true /*bHeader*/, true /*bData*/);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

void UGroomBindingAsset::Serialize(FArchive& Ar)
{
	uint8 Flags = 0;
#if WITH_EDITOR
	if (GetGroom())
	{
		Flags = GetGroom()->GenerateClassStripFlags(Ar);
	}
#endif

	Super::Serialize(Ar);
	Ar.UsingCustomVersion(FAnimObjectVersion::GUID);
#if WITH_EDITOR
	// When using editor:
	// * The header are loaded in CacheDerivedData(), and the data are streamed from DDC
	// * When cooking, we write out data from the cached cooked platform data
	if (Ar.IsCooking())
	{
		if (TArray<UGroomBindingAsset::FHairGroupPlatformData>* CookedDatas = GetCachedCookedPlatformData(Ar.CookingTarget()))
		{
			InternalSerializePlatformDatas(Ar, this /*Owner*/, *CookedDatas, Flags);
			bIsValid = true;
		}
		else
		{
			UE_LOG(LogHairStrands, Error, TEXT("[Groom] The binding asset (%s) has missing cooked platform data."), *GetName());
		}
	}
#else
	// Always loaded dara from the archive when not using the editor
	{
		InternalSerializePlatformDatas(Ar, this, GetHairGroupsPlatformData(), Flags);
		bIsValid = true;
	}
#endif
}

void UGroomBindingAsset::InitResource()
{
	LLM_SCOPE_BYTAG(Groom);

	TRACE_CPUPROFILER_EVENT_SCOPE(UGroomBindingAsset::InitResource);

	// Ensure we are releasing binding resources before reallocating them
	ReleaseResource(true/*bResetLoadedSize*/);

	for (UGroomBindingAsset::FHairGroupPlatformData& BulkData : GetHairGroupsPlatformData())
	{
		const int32 GroupIndex = GetHairGroupResources().Num();
		FHairGroupResource& Resource = GetHairGroupResources().AddDefaulted_GetRef();

		FHairResourceName ResourceName(GetFName(), GroupIndex);
		const FName OwnerName = GetAssetPathName();

		// Guides
		Resource.SimRootResources = nullptr;
		if (BulkData.SimRootBulkDatas.Num() > 0)
		{
			Resource.SimRootResources = new FHairStrandsRestRootResource(BulkData.SimRootBulkDatas, EHairStrandsResourcesType::Guides, ResourceName, OwnerName);
			Resource.SimRootResources->BeginInitResource();
		}

		// Strands
		Resource.RenRootResources = nullptr;
		if (IsHairStrandsEnabled(EHairStrandsShaderType::Strands) && BulkData.RenRootBulkDatas.Num() > 0)
		{
			Resource.RenRootResources = new FHairStrandsRestRootResource(BulkData.RenRootBulkDatas, EHairStrandsResourcesType::Strands, ResourceName, OwnerName);
			Resource.RenRootResources->BeginInitResource();
		}

		// Cards
		if (IsHairStrandsEnabled(EHairStrandsShaderType::Cards))
		{
			const uint32 CardsLODCount = BulkData.CardsRootBulkDatas.Num();
			Resource.CardsRootResources.SetNum(CardsLODCount);
			for (uint32 CardsLODIt=0; CardsLODIt<CardsLODCount; ++CardsLODIt)
			{
				Resource.CardsRootResources[CardsLODIt] = nullptr;
				if (BulkData.CardsRootBulkDatas[CardsLODIt].Num() > 0)
				{
					Resource.CardsRootResources[CardsLODIt] = new FHairStrandsRestRootResource(BulkData.CardsRootBulkDatas[CardsLODIt], EHairStrandsResourcesType::Cards, FHairResourceName(GetFName(), GroupIndex, CardsLODIt), GetAssetPathName(CardsLODIt));
					Resource.CardsRootResources[CardsLODIt]->BeginInitResource();
				}
			}
		}
	}
}

void UGroomBindingAsset::UpdateResource()
{
	for (FHairGroupResource& Resource : GetHairGroupResources())
	{
		if (Resource.SimRootResources)
		{
			Resource.SimRootResources->BeginUpdateResourceRHI();
		}

		if (Resource.RenRootResources)
		{
			Resource.RenRootResources->BeginUpdateResourceRHI();
		}

		for (FHairStrandsRestRootResource* CardsRootResource : Resource.CardsRootResources)
		{
			if (CardsRootResource)
			{
				CardsRootResource->BeginUpdateResourceRHI();
			}
		}
	}
}

void UGroomBindingAsset::ReleaseResource(bool bResetLoadedSize)
{
	// Delay destruction to insure that the rendering thread is done with all resources usage
	if (GetHairGroupResources().Num() > 0)
	{
		for (FHairGroupResource& Resource : GetHairGroupResources())
		{
			FHairStrandsRestRootResource* InSimRootResources = Resource.SimRootResources;
			FHairStrandsRestRootResource* InRenRootResources = Resource.RenRootResources;
			ENQUEUE_RENDER_COMMAND(ReleaseHairStrandsResourceCommand)(UE::RenderCommandPipe::Groom,
				[InSimRootResources, InRenRootResources, bResetLoadedSize](FRHICommandList& RHICmdList)
			{
				if (InSimRootResources)
				{
					InSimRootResources->ReleaseResource();
					delete InSimRootResources;
				}
				if (InRenRootResources)
				{
					if (bResetLoadedSize)
					{
						InRenRootResources->InternalResetLoadedSize();
					}
					InRenRootResources->ReleaseResource();
					delete InRenRootResources;
				}
			});
			Resource.SimRootResources = nullptr;
			Resource.RenRootResources = nullptr;

			for (FHairStrandsRestRootResource*& InCardsRootResources : Resource.CardsRootResources)
			{
				ENQUEUE_RENDER_COMMAND(ReleaseHairStrandsResourceCommand)(UE::RenderCommandPipe::Groom,
					[InCardsRootResources](FRHICommandList& RHICmdList)
					{
						if (InCardsRootResources)
						{
							InCardsRootResources->ReleaseResource();
							delete InCardsRootResources;
						}
					});
				InCardsRootResources = nullptr;
			}
		}
		GetHairGroupResources().Empty();
	}

	// Process resources to be deleted (should happen only in editor)
	FHairGroupResource ResourceToDelete;
	while (RemoveHairGroupResourcesToDelete(ResourceToDelete))
	{
		FHairStrandsRestRootResource* InSimRootResources = ResourceToDelete.SimRootResources;
		FHairStrandsRestRootResource* InRenRootResources = ResourceToDelete.RenRootResources;
		ENQUEUE_RENDER_COMMAND(ReleaseHairStrandsResourceCommand)(UE::RenderCommandPipe::Groom,
			[InSimRootResources, InRenRootResources](FRHICommandList& RHICmdList)
		{
			if (InSimRootResources)
			{
				InSimRootResources->ReleaseResource();
				delete InSimRootResources;
			}
			if (InRenRootResources)
			{
				InRenRootResources->ReleaseResource();
				delete InRenRootResources;
			}
		});
		//	#hair_todo: release cards root resources
	}
}

void UGroomBindingAsset::Reset()
{
	// No need for resetting LoadedSize as the bulk datas are removed (e.g., when reloading/reimporting a groom asset)
	ReleaseResource(false/*bResetLoadedSize*/);
	for (UGroomBindingAsset::FHairGroupPlatformData& Data : GetHairGroupsPlatformData())
	{
		Data.SimRootBulkDatas.Empty();
		Data.RenRootBulkDatas.Empty();

		for (TArray<FHairStrandsRootBulkData>& CardsRootBulkData : Data.CardsRootBulkDatas)
		{
			CardsRootBulkData.Empty();
		}
	}

	bIsValid = false;
}

#if WITH_EDITORONLY_DATA
void UGroomBindingAsset::InvalidateBinding(class USkeletalMesh*)
{
	CacheDerivedDatas();
}

void UGroomBindingAsset::InvalidateBinding()
{
	CacheDerivedDatas();
}

#endif

void UGroomBindingAsset::PostLoad()
{
	LLM_SCOPE_BYTAG(Groom);

	Super::PostLoad();

	// Compute a hash of the Groom asset fullname for finding unique groom during LOD selection/streaming
	AssetNameHash = GetAssetNameHash(GetFullName());

	if (UGroomAsset* LocalGroom = GetGroom())
	{
		// Make sure that the asset initialized its resources first since the component needs them to initialize its own resources
		LocalGroom->ConditionalPostLoad();

	#if WITH_EDITOR
		CacheDerivedDatas();

		// Sanity check. This function will report back warnings/issues into the log for user.
		UGroomBindingAsset::IsCompatible(LocalGroom, this, true);

		if (USkeletalMesh* LocalTargetSkeletalMesh = GetTargetSkeletalMesh())
		{
			LocalTargetSkeletalMesh->OnPostMeshCached().AddUObject(this, &UGroomBindingAsset::InvalidateBinding);
			bRegisterTargetMeshCallback = true;
		}

		if (USkeletalMesh* LocalSourceSkeletalMesh = GetSourceSkeletalMesh())
		{
			LocalSourceSkeletalMesh->OnPostMeshCached().AddUObject(this, &UGroomBindingAsset::InvalidateBinding);
			bRegisterSourceMeshCallback = true;
		}

		if (LocalGroom)
		{
			LocalGroom->GetOnGroomAssetChanged().AddUObject(this, &UGroomBindingAsset::InvalidateBinding);
			LocalGroom->GetOnGroomAssetResourcesChanged().AddUObject(this, &UGroomBindingAsset::InvalidateBinding);
			bRegisterGroomAssetCallback = true;
		}
	#endif
	}

	// * When running with the editor, InitResource is called in CacheDerivedDatas
	// * When running without the editor, InitResource is explicitely called here
#if !WITH_EDITOR
	if (!IsTemplate() && IsValid())
	{
		InitResource();
	}
#endif
}

void UGroomBindingAsset::PreSave(const class ITargetPlatform* TargetPlatform)
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS;
	Super::PreSave(TargetPlatform);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS;
}

void UGroomBindingAsset::PreSave(FObjectPreSaveContext ObjectSaveContext)
{
	Super::PreSave(ObjectSaveContext);
#if WITH_EDITOR
	OnGroomBindingAssetChanged.Broadcast();
#endif
}

void UGroomBindingAsset::PostSaveRoot(bool bCleanupIsRequired)
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS;
	Super::PostSaveRoot(bCleanupIsRequired);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS;
}

void UGroomBindingAsset::PostSaveRoot(FObjectPostSaveRootContext ObjectSaveContext)
{
	Super::PostSaveRoot(ObjectSaveContext);
#if WITH_EDITOR
	OnGroomBindingAssetChanged.Broadcast();
#endif
}

void UGroomBindingAsset::BeginDestroy()
{
	ReleaseResource(false/*bResetLoadedSize*/);
	Super::BeginDestroy();

#if WITH_EDITOR
	if (GetTargetSkeletalMesh() && bRegisterTargetMeshCallback)
	{
		GetTargetSkeletalMesh()->OnPostMeshCached().RemoveAll(this);
		bRegisterTargetMeshCallback = false;
	}

	if (GetSourceSkeletalMesh() && bRegisterSourceMeshCallback)
	{
		GetSourceSkeletalMesh()->OnPostMeshCached().RemoveAll(this);
		bRegisterSourceMeshCallback = false;
	}

	if (GetGroom() && bRegisterGroomAssetCallback)
	{
		GetGroom()->GetOnGroomAssetResourcesChanged().RemoveAll(this);
		GetGroom()->GetOnGroomAssetChanged().RemoveAll(this);
		bRegisterGroomAssetCallback = false;
	}
#endif
}

bool UGroomBindingAsset::IsCompatible(const USkeletalMesh* InSkeletalMesh, const UGroomBindingAsset* InBinding, bool bIssueWarning)
{
	if (InBinding && InSkeletalMesh && IsHairStrandsBindingEnable())
	{
		if (InBinding->GetGroomBindingType() != EGroomBindingMeshType::SkeletalMesh)
		{
			if (bIssueWarning)
			{
				UE_LOG(LogHairStrands, Warning, TEXT("[Groom] The binding asset (%s) cannot be bound to a SkeletalMesh because it is not the correct binding type."), *InBinding->GetName());
			}
			return false;
		}

		if (!InBinding->GetTargetSkeletalMesh())
		{
			if (bIssueWarning)
			{
				UE_LOG(LogHairStrands, Warning, TEXT("[Groom] The binding asset (%s) does not have a target skeletal mesh. Falling back onto non-binding version."), *InBinding->GetName());
			}
			return false;
		}
		
		// Relax the mismatch of skeletal mesh as this is not necesarely a good metric: something the same skeletal mesh can be imported with/without animation, and all projection data 
		// matches in this case and it is useful to be able to reuse the binding asset in this case
		#if 0
		// TODO: need something better to assess that skeletal meshes match. In the mean time, string comparison. 
		// Since they can be several instances of a skeletalMesh asset (??), a numerical suffix is sometime added to the name (e.g., SkeletalName_0).
		// This is why we are using substring comparison.
		//if (InSkeletalMesh->GetPrimaryAssetId() != InBinding->TargetSkeletalMesh->GetPrimaryAssetId())
		if (!InSkeletalMesh->GetName().Contains(InBinding->TargetSkeletalMesh->GetName()))
		{
			if (bIssueWarning)
			{
				UE_LOG(LogHairStrands, Warning, TEXT("[Groom] The Groom binding (%s) does not reference the same skeletal asset (BindingAsset's skeletal:%s vs. Attached skeletal:%s). The binding asset will not be used."),
					*InBinding->GetName(),
					*InBinding->TargetSkeletalMesh->GetName(),
					*InSkeletalMesh->GetName());
			}
			return false;
		}
		#endif

		for (const FHairGroupResource& Resource : InBinding->GetHairGroupResources())
		{
			if (Resource.SimRootResources && InSkeletalMesh->GetLODNum() != Resource.SimRootResources->GetLODCount())
			{
				if (bIssueWarning)
				{
					UE_LOG(LogHairStrands, Warning, TEXT("[Groom] The Groom binding (%s) does not have the same have LOD count (LOD sim:%d) than the skeletal mesh (%s, LOD:%d). The binding asset will not be used."),
						*InBinding->GetName(),
						Resource.SimRootResources->GetLODCount(),
						*InSkeletalMesh->GetName(),
						InSkeletalMesh->GetLODNum());
				}
				return false;
			}

			if (Resource.RenRootResources && InSkeletalMesh->GetLODNum() != Resource.RenRootResources->GetLODCount() && IsHairStrandsEnabled(EHairStrandsShaderType::Strands))
			{
				if (bIssueWarning)
				{
					UE_LOG(LogHairStrands, Warning, TEXT("[Groom] The Groom binding (%s) does not have the same have LOD count (LOD render:%d) than the skeletal mesh (%s, LOD:%d). The binding asset will not be used."),
						*InBinding->GetName(),
						Resource.RenRootResources->GetLODCount(),
						*InSkeletalMesh->GetName(),
						InSkeletalMesh->GetLODNum());
				}
				return false;
			}
		}
	}

	return true;
}

bool UGroomBindingAsset::IsCompatible(const UGeometryCache* InGeometryCache, const UGroomBindingAsset* InBinding, bool bIssueWarning)
{
	if (InBinding && InGeometryCache && IsHairStrandsBindingEnable())
	{
		if (InBinding->GetGroomBindingType() != EGroomBindingMeshType::GeometryCache)
		{
			if (bIssueWarning)
			{
				UE_LOG(LogHairStrands, Warning, TEXT("[Groom] The binding asset (%s) cannot be bound to a GeometryCache because it is not the correct binding type."), *InBinding->GetName());
			}
			return false;
		}

		if (!InBinding->GetTargetGeometryCache())
		{
			if (bIssueWarning)
			{
				UE_LOG(LogHairStrands, Warning, TEXT("[Groom] The binding asset (%s) does not have a target GeometryCache."), *InBinding->GetName());
			}
			return false;
		}

		TArray<FGeometryCacheMeshData> MeshesData;
		InGeometryCache->GetMeshDataAtTime(0.0f, MeshesData);
		if (MeshesData.Num() > 1)
		{
			if (bIssueWarning)
			{
				UE_LOG(LogHairStrands, Warning, TEXT("[Groom] Cannot be bound to a non-flattened GeometryCache. Re-import %s with 'Flatten Tracks' enabled."), *InGeometryCache->GetName());
			}
			return false;
		}
		else if (MeshesData.Num() == 0)
		{
			if (bIssueWarning)
			{
				UE_LOG(LogHairStrands, Warning, TEXT("[Groom] %s is not a valid GeometryCache to bind to."), *InGeometryCache->GetName());
			}
			return false;
		}
	}

	return true;
}

static bool DoesGroomNeedStrandsBinding(const UGroomAsset* InGroom, uint32 InGroupIndex)
{
	if (IsHairStrandsEnabled(EHairStrandsShaderType::Strands))
	{
		for (const FHairLODSettings& LODSettings : InGroom->GetHairGroupsLOD()[InGroupIndex].LODs)
		{
			if (LODSettings.bVisible && LODSettings.GeometryType == EGroomGeometryType::Strands)
			{
				return true;
			}
		}
	}
	return false;
}

bool UGroomBindingAsset::IsCompatible(const UGroomAsset* InGroom, const UGroomBindingAsset* InBinding, bool bIssueWarning)
{
	if (InBinding && InGroom && IsHairStrandsBindingEnable())
	{
		if (InBinding->GetGroom() && !InBinding->GetGroom()->IsValid())
		{
			// The groom could be invalid if it's still being loaded asynchronously
			return false;
		}

		if (!InBinding->GetGroom())
		{
			UE_CLOG(bIssueWarning, LogHairStrands, Warning, TEXT("[Groom] The binding asset (%s) does not reference a groom. Falling back onto non-binding version."), *InBinding->GetName());
			return false;
		}

		if (InGroom->GetPrimaryAssetId() != InBinding->GetGroom()->GetPrimaryAssetId())
		{
			UE_LOG(LogHairStrands, Warning, TEXT("[Groom] The Groom binding (%s) does not reference the same groom asset (BindingAsset's groom:%s vs. Groom:%s). The binding asset will not be used."), 
				*InBinding->GetName(),
				*InBinding->GetGroom()->GetName(),
				*InGroom->GetName());
			return false;
		}

		const uint32 GroupCount = InGroom->GetNumHairGroups();
		if (GroupCount != InBinding->GetGroupInfos().Num())
		{
			UE_CLOG(bIssueWarning, LogHairStrands, Warning, TEXT("[Groom] The GroomBinding asset (%s) does not contains the same number of groups (%d vs. %d) than the groom (%s). The binding asset will not be used."),
				*InBinding->GetName(),
				GroupCount,
				InBinding->GetGroupInfos().Num(),
				*InGroom->GetName());
			return false;
		}

		for (uint32 GroupIt = 0; GroupIt < GroupCount; ++GroupIt)
		{
			// Guides
			{
				const uint32 GroomCount = InGroom->GetHairGroupsPlatformData()[GroupIt].Guides.BulkData.GetNumCurves();
				const uint32 BindingCount = InBinding->GetGroupInfos()[GroupIt].SimRootCount;

				if (GroomCount != 0 && GroomCount != BindingCount)
				{
					UE_CLOG(bIssueWarning, LogHairStrands, Warning, TEXT("[Groom] The GroomBinding asset (%s) does not contains the same guides in group %d (%d vs. %d) than the groom (%s). The binding asset will not be used."),
						*InBinding->GetName(),
						GroupIt,
						GroomCount,
						BindingCount,
						*InGroom->GetName());
					return false;
				}
			}

			// Strands
			const bool bNeedStrandsRoot = DoesGroomNeedStrandsBinding(InGroom, GroupIt);
			if (bNeedStrandsRoot)
			{
				const uint32 GroomCount = InGroom->GetHairGroupsPlatformData()[GroupIt].Strands.BulkData.GetNumCurves();
				const uint32 BindingCount = InBinding->GetGroupInfos()[GroupIt].RenRootCount;

				// Groom may have stripped strands data so GroomCount would be 0
				if (GroomCount != 0 && GroomCount != BindingCount)
				{
					UE_CLOG(bIssueWarning, LogHairStrands, Warning, TEXT("[Groom] The GroomBinding asset (%s) does not contains the same curves in group %d (%d vs. %d) than the groom (%s). The binding asset will not be used."),
						*InBinding->GetName(),
						GroupIt,
						GroomCount,
						BindingCount,
						*InGroom->GetName());
					return false;
				}
			}
		}
	}
	return true;
}

bool UGroomBindingAsset::IsBindingAssetValid(const UGroomBindingAsset* InBinding, bool bIsBindingReloading, bool bIssueWarning)
{
	if (InBinding && IsHairStrandsBindingEnable())
	{
		if (!InBinding->IsValid())
		{
			if (bIssueWarning)
			{
				UE_LOG(LogHairStrands, Warning, TEXT("[Groom] The binding asset (%s) is invalid. It failed to load or build. Falling back onto non-binding version."), *InBinding->GetName());
			}
			return false;
		}

		if (!InBinding->GetGroom())
		{
			if (bIssueWarning)
			{
				UE_LOG(LogHairStrands, Warning, TEXT("[Groom] The binding asset (%s) does not reference a groom. Falling back onto non-binding version."), *InBinding->GetName());
			}
			return false;
		}
		if (InBinding->GetGroupInfos().Num() == 0)
		{
			if (bIssueWarning)
			{
				UE_LOG(LogHairStrands, Warning, TEXT("[Groom] The GroomBinding asset (Groom:%s) does not contain any groups. It is invalid and can't be assigned. The binding asset will not be used."), *InBinding->GetGroom()->GetName());
			}
			return false;
		}

		uint32 GroupIt = 0;
		for (const FGoomBindingGroupInfo& Info : InBinding->GetGroupInfos())
		{
			if (Info.SimRootCount == 0)
			{
				if (bIssueWarning)
				{
					UE_LOG(LogHairStrands, Warning, TEXT("[Groom] The GroomBinding asset (Groom:%s) has group with 0 guides. It is invalid and can't be assigned. The binding asset will not be used."), *InBinding->GetGroom()->GetName());
				}
				return false;
			}

			const bool bNeedStrandsRoot = DoesGroomNeedStrandsBinding(InBinding->GetGroom(), GroupIt);
			if (bNeedStrandsRoot && Info.RenRootCount == 0 && IsHairStrandsEnabled(EHairStrandsShaderType::Strands))
			{
				if (bIssueWarning)
				{
					UE_LOG(LogHairStrands, Warning, TEXT("[Groom] The GroomBinding asset (Groom:%s) has group with 0 curves. It is invalid and can't be assigned. The binding asset will not be used."), *InBinding->GetGroom()->GetName());
				}
				return false;
			}

			++GroupIt;
		}
	}
	return true;
}

bool UGroomBindingAsset::HasValidTarget() const
{
	return (GetGroomBindingType() == EGroomBindingMeshType::SkeletalMesh && GetTargetSkeletalMesh()) ||
		   (GetGroomBindingType() == EGroomBindingMeshType::GeometryCache && GetTargetGeometryCache());
}

#if WITH_EDITOR

void UGroomBindingAsset::Build()
{
	if (GetGroom() && HasValidTarget())
	{
		OnGroomBindingAssetChanged.Broadcast();
		Reset();
		CacheDerivedDatas();
	}
}

void UGroomBindingAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	CacheDerivedDatas();
	OnGroomBindingAssetChanged.Broadcast();
}
#endif // WITH_EDITOR

void UpdateGroomBindingAssetInfos(UGroomBindingAsset* In)
{
	if (In)
	{
		const uint32 GroupCount = In->GetHairGroupsPlatformData().Num();
		In->GetGroupInfos().SetNum(GroupCount);
		for (uint32 GroupIt=0; GroupIt< GroupCount; ++GroupIt)
		{
			FGoomBindingGroupInfo& Info = In->GetGroupInfos()[GroupIt];
			const UGroomBindingAsset::FHairGroupPlatformData& BulkData = In->GetHairGroupsPlatformData()[GroupIt];
			Info.SimRootCount = BulkData.SimRootBulkDatas.Num() > 0 ? BulkData.SimRootBulkDatas[0].GetRootCount() : 0u;
			Info.SimLODCount  = BulkData.SimRootBulkDatas.Num();

			Info.RenRootCount = BulkData.RenRootBulkDatas.Num() > 0 ? BulkData.RenRootBulkDatas[0].GetRootCount() : 0u;
			Info.RenLODCount  = BulkData.RenRootBulkDatas.Num();
		}
	}
}

#if WITH_EDITORONLY_DATA

// If groom binding derived data needs to be rebuilt (new format, serialization
// differences, etc.) replace the version GUID below with a new one.
// In case of merge conflicts with DDC versions, you MUST generate a new GUID
// and set this new GUID as the version.
// DDC Guid needs to be bumped in:
// * Main    : Engine\Source\Runtime\Core\Private\UObject\DevObjectVersion.cpp
// * Release : Engine\Source\Runtime\Core\Private\UObject\UE5ReleaseStreamObjectVersion.cpp
// * ...

namespace GroomBindingDerivedDataCacheUtils
{
	const FString& GetGroomBindingDerivedDataVersion()
	{
		static FString CachedVersionString = FDevSystemGuids::GetSystemGuid(FDevSystemGuids::Get().GROOM_BINDING_DERIVED_DATA_VERSION).ToString();
		return CachedVersionString;
	}

	FString BuildGroomBindingDerivedDataKey(const FString& KeySuffix)
	{
		return FDerivedDataCacheInterface::BuildCacheKey(*(TEXT("GROOM_BINDING_V") + FGroomBindingBuilder::GetVersion() + TEXT("_")), *GetGroomBindingDerivedDataVersion(), *KeySuffix);
	}
}

static FString BuildDerivedDataKeySuffix(const UGroomBindingAsset& BindingAsset, const ITargetPlatform* TargetPlatform)
{
	FString BindingType;
	FString SourceKey;
	FString TargetKey;

	if (BindingAsset.GetGroomBindingType() == EGroomBindingMeshType::SkeletalMesh)
	{
		// Binding type is implicitly SkeletalMesh so keep BindingType empty to prevent triggering rebuild of old binding for nothing
		SourceKey = BindingAsset.GetSourceSkeletalMesh() ? BindingAsset.GetSourceSkeletalMesh()->BuildDerivedDataKey(TargetPlatform) : FString();
		TargetKey = BindingAsset.GetTargetSkeletalMesh() ? BindingAsset.GetTargetSkeletalMesh()->BuildDerivedDataKey(TargetPlatform) : FString();
	}
	else
	{
		BindingType = "GEOCACHE_";
		SourceKey = BindingAsset.GetSourceGeometryCache() ? BindingAsset.GetSourceGeometryCache()->GetHash() : FString();
		TargetKey = BindingAsset.GetTargetGeometryCache() ? BindingAsset.GetTargetGeometryCache()->GetHash() : FString();
	}
	// When possible, use the GroomAsset 'cached DDC key'. This allows to avoid a bug where the DDC key would change 
	// when loading GroomAsset's hair description, which would modify the hair description hash ID with legacy content.
	FString GroomKey  = BindingAsset.GetGroom() ? BindingAsset.GetGroom()->GetDerivedDataKey(true /*bUseCachedKey*/) : FString();
	FString PointKey  = FString::FromInt(BindingAsset.GetNumInterpolationPoints());
	FString SectionKey = FString::FromInt(BindingAsset.GetMatchingSection());

	uint32 KeyLength  = BindingType.Len() + SourceKey.Len() + TargetKey.Len() + GroomKey.Len() + PointKey.Len() + SectionKey.Len();

	FString KeySuffix;
	KeySuffix.Reserve(KeyLength);
	KeySuffix = BindingType + SourceKey + TargetKey + GroomKey + PointKey + SectionKey;
	return KeySuffix;
}

static FString BuildDerivedDataKeyGroup(const UGroomBindingAsset& BindingAsset, const ITargetPlatform* TargetPlatform, uint32 InGroupIndex)
{
	const FString DeriveDataKeySuffix = BuildDerivedDataKeySuffix(BindingAsset, TargetPlatform);
	return GroomBindingDerivedDataCacheUtils::BuildGroomBindingDerivedDataKey(DeriveDataKeySuffix + FString(TEXT("_Group")) + FString::FromInt(InGroupIndex));
}

static FString BuildDerivedDataKeyGroup(const FString& InDeriveDataKeySuffix , uint32 InGroupIndex)
{
	return GroomBindingDerivedDataCacheUtils::BuildGroomBindingDerivedDataKey(InDeriveDataKeySuffix + FString(TEXT("_Group")) + FString::FromInt(InGroupIndex));
}

static TArray<FString> GetGroupDerivedDataKeys(const UGroomBindingAsset* In, const ITargetPlatform* TargetPlatform);
static void CacheDerivedDatas(UGroomBindingAsset* In, const uint32 InGroupIndex, const FString& DerivedDataKey, bool& bOutValid, const ITargetPlatform* TargetPlatform, UGroomBindingAsset::FHairGroupPlatformData& OutPlatformData);

void UGroomBindingAsset::CacheDerivedDatas()
{
	if (!GetGroom() || !GetGroom()->IsValid())
	{
		return;
	}

	// 1. Set group count to the groom target
	const uint32 GroupCount = GetGroom()->GetNumHairGroups();
	GetHairGroupsPlatformData().SetNum(GroupCount);
	CachedDerivedDataKey.SetNum(GroupCount);
	GetGroupInfos().SetNum(GroupCount);

	// 2.1 Prepare main cache key
	// Cache derived data for the running platform.
	ITargetPlatform* RunningPlatform = GetTargetPlatformManagerRef().GetRunningTargetPlatform();
	check(RunningPlatform);
	const FString KeySuffix = BuildDerivedDataKeySuffix(*this, RunningPlatform);

	// 2.2 Build the key for each group and check if any group needs to be rebuilt
	const TArray<FString> GroupDerivedDataKeys = GetGroupDerivedDataKeys(this, RunningPlatform);
	const bool bAnyGroupNeedRebuild = GroupDerivedDataKeys != CachedDerivedDataKey;

	// 3. Build or retrieve from cache, binding data for each group
	bIsValid = true;
	bool bReloadResource = false;
	if (bAnyGroupNeedRebuild)
	{
		FGroomComponentRecreateRenderStateContext RecreateRenderContext(GetGroom());

		for (uint32 GroupIndex = 0; GroupIndex < GroupCount; ++GroupIndex)
		{
			// 1. Build data
			bool bGroupValid = true;
			bool bGroupReloadResource = false;
			if (GroupDerivedDataKeys[GroupIndex] != CachedDerivedDataKey[GroupIndex])
			{
				::CacheDerivedDatas(this, GroupIndex, GroupDerivedDataKeys[GroupIndex], bGroupValid, RunningPlatform, GetHairGroupsPlatformData()[GroupIndex]);
	
				if (bGroupValid)
				{
					bGroupReloadResource = true;
					CachedDerivedDataKey[GroupIndex] = GroupDerivedDataKeys[GroupIndex] ;
				}
				else
				{
					UE_LOG(LogHairStrands, Error, TEXT("[Groom] The binding asset (%s) couldn't be built. This binding asset won't be used."), *GetName());
				}
			}
	
			// 2. Release existing resources data
			if (bGroupReloadResource)
			{
				UGroomBindingAsset::FHairGroupResources& OutHairGroupResources = GetHairGroupResources();
				if (OutHairGroupResources.Num() > 0)
				{
					for (UGroomBindingAsset::FHairGroupResource& GroupResources : OutHairGroupResources)
					{
						AddHairGroupResourcesToDelete(GroupResources);
					}
					OutHairGroupResources.Empty();
				}
				check(OutHairGroupResources.Num() == 0);
			}

			bIsValid = bIsValid && bGroupValid;
			bReloadResource = bReloadResource || bGroupReloadResource;
		}

		// 3. Update binding infos here as they need to be valid when RecreateRenderContext is deleted
		//    When RecreateRenderContext's Dtor is called, it will recreate component, which will run 
		//    the binding validation to assess if the binding asset is compatible. This validation logic 
		//    use the binding infos to know if curve count match between GroomAsset and GroomBindingAsset
		UpdateGroomBindingAssetInfos(this);

		// 4. Reload resources if needed
		if (bReloadResource)
		{
			InitResource();
		}
	}
	else
	{
		// 3. Patch hair group info if it does not match the DDC-read/deserialized data
		UpdateGroomBindingAssetInfos(this);
	}
}

static void CacheDerivedDatas(UGroomBindingAsset* In, const uint32 InGroupIndex, const FString& DerivedDataKey, bool& bOutValid, const ITargetPlatform* TargetPlatform, UGroomBindingAsset::FHairGroupPlatformData& OutPlatformData)
{
	{
		bOutValid = false;
		using namespace UE::DerivedData;

		const FCacheKey HeaderKey = ConvertLegacyCacheKey(DerivedDataKey + FString(TEXT("_Header")));
		const FSharedString Name = MakeStringView(In->GetPathName());
		FSharedBuffer Data;
		{
			FRequestOwner Owner(EPriority::Blocking);
			GetCache().GetValue({ {Name, HeaderKey} }, Owner, [&Data](FCacheGetValueResponse&& Response)
			{
				Data = Response.Value.GetData().Decompress();
			});
			Owner.Wait();
		}

		// Populate key/name for streaming data request
		auto FillDrivedDataKey = [&DerivedDataKey, &Name](UGroomBindingAsset::FHairGroupPlatformData& In)
		{
			for (uint32 MeshLODIndex = 0, MeshLODCount=In.RenRootBulkDatas.Num(); MeshLODIndex < MeshLODCount; ++MeshLODIndex)
			{
				In.RenRootBulkDatas[MeshLODIndex].DerivedDataKey = DerivedDataKey + FString::Printf(TEXT("_RenRootData_MeshLOD%d"), MeshLODIndex);
			}
		};

		bool bHasDataInCache = false;
		if (Data)
		{
			UE_CLOG(IsHairStrandsDDCLogEnable(), LogHairStrands, Log, TEXT("[GroomBinding/DDC] Found (GroomBinding:%s)."), *In->GetName());

			// Header
			FMemoryReaderView Ar(Data, /*bIsPersistent*/ true);
			InternalSerializePlatformData(Ar, In, OutPlatformData, 0 /*Flags*/, true /*bHeader*/, false /*bData*/);
			bHasDataInCache = true;

			// Fill DDC key for each strands LOD root bulk data. Done after InternalSerializePlatformData(), as RenRootBulkDatas is not filled in yet, and OutPlatformData.RenRootBulkDatas.Num() == 0.
			FillDrivedDataKey(OutPlatformData);

			// Verify that all strands data are correctly cached into the DDC
			{
				for (int32 MeshLODIndex = 0, MeshLODCount = OutPlatformData.RenRootBulkDatas.Num(); MeshLODIndex < MeshLODCount; ++MeshLODIndex)
				{
					FHairStreamingRequest R; bHasDataInCache &= R.WarmCache(HAIR_MAX_NUM_CURVE_PER_GROUP, HAIR_MAX_NUM_POINT_PER_GROUP, OutPlatformData.RenRootBulkDatas[MeshLODIndex]);
				}
			}

			bOutValid = true;
		}
		if (!bHasDataInCache)
		{
			UE_CLOG(IsHairStrandsDDCLogEnable(), LogHairStrands, Log, TEXT("[GroomBinding/DDC] Not found (GroomBinding:%s)."), *In->GetName());

			// Build groom binding data
			FGroomBindingBuilder::FInput BuilderInput;
			BuilderInput.BindingType = In->GetGroomBindingType();
			BuilderInput.NumInterpolationPoints = In->GetNumInterpolationPoints();
			BuilderInput.MatchingSection = In->GetMatchingSection();
			BuilderInput.bHasValidTarget = In->HasValidTarget();
			BuilderInput.GroomAsset = In->GetGroom();
			BuilderInput.SourceSkeletalMesh = In->GetSourceSkeletalMesh();
			BuilderInput.TargetSkeletalMesh = In->GetTargetSkeletalMesh();
			BuilderInput.SourceGeometryCache = In->GetSourceGeometryCache();
			BuilderInput.TargetGeometryCache = In->GetTargetGeometryCache();

			bOutValid = FGroomBindingBuilder::BuildBinding(BuilderInput, InGroupIndex, TargetPlatform, OutPlatformData);

			if (bOutValid)
			{
				FillDrivedDataKey(OutPlatformData);

				// Header
				{
					TArray<uint8> WriteData;
					FMemoryWriter Ar(WriteData, /*bIsPersistent*/ true);
					InternalSerializePlatformData(Ar, In, OutPlatformData, 0 /*Flags*/, true /*bHeader*/, false /*bData*/);
	
					FRequestOwner AsyncOwner(EPriority::Normal);
					GetCache().PutValue({ {Name, HeaderKey, FValue::Compress(MakeSharedBufferFromArray(MoveTemp(WriteData)))} }, AsyncOwner);
					AsyncOwner.KeepAlive();
				}
	
				// Data
				for (uint32 MeshLODIndex=0, MeshLODCount = OutPlatformData.RenRootBulkDatas.Num(); MeshLODIndex<MeshLODCount;++MeshLODIndex)
				{
					TArray<FCachePutValueRequest> Out;
					OutPlatformData.RenRootBulkDatas[MeshLODIndex].Write_DDC(In, Out);

					FRequestOwner AsyncOwner(EPriority::Normal);
					GetCache().PutValue(Out, AsyncOwner);
					AsyncOwner.KeepAlive();
				}
			}
		}
	}
}

static TArray<FString> GetGroupDerivedDataKeys(const UGroomBindingAsset* In, const ITargetPlatform* TargetPlatform)
{
	check(In);
	check(TargetPlatform);

	const FString KeySuffix = BuildDerivedDataKeySuffix(*In, TargetPlatform);
	const uint32 GroupCount = In->GetGroupInfos().Num();

	TArray<FString> Out;
	Out.SetNum(GroupCount);
	for (uint32 GroupIndex = 0; GroupIndex < GroupCount; ++GroupIndex)
	{
		Out[GroupIndex] = BuildDerivedDataKeyGroup(KeySuffix, GroupIndex);
	}
	return Out;
}

static UGroomBindingAsset::FCachedCookedPlatformData* FindCachedCookedPlatformData(const TArray<FString>& InGroupKeys, TArray<UGroomBindingAsset::FCachedCookedPlatformData*>& InCachedCookedData)
{
	for (UGroomBindingAsset::FCachedCookedPlatformData* CookedPlatformData : InCachedCookedData)
	{
		if (CookedPlatformData->GroupDerivedDataKeys == InGroupKeys)
		{
			return CookedPlatformData;
		}
	}
	return nullptr;
}

void UGroomBindingAsset::BeginCacheForCookedPlatformData(const ITargetPlatform* TargetPlatform)
{
	Super::BeginCacheForCookedPlatformData(TargetPlatform);

	// 1. Build the key for each group
	const TArray<FString> GroupDerivedDataKeys = GetGroupDerivedDataKeys(this, TargetPlatform);

	// 2. Find existing cached cooked data
	UGroomBindingAsset::FCachedCookedPlatformData* TargetPlatformData = FindCachedCookedPlatformData(GroupDerivedDataKeys, CachedCookedPlatformDatas);

	// 3. If the target cooked data does not already exist, we build it
	if (TargetPlatformData == nullptr && GetGroom() != nullptr)
	{
		// 3.1 Build cooked derived data
		const uint32 GroupCount = GroupDerivedDataKeys.Num();
		TargetPlatformData = new FCachedCookedPlatformData();
		TargetPlatformData->GroupDerivedDataKeys = GroupDerivedDataKeys;
		TargetPlatformData->GroupPlatformDatas.SetNum(GroupCount);
		for (uint32 GroupIndex = 0; GroupIndex < GroupCount; ++GroupIndex)
		{
			bool bGroupValid = true;
			::CacheDerivedDatas(this, GroupIndex, TargetPlatformData->GroupDerivedDataKeys[GroupIndex], bGroupValid, TargetPlatform, TargetPlatformData->GroupPlatformDatas[GroupIndex]);
			if (!bGroupValid)
			{
				UE_LOG(LogHairStrands, Error, TEXT("[Groom] The binding asset (%s) couldn't be built. This binding asset won't be used."), *GetName());
			}
		}

		// 3.2 Place cooked derived data into their bulk data. 
		// This is done only for strands, which support DDC streaming
		// When cooking data, force loading of *all* bulk data prior to saving them
		// Note: bFillBulkdata is true for filling in the bulkdata container prior to serialization. This also forces the resources loading 
		// from the 'start' (i.e., without offset)
		for (uint32 GroupIndex = 0; GroupIndex < GroupCount; ++GroupIndex)
		{
			TArray<FHairStrandsRootBulkData>& RenRootBulkDatas = TargetPlatformData->GroupPlatformDatas[GroupIndex].RenRootBulkDatas;
			for (int32 MeshLODIndex = 0, MeshLODCount = RenRootBulkDatas.Num(); MeshLODIndex < MeshLODCount; ++MeshLODIndex)
			{
				FHairStreamingRequest R; R.Request(HAIR_MAX_NUM_CURVE_PER_GROUP, HAIR_MAX_NUM_POINT_PER_GROUP, RenRootBulkDatas[MeshLODIndex], true /*bWait*/, true /*bFillBulkdata*/, true /*bWarmCache*/, GetFName());
			}
		}

		CachedCookedPlatformDatas.Add(TargetPlatformData);
	}
}

TArray<UGroomBindingAsset::FHairGroupPlatformData>* UGroomBindingAsset::GetCachedCookedPlatformData(const ITargetPlatform* TargetPlatform)
{
	// 1. Build the key for each group
	const TArray<FString> GroupDerivedDataKeys = GetGroupDerivedDataKeys(this, TargetPlatform);

	// 2. Find existing cached cooked data
	if (UGroomBindingAsset::FCachedCookedPlatformData* CachedCookedPlatformData = FindCachedCookedPlatformData(GroupDerivedDataKeys, CachedCookedPlatformDatas))
	{
		return &CachedCookedPlatformData->GroupPlatformDatas;
	}
	else
	{
		return nullptr;
	}
}

void UGroomBindingAsset::ClearAllCachedCookedPlatformData()
{
	for (UGroomBindingAsset::FCachedCookedPlatformData* CookedPlatformData : CachedCookedPlatformDatas)
	{
		delete CookedPlatformData;
	}
	CachedCookedPlatformDatas.Empty();

	Super::ClearAllCachedCookedPlatformData();
}

#endif

void UGroomBindingAsset::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(GetHairGroupsPlatformData().GetAllocatedSize());

	for (const FHairGroupResource& Group : GetHairGroupResources())
	{
		if (Group.SimRootResources) CumulativeResourceSize.AddDedicatedVideoMemoryBytes(Group.SimRootResources->GetResourcesSize());
		if (Group.RenRootResources) CumulativeResourceSize.AddDedicatedVideoMemoryBytes(Group.RenRootResources->GetResourcesSize());
		for (const FHairStrandsRestRootResource* CardsRootResource : Group.CardsRootResources)
		{
			if (CardsRootResource) CumulativeResourceSize.AddDedicatedVideoMemoryBytes(CardsRootResource->GetResourcesSize());
		}
	}
}

FName UGroomBindingAsset::GetAssetPathName(int32 LODIndex)
{
#if RHI_ENABLE_RESOURCE_INFO
	if (LODIndex > -1)
	{
		return FName(FString::Printf(TEXT("%s [LOD%d]"), *GetPathName(), LODIndex));
	}
	else
	{
		return FName(GetPathName());
	}
#else
	return NAME_None;
#endif
}

#define DEFINE_GROOMBINDING_MEMBER_ACCESSOR(Type, Access, Name, GetConst, SetConst)\
	FName UGroomBindingAsset::Get##Name##MemberName()\
	{\
		PRAGMA_DISABLE_DEPRECATION_WARNINGS\
		return FName(TEXT(#Name));\
		PRAGMA_ENABLE_DEPRECATION_WARNINGS\
	}\
	UFUNCTION(BlueprintGetter)\
	GetConst Type Access UGroomBindingAsset::Get##Name() const\
	{\
		PRAGMA_DISABLE_DEPRECATION_WARNINGS\
		return Name;\
		PRAGMA_ENABLE_DEPRECATION_WARNINGS\
	}\
	UFUNCTION(BlueprintSetter)\
	void UGroomBindingAsset::Set##Name(SetConst Type Access In##Name)\
	{\
		PRAGMA_DISABLE_DEPRECATION_WARNINGS\
		Name = In##Name;\
		PRAGMA_ENABLE_DEPRECATION_WARNINGS\
	}

// Define most of the binding member accessor
DEFINE_GROOMBINDING_MEMBER_ACCESSOR(EGroomBindingMeshType, , GroomBindingType, , );
DEFINE_GROOMBINDING_MEMBER_ACCESSOR(UGroomAsset, *, Groom, , );
DEFINE_GROOMBINDING_MEMBER_ACCESSOR(USkeletalMesh, *, SourceSkeletalMesh, , );
DEFINE_GROOMBINDING_MEMBER_ACCESSOR(USkeletalMesh, *, TargetSkeletalMesh, , );
DEFINE_GROOMBINDING_MEMBER_ACCESSOR(UGeometryCache, *, SourceGeometryCache, , );
DEFINE_GROOMBINDING_MEMBER_ACCESSOR(UGeometryCache, *, TargetGeometryCache, , );
DEFINE_GROOMBINDING_MEMBER_ACCESSOR(int32, , NumInterpolationPoints, , );
DEFINE_GROOMBINDING_MEMBER_ACCESSOR(int32, , MatchingSection, , );
DEFINE_GROOMBINDING_MEMBER_ACCESSOR(TArray<FGoomBindingGroupInfo>, &, GroupInfos, const, const);

TArray<FGoomBindingGroupInfo>& UGroomBindingAsset::GetGroupInfos()
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	return GroupInfos;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

void UGroomBindingAsset::AddHairGroupResourcesToDelete(FHairGroupResource& In)
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	HairGroupResourcesToDelete.Enqueue(In);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

bool UGroomBindingAsset::RemoveHairGroupResourcesToDelete(FHairGroupResource& Out)
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	return HairGroupResourcesToDelete.Dequeue(Out);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

FName UGroomBindingAsset::GetHairGroupResourcesMemberName()
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	return GET_MEMBER_NAME_CHECKED(UGroomBindingAsset, GroupInfos);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

UGroomBindingAsset::FHairGroupResources& UGroomBindingAsset::GetHairGroupResources()
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	return HairGroupResources;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

const UGroomBindingAsset::FHairGroupResources& UGroomBindingAsset::GetHairGroupResources() const
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	return HairGroupResources;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

void UGroomBindingAsset::SetHairGroupResources(UGroomBindingAsset::FHairGroupResources InHairGroupResources)
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	HairGroupResources = InHairGroupResources;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

FName UGroomBindingAsset::GetHairGroupPlatformDataMemberName()
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	return GET_MEMBER_NAME_CHECKED(UGroomBindingAsset, HairGroupsPlatformData);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

const TArray<UGroomBindingAsset::FHairGroupPlatformData>& UGroomBindingAsset::GetHairGroupsPlatformData() const
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	return HairGroupsPlatformData;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

TArray<UGroomBindingAsset::FHairGroupPlatformData>& UGroomBindingAsset::GetHairGroupsPlatformData()
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	return HairGroupsPlatformData;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

#if WITH_EDITOR
void UGroomBindingAsset::RecreateResources()
{
	ReleaseResource(true /*bResetLoadedSize*/);
	InitResource();
	OnGroomBindingAssetChanged.Broadcast();
}

void UGroomBindingAsset::ChangeFeatureLevel(ERHIFeatureLevel::Type In)
{
	// When changing feature level, recreate resources to the correct feature level
	if (CachedResourcesFeatureLevel != In)
	{
		RecreateResources();
		CachedResourcesFeatureLevel = In;
	}
}

void UGroomBindingAsset::ChangePlatformLevel(ERHIFeatureLevel::Type In)
{
	// When changing platform preview level, recreate resources to the correct platform settings (e.g., r.hairstrands.strands=0/1)
	if (CachedResourcesPlatformLevel != In)
	{
		RecreateResources();
		CachedResourcesPlatformLevel = In;
	}
}
#endif // WITH_EDITOR