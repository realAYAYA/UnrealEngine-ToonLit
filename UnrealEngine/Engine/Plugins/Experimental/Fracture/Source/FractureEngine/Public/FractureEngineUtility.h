// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Math/Box.h"

namespace UE::Geometry { class FDynamicMesh3; }

//class FDynamicMesh3;

class FRACTUREENGINE_API FFractureEngineUtility
{
public:
	/**
	* Outputs the vertex and triangle data of a FBox into OutVertices and OutTriangles
	*/
	static void ConvertBoxToVertexAndTriangleData(const FBox& InBox, TArray<FVector3f>& OutVertices, TArray<FIntVector>& OutTriangles);

	/**
	* Creates a mesh from vertex and triangle data
	*/
	static void ConstructMesh(UE::Geometry::FDynamicMesh3& OutMesh, const TArray<FVector3f>& InVertices, const TArray<FIntVector>& InTriangles);

	/** 
	* Outputs the vertex and triangle data of a mesh into OutVertices and OutTriangles
	*/
	static void DeconstructMesh(const UE::Geometry::FDynamicMesh3& InMesh, TArray<FVector3f>& OutVertices, TArray<FIntVector>& OutTriangles);

};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "Algo/Count.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "GeometryCollection/GeometryCollection.h"
#endif
