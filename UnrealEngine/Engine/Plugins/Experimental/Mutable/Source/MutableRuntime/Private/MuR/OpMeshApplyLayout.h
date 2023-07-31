// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/MeshPrivate.h"
#include "MuR/Platform.h"

namespace mu
{


	//---------------------------------------------------------------------------------------------
    inline void MeshApplyLayout( Mesh* pApplied, const Layout* pLayout, int texCoordsSet )
	{
		MUTABLE_CPUPROFILER_SCOPE(MeshApplyLayout);

        int buffer = -1;
		int channel = -1;
		pApplied->GetVertexBuffers().FindChannel( MBS_TEXCOORDS, texCoordsSet, &buffer, &channel );

        int layoutBuffer = -1;
        int layoutChannel = -1;
        pApplied->GetVertexBuffers().FindChannel( MBS_LAYOUTBLOCK, texCoordsSet, &layoutBuffer, &layoutChannel );

		if (buffer < 0 || layoutBuffer < 0)
		{
			return;
		}

		//{
		//	FString debugLog;
		//	pApplied->GetPrivate()->Log(debugLog, 4);
		//	UE_LOG(LogMutableCore, Warning, TEXT("before: %s"), *debugLog);
		//}

		// Get the information about the texture coordinates channel
		MESH_BUFFER_SEMANTIC semantic;
		int semanticIndex;
		MESH_BUFFER_FORMAT format;
		int components;
		int offset;
		pApplied->GetVertexBuffers().GetChannel
				( buffer, channel, &semantic, &semanticIndex, &format, &components, &offset );
		check( semantic == MBS_TEXCOORDS );
		//check( format == MBF_FLOAT32 );
		//check( components == 2 );

        uint8_t* pData = pApplied->GetVertexBuffers().GetBufferData( buffer );
		int elemSize = pApplied->GetVertexBuffers().GetElementSize( buffer );
		int channelOffset = pApplied->GetVertexBuffers().GetChannelOffset( buffer, channel );
		pData += channelOffset;

		struct Box
		{
			FVector2f min, size;
		};
		TArray< Box > transforms;
		transforms.SetNum(pLayout->GetBlockCount());
		for ( int b=0; b<pLayout->GetBlockCount(); ++b )
		{
			FIntPoint grid = pLayout->GetGridSize();

			box< vec2<int> > block;
			pLayout->GetBlock( b, &block.min[0], &block.min[1], &block.size[0], &block.size[1] );

			Box rect;
			rect.min[0] = ( (float)block.min[0] ) / (float) grid[0];
			rect.min[1] = ( (float)block.min[1] ) / (float) grid[1];
			rect.size[0] = ( (float)block.size[0] ) / (float) grid[0];
			rect.size[1] = ( (float)block.size[1] ) / (float) grid[1];
			transforms[b] = rect;
		}

        check( layoutBuffer>=0 && layoutChannel>=0 );
        check( pApplied->GetVertexBuffers().m_buffers[layoutBuffer].m_channels.Num()==1 );
        check( pApplied->GetVertexBuffers().m_buffers[layoutBuffer].m_channels[layoutChannel].m_format==MBF_UINT16 );
        check( pApplied->GetVertexBuffers().m_buffers[layoutBuffer].m_channels[layoutChannel].m_componentCount==1 );

        const uint16* pLayoutData = reinterpret_cast<const uint16*>
                ( pApplied->GetVertexBuffers().GetBufferData( layoutBuffer ) );

        uint8* pVertices = pData;
		for ( int v=0; v<pApplied->GetVertexBuffers().GetElementCount(); ++v )
		{
			int blockId = pLayoutData[v];

			// TODO: Optimise
			int relBlock = pLayout->FindBlock( blockId );

			// once we do remove the vertices, this assert can be activated.
			//check( relBlock>=0 && relBlock<(int)transforms.size() );

			// once we do remove the vertices, this condition is not necessary
			if (relBlock < 0)
			{
				continue;
			}

			if ( format == MBF_FLOAT32 )
			{
				// Not valid in arm because of the potential non-alignement of floats
				//vec2<float>* pUV = reinterpret_cast<vec2<float>*>( pVertices );
				//*pUV = (*pUV) * transforms[ relBlock ].size + transforms[ relBlock ].min;

				FVector2f uv;
				FMemory::Memcpy( &uv, pVertices, sizeof(float)*2 );

				uv = uv * transforms[ relBlock ].size + transforms[ relBlock ].min;
				FMemory::Memcpy( pVertices, &uv, sizeof(float)*2 );
			}
			else if ( format == MBF_FLOAT16 )
			{
				// TODO: Optimise
				float16* pUV = reinterpret_cast<float16*>( pVertices );

				FVector2f uv;
				uv[0] = halfToFloat( pUV[0] );
				uv[1] = halfToFloat( pUV[1] );
				uv = uv * transforms[ relBlock ].size + transforms[ relBlock ].min;

				pUV[0] = floatToHalf( uv[0] );
				pUV[1] = floatToHalf( uv[1] );
			}

			else if ( ( format == MBF_NUINT32 )
						|| ( format == MBF_NINT32 )
						|| ( format == MBF_UINT32 )
						|| ( format == MBF_INT32 )
						)
			{
				// TODO: Optimise
                uint32_t* pUV = reinterpret_cast<uint32_t*>( pVertices );

				for ( int c=0; c<2; ++c )
				{
                    uint64_t u_32 = pUV[c];
                    uint64_t u_48 = u_32 * ((uint64_t)(((float)0xffff) * transforms[ relBlock ].size[c]));
                    u_48 += (uint64_t)(((float)0xffffffffffULL) * transforms[ relBlock ].min[c]);
                    pUV[c] = (uint16)( u_48 >> 16 );
				}
			}

			else if ( ( format == MBF_NUINT16 )
						|| ( format == MBF_NINT16 )
						|| ( format == MBF_UINT16 )
						|| ( format == MBF_INT16 )
						)
			{
				// TODO: Optimise
                uint16* pUV = reinterpret_cast<uint16*>( pVertices );

				for ( int c=0; c<2; ++c )
				{
                    uint32_t u_16 = pUV[c];
                    uint32_t u_32 = u_16 * ((uint8_t)(((float)0xffff) * transforms[ relBlock ].size[c]));
                    u_32 += (uint32_t)(((float)0xffffffff) * transforms[ relBlock ].min[c]);
                    pUV[c] = (uint16)( u_32 >> 16 );
				}
			}

			else if ( ( format == MBF_NUINT8 )
						|| ( format == MBF_NINT8 )
						|| ( format == MBF_UINT8 )
						|| ( format == MBF_INT8 )
						)
			{
				// TODO: Optimise
                uint8_t* pUV = reinterpret_cast<uint8_t*>( pVertices );

				for ( int c=0; c<2; ++c )
				{
                    uint32_t u_8 = pUV[c];
                    uint32_t u_24 = u_8 * ((uint32_t)(((float)0xffff) * transforms[ relBlock ].size[c]));
                    u_24 += (uint32_t)(((float)0xffffff) * transforms[ relBlock ].min[c]);
                    pUV[c] = (uint8_t)( u_24 >> 16 );
				}
			}

			else
			{
				UE_LOG(LogMutableCore, Warning, TEXT("This case is not implemented.."));
				check( false );
			}

			pVertices += elemSize;
		}

		//
		pApplied->SetLayout( texCoordsSet, pLayout->Clone() );

		//{
		//	FString debugLog;
		//	pApplied->GetPrivate()->Log(debugLog, 4);
		//	UE_LOG(LogMutableCore, Warning, TEXT("after: %s"), *debugLog);
		//}
	}

}
