// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"


class FGeometryCollection;

class FGeometryCollectionCreationParameters
{
public:
	GEOMETRYCOLLECTIONENGINE_API FGeometryCollectionCreationParameters(FGeometryCollection& GeometryCollection, bool ReCalculateNormalsIn = false, bool ReCalculateTangetsIn = false);
	GEOMETRYCOLLECTIONENGINE_API ~FGeometryCollectionCreationParameters();

private:
	bool ReCalculateNormals;
	bool ReCalculateTangents;
	FGeometryCollection& GeometryCollection;

};
