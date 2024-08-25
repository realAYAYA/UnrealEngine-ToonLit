// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "HairDescription.h"
#include "HairStrandsDatas.h"
#include "RenderResource.h"
#include "GroomResources.h"
#include "HairStrandsInterface.h"
#include "Engine/SkeletalMesh.h"
#include "GroomBindingAsset.generated.h"


class UAssetUserData;
class UGeometryCache;
class UMaterialInterface;
class UNiagaraSystem;
class UGroomAsset;

USTRUCT(BlueprintType)
struct HAIRSTRANDSCORE_API FGoomBindingGroupInfo
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "Info", meta = (DisplayName = "Curve Count"))
	int32 RenRootCount = 0;

	UPROPERTY(VisibleAnywhere, Category = "Info", meta = (DisplayName = "Curve LOD"))
	int32 RenLODCount = 0;

	UPROPERTY(VisibleAnywhere, Category = "Info", meta = (DisplayName = "Guide Count"))
	int32 SimRootCount = 0;

	UPROPERTY(VisibleAnywhere, Category = "Info", meta = (DisplayName = "Guide LOD"))
	int32 SimLODCount = 0;
};

/** Enum that describes the type of mesh to bind to */
UENUM(BlueprintType)
enum class EGroomBindingMeshType : uint8
{
	SkeletalMesh,
	GeometryCache
};

/**
 * Implements an asset that can be used to store binding information between a groom and a skeletal mesh
 */
UCLASS(BlueprintType, hidecategories = (Object))
class HAIRSTRANDSCORE_API UGroomBindingAsset : public UObject
{
	GENERATED_BODY()

#if WITH_EDITOR
	/** Notification when anything changed */
	DECLARE_MULTICAST_DELEGATE(FOnGroomBindingAssetChanged);
#endif

private:
	/** Type of mesh to create groom binding for */
	UE_DEPRECATED(5.3, "Please do not access this member directly; use UGroomBindingAsset::GetGroomBindingType() or UGroomBindingAsset::SetGroomBindingType().")
	UPROPERTY(VisibleAnywhere, BlueprintGetter = GetGroomBindingType, BlueprintSetter = SetGroomBindingType, Category = "BuildSettings")
	EGroomBindingMeshType GroomBindingType = EGroomBindingMeshType::SkeletalMesh;

public:
	static FName GetGroomBindingTypeMemberName();
	UFUNCTION(BlueprintGetter) EGroomBindingMeshType GetGroomBindingType() const;
	UFUNCTION(BlueprintSetter) void SetGroomBindingType(EGroomBindingMeshType InGroomBindingType);

private:
	/** Groom to bind. */
	UE_DEPRECATED(5.3, "Please do not access this member directly; use UGroomBindingAsset::GetGroom() or UGroomBindingAsset::SetGroom().")
	UPROPERTY(EditAnywhere, BlueprintGetter = GetGroom, BlueprintSetter = SetGroom, Category = "BuildSettings", AssetRegistrySearchable)
	TObjectPtr<UGroomAsset> Groom;

public:
	static FName GetGroomMemberName();
	UFUNCTION(BlueprintGetter) UGroomAsset* GetGroom() const;
	UFUNCTION(BlueprintSetter) void SetGroom(UGroomAsset* InGroom);

private:
	/** Skeletal mesh on which the groom has been authored. This is optional, and used only if the hair
		binding is done a different mesh than the one which it has been authored */
	UE_DEPRECATED(5.3, "Please do not access this member directly; use UGroomBindingAsset::GetSourceSkeletalMesh() or UGroomBindingAsset::SetSourceSkeletalMesh().")
	UPROPERTY(EditAnywhere, BlueprintGetter = GetSourceSkeletalMesh, BlueprintSetter = SetSourceSkeletalMesh, Category = "BuildSettings")
	TObjectPtr<USkeletalMesh> SourceSkeletalMesh;

public:
	static FName GetSourceSkeletalMeshMemberName();
	UFUNCTION(BlueprintGetter) USkeletalMesh* GetSourceSkeletalMesh() const;
	UFUNCTION(BlueprintSetter) void SetSourceSkeletalMesh(USkeletalMesh* InSkeletalMesh);

private:
	/** Skeletal mesh on which the groom is attached to. */
	UE_DEPRECATED(5.3, "Please do not access this member directly; use UGroomBindingAsset::GetTargetSkeletalMesh() or UGroomBindingAsset::SetTargetSkeletalMesh().")
	UPROPERTY(EditAnywhere, BlueprintGetter = GetTargetSkeletalMesh, BlueprintSetter = SetTargetSkeletalMesh, Category = "BuildSettings")
	TObjectPtr<USkeletalMesh> TargetSkeletalMesh;

public:
	static FName GetTargetSkeletalMeshMemberName();
	UFUNCTION(BlueprintGetter) USkeletalMesh* GetTargetSkeletalMesh() const;
	UFUNCTION(BlueprintSetter) void SetTargetSkeletalMesh(USkeletalMesh* InSkeletalMesh);

private:
	UE_DEPRECATED(5.3, "Please do not access this member directly; use UGroomBindingAsset::GetSourceGeometryCache() or UGroomBindingAsset::SetSourceGeometryCache().")
	UPROPERTY(VisibleAnywhere, BlueprintGetter = GetSourceGeometryCache, BlueprintSetter = SetSourceGeometryCache, Category = "BuildSettings")
	TObjectPtr<UGeometryCache> SourceGeometryCache;

public:
	static FName GetSourceGeometryCacheMemberName();
	UFUNCTION(BlueprintGetter) UGeometryCache* GetSourceGeometryCache() const;
	UFUNCTION(BlueprintSetter) void SetSourceGeometryCache(UGeometryCache* InGeometryCache);

private:
	UE_DEPRECATED(5.3, "Please do not access this member directly; use UGroomBindingAsset::GetTargetGeometryCache() or UGroomBindingAsset::SetTargetGeometryCache().")
	UPROPERTY(VisibleAnywhere, BlueprintGetter = GetTargetGeometryCache, BlueprintSetter = SetTargetGeometryCache, Category = "BuildSettings")
	TObjectPtr<UGeometryCache> TargetGeometryCache;

public:
	static FName GetTargetGeometryCacheMemberName();
	UFUNCTION(BlueprintGetter) UGeometryCache* GetTargetGeometryCache() const;
	UFUNCTION(BlueprintSetter) void SetTargetGeometryCache(UGeometryCache* InGeometryCache);

private:
	/** Number of points used for the rbf interpolation */
	UE_DEPRECATED(5.3, "Please do not access this member directly; use UGroomBindingAsset::GetNumInterpolationPoints() or UGroomBindingAsset::SetNumInterpolationPoints().")
	UPROPERTY(VisibleAnywhere, BlueprintGetter = GetNumInterpolationPoints, BlueprintSetter = SetNumInterpolationPoints, Category = "BuildSettings")
	int32 NumInterpolationPoints = 100;

public:
	static FName GetNumInterpolationPointsMemberName();
	UFUNCTION(BlueprintGetter) int32 GetNumInterpolationPoints() const;
	UFUNCTION(BlueprintSetter) void SetNumInterpolationPoints(int32 InNumInterpolationPoints);

private:
	/** Number of points used for the rbf interpolation */
	UE_DEPRECATED(5.3, "Please do not access this member directly; use UGroomBindingAsset::GetMatchingSection() or UGroomBindingAsset::SetMatchingSection().")
	UPROPERTY(VisibleAnywhere, BlueprintGetter = GetMatchingSection, BlueprintSetter = SetMatchingSection, Category = "BuildSettings")
	int32 MatchingSection = 0;

public:
	static FName GetMatchingSectionMemberName();
	UFUNCTION(BlueprintGetter) int32 GetMatchingSection() const;
	UFUNCTION(BlueprintSetter) void SetMatchingSection(int32 InMatchingSection);

private:
	UE_DEPRECATED(5.3, "Please do not access this member directly; use UGroomBindingAsset::GetGroupInfos() or UGroomBindingAsset::SetGroupInfos().")
	UPROPERTY(EditAnywhere, EditFixedSize, BlueprintGetter = GetGroupInfos, BlueprintSetter = SetGroupInfos, Category = "HairGroups", meta = (DisplayName = "Group"))
	TArray<FGoomBindingGroupInfo> GroupInfos;

public:
	static FName GetGroupInfosMemberName();
	UFUNCTION(BlueprintGetter) const TArray<FGoomBindingGroupInfo>& GetGroupInfos() const;
	UFUNCTION(BlueprintSetter) void SetGroupInfos(const TArray<FGoomBindingGroupInfo>& InGroupInfos);
	TArray<FGoomBindingGroupInfo>& GetGroupInfos();

	/** GPU and CPU binding data for both simulation and rendering. */
	struct FHairGroupResource
	{
		FHairStrandsRestRootResource* SimRootResources = nullptr;
		FHairStrandsRestRootResource* RenRootResources = nullptr;
		TArray<FHairStrandsRestRootResource*> CardsRootResources;
	};
	typedef TArray<FHairGroupResource> FHairGroupResources;

private:
	UE_DEPRECATED(5.3, "Please do not access this member directly; use UGroomBindingAsset::GetHairGroupResources() or UGroomBindingAsset::SetHairGroupResources().")
	FHairGroupResources HairGroupResources;

public:
	static FName GetHairGroupResourcesMemberName();
	FHairGroupResources& GetHairGroupResources();
	const FHairGroupResources& GetHairGroupResources() const;
	void SetHairGroupResources(FHairGroupResources InHairGroupResources);

	/** Binding bulk data */
	struct FHairGroupPlatformData
	{
		TArray<FHairStrandsRootBulkData>		 SimRootBulkDatas;
		TArray<FHairStrandsRootBulkData>		 RenRootBulkDatas;
		TArray<TArray<FHairStrandsRootBulkData>> CardsRootBulkDatas;
	};

private:
	/** Queue of resources which needs to be deleted. This queue is needed for keeping valid pointer on the group resources 
	   when the binding asset is recomputed */
	UE_DEPRECATED(5.3, "Please do not access this member directly; use UGroomBindingAsset::GetHairGroupPlatformData().")
	TQueue<FHairGroupResource> HairGroupResourcesToDelete;

	/** Platform data for each hair groups */
	UE_DEPRECATED(5.3, "Please do not access this member directly; use UGroomBindingAsset::GetHairGroupPlatformData().")
	TArray<FHairGroupPlatformData> HairGroupsPlatformData;

public:
	void AddHairGroupResourcesToDelete(FHairGroupResource& In);
	bool RemoveHairGroupResourcesToDelete(FHairGroupResource& Out);

	static FName GetHairGroupPlatformDataMemberName();
	const TArray<FHairGroupPlatformData>& GetHairGroupsPlatformData() const;
	TArray<FHairGroupPlatformData>& GetHairGroupsPlatformData();

public:
	//~ Begin UObject Interface.
	virtual void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) override;
	virtual void PostLoad() override;
	PRAGMA_DISABLE_DEPRECATION_WARNINGS // Suppress compiler warning on override of deprecated function
	UE_DEPRECATED(5.0, "Use version that takes FObjectPreSaveContext instead.")
	virtual void PreSave(const class ITargetPlatform* TargetPlatform) override;
	UE_DEPRECATED(5.0, "Use version that takes FObjectPostSaveRootContext instead.")
	virtual void PostSaveRoot(bool bCleanupIsRequired) override;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	virtual void PreSave(FObjectPreSaveContext ObjectSaveContext) override;
	virtual void PostSaveRoot(FObjectPostSaveRootContext ObjectSaveContext) override;
	virtual void BeginDestroy() override;
	virtual void Serialize(FArchive& Ar) override;

	static bool IsCompatible(const USkeletalMesh* InSkeletalMesh, const UGroomBindingAsset* InBinding, bool bIssueWarning);
	static bool IsCompatible(const UGeometryCache* InGeometryCache, const UGroomBindingAsset* InBinding, bool bIssueWarning);
	static bool IsCompatible(const UGroomAsset* InGroom, const UGroomBindingAsset* InBinding, bool bIssueWarning);
	static bool IsBindingAssetValid(const UGroomBindingAsset* InBinding, bool bIsBindingReloading, bool bIssueWarning);

	/** Returns true if the target is not null and matches the binding type */ 
	bool HasValidTarget() const;

	/** Helper function to return the asset path name, optionally joined with the LOD index if LODIndex > -1. */
	FName GetAssetPathName(int32 LODIndex = -1);
	uint32 GetAssetHash() const { return AssetNameHash; }

#if WITH_EDITOR
	FOnGroomBindingAssetChanged& GetOnGroomBindingAssetChanged() { return OnGroomBindingAssetChanged; }

	/**  Part of UObject interface  */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	
#endif // WITH_EDITOR

	/** Initialize resources. */
	void InitResource();

	/** Update resources. */
	void UpdateResource();

	/** Release the hair strands resource. */
	void ReleaseResource(bool bResetLoadedSize);

	void Reset();

	/** Return true if the binding asset is valid, i.e., correctly built and loaded. */
	bool IsValid() const { return bIsValid;  }

	//private :
#if WITH_EDITOR
	FOnGroomBindingAssetChanged OnGroomBindingAssetChanged;

	void RecreateResources();
	void ChangeFeatureLevel(ERHIFeatureLevel::Type PendingFeatureLevel);
	void ChangePlatformLevel(ERHIFeatureLevel::Type PendingFeatureLevel);
#endif

#if WITH_EDITORONLY_DATA
	/** Build/rebuild a binding asset */
	void Build();

	void CacheDerivedDatas();

	virtual void BeginCacheForCookedPlatformData(const ITargetPlatform* TargetPlatform);
	virtual void ClearAllCachedCookedPlatformData();
	TArray<FHairGroupPlatformData>* GetCachedCookedPlatformData(const ITargetPlatform* TargetPlatform);

	void InvalidateBinding();
	void InvalidateBinding(class USkeletalMesh*);

	struct FCachedCookedPlatformData
	{
		TArray<FString> GroupDerivedDataKeys;
		TArray<FHairGroupPlatformData> GroupPlatformDatas;
	};
private:
	TArray<FCachedCookedPlatformData*> CachedCookedPlatformDatas;

	bool bRegisterSourceMeshCallback = false;
	bool bRegisterTargetMeshCallback = false;
	bool bRegisterGroomAssetCallback = false;
	TArray<FString> CachedDerivedDataKey;
#endif
#if WITH_EDITOR
	ERHIFeatureLevel::Type CachedResourcesFeatureLevel = ERHIFeatureLevel::Num;
	ERHIFeatureLevel::Type CachedResourcesPlatformLevel = ERHIFeatureLevel::Num;
#endif
	bool bIsValid = false;
	uint32 AssetNameHash = 0;
};

UCLASS(BlueprintType, hidecategories = (Object))
class HAIRSTRANDSCORE_API UGroomBindingAssetList : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Transient, EditFixedSize, Category = "Bindings")
	TArray<TObjectPtr<UGroomBindingAsset>> Bindings;
};

struct FGroomBindingAssetMemoryStats
{
	struct FStats
	{
		uint32 Guides = 0;
		uint32 Strands= 0;
		uint32 Cards  = 0;
	};
	FStats CPU;
	FStats GPU;

	static FGroomBindingAssetMemoryStats Get(const UGroomBindingAsset::FHairGroupPlatformData& InCPU, const UGroomBindingAsset::FHairGroupResource& InGPU);
	void Accumulate(const FGroomBindingAssetMemoryStats& In);
	uint32 GetTotalCPUSize() const;
	uint32 GetTotalGPUSize() const;
};