// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"


struct FConversionToMeshDescriptionOptions
{
public:
	/** Should triangle groups be transfered to MeshDescription via custom PolyTriGroups attribute */
	bool bSetPolyGroups = true;


	/** Should Positions of vertices in MeshDescription be updated */
	bool bUpdatePositions = true;

	/** Should normals of MeshDescription be updated, if available and relevant */
	bool bUpdateNormals = true;

	/** Should Tangents and BiTangentSign of MeshDescription be updated, if available and relevant */
	bool bUpdateTangents = false;

	/** Should UVs of MeshDescription be updated, if available and relevant */
	bool bUpdateUVs = false;

	/** Should Vertex Colors of MeshDescription be updated, if available and relevant */
	bool bUpdateVtxColors = false;

	/** Should Vertex Colors of MeshDescription be transformed from SRGB to Linear */
	bool bTransformVtxColorsSRGBToLinear = true;

	//
	// utility functions for common configuration cases
	//

	void SetToVertexColorsOnly()
	{
		bSetPolyGroups = bUpdatePositions = bUpdateNormals = bUpdateTangents = bUpdateUVs = false;
		bUpdateVtxColors = true;
	}
};
