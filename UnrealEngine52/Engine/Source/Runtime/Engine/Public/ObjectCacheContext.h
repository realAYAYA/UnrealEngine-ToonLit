// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Containers/Set.h"
#include "Containers/Map.h"
#include "Misc/Optional.h"
#include "UObject/ObjectKey.h"
#include "Misc/ScopeRWLock.h"

class UObject;
class UPrimitiveComponent;
class UStaticMeshComponent;
class USkinnedMeshComponent;
class UMaterialInterface;
class UStaticMesh;
class UTexture;
class USkeletalMesh;
class USkinnedAsset;

/**
 *  Class to abstract implementation details of the containers used inside the
 *  object cache so they can be changed later if needed without API changes.
 */
template< class T > class TObjectCacheIterator
{
public:
	/**
	 * Constructor
	 */
	explicit TObjectCacheIterator(TArray<T*>&& InObjectArray)
		: OwnedArray(MoveTemp(InObjectArray))
		, ArrayView(OwnedArray)
	{
	}

	/**
	 * Constructor
	 */
	explicit TObjectCacheIterator(TArrayView<T*> InArrayView)
		: ArrayView(InArrayView)
	{
	}

	FORCEINLINE const int32 IsEmpty() const { return ArrayView.IsEmpty(); }

	/**
	 * DO NOT USE DIRECTLY
	 * STL-like iterators to enable range-based for loop support.
	 */
	FORCEINLINE T** begin() const { return ArrayView.begin(); }
	FORCEINLINE T** end() const { return ArrayView.end(); }

protected:
	/** Sometimes, a copy might be necessary to maintain thread-safety */
	TArray<T*> OwnedArray;
	/** The array view is used to do the iteration, the arrayview might point to another object if the array is not owned **/
	TArrayView<T*> ArrayView;
};

/** 
 *  Context containing a lazy initialized ObjectIterator cache along with some useful
 *  reverse lookup tables that can be used during heavy scene updates
 *  of async asset compilation.
 */
class ENGINE_API FObjectCacheContext
{
public:
	TObjectCacheIterator<UPrimitiveComponent>   GetPrimitiveComponents();
	TObjectCacheIterator<UStaticMeshComponent>  GetStaticMeshComponents();
	TObjectCacheIterator<USkinnedMeshComponent> GetSkinnedMeshComponents();
	TObjectCacheIterator<USkinnedMeshComponent> GetSkinnedMeshComponents(USkinnedAsset* InSkinnedAsset);
	TObjectCacheIterator<UStaticMeshComponent>  GetStaticMeshComponents(UStaticMesh* InStaticMesh);
	TObjectCacheIterator<UMaterialInterface>    GetMaterialsAffectedByTexture(UTexture* InTexture);
	TObjectCacheIterator<UPrimitiveComponent>   GetPrimitivesAffectedByMaterial(UMaterialInterface* InMaterial);
	TObjectCacheIterator<UPrimitiveComponent>   GetPrimitivesAffectedByMaterials(TArrayView<UMaterialInterface*> InMaterials);
	TObjectCacheIterator<UTexture>              GetUsedTextures(UMaterialInterface* InMaterial);
	TObjectCacheIterator<UMaterialInterface>    GetUsedMaterials(UPrimitiveComponent* InComponent);
	TObjectCacheIterator<UMaterialInterface>    GetMaterialsAffectedByMaterials(TArrayView<UMaterialInterface*> InMaterials);

private:
	friend class FObjectCacheContextScope;
	FObjectCacheContext() = default;
	TMap<TObjectKey<UPrimitiveComponent>, TSet<UMaterialInterface*>> PrimitiveComponentToMaterial;
	TMap<TObjectKey<UMaterialInterface>, TSet<UTexture*>> MaterialUsedTextures;
	TOptional<TMap<TObjectKey<UTexture>, TSet<UMaterialInterface*>>> TextureToMaterials;
	TOptional<TMap<TObjectKey<UMaterialInterface>, TSet<UPrimitiveComponent*>>> MaterialToPrimitives;
	TOptional<TMap<TObjectKey<UStaticMesh>, TSet<UStaticMeshComponent*>>> StaticMeshToComponents;
	TOptional<TMap<TObjectKey<USkinnedAsset>, TSet<USkinnedMeshComponent*>>> SkinnedAssetToComponents;
	TOptional<TArray<USkinnedMeshComponent*>> SkinnedMeshComponents;
	TOptional<TArray<UStaticMeshComponent*>> StaticMeshComponents;
	TOptional<TArray<UPrimitiveComponent*>> PrimitiveComponents;
};

/**
 * A scope that can be used to maintain a FObjectCacheContext active until the scope
 * is destroyed. Should only be used during short periods when there is no new
 * objects created and no object dependency changes. (i.e. Scene update after asset compilation).
 */
class FObjectCacheContextScope
{
public:
	ENGINE_API FObjectCacheContextScope();
	ENGINE_API ~FObjectCacheContextScope();
	ENGINE_API FObjectCacheContext& GetContext();
private:
	// Scopes can be stacked over one another, but only the outermost will
	// own the actual context and destroy it at the end, all inner scopes
	// will feed off the already existing one and will not own it.
	bool bIsOwner = false;
};
