// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ContainersFwd.h" // IWYU pragma: keep

class FGeometryCollection;
struct FManagedArrayCollection;

class FRACTUREENGINE_API FFractureEngineMaterials
{
public:

	enum class ETargetFaces
	{
		InternalFaces,
		ExternalFaces,
		AllFaces
	};

	static void SetMaterial(FManagedArrayCollection& InCollection, const TArray<int32>& InBoneSelection, ETargetFaces TargetFaces, int32 MaterialID);

	static void SetMaterialOnGeometryAfter(FManagedArrayCollection& InCollection, int32 FirstGeometryIndex, ETargetFaces TargetFaces, int32 MaterialID);

	static void SetMaterialOnAllGeometry(FManagedArrayCollection& InCollection, ETargetFaces TargetFaces, int32 MaterialID)
	{
		SetMaterialOnGeometryAfter(InCollection, 0, TargetFaces, MaterialID);
	}

};
