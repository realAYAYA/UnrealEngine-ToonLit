// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Async/ParallelFor.h"
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
		inline void ProcessPixel(uint8* pBufferPos, float[1]) const
		{
			pBufferPos[0] = 255;
		}

		inline void operator()(uint8* BufferPos, float Interpolators[1]) const
		{
			ProcessPixel(BufferPos, Interpolators);
		}
	};


	inline void ImageRasterMesh( const Mesh* pMesh, Image* pImage, int32 LayoutIndex, int32 BlockId,
		UE::Math::TIntVector2<uint16> CropMin, UE::Math::TIntVector2<uint16> UncroppedSize )
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

			bool bUseCropping = UncroppedSize[0] > 0;
			if (bUseCropping)
			{
				vertices[v].x = uv[0] * UncroppedSize[0] - CropMin[0];
				vertices[v].y = uv[1] * UncroppedSize[1] - CropMin[1];
			}
			else
			{
				vertices[v].x = uv[0] * sizeX;
				vertices[v].y = uv[1] * sizeY;
			}
			++texIt;
		}

		// Get the indices
		int32 faceCount = pMesh->GetFaceCount();
		TArray<int32> indices;
		indices.SetNumZeroed(faceCount * 3);

		UntypedMeshBufferIteratorConst indIt( pMesh->GetIndexBuffers(), MBS_VERTEXINDEX, 0 );
		for ( int i=0; i<faceCount*3; ++i )
		{
            uint32_t index=0;
			ConvertData( 0, &index, MBF_UINT32, indIt.ptr(), indIt.GetFormat() );

			indices[i] = index;
			++indIt;
		}

        UntypedMeshBufferIteratorConst bloIt( pMesh->GetVertexBuffers(), MBS_LAYOUTBLOCK, LayoutIndex );

        if (BlockId <0 || bloIt.GetElementSize()==0 )
		{
			// Raster all the faces
            WhitePixelProcessor pixelProc;
			const TArrayView<uint8> ImageData = pImage->DataStorage.GetLOD(0);

			//for ( int f=0; f<faceCount; ++f )
			const auto& ProcessFace = [
				vertices, indices, ImageData, sizeX, sizeY, pixelProc
			] (int32 f)
			{
				constexpr int32 NumInterpolators = 1;
				Triangle<NumInterpolators>(ImageData.GetData(), ImageData.Num(),
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
			TArray<uint16> blocks;
			blocks.SetNumZeroed(vertexCount);

			for ( int i=0; i<vertexCount; ++i )
			{
                uint16 index=0;
				ConvertData( 0, &index, MBF_UINT16, bloIt.ptr(), bloIt.GetFormat() );

				blocks[i] = index;
				++bloIt;
			}

            WhitePixelProcessor pixelProc;

			const TArrayView<uint8> ImageData = pImage->DataStorage.GetLOD(0); 

			//for (int f = 0; f < faceCount; ++f)
			const auto& ProcessFace = [
				vertices, indices, blocks, BlockId, ImageData, sizeX, sizeY, pixelProc
			] (int32 f)
			{
				// TODO: Select faces outside for loop?
				if (blocks[indices[f * 3 + 0]] == BlockId)
				{
					constexpr int32 NumInterpolators = 1;
					Triangle<NumInterpolators>(ImageData.GetData(), ImageData.Num(),
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
