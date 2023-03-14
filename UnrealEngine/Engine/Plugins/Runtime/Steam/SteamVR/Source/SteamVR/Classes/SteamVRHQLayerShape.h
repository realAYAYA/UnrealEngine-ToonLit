// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Components/StereoLayerComponent.h"
#include "SteamVRHQLayerShape.generated.h"

UCLASS(meta = (DisplayName = "High Quality Layer (SteamVR)"))
class STEAMVR_API USteamVRHQStereoLayerShape : public UStereoLayerShapeQuad
{
	GENERATED_BODY()
public:
	virtual void ApplyShape(IStereoLayers::FLayerDesc& LayerDesc) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, export, Category = "HQ Layer Properties")
	bool bCurved;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, export, Category = "HQ Layer Properties")
	bool bAntiAlias;
	
	/** For curved layers, sets the minimum distance from the layer used to automatically curve
      * the surface around the viewer.  The minimum distance is when the layer is most curved. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, export, Category = "HQ Layer Properties")
	float AutoCurveMinDistance;
	
	/** For curved layers, sets the maximum distance from the layer used to automatically curve
	  * the surface around the viewer.  The maximum distance is when the layer is the least curved. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, export, Category = "HQ Layer Properties")
	float AutoCurveMaxDistance;

	UFUNCTION(BlueprintCallable, Category = "Components|Stereo Layer")
	void SetCurved(bool InCurved);
	UFUNCTION(BlueprintCallable, Category = "Components|Stereo Layer")
	void SetAntiAlias(bool InAntiAlias);
	UFUNCTION(BlueprintCallable, Category = "Components|Stereo Layer")
	void SetAutoCurveMinDistance(float InDistance);
	UFUNCTION(BlueprintCallable, Category = "Components|Stereo Layer")
	void SetAutoCurveMaxDistance(float InDistance);
};