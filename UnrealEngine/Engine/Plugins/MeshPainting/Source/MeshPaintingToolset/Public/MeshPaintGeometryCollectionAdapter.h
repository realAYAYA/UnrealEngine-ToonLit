// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BaseMeshPaintComponentAdapter.h"
#include "MeshPaintComponentAdapterFactory.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "UObject/WeakObjectPtr.h"

class UBodySetup;
class UGeometryCollection;
class UGeometryCollectionComponent;
class UTexture;
class UGeometryCollection;
class UMeshComponent;

//////////////////////////////////////////////////////////////////////////
// FMeshPaintGeometryAdapterForSkeletalMeshes
class MESHPAINTINGTOOLSET_API FMeshPaintGeometryCollectionComponentAdapter : public FBaseMeshPaintComponentAdapter
{
public:
	static void InitializeAdapterGlobals();
	static void AddReferencedObjectsGlobals(FReferenceCollector& Collector);
	static void CleanupGlobals();
	/** Start IMeshPaintGeometryAdapter Overrides */
	virtual bool Construct(UMeshComponent* InComponent, int32 InMeshLODIndex) override;
	virtual ~FMeshPaintGeometryCollectionComponentAdapter();
	virtual bool Initialize() override;
	virtual void OnAdded() override;
	virtual void OnRemoved() override;
	virtual bool IsValid() const override;
	virtual bool SupportsTexturePaint() const override { return IsValid(); }
	virtual bool SupportsVertexPaint() const override { return IsValid(); }
	virtual bool LineTraceComponent(struct FHitResult& OutHit, const FVector Start, const FVector End, const struct FCollisionQueryParams& Params) const override;
	virtual void QueryPaintableTextures(int32 MaterialIndex, int32& OutDefaultIndex, TArray<struct FPaintableTexture>& InOutTextureList) override;
	virtual void ApplyOrRemoveTextureOverride(UTexture* SourceTexture, UTexture* OverrideTexture) const override;
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual void PreEdit() override;
	virtual void PostEdit() override;
	virtual void GetTextureCoordinate(int32 VertexIndex, int32 ChannelIndex, FVector2D& OutTextureCoordinate) const override;
	virtual void GetVertexColor(int32 VertexIndex, FColor& OutColor, bool bInstance = true) const override;
	virtual void SetVertexColor(int32 VertexIndex, FColor Color, bool bInstance = true) override;
	virtual FMatrix GetComponentToWorldMatrix() const override;
	/** End IMeshPaintGeometryAdapter Overrides */
		
	/** Start FBaseMeshPaintGeometryAdapter Overrides */
	virtual bool InitializeVertexData();
	/** End FBaseMeshPaintGeometryAdapter Overrides */
protected:

	/** Delegate called when geometry collection is changed on the component */
	FDelegateHandle GeometryCollectionChangedHandle;

	/** Geometry Collection component represented by this adapter */
	TWeakObjectPtr<UGeometryCollectionComponent> GeometryCollectionComponent;

	bool bSavedShowBoneColors = false;

	/// Get the underlying UGeometryCollection from the component, as a non-const object
	/// Caller must have already validated that the component weak pointer is still valid (as this is called per-vertex)
	UGeometryCollection* GetGeometryCollectionObject() const;

	void OnGeometryCollectionChanged();


	// Like IsValid() but does not verify that the cached data matches
	bool HasValidGeometryCollection() const;
	
	// TODO: Store a LOD index if/when GeometryCollection supports LODs
};

//////////////////////////////////////////////////////////////////////////
// FMeshPaintGeometryAdapterForSkeletalMeshesFactory

class MESHPAINTINGTOOLSET_API FMeshPaintGeometryCollectionComponentAdapterFactory : public IMeshPaintComponentAdapterFactory
{
public:
	virtual TSharedPtr<IMeshPaintComponentAdapter> Construct(UMeshComponent* InComponent, int32 InMeshLODIndex) const override;
	virtual void InitializeAdapterGlobals() override { FMeshPaintGeometryCollectionComponentAdapter::InitializeAdapterGlobals(); }
	virtual void AddReferencedObjectsGlobals(FReferenceCollector& Collector) override { FMeshPaintGeometryCollectionComponentAdapter::AddReferencedObjectsGlobals(Collector); }
	virtual void CleanupGlobals() override { FMeshPaintGeometryCollectionComponentAdapter::CleanupGlobals(); }
};
