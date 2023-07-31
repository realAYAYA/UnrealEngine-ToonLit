// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/MeshComponent.h"
#include "PointCloudComponent.generated.h"

class FPrimitiveSceneProxy;

/**
 * Component for rendering a point cloud
 */
UCLASS(hidecategories = (Object, LOD), meta = (BlueprintSpawnableComponent), ClassGroup = PointCloud)
class POINTCLOUD_API UPointCloudComponent :
	public UMeshComponent
{
	GENERATED_UCLASS_BODY()

	/**	If true, each tick the component will render its point cloud */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Point Cloud")
	bool bIsVisible;
	
	/**	If > 0, will automatically update the point cloud data from AR system based on this interval (sec) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Point Cloud")
	float PointCloudUpdateInterval = 0.f;
	
	/** Changes the visibility setting */
	UFUNCTION(BlueprintCallable, Category = "Point Cloud")
	void SetIsVisible(bool bNewVisibility);

	/** Point cloud data that will be used for rendering, assumes each point is in world space */
	UPROPERTY(BlueprintReadOnly, Category = "Point Cloud")
	TArray<FVector> PointCloud;

	/** Point cloud color data that will be used for rendering */
	UPROPERTY(BlueprintReadOnly, Category = "Point Cloud")
	TArray<FColor> PointColors;

	/**
	 * Updates the point cloud data with the new set of points
	 *
	 * @param Points the new set of points to use
	 */
	UFUNCTION(BlueprintCallable, Category = "Point Cloud")
	void SetPointCloud(const TArray<FVector>& Points);

	/**
	 * Updates the point cloud data with the new set of points and colors
	 *
	 * @param Points the new set of points to use
	 * @param Colors the new set of colors to use, must match points size
	 */
	UFUNCTION(BlueprintCallable, Category = "Point Cloud")
	void SetPointCloudWithColors(const TArray<FVector>& Points, const TArray<FColor>& Colors);

	/**
	 * Empties the point cloud
	 */
	UFUNCTION(BlueprintCallable, Category = "Point Cloud")
	void ClearPointCloud();
	
	/** The color to render the points with */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Point Cloud")
	FLinearColor PointColor = FLinearColor::White;

	/** Allows you to change the color of the points being rendered */
	UFUNCTION(BlueprintCallable, Category = "Point Cloud")
	void SetPointColor(const FLinearColor& Color);

	/** The size of the point when rendering */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Point Cloud")
	float PointSize = 5.f;
	
	/** Allows you to change the size of the points being rendered */
	UFUNCTION(BlueprintCallable, Category = "Point Cloud")
	void SetPointSize(float Size);

	/** Determines which points are within the box and returns those to the caller */
	UFUNCTION(BlueprintPure, Category = "Point Cloud")
	TArray<FVector> GetPointsInBox(const FBox& WorldSpaceBox) const;

	/** Determines which points are outside the box and returns those to the caller */
	UFUNCTION(BlueprintPure, Category = "Point Cloud")
	TArray<FVector> GetPointsOutsideBox(const FBox& WorldSpaceBox) const;

	/** The material to render with */
	UPROPERTY()
	TObjectPtr<UMaterialInterface> PointCloudMaterial;
	
	/** An optional sprite texture to render the point with */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Point Cloud")
	TObjectPtr<UTexture> SpriteTexture;
	
private:
	virtual void BeginPlay() override;
	virtual FMatrix GetRenderMatrix() const override;
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual int32 GetNumMaterials() const override { return 1; }
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials = false) const override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	
	void UpdatePointCloud();

	/** World space bounds of the point cloud */
	UPROPERTY()
	FBoxSphereBounds WorldBounds;

	/** Used to know whether to update the point cloud or not */
	float LastUpdateTimestamp;
};


