// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/MemoryPrivate.h"
#include "MuR/Mesh.h"


namespace mu
{
	class Image;
	struct PROJECTOR;
	template <int NUM_INTERPOLATORS> class RasterVertex;

    struct SCRATCH_IMAGE_PROJECT
    {
        TArray< RasterVertex<4> > vertices;
		TArray<uint8> culledVertex;
    };


//    extern void ImageProject( const Mesh* pMesh,
//                              Image* pTargetImage,
//                              const Image* pSource,
//                              const Image* pMask,
//                              const PROJECTOR& projector,
//                              float fadeStart,
//                              float fadeEnd,
//                              int layout,
//                              int block,
//                              SCRATCH_IMAGE_PROJECT* scratch );

    extern void ImageRasterProjectedPlanar( const Mesh* pMesh,
                                            Image* pTargetImage,
                                            const Image* pSource,
                                            const Image* pMask,
                                            float fadeStart,
                                            float fadeEnd,
                                            int layout,
                                            int block,
                                            SCRATCH_IMAGE_PROJECT* scratch );

    extern void ImageRasterProjectedCylindrical( const Mesh* pMesh,
                                      Image* pTargetImage,
                                      const Image* pSource,
                                      const Image* pMask,
                                      float fadeStart,
                                      float fadeEnd,
                                      int layout,
                                      float projectionAngle,
                                      SCRATCH_IMAGE_PROJECT* scratch );

    extern void ImageRasterProjectedWrapping( const Mesh* pMesh,
                                      Image* pTargetImage,
                                      const Image* pSource,
                                      const Image* pMask,
                                      float fadeStart,
                                      float fadeEnd,
                                      int layout,
                                      int block,
                                      SCRATCH_IMAGE_PROJECT* scratch );

    extern MeshPtr MeshProject( const Mesh* pMesh,
                                const PROJECTOR& projector );

	MUTABLERUNTIME_API extern MeshPtr CreateMeshOptimisedForProjection( int layout );
	MUTABLERUNTIME_API extern MeshPtr CreateMeshOptimisedForWrappingProjection( int layout );

}
