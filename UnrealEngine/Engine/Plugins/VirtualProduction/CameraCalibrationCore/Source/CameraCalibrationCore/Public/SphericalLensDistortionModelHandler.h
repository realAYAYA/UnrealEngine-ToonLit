// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LensDistortionModelHandlerBase.h"

#include "Models/SphericalLensModel.h"

#include "SphericalLensDistortionModelHandler.generated.h"

/** Lens distortion handler for a spherical lens model that implements the Brown-Conrady polynomial model */
UCLASS(BlueprintType)
class CAMERACALIBRATIONCORE_API USphericalLensDistortionModelHandler : public ULensDistortionModelHandlerBase
{
	GENERATED_BODY()

protected:
	//~ Begin ULensDistortionModelHandlerBase interface
	virtual void InitializeHandler() override;
	virtual FVector2D ComputeDistortedUV(const FVector2D& InScreenUV) const override;
	virtual void InitDistortionMaterials() override;
	virtual void UpdateMaterialParameters() override;
	virtual void InterpretDistortionParameters() override;
	//~ End ULensDistortionModelHandlerBase interface

private:
	/** Spherical lens distortion parameters (k1, k2, k3, p1, p2) */
	FSphericalDistortionParameters SphericalParameters;
};
