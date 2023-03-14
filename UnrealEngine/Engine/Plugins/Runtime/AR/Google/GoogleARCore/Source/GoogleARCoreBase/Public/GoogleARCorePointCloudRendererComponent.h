// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"

#include "GoogleARCorePointCloudRendererComponent.generated.h"

/**
 * A helper component that renders the latest point cloud from the ARCore tracking session.
 * NOTE: This class is now deprecated, use UPointCloudComponent from the "PointCloud" plugin.
 */
UCLASS(Experimental, ClassGroup = (GoogleARCore), Deprecated)
class GOOGLEARCOREBASE_API UDEPRECATED_GoogleARCorePointCloudRendererComponent : public USceneComponent
{
	GENERATED_BODY()
public:
	/** The color of the point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GoogleARCore|PointCloudRenderer")
	FColor PointColor;

	/** The size of the point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GoogleARCore|PointCloudRenderer")
	float PointSize;

	UDEPRECATED_GoogleARCorePointCloudRendererComponent();

	/** Function called on every frame on this Component. */
	void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction * ThisTickFunction) override;

private:
	TArray<FVector> PointCloudInWorldSpace;
	double PreviousPointCloudTimestamp;

	void DrawPointCloud();
};
