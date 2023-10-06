// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Utils/AddonTools.h"

#include "Quat.h"
#include "ModelVector.hpp"
#include "Vertex.hpp"

BEGIN_NAMESPACE_UE_AC

// Tool geometry class
class FGeometryUtil
{
  public:
	// This class is only a set of static functions
	FGeometryUtil() = delete;

	// Extract the rotation from the matrix and return as a Quat
	static FQuat GetRotationQuat(const double Matrix[3][4]);

	// Return the Quat equivalent to the direction vector
	static FQuat GetRotationQuat(const ModelerAPI::Vector& Direction);

	// Convert Archicad camera rotation to an Unreal Quat
	static FQuat GetRotationQuat(const double PitchInDegrees, const double YawInDegrees, const double RollInDegrees);

	// Extract Archicad translation from the matrix and return an Unreal one (in centimeters)
	static FVector GetTranslationVector(const double Matrix[3][4]);

	// Convert Archicad Vertex to Unreal one
	static FVector GetTranslationVector(const ModelerAPI::Vertex Pos);

	// Return focal that fit ViewAngle in SensorWidth
	static float GetCameraFocalLength(const double SensorWidth, const double ViewAngle);

	// Return the distance in 3d (input in meters, result in centimeters)
	static float GetDistance3D(const double DistanceZ, const double Distance2D);

	// Return the pitch in degrees
	static double GetPitchAngle(const double CameraZ, const double TargetZ, const double Distance2D);

	// Return clamped value
	static float Clamp(float InValue, float Min, float Max)
	{
		return InValue < Min ? Min : (InValue > Max ? Max : InValue);
	}
};

END_NAMESPACE_UE_AC
