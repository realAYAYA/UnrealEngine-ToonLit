// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Utils/AddonTools.h"

#include "SecureHash.h"

class FDatasmithMesh;

BEGIN_NAMESPACE_UE_AC

// Class offering some basic md5 hash services
class FDatasmithHashTools
{
  public:
	// Working MD5 generator
	FMD5& MD5;

	// Constructor.
	FDatasmithHashTools(FMD5& IOMd5)
		: MD5(IOMd5)
	{
	}

	// Return the cumulated hashes. After this call MD5 connot be used anymore.
	FMD5Hash GetHashValue();

	// Recommended for values that come from input like coordinates, colors
	static float FixedPointTolerance;

	// This value mean no tolerance
	static const float MaxInvFixPointTolerance;

	// Hash float with a fixed tolerance value (good for coordinates, colors).
	void HashFixedPointFloatTolerance(double InValue, float InInvTolerance = FixedPointTolerance);

	// Recommended for values that come from computation, like normals, UVs, rotations...
	static float FloatTolerance;

	// This value mean no tolerance
	static const float MaxFloatTolerance;

	// Hash float value, use tolerance to absorb compute error (good for normal, rotation...)
	void HashFloatTolerance(double InValue, float InTolerance = FloatTolerance);

	// Hash 3d point related value, taking care to absorb input error
	void HashFixVector(const FVector& InVector, float InInvTolerance = FixedPointTolerance);

	// Hash 3d vector value, taking care to absorb compute error
	void HashFloatVector(const FVector& InVector, float InTolerance = FloatTolerance);

	// Hash 3d scale value, taking care to absorb compute error
	void HashScaleVector(const FVector& InScale, float InTolerance);

	// Hash quaternion value, taking care to absorb compute error
	void HashQuat(const FQuat& InQuat, float InTolerance = FloatTolerance);

	template < class T > void TUpdate(const T& InValue)
	{
		MD5.Update(reinterpret_cast< const uint8* >(&InValue), sizeof(InValue));
	}

	void Update(const FString& InString)
	{
		MD5.Update(reinterpret_cast< const uint8* >(*InString), InString.Len() * sizeof(TCHAR));
	}

	void Update(const TCHAR* InString)
	{
		MD5.Update(reinterpret_cast< const uint8* >(InString), TCString< TCHAR >::Strlen(InString) * sizeof(TCHAR));
	}

	// Return a hash value ralated to geometry
	void ComputeDatasmithMeshHash(const FDatasmithMesh& Mesh);
};

class FDatasmithHash : public FDatasmithHashTools
{
  public:
	FMD5 MyMD5;

	FDatasmithHash()
		: FDatasmithHashTools(MyMD5)
	{
	}
};

END_NAMESPACE_UE_AC
