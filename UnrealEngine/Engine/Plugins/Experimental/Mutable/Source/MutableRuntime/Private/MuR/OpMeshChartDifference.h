// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/MeshPrivate.h"
#include "MuR/Platform.h"


namespace mu
{


    //---------------------------------------------------------------------------------------------
    inline void MeshChartCreateChartBuffer( Mesh* pSource, unsigned chart )
    {
        check( chart<=0xFF );
        check( chart<32 );
		(void)chart;

        //UINT32 chartMask = (1 << chart);

        // Make sure that the source mesh has a chart buffer
        int chartBuffer = -1;
        int chartChannel = -1;
        pSource->GetFaceBuffers().FindChannel( MBS_CHART, 0, &chartBuffer, &chartChannel );
        if ( chartBuffer<0 )
        {
            // Add a new empty chart channel in its own new face buffer
            int bufCount = pSource->GetFaceBuffers().GetBufferCount();
            pSource->GetFaceBuffers().SetBufferCount( bufCount + 1 );

            MESH_BUFFER_SEMANTIC semantic = MBS_CHART;
            int semanticIndex = 0;
            MESH_BUFFER_FORMAT format = MBF_UINT32;
            int components = 1;
            int offset = 0;
            pSource->GetFaceBuffers().SetBuffer
                    ( bufCount, 4, 1, &semantic, &semanticIndex, &format, &components, &offset );
            uint8_t* pBuf = pSource->GetFaceBuffers().GetBufferData( bufCount );
            int size = pSource->GetFaceBuffers().GetElementSize( bufCount )
                     * pSource->GetFaceBuffers().GetElementCount();
			FMemory::Memzero( pBuf, size );
        }
    }


	//---------------------------------------------------------------------------------------------
    inline void MeshChartDifference( Mesh* pSource, const Mesh* pTarget, unsigned chart )
	{
		check( chart<=0xFF );
		check( chart<32 );

		int sourceFaceCount = pSource->GetFaceCount();
		int targetFaceCount = pTarget->GetFaceCount();
		if ( (!sourceFaceCount) || (!targetFaceCount) )
		{
			return;
        }

        // Make a tolerance proportional to the mesh bounding box size
        // TODO: Use precomputed bounding box
        box< vec3<float> > aabbox;
        if ( targetFaceCount > 0 )
        {
            MeshBufferIteratorConst<MBF_FLOAT32,float,3> itp( pTarget->GetVertexBuffers(), MBS_POSITION );

            aabbox.min = *itp;
            ++itp;

            for ( int v=1; v<pTarget->GetVertexBuffers().GetElementCount(); ++v )
            {
                aabbox.Bound( *itp );
                ++itp;
            }
        }
        float tolerance = 1e-5f * length(aabbox.size);


        uint32_t chartMask = (1 << chart);

		// Make sure that the source mesh has a chart buffer
        MeshChartCreateChartBuffer(pSource,chart);

        // Bucketed version

		// For every face in the source, see if it is also in the target.
		Mesh::VERTEX_MATCH_MAP vertexMap;
        pTarget->GetVertexMap( *pSource, vertexMap, tolerance );

        MeshBufferIterator<MBF_UINT32,uint32_t,1> itChart( pSource->GetFaceBuffers(), MBS_CHART );


		// Classify the target faces in buckets along the Y axis
#define NUM_BUCKETS	128
#define AXIS		1
		TArray<int> buckets[ NUM_BUCKETS ];
		float bucketStart = aabbox.min[AXIS];
		float bucketSize = aabbox.size[AXIS] / NUM_BUCKETS;

		float bucketThreshold = ( 4 * tolerance ) / bucketSize;
        UntypedMeshBufferIteratorConst iti( pTarget->GetIndexBuffers(), MBS_VERTEXINDEX );
		MeshBufferIteratorConst<MBF_FLOAT32,float,3> itp( pTarget->GetVertexBuffers(), MBS_POSITION );
		for ( int tf=0; tf<targetFaceCount; tf++ )
		{
            uint32_t index0 = iti.GetAsUINT32(); ++iti;
            uint32_t index1 = iti.GetAsUINT32(); ++iti;
            uint32_t index2 = iti.GetAsUINT32(); ++iti;
			float y = ( (*(itp+index0))[AXIS] + (*(itp+index1))[AXIS] + (*(itp+index2))[AXIS] ) / 3;
			float fbucket = (y-bucketStart) / bucketSize;
			int bucket = FMath::Min( NUM_BUCKETS-1, FMath::Max( 0, (int)fbucket ) );
			buckets[bucket].Add(tf);
			int hibucket = FMath::Min( NUM_BUCKETS-1, FMath::Max( 0, (int)(fbucket+bucketThreshold) ) );
			if (hibucket!=bucket)
			{
				buckets[hibucket].Add(tf);
			}
			int lobucket = FMath::Min( NUM_BUCKETS-1, FMath::Max( 0, (int)(fbucket-bucketThreshold) ) );
			if (lobucket!=bucket)
			{
				buckets[lobucket].Add(tf);
			}
		}

//		LogDebug("Box : min %.3f, %.3f, %.3f    size %.3f,%.3f,%.3f\n",
//				aabbox.min[0], aabbox.min[1], aabbox.min[2],
//				aabbox.size[0], aabbox.size[1], aabbox.size[2] );
//		for ( int b=0; b<NUM_BUCKETS; ++b )
//		{
//			LogDebug("bucket : %d\n", buckets[b].size() );
//		}

        UntypedMeshBufferIteratorConst ito( pSource->GetIndexBuffers(), MBS_VERTEXINDEX );
		MeshBufferIteratorConst<MBF_FLOAT32,float,3> itop( pSource->GetVertexBuffers(), MBS_POSITION );
        UntypedMeshBufferIteratorConst itti( pTarget->GetIndexBuffers(), MBS_VERTEXINDEX );
		for ( int f=0; f<sourceFaceCount; ++f )
		{
			bool hasFace = false;
            vec3<uint32_t> ov;
            ov[0] = ito.GetAsUINT32(); ++ito;
            ov[1] = ito.GetAsUINT32(); ++ito;
            ov[2] = ito.GetAsUINT32(); ++ito;

			// find the bucket for this face
			float y = ( (*(itop+ov[0]))[AXIS] + (*(itop+ov[1]))[AXIS] + (*(itop+ov[2]))[AXIS] ) / 3;
			float fbucket = (y-bucketStart) / bucketSize;
			int bucket = FMath::Min( NUM_BUCKETS-1, FMath::Max( 0, (int)fbucket ) );

			for ( int32 btf=0; !hasFace && btf<buckets[bucket].Num(); btf++ )
			{
				int tf =  buckets[bucket][btf];

                vec3<uint32_t> v;
                v[0] = (itti+3*tf+0).GetAsUINT32();
                v[1] = (itti+3*tf+1).GetAsUINT32();
                v[2] = (itti+3*tf+2).GetAsUINT32();

				hasFace = true;
				for ( int vi=0; hasFace && vi<3; ++vi )
				{
					hasFace = vertexMap.Matches(v[vi],ov[0])
						 || vertexMap.Matches(v[vi],ov[1])
						 || vertexMap.Matches(v[vi],ov[2]);
				}
			}

			if ( hasFace )
			{
				(*itChart)[0] |= chartMask;
			}

			++itChart;
		}

        /*
        // Non bucket version
		// For every face in the source, see if it is also in the target.
		Mesh::Private::VERTEX_MATCH_MAP vertexMap;
        pTarget->GetPrivate()->GetVertexMap( *pSource->GetPrivate(), vertexMap, tolerance );

		MeshBufferIterator<MBF_UINT32,UINT32,1> itChart( *pSource->GetFaceBuffers(), MBS_CHART );

		MeshBufferIteratorConst<MBF_UINT32,UINT32,1> ito( *pSource->GetIndexBuffers(), MBS_VERTEXINDEX );
		for ( int f=0; f<sourceFaceCount; ++f )
		{
			bool hasFace = false;
			vec3<UINT32> ov;
			ov[0] = (*ito)[0]; ++ito;
			ov[1] = (*ito)[0]; ++ito;
			ov[2] = (*ito)[0]; ++ito;

			if ( targetFaceCount>0 )
			{
				MeshBufferIteratorConst<MBF_UINT32,UINT32,1> it( *pTarget->GetIndexBuffers(), MBS_VERTEXINDEX );
				for ( int tf=0; !hasFace && tf<targetFaceCount; tf++ )
				{
					vec3<UINT32> v;
					v[0] = (*it)[0]; ++it;
					v[1] = (*it)[0]; ++it;
					v[2] = (*it)[0]; ++it;

					hasFace = true;
					for ( int vi=0; hasFace && vi<3; ++vi )
					{
						hasFace = vertexMap.Matches(v[vi],ov[0])
							 || vertexMap.Matches(v[vi],ov[1])
							 || vertexMap.Matches(v[vi],ov[2]);
					}
				}
			}

			if ( hasFace )
			{
				(*itChart)[0] |= chartMask;
			}

			++itChart;
		}
        */

	}

}
