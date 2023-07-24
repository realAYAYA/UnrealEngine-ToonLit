// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Mesh.h"


namespace mu
{
class FMeshBufferSet;

	//---------------------------------------------------------------------------------------------
	//! Convert a mesh format into another one.
	//! Slow implementation, but it should never happen at run-time
	//! \param keepSystemBuffers Will keep the internal system buffers even if they are not in the
	//! original format. If they are, they will be duplicated, so be careful.
	//---------------------------------------------------------------------------------------------
	MUTABLERUNTIME_API extern MeshPtr MeshFormat
		(
			const Mesh* pSource,
			const Mesh* pFormat,
			bool keepSystemBuffers,
			bool formatVertices,
			bool formatIndices,
			bool formatFaces,
			bool ignoreMissingChannels
		);


    //---------------------------------------------------------------------------------------------
    //! Fill a meshbuffer from result with the current data in source but keeping the format
    //! already in the result.
    //---------------------------------------------------------------------------------------------
    void MeshFormatBuffer
        (
            const FMeshBufferSet& pSource,
            FMeshBufferSet& pResult,
            int bufferIndex
        );

}