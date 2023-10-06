// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LensDistortionModelHandlerBase.h"

#include "Models/AnamorphicLensModel.h"

#include "AnamorphicLensDistortionModelHandler.generated.h"

/** Lens distortion handler for an Anamorphic lens model that implements the 3DE4 Anamorphic - Standard Degree 4 model */
UCLASS(BlueprintType)
class CAMERACALIBRATIONCORE_API UAnamorphicLensDistortionModelHandler : public ULensDistortionModelHandlerBase
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
	/** Anamorphic lens distortion parameters */
	FAnamorphicDistortionParameters AnamorphicParameters;
};
