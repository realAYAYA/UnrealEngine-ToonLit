// Copyright Epic Games, Inc. All Rights Reserved.

#include "Mesh.h"

bool FText3DMesh::IsEmpty() const
{
	return Vertices.Num() == 0 || Indices.Num() == 0;
}
