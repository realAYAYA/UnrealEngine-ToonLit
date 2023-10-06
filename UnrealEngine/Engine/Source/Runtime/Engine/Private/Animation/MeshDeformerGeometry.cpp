// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/MeshDeformerGeometry.h"

FMeshDeformerGeometry::FMeshDeformerGeometry()
{
	VertexFactoryUserData.DeformerGeometry = this;
}

void FMeshDeformerGeometry::Reset()
{
	PositionUpdatedFrame = TangentUpdatedFrame = ColorUpdatedFrame = 0;
	Position = PrevPosition = Tangent = Color = nullptr;
	PositionSRV = PrevPositionSRV = TangentSRV = ColorSRV = nullptr;
}
