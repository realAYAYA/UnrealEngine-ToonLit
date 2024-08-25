// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "HairDescription.h"
#include "HairStrandsDatas.h"
#include "RenderResource.h"
#include "GroomResources.h"
#include "GroomSettings.h"
#include "GroomAssetCards.h"
#include "GroomAssetMeshes.h"
#include "GroomAssetInterpolation.h"
#include "GroomAssetPhysics.h"
#include "GroomAssetRendering.h"
#include "Curves/CurveFloat.h"
#include "HairStrandsInterface.h"
#include "Engine/SkeletalMesh.h"
#include "Interfaces/Interface_AssetUserData.h"
#include "PerPlatformProperties.h"
#include "UObject/StrongObjectPtr.h"

#include "GroomAsset.generated.h"


class UAssetUserData;
class UMaterialInterface;
class UNiagaraSystem;
struct FHairStrandsRestResource;
struct FHairStrandsInterpolationResource;
struct FHairStrandsRaytracingResource;

enum class EHairGroupInfoFlags : uint8
{
	HasTrimmedPoint = 1,
	HasTrimmedCurve = 2
};

USTRUCT(BlueprintType)
struct HAIRSTRANDSCORE_API FHairGroupLODInfo
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "Info", meta = (DisplayName = "Point Count"))
	int32 NumPoints = 0;

	UPROPERTY(VisibleAnywhere, Category = "Info", meta = (DisplayName = "Curve Count"))
	int32 NumCurves = 0;
};

USTRUCT(BlueprintType)
struct HAIRSTRANDSCORE_API FHairGroupInfo
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "Info")
	int32 GroupID = 0;

	UPROPERTY(VisibleAnywhere, Category = "Info")
	FName GroupName = NAME_None;

	UPROPERTY(VisibleAnywhere, Category = "Info", meta = (DisplayName = "Curve Count"))
	int32 NumCurves = 0;

	UPROPERTY(VisibleAnywhere, Category = "Info", meta = (DisplayName = "Guide Count"))
	int32 NumGuides = 0;

	UPROPERTY(VisibleAnywhere, Category = "Info", meta = (DisplayName = "Curve Vertex Count"))
	int32 NumCurveVertices = 0;

	UPROPERTY(VisibleAnywhere, Category = "Info", meta = (DisplayName = "Guide Vertex Count"))
	int32 NumGuideVertices = 0;

	UPROPERTY(VisibleAnywhere, Category = "Info", meta = (DisplayName = "Length of the longest hair strands"))
	float MaxCurveLength = 0;

	UPROPERTY()
	uint32 Flags = 0;

	UPROPERTY()
	TArray<FHairGroupLODInfo> LODInfos;
};

USTRUCT(BlueprintType)
struct HAIRSTRANDSCORE_API FHairGroupsMaterial
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Material")
	TObjectPtr<UMaterialInterface> Material = nullptr;

	UPROPERTY(EditAnywhere, Category = "Material")
	FName SlotName = NAME_None;
};


struct FHairGroupResources
{
	struct FGuides
	{
		bool IsValid() const { return RestResource != nullptr; }

		/* Return the memory size for GPU resources */
		uint32 GetResourcesSize() const
		{
			uint32 Total = 0;
			if (RestResource) Total += RestResource->GetResourcesSize();
			return Total;
		}

		FHairStrandsRestResource* RestResource = nullptr;

	} Guides;

	struct FStrands
	{
		bool IsValid() const { return RestResource != nullptr; }

		/* Return the memory size for GPU resources */
		uint32 GetResourcesSize() const
		{
			uint32 Total = 0;
			if (RestResource) 			Total += RestResource->GetResourcesSize();
			if (InterpolationResource) 	Total += InterpolationResource->GetResourcesSize();
			if (ClusterResource) 		Total += ClusterResource->GetResourcesSize();
			#if RHI_RAYTRACING
			if (RaytracingResource) 	Total += RaytracingResource->GetResourcesSize();
			#endif
			return Total;
		}

		FHairStrandsRestResource*			RestResource = nullptr;
		FHairStrandsInterpolationResource*	InterpolationResource = nullptr;
		FHairStrandsClusterResource* 		ClusterResource = nullptr;
		#if RHI_RAYTRACING
		FHairStrandsRaytracingResource*		RaytracingResource = nullptr;
		#endif

		bool bIsCookedOut = false;
	} Strands;

	struct FCards
	{
		/* Return the memory size for GPU resources */
		uint32 GetResourcesSize() const
		{
			uint32 Total = 0;
			for (const FLOD& LOD : LODs)
			{
				Total += LOD.GetResourcesSize();
			}
			return Total;
		}

		struct FLOD
		{
			/* Return the memory size for GPU resources */
			uint32 GetResourcesSize() const
			{
				uint32 Total = 0;
				if (RestResource) 				Total += RestResource->GetResourcesSize();
				if (InterpolationResource) 		Total += InterpolationResource->GetResourcesSize();
				if (GuideRestResource) 			Total += GuideRestResource->GetResourcesSize();
				if (GuideInterpolationResource) Total += GuideInterpolationResource->GetResourcesSize();
				#if RHI_RAYTRACING
				if (RaytracingResource) 		Total += RaytracingResource->GetResourcesSize();
				#endif
				return Total;
			}

			bool IsValid() const { return RestResource != nullptr; }

			FHairCardsRestResource*				RestResource = nullptr;
			FHairCardsInterpolationResource*	InterpolationResource = nullptr;
			FHairStrandsRestResource*			GuideRestResource = nullptr;
			FHairStrandsInterpolationResource*	GuideInterpolationResource = nullptr;
			#if RHI_RAYTRACING
			FHairStrandsRaytracingResource*		RaytracingResource = nullptr;
			#endif

			bool bIsCookedOut = false;
		};
		TArray<FLOD> LODs;
	} Cards;

	struct FMeshes
	{	
		/* Return the memory size for GPU resources */
		uint32 GetResourcesSize() const
		{
			uint32 Total = 0;
			for (const FLOD& LOD : LODs)
			{
				Total += LOD.GetResourcesSize();
			}
			return Total;
		}

		struct FLOD
		{
			/* Return the memory size for GPU resources */
			uint32 GetResourcesSize() const
			{
				uint32 Total = 0;
				if (RestResource) 		Total += RestResource->GetResourcesSize();
				#if RHI_RAYTRACING
				if (RaytracingResource) Total += RaytracingResource->GetResourcesSize();
				#endif
				return Total;
			}

			bool IsValid() const { return RestResource != nullptr; }

			FHairMeshesRestResource* 		RestResource = nullptr;
			#if RHI_RAYTRACING
			FHairStrandsRaytracingResource* RaytracingResource = nullptr;
			#endif
			bool bIsCookedOut = false;
		};
		TArray<FLOD> LODs;
	} Meshes;

	struct FDebug
	{
		FHairStrandsDebugResources* Resource = nullptr;
	} Debug;
};

FORCEINLINE uint32 GetDataSize(const FHairStrandsBulkData& BulkData)
{
	uint32 Total = 0;
	Total += BulkData.Data.Positions.IsBulkDataLoaded() 		? BulkData.Data.Positions.GetBulkDataSize()   : 0;
	Total += BulkData.Data.CurveAttributes.IsBulkDataLoaded()	? BulkData.Data.CurveAttributes.GetBulkDataSize() : 0;
	Total += BulkData.Data.PointAttributes.IsBulkDataLoaded()	? BulkData.Data.PointAttributes.GetBulkDataSize() : 0;
	Total += BulkData.Data.Curves.IsBulkDataLoaded() 			? BulkData.Data.Curves.GetBulkDataSize(): 0;
	Total += BulkData.Data.PointToCurve.IsBulkDataLoaded()		? BulkData.Data.PointToCurve.GetBulkDataSize() : 0;
	return Total;
}

FORCEINLINE uint32 GetDataSize(const FHairStrandsInterpolationBulkData& InterpolationBulkData) 	
{
	uint32 Total = 0;
	Total += InterpolationBulkData.Data.CurveInterpolation.IsBulkDataLoaded()? InterpolationBulkData.Data.CurveInterpolation.GetBulkDataSize() : 0;
	Total += InterpolationBulkData.Data.PointInterpolation.IsBulkDataLoaded()? InterpolationBulkData.Data.PointInterpolation.GetBulkDataSize() : 0;
	return Total;
}

struct FHairGroupPlatformData
{
	struct FGuides
	{
		bool HasValidData() const		{ return BulkData.GetNumPoints() > 0;}
		const FBox& GetBounds() const	{ return BulkData.GetBounds(); }
		uint32 GetDataSize() const		{ return ::GetDataSize(BulkData); }
		FHairStrandsBulkData BulkData;
	} Guides;

	struct FStrands
	{
		bool HasValidData() const		{ return BulkData.GetNumPoints() > 0;}
		const FBox& GetBounds() const	{ return BulkData.GetBounds(); }

		uint32 GetDataSize() const;
		FHairStrandsBulkData				BulkData;
		FHairStrandsInterpolationBulkData	InterpolationBulkData;
		FHairStrandsClusterBulkData			ClusterBulkData;
		bool bIsCookedOut = false;
	} Strands;

	struct FCards
	{
		bool HasValidData() const 
		{ 
			for (const FLOD& LOD : LODs)
			{
				if (LOD.HasValidData())
				return true;
			}
			return false;
		}

		bool HasValidData(uint32 LODIt) const 	{ return LODIt < uint32(LODs.Num()) && LODs[LODIt].HasValidData(); }
		bool IsValid(uint32 LODIt) const 		{ return LODIt < uint32(LODs.Num()) && LODs[LODIt].IsValid(); }
		FBox GetBounds() const
		{
			for (const FLOD& LOD : LODs)
			{
				if (LOD.IsValid()) return LOD.BulkData.BoundingBox;
			}
			return FBox();
		}

		uint32 GetDataSize() const
		{
			uint32 Total = 0;
			for (const FLOD& LOD : LODs)
			{
				Total += LOD.GetDataSize();
			}
			return Total;
		}

		struct FLOD
		{
			uint32 GetDataSize() const
			{
				uint32 Total = 0;
				Total += BulkData.Positions.GetAllocatedSize();
				Total += BulkData.Normals.GetAllocatedSize();
				Total += BulkData.UVs.GetAllocatedSize();
				Total += BulkData.Indices.GetAllocatedSize();
				Total += InterpolationBulkData.Interpolation.GetAllocatedSize();
				Total += ::GetDataSize(GuideBulkData);
				Total += ::GetDataSize(GuideInterpolationBulkData);
				return Total;
			}

			bool HasValidData() const	{ return BulkData.IsValid(); }
			bool IsValid() const 		{ return BulkData.IsValid(); }

			// Main data & Resources
			FHairCardsBulkData					BulkData;
			FHairCardsInterpolationBulkData		InterpolationBulkData;
			FHairStrandsBulkData				GuideBulkData;
			FHairStrandsInterpolationBulkData	GuideInterpolationBulkData;

			bool bIsCookedOut = false;
		};
		TArray<FLOD> LODs;
	} Cards;

	struct FMeshes
	{
		bool HasValidData() const
		{
			for (const FLOD& LOD : LODs)
			{
				if (LOD.HasValidData())
				return true;
			}
			return false;
		}
		bool HasValidData(uint32 LODIt) const 	{ return LODIt < uint32(LODs.Num()) && LODs[LODIt].HasValidData(); }
		bool IsValid(uint32 LODIt) const 		{ return LODIt < uint32(LODs.Num()) && LODs[LODIt].IsValid(); }
		FBox GetBounds() const
		{
			for (const FLOD& LOD : LODs)
			{
				if (LOD.IsValid()) return LOD.BulkData.BoundingBox;
			}
			return FBox();
		}

		uint32 GetDataSize() const
		{
			uint32 Total = 0;
			for (const FLOD& LOD : LODs)
			{
				Total += LOD.GetDataSize();
			}
			return Total;
		}

		struct FLOD
		{
			uint32 GetDataSize() const
			{
				uint32 Total = 0;
				Total += BulkData.Positions.GetAllocatedSize();
				Total += BulkData.Normals.GetAllocatedSize();
				Total += BulkData.UVs.GetAllocatedSize();
				Total += BulkData.Indices.GetAllocatedSize();
				return Total;
			}

			bool HasValidData() const 	{ return BulkData.IsValid(); }
			bool IsValid() const 		{ return BulkData.IsValid(); }

			FHairMeshesBulkData BulkData;
			bool bIsCookedOut = false;
		};
		TArray<FLOD> LODs;
	} Meshes;

	struct FDebug
	{
		FHairStrandsDebugDatas Data;
	} Debug;
};

struct FHairDescriptionGroup
{
	FHairGroupInfo		Info;
	FHairStrandsDatas	Strands;
	FHairStrandsDatas	Guides;
	uint32 GetHairAttributes() const { return Strands.GetAttributes() | Guides.GetAttributes(); }
	uint32 GetHairAttributeFlags() const { return Strands.GetAttributeFlags() | Guides.GetAttributeFlags(); }
};

struct FHairDescriptionGroups
{
	TArray<FHairDescriptionGroup> HairGroups;
	FBoxSphereBounds3f Bounds;
	bool  IsValid() const;
};

USTRUCT(BlueprintType)
struct HAIRSTRANDSCORE_API FHairGroupInfoWithVisibility : public FHairGroupInfo
{
	GENERATED_BODY()

	/** Toggle hair group visibility. This visibility flag is not persistent to the asset, and exists only as a preview/helper mechanism */
	UPROPERTY(EditAnywhere, Category = "Info", meta = (DisplayName = "Visible"))
	bool bIsVisible = true;
};

UENUM(BlueprintType)
enum class EHairAtlasTextureType : uint8
{
	Depth,
	Tangent,
	Attribute,
	Coverage,
	AuxilaryData,
	Material
};

struct FHairVertexFactoryTypesPerMaterialData
{
	int16 MaterialIndex;
	EHairGeometryType HairGeometryType;
	FPSOPrecacheVertexFactoryDataList VertexFactoryDataList;
};

/**
 * Implements an asset that can be used to store hair strands
 */
UCLASS(BlueprintType, AutoExpandCategories = ("HairRendering", "HairPhysics", "HairInterpolation"), hidecategories = (Object, "Hidden"))
class HAIRSTRANDSCORE_API UGroomAsset : public UObject, public IInterface_AssetUserData
{
	GENERATED_UCLASS_BODY()

#if WITH_EDITOR
	/** Notification when anything changed */
	DECLARE_MULTICAST_DELEGATE(FOnGroomAssetChanged);
	DECLARE_MULTICAST_DELEGATE(FOnGroomAssetResourcesChanged);
	DECLARE_MULTICAST_DELEGATE(FOnGroomAsyncLoadFinished);
#endif

private:
	UPROPERTY(EditAnywhere, Category = "HairLOD", meta = (DisplayName = "LOD Mode", ToolTip = "Define how LOD adapts curves & points for strands geometry. Auto: adapts the curve count based on screen coverage. Manual: use the discrete LOD created for each groups"))
	EGroomLODMode LODMode = EGroomLODMode::Default;

	UPROPERTY(EditAnywhere, Category = "HairLOD", meta = (DisplayName = "Auto LOD Bias", ToolTip = "When Auto LOD is selected, decrease the screen size at which curves reduction will occur.", ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1.0"))
	float AutoLODBias = 0;

public:
	UE_DEPRECATED(5.3, "Please do not access this member directly; use UGroomAsset accessor.")
	UPROPERTY(EditAnywhere, EditFixedSize, EditFixedSize, Category = "HairInfo", meta = (DisplayName = "Group"))
	TArray<FHairGroupInfoWithVisibility> HairGroupsInfo;

	UE_DEPRECATED(5.3, "Please do not access this member directly; use UGroomAsset accessor.")
	UPROPERTY(EditAnywhere, EditFixedSize, BlueprintGetter = GetHairGroupsRendering, BlueprintSetter = SetHairGroupsRendering, Category = "HairRendering", meta = (DisplayName = "Group"))
	TArray<FHairGroupsRendering> HairGroupsRendering;

	UE_DEPRECATED(5.3, "Please do not access this member directly; use UGroomAsset accessor.")
	UPROPERTY(EditAnywhere, EditFixedSize, BlueprintGetter = GetHairGroupsPhysics, BlueprintSetter = SetHairGroupsPhysics, Category = "HairPhysics", meta = (DisplayName = "Group"))
	TArray<FHairGroupsPhysics> HairGroupsPhysics;

	UE_DEPRECATED(5.3, "Please do not access this member directly; use UGroomAsset accessor.")
	UPROPERTY(EditAnywhere, EditFixedSize, BlueprintGetter = GetHairGroupsInterpolation, BlueprintSetter = SetHairGroupsInterpolation, Category = "HairInterpolation", meta = (DisplayName = "Group"))
	TArray<FHairGroupsInterpolation> HairGroupsInterpolation;

	UE_DEPRECATED(5.3, "Please do not access this member directly; use UGroomAsset accessor.")
	UPROPERTY(EditAnywhere, EditFixedSize, BlueprintGetter = GetHairGroupsLOD, BlueprintSetter = SetHairGroupsLOD, Category = "HairLOD", meta = (DisplayName = "Group"))
	TArray<FHairGroupsLOD> HairGroupsLOD;

	/** Cards - Source description data */
	UE_DEPRECATED(5.3, "Please do not access this member directly; use UGroomAsset accessor.")
	UPROPERTY(EditAnywhere, BlueprintGetter = GetHairGroupsCards, BlueprintSetter = SetHairGroupsCards, Category = "HairCards", meta = (DisplayName = "Group"))
	TArray<FHairGroupsCardsSourceDescription> HairGroupsCards;

	/** Meshes - Source description data */
	UE_DEPRECATED(5.3, "Please do not access this member directly; use UGroomAsset accessor.")
	UPROPERTY(EditAnywhere, BlueprintGetter = GetHairGroupsMeshes, BlueprintSetter = SetHairGroupsMeshes, Category = "HairMeshes", meta = (DisplayName = "Group"))
	TArray<FHairGroupsMeshesSourceDescription> HairGroupsMeshes;

	/** Meshes - Source description data */
	UE_DEPRECATED(5.3, "Please do not access this member directly; use UGroomAsset accessor.")
		UPROPERTY(EditAnywhere, BlueprintGetter = GetHairGroupsMaterials, BlueprintSetter = SetHairGroupsMaterials, Category = "HairMaterials", meta = (DisplayName = "Group"))
	TArray<FHairGroupsMaterial> HairGroupsMaterials;

	/** Enable radial basis function interpolation to be used instead of the local skin rigid transform */
	UE_DEPRECATED(5.3, "Please do not access this member directly; use UGroomAsset accessor.")
	UPROPERTY(EditAnywhere, EditFixedSize, BlueprintGetter = GetEnableGlobalInterpolation, BlueprintSetter = SetEnableGlobalInterpolation, Category = "HairInterpolation", meta = (ToolTip = "Enable radial basis function interpolation to be used instead of the local skin rigid transform (WIP)", DisplayName = "RBF Interpolation"))
	bool EnableGlobalInterpolation = false;

	/** Enable guide-cache support. This allows to attach a guide-cache dynamically at runtime */
	UE_DEPRECATED(5.3, "Please do not access this member directly; use UGroomAsset accessor.")
	UPROPERTY(EditAnywhere, EditFixedSize, BlueprintGetter = GetEnableSimulationCache, BlueprintSetter = SetEnableSimulationCache, Category = "HairInterpolation", meta = (ToolTip = "Enable guide-cache support. This allows to attach a simulation-cache dynamically at runtime", DisplayName = "Enable Guide-Cache Support"))
	bool EnableSimulationCache = false;

	/** Type of interpolation used */
	UE_DEPRECATED(5.3, "Please do not access this member directly; use UGroomAsset accessor.")
	UPROPERTY(EditAnywhere, EditFixedSize, BlueprintGetter = GetHairInterpolationType, BlueprintSetter = SetHairInterpolationType, Category = "HairInterpolation", meta = (ToolTip = "Type of interpolation used (WIP)"))
	EGroomInterpolationType HairInterpolationType = EGroomInterpolationType::SmoothTransform;

	/** Deformed skeletal mesh that will drive the groom deformation/simulation. For creating this skeletal mesh, enable EnableDeformation within the interpolation settings*/
	UE_DEPRECATED(5.3, "Please do not access this member directly; use UGroomAsset accessor.")
	UPROPERTY(VisibleAnywhere, BlueprintGetter = GetRiggedSkeletalMesh, BlueprintSetter = SetRiggedSkeletalMesh, Category = "HairInterpolation")
	TObjectPtr<USkeletalMesh> RiggedSkeletalMesh;

	/** Deformed skeletal mesh mapping from groups to sections */
	UE_DEPRECATED(5.3, "Please do not access this member directly; use UGroomAsset accessor.")
	UPROPERTY()
	TArray<int32> DeformedGroupSections;

	/** Minimum LOD to cook */
	UE_DEPRECATED(5.3, "Please do not access this member directly; use UGroomAsset accessor.")
	UPROPERTY(EditAnywhere, Category = "LOD", meta = (DisplayName = "Minimum LOD"))
	FPerPlatformInt MinLOD;

	/** When true all LODs below MinLod will still be cooked */
	UE_DEPRECATED(5.3, "Please do not access this member directly; use UGroomAsset accessor.")
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "LOD")
	FPerPlatformBool DisableBelowMinLodStripping;

	/** The LOD bias to use after LOD stripping, regardless of MinLOD. Computed at cook time */
	UE_DEPRECATED(5.3, "Please do not access this member directly; use UGroomAsset accessor.")
	UPROPERTY()
	TArray<float> EffectiveLODBias;

	/** Store strands/cards/meshes data */
	UE_DEPRECATED(5.3, "Please do not access this member directly; use UGroomAsset accessor.")
	TArray<FHairGroupPlatformData> HairGroupsPlatformData;

private:
	/** Store strands/cards/meshes resources */
	TArray<FHairGroupResources> HairGroupsResources;

public:
	static FName GetHairGroupsRenderingMemberName();
	UFUNCTION(BlueprintGetter) TArray<FHairGroupsRendering>& GetHairGroupsRendering();
	UFUNCTION(BlueprintSetter) void SetHairGroupsRendering(const TArray<FHairGroupsRendering>& In);
	const TArray<FHairGroupsRendering>& GetHairGroupsRendering() const;

	static FName GetHairGroupsPhysicsMemberName();
	UFUNCTION(BlueprintGetter) TArray<FHairGroupsPhysics>& GetHairGroupsPhysics();
	UFUNCTION(BlueprintSetter) void SetHairGroupsPhysics(const TArray<FHairGroupsPhysics>& In);
	const TArray<FHairGroupsPhysics>& GetHairGroupsPhysics() const;

	static FName GetHairGroupsInterpolationMemberName();
	UFUNCTION(BlueprintGetter) TArray<FHairGroupsInterpolation>& GetHairGroupsInterpolation();
	UFUNCTION(BlueprintSetter) void SetHairGroupsInterpolation(const TArray<FHairGroupsInterpolation>& In);
	const TArray<FHairGroupsInterpolation>& GetHairGroupsInterpolation() const;

	static FName GetHairGroupsLODMemberName();
	UFUNCTION(BlueprintGetter) TArray<FHairGroupsLOD>& GetHairGroupsLOD();
	UFUNCTION(BlueprintSetter) void SetHairGroupsLOD(const TArray<FHairGroupsLOD>& In);
	const TArray<FHairGroupsLOD>& GetHairGroupsLOD() const;

	static FName GetHairGroupsCardsMemberName();
	UFUNCTION(BlueprintGetter) TArray<FHairGroupsCardsSourceDescription>& GetHairGroupsCards();
	UFUNCTION(BlueprintSetter) void SetHairGroupsCards(const TArray<FHairGroupsCardsSourceDescription>& In);
	const TArray<FHairGroupsCardsSourceDescription>& GetHairGroupsCards() const;

	static FName GetHairGroupsMeshesMemberName();
	UFUNCTION(BlueprintGetter) TArray<FHairGroupsMeshesSourceDescription>& GetHairGroupsMeshes();
	UFUNCTION(BlueprintSetter) void SetHairGroupsMeshes(const TArray<FHairGroupsMeshesSourceDescription>& In);
	const TArray<FHairGroupsMeshesSourceDescription>& GetHairGroupsMeshes() const;

	static FName GetHairGroupsMaterialsMemberName();
	UFUNCTION(BlueprintGetter) TArray<FHairGroupsMaterial>& GetHairGroupsMaterials();
	UFUNCTION(BlueprintSetter) void SetHairGroupsMaterials(const TArray<FHairGroupsMaterial>& In);
	const TArray<FHairGroupsMaterial>& GetHairGroupsMaterials() const;

	static FName GetEnableGlobalInterpolationMemberName();
	UFUNCTION(BlueprintGetter) bool GetEnableGlobalInterpolation() const;
	UFUNCTION(BlueprintSetter) void SetEnableGlobalInterpolation(bool In);

	static FName GetEnableSimulationCacheMemberName();
	UFUNCTION(BlueprintGetter) bool GetEnableSimulationCache() const;
	UFUNCTION(BlueprintSetter) void SetEnableSimulationCache(bool In);

	static FName GetHairInterpolationTypeMemberName();
	UFUNCTION(BlueprintGetter) EGroomInterpolationType GetHairInterpolationType() const;
	UFUNCTION(BlueprintSetter) void SetHairInterpolationType(EGroomInterpolationType In);

	static FName GetRiggedSkeletalMeshMemberName();
	UFUNCTION(BlueprintGetter) USkeletalMesh* GetRiggedSkeletalMesh() const;
	UFUNCTION(BlueprintSetter) void SetRiggedSkeletalMesh(USkeletalMesh* In);

	static FName GetDeformedGroupSectionsMemberName();
	UFUNCTION(BlueprintGetter) TArray<int32>& GetDeformedGroupSections();
	UFUNCTION(BlueprintSetter) void SetDeformedGroupSections(const TArray<int32>& In);
	const TArray<int32>& GetDeformedGroupSections() const;

	static FName GetMinLODMemberName();
	FPerPlatformInt GetMinLOD() const;
	void SetMinLOD(FPerPlatformInt In);

	static FName GetDisableBelowMinLodStrippingMemberName();
	FPerPlatformBool GetDisableBelowMinLodStripping() const;
	void SetDisableBelowMinLodStripping(FPerPlatformBool In);

	static FName GetEffectiveLODBiasMemberName();
	TArray<float>& GetEffectiveLODBias();
	void SetEffectiveLODBias(const TArray<float>& In);
	const TArray<float>& GetEffectiveLODBias() const;

	static FName GetHairGroupsPlatformDataMemberName();
	TArray<FHairGroupPlatformData>& GetHairGroupsPlatformData();
	const TArray<FHairGroupPlatformData>& GetHairGroupsPlatformData() const;
	void SetHairGroupsPlatformData(const TArray<FHairGroupPlatformData>& In);

	static FName GetHairGroupsInfoMemberName();
	TArray<FHairGroupInfoWithVisibility>& GetHairGroupsInfo();
	const TArray<FHairGroupInfoWithVisibility>& GetHairGroupsInfo() const;
	void SetHairGroupsInfo(const TArray<FHairGroupInfoWithVisibility>& In);

	const TArray<FHairGroupResources>& GetHairGroupsResources() const;
	TArray<FHairGroupResources>& GetHairGroupsResources();

	static FName GetLODModeMemberName();
	EGroomLODMode GetLODMode() const;

	static FName GetAutoLODBiasMemberName();
	float GetAutoLODBias() const;
public:

	//~ Begin UObject Interface.
	virtual void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) override;
	virtual void PostLoad() override;
	virtual void BeginDestroy() override;
	virtual void Serialize(FArchive& Ar) override;

#if WITH_EDITOR
	FOnGroomAssetChanged& GetOnGroomAssetChanged() { return OnGroomAssetChanged;  }
	FOnGroomAssetResourcesChanged& GetOnGroomAssetResourcesChanged() { return OnGroomAssetResourcesChanged; }
	FOnGroomAsyncLoadFinished& GetOnGroomAsyncLoadFinished() { return OnGroomAsyncLoadFinished; }

	/**  Part of Uobject interface  */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

#endif // WITH_EDITOR

#if WITH_EDITORONLY_DATA

	/** Asset data to be used when re-importing */
	UPROPERTY(VisibleAnywhere, Instanced, Category = ImportSettings)
	TObjectPtr<class UAssetImportData> AssetImportData;

	/** Retrieve the asset tags*/
	virtual void GetAssetRegistryTags(FAssetRegistryTagsContext Context) const override;
	UE_DEPRECATED(5.4, "Implement the version that takes FAssetRegistryTagsContext instead.")
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;

	/** Part of Uobject interface */
	virtual void PostInitProperties() override;

#endif // WITH_EDITORONLY_DATA

	bool IsValid() const { return bIsInitialized; }

	// Helper functions for setting options on all hair groups
	void SetStableRasterization(bool bEnable);
	void SetScatterSceneLighting(bool Enable);
	void SetHairWidth(float Width);

	/** Initialize/Update/Release resources. */
	void InitResources();
	void InitGuideResources();
	void InitStrandsResources();
	void InitCardsResources();
	void InitMeshesResources();
#if WITH_EDITOR
	void UpdateResource();
#endif
	void ReleaseResource();
	void ReleaseGuidesResource(uint32 GroupIndex);
	void ReleaseStrandsResource(uint32 GroupIndex);
	void ReleaseCardsResource(uint32 GroupIndex);
	void ReleaseMeshesResource(uint32 GroupIndex);

	void SetNumGroup(uint32 InGroupCount, bool bResetGroupData=true, bool bResetOtherData=true);
	bool AreGroupsValid() const;
	int32 GetNumHairGroups() const;

	int32 GetLODCount() const;
#if WITH_EDITORONLY_DATA
	void StripLODs(const TArray<int32>& LODsToKeep, bool bRebuildResources);
#endif // WITH_EDITORONLY_DATA

	/** Debug data for derived asset generation (strands textures, ...). */
	bool HasDebugData() const;
	void CreateDebugData();

	/** Returns true if the asset has the HairDescription needed to recompute its groom data */
	bool CanRebuildFromDescription() const;

	//~ Begin IInterface_AssetUserData Interface
	virtual void AddAssetUserData(UAssetUserData* InUserData) override;
	virtual void RemoveUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass) override;
	virtual UAssetUserData* GetAssetUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass) override;
	virtual const TArray<UAssetUserData*>* GetAssetUserDataArray() const override;
	//~ End IInterface_AssetUserData Interface

	EGroomGeometryType GetGeometryType(int32 GroupIndex, int32 LODIndex) const;
	EGroomBindingType GetBindingType(int32 GroupIndex, int32 LODIndex) const;
	bool IsVisible(int32 GroupIndex, int32 LODIndex) const;
	bool IsSimulationEnable(int32 GroupIndex, int32 LODIndex) const;
	bool IsSimulationEnable() const;
	bool IsDeformationEnable(int32 GroupIndex) const;
	bool IsGlobalInterpolationEnable(int32 GroupIndex, int32 LODIndex) const;
	bool NeedsInterpolationData(int32 GroupIndex) const;
	bool NeedsInterpolationData() const;

	void UpdateHairGroupsInfo();
	bool HasGeometryType(EGroomGeometryType Type) const;
	bool HasGeometryType(uint32 GroupIndex, EGroomGeometryType Type) const;

	/** Used for PSO precaching of used materials and vertex factories */
	TArray<FHairVertexFactoryTypesPerMaterialData> CollectVertexFactoryTypesPerMaterialData(EShaderPlatform ShaderPlatform);

	/** Helper function to return the asset path name, optionally joined with the LOD index if LODIndex > -1. */
	FName GetAssetPathName(int32 LODIndex = -1);
	uint32 GetAssetHash() const { return AssetNameHash; }

//private :
#if WITH_EDITOR
	FOnGroomAssetChanged OnGroomAssetChanged;
	FOnGroomAssetResourcesChanged OnGroomAssetResourcesChanged;
	FOnGroomAsyncLoadFinished OnGroomAsyncLoadFinished;

	void MarkMaterialsHasChanged();

	void RecreateResources();
	void ChangeFeatureLevel(ERHIFeatureLevel::Type PendingFeatureLevel);
	void ChangePlatformLevel(ERHIFeatureLevel::Type PendingFeatureLevel);
#endif

	/** Array of user data stored with the asset */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Instanced, Category = Hidden)
	TArray<TObjectPtr<UAssetUserData>> AssetUserData;

	/* Return the material slot index corresponding to the material name */
	int32 GetMaterialIndex(FName MaterialSlotName) const;
	bool IsMaterialSlotNameValid(FName MaterialSlotName) const;
	bool IsMaterialUsed(int32 MaterialIndex) const;
	TArray<FName> GetMaterialSlotNames() const;

	bool BuildCardsData();
	bool BuildMeshesData();

	enum EClassDataStripFlag : uint8
	{
		CDSF_ImportedStrands = 1,
		CDSF_MinLodData = 2,
		CDSF_StrandsStripped = 4,
		CDSF_CardsStripped = 8,
		CDSF_MeshesStripped = 16
	};

	uint8 GenerateClassStripFlags(FArchive& Ar);

private:
	void ApplyStripFlags(uint8 StripFlags, const class ITargetPlatform* CookTarget);

	// Functions allocating lazily/on-demand resources (guides, interpolation, RT geometry, ...)
	FHairStrandsRestResource*			AllocateGuidesResources(uint32 GroupIndex);
	FHairStrandsInterpolationResource*	AllocateInterpolationResources(uint32 GroupIndex);
#if RHI_RAYTRACING
	FHairStrandsRaytracingResource*		AllocateCardsRaytracingResources(uint32 GroupIndex, uint32 LODIndex);
	FHairStrandsRaytracingResource*		AllocateMeshesRaytracingResources(uint32 GroupIndex, uint32 LODIndex);
	FHairStrandsRaytracingResource*		AllocateStrandsRaytracingResources(uint32 GroupIndex);
#endif // RHI_RAYTRACING
	friend class UGroomComponent;

#if WITH_EDITORONLY_DATA
	bool HasImportedStrandsData() const;

	bool BuildHairGroup_Cards(uint32 GroupIndex);
	bool BuildHairGroup_Meshes(uint32 GroupIndex);

	bool HasChanged_Cards(uint32 GroupIndex, TArray<bool>& OutIsValid) const;
	bool HasChanged_Meshes(uint32 GroupIndex, TArray<bool>& OutIsValid) const;

	bool HasValidData_Cards(uint32 GroupIndex) const;
	bool HasValidData_Meshes(uint32 GroupIndex) const;
public:
	enum EHairDescriptionType
	{
		Source,
		Edit,
		Count
	};

	/** Commits a HairDescription to buffer for serialization */
	void CommitHairDescription(FHairDescription&& HairDescription, EHairDescriptionType Type);
	FHairDescription GetHairDescription() const;

	/** Get/Build render & guides data based on the hair description and interpolation settings */
	bool GetHairStrandsDatas(const int32 GroupIndex, FHairStrandsDatas& OutStrandsData, FHairStrandsDatas& OutGuidesData);
	bool GetHairCardsGuidesDatas(const int32 GroupIndex, const int32 LODIndex, FHairStrandsDatas& OutCardsGuidesData);

	/** Caches the computed (group) groom data with the given build settings from/to the Derived Data Cache, building it if needed.
	 *  This function assumes the interpolation settings are properly populated, as they will be used to build the asset.
	 */
	bool CacheDerivedDatas();
	bool CacheDerivedData(uint32 GroupIndex);
	bool CacheStrandsData(uint32 GroupIndex, FString& OutDerivedDataKey);
	bool CacheCardsData(uint32 GroupIndex, const FString& StrandsKey);
	bool CacheMeshesData(uint32 GroupIndex);

	FString GetDerivedDataKey(bool bUseCacheKey=false);
	FString GetDerivedDataKeyForCards(uint32 GroupIt, const FString& StrandsKey);
	FString GetDerivedDataKeyForStrands(uint32 GroupIndex);
	FString GetDerivedDataKeyForMeshes(uint32 GroupIndex);

	const FHairDescriptionGroups& GetHairDescriptionGroups();
private:
	
	bool IsFullyCached();
	TUniquePtr<FHairDescriptionBulkData> HairDescriptionBulkData[EHairDescriptionType::Count];
	EHairDescriptionType HairDescriptionType = EHairDescriptionType::Source;

	// Transient HairDescription & HairDescriptionGroups, which are built from HairDescriptionBulkData.
	// All these data (bulk/desc/groups) needs to be in sync. I.e., when the HairDescription is updated, 
	// HairDescriptionGroups needs to also be updated
	TUniquePtr<FHairDescription> CachedHairDescription[EHairDescriptionType::Count];
	TUniquePtr<FHairDescriptionGroups> CachedHairDescriptionGroups[EHairDescriptionType::Count];

	TArray<FString> StrandsDerivedDataKey;
	TArray<FString> CardsDerivedDataKey;
	TArray<FString> MeshesDerivedDataKey;

	TStrongObjectPtr<UGroomAsset> GroomAssetStrongPtr;
	bool bRetryLoadFromGameThread = false;
#endif // WITH_EDITORONLY_DATA
	bool bIsInitialized = false;
	uint32 AssetNameHash = 0;

#if WITH_EDITOR
public:
	void UpdateCachedSettings();
private:
	void SavePendingProceduralAssets();

	// Cached groom settings to know if we need to recompute interpolation data or 
	// decimation when the asset is saved
	TArray<FHairGroupsRendering>				CachedHairGroupsRendering;
	TArray<FHairGroupsPhysics>					CachedHairGroupsPhysics;
	TArray<FHairGroupsInterpolation>			CachedHairGroupsInterpolation;
	TArray<FHairGroupsLOD>						CachedHairGroupsLOD;
	TArray<FHairGroupsCardsSourceDescription>	CachedHairGroupsCards;
	TArray<FHairGroupsMeshesSourceDescription>	CachedHairGroupsMeshes;
	ERHIFeatureLevel::Type 						CachedResourcesPlatformLevel= ERHIFeatureLevel::Num;
	ERHIFeatureLevel::Type 						CachedResourcesFeatureLevel = ERHIFeatureLevel::Num;

	// Queue of procedural assets which needs to be saved
	TQueue<UStaticMesh*> AssetToSave_Meshes;
	TQueue<FHairGroupCardsTextures*> AssetToSave_Textures;
#endif // WITH_EDITOR
};

struct FGroomAssetMemoryStats
{
	struct FStats
	{
		uint32 Guides = 0;
		uint32 Strands= 0;
		uint32 Cards  = 0;
		uint32 Meshes = 0;
	};
	FStats CPU;
	FStats GPU;

	struct FStrandsDetails
	{
		uint32 Rest = 0;
		uint32 Interpolation= 0;
		uint32 Cluster  = 0;
		uint32 Raytracing = 0;
	};
	FStrandsDetails Memory;
	FStrandsDetails Curves;

	static FGroomAssetMemoryStats Get(const FHairGroupPlatformData& InData, const FHairGroupResources& In);
	void Accumulate(const FGroomAssetMemoryStats& In);
	uint32 GetTotalCPUSize() const;
	uint32 GetTotalGPUSize() const;
};