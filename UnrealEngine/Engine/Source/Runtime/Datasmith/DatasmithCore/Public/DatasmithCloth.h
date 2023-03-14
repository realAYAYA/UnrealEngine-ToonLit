// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "Math/UnrealMathSSE.h"
#include "Math/Vector.h"
#include "Math/Vector2D.h"
#include "Misc/TVariant.h"
#include "UObject/NameTypes.h"

class FDatasmithMesh;


struct FParameterData
{
	FString Name;

	enum class ETarget { Vertex };
	ETarget Target = ETarget::Vertex; // (also drives the expected number of values)

	TVariant<TArray<float>, TArray<double>> Data;

public:
	friend FArchive& operator<<(FArchive& Ar, FParameterData& Pattern);
};


class DATASMITHCORE_API FDatasmithClothPattern
{
public:
	TArray<FVector2f> SimPosition;
	TArray<FVector3f> SimRestPosition;
	TArray<uint32> SimTriangleIndices;

	TArray<FParameterData> Parameters;

public:
	bool IsValid() { return SimRestPosition.Num() == SimPosition.Num() && SimTriangleIndices.Num() % 3 == 0 && SimTriangleIndices.Num(); }
	friend FArchive& operator<<(FArchive& Ar, FDatasmithClothPattern& Pattern);
};


class FDatasmithClothPresetProperty
{
public:
	FName Name;
	double Value;

public:
	friend FArchive& operator<<(FArchive& Ar, FDatasmithClothPresetProperty& Property);
};


class DATASMITHCORE_API FDatasmithClothPresetPropertySet
{
public:
	FString SetName;
	TArray<FDatasmithClothPresetProperty> Properties;

public:
	friend FArchive& operator<<(FArchive& Ar, FDatasmithClothPresetPropertySet& PropertySet);
};


class DATASMITHCORE_API FDatasmithCloth
{
public:
	TArray<FDatasmithClothPattern> Patterns;
	TArray<FDatasmithClothPresetPropertySet> PropertySets;

public:
	friend FArchive& operator<<(FArchive& Ar, FDatasmithCloth& Cloth);
};

