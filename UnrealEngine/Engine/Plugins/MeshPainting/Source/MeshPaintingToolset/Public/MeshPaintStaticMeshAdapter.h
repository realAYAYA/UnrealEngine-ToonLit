// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BaseMeshPaintComponentAdapter.h"
#include "RawIndexBuffer.h"
#include "Components/StaticMeshComponent.h"
#include "MeshPaintComponentAdapterFactory.h"

class UBodySetup;
class UStaticMesh;
class UTexture;
struct FStaticMeshLODResources;

//////////////////////////////////////////////////////////////////////////
// FMeshPaintGeometryAdapterForStaticMeshes

class MESHPAINTINGTOOLSET_API FMeshPaintStaticMeshComponentAdapter : public FBaseMeshPaintComponentAdapter
{
public:
	static void InitializeAdapterGlobals();
	static void AddReferencedObjectsGlobals(FReferenceCollector& Collector);
	static void CleanupGlobals();
	/** Start IMeshPaintGeometryAdapter Overrides */
	virtual bool Construct(UMeshComponent* InComponent, int32 InMeshLODIndex) override;
	virtual ~FMeshPaintStaticMeshComponentAdapter();
	virtual bool Initialize() override;
	virtual void OnAdded() override {}
	virtual void OnRemoved() override {}
	virtual bool IsValid() const override { return StaticMeshComponent.IsValid() && ReferencedStaticMesh && StaticMeshComponent->GetStaticMesh() == ReferencedStaticMesh; }
	virtual bool SupportsTexturePaint() const override { return true; }
	virtual bool SupportsVertexPaint() const override { return StaticMeshComponent.IsValid() && !StaticMeshComponent->bDisallowMeshPaintPerInstance; }
	virtual bool LineTraceComponent(struct FHitResult& OutHit, const FVector Start, const FVector End, const struct FCollisionQueryParams& Params) const override;	
	virtual void QueryPaintableTextures(int32 MaterialIndex, int32& OutDefaultIndex, TArray<struct FPaintableTexture>& InOutTextureList) override;
	virtual void ApplyOrRemoveTextureOverride(UTexture* SourceTexture, UTexture* OverrideTexture) const override;
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual void GetTextureCoordinate(int32 VertexIndex, int32 ChannelIndex, FVector2D& OutTextureCoordinate) const override;
	virtual void GetVertexColor(int32 VertexIndex, FColor& OutColor, bool bInstance = true) const override;
	virtual void SetVertexColor(int32 VertexIndex, FColor Color, bool bInstance = true) override;
	virtual FMatrix GetComponentToWorldMatrix() const override;
	virtual void PreEdit() override;
	virtual void PostEdit() override;
	/** End IMeshPaintGeometryAdapter Overrides*/

	/** Begin FMeshBasePaintGeometryAdapter */
	virtual bool InitializeVertexData() override;	
	/** End FMeshBasePaintGeometryAdapter */

protected:
	/** Callback for when the static mesh data is rebuilt */
	void OnPostMeshBuild(UStaticMesh* StaticMesh);
	/** Callback for when the static mesh on the component is changed */
	void OnStaticMeshChanged(UStaticMeshComponent* StaticMeshComponent);

	/** Static mesh component represented by this adapter */
	TWeakObjectPtr<UStaticMeshComponent> StaticMeshComponent;
	/** Static mesh currently set to the Static Mesh Component */
	UStaticMesh* ReferencedStaticMesh;
	/** LOD model (at Mesh LOD Index) containing data to change */
	FStaticMeshLODResources* LODModel;
	/** LOD Index for which data has to be retrieved / altered*/
	int32 MeshLODIndex;
};

//////////////////////////////////////////////////////////////////////////
// FMeshPaintGeometryAdapterForStaticMeshesFactory

class MESHPAINTINGTOOLSET_API FMeshPaintStaticMeshComponentAdapterFactory : public IMeshPaintComponentAdapterFactory
{
public:
	virtual TSharedPtr<IMeshPaintComponentAdapter> Construct(class UMeshComponent* InComponent, int32 InMeshLODIndex) const override;
	virtual void InitializeAdapterGlobals() override { FMeshPaintStaticMeshComponentAdapter::InitializeAdapterGlobals(); }
	virtual void AddReferencedObjectsGlobals(FReferenceCollector& Collector) override { FMeshPaintStaticMeshComponentAdapter::AddReferencedObjectsGlobals(Collector); }
	virtual void CleanupGlobals() override { FMeshPaintStaticMeshComponentAdapter::CleanupGlobals(); }
};
