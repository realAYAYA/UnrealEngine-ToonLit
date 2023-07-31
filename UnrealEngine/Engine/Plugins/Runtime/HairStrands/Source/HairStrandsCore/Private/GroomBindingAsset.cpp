// Copyright Epic Games, Inc. All Rights Reserved.

#include "GroomBindingAsset.h"

#include "EngineUtils.h"
#include "GeometryCache.h"
#include "GeometryCacheMeshData.h"
#include "GroomAsset.h"
#include "GroomBindingBuilder.h"
#include "HAL/LowLevelMemTracker.h"
#include "UObject/ObjectSaveContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GroomBindingAsset)

#if WITH_EDITORONLY_DATA
#include "DerivedDataCacheInterface.h"
#include "Serialization/LargeMemoryReader.h"
#include "Serialization/LargeMemoryWriter.h"
#include "GroomComponent.h"
#endif

LLM_DECLARE_TAG(Groom);

/////////////////////////////////////////////////////////////////////////////////////////

bool IsHairStrandsDDCLogEnable();

////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void InternalSerialize(FArchive& Ar, UObject* Owner, TArray<FHairStrandsRootBulkData>& GroupDatas)
{
	uint32 GroupCount = GroupDatas.Num();
	Ar << GroupCount;
	if (Ar.IsLoading())
	{
		GroupDatas.SetNum(GroupCount);
	}
	for (uint32 GroupIt = 0; GroupIt < GroupCount; ++GroupIt)
	{
		GroupDatas[GroupIt].Serialize(Ar, Owner);
	}
}

static void InternalSerialize(FArchive& Ar, UObject* Owner, FHairGroupBulkData& GroupData, uint32 Flags)
{
	Ar.UsingCustomVersion(FAnimObjectVersion::GUID);
	GroupData.SimRootBulkData.Serialize(Ar, Owner);
	if (!(Flags & UGroomAsset::CDSF_StrandsStripped))
	{
		GroupData.RenRootBulkData.Serialize(Ar, Owner);
	}
	if (Ar.CustomVer(FAnimObjectVersion::GUID) >= FAnimObjectVersion::SerializeHairBindingAsset)
	{
		InternalSerialize(Ar, Owner, GroupData.CardsRootBulkData);
	}
}

static void InternalSerialize(FArchive& Ar, UObject* Owner, TArray<FHairGroupBulkData>& GroupDatas, uint32 Flags)
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
		InternalSerialize(Ar, Owner, GroupDatas[GroupIt], Flags);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

void UGroomBindingAsset::Serialize(FArchive& Ar)
{
	uint8 Flags = 0;
#if WITH_EDITOR
	if (Groom)
	{
		Flags = Groom->GenerateClassStripFlags(Ar);
	}
#endif

	Super::Serialize(Ar);
	Ar.UsingCustomVersion(FAnimObjectVersion::GUID);
#if WITH_EDITOR
	if (Ar.CustomVer(FAnimObjectVersion::GUID) < FAnimObjectVersion::GroomBindingSerialization || Ar.IsCooking())
#endif
	{
		InternalSerialize(Ar, this, HairGroupBulkDatas, Flags);
		bIsValid = true;
	}
}

void UGroomBindingAsset::InitResource()
{
	LLM_SCOPE_BYTAG(Groom);

	// Ensure we are releasing binding resources before reallocating them
	ReleaseResource();

	for (FHairGroupBulkData& BulkData : HairGroupBulkDatas)
	{
		const int32 GroupIndex = HairGroupResources.Num();
		FHairGroupResource& Resource = HairGroupResources.AddDefaulted_GetRef();

		FHairResourceName ResourceName(GetFName(), GroupIndex);

		// Guides
		Resource.SimRootResources = nullptr;
		if (BulkData.SimRootBulkData.IsValid())
		{
			Resource.SimRootResources = new FHairStrandsRestRootResource(BulkData.SimRootBulkData, EHairStrandsResourcesType::Guides, ResourceName);
			BeginInitResource(Resource.SimRootResources);
		}

		// Strands
		Resource.RenRootResources = nullptr;
		if (IsHairStrandsEnabled(EHairStrandsShaderType::Strands) && BulkData.RenRootBulkData.IsValid())
		{
			Resource.RenRootResources = new FHairStrandsRestRootResource(BulkData.RenRootBulkData, EHairStrandsResourcesType::Strands, ResourceName);
			BeginInitResource(Resource.RenRootResources);
		}

		// Cards
		if (IsHairStrandsEnabled(EHairStrandsShaderType::Cards))
		{
			const uint32 CardsLODCount = BulkData.CardsRootBulkData.Num();
			Resource.CardsRootResources.SetNum(CardsLODCount);
			for (uint32 CardsLODIt=0; CardsLODIt<CardsLODCount; ++CardsLODIt)
			{
				Resource.CardsRootResources[CardsLODIt] = nullptr;
				if (BulkData.CardsRootBulkData[CardsLODIt].IsValid())
				{
					Resource.CardsRootResources[CardsLODIt] = new FHairStrandsRestRootResource(BulkData.CardsRootBulkData[CardsLODIt], EHairStrandsResourcesType::Cards, FHairResourceName(GetFName(), GroupIndex, CardsLODIt));
					BeginInitResource(Resource.CardsRootResources[CardsLODIt]);
				}
			}
		}
	}
}

void UGroomBindingAsset::UpdateResource()
{
	for (FHairGroupResource& Resource : HairGroupResources)
	{
		if (Resource.SimRootResources)
		{
			BeginUpdateResourceRHI(Resource.SimRootResources);
		}

		if (Resource.RenRootResources)
		{
			BeginUpdateResourceRHI(Resource.RenRootResources);
		}

		for (FHairStrandsRestRootResource* CardsRootResource : Resource.CardsRootResources)
		{
			if (CardsRootResource)
			{
				BeginUpdateResourceRHI(CardsRootResource);
			}
		}
	}
}

void UGroomBindingAsset::ReleaseResource()
{
	// Delay destruction to insure that the rendering thread is done with all resources usage
	if (HairGroupResources.Num() > 0)
	{
		for (FHairGroupResource& Resource : HairGroupResources)
		{
			FHairStrandsRestRootResource* InSimRootResources = Resource.SimRootResources;
			FHairStrandsRestRootResource* InRenRootResources = Resource.RenRootResources;
			ENQUEUE_RENDER_COMMAND(ReleaseHairStrandsResourceCommand)(
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
			Resource.SimRootResources = nullptr;
			Resource.RenRootResources = nullptr;

			for (FHairStrandsRestRootResource*& InCardsRootResources : Resource.CardsRootResources)
			{
				ENQUEUE_RENDER_COMMAND(ReleaseHairStrandsResourceCommand)(
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
		HairGroupResources.Empty();
	}

	// Process resources to be deleted (should happen only in editor)
	FHairGroupResource ResourceToDelete;
	while (HairGroupResourcesToDelete.Dequeue(ResourceToDelete))
	{
		FHairStrandsRestRootResource* InSimRootResources = ResourceToDelete.SimRootResources;
		FHairStrandsRestRootResource* InRenRootResources = ResourceToDelete.RenRootResources;
		ENQUEUE_RENDER_COMMAND(ReleaseHairStrandsResourceCommand)(
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
	ReleaseResource();
	for (FHairGroupBulkData& Data : HairGroupBulkDatas)
	{
		Data.SimRootBulkData.Reset();
		Data.RenRootBulkData.Reset();

		for (FHairStrandsRootBulkData& CardsRootBulkData : Data.CardsRootBulkData)
		{
			CardsRootBulkData.Reset();
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

	if (Groom)
	{
		// Make sure that the asset initialized its resources first since the component needs them to initialize its own resources
		Groom->ConditionalPostLoad();

	#if WITH_EDITOR
		CacheDerivedDatas();

		// Sanity check. This function will report back warnings/issues into the log for user.
		UGroomBindingAsset::IsCompatible(Groom, this, true);

		if (TargetSkeletalMesh)
		{
			TargetSkeletalMesh->OnPostMeshCached().AddUObject(this, &UGroomBindingAsset::InvalidateBinding);
			bRegisterTargetMeshCallback = true;
		}

		if (SourceSkeletalMesh)
		{
			SourceSkeletalMesh->OnPostMeshCached().AddUObject(this, &UGroomBindingAsset::InvalidateBinding);
			bRegisterSourceMeshCallback = true;
		}

		if (Groom)
		{
			Groom->GetOnGroomAssetChanged().AddUObject(this, &UGroomBindingAsset::InvalidateBinding);
			bRegisterGroomAssetCallback = true;
		}
	#endif
	}

	if (!IsTemplate() && IsValid())
	{
		InitResource();
	}
}

void UGroomBindingAsset::PreSave(const class ITargetPlatform* TargetPlatform)
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS;
	Super::PreSave(TargetPlatform);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS;
}

void UGroomBindingAsset::PreSave(FObjectPreSaveContext ObjectSaveContext)
{
#if WITH_EDITOR
	while (QueryStatus == EQueryStatus::Submitted)
	{
		FPlatformProcess::Sleep(1);
	}
#endif
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
	ReleaseResource();
	Super::BeginDestroy();

#if WITH_EDITOR
	if (TargetSkeletalMesh && bRegisterTargetMeshCallback)
	{
		TargetSkeletalMesh->OnPostMeshCached().RemoveAll(this);
		bRegisterTargetMeshCallback = false;
	}

	if (SourceSkeletalMesh && bRegisterSourceMeshCallback)
	{
		SourceSkeletalMesh->OnPostMeshCached().RemoveAll(this);
		bRegisterSourceMeshCallback = false;
	}

	if (Groom && bRegisterGroomAssetCallback)
	{
		Groom->GetOnGroomAssetChanged().RemoveAll(this);
		bRegisterGroomAssetCallback = false;
	}
#endif
}

bool UGroomBindingAsset::IsCompatible(const USkeletalMesh* InSkeletalMesh, const UGroomBindingAsset* InBinding, bool bIssueWarning)
{
	if (InBinding && InSkeletalMesh && IsHairStrandsBindingEnable())
	{
		if (InBinding->GroomBindingType != EGroomBindingMeshType::SkeletalMesh)
		{
			if (bIssueWarning)
			{
				UE_LOG(LogHairStrands, Warning, TEXT("[Groom] The binding asset (%s) cannot be bound to a SkeletalMesh because it is not the correct binding type."), *InBinding->GetName());
			}
			return false;
		}

		if (!InBinding->TargetSkeletalMesh)
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

		for (const FHairGroupResource& Resource : InBinding->HairGroupResources)
		{
			if (Resource.SimRootResources && InSkeletalMesh->GetLODNum() != Resource.SimRootResources->BulkData.MeshProjectionLODs.Num())
			{
				if (bIssueWarning)
				{
					UE_LOG(LogHairStrands, Warning, TEXT("[Groom] The Groom binding (%s) does not have the same have LOD count (LOD sim:%d) than the skeletal mesh (%s, LOD:%d). The binding asset will not be used."),
						*InBinding->GetName(),
						Resource.SimRootResources->BulkData.MeshProjectionLODs.Num(),
						*InSkeletalMesh->GetName(),
						InSkeletalMesh->GetLODNum());
				}
				return false;
			}

			if (Resource.RenRootResources && InSkeletalMesh->GetLODNum() != Resource.RenRootResources->BulkData.MeshProjectionLODs.Num() && IsHairStrandsEnabled(EHairStrandsShaderType::Strands))
			{
				if (bIssueWarning)
				{
					UE_LOG(LogHairStrands, Warning, TEXT("[Groom] The Groom binding (%s) does not have the same have LOD count (LOD render:%d) than the skeletal mesh (%s, LOD:%d). The binding asset will not be used."),
						*InBinding->GetName(),
						Resource.RenRootResources->BulkData.MeshProjectionLODs.Num(),
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
		if (InBinding->GroomBindingType != EGroomBindingMeshType::GeometryCache)
		{
			if (bIssueWarning)
			{
				UE_LOG(LogHairStrands, Warning, TEXT("[Groom] The binding asset (%s) cannot be bound to a GeometryCache because it is not the correct binding type."), *InBinding->GetName());
			}
			return false;
		}

		if (!InBinding->TargetGeometryCache)
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

bool UGroomBindingAsset::IsCompatible(const UGroomAsset* InGroom, const UGroomBindingAsset* InBinding, bool bIssueWarning)
{
	if (InBinding && InGroom && IsHairStrandsBindingEnable())
	{
		if (InBinding->Groom && !InBinding->Groom->IsValid())
		{
			// The groom could be invalid if it's still being loaded asynchronously
			return false;
		}

		if (!InBinding->Groom)
		{
			if (bIssueWarning)
			{
				UE_LOG(LogHairStrands, Warning, TEXT("[Groom] The binding asset (%s) does not reference a groom. Falling back onto non-binding version."), *InBinding->GetName());
			}
			return false;
		}

		if (InGroom->GetPrimaryAssetId() != InBinding->Groom->GetPrimaryAssetId())
		{
			UE_LOG(LogHairStrands, Warning, TEXT("[Groom] The Groom binding (%) does not reference the same groom asset (BindingAsset's groom:%s vs. Groom:%s). The binding asset will not be used."), 
				*InBinding->GetName(),
				*InBinding->Groom->GetName(),
				*InGroom->GetName());
			return false;
		}

		const uint32 GroupCount = InGroom->GetNumHairGroups();
		if (GroupCount != InBinding->GroupInfos.Num())
		{
			if (bIssueWarning)
			{
				UE_LOG(LogHairStrands, Warning, TEXT("[Groom] The GroomBinding asset (%s) does not contains the same number of groups (%d vs. %d) than the groom (%s). The binding asset will not be used."),
					*InBinding->GetName(),
					GroupCount,
					InBinding->GroupInfos.Num(),
					*InGroom->GetName());
			}
			return false;
		}

		for (uint32 GroupIt = 0; GroupIt < GroupCount; ++GroupIt)
		{
			{
				const uint32 GroomCount = InGroom->HairGroupsData[GroupIt].Guides.BulkData.GetNumCurves();
				const uint32 BindingCount = InBinding->GroupInfos[GroupIt].SimRootCount;

				if (GroomCount != 0 && GroomCount != BindingCount)
				{
					if (bIssueWarning)
					{
						UE_LOG(LogHairStrands, Warning, TEXT("[Groom] The GroomBinding asset (%s) does not contains the same guides in group %d (%d vs. %d) than the groom (%s). The binding asset will not be used."),
							*InBinding->GetName(),
							GroupIt,
							GroomCount,
							BindingCount,
							*InGroom->GetName());
					}
					return false;
				}
			}

			if (IsHairStrandsEnabled(EHairStrandsShaderType::Strands))
			{
				const uint32 GroomCount = InGroom->HairGroupsData[GroupIt].Strands.BulkData.GetNumCurves();
				const uint32 BindingCount = InBinding->GroupInfos[GroupIt].RenRootCount;

				// Groom may have stripped strands data so GroomCount would be 0
				if (GroomCount != 0 && GroomCount != BindingCount)
				{
					if (bIssueWarning)
					{
						UE_LOG(LogHairStrands, Warning, TEXT("[Groom] The GroomBinding asset (%s) does not contains the same curves in group %d (%d vs. %d) than the groom (%s). The binding asset will not be used."),
							*InBinding->GetName(),
							GroupIt,
							GroomCount,
							BindingCount,
							*InGroom->GetName());
					}
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

		if (!InBinding->Groom)
		{
			if (bIssueWarning)
			{
				UE_LOG(LogHairStrands, Warning, TEXT("[Groom] The binding asset (%s) does not reference a groom. Falling back onto non-binding version."), *InBinding->GetName());
			}
			return false;
		}
		if (InBinding->GroupInfos.Num() == 0)
		{
			if (bIssueWarning)
			{
				UE_LOG(LogHairStrands, Warning, TEXT("[Groom] The GroomBinding asset (Groom:%s) does not contain any groups. It is invalid and can't be assigned. The binding asset will not be used."), *InBinding->Groom->GetName());
			}
			return false;
		}

		for (const FGoomBindingGroupInfo& Info : InBinding->GroupInfos)
		{
			if (Info.SimRootCount == 0)
			{
				if (bIssueWarning)
				{
					UE_LOG(LogHairStrands, Warning, TEXT("[Groom] The GroomBinding asset (Groom:%s) has group with 0 guides. It is invalid and can't be assigned. The binding asset will not be used."), *InBinding->Groom->GetName());
				}
				return false;
			}

			if (Info.RenRootCount == 0 && IsHairStrandsEnabled(EHairStrandsShaderType::Strands))
			{
				if (bIssueWarning)
				{
					UE_LOG(LogHairStrands, Warning, TEXT("[Groom] The GroomBinding asset (Groom:%s) has group with 0 curves. It is invalid and can't be assigned. The binding asset will not be used."), *InBinding->Groom->GetName());
				}
				return false;
			}
		}
	}
	return true;
}

bool UGroomBindingAsset::HasValidTarget() const
{
	return (GroomBindingType == EGroomBindingMeshType::SkeletalMesh && TargetSkeletalMesh) ||
		   (GroomBindingType == EGroomBindingMeshType::GeometryCache && TargetGeometryCache);
}

#if WITH_EDITOR

void UGroomBindingAsset::Build()
{
	if (Groom && HasValidTarget())
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

#if WITH_EDITORONLY_DATA

// If groom binding derived data needs to be rebuilt (new format, serialization
// differences, etc.) replace the version GUID below with a new one.
// In case of merge conflicts with DDC versions, you MUST generate a new GUID
// and set this new GUID as the version.
#define GROOM_BINDING_DERIVED_DATA_VERSION TEXT("30769E530C574C7BA15C56F224A64E32")

namespace GroomBindingDerivedDataCacheUtils
{
	const FString& GetGroomBindingDerivedDataVersion()
	{
		static FString CachedVersionString(GROOM_BINDING_DERIVED_DATA_VERSION);
		return CachedVersionString;
	}

	FString BuildGroomBindingDerivedDataKey(const FString& KeySuffix)
	{
		return FDerivedDataCacheInterface::BuildCacheKey(*(TEXT("GROOM_BINDING_V") + FGroomBindingBuilder::GetVersion() + TEXT("_")), *GetGroomBindingDerivedDataVersion(), *KeySuffix);
	}
}

static FString BuildDerivedDataKeySuffix(const UGroomBindingAsset& BindingAsset)
{
	FString BindingType;
	FString SourceKey;
	FString TargetKey;

	if (BindingAsset.GroomBindingType == EGroomBindingMeshType::SkeletalMesh)
	{
		// Binding type is implicitly SkeletalMesh so keep BindingType empty to prevent triggering rebuild of old binding for nothing
		SourceKey = BindingAsset.SourceSkeletalMesh ? BindingAsset.SourceSkeletalMesh->GetDerivedDataKey() : FString();
		TargetKey = BindingAsset.TargetSkeletalMesh ? BindingAsset.TargetSkeletalMesh->GetDerivedDataKey() : FString();
	}
	else
	{
		BindingType = "GEOCACHE_";
		SourceKey = BindingAsset.SourceGeometryCache ? BindingAsset.SourceGeometryCache->GetHash() : FString();
		TargetKey = BindingAsset.TargetGeometryCache ? BindingAsset.TargetGeometryCache->GetHash() : FString();
	}
	FString GroomKey  = BindingAsset.Groom ? BindingAsset.Groom->GetDerivedDataKey()  : FString();
	FString PointKey  = FString::FromInt(BindingAsset.NumInterpolationPoints);
	FString SectionKey = FString::FromInt(BindingAsset.MatchingSection);

	uint32 KeyLength  = BindingType.Len() + SourceKey.Len() + TargetKey.Len() + GroomKey.Len() + PointKey.Len() + SectionKey.Len();

	FString KeySuffix;
	KeySuffix.Reserve(KeyLength);
	KeySuffix = BindingType + SourceKey + TargetKey + GroomKey + PointKey + SectionKey;
	return KeySuffix;
}

void UGroomBindingAsset::CacheDerivedDatas()
{
	if (!Groom || !Groom->IsValid())
	{
		// The groom could be invalid if it's still being loaded asynchronously
		return;
	}

	// List all the components which will need to be recreated to get the new binding information
	const FString KeySuffix = BuildDerivedDataKeySuffix(*this);
	const FString DerivedDataKey = GroomBindingDerivedDataCacheUtils::BuildGroomBindingDerivedDataKey(KeySuffix);

	if (DerivedDataKey != CachedDerivedDataKey)
	{
		FGroomComponentRecreateRenderStateContext RecreateRenderContext(Groom);

		TArray<uint8> DerivedData;
		if (GetDerivedDataCacheRef().GetSynchronous(*DerivedDataKey, DerivedData, GetPathName()))
		{
			if (IsHairStrandsDDCLogEnable())
			{
				UE_LOG(LogHairStrands, Log, TEXT("[GroomBinding/DDC] Found (GroomiBinding:%s)."), *GetName());
			}

			FMemoryReader Ar(DerivedData, /*bIsPersistent=*/ true);

			int64 UncompressedSize = 0;
			Ar << UncompressedSize;

			uint8* DecompressionBuffer = reinterpret_cast<uint8*>(FMemory::Malloc(UncompressedSize));
			Ar.SerializeCompressed(DecompressionBuffer, UncompressedSize, NAME_Zlib);

			FLargeMemoryReader LargeMemReader(DecompressionBuffer, UncompressedSize, ELargeMemoryReaderFlags::Persistent | ELargeMemoryReaderFlags::TakeOwnership);
			InternalSerialize(LargeMemReader, this, HairGroupBulkDatas, 0);

			bIsValid = true;
		}
		else
		{
			if (IsHairStrandsDDCLogEnable())
			{
				UE_LOG(LogHairStrands, Log, TEXT("[GroomBinding/DDC] Not found (GroomiBinding:%s)."), *GetName());
			}

			// Build groom binding data
			bIsValid = FGroomBindingBuilder::BuildBinding(this, false);
			if (bIsValid)
			{
				// Using a LargeMemoryWriter for serialization since the data can be bigger than 2 GB
				FLargeMemoryWriter LargeMemWriter(0, /*bIsPersistent=*/ true);
				InternalSerialize(LargeMemWriter, this, HairGroupBulkDatas, 0);

				int64 UncompressedSize = LargeMemWriter.TotalSize();

				// Then the content of the LargeMemWriter is compressed into a MemoryWriter
				// Compression ratio can reach about 5:2 depending on the data
				{
					FMemoryWriter CompressedArchive(DerivedData, true);

					CompressedArchive << UncompressedSize; // needed for allocating decompression buffer
					CompressedArchive.SerializeCompressed(LargeMemWriter.GetData(), UncompressedSize, NAME_Zlib);

					GetDerivedDataCacheRef().Put(*DerivedDataKey, DerivedData, GetPathName());
				}
			}
		}

		// Patch hair group info if it does not match the DDC-read/deserialized data
		const uint32 GroupCount = HairGroupBulkDatas.Num();
		if (GroupInfos.Num() != GroupCount)
		{
			GroupInfos.SetNum(GroupCount);
		}
		for (uint32 GroupIt=0; GroupIt< GroupCount; ++GroupIt)
		{
			FGoomBindingGroupInfo& Info = GroupInfos[GroupIt];
			const FHairGroupBulkData& Data = HairGroupBulkDatas[GroupIt];
			{
				Info.SimRootCount = Data.SimRootBulkData.RootCount;
				Info.SimLODCount  = Data.SimRootBulkData.MeshProjectionLODs.Num();
				Info.RenRootCount = Data.RenRootBulkData.RootCount;
				Info.RenLODCount  = Data.RenRootBulkData.MeshProjectionLODs.Num();
			}
		}

		if (bIsValid)
		{
			CachedDerivedDataKey = DerivedDataKey;
			InitResource();
		}
		else
		{
			UE_LOG(LogHairStrands, Error, TEXT("[Groom] The binding asset (%s) couldn't be built. This binding asset won't be used."), *GetName());
		}
	}
}
#endif

void UGroomBindingAsset::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(HairGroupBulkDatas.GetAllocatedSize());

	for (const FHairGroupResource& Group : HairGroupResources)
	{
		if (Group.SimRootResources) CumulativeResourceSize.AddDedicatedVideoMemoryBytes(Group.SimRootResources->GetResourcesSize());
		if (Group.RenRootResources) CumulativeResourceSize.AddDedicatedVideoMemoryBytes(Group.RenRootResources->GetResourcesSize());
		for (const FHairStrandsRestRootResource* CardsRootResource : Group.CardsRootResources)
		{
			if (CardsRootResource) CumulativeResourceSize.AddDedicatedVideoMemoryBytes(CardsRootResource->GetResourcesSize());
		}
	}
}

