// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/ObjectMacros.h"
#include "Components/MeshComponent.h"
#include "OculusMR_PlaneMeshComponent.generated.h"

class FPrimitiveSceneProxy;
class UTextureRenderTarget2D;

USTRUCT(BlueprintType)
struct FOculusMR_PlaneMeshTriangle
{
    GENERATED_USTRUCT_BODY()

    UPROPERTY()
    FVector Vertex0 = FVector(0.0f);

    UPROPERTY()
    FVector2D UV0 = FVector2D(0.0f);

    UPROPERTY()
    FVector Vertex1 = FVector(0.0f);

    UPROPERTY()
    FVector2D UV1 = FVector2D(0.0f);

    UPROPERTY()
    FVector Vertex2 = FVector(0.0f);

    UPROPERTY()
    FVector2D UV2 = FVector2D(0.0f);
};

/** Component that allows you to specify custom triangle mesh geometry */
UCLASS(hidecategories = (Object, LOD, Physics, Collision), editinlinenew, ClassGroup = Rendering, NotPlaceable, NotBlueprintable)
class UOculusMR_PlaneMeshComponent: public UMeshComponent
{
    GENERATED_UCLASS_BODY()

    /** Set the geometry to use on this triangle mesh */
    UFUNCTION(BlueprintCallable, Category = "Components|CustomMesh")
    bool SetCustomMeshTriangles(const TArray<FOculusMR_PlaneMeshTriangle>& Triangles);

    /** Add to the geometry to use on this triangle mesh.  This may cause an allocation.  Use SetCustomMeshTriangles() instead when possible to reduce allocations. */
    UFUNCTION(BlueprintCallable, Category = "Components|CustomMesh")
    void AddCustomMeshTriangles(const TArray<FOculusMR_PlaneMeshTriangle>& Triangles);

    /** Removes all geometry from this triangle mesh.  Does not deallocate memory, allowing new geometry to reuse the existing allocation. */
    UFUNCTION(BlueprintCallable, Category = "Components|CustomMesh")
    void ClearCustomMeshTriangles();

    void Place(const FVector& Center, const FVector& Up, const FVector& Normal, const FVector2D& Size);

	void SetPlaneRenderTarget(UTextureRenderTarget2D* RT) { PlaneRenderTarget = RT; }

private:

    //~ Begin UPrimitiveComponent Interface.
    virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
    //~ End UPrimitiveComponent Interface.

    //~ Begin UMeshComponent Interface.
    virtual int32 GetNumMaterials() const override;
    //~ End UMeshComponent Interface.

    //~ Begin USceneComponent Interface.
    virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
    //~ Begin USceneComponent Interface.

    TArray<FOculusMR_PlaneMeshTriangle> CustomMeshTris;

	UTextureRenderTarget2D* PlaneRenderTarget;

    friend class FOculusMR_PlaneMeshSceneProxy;
};


