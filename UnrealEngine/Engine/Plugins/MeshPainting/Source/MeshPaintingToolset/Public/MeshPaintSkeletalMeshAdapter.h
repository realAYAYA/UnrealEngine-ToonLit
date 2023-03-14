// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RawIndexBuffer.h"
#include "Components/SkeletalMeshComponent.h"
#include "BaseMeshPaintComponentAdapter.h"
#include "MeshPaintComponentAdapterFactory.h"

class UBodySetup;
class USkeletalMesh;
class USkeletalMeshComponent;
class UTexture;
class FSkeletalMeshRenderData;
class FSkeletalMeshLODRenderData;
class FSkeletalMeshLODModel;
class UMeshComponent;

//////////////////////////////////////////////////////////////////////////
// FMeshPaintGeometryAdapterForSkeletalMeshes
class MESHPAINTINGTOOLSET_API FMeshPaintSkeletalMeshComponentAdapter : public FBaseMeshPaintComponentAdapter
{
public:
	static void InitializeAdapterGlobals();
	static void AddReferencedObjectsGlobals(FReferenceCollector& Collector);
	static void CleanupGlobals();
	/** Start IMeshPaintGeometryAdapter Overrides */
	virtual bool Construct(UMeshComponent* InComponent, int32 InMeshLODIndex) override;
	virtual ~FMeshPaintSkeletalMeshComponentAdapter();
	virtual bool Initialize() override;
	virtual void OnAdded() override;
	virtual void OnRemoved() override;
	virtual bool IsValid() const override { return SkeletalMeshComponent.IsValid() && ReferencedSkeletalMesh && SkeletalMeshComponent->GetSkeletalMeshAsset() == ReferencedSkeletalMesh; }
	virtual bool SupportsTexturePaint() const override { return true; }
	virtual bool SupportsVertexPaint() const override { return SkeletalMeshComponent.IsValid(); }
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
	/** Callback for when the skeletal mesh on the component is changed */
	void OnSkeletalMeshChanged();
	/** Callback for when skeletal mesh DDC data is rebuild */
	void OnPostMeshCached(USkeletalMesh* SkeletalMesh);

	/** Delegate called when skeletal mesh is changed on the component */
	FDelegateHandle SkeletalMeshChangedHandle;	

	/** Skeletal mesh component represented by this adapter */
	TWeakObjectPtr<USkeletalMeshComponent> SkeletalMeshComponent;
	/** Skeletal mesh currently set to the Skeletal Mesh Component */
	USkeletalMesh* ReferencedSkeletalMesh;
	/** Skeletal Mesh resource retrieved from the Skeletal Mesh */
	FSkeletalMeshRenderData* MeshResource;

	/** LOD render data (at Mesh LOD Index) containing data to change */
	FSkeletalMeshLODRenderData* LODData;
	/** LOD model (source) data (at Mesh LOD Index) containing data to change */
	FSkeletalMeshLODModel* LODModel;
	/** LOD Index for which data has to be retrieved / altered*/
	int32 MeshLODIndex;
};

//////////////////////////////////////////////////////////////////////////
// FMeshPaintGeometryAdapterForSkeletalMeshesFactory

class MESHPAINTINGTOOLSET_API FMeshPaintSkeletalMeshComponentAdapterFactory : public IMeshPaintComponentAdapterFactory
{
public:
	virtual TSharedPtr<IMeshPaintComponentAdapter> Construct(UMeshComponent* InComponent, int32 InMeshLODIndex) const override;
	virtual void InitializeAdapterGlobals() override { FMeshPaintSkeletalMeshComponentAdapter::InitializeAdapterGlobals(); }
	virtual void AddReferencedObjectsGlobals(FReferenceCollector& Collector) override { FMeshPaintSkeletalMeshComponentAdapter::AddReferencedObjectsGlobals(Collector); }
	virtual void CleanupGlobals() override { FMeshPaintSkeletalMeshComponentAdapter::CleanupGlobals(); }
};
