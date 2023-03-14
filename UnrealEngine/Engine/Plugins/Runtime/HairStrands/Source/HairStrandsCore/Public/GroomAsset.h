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

	UPROPERTY(VisibleAnywhere, Category = "Info", meta = (DisplayName = "Imported width (DCC) of the larget hair strands"))
	float MaxImportedWidth = -1.f;

	UPROPERTY(VisibleAnywhere, Category = "Info", meta = (DisplayName = "Card Group ID for split hair card generation"))
	int32 GroupCardsID = 0;
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


/** Describe all data & resource for a groom asset's hair group */
struct HAIRSTRANDSCORE_API FHairGroupData
{
	////////////////////////////////////////////////////////////////////////////
	// Helper

	struct FBase
	{
		bool HasValidData() const		{ return BulkData.GetNumPoints() > 0;}
		bool IsValid() const			{ return RestResource != nullptr; }
		const FBox& GetBounds() const	{ return BulkData.GetBounds(); }

		uint32 GetDataSize() const
		{
			uint32 Total = 0;
			Total += BulkData.Positions.IsBulkDataLoaded() 		? BulkData.Positions.GetBulkDataSize()   : 0;
			Total += BulkData.Attributes0.IsBulkDataLoaded() 	? BulkData.Attributes0.GetBulkDataSize() : 0;
			Total += BulkData.Attributes1.IsBulkDataLoaded() 	? BulkData.Attributes1.GetBulkDataSize() : 0;
			Total += BulkData.Materials.IsBulkDataLoaded() 		? BulkData.Materials.GetBulkDataSize()   : 0;
			Total += BulkData.CurveOffsets.IsBulkDataLoaded() 	? BulkData.CurveOffsets.GetBulkDataSize(): 0;
			return Total;
		}

		FHairStrandsBulkData				BulkData;
		FHairStrandsRestResource*			RestResource = nullptr;
	};

	struct FBaseWithInterpolation : FBase
	{
		uint32 GetDataSize() const
		{
			uint32 Total = 0;
			Total += FBase::GetDataSize();
			Total += InterpolationBulkData.Interpolation.IsBulkDataLoaded()		? InterpolationBulkData.Interpolation.GetBulkDataSize() : 0;
			Total += InterpolationBulkData.Interpolation0.IsBulkDataLoaded()	? InterpolationBulkData.Interpolation0.GetBulkDataSize() : 0;
			Total += InterpolationBulkData.Interpolation1.IsBulkDataLoaded()	? InterpolationBulkData.Interpolation1.GetBulkDataSize() : 0;
			Total += InterpolationBulkData.SimRootPointIndex.IsBulkDataLoaded()	? InterpolationBulkData.SimRootPointIndex.GetBulkDataSize() : 0;
			return Total;
		}

		FHairStrandsInterpolationBulkData	InterpolationBulkData;
		FHairStrandsInterpolationResource*	InterpolationResource = nullptr;
	};

	////////////////////////////////////////////////////////////////////////////
	// Data

	struct FSim : FBase
	{
		/* Return the memory size for GPU resources */
		uint32 GetResourcesSize() const
		{
			uint32 Total = 0;
			if (RestResource) Total += RestResource->GetResourcesSize();
			return Total;
		}

		uint32 GetDataSize() const
		{
			uint32 Total = 0;
			Total += FBase::GetDataSize();
			return Total;
		}

	} Guides;

	struct FStrands : FBaseWithInterpolation
	{
		/* Return the memory size for GPU resources */
		uint32 GetResourcesSize() const
		{
			uint32 Total = 0;
			if (RestResource) Total += RestResource->GetResourcesSize();
			if (InterpolationResource) Total += InterpolationResource->GetResourcesSize();
			if (ClusterCullingResource) Total += ClusterCullingResource->GetResourcesSize();
			#if RHI_RAYTRACING
			if (RaytracingResource) Total += RaytracingResource->GetResourcesSize();
			#endif
			return Total;
		}

		uint32 GetDataSize() const;
#if 0
		{
			uint32 Total = 0;
			Total += FBaseWithInterpolation::GetDataSize();

			Total += ClusterCullingBulkData.LODVisibility.GetAllocatedSize();
			Total += ClusterCullingBulkData.CPULODScreenSize.GetAllocatedSize();
			Total += ClusterCullingBulkData.ClusterInfos.IsBulkDataLoaded()			? ClusterCullingBulkData.ClusterInfos.GetBulkDataSize() : 0;
			Total += ClusterCullingBulkData.ClusterLODInfos.IsBulkDataLoaded()		? ClusterCullingBulkData.ClusterLODInfos.GetBulkDataSize() : 0;
			Total += ClusterCullingBulkData.VertexToClusterIds.IsBulkDataLoaded()	? ClusterCullingBulkData.VertexToClusterIds.GetBulkDataSize() : 0;
			Total += ClusterCullingBulkData.ClusterVertexIds.IsBulkDataLoaded()		? ClusterCullingBulkData.ClusterVertexIds.GetBulkDataSize() : 0;
			return Total;
		}
#endif
		FHairStrandsClusterCullingBulkData	ClusterCullingBulkData;
		FHairStrandsClusterCullingResource* ClusterCullingResource = nullptr;

		#if RHI_RAYTRACING
		FHairStrandsRaytracingResource*		RaytracingResource = nullptr;
		#endif

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

		bool HasValidData(uint32 LODIt) const { return LODIt < uint32(LODs.Num()) && LODs[LODIt].HasValidData(); }
		bool IsValid(uint32 LODIt) const { return LODIt < uint32(LODs.Num()) && LODs[LODIt].IsValid(); }
		FBox GetBounds() const
		{
			for (const FLOD& LOD : LODs)
			{
				if (LOD.IsValid()) return LOD.BulkData.BoundingBox;
			}
			return FBox();
		}

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
			/* Return the memory size for GPU resources */
			uint32 GetResourcesSize() const
			{
				uint32 Total = 0;
				if (RestResource) Total += RestResource->GetResourcesSize();
				if (InterpolationResource) Total += InterpolationResource->GetResourcesSize();
				#if RHI_RAYTRACING
				if (RaytracingResource) Total += RaytracingResource->GetResourcesSize();
				#endif
				return Total;
			}

			uint32 GetDataSize() const
			{
				uint32 Total = 0;
				Total += BulkData.Positions.GetAllocatedSize();
				Total += BulkData.Normals.GetAllocatedSize();
				Total += BulkData.UVs.GetAllocatedSize();
				Total += BulkData.Indices.GetAllocatedSize();

				Total += InterpolationBulkData.Interpolation.GetAllocatedSize();

				Total += Guides.GetDataSize();
				return Total;
			}

			bool HasValidData() const { return BulkData.IsValid(); }
			bool IsValid() const { return BulkData.IsValid() && RestResource != nullptr; }

			// Main data & Resources
			FHairCardsBulkData					BulkData;
			FHairCardsRestResource*				RestResource = nullptr;

			// Interpolation data & resources
			FHairCardsInterpolationBulkData		InterpolationBulkData;
			FHairCardsInterpolationResource*	InterpolationResource = nullptr;

			FBaseWithInterpolation				Guides;

			#if RHI_RAYTRACING
			FHairStrandsRaytracingResource*		RaytracingResource = nullptr;
			#endif

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
		bool HasValidData(uint32 LODIt) const { return LODIt < uint32(LODs.Num()) && LODs[LODIt].HasValidData(); }
		bool IsValid(uint32 LODIt) const { return LODIt < uint32(LODs.Num()) && LODs[LODIt].IsValid(); }
		FBox GetBounds() const
		{
			for (const FLOD& LOD : LODs)
			{
				if (LOD.IsValid()) return LOD.BulkData.BoundingBox;
			}
			return FBox();
		}
		
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
			/* Return the memory size for GPU resources */
			uint32 GetResourcesSize() const
			{
				uint32 Total = 0;
				if (RestResource) Total += RestResource->GetResourcesSize();
				#if RHI_RAYTRACING
				if (RaytracingResource) Total += RaytracingResource->GetResourcesSize();
				#endif
				return Total;
			}

			uint32 GetDataSize() const
			{
				uint32 Total = 0;
				Total += BulkData.Positions.GetAllocatedSize();
				Total += BulkData.Normals.GetAllocatedSize();
				Total += BulkData.UVs.GetAllocatedSize();
				Total += BulkData.Indices.GetAllocatedSize();
				return Total;
			}

			bool HasValidData() const { return BulkData.IsValid(); }
			bool IsValid() const { return BulkData.IsValid() && RestResource != nullptr; }

			FHairMeshesBulkData BulkData;
			FHairMeshesRestResource* RestResource = nullptr;
			#if RHI_RAYTRACING
			FHairStrandsRaytracingResource* RaytracingResource = nullptr;
			#endif
			bool bIsCookedOut = false;
		};
		TArray<FLOD> LODs;
	} Meshes;

	struct FDebug
	{
		FHairStrandsDebugDatas Data;
		FHairStrandsDebugDatas::FResources* Resource = nullptr;
	} Debug;

	bool bIsCookedOut = false;
};

struct FHairDescriptionGroup
{
	FHairGroupInfo		Info;
	FHairStrandsDatas	Strands;
	FHairStrandsDatas	Guides;

	bool  bCanUseClosestGuidesAndWeights = false;
	bool  bHasUVData = false;
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
	TArray<const FVertexFactoryType*, TInlineAllocator<2>> VertexFactoryTypes;
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

public:
	
	UPROPERTY(EditAnywhere, Transient, EditFixedSize, Category = "HairInfo", meta = (DisplayName = "Group"))
	TArray<FHairGroupInfoWithVisibility> HairGroupsInfo;

	UPROPERTY(EditAnywhere, EditFixedSize, BlueprintReadWrite, Category = "HairRendering", meta = (DisplayName = "Group"))
	TArray<FHairGroupsRendering> HairGroupsRendering;

	UPROPERTY(EditAnywhere, EditFixedSize, BlueprintReadWrite, Category = "HairPhysics", meta = (DisplayName = "Group"))
	TArray<FHairGroupsPhysics> HairGroupsPhysics;

	UPROPERTY(EditAnywhere, EditFixedSize, BlueprintReadWrite, Category = "HairInterpolation", meta = (DisplayName = "Group"))
	TArray<FHairGroupsInterpolation> HairGroupsInterpolation;

	UPROPERTY(EditAnywhere, EditFixedSize, BlueprintReadWrite, Category = "HairLOD", meta = (DisplayName = "Group"))
	TArray<FHairGroupsLOD> HairGroupsLOD;

	/** Cards - Source description data */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HairCards", meta = (DisplayName = "Group"))
	TArray<FHairGroupsCardsSourceDescription> HairGroupsCards;

	/** Meshes - Source description data */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HairMeshes", meta = (DisplayName = "Group"))
	TArray<FHairGroupsMeshesSourceDescription> HairGroupsMeshes;

	/** Meshes - Source description data */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HairMaterials", meta = (DisplayName = "Group"))
	TArray<FHairGroupsMaterial> HairGroupsMaterials;

	/** Store strands/cards/meshes data */
	TArray<FHairGroupData> HairGroupsData;

	/** Enable radial basis function interpolation to be used instead of the local skin rigid transform */
	UPROPERTY(EditAnywhere, EditFixedSize, BlueprintReadWrite, Category = "HairInterpolation", meta = (ToolTip = "Enable radial basis function interpolation to be used instead of the local skin rigid transform (WIP)", DisplayName = "RBF Interpolation"))
	bool EnableGlobalInterpolation = false;

	/** Type of interpolation used */
	UPROPERTY(EditAnywhere, EditFixedSize, BlueprintReadWrite, Category = "HairInterpolation", meta = (ToolTip = "Type of interpolation used (WIP)"))
	EGroomInterpolationType HairInterpolationType = EGroomInterpolationType::SmoothTransform;

	/** Deformed skeletal mesh that will drive the groom deformation/simulation. For creating this skeletal mesh, enable EnableDeformation within the interpolation settings*/
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "HairInterpolation")
	TObjectPtr<USkeletalMesh> RiggedSkeletalMesh;

	/** Deformed skeletal mesh mapping from groups to sections */
	UPROPERTY()
	TArray<int32> DeformedGroupSections;

	/** Minimum LOD to cook */
	UPROPERTY(EditAnywhere, Category = "LOD", meta = (DisplayName = "Minimum LOD"))
	FPerPlatformInt MinLOD;

	/** When true all LODs below MinLod will still be cooked */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "LOD")
	FPerPlatformBool DisableBelowMinLodStripping;

	/** The LOD bias to use after LOD stripping, regardless of MinLOD. Computed at cook time */
	UPROPERTY()
	TArray<float> EffectiveLODBias;

	//~ Begin UObject Interface.
	virtual void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) override;
	virtual void PostLoad() override;
	virtual void BeginDestroy() override;
	virtual void Serialize(FArchive& Ar) override;

	PRAGMA_DISABLE_DEPRECATION_WARNINGS // Suppress compiler warning on override of deprecated function
	UE_DEPRECATED(5.0, "Use version that takes FObjectPreSaveContext instead.")
	virtual void PreSave(const class ITargetPlatform* TargetPlatform) override;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	virtual void PreSave(FObjectPreSaveContext ObjectSaveContext) override;

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

	/** Retrievde the asset tags*/
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

//private :
#if WITH_EDITOR
	FOnGroomAssetChanged OnGroomAssetChanged;
	FOnGroomAssetResourcesChanged OnGroomAssetResourcesChanged;
	FOnGroomAsyncLoadFinished OnGroomAsyncLoadFinished;

	void MarkMaterialsHasChanged();

	// Save out a static mesh based on generated cards
	void SaveProceduralCards(uint32 CardsGroupIndex);
	
#endif

	/** Array of user data stored with the asset */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Instanced, Category = Hidden)
	TArray<TObjectPtr<UAssetUserData>> AssetUserData;

	/* Return the material slot index corresponding to the material name */
	int32 GetMaterialIndex(FName MaterialSlotName) const;
	bool IsMaterialSlotNameValid(FName MaterialSlotName) const;
	bool IsMaterialUsed(int32 MaterialIndex) const;
	TArray<FName> GetMaterialSlotNames() const;

	bool BuildCardsGeometry();
	bool BuildMeshesGeometry();

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

	bool BuildCardsGeometry(uint32 GroupIndex);
	bool BuildMeshesGeometry(uint32 GroupIndex);

	bool HasValidCardsData(uint32 GroupIndex) const;
	bool HasValidMeshesData(uint32 GroupIndex) const;
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
	bool CacheCardsGeometry(uint32 GroupIndex, const FString& StrandsKey);
	bool CacheMeshesGeometry(uint32 GroupIndex);

	FString GetDerivedDataKey();
	FString GetDerivedDataKeyForCards(uint32 GroupIt, const FString& StrandsKey);
	FString GetDerivedDataKeyForStrands(uint32 GroupIndex);
	FString GetDerivedDataKeyForMeshes(uint32 GroupIndex);

	const FHairDescriptionGroups& GetHairDescriptionGroups();
private:
	
	FString BuildDerivedDataKeySuffix(uint32 GroupIndex, const FHairGroupsInterpolation& InterpolationSettings, const FHairGroupsLOD& LODSettings) const;
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

	// Queue of procedural assets which needs to be saved
	TQueue<UStaticMesh*> AssetToSave_Meshes;
	TQueue<FHairGroupCardsTextures*> AssetToSave_Textures;
#endif // WITH_EDITOR
};
