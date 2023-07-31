// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/System.h"

class USkeletalMesh;

/**
 * Namespace designed to contain a series of methods used for the generation of unreal meshes for previewing purposes.
 * While they can be accessed to do other stuff it is not granted they will produce a complete USkeletalMesh since at the
 * moment it only tries to generate something viewable, but may be missing bone, vertex or/and animation data.
 */
namespace MutableMeshPreviewUtils
{
	/**
	 * Method designed to provide a way of getting a USkeletalMesh based on the data found on a mutable mesh. The
	 * usage of a Reference Skeletal mesh is required to aid on the generation of the skeletal mesh.
	 * @param InMutableMesh - The mutable mesh used as base for the generation of the USkeletalMesh
	 * @param InReferenceSkeletalMesh - Skeletal mesh used as source for the data not stored by mutable mesh. It must be
	 * "similar" in terms of bone structure to the mutable mesh. Example: If generating a mesh from a mutable mesh found
	 * on an instance (using the mutable debugger) you may want to provide as reference skeletal mesh the one used by
	 * the Customizable Object.
	 * @return - A new Skeletal mesh based on the provided data. It may be a nullptr if the conversion fails.
	 */
	CUSTOMIZABLEOBJECTEDITOR_API USkeletalMesh* GenerateSkeletalMeshFromMutableMesh(
		mu::MeshPtrConst InMutableMesh, const USkeletalMesh* InReferenceSkeletalMesh);

}
