// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Interfaces/Interface_AssetUserData.h"
#include "RenderCommandFence.h"

#include "GeometryCache.generated.h"

class UGeometryCacheTrack;
class UMaterialInterface;
struct FGeometryCacheMeshData;

DECLARE_LOG_CATEGORY_EXTERN(LogGeometryCache, Log, All);

/**
* A Geometry Cache is a piece/set of geometry that consists of individual Mesh/Transformation samples.
* In contrast with Static Meshes they can have their vertices animated in certain ways. * 
*/
UCLASS(hidecategories = Object, BlueprintType, config = Engine)
class GEOMETRYCACHE_API UGeometryCache : public UObject, public IInterface_AssetUserData
{
	GENERATED_UCLASS_BODY()
public:
	//~ Begin UObject Interface.
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostInitProperties() override;
	virtual FString GetDesc() override;
	virtual void GetAssetRegistryTags(FAssetRegistryTagsContext Context) const override;
	UE_DEPRECATED(5.4, "Implement the version that takes FAssetRegistryTagsContext instead.")
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
	virtual void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) override;

	virtual void BeginDestroy() override;
	virtual bool IsReadyForFinishDestroy() override;
#if WITH_EDITOR
	virtual void PreEditChange(FProperty* PropertyAboutToChange) override;
#endif // WITH_EDITOR
	//~ End UObject Interface.

	/**
	* AddTrack
	*
	* @param Track - GeometryCacheTrack instance that is a part of the GeometryCacheAsset
	* @return void
	*/
	void AddTrack(UGeometryCacheTrack* Track);
		
	/** Clears all stored data so the reimporting step can fill the instance again*/
	void ClearForReimporting();	

#if WITH_EDITORONLY_DATA
	/** Importing data and options used for this Geometry cache object*/
	UPROPERTY(Category = ImportSettings, VisibleAnywhere, Instanced)
	TObjectPtr<class UAssetImportData> AssetImportData;	

	/** Information for thumbnail rendering */
	UPROPERTY(VisibleAnywhere, Instanced, Category = Thumbnail)
	TObjectPtr<class UThumbnailInfo> ThumbnailInfo;
#endif
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GeometryCache)
	TArray<TObjectPtr<UMaterialInterface>> Materials;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GeometryCache)
	TArray<FName> MaterialSlotNames;

	/** GeometryCache track defining the samples/geometry data for this GeomCache instance */
	UPROPERTY(VisibleAnywhere, Category=GeometryCache)
	TArray<TObjectPtr<UGeometryCacheTrack>> Tracks;

	/** Set the start and end frames for the GeometryCache */
	void SetFrameStartEnd(int32 InStartFrame, int32 InEndFrame);

	/** Get the start frame */
	int32 GetStartFrame() const;

	/** Get the end frame */
	int32 GetEndFrame() const;
	
	/** Calculate it's duration */
	float CalculateDuration() const;

	/** Get the Frame at the Specified Time */
	int32  GetFrameAtTime(const float Time) const;

	/** Get the mesh data at the specified time */
	void GetMeshDataAtTime(float Time, TArray<FGeometryCacheMeshData>& OutMeshData) const;

	/** Get the hash of the meshes data of the GeometryCache */
	FString GetHash() const;

public:
	/** Array of user data stored with the asset */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Instanced, Category = Hidden)
	TArray<TObjectPtr<UAssetUserData>> AssetUserData;

	//~ Begin IInterface_AssetUserData Interface
	virtual void AddAssetUserData(UAssetUserData* InUserData) override;
	virtual void RemoveUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass) override;
	virtual UAssetUserData* GetAssetUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass) override;
	virtual const TArray<UAssetUserData*>* GetAssetUserDataArray() const override;
	//~ End IInterface_AssetUserData Interface

private:
	/** A fence which is used to keep track of the rendering thread releasing the geometry cache resources. */
	FRenderCommandFence ReleaseResourcesFence;

protected:
	UPROPERTY(BlueprintReadOnly, Category = GeometryCache)
	int32 StartFrame;

	UPROPERTY(BlueprintReadOnly, Category = GeometryCache)
	int32 EndFrame;

	UPROPERTY()
	uint64 Hash;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
