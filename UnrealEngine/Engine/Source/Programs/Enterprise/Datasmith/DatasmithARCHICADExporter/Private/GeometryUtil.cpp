// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryUtil.h"

#include "Vector3D.hpp"

BEGIN_NAMESPACE_UE_AC

// Extract the rotation from the matrix and return as a Quat
FQuat FGeometryUtil::GetRotationQuat(const double Matrix[3][4])
{
	double						RotAngle = 0.0;
	Geometry::Vector3< double > RotAxis;
	if (Geometry::IsNearZero(abs(Matrix[0][1] - Matrix[1][0])) &&
		Geometry::IsNearZero(abs(Matrix[0][2] - Matrix[2][0])) &&
		Geometry::IsNearZero(abs(Matrix[1][2] - Matrix[2][1])))
	{
		if (Geometry::IsNearZero(abs(Matrix[0][1] + Matrix[1][0]), 0.1) &&
			Geometry::IsNearZero(abs(Matrix[0][2] + Matrix[2][0]), 0.1) &&
			Geometry::IsNearZero(abs(Matrix[1][2] + Matrix[2][1]), 0.1) &&
			Geometry::IsNearZero(abs(Matrix[0][0] + Matrix[1][1] + Matrix[2][2] - 3), 0.1))
		{
			// no rotation
			RotAngle = 0.0;
		}
		else
		{ // 180 degrees rotation
			RotAngle = PI;
			const double xx = (Matrix[0][0] + 1.0) * 0.5;
			const double yy = (Matrix[1][1] + 1.0) * 0.5;
			const double zz = (Matrix[2][2] + 1.0) * 0.5;
			const double xy = (Matrix[0][1] + Matrix[1][0]) * 0.25;
			const double xz = (Matrix[0][2] + Matrix[2][0]) * 0.25;
			const double yz = (Matrix[1][2] + Matrix[2][1]) * 0.25;

			if ((xx > yy) && (xx > zz))
			{
				if (Geometry::IsNearZero(xx))
				{
					RotAxis = Geometry::Vector3< double >(0.0, 0.7071, 0.7071);
				}
				else
				{
					RotAxis[0] = sqrt(xx);
					RotAxis[1] = xy / RotAxis[0];
					RotAxis[2] = xz / RotAxis[0];
				}
			}
			else if (yy > zz)
			{
				if (Geometry::IsNearZero(yy))
				{
					RotAxis = Geometry::Vector3< double >(0.7071, 0.0, 0.7071);
				}
				else
				{
					RotAxis[1] = sqrt(yy);
					RotAxis[0] = xy / RotAxis[1];
					RotAxis[2] = yz / RotAxis[1];
				}
			}
			else
			{
				if (Geometry::IsNearZero(zz))
				{
					RotAxis = Geometry::Vector3< double >(0.7071, 0.7071, 0.0);
				}
				else
				{
					RotAxis[2] = sqrt(zz);
					RotAxis[0] = xz / RotAxis[2];
					RotAxis[1] = yz / RotAxis[2];
				}
			}
		}
	}
	else
	{
		RotAngle = acos((Matrix[0][0] + Matrix[1][1] + Matrix[2][2] - 1.0) * 0.5);
		RotAxis = Geometry::Vector3< double >(
			(Matrix[2][1] - Matrix[1][2]) / sqrt(sqr(Matrix[2][1] - Matrix[1][2]) + sqr(Matrix[0][2] - Matrix[2][0]) +
												 sqr(Matrix[1][0] - Matrix[0][1])),
			(Matrix[0][2] - Matrix[2][0]) / sqrt(sqr(Matrix[2][1] - Matrix[1][2]) + sqr(Matrix[0][2] - Matrix[2][0]) +
												 sqr(Matrix[1][0] - Matrix[0][1])),
			(Matrix[1][0] - Matrix[0][1]) / sqrt(sqr(Matrix[2][1] - Matrix[1][2]) + sqr(Matrix[0][2] - Matrix[2][0]) +
												 sqr(Matrix[1][0] - Matrix[0][1])));
	}
	RotAxis.NormalizeVector();

	return FQuat(FVector(float(RotAxis.x), float(-RotAxis.y), float(RotAxis.z)), float(RotAngle)).Inverse();
}

// Return the Quat equivalent to the direction vector
FQuat FGeometryUtil::GetRotationQuat(const ModelerAPI::Vector& Direction)
{
	const Geometry::Vector3< double > DefaultDirVec(1.0, 0.0, 0.0);
	Geometry::Vector3< double >		  DirVec(Direction.x, -Direction.y, Direction.z);
	DirVec.NormalizeVector();

	const double distToDirSqr = (DirVec - DefaultDirVec).GetLengthSqr();
	const double RotAngle = acos((2.0 - distToDirSqr) * 0.5); // Rotation angle in radian

	Geometry::Vector3< double > RotAxis = DefaultDirVec ^ DirVec;
	RotAxis.NormalizeVector();

	return FQuat(FVector(float(RotAxis.x), float(RotAxis.y), float(RotAxis.z)), float(RotAngle));
}

// Convert Archicad camera rotation to an Unreal Quat
FQuat FGeometryUtil::GetRotationQuat(const double PitchInDegrees, const double YawInDegrees, const double RollInDegrees)
{
	return FQuat(FRotator(float(-PitchInDegrees), float(180.0 - YawInDegrees), float(-RollInDegrees)));
}

// Extract Archicad translation from the matrix and return an Unreal one (in centimeters)
FVector FGeometryUtil::GetTranslationVector(const double Matrix[3][4])
{
	return FVector(float(Matrix[0][3] * 100.0), -float(Matrix[1][3] * 100.0),
				   float(Matrix[2][3] * 100.0)); // The base unit is centimetre in Unreal
}

// Convert Archicad Vertex to Unreal one (in centimeters)
FVector FGeometryUtil::GetTranslationVector(const ModelerAPI::Vertex PosInMeters)
{
	return FVector(float(PosInMeters.x * 100.0), -float(PosInMeters.y * 100.0), float(PosInMeters.z * 100.0));
}

// Return focal that fit ViewAngle in SensorWidth
float FGeometryUtil::GetCameraFocalLength(const double SensorWidth, const double ViewAngleInDegrees)
{
	return float(SensorWidth / (2.0 * tan(ViewAngleInDegrees * (PI / 180.0) *
										  0.5))); // the sensor width and focal length are in millimetre
}

// Return the distance in 3d (input in meters, result in centimeters)
float FGeometryUtil::GetDistance3D(const double distanceZ, const double Distance2D)
{
	return float(sqrt(distanceZ * distanceZ + Distance2D * Distance2D) * 100.0);
}

// Return the pitch in degrees
double FGeometryUtil::GetPitchAngle(const double CameraZ, const double TargetZ, const double Distance2D)
{
	const double angleSign = (CameraZ < TargetZ) ? -1.0 : 1.0;
	const double distanceZ = TargetZ - CameraZ;
	const double realDistance = sqrt(distanceZ * distanceZ + Distance2D * Distance2D);

	if (Geometry::IsNotNearZero(Distance2D - realDistance))
		return acos(Distance2D / realDistance) * angleSign * (180.0 / PI);
	else
		return 0.0;
}

END_NAMESPACE_UE_AC
