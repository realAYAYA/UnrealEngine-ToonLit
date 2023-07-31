// Copyright Epic Games, Inc. All Rights Reserved.

#include "DatasmithHashTools.h"

#include "DatasmithMesh.h"
#include "Version.h"

BEGIN_NAMESPACE_UE_AC

FMD5Hash FDatasmithHashTools::GetHashValue()
{
	FMD5Hash HashValue;
	HashValue.Set(MD5);

	return HashValue;
}

// Recommended for values that come from input like coordinates, colors
float		FDatasmithHashTools::FixedPointTolerance = 1 / 0.01f;
const float FDatasmithHashTools::MaxInvFixPointTolerance = FLOAT_NON_FRACTIONAL;

// Hash float with a fixed tolerance value.
void FDatasmithHashTools::HashFixedPointFloatTolerance(double InValue, float InvTolerance)
{
	if (InvTolerance < MaxInvFixPointTolerance)
	{
		float Rounded = std::floorf((float)InValue * InvTolerance + 0.5f);
		if (Rounded == -0.0f)
		{
			Rounded = 0.0f; // We want to confond negative near zero to positive near zero
		}
		TUpdate(Rounded);
	}
	else
	{
		// No tolerance -> we take the hash value itself
		TUpdate((float)InValue);
	}
}

// Recommended for values that come from computation, like normals, UVs, rotations...
float		FDatasmithHashTools::FloatTolerance = KINDA_SMALL_NUMBER;
const float FDatasmithHashTools::MaxFloatTolerance = 1.0f / FLOAT_NON_FRACTIONAL;

// Hash float value, Use tolerance to absorb compute error
void FDatasmithHashTools::HashFloatTolerance(double InValue, float InTolerance)
{
	if (InTolerance > MaxFloatTolerance)
	{
		// Near zero or negative
		if ((float)InValue <= InTolerance)
		{
			InValue = -InValue;
			if ((float)InValue <= InTolerance)
			{
				TUpdate(0.0f);
				return; // Near zero ?
			}
			TUpdate(false);
		}

		// Tricky way to hash value so that near values (because of compute error) give same hash values
		float LogValue = log10((float)InValue);
		float IntLogValue = std::floorf(LogValue / InTolerance + 0.5f);
		if (IntLogValue == -0.0f)
		{
			IntLogValue = 0.0f; // We want to confond negative near zero to positive near zero
		}
		TUpdate(IntLogValue);
	}
	else
	{
		// No tolerance -> we take the hash value itself
		TUpdate((float)InValue);
	}
}

// Hash 3d point related value, taking care to absorb input error
void FDatasmithHashTools::HashFixVector(const FVector& InVector, float InInvTolerance)
{
	HashFixedPointFloatTolerance(InVector.X, InInvTolerance);
	HashFixedPointFloatTolerance(InVector.Y, InInvTolerance);
	HashFixedPointFloatTolerance(InVector.Z, InInvTolerance);
}

// Hash 3d vector value, taking care to absorb compute error
void FDatasmithHashTools::HashFloatVector(const FVector& InVector, float InTolerance)
{
	HashFloatTolerance(InVector.X, InTolerance);
	HashFloatTolerance(InVector.Y, InTolerance);
	HashFloatTolerance(InVector.Z, InTolerance);
}

// Hash 3d scaling value, taking care to absorb compute error
void FDatasmithHashTools::HashScaleVector(const FVector& InScale, float InTolerance)
{
	// Absorb negative scaling resulting of quad and matix operations
	HashFloatTolerance(abs(InScale.X), InTolerance);
	HashFloatTolerance(abs(InScale.Y), InTolerance);
	HashFloatTolerance(abs(InScale.Z), InTolerance);
}

// Determine if this quaternions come from a operation with a non rotational matrix
static bool QuatNeglectible(FQuat InQuat, float InTolerance)
{
	float t2 = InTolerance * InTolerance;
	int	  NbZeros = 0;
	if (InQuat.X * InQuat.X < t2)
	{
		++NbZeros;
	}
	if (InQuat.Y * InQuat.Y < t2)
	{
		++NbZeros;
	}
	if (InQuat.Z * InQuat.Z < t2)
	{
		++NbZeros;
	}
	if (InQuat.W * InQuat.W < t2)
	{
		++NbZeros;
	}
	return NbZeros > 1;
}

// Normalize quat components values
/* Quat components values usualy are in turn (-0.5..0.5).
 * Bring back to 0.0 to 1.0 and make 0.0 turn == 1.0 turn
 */
static float ZeroOne(double x)
{
	if (x < 0.0)
		x = 1.0 + x;
	return (float)FMath::Sqrt(FMath::Abs(x - x * x)); // = sqrt(abs(x * (1 - x))
}

// Hash quaternion value, taking care to absorb compute error
void FDatasmithHashTools::HashQuat(const FQuat& InQuat, float InTolerance)
{
	if (QuatNeglectible(InQuat, InTolerance))
	{
		TUpdate(false);
		return;
	}
	HashFloatTolerance(ZeroOne(InQuat.X), InTolerance);
	HashFloatTolerance(ZeroOne(InQuat.Y), InTolerance);
	HashFloatTolerance(ZeroOne(InQuat.Z), InTolerance);
	HashFloatTolerance(ZeroOne(InQuat.W), InTolerance);
}

void FDatasmithHashTools::ComputeDatasmithMeshHash(const FDatasmithMesh& Mesh)
{
	// If Datasmith change something to format on disk, we will increment this value to force new hash value
	const uint32 DatasmithMeshVersion = 0;
	TUpdate(DatasmithMeshVersion);

	int32 VerticesCount = Mesh.GetVerticesCount();
	TUpdate(VerticesCount);
	for (int32 IdxVertice = 0; IdxVertice < VerticesCount; ++IdxVertice)
	{
		HashFixVector(FVector(Mesh.GetVertex(IdxVertice)));
		TUpdate(Mesh.GetVertexColor(IdxVertice));
	}

	int32 UVChannelCount = Mesh.GetUVChannelsCount();
	TUpdate(UVChannelCount);
	for (int32 IdxChannel = 0; IdxChannel < UVChannelCount; ++IdxChannel)
	{
		int32 UVCount = Mesh.GetUVCount(IdxChannel);
		TUpdate(UVCount);
		for (int32 IdxUV = 0; IdxUV < UVCount; ++IdxUV)
		{
			FVector2D UV = Mesh.GetUV(IdxChannel, IdxUV);
			HashFloatTolerance(UV.X);
			HashFloatTolerance(UV.Y);
		}
	}

	int32 FacesCount = Mesh.GetFacesCount();
	TUpdate(FacesCount);
	for (int32 IdxFace = 0; IdxFace < FacesCount; ++IdxFace)
	{
		int32 Vertex1;
		int32 Vertex2;
		int32 Vertex3;
		int32 MaterialId;
		Mesh.GetFace(IdxFace, Vertex1, Vertex2, Vertex3, MaterialId);
		TUpdate(Vertex1);
		TUpdate(Vertex2);
		TUpdate(Vertex3);
		TUpdate(MaterialId);
		TUpdate(Mesh.GetFaceSmoothingMask(IdxFace));

		for (int32 IdxComponent = 0; IdxComponent < 3; IdxComponent++)
		{
			HashFloatVector(FVector(Mesh.GetNormal(IdxFace * 3 + IdxComponent)));
		}

		for (int32 IdxChannel = 0; IdxChannel < UVChannelCount; ++IdxChannel)
		{
			Mesh.GetFaceUV(IdxFace, IdxChannel, Vertex1, Vertex2, Vertex3);
			TUpdate(Vertex1);
			TUpdate(Vertex2);
			TUpdate(Vertex3);
		}
	}

	TUpdate(Mesh.GetLightmapSourceUVChannel());

	int32 LODsCount = Mesh.GetLODsCount();
	TUpdate(LODsCount);
	for (int32 IdxLOD = 0; IdxLOD < LODsCount; ++IdxLOD)
	{
#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION == 26
		ComputeDatasmithMeshHash(Mesh.GetLOD(IdxLOD));
#else
		ComputeDatasmithMeshHash(*Mesh.GetLOD(IdxLOD));
#endif
	}
}

END_NAMESPACE_UE_AC
