// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MathUtil.h"
#include "VectorTypes.h"
#include "IntVectorTypes.h"
#include "GeometryTypes.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"

class FProgressCancel;

namespace UE
{
namespace Geometry
{

DYNAMICMESH_API void ComputeArbitraryTrianglePatchUVs(
	FDynamicMesh3& Mesh, 
	FDynamicMeshUVOverlay& UVOverlay,
	const TArray<int32>& TriangleSet );


} // end namespace UE::Geometry
} // end namespace UE