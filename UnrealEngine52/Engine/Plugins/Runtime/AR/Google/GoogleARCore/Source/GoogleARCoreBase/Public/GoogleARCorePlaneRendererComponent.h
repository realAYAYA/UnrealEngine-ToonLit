// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Components/SceneComponent.h"

#include "GoogleARCorePlaneRendererComponent.generated.h"

/**
 * A helper component that renders all the ARCore planes in the current tracking session.
 * NOTE: This class is now deprecated, plane visualization is done through UARPlaneComponent.
 */
UCLASS(Experimental, ClassGroup = (GoogleARCore), Deprecated)
class GOOGLEARCOREBASE_API UDEPRECATED_GoogleARCorePlaneRendererComponent : public USceneComponent
{
	GENERATED_BODY()
public:

	/** Render the plane quad when set to true. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GoogleARCore|TrackablePlaneRenderer")
	bool bRenderPlane;

	/** Render the plane boundary polygon lines when set to true. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GoogleARCore|TrackablePlaneRenderer")
	bool bRenderBoundaryPolygon;

	/** The color of the plane. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GoogleARCore|TrackablePlaneRenderer")
	FColor PlaneColor;

	/** The color of the boundary polygon. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GoogleARCore|TrackablePlaneRenderer")
	FColor BoundaryPolygonColor;

	/** The line thickness for the plan boundary polygon. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GoogleARCore|TrackablePlaneRenderer")
	float BoundaryPolygonThickness;

	UDEPRECATED_GoogleARCorePlaneRendererComponent();

	/** Function called every frame on this Component. */
	void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction * ThisTickFunction) override;

private:
	TArray<int> PlaneIndices;
	TArray<FVector> PlaneVertices;

	void DrawPlanes();
};
