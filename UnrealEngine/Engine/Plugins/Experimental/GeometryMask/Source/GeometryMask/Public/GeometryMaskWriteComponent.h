// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "GeometryMaskTypes.h"
#include "IGeometryMaskWriteInterface.h"

#include "GeometryMaskWriteComponent.generated.h"

class UCanvasRenderTarget2D;
class UDynamicMeshComponent;
class UStaticMeshComponent;

/** Mesh data for a single mesh, in local space. */
USTRUCT()
struct FGeometryMaskBatchElementData
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FVector3f> Vertices;

	UPROPERTY()
	TArray<uint32> Indices;

	UPROPERTY()
	int32 NumTriangles = 0;

	/** Used to check cached state. */
	UPROPERTY()
	uint64 ChangeStamp = 0;

	void Reserve(
		const int32 InNumVertices,
		const int32 InNumIndices,
		const int32 InNumTriangles)
	{
		Vertices.Reserve(InNumVertices);
		Indices.Reserve(InNumIndices);
		NumTriangles = InNumTriangles;
	}
};

UCLASS(BlueprintType, HideCategories=(Activation, Cooking, AssetUserData, Navigation), meta=(BlueprintSpawnableComponent))
class GEOMETRYMASK_API UGeometryMaskWriteMeshComponent
	: public UGeometryMaskCanvasReferenceComponentBase
	, public IGeometryMaskWriteInterface
{
	GENERATED_BODY()

public:
	//~ Begin IGeometryMaskWriteInterface
	virtual void SetParameters(FGeometryMaskWriteParameters& InParameters) override;
	virtual const FGeometryMaskWriteParameters& GetParameters() const override
	{
		return Parameters;
	}
	virtual void DrawToCanvas(FCanvas* InCanvas) override;
	virtual FOnGeometryMaskSetCanvasNativeDelegate& OnSetCanvas() override
	{
		return OnSetCanvasDelegate;
	}
	//~ End IGeometryMaskWriteInterface

protected:
	//~ Begin UActorComponent
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;
	//~ End UActorComponent
	
	//~ Begin UGeometryMaskCanvasReferenceComponentBase
    virtual bool TryResolveCanvas() override;
	virtual bool Cleanup() override;
    //~ End UGeometryMaskCanvasReferenceComponentBase

	// Resets cached data, triggers rebuild
	void ResetCachedData();
	
	void UpdateCachedData();
	void UpdateCachedStaticMeshData(TConstArrayView<UPrimitiveComponent*> InPrimitiveComponents);
	void UpdateCachedDynamicMeshData(TConstArrayView<UPrimitiveComponent*> InPrimitiveComponents);

#if WITH_EDITOR
	void OnStaticMeshChanged(UStaticMeshComponent* InStaticMeshComponent);
#endif
	void OnDynamicMeshChanged(UDynamicMeshComponent* InDynamicMeshComponent);
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mask", meta=(ShowOnlyInnerProperties))
	FGeometryMaskWriteParameters Parameters;

	/** Write to the mask even when this or it's owner is hidden. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mask")
	bool bWriteWhenHidden = true;

	UPROPERTY(Transient, DuplicateTransient)
	int32 LastPrimitiveComponentCount;

	UPROPERTY(DuplicateTransient)
	TMap<FName, TWeakObjectPtr<USceneComponent>> CachedComponentsWeak;
	
	UPROPERTY(DuplicateTransient)
	TMap<FName, FGeometryMaskBatchElementData> CachedMeshData;

	/** To protect async-unsafe calls. */
	FCriticalSection CanvasCS;
};
