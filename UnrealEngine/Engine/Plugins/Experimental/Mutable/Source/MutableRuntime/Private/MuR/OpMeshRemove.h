// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "HAL/Platform.h"


namespace mu
{
	class Mesh;

	/** Remove a list of vertices and related faces from a mesh. The list of vertices is stored in a specially formattes Mask mesh. */
	extern void MeshRemoveMask(Mesh* Result, const Mesh* Source, const Mesh* Mask, bool& bOutSuccess);

	/** Remove a list of vertices and related faces from a mesh. The list is stored as a bool map for every vertex in the mesh. */
	extern void MeshRemoveVerticesWithMap(Mesh* Result, const uint8* RemovedVertices, uint32 RemovedVerticesCount);

}
