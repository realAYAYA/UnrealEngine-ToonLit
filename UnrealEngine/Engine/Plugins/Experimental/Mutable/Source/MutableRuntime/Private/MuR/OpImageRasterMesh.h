// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/ImagePrivate.h"
#include "MuR/Platform.h"
#include "MuR/MeshPrivate.h"
#include "MuR/Raster.h"
#include "MuR/ConvertData.h"


namespace mu
{

	class WhitePixelProcessor
	{
	public:
		inline void ProcessPixel( unsigned char* pBufferPos, float[1] ) const
		{
			pBufferPos[0] = 255;
		}
	};


	inline void ImageRasterMesh( const Mesh* pMesh, Image* pImage, int block )
	{
		MUTABLE_CPUPROFILER_SCOPE(ImageRasterMesh)

		check( pImage->GetFormat()== EImageFormat::IF_L_UBYTE );

		int sizeX = pImage->GetSizeX();
		int sizeY = pImage->GetSizeY();

		// Get the vertices
		int vertexCount = pMesh->GetVertexCount();
		TArray< RasterVertex<1> > vertices;
		vertices.SetNumZeroed(vertexCount);

		UntypedMeshBufferIteratorConst texIt( pMesh->GetVertexBuffers(), MBS_TEXCOORDS, 0 );
		for ( int v=0; v<vertexCount; ++v )
		{
            float uv[2] = {0.0f,0.0f};
			ConvertData( 0, uv, MBF_FLOAT32, texIt.ptr(), texIt.GetFormat() );
			ConvertData( 1, uv, MBF_FLOAT32, texIt.ptr(), texIt.GetFormat() );

			vertices[v].x = uv[0] * sizeX;
			vertices[v].y = uv[1] * sizeY;
			++texIt;
		}

		// Get the indices
		int faceCount = pMesh->GetFaceCount();
		TArray<int> indices;
		indices.SetNumZeroed(faceCount * 3);

		UntypedMeshBufferIteratorConst indIt( pMesh->GetIndexBuffers(), MBS_VERTEXINDEX, 0 );
		for ( int i=0; i<faceCount*3; ++i )
		{
            uint32_t index=0;
			ConvertData( 0, &index, MBF_UINT32, indIt.ptr(), indIt.GetFormat() );

			indices[i] = index;
			++indIt;
		}

        UntypedMeshBufferIteratorConst bloIt( pMesh->GetVertexBuffers(), MBS_LAYOUTBLOCK, 0 );

        if ( block<0 || bloIt.GetElementSize()==0 )
		{
			// Raster all the faces
            WhitePixelProcessor pixelProc;
			//for ( int f=0; f<faceCount; ++f )
			const auto& ProcessFace = [
				vertices, indices, pImage, sizeX, sizeY, pixelProc
			] (int32 f)
			{
				Triangle(pImage->GetData(),
					sizeX, sizeY,
					1,
					vertices[indices[f * 3 + 0]],
					vertices[indices[f * 3 + 1]],
					vertices[indices[f * 3 + 2]],
					pixelProc,
					false);
			};

			ParallelFor(faceCount, ProcessFace);
		}
		else
		{
			// Raster only the faces in the selected block

			// Get the block per face
			TArray<int> blocks;
			blocks.SetNumZeroed(vertexCount);

			for ( int i=0; i<vertexCount; ++i )
			{
                uint16 index=0;
				ConvertData( 0, &index, MBF_UINT16, bloIt.ptr(), bloIt.GetFormat() );

				blocks[i] = index;
				++bloIt;
			}

            WhitePixelProcessor pixelProc;
			//for (int f = 0; f < faceCount; ++f)
			const auto& ProcessFace = [
				vertices, indices, blocks, block, pImage, sizeX, sizeY, pixelProc
			] (int32 f)
			{
				// TODO: Select faces outside for loop?
				if (blocks[indices[f * 3 + 0]] == block)
				{
					Triangle(pImage->GetData(),
						sizeX, sizeY,
						1,
						vertices[indices[f * 3 + 0]],
						vertices[indices[f * 3 + 1]],
						vertices[indices[f * 3 + 2]],
						pixelProc,
						false);
				}
			};

			ParallelFor(faceCount, ProcessFace);
		}

	}

}
