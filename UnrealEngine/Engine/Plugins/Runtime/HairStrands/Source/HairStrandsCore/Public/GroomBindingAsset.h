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
struct FHairGroupData;

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

/** Binding bulk data */
struct FHairGroupBulkData
{
	FHairStrandsRootBulkData		SimRootBulkData;
	FHairStrandsRootBulkData		RenRootBulkData;
	TArray<FHairStrandsRootBulkData>CardsRootBulkData;
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

public:
	/** Type of mesh to create groom binding for */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "BuildSettings")
	EGroomBindingMeshType GroomBindingType = EGroomBindingMeshType::SkeletalMesh;

	/** Groom to bind. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BuildSettings", AssetRegistrySearchable)
	TObjectPtr<UGroomAsset> Groom;

	/** Skeletal mesh on which the groom has been authored. This is optional, and used only if the hair
		binding is done a different mesh than the one which it has been authored */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BuildSettings")
	TObjectPtr<USkeletalMesh> SourceSkeletalMesh;

	/** Skeletal mesh on which the groom is attached to. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BuildSettings")
	TObjectPtr<USkeletalMesh> TargetSkeletalMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "BuildSettings")
	TObjectPtr<UGeometryCache> SourceGeometryCache;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "BuildSettings")
	TObjectPtr<UGeometryCache> TargetGeometryCache;

	/** Number of points used for the rbf interpolation */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "BuildSettings")
	int32 NumInterpolationPoints = 100;

	/** Number of points used for the rbf interpolation */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "BuildSettings")
	int32 MatchingSection = 0;

	UPROPERTY(EditAnywhere, EditFixedSize, BlueprintReadWrite, Category = "HairGroups", meta = (DisplayName = "Group"))
	TArray<FGoomBindingGroupInfo> GroupInfos;

	/** GPU and CPU binding data for both simulation and rendering. */
	struct FHairGroupResource
	{
		FHairStrandsRestRootResource* SimRootResources = nullptr;
		FHairStrandsRestRootResource* RenRootResources = nullptr;
		TArray<FHairStrandsRestRootResource*> CardsRootResources;
	};
	typedef TArray<FHairGroupResource> FHairGroupResources;
	FHairGroupResources HairGroupResources;

	/** Queue of resources which needs to be deleted. This queue is needed for keeping valid pointer on the group resources 
	   when the binding asset is recomputed */
	TQueue<FHairGroupResource> HairGroupResourcesToDelete;

	/** Root bulk data for each hair gruops */
	TArray<FHairGroupBulkData> HairGroupBulkDatas;

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

#if WITH_EDITOR
	FOnGroomBindingAssetChanged& GetOnGroomBindingAssetChanged() { return OnGroomBindingAssetChanged; }

	/**  Part of UObject interface  */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	
#endif // WITH_EDITOR

	enum class EQueryStatus
	{
		None,
		Submitted,
		Completed
	};
	volatile EQueryStatus QueryStatus = EQueryStatus::None;

	/** Initialize resources. */
	void InitResource();

	/** Update resources. */
	void UpdateResource();

	/** Release the hair strands resource. */
	void ReleaseResource();

	void Reset();

	/** Return true if the binding asset is valid, i.e., correctly built and loaded. */
	bool IsValid() const { return bIsValid;  }

	//private :
#if WITH_EDITOR
	FOnGroomBindingAssetChanged OnGroomBindingAssetChanged;
#endif

#if WITH_EDITORONLY_DATA
	/** Build/rebuild a binding asset */
	void Build();

	void CacheDerivedDatas();
	void InvalidateBinding();
	void InvalidateBinding(class USkeletalMesh*);
	bool bRegisterSourceMeshCallback = false;
	bool bRegisterTargetMeshCallback = false;
	bool bRegisterGroomAssetCallback = false;
	FString CachedDerivedDataKey;
#endif
	bool bIsValid = false;
};

UCLASS(BlueprintType, hidecategories = (Object))
class HAIRSTRANDSCORE_API UGroomBindingAssetList : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Transient, EditFixedSize, Category = "Bindings")
	TArray<TObjectPtr<UGroomBindingAsset>> Bindings;
};