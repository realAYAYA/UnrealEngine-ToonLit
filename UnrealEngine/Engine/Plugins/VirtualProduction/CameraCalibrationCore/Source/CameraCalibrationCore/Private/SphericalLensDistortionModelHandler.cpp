// Copyright Epic Games, Inc. All Rights Reserved.

#include "SphericalLensDistortionModelHandler.h"

#include "Engine/TextureRenderTarget2D.h"
#include "CameraCalibrationSettings.h"
#include "Math/NumericLimits.h"

void USphericalLensDistortionModelHandler::InitializeHandler()
{
	LensModelClass = USphericalLensModel::StaticClass();
}

FVector2D USphericalLensDistortionModelHandler::ComputeDistortedUV(const FVector2D& InUndistortedUV) const
{
	// These distances cannot be zero in real-life. If they are, the current distortion state must be bad
	if ((CurrentState.FocalLengthInfo.FxFy.X == 0.0f) || (CurrentState.FocalLengthInfo.FxFy.Y == 0.0f))
	{
		return InUndistortedUV;
	}

	FVector2D NormalizedDistanceFromImageCenter = (InUndistortedUV - CurrentState.ImageCenter.PrincipalPoint) / CurrentState.FocalLengthInfo.FxFy;
	const FVector2D OriginalDistance = NormalizedDistanceFromImageCenter;

	// Iterative approach to distort an undistorted UV using coefficients that were designed to undistort
	for (int32 Index = 0; Index < 2; ++Index)
	{
		const FVector2D DistanceSquared = NormalizedDistanceFromImageCenter * NormalizedDistanceFromImageCenter;
		const float RSquared = DistanceSquared.X + DistanceSquared.Y;

		const float RadialDistortion = 1.0f + (SphericalParameters.K1 * RSquared) + (SphericalParameters.K2 * RSquared * RSquared) + (SphericalParameters.K3 * RSquared * RSquared * RSquared);

		const FVector2D TangentialDistortion = 
		{
			(SphericalParameters.P2 * (RSquared + (2.0f * DistanceSquared.X))) + (2.0f * SphericalParameters.P1 * NormalizedDistanceFromImageCenter.X * NormalizedDistanceFromImageCenter.Y),
			(SphericalParameters.P1 * (RSquared + (2.0f * DistanceSquared.Y))) + (2.0f * SphericalParameters.P2 * NormalizedDistanceFromImageCenter.X * NormalizedDistanceFromImageCenter.Y)
		};

		// Guard against divide-by-zero errors
		if (RadialDistortion == 0.0f)
		{
			NormalizedDistanceFromImageCenter = FVector2D(TNumericLimits<float>::Max(), TNumericLimits<float>::Max());
			break;
		}
		else
		{
			NormalizedDistanceFromImageCenter = (OriginalDistance - TangentialDistortion) / RadialDistortion;
		}
	}

	const FVector2D DistortedUV = (NormalizedDistanceFromImageCenter * CurrentState.FocalLengthInfo.FxFy) + FVector2D(0.5f, 0.5f);
	return DistortedUV;
}

void USphericalLensDistortionModelHandler::InitDistortionMaterials()
{
	if (DistortionPostProcessMID == nullptr)
	{
		UMaterialInterface* DistortionMaterialParent = GetDefault<UCameraCalibrationSettings>()->GetDefaultDistortionMaterial(this->StaticClass());
		DistortionPostProcessMID = UMaterialInstanceDynamic::Create(DistortionMaterialParent, this);
	}

	if (UndistortionDisplacementMapMID == nullptr)
	{
		UMaterialInterface* MaterialParent = GetDefault<UCameraCalibrationSettings>()->GetDefaultUndistortionDisplacementMaterial(this->StaticClass());
		UndistortionDisplacementMapMID = UMaterialInstanceDynamic::Create(MaterialParent, this);
	}

	if (DistortionDisplacementMapMID == nullptr)
	{
		UMaterialInterface* MaterialParent = GetDefault<UCameraCalibrationSettings>()->GetDefaultDistortionDisplacementMaterial(this->StaticClass());
		DistortionDisplacementMapMID = UMaterialInstanceDynamic::Create(MaterialParent, this);
	}

	DistortionPostProcessMID->SetTextureParameterValue("UndistortionDisplacementMap", UndistortionDisplacementMapRT);
	DistortionPostProcessMID->SetTextureParameterValue("DistortionDisplacementMap", DistortionDisplacementMapRT);

	SetDistortionState(CurrentState);
}

void USphericalLensDistortionModelHandler::UpdateMaterialParameters()
{
	//Helper function to set material parameters of an MID
	const auto SetDistortionMaterialParameters = [this](UMaterialInstanceDynamic* const MID)
	{
		MID->SetScalarParameterValue("k1", SphericalParameters.K1);
		MID->SetScalarParameterValue("k2", SphericalParameters.K2);
		MID->SetScalarParameterValue("k3", SphericalParameters.K3);
		MID->SetScalarParameterValue("p1", SphericalParameters.P1);
		MID->SetScalarParameterValue("p2", SphericalParameters.P2);

		MID->SetScalarParameterValue("cx", CurrentState.ImageCenter.PrincipalPoint.X);
		MID->SetScalarParameterValue("cy", CurrentState.ImageCenter.PrincipalPoint.Y);

		MID->SetScalarParameterValue("fx", CurrentState.FocalLengthInfo.FxFy.X);
		MID->SetScalarParameterValue("fy", CurrentState.FocalLengthInfo.FxFy.Y);
	};

	SetDistortionMaterialParameters(UndistortionDisplacementMapMID);
	SetDistortionMaterialParameters(DistortionDisplacementMapMID);
}

void USphericalLensDistortionModelHandler::InterpretDistortionParameters()
{
	LensModelClass->GetDefaultObject<ULensModel>()->FromArray<FSphericalDistortionParameters>(CurrentState.DistortionInfo.Parameters, SphericalParameters);
}
