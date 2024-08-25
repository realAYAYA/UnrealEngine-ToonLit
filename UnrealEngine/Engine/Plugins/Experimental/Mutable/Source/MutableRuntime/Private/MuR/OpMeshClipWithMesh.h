// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Platform.h"

namespace mu
{
	class Mesh;
	class Image;

    /**  */
	extern void MeshClipWithMesh(Mesh* Result, const Mesh* pBase, const Mesh* pClipMesh, bool& bOutSuccess);

    /** Generate a mask mesh with the faces of the base mesh inside the clip mesh. */
	extern void MeshMaskClipMesh(Mesh* Result, const Mesh* pBase, const Mesh* pClipMesh, bool& bOutSuccess);

	/** Generate a mask mesh with the faces of the base mesh that have all 3 vertices marked in the fiven mask. */
	extern void MeshMaskClipUVMask(Mesh* Result, const Mesh* Base, const Image* Mask, uint8 LayoutIndex, bool& bOutSuccess);

    /** Generate a mask mesh with the faces of the base mesh matching the fragment. */
	extern void MeshMaskDiff(Mesh* Result, const Mesh* pBase, const Mesh* pFragment, bool& bOutSuccess);

}
