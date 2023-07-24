// Copyright Epic Games, Inc. All Rights Reserved.

#include "MeshAttributes.h"

namespace MeshAttribute
{
	namespace Vertex
	{
		const FName Position("Position ");
	}

	namespace VertexInstance
	{
		const FName VertexIndex("VertexIndex");
	}

	namespace Edge
	{
		const FName VertexIndex("VertexIndex");
	}

	namespace Triangle
	{
		const FName VertexInstanceIndex("VertexInstanceIndex");
		const FName PolygonIndex("PolygonIndex");
		const FName EdgeIndex("EdgeIndex");
		const FName VertexIndex("VertexIndex");
		const FName UVIndex("UVIndex");
		const FName PolygonGroupIndex("PolygonGroupIndex");
	}

	namespace UV
	{
		const FName UVCoordinate("UVCoordinate");
	}

	namespace Polygon
	{
		const FName PolygonGroupIndex("PolygonGroupIndex");
	}
}


void FMeshAttributes::Register(bool bKeepExistingAttribute)
{
	// Nothing to do here: Vertex positions are already registered by the FMeshDescription constructor
}

