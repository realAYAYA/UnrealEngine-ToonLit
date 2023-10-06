// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnamorphicLensDistortionModelHandler.h"

#include "CameraCalibrationSettings.h"
#include "Engine/TextureRenderTarget2D.h"

void UAnamorphicLensDistortionModelHandler::InitializeHandler()
{
	LensModelClass = UAnamorphicLensModel::StaticClass();
}

FVector2D UAnamorphicLensDistortionModelHandler::ComputeDistortedUV(const FVector2D& InUndistortedUV) const
{
	// The following implementation is based on the 3DE4 Anamorphic - Standard, Degree 4 model for distortion

	// Pre-compute some values that will be useful later
	const FVector2D FilmbackSize = FVector2D(CameraFilmback.SensorWidth * AnamorphicParameters.PixelAspect, CameraFilmback.SensorHeight);

	// Guard against divide-by-zero errors
	if (FMath::IsNearlyZero(FilmbackSize.X) || FMath::IsNearlyZero(FilmbackSize.Y) || FMath::IsNearlyZero(AnamorphicParameters.PixelAspect))
	{
		return InUndistortedUV;
	}

	const double FilmbackRadius = 0.5f * FMath::Sqrt((FilmbackSize.X * FilmbackSize.X) + (FilmbackSize.Y * FilmbackSize.Y));

	const double MountAngleRadians = FMath::DegreesToRadians(AnamorphicParameters.LensRotation);
	const double CosMountAngle = FMath::Cos(MountAngleRadians);
	const double SinMountAngle = FMath::Sin(MountAngleRadians);

	// Step 1: Transform input UVs into diagonally-normalized coordinates, based on the width and height of the filmback/image
	FVector2D DNCoordinates = (InUndistortedUV - 0.5f) * (FilmbackSize / FilmbackRadius);

	// Step 2: Scale the x-coordinate by the inverse of the pixel aspect ratio
	DNCoordinates.X = DNCoordinates.X / AnamorphicParameters.PixelAspect;

	// Step 3: Rotate x and y by the inverse of the mounting angle
	DNCoordinates.X = (DNCoordinates.X * CosMountAngle) + (DNCoordinates.Y * SinMountAngle);
	DNCoordinates.Y = (DNCoordinates.Y * CosMountAngle) - (DNCoordinates.X * SinMountAngle);

	// Step 4: Compute the anamorphic distortion for x and y, to the 4th degree
	const double DNRadiusSq = (DNCoordinates.X * DNCoordinates.X) + (DNCoordinates.Y * DNCoordinates.Y);
	const double DNRadius4th = DNRadiusSq * DNRadiusSq;
	const double Phi = FMath::Atan2(DNCoordinates.Y, DNCoordinates.X);

	FVector2D DNCoordinatesDistorted;
	DNCoordinatesDistorted.X = DNCoordinates.X * (1 + (AnamorphicParameters.CX02 * DNRadiusSq) + (AnamorphicParameters.CX04 * DNRadius4th) + (AnamorphicParameters.CX22 * DNRadiusSq * FMath::Cos(2 * Phi)) + (AnamorphicParameters.CX24 * DNRadius4th * FMath::Cos(2 * Phi)) + (AnamorphicParameters.CX44 * DNRadius4th * FMath::Cos(4 * Phi)));
	DNCoordinatesDistorted.Y = DNCoordinates.Y * (1 + (AnamorphicParameters.CY02 * DNRadiusSq) + (AnamorphicParameters.CY04 * DNRadius4th) + (AnamorphicParameters.CY22 * DNRadiusSq * FMath::Cos(2 * Phi)) + (AnamorphicParameters.CY24 * DNRadius4th * FMath::Cos(2 * Phi)) + (AnamorphicParameters.CY44 * DNRadius4th * FMath::Cos(4 * Phi)));

	// Step 5: Scale the x-coordinate by the pixel aspect ratio, and the x- and y-coordinates by the lens-breathing squeeze factors
	DNCoordinatesDistorted.X = DNCoordinatesDistorted.X * AnamorphicParameters.PixelAspect * AnamorphicParameters.SqueezeX;
	DNCoordinatesDistorted.Y = DNCoordinatesDistorted.Y * AnamorphicParameters.SqueezeY;

	// Step 6: Rotate x and y by the mounting angle
	DNCoordinates.X = (DNCoordinates.X * CosMountAngle) - (DNCoordinates.Y * SinMountAngle);
	DNCoordinates.Y = (DNCoordinates.Y * CosMountAngle) + (DNCoordinates.X * SinMountAngle);

	// Step 7: Transform diagonally normalized coordinates back into unit coordinates
	const FVector2D DistortedUV = ((DNCoordinatesDistorted * FilmbackRadius) / FilmbackSize) + 0.5f;

	return DistortedUV;
}

void UAnamorphicLensDistortionModelHandler::InitDistortionMaterials()
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

void UAnamorphicLensDistortionModelHandler::UpdateMaterialParameters()
{
	//Helper function to set material parameters of an MID
	const auto SetDistortionMaterialParameters = [this](UMaterialInstanceDynamic* const MID)
	{
		MID->SetScalarParameterValue("pixel_aspect", AnamorphicParameters.PixelAspect);

 		MID->SetScalarParameterValue("w_fb", CameraFilmback.SensorWidth * AnamorphicParameters.PixelAspect);
 		MID->SetScalarParameterValue("h_fb", CameraFilmback.SensorHeight);

		MID->SetScalarParameterValue("cx02", AnamorphicParameters.CX02);
		MID->SetScalarParameterValue("cx04", AnamorphicParameters.CX04);
		MID->SetScalarParameterValue("cx22", AnamorphicParameters.CX22);
		MID->SetScalarParameterValue("cx24", AnamorphicParameters.CX24);
		MID->SetScalarParameterValue("cx44", AnamorphicParameters.CX44);

		MID->SetScalarParameterValue("cy02", AnamorphicParameters.CY02);
		MID->SetScalarParameterValue("cy04", AnamorphicParameters.CY04);
		MID->SetScalarParameterValue("cy22", AnamorphicParameters.CY22);
		MID->SetScalarParameterValue("cy24", AnamorphicParameters.CY24);
		MID->SetScalarParameterValue("cy44", AnamorphicParameters.CY44);

		MID->SetScalarParameterValue("sx", AnamorphicParameters.SqueezeX);
		MID->SetScalarParameterValue("sy", AnamorphicParameters.SqueezeY);

		MID->SetScalarParameterValue("phi_mnt", AnamorphicParameters.LensRotation);
	};

	SetDistortionMaterialParameters(UndistortionDisplacementMapMID);
	SetDistortionMaterialParameters(DistortionDisplacementMapMID);
}

void UAnamorphicLensDistortionModelHandler::InterpretDistortionParameters()
{
	LensModelClass->GetDefaultObject<ULensModel>()->FromArray<FAnamorphicDistortionParameters>(CurrentState.DistortionInfo.Parameters, AnamorphicParameters);
}
