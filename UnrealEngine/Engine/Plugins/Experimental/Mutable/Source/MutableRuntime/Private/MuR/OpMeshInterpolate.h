// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/MeshPrivate.h"
#include "MuR/Platform.h"


namespace mu
{


	//---------------------------------------------------------------------------------------------
    inline MeshPtr MeshInterpolate( const Mesh* pA, const Mesh* pB, float factor )
	{
		bool correct = ( pA->GetVertexBuffers().GetElementCount() == pB->GetVertexBuffers().GetElementCount() )
					&& ( pA->GetIndexCount() == pB->GetIndexCount() )
					&& ( pA->HasCompatibleFormat(pB) );

		MeshPtr pDest = pA->Clone();

		// Deal with degenerated data
		if ( correct && pDest->GetVertexBuffers().GetElementCount() )
		{
			// For now, just interpolate the position and normal buffer.

			// A mesh position
			int aPosElementSize = 0;
            uint8* pAPosBuf = 0;
			GetMeshBuf( pDest.get(), MBS_POSITION, MBF_FLOAT32, 3, pAPosBuf, aPosElementSize );

			// A mesh normal
			int aNorElementSize = 0;
            uint8* pANorBuf = 0;
			GetMeshBuf( pDest.get(), MBS_NORMAL, MBF_FLOAT32, 3, pANorBuf, aNorElementSize );

			// B mesh position
			int bPosElementSize = 0;
            const uint8* pBPosBuf = 0;
			GetMeshBuf( pB, MBS_POSITION, MBF_FLOAT32, 3, pBPosBuf, bPosElementSize );

			// B mesh normal
			int bNorElementSize = 0;
            const uint8* pBNorBuf = 0;
			GetMeshBuf( pB, MBS_NORMAL, MBF_FLOAT32, 3, pBNorBuf, bNorElementSize );

            uint32 vcount = pA->GetVertexBuffers().GetElementCount();
            for ( uint32 vi=0; vi<vcount; ++vi )
			{
				FVector3f* aPos = reinterpret_cast<FVector3f* >( pAPosBuf );
				const FVector3f* bPos = reinterpret_cast< const FVector3f* >( pBPosBuf );
				*aPos += (*bPos - *aPos) * factor;

				FVector3f* aNor = reinterpret_cast<FVector3f* >( pANorBuf );
				const FVector3f* bNor = reinterpret_cast< const FVector3f* >( pBNorBuf );
				*aNor += (*bNor - *aNor) * factor;

				pAPosBuf += aPosElementSize;
				pBPosBuf += bPosElementSize;
				pANorBuf += aNorElementSize;
				pBNorBuf += bNorElementSize;
			}
		}
		else
		{
			// TODO report somehow
		}

		return pDest;
	}

}
